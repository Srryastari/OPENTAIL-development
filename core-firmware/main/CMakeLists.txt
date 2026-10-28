# Keep CORE's embedded Studio page in sync with the exact source used by
# web deployment and the Android app. Generated only at CMake configuration;
# the sibling source file is tracked as a configure dependency.
set(OPENTAIL_STUDIO_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../../web-studio/opentail.htm")
set(OPENTAIL_STUDIO_GZIP "${CMAKE_CURRENT_LIST_DIR}/assets/studio.htm.gz")
if(EXISTS "${OPENTAIL_STUDIO_SOURCE}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${OPENTAIL_STUDIO_SOURCE}")
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "-c"
            "import gzip, pathlib, sys; pathlib.Path(sys.argv[2]).write_bytes(gzip.compress(pathlib.Path(sys.argv[1]).read_bytes(), compresslevel=9, mtime=0))"
            "${OPENTAIL_STUDIO_SOURCE}" "${OPENTAIL_STUDIO_GZIP}"
        RESULT_VARIABLE OPENTAIL_STUDIO_GZIP_RESULT
        ERROR_VARIABLE OPENTAIL_STUDIO_GZIP_ERROR
    )
    if(NOT OPENTAIL_STUDIO_GZIP_RESULT EQUAL 0)
        message(FATAL_ERROR "Cannot generate CORE Studio embed: ${OPENTAIL_STUDIO_GZIP_ERROR}")
    endif()
else()
    message(WARNING "No sibling web-studio/opentail.htm; using prepackaged CORE Studio gzip")
endif()

idf_component_register(SRCS "opentail_core.c" "core_logic.c" "arbitration_logic.c" "output_manager.c" "trigger_manager.c" "show_logic.c" "wled_fx_compat.c" "wled_1d.c" "ble_fx_command.c" "particle_system_1d.c" "effects_1d.c" "fx_preset_record.c" "palette_1d.c" "fx_zone_bank.c" "status_logic.c" "status_led.c" "ble_status.c" "ble_command.c" "ble_config.c"
                       INCLUDE_DIRS "."
                       EMBED_FILES "assets/studio.htm.gz"
                       REQUIRES esp_wifi esp_netif esp_event esp_http_server esp_driver_spi esp_timer driver nvs_flash json bt mbedtls)
