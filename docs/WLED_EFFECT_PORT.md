# WLED effects and 1D particle port — staged integration

## Status
First implementation: standalone **original OpenTail 1D particle adapter**, with a host-side C++ smoke test. It is *not* a claim that WLED's full effect library or WLED's original particle engine has been ported. CORE's active baseline source is not yet extracted or built, so this header is deliberately unintegrated.

## Upstream sources inspected (WLED)
- https://github.com/wled/WLED/blob/main/wled00/FX.cpp
- https://github.com/wled/WLED/blob/main/wled00/FX.h
- https://github.com/wled/WLED/blob/main/wled00/FX_fcn.cpp
- https://github.com/wled/WLED/blob/main/wled00/FXparticleSystem.h
- https://github.com/wled/WLED/blob/main/wled00/FXparticleSystem.cpp

The upstream WLED code and particle engine are licensed **EUPL-1.2-or-later**. Do not copy upstream implementation into OpenTail's potentially proprietary CORE without a deliberate license/legal decision. Keep attribution, upstream provenance, and distribution obligations where required. This branch's Particle1D.h is an original lightweight adapter informed by the public API concepts, not a copied WLED engine.

## Intended architecture
- Retain WLED's supported effects in the existing WLED runtime when possible; use WLED's JSON segment/effect API rather than duplicating built-in effects.
- Port genuinely missing custom effects into an optional engine that writes only to caller-owned pixel buffers.
- Establish per-output/zone logical 1D indices. No 2D matrix or LED-coordinate mapping is required for the current taillight layout.
- Provide effect parameters: speed, intensity, width, direction, color, palette, FPS budget, and deterministic selection IDs.
- Never let show effects overwrite parking/brake/turn/reverse priority; road-lighting arbitration always wins.
- Startup/shutdown transition animations are opt-in and must respect power-off sequencing and fallback ownership.
- Reverse direction is a logical 1D index transform, not a rewiring requirement.

## Integration gates before enabling on hardware
1. Extract archived CORE v0.1.24 source into the working tree and successfully build/flash the baseline.
2. Add an adapter from CORE output-zone buffers to Particle1D; do not change the approved Studio tabs.
3. Run host unit tests, sanitizer builds when available, and device soak tests with multiple active zones.
4. Benchmark render time and memory on ESP32-S3 with target strip sizes; enforce per-zone particle limits and frame-time cap.
5. Verify simultaneous park/brake/turn/reverse masks remain correct with effects running; verify stop/restore and restart persistence.
6. Confirm physical WS2815 color order, brightness limits and power behavior. Do not claim road safety or watchdog validation from software tests.

## Host smoke test
`g++ -std=c++17 -Wall -Wextra -Werror core-firmware/effects/tests/particle1d_smoke.cpp -o /tmp/opentail-particle-test && /tmp/opentail-particle-test`

## Scope limitations
This first slice supports particle emit, TTL, motion, optional acceleration, wrap/bounce/kill boundaries, brightness fade and additive 1D rendering. It does **not** yet reproduce WLED particle collision, emitters, palettes, blur, per-particle shapes or full WLED effect parity. FPS targets of 60–120 remain unverified.
