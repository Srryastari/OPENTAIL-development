#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "status_logic.h"
// Fixed onboard status LED brightness; not user-configurable.
#define OT_ST_FIXED_BRIGHTNESS 65u
typedef struct {
    bool available,enabled,wifi_ready,show_active,ever_connected,ble_connected;
    uint8_t brightness;
    ot_st_mode_t mode;
    bool self_test_active;
    ot_st_mode_t self_test_mode;
    uint32_t self_test_remaining_ms;
} ot_st_snapshot_t;
void ot_st_init(void);
void ot_st_wifi_ready(void);
void ot_st_set_fault(const char *reason);
void ot_st_show_active(bool on); // true only while a native bench show actually renders
void ot_st_session(bool active);
void ot_st_ble_connected(bool active); // Studio heartbeat or explicit disconnect
/* Temporary onboard-pixel-only tests. Only SHOW (rainbow) and OFF(cancel).
 * Red fault blinking is RESERVED for a real controller fault.
 * Does not change show arbitration or drive external strips.
 */
bool ot_st_self_test(ot_st_mode_t mode);
void ot_st_snapshot(ot_st_snapshot_t *out);
void ot_st_format_json(char *buf,size_t size);
