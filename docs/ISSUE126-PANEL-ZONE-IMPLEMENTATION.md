# Issue #126 — Panel/Zone CORE implementation status

## Exact source and ownership

- Required starting HEAD: 66c2d9c843a0b434f9399811110e42d7e26c4692
- Feature branch: firmware/issue126-panel-zone-core-20261108
- Stacked PR target: triggers/issue121-turn-release-debounce-20261108 (the exact HEAD of Triggers PR #123; required starting commit 66c2d9c843a0b434f9399811110e42d7e26c4692)
- Hardware target: one ESP32-S3 N16R8. No second MCU.
- No Android/UI source changes, no merge, and no automatic visual sweep.
- Android handoff remains blocked until Overseer verifies the firmware contract and exact HEAD.

## Implemented in this draft

### Hierarchical model and rendering

Controller → Output Lane → Panel → Zone → Pixel Ranges → LEDs.

- One continuous physical strip can be divided into independently configured logical zones.
- A logical zone can own multiple discontiguous physical ranges.
- CORE builds a physical-pixel-to-zone/logical-index map, gathers one compact logical frame per effect zone, renders it once, and scatters it back to the physical ranges. Electrical strip order is unchanged.
- Per-zone effect state is retained by Panel ID, Zone ID and effect generation. Geometry-only edits preserve effect phase and unaffected zone state.
- Preview geometry is snapshotted at a CORE frame boundary. While a physical input is active, the committed geometry remains authoritative for that frame.
- All effect rendering, frame snapshots, pixel writes and RMT ownership remain inside CORE.

### Migration and current NVS boundary

- The new Panel/Zone active/staged journal has been moved to the dedicated ot_zone_map namespace.
- First-boot v1/v2 conversion is staged in ot_zone_map before promotion. Legacy profile bytes in ot_output_cfg are left untouched as migration input.
- A staged record is recoverable after reboot and promotion is still gated by raw/debounced physical-input quietness and an unchanged edge epoch.
- The existing ot_output_cfg profile_v2/profile records are still legacy combined records. The separate hardware-only lane record, cross-namespace hardware/map reconciliation, and persistent reset tombstone required by Issue #126 section 2A are NOT YET COMPLETE. The map journal currently carries a combined runtime snapshot for compatibility; it must not be described as the final hardware/map split.
- The reset tombstone and authenticated two-step reset handshake in section 6C are NOT YET IMPLEMENTED. Until then, do not erase map keys or assume a reset can never re-import legacy zones.

### Commissioning fallback and diagnostics

- Idle with no input, diagnostic session or authorized show is dark.
- Unassigned pixels and unmapped gaps use the global fixed-input fallback: Brake red on both lanes; Parking dim red on both; Reverse white on both; Left amber on physical Output 1; Right amber on physical Output 2; Hazards amber on both.
- Explicit OFF remains distinct from UNASSIGNED.
- The exact five-byte Pixel Probe remains authenticated and has a 2-second volatile lease.
- The cumulative Fill-to-Index primitive exists in CORE and reports endpoint+1 pixels. Its proposed separate BLE command is not accepted or advertised pending owner review. No visual sweep is implemented.

### Live arrangement preview

The existing authenticated Panel v3 characteristic now has a bounded volatile preview transaction:

| Opcode | Purpose |
|---:|---|
| 0x10 | Begin/renew preview with lane, payload length, CRC16 and base revision |
| 0x11 | Upload 1–15 byte chunks |
| 0x12 | Validate and atomically replace the latest preview snapshot |
| 0x13 | Save through the existing staged/active journal |
| 0x14 | Cancel preview |

- Preview lease: 2 seconds, wrap-safe and renewable.
- Preview does not write NVS. Save stages durable state; authoritative readback remains old until promotion.
- Preview is cancelled by disconnect, STOP, RMT fault and watchdog decorative inhibit.
- Panel/Zone promotion may occur during Show Mode; Trigger and legacy Output promotion retain their Show Mode gate.
- The separate hardware-only NVS schema and cross-namespace transaction remain outstanding as stated above.

### Trigger ISR telemetry (Issue #126 section 6D)

- The existing per-lane SPSC turn-edge queues and writer-active protocol are preserved; no generic MPSC replacement was introduced.
- The ISR now rejects invalid input indices before pin/counter lookup and uses the existing IRAM-safe GPIO LL read.
- Aligned DRAM 32-bit atomics count raw, normalized asserted and deasserted edges for IN1–IN5.
- IN3/IN4 queue depth, high-water, overflow flag and cumulative overflow count are exposed through an authenticated read-only characteristic in the existing BLE service (UUID suffix 0x34). There is no new unauthenticated HTTP endpoint.
- A compile-time lock-free assertion guards 32-bit ISR atomics. ESP32-S3 disassembly/IRAM-map confirmation remains a CI evidence gate.

### Brightness contract (Issue #126 section 6B)

- Requests are accepted as 8-bit values 0–255 and clamped to the currently reported effective ceiling.
- The final CORE pixel boundary applies one ratio-preserving ceiling after calibration and before frame capture/RMT swizzle. Trigger and diagnostic paths do not bypass it.
- Current effective ceiling remains OT_MAX_BRIGHTNESS=32/255. This is a conservative software ceiling, NOT a measured current limiter. It has not been raised.
- Host tests cover requested values 0, 1, 32, 64, 128, 200 and 255, RGB primaries, white and ratio-preserving output.
- BlueGhozt optical A/B, current ramp, strip voltage drop and thermal measurements remain physical bench gates and have NOT been run.

## STOP/system-inhibit audit

The existing STOP command stops Show/Demo/Simulation and clears diagnostics/preview. It intentionally leaves physical Brake/Turn/Reverse authority intact. The current baseline has no separate physical global system-inhibit input/path. No GPIO or hardware interlock is invented here. Issue #126's all-pixels-black global inhibit behavior requires the owner to identify/approve an actual inhibit source before implementation; it must not silently suppress vehicle safety signals.

## Capacity and timing estimates

- Compile-time raw buffer capacity: 2,048 pixels per output lane; this is not a detected strip length.
- At 800 kHz, 2,048 pixels require at least 61.44 ms of 24-bit wire time per lane before reset/latch overhead (about 16.3 FPS theoretical). This is protocol math, not measured frame timing.
- New render working arrays are approximately 34,816 bytes for two lanes at 2,048 pixels: physical RGB frames, ownership map, logical indices, prior ownership and one logical RGB scratch frame. This excludes effect states, palettes, controller snapshots, RMT buffers and task stacks.
- Host tests print the sizes of the Panel/Zone controller and preview objects. ESP-IDF linker/map, heap and powered maximum-capacity timing measurements remain pending.

## Verification status

- Host regression suite and exact-source contract tests are wired into the draft workflow.
- ESP-IDF v5.5.5 ESP32-S3 build and flash-manifest checks are wired into the draft workflow; no device is flashed by CI.
- Cross-namespace hardware/map migration, reset handshake/tombstone, independent exact-HEAD review, IRAM disassembly confirmation, and physical bench tests remain open.
- Passing software CI is not physical or road certification.
