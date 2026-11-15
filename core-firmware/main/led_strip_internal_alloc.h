#pragma once
/* Forced include ONLY for led_strip 3.1.1's two RMT implementation files.
 * Their SHA-256 hashes are checked by CMake. Never change global malloc policy,
 * SPI/status LED allocations, IDF GDMA symbol storage or other components. */
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "esp_log.h"
static inline void *ot_led_strip_internal_calloc(size_t count,size_t size){
    void *p=heap_caps_calloc(count,size,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if(p){
        const size_t bytes=count*size; /* calloc has already checked overflow. */
        const bool internal=esp_ptr_internal(p)&&bytes>0&&
            esp_ptr_internal((const uint8_t *)p+bytes-1);
        if(!internal){free(p);return NULL;} /* No PSRAM fallback. */
        ESP_LOGI("OT_RMT_MEM","RMT allocation bytes=%u internal=1 (pixel object or encoder)",(unsigned)bytes);
    }
    return p;
}
#define calloc(count,size) ot_led_strip_internal_calloc((count),(size))
