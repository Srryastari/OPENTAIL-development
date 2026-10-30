#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Bench-only visual diagnostics; never a safety interlock or road-light fallback. */
typedef enum { OT_ST_OFF, OT_ST_STARTUP, OT_ST_WIFI, OT_ST_CONNECTED,
               OT_ST_DISCONNECTED, OT_ST_BLE, OT_ST_BOTH, OT_ST_SHOW, OT_ST_FAULT } ot_st_mode_t;
typedef struct { uint8_t r,g,b; } ot_st_rgb_t;
const char *ot_st_mode_name(ot_st_mode_t mode);
/* All connected transports use the locked green double pulse. */
ot_st_mode_t ot_st_connection_mode(bool wifi_session,bool ble_connected);
ot_st_rgb_t ot_st_render(ot_st_mode_t mode, uint32_t phase_ms, uint8_t brightness, bool enabled);

/* Wrap-safe for self-tests of less than 2^31 milliseconds. */
bool ot_st_test_window_active(ot_st_mode_t mode,uint32_t now_ms,uint32_t deadline_ms);
