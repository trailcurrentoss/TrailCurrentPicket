idf_component_register(SRCS "main.c" "ota.c"
                  INCLUDE_DIRS "."
                  REQUIRES driver nvs_flash esp_wifi esp_http_server mdns esp_timer app_update)

# Module address (0-7): idf.py build -DPICKET_ADDRESS=3
if(DEFINED PICKET_ADDRESS)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PICKET_ADDRESS=${PICKET_ADDRESS})
endif()
