# WLED feature reference map — OpenTail CORE (2026-10-26)

This page identifies **upstream design references**, not a claim that the entirety of WLED is ported. See `docs/CORE-BLE-FX-ARCHITECTURE.md` for the locked BLE-first, no-duplicate-FX design.

Pinned source: [WLED `20e7a717664295eefc6ad77972cee1888d2da072`](https://github.com/wled/WLED/tree/20e7a717664295eefc6ad77972cee1888d2da072). License: EUPL-1.2-or-later for adapted WLED code; comply with notices/source obligations before shipping.

## 1. Particle system — private CORE subsystem

Reference: https://kno.wled.ge/features/particle-system/#1d-particle-system
Source: `wled00/FXparticleSystem.h`, `wled00/FXparticleSystem.cpp`.

- WLED 1D effects include sparks, rain, fire and bouncing balls. Positions, velocities, lifetimes, size, collisions, accelerations, age/fade and additive rendering determine the frames.
- Current OpenTail private `particle_system_1d.[ch]` implements a bounded 64-slot pool, age/lifetime, q8 motion, optional acceleration, edge kill/wrap/bounce, additive rendering and optional two-pixel fractional-position rendering. **It is original simplified code, not a verbatim WLED port or verified WLED-equivalent physics.**
- The original **Embers** effect (OpenTail ID 0x8000) now exercises this private particle backend through `effects_1d.[ch]` and the same BLE effect registry as WLED-based modes. It is not an adaptation or duplicate of any WLED PS recipe. Missing: WLED emitter semantics, drag/wind, size profiles, collisions/mass, official WLED PS recipes, palette-driven particle colors, and host/golden WLED physics parity.
- Particle physics belongs INSIDE named effect implementations. Do **not** expose a menu item called `Particle Engine`, a duplicate `Sparks` variant, an extra BLE opcode or a separate PS toggle. The single registry advertises an effect only when its recipe and all required internals are ported and tested.
- Collision checks grow with particle count squared. Bound the work per frame, lower particle count under resource pressure and never trade away physical lighting response. A PSRAM-equipped ESP32-S3 does not eliminate frame-time limits.
- No default 2D map: GS400 strips use independent 1D zones. 2D/matrix-only WLED effects are not advertised.

## 2. CCT and white handling — physical bus capabilities first

Reference: https://kno.wled.ge/features/cct/
Source areas: `wled00/colors.cpp`, `wled00/led.cpp` and the LED-bus/segment white-channel logic.

- **WS2815 is RGB-only.** It has no physical W, warm-white or cool-white emitter. A white-balance/temperature *RGB simulation* is possible but is not real tunable-CCT hardware. OpenTail Output Manager's existing `white_gain[3]` is RGB calibration only, not a CCT output.
- Reserve future types `RGB`, `RGBW`, `RGBCCT` / `WW+CW`. Offer the CCT and auto-white controls **only** for validated eligible output protocols; do not show disabled white sliders for every GS400 zone.
- On eligible hardware, distinguish WLED's auto-white policies (None, Accurate, Brighter, Dual, Max) from warm/cool CCT channel mixing. Explicitly control additive CCT mixing and power/temperature headroom. Start with linear/non-additive policy; current prototype provides no certified per-bus current limiting.
- Road-light triggers remain fixed brightness/color and never inherit show-mode palette, CCT or white-color corrections unless separately bench-validated for the applicable lamps.
- Current alpha **does not implement real CCT**, and BLE FX v1 carries no CCT field; reject or hide unsupported controls rather than simulate a white data channel.

## 3. Palettes — shared resource, not duplicate effects

Reference: https://kno.wled.ge/features/palettes/
Source: `wled00/colors.cpp`, `wled00/FX_fcn.cpp`, `wled00/FX.cpp` and palette registries.

- Palette IDs are **not effect IDs**. WLED palette 0 is `Default` (effect-dependent); WLED also defines 2 = `Color 1`, 3 = `Colors 1&2`, 4 = `Color Gradient`, and many more. Do not confuse the documented count of WLED palettes with an implemented CORE palette count.
- Preserve canonical upstream IDs and palette stops/interpolation semantics. Custom palettes should use a versioned, bounded NVS record with CRC and explicit save/transfer acknowledgements. Avoid overwriting existing Output Manager or Trigger Manager NVS schemas.
- CORE now implements a constrained color source for the original **Embers** effect: WLED palette IDs **0 Default, 2 Color 1, 3 Colors 1&2, and 4 two-color Gradient**, with pure C lookups/host checks. Other WLED-compatible effects still accept only `palette_id=0`, and **no claim of full WLED palette interpolation parity** is made. BLE and flash presets reject unsupported IDs. WLED IDs 1 (Random Cycle), 5 (Colors Only), curated palettes and custom palette upload remain future work.
- Effect/palette capability metadata drives the ONE Show Modes editor; adding palettes does not create a second effects list. A particle effect resolves colors from exactly the same palette pipeline as any other compatible effect.

## 4. Multi-strip — independent outputs, not Wi-Fi streaming

Reference: https://kno.wled.ge/features/multi-strip/
Source: `wled00/led.cpp` and target bus drivers.

- WLED documents up to **12 strips on ESP32-S3 using parallel I2S**. That is a capability of WLED's specified driver path, **not evidence** that our existing `led_strip_rmt` alpha, selected pins or enclosure can drive 12 outputs simultaneously.
- Current OpenTail CORE has **2 configured RMT physical outputs**; `output_manager.[ch]` supports up to 8 contiguous zones per output, with runtime names, pixel counts and BRG order. Our 12-output/roughly 6,000-pixel target is a future, measured hardware/driver milestone.
- Keep per-output protocol, count, color order, calibration and ownership separate from per-zone effects, palette and direction. Never assume both sides of a car share one LED chain. Keep output power injection external/fused.
- Independent effect phase/state per zone; update each segment once per output/frame. BLE is for settings and presets, **never thousands of per-pixel data writes**. No Wi-Fi, remote WLED controller or network rendering is required.
- Measure CPU, free heap/PSRAM, RMT/I2S allocation constraints and actual 800 kHz WS2815 refresh times per output. Longer strips may preclude the desired high FPS. Count and driver choices must be verified on the physical ESP32-S3.

## Porting order and gates

| Stage | Deliverable | Evidence required |
|---|---|---|
| A (current) | 1D host-tested WLED FX IDs, internal bounded particle utilities, BLE FX commands | Host CI, ESP-IDF build, Android compile, Studio checks; physical frame parity still unknown |
| B (software integrated) | Existing Android Studio Show Modes catalog driven by CORE Bluetooth capability readback; legacy BLE FX picker removed from injected Android UI | Mocked JavaScript integration checks and APK compile; physical Android BLE/GATT and on-screen review still pending |
| C (partial) | Original Embers uses private CORE particle physics; upstream WLED palettes/PS effect algorithms remain unported | Host simulation tests; hardware WS2815 playback and official WLED golden parity still pending |
| D | CCT/RGBW eligible-bus support, auto-white safety and calibration | Independent RGBW / tunable-white hardware validation |
| E (hardware blocked) | Expand beyond 2 RMT outputs toward 12 and roughly 6k pixels | Requires actual ESP32-S3 parallel I2S driver allocation, reserved pinout, scope timing, electrical protection and hardware qualification; 2 outputs currently compiled |
| F (partial) | Eight CRC/format-checked NVS FX slots and explicit BLE Save/Play/Delete; no reboot autorun | CI tests; real flash/reboot/disconnect not yet tested. Startup/shutdown sequences and independent road-light hardware fallback not implemented |

All source-ported effects and library components remain **BENCH ONLY** until the stated evidence exists. License review remains a release gate.
