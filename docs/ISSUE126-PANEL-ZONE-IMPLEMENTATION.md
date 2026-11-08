# Issue #126 — CORE Panel/Zone implementation contract

## Source ancestry and ownership

- Feature branch: firmware/issue126-panel-zone-core-20261108
- Required parent: Triggers PR #123 HEAD 66c2d9c843a0b434f9399811110e42d7e26c4692
- PR target: triggers/issue121-turn-release-debounce-20261108
- One ESP32-S3 N16R8. No Android, UI, OTA-discovery, or second-MCU changes.
- No automatic visual sweep. Pixel Probe and manual cumulative Fill-to-Index are the only pixel diagnostics.

## Hierarchical schema v3

Controller → Output Lane → Panel → Zone → Pixel Ranges → LEDs

- Lane coordinates are zero-based physical output coordinates.
- Panel IDs and Zone IDs share a controller-wide nonzero 16-bit namespace.
- A lane supports up to 4 panels, 8 zones total, and 8 ranges per zone.
- A range is {start:uint16,count:uint16}; count must be positive and its exclusive end must not exceed the configured lane length.
- Ranges may be adjacent or discontiguous, but may not overlap.
- Multiple physical ranges remain one logical zone. CORE gathers the logical frame and scatters it to actual physical coordinates; it never reorders electrical pixels.
- UNASSIGNED (0xff) is distinct from OFF (0). Unassigned pixels use commissioning fallback; explicit OFF remains dark except during an authorized diagnostic.
- Fresh flash keeps physical length unknown (active_pixels=0). The 2,048-pixel build budget is only the raw fallback/render buffer extent and is never written as a detected strip length.

## Migration and NVS

- Schema v3 uses separate panel_active and panel_stage keys in the existing ot_output_cfg namespace.
- v1/v2 Output Manager records are validated and deterministically migrated in RAM into one default Panel per lane, preserving names, pixel counts, color order, zone names, roles, RGB/white calibration and gamma.
- Legacy records are not reinterpreted in place. The v3 journal stores previous and candidate images, CRC32 and monotonic per-lane revisions.
- Stage is durably written before CORE exposes a candidate. The active record retains both images through promotion; a reboot or failed finalization restores the previous image and retries only after the physical-input quiet window.
- Physical input edges always preempt configuration activation. Panel/Zone Save may stage while a physical input is asserted; activation waits until raw and debounced inputs are quiet and the edge epoch is unchanged.
- Only Panel/Zone activation may occur while Show Mode is playing. Trigger and legacy Output promotion keep their previous Show Mode gate.

## BLE v3 contract

The existing authenticated BLE session and Panel v3 characteristics are reused. The 667-byte per-lane payload is little-endian and protected by CRC16-CCITT (initial value 0xffff, polynomial 0x1021); chunks carry at most 15 data bytes.

Capability descriptor (11 bytes):

| Byte | Meaning |
|---:|---|
| 0 | Transport version 3 |
| 1 | Schema version 3 |
| 2 | Max panels per lane (4) |
| 3 | Max zones per lane (8) |
| 4 | Max ranges per zone (8) |
| 5 | Feature bits: bit 0 Pixel Probe, bit 1 live arrangement preview |
| 6–7 | Maximum payload length, LE16 (667) |
| 8 | Maximum chunk data bytes (15) |
| 9 | Preview lease in seconds (2) |
| 10 | Effective firmware brightness ceiling (currently 32/255; not a measured current limiter) |

Existing commands 1–6 retain read, read-chunk, begin-save, upload-chunk, commit-save and abort-save behavior. New Panel command opcodes:

| Opcode | Request | Behavior |
|---:|---|---|
| 0x10 | version, opcode, lane, length LE16, CRC16 LE, base revision LE16 | Begin/renew one preview transaction |
| 0x11 | version, opcode, lane, offset LE16, 1–15 data bytes | Upload preview snapshot chunk; identical duplicates are idempotent |
| 0x12 | version, opcode, lane | Validate complete CRC and geometry, then atomically replace volatile preview |
| 0x13 | version, opcode, lane | Save latest preview using the existing staged/active journal |
| 0x14 | version, opcode, lane | Cancel volatile preview and restore committed geometry |

All commands require the active authenticated connection and a previously read Panel v3 capability descriptor. A preview update is a single latest-value snapshot, not a queued slider-event stream. CORE copies the validated candidate at the beginning of a render frame. Preview changes do not write NVS. A 2,000 ms renewable lease expires using wrap-safe monotonic time. Disconnect, STOP, RMT fault and decorative watchdog inhibit cancel volatile preview. CORE readback continues to report committed state; after Save, the new hierarchy becomes active only after journal promotion and authoritative readback.

