#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "ota.h"
#include "discovery.h"

static const char *TAG = "picket";

// =============================================================================
// Pin Definitions — Waveshare ESP32-S3-RS485-CAN pin header
// =============================================================================

// CAN bus pins (onboard TJA1051 transceiver, not on pin header)
#define CAN_TX_PIN   15
#define CAN_RX_PIN   16

// Status LED (onboard LED on GPIO0)
#define STATUS_LED_PIN  0

// Reed switch inputs (Normally Open — close when magnet nearby / door closed)
// HIGH = door open, LOW = door closed (using internal pull-ups)
static const gpio_num_t RSW_PINS[] = {
    GPIO_NUM_4,   // RSW01
    GPIO_NUM_5,   // RSW02
    GPIO_NUM_6,   // RSW03
    GPIO_NUM_7,   // RSW04
    GPIO_NUM_8,   // RSW05
    GPIO_NUM_9,   // RSW06
    GPIO_NUM_10,  // RSW07
    GPIO_NUM_11,  // RSW08
    GPIO_NUM_12,  // RSW09
    GPIO_NUM_13,  // RSW10
    GPIO_NUM_14,  // RSW11
    GPIO_NUM_43,  // RSW12
    GPIO_NUM_44,  // RSW13
};
#define NUM_RSW (sizeof(RSW_PINS) / sizeof(RSW_PINS[0]))

// =============================================================================
// CAN Bus Configuration
// =============================================================================

// CAN IDs 0x0A-0x11 reserved for up to 8 Picket modules.
// Set address at build time: idf.py build -DPICKET_ADDRESS=3
#define CAN_BASE_ID     0x0A
#define CAN_BAUDRATE    500000

#ifndef PICKET_ADDRESS
#define PICKET_ADDRESS  0
#endif
#if PICKET_ADDRESS < 0 || PICKET_ADDRESS > 7
#error "PICKET_ADDRESS must be 0-7"
#endif

// Transmit interval (200 ms = 5 Hz)
#define TX_INTERVAL_MS  200
#define TX_PROBE_INTERVAL_MS  2000 // slow probe when no peers detected

// Debounce time for reed switch readings
#define DEBOUNCE_MS     50

// =============================================================================
// Global State
// =============================================================================

static const uint32_t s_can_message_id = CAN_BASE_ID + PICKET_ADDRESS;

// Debounced reed switch state
static volatile uint16_t s_debounced_state = 0;

// =============================================================================
// Reed Switch Reading
// =============================================================================

static uint16_t read_reed_switches(void)
{
    uint16_t state = 0;
    for (int i = 0; i < NUM_RSW; i++) {
        // HIGH = door open (pull-up, NO reed switch open)
        // LOW = door closed (reed switch closed by magnet)
        if (gpio_get_level(RSW_PINS[i]) == 1) {
            state |= (1 << i);
        }
    }
    return state;
}

// =============================================================================
// TWAI (CAN) task — runs independently (pattern from Solstice)
// =============================================================================

