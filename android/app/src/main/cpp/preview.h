#pragma once
#include "core/effects_1d.h"
typedef struct ot_preview ot_preview;
ot_preview *ot_preview_create(uint16_t effect, size_t count);
void ot_preview_free(ot_preview *p);
/* RGB renderer only. No BLE, GPIO, NVS, trigger or physical-output dependency. */
bool ot_preview_render(ot_preview *p, uint32_t ms, uint16_t frame_ms,
    uint8_t speed,uint8_t intensity,uint8_t brightness,uint8_t palette,
    uint32_t foreground,uint32_t background,const uint8_t *stops,size_t stop_count,
    const ot_wled_audio_peak_t *audio,uint32_t *argb);