Pixel Probe remains a separate exact five-byte Write-Without-Response packet:

AA | lane(01/02) | pixel_index_MSB | pixel_index_LSB | color(00 off, 01 white, 02 cyan)

Its 2,000 ms lease is volatile. Physical trigger colors preempt diagnostic output. Diagnostic output is scaled to the same effective brightness ceiling. BLE never owns RMT/DMA.

### Cumulative Fill-to-Index proposal — not enabled

The CORE primitive accepts an inclusive endpoint and reports count=endpoint+1. The separate command is proposed, not finalized or advertised, pending owner review:

AB | lane(01/02) | endpoint_MSB | endpoint_LSB | color(01 white, 02 cyan)

This packet is not accepted by the current BLE command handler. It does not reuse or overload the Pixel Probe color byte. There is no automatic sweep.

## Live Arrangement while Show Mode is active

- Preview geometry is applied atomically from a single state snapshot at a CORE frame boundary.
- The global Show Mode session, effect ID, parameters, palette, speed, intensity and start-time/phase anchor are not restarted.
- Zone effect state is retained by stable Panel ID, Zone ID and effect generation. A geometry-only range/length change does not reset that zone's effect state.
- The logical effect frame is gathered from the previous physical ranges, rendered once, and scattered to the new physical ranges. Newly included pixels are initialized without stale data; removed pixels no longer belong to the zone and are rendered by their new owner or fallback.
- Unaffected zones keep their independent state.
- If any physical input is active, preview geometry is temporarily not used for that frame; the committed map and physical arbitration remain authoritative. The preview can resume after release if its lease remains valid.
- Save while Show Mode remains active stages the map and permits Panel/Zone promotion during playback. Physical inputs still defer activation. Cancel, expiry or disconnect restores the last committed geometry unless Save has already durably staged a candidate.

## Commissioning fallback and arbitration

- Idle with no input, diagnostic session or authorized show is dark.
- For unconfigured lanes, or gaps/UNASSIGNED pixels, CORE applies raw input fallback: Brake red on both lanes; Parking dim red on both; Reverse white on both; Left amber on physical Output 1; Right amber on physical Output 2; Hazards amber on both.
- Configured roles keep their existing trigger behavior. Explicit OFF is not treated as unassigned.
- The existing STOP command stops Show/Demo/Simulation and clears diagnostics/preview; it does not suppress physical Brake/Turn/Reverse, matching the established safety contract. No new STOP GPIO is invented.
- The existing single-ESP watchdog decorative inhibit, trigger edge history, 140-effect renderer and RMT task ownership remain in CORE.

## Brightness parity and electrical safety

- The current source ceiling remains OT_MAX_BRIGHTNESS=32 (12.5% of an 8-bit channel). This is a software ceiling, not a measured current limiter.
- CORE status and the Panel v3 capability descriptor report the effective ceiling explicitly. Diagnostic output is scaled to the same ceiling rather than silently bypassing it.
- The software contract remains 8-bit RGB channel values (0–255) with a separately reported effective ceiling. The cap is not raised until the 12 V supply, voltage at the strip, current, fuse, wiring, connectors and thermal behavior have been measured.
- BlueGhozt A/B optical comparison, electrical load ramp, and voltage-drop/data-integrity measurements are physical bench gates and remain NOT RUN until measured on identical strips/supply/geometry with fixed camera exposure or a lux meter.
- At 800 kHz, 2,048 pixels require at least 61.44 ms of 24-bit wire time per lane (before reset/latch overhead), approximately 16.3 FPS theoretical. This is a protocol calculation, not a measured CORE frame-time result.
- New render working arrays at 2,048 pixels/lane account for approximately 34,816 bytes: two physical RGB frames, physical zone ownership, logical indices, prior ownership, and one logical RGB scratch frame. This excludes effect states, palettes, controller snapshots, RMT driver buffers and task stacks. The ESP-IDF linker/map and runtime heap report remain required.

## Evidence boundary

Host tests and the ESP-IDF 5.5.5 build are software evidence only. Powered-board RMT timing, 2,048-pixel memory/thermal behavior, brightness/current A/B, optocoupler waveforms, vehicle behavior and independent exact-HEAD review must be reported separately. No physical certification is claimed.
