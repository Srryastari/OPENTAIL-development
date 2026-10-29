#pragma once
#include "core_logic.h"
#include "output_manager.h"

// Provisional wiring map: verify pin labels on your exact hiBCTR N16R8 before connecting.
#define OT_OUT_LEFT_GPIO 16    // pixel output 1 (direct GPIO bench or buffered data stage)
#define OT_OUT_RIGHT_GPIO 17   // pixel output 2 (direct GPIO bench or buffered data stage)
// 8 digital outputs of a pre-verified 3.3-V-safe, isolated trigger interface.
#define OT_INPUT_GPIO_LIST {4,5,6,7,8,9,10,11}
// Set to 0 if the measured optocoupler output goes 3.3 V HIGH when 12 V is applied.
#define OT_INPUT_ACTIVE_LOW 1

// Capacity is temporary and universal; old 522/30 and other NVS profiles remain intact.
#define OT_SMALL_BENCH_LAYOUT 0
#if OT_SMALL_BENCH_LAYOUT
#define OT_CHAIN_PIXELS 30
#define OT_LAYOUT_SECTIONS {6,6,6,6,6,0,0,0}
#else
// Driver allocates the current per-output cap; no zone names, roles or sizes are fixed.
// Output Manager configuration stored in NVS determines real zone positions, names and roles.
// Neutral placeholder layout exists for show command validation only.
// It is NOT applied to a vehicle or saved to NVS.
#define OT_CHAIN_PIXELS OT_OM_MAX_PIXELS_PER_OUTPUT
#define OT_LAYOUT_SECTIONS {OT_OM_MAX_PIXELS_PER_OUTPUT,0,0,0,0,0,0,0}
#endif
#define OT_MAX_BRIGHTNESS 32 // <= 12.5% channel brightness; not a measured current limiter.
#define OT_DEBOUNCE_TICKS 4 // four 10ms samples
#define OT_DEMO_MAX_MS 10000

// Verify onboard RGB pin on your exact PCB revision (some boards need an RGB solder jumper).
#define OT_STATUS_LED_GPIO 48 // provisional onboard WS2812/NeoPixel; independent SPI2 backend
