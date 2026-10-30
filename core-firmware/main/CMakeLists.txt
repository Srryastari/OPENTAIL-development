# OpenTail CORE owns a small, device-local /dev diagnostics dashboard.
# Studio (web-studio/opentail.htm) and Android bundle are independent authoring
# products. They are no longer embedded, synchronized or versioned with CORE.
idf_component_register(
    SRCS "opentail_core.c" "core_logic.c" "arbitration_logic.c"
         "output_manager.c" "trigger_manager.c" "show_logic.c"
         "wled_fx_compat.c" "wled_1d.c" "ble_fx_command.c"
         "particle_system_1d.c" "fx_overlay_engine.c" "effects_1d.c" "ot_auto_effects.c"
         "fx_preset_record.c" "palette_1d.c" "fx_zone_bank.c"
         "status_logic.c" "status_led.c" "ble_status.c"
         "ble_command.c" "ble_config.c" "ot_ota_update.c"
         "ot_fps_pacer.c" "ot_frame_snapshot.c"
    INCLUDE_DIRS "."
    EMBED_TXTFILES "assets/dev_console.htm"
    REQUIRES esp_wifi esp_netif esp_event esp_http_server esp_driver_spi
             esp_timer driver nvs_flash json bt mbedtls app_update esp_partition
)
