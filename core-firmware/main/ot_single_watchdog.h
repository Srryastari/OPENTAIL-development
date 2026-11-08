#pragma once
/* OpenTail single-ESP watchdog boot policy. Pure C; no GPIO, NVS or side effects.
 * A watchdog reset cannot guarantee light output after power/chip/data failure.
 */
#include <stdbool.h>
#include <stdint.h>

#define OT_SW_RTC_MAGIC 0x4F545731u /* OTW1 */
#define OT_SW_BOOTLOOP_LIMIT 3u
#define OT_SW_STABLE_MS 60000u
#define OT_SW_MIN_INPUT_SAMPLES 100u
#define OT_SW_MIN_FULL_FRAMES 100u

typedef enum {
    OT_SW_RESET_OTHER = 0,
    OT_SW_RESET_POWER,
    OT_SW_RESET_SOFTWARE,
    OT_SW_RESET_PANIC,
    OT_SW_RESET_TASK_WDT,
    OT_SW_RESET_INTERRUPT_WDT,
    OT_SW_RESET_OTHER_WDT,
    OT_SW_RESET_BROWNOUT,
    OT_SW_RESET_CPU_LOCKUP
} ot_sw_reset_class_t;

typedef struct {
    uint32_t healthy_since_ms;
    bool active;
} ot_sw_stability_window_t;

typedef struct {
    uint32_t magic;
    uint32_t crash_streak;
    uint32_t stable_boot;
    uint32_t check;
} ot_sw_rtc_record_t;

typedef struct {
    ot_sw_reset_class_t reset_class;
    uint32_t crash_streak_at_boot;
    bool boot_loop_seen;
    bool stable;
} ot_sw_boot_t;

uint32_t ot_sw_rtc_check(const ot_sw_rtc_record_t *record);
bool ot_sw_rtc_valid(const ot_sw_rtc_record_t *record);
ot_sw_boot_t ot_sw_boot_record(ot_sw_rtc_record_t *record,
                              ot_sw_reset_class_t reason);
bool ot_sw_is_crash_reset(ot_sw_reset_class_t reason);
bool ot_sw_can_mark_stable(uint32_t elapsed_ms, uint32_t sampled_inputs,
                           uint32_t full_frames, bool output_fault,
                           bool config_fault);
/* Tracks a continuous healthy interval; unhealthy events reset its start. */
bool ot_sw_stability_window_update(ot_sw_stability_window_t *window,
                                   uint32_t now_ms, bool healthy,
                                   uint32_t *healthy_elapsed_ms);
bool ot_sw_mark_stable(ot_sw_rtc_record_t *record, ot_sw_boot_t *boot,
                        uint32_t elapsed_ms, uint32_t sampled_inputs,
                        uint32_t full_frames, bool output_fault,
                        bool config_fault);
/* Only actual completion of all configured RMT lanes, or an explicitly
 * latched/handled driver fault, qualifies as progress. A submitted transfer
 * or merely looping through the render task is NOT progress. */
bool ot_sw_render_progress_eligible(bool all_lanes_completed,
                                     bool any_lane_completed);
bool ot_sw_decorative_inhibited(const ot_sw_boot_t *boot);
const char *ot_sw_reset_name(ot_sw_reset_class_t reason);
