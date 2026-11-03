#pragma once
/* ESP-IDF 5.5.5 adapter: ONE ESP32-S3, no added GPIO or reset supervisor.
 * Uses the IDF TWDT and RTC no-init state; no second MCU/LED takeover. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void ot_sw_idf_boot_record(void);
bool ot_sw_idf_configure(void);
bool ot_sw_idf_watch_inputs(void);
bool ot_sw_idf_watch_render(void);
void ot_sw_idf_cancel_inputs(void);
void ot_sw_idf_cancel_render(void);
void ot_sw_idf_input_sampled(void);
void ot_sw_idf_render_cycle(bool all_outputs_completed, bool diagnosed_fault,
                            bool configuration_fault);
bool ot_sw_idf_decorative_inhibited(void);
bool ot_sw_idf_stable(void);
bool ot_sw_idf_boot_loop_seen(void);
void ot_sw_idf_status_json(char *buf, size_t size);
