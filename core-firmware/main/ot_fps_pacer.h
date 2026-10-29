#pragma once
/* OpenTail CORE adaptive RGB pixel pacer. Pure C, no board/vehicle geometry.
 * Preferred 53..120 FPS when achievable; never promises minimum optical FPS.
 * Load estimate uses protocol wire duration + observed rendering/RMT work.
 */
#include <stdint.h>

#define OT_FPS_PREFERRED_LOW 53u
#define OT_FPS_PREFERRED_HIGH 120u

typedef struct {
    uint16_t target_fps;
    uint16_t last_longest_output_pixels;
    uint32_t mean_work_us;
    uint8_t upgrade_streak;
} ot_fps_pacer_t;

void ot_fps_pacer_init(ot_fps_pacer_t *p);
void ot_fps_pacer_observe(ot_fps_pacer_t *p, uint32_t work_us,
                          uint16_t longest_output_pixels);
uint32_t ot_fps_pacer_period_us(const ot_fps_pacer_t *p);
uint32_t ot_fps_pacer_wait_us(const ot_fps_pacer_t *p, uint32_t work_us);