static void twai_task(void *arg)
{
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install TWAI driver");
        vTaskDelete(NULL);
        return;
    }
    if (twai_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start TWAI driver");
        vTaskDelete(NULL);
        return;
    }

    uint32_t alerts = TWAI_ALERT_RX_DATA | TWAI_ALERT_ERR_PASS |
                      TWAI_ALERT_BUS_ERROR | TWAI_ALERT_RX_QUEUE_FULL |
                      TWAI_ALERT_BUS_OFF | TWAI_ALERT_BUS_RECOVERED |
                      TWAI_ALERT_ERR_ACTIVE | TWAI_ALERT_TX_FAILED |
                      TWAI_ALERT_TX_SUCCESS;
    twai_reconfigure_alerts(alerts, NULL);
    ESP_LOGI(TAG, "TWAI driver started (NORMAL mode)");

    typedef enum { TX_ACTIVE, TX_PROBING } tx_state_t;
    bool bus_off = false;
    tx_state_t tx_state = TX_ACTIVE;
    int tx_fail_count = 0;
    const int TX_FAIL_THRESHOLD = 3;
    int64_t last_tx_us = 0;
    const int64_t tx_period_us = TX_INTERVAL_MS * 1000LL;
    const int64_t tx_probe_period_us = TX_PROBE_INTERVAL_MS * 1000LL;

    // Debounce state (owned by this task)
    uint16_t last_raw_state = read_reed_switches();
    uint16_t debounced = last_raw_state;
    int64_t last_change_us = esp_timer_get_time();

    while (1) {
        uint32_t triggered;
        twai_read_alerts(&triggered, pdMS_TO_TICKS(10));

        // --- Bus error handling ---
        if (triggered & TWAI_ALERT_BUS_OFF) {
            ESP_LOGE(TAG, "TWAI bus-off, initiating recovery");
            bus_off = true;
            twai_initiate_recovery();
            continue;
        }
        if (triggered & TWAI_ALERT_BUS_RECOVERED) {
            ESP_LOGI(TAG, "TWAI bus recovered, restarting");
            twai_start();
            bus_off = false;
            tx_fail_count = 0;
            tx_state = TX_PROBING;
        }
        if (triggered & TWAI_ALERT_ERR_PASS) {
            ESP_LOGW(TAG, "TWAI error passive (no peers ACKing?)");
        }
        if (triggered & TWAI_ALERT_TX_FAILED) {
            if (tx_state == TX_ACTIVE) {
                tx_fail_count++;
                if (tx_fail_count >= TX_FAIL_THRESHOLD) {
                    tx_state = TX_PROBING;
                    ESP_LOGW(TAG, "TWAI no peers detected, entering slow probe");
                }
            }
        }
        if (triggered & TWAI_ALERT_TX_SUCCESS) {
            if (tx_state == TX_PROBING) {
                tx_state = TX_ACTIVE;
                tx_fail_count = 0;
                ESP_LOGI(TAG, "TWAI probe ACK'd, peer detected, resuming normal TX");
            }
            tx_fail_count = 0;
        }

        // --- Drain received messages ---
        if (triggered & TWAI_ALERT_RX_DATA) {
            if (tx_state == TX_PROBING) {
                tx_state = TX_ACTIVE;
                tx_fail_count = 0;
                ESP_LOGI(TAG, "TWAI peer detected via RX, resuming normal TX");
            }
            twai_message_t msg;
            while (twai_receive(&msg, 0) == ESP_OK) {
                if (msg.rtr) continue;

                if (msg.identifier == CAN_ID_OTA_TRIGGER) {
                    ota_handle_trigger(msg.data, msg.data_length_code);
                } else if (msg.identifier == CAN_ID_WIFI_CONFIG) {
                    ota_handle_wifi_config(msg.data, msg.data_length_code);
                } else if (msg.identifier == CAN_ID_DISCOVERY_TRIGGER) {
                    discovery_handle_trigger();
                }
            }
        }

        // --- Debounce reed switches ---
        uint16_t raw = read_reed_switches();
        int64_t now_us = esp_timer_get_time();

        if (raw != last_raw_state) {
            last_raw_state = raw;
            last_change_us = now_us;
        }
        if ((now_us - last_change_us) >= ((int64_t)DEBOUNCE_MS * 1000)) {
            debounced = last_raw_state;
        }
        s_debounced_state = debounced;

        // --- Periodic transmit ---
        int64_t effective_period = (tx_state == TX_PROBING) ? tx_probe_period_us : tx_period_us;
        if (!bus_off && (now_us - last_tx_us >= effective_period)) {
            last_tx_us = now_us;

            twai_message_t tx_msg = {
                .identifier = s_can_message_id,
                .data_length_code = 2,
                .data = {
                    (uint8_t)(debounced & 0xFF),         // RSW01-RSW08
                    (uint8_t)((debounced >> 8) & 0x1F),  // RSW09-RSW13
                }
            };
            twai_transmit(&tx_msg, 0);
        }
    }
}

// =============================================================================
// Main application
// =============================================================================

void app_main(void)
{
    ota_init();
    discovery_init();

    ESP_LOGI(TAG, "=== TrailCurrent Picket ===");
    ESP_LOGI(TAG, "Hostname: %s", ota_get_hostname());

    // Configure status LED
    gpio_config_t led_cfg = {
        .pin_bit_mask = (1ULL << STATUS_LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led_cfg);
    gpio_set_level(STATUS_LED_PIN, 1);

    // Configure reed switch inputs with internal pull-ups
    for (int i = 0; i < NUM_RSW; i++) {
        gpio_config_t io_cfg = {
            .pin_bit_mask = (1ULL << RSW_PINS[i]),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
        };
        gpio_config(&io_cfg);
    }
    ESP_LOGI(TAG, "Configured %d reed switch inputs", NUM_RSW);

    ESP_LOGI(TAG, "Module address: %d, CAN ID: 0x%02X", PICKET_ADDRESS, (unsigned)s_can_message_id);

    // CAN runs in its own task so bus errors never block app_main
    xTaskCreate(twai_task, "twai", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Setup complete");

    // Main task has nothing else to do — park it
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
