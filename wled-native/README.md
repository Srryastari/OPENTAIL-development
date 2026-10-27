# OpenTail / WLED native 1D renderer migration (prototype)

**Goal (owner-locked October 27, 2026):** WLED's actual 1D renderer, not OpenTail look-alike algorithms. No 2D LED map required; keep per-output and per-zone routing.

## Non-negotiable implementation decision

For **1:1 effect behavior**, native WLED must perform the rendering. Reimplementing a subset of `FX.cpp` in isolated C, even with the same numeric IDs, cannot claim full parity without the original segment runtime, palettes, scheduling, persistent effect data, blending, transitions, particle physics, random state, and audio inputs.

- **Upstream pinned source:** [wled/WLED commit `20e7a717664295eefc6ad77972cee1888d2da072`](https://github.com/wled/WLED/tree/20e7a717664295eefc6ad77972cee1888d2da072). Do **not** silently update the pin.
- **Renderer:** WLED `wled00/FX.cpp`, `FX.h`, `FX_fcn.cpp`, `FXparticleSystem.cpp`, `colors.cpp`, and WLED Segment / bus runtime. Keep upstream source intact whenever possible.
- **Control plane:** OpenTail Layout, Show Modes, Triggers and saved profiles. Their purpose is to select WLED segment configurations, not to generate per-pixel frames.
- **Current native bridge:** `opentail-wled-bridge.mjs` converts independently assigned OpenTail 1D zones to the real WLED `/json/state` `seg` schema, preserving effect IDs, palette IDs, speed, intensity, colors, brightness, reverse and mirror. It never contains a pseudo-WLED effect loop. Tests run with `node --test wled-native/tests/native-bridge.test.mjs`.
- **WLED catalog:** obtain deployed firmware's `/json/eff`, `/json/fxdata`, `/json/info`; maintain an audited upstream 1D/2D capability classification. Don't expose 2D-only or absent IDs or claim audio-reactive parity without the proper audio data source. Native effect/palette metadata and options remain WLED-owned, not fabricated by Studio.
- **Preservation:** Current `core-firmware/` ESP-IDF alpha and verified Output Manager NVS, GPIO16/17 wiring, BLE pairing, trigger/arbitration and PR #12 remain intact. No forced migration or erase-flash. The native adapter does **not** yet replace the CORE ESP-IDF renderer or provide offline BLE transport.

## Path to an actual WLED-native OpenTail CORE firmware

1. Build a fork or pinned WLED checkout for the **ESP32-S3 N16R8 (16MB flash, 8MB PSRAM)**, using WLED's official PlatformIO Arduino-based environment. The ESP-IDF `led_strip_rmt` alpha is NOT wire-compatible with the native engine just by importing effects.
2. Add an OpenTail WLED usermod / firmware bridge that maps authenticated CORE BLE effect packets and Output Manager layouts onto real WLED `Segment` state. Reconcile WLED native flash, partition layout, pairing/NVS namespaces and boot/rollback before any flash. The JavaScript JSON bridge is a reference control-plane adapter, **not a BLE implementation**.
3. Establish one global 1D segment mapping per physical chain with stable IDs; preserve independent background effects while parking acts as baseline. Brake, turns, hazards and reverse overlay only assigned zones and release to the same native effect state; don't restart unaffected segments. Validate correct WLED transitions and per-segment brightness.
4. Register all **actually compiled** WLED 1D IDs—including random, noise, palette, 1D particle and audio-reactive effects where source + dependencies are enabled—from the pinned native engine. 2D-only modes stay hidden. Validate the IDs/metadata against upstream and return capabilities over BLE; no placeholder or fake implementations.
5. Regression tests: upstream build and exact ID catalog, WLED /json/state roundtrip, CRC/persistence migration, BLE authorization, parallel-zone independence, Stop/off frame, trigger arbitration/return, WLED effects/palettes and native random/particle behavior. On actual 522 LEDs, verify GPIO16 vs GPIO17 at low brightness with level-shifted data and correctly rated fused power.
6. Maintain WLED attribution and **EUPL-1.2-or-later** licensing obligations for adapted source, notices and redistribution.

## Status gates (not yet passed)

- [x] Architecture decision and upstream source pinned.
- [x] Native WLED segment-mapping adapter with six host tests.
- [ ] Build a native WLED ESP32-S3 firmware image containing the OpenTail integration.
- [ ] Native CORE BLE/GATT adapter, saved zone settings and Show Modes capability readback.
- [ ] Demonstrate every enabled 1D mode is native WLED code with exact ID and metadata.
- [ ] On-device segment, palette, brightness, 522-pixel stability and Stop/trigger validation.
- [ ] WLED licensing and firmware distribution review.

**Do not label v0.1.25 ESP-IDF CORE or this adapter as a completed 1:1 WLED port.** Native integration is under development and unmerged; physical safety/failover remains separately unverified.
