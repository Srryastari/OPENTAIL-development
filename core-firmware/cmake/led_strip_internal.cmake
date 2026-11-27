# Apply the allocator correction without modifying managed_components or
# vendoring/forking the driver. A different dependency requires a fresh audit.
idf_component_get_property(ot_led_strip_dir espressif__led_strip COMPONENT_DIR)
idf_component_get_property(ot_led_strip_lib espressif__led_strip COMPONENT_LIB)
set(ot_led_strip_sources
    "${ot_led_strip_dir}/src/led_strip_rmt_dev.c"
    "${ot_led_strip_dir}/src/led_strip_rmt_encoder.c")
set(ot_led_strip_hashes
    "42d57fc98147239ca23389a58fbf33cfe83dc932d46ee38d9f6104949b88c32b"
    "7aec4c1946a554cae1b307166ad0185f6335436f5ffd1cafb71e812c484f4050")
foreach(ot_idx RANGE 0 1)
    list(GET ot_led_strip_sources ${ot_idx} ot_src)
    list(GET ot_led_strip_hashes ${ot_idx} ot_expected)
    file(SHA256 "${ot_src}" ot_actual)
    if(NOT ot_actual STREQUAL ot_expected)
        message(FATAL_ERROR "led_strip 3.1.1 RMT source changed; re-audit allocator/ISR: ${ot_src}")
    endif()
    # SOURCE properties live in the dependency's own CMake directory scope.
    set_property(SOURCE "${ot_src}" TARGET_DIRECTORY ${ot_led_strip_lib}
        APPEND PROPERTY COMPILE_OPTIONS
        "-include${CMAKE_CURRENT_LIST_DIR}/../main/led_strip_internal_alloc.h")
endforeach()
