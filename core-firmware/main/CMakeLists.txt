idf_component_register(SRCS "opentail_core.c" "core_logic.c" "arbitration_logic.c" "output_manager.c" "trigger_manager.c" "show_logic.c" "wled_fx_compat.c" "ble_fx_command.c" "particle_system_1d.c" "status_logic.c" "status_led.c" "ble_status.c" "ble_command.c" "ble_config.c"
                       INCLUDE_DIRS "."
                       EMBED_FILES "assets/studio.htm.gz"
                       REQUIRES esp_wifi esp_netif esp_event esp_http_server esp_driver_spi esp_timer driver nvs_flash json bt mbedtls)
