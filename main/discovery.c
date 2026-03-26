#include "discovery.h"
#include "ota.h"

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_app_desc.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "mdns.h"
#include "nvs.h"
#include "driver/gpio.h"

static const char *TAG = "discovery";

// ---------------------------------------------------------------------------
// Compile-time module identity
// ---------------------------------------------------------------------------

#ifndef MODULE_TYPE
#error "MODULE_TYPE must be defined (e.g., \"picket\")"
#endif

#ifndef PICKET_ADDRESS
#define PICKET_ADDRESS 0
#endif

#define CAN_BASE_ID 0x0A

// ---------------------------------------------------------------------------
// NVS configured flag
// ---------------------------------------------------------------------------

#define NVS_DISCOVERY_NS    "discovery"
#define NVS_KEY_CONFIGURED  "configured"

static bool s_configured = false;

// ---------------------------------------------------------------------------
// Discovery state
// ---------------------------------------------------------------------------

static volatile bool s_confirmed = false;

// ---------------------------------------------------------------------------
// Status LED blink (GPIO0, 4 Hz)
// ---------------------------------------------------------------------------

#define STATUS_LED_PIN 0

static esp_timer_handle_t s_led_timer = NULL;
static bool s_led_state = false;

static void led_blink_cb(void *arg)
{
    s_led_state = !s_led_state;
    gpio_set_level(STATUS_LED_PIN, s_led_state ? 1 : 0);
}

static void led_blink_start(void)
{
    esp_timer_create_args_t args = {
        .callback = led_blink_cb,
        .name = "disc_led",
    };
    esp_timer_create(&args, &s_led_timer);
    esp_timer_start_periodic(s_led_timer, 125000);  // 125 ms = 4 Hz
}

static void led_blink_stop(void)
{
    if (s_led_timer) {
        esp_timer_stop(s_led_timer);
        esp_timer_delete(s_led_timer);
        s_led_timer = NULL;
    }
    gpio_set_level(STATUS_LED_PIN, 1);  // restore solid on
}

// ---------------------------------------------------------------------------
// NVS helpers
// ---------------------------------------------------------------------------

static bool load_configured(void)
{
    nvs_handle_t handle;
    uint8_t val = 0;
    if (nvs_open(NVS_DISCOVERY_NS, NVS_READONLY, &handle) == ESP_OK) {
        nvs_get_u8(handle, NVS_KEY_CONFIGURED, &val);
        nvs_close(handle);
    }
    return val != 0;
}

static void save_configured(bool configured)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_DISCOVERY_NS, NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing");
        return;
    }
    nvs_set_u8(handle, NVS_KEY_CONFIGURED, configured ? 1 : 0);
    nvs_commit(handle);
    nvs_close(handle);
    s_configured = configured;
}

// ---------------------------------------------------------------------------
// mDNS service advertisement with TXT records
// ---------------------------------------------------------------------------

static void discovery_mdns_start(void)
{
    const char *hostname = ota_get_hostname();

    // Build string versions of address and CAN ID
    char addr_str[4];
    char canid_str[8];
    snprintf(addr_str, sizeof(addr_str), "%d", PICKET_ADDRESS);
    snprintf(canid_str, sizeof(canid_str), "0x%02X", CAN_BASE_ID + PICKET_ADDRESS);

    // Get firmware version from app descriptor
    const esp_app_desc_t *app = esp_app_get_description();

    mdns_init();
    mdns_hostname_set(hostname);
    mdns_instance_name_set("TrailCurrent Module");

    mdns_txt_item_t txt[] = {
        { "type",  MODULE_TYPE },
        { "addr",  addr_str },
        { "canid", canid_str },
        { "fw",    app->version },
    };

    mdns_service_add("TrailCurrent Discovery", "_trailcurrent", "_tcp",
                     80, txt, sizeof(txt) / sizeof(txt[0]));

    ESP_LOGI(TAG, "mDNS discovery: %s.local type=%s addr=%s canid=%s fw=%s",
             hostname, MODULE_TYPE, addr_str, canid_str, app->version);
}

// ---------------------------------------------------------------------------
// HTTP confirmation endpoint — Headwaters calls this to acknowledge
// ---------------------------------------------------------------------------

static esp_err_t confirm_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Discovery confirmed by Headwaters");
    httpd_resp_sendstr(req, "confirmed\n");
    s_confirmed = true;
    return ESP_OK;
}

static httpd_handle_t discovery_start_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return NULL;
    }

    httpd_uri_t confirm_uri = {
        .uri     = "/discovery/confirm",
        .method  = HTTP_GET,
        .handler = confirm_handler,
    };
    httpd_register_uri_handler(server, &confirm_uri);

    return server;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void discovery_init(void)
{
    s_configured = load_configured();
    if (s_configured) {
        ESP_LOGI(TAG, "Module already configured — will ignore discovery triggers");
    } else {
        ESP_LOGI(TAG, "Module unconfigured — will respond to CAN 0x02 discovery trigger");
    }
}

void discovery_handle_trigger(void)
{
    if (s_configured) {
        return;  // silently ignore
    }

    if (!ota_has_credentials()) {
        ESP_LOGE(TAG, "Discovery triggered but no WiFi credentials — cannot respond");
        return;
    }

    ESP_LOGI(TAG, "=== Entering discovery mode ===");

    led_blink_start();
    wifi_connect();
    discovery_mdns_start();
    httpd_handle_t server = discovery_start_server();

    // Wait for confirmation or timeout
    s_confirmed = false;
    int64_t start = esp_timer_get_time();

    while (!s_confirmed) {
        vTaskDelay(pdMS_TO_TICKS(100));
        int64_t elapsed_ms = (esp_timer_get_time() - start) / 1000;
        if (elapsed_ms >= DISCOVERY_TIMEOUT_MS) {
            ESP_LOGW(TAG, "Discovery timeout — no confirmation received");
            break;
        }
    }

    // Cleanup
    if (server) {
        httpd_stop(server);
    }
    mdns_free();
    wifi_disconnect();
    led_blink_stop();

    if (s_confirmed) {
        save_configured(true);
        ESP_LOGI(TAG, "=== Discovery complete — module registered ===");
    } else {
        ESP_LOGI(TAG, "=== Discovery timed out — will respond to next trigger ===");
    }
}

void discovery_handle_reset(const uint8_t *data, uint8_t len)
{
    if (len < 3) return;

    const char *hostname = ota_get_hostname();
    char target[16];
    snprintf(target, sizeof(target), "esp32-%02X%02X%02X",
             data[0], data[1], data[2]);

    if (strcmp(target, hostname) != 0) {
        return;
    }

    ESP_LOGI(TAG, "Discovery reset received — clearing configured flag");
    save_configured(false);
}
