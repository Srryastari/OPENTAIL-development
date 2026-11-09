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

- The actual CORE runtime now uses the v4 split rather than only the portable host model:
  - `ot_output_cfg`: hardware-only `lane_active_v4` / `lane_stage_v4` records (lane identity, active pixel capacity, color order, RGB/white calibration and gamma).
  - `ot_zone_map`: geometry-only `map_active_v4` / `map_stage_v4` records, the cross-namespace pair intent and the v4 initialization marker.
  - `ot_trig_cfg`: the existing IN1..IN8 function profile/journal plus the separate CRC-protected `policy_v1` physical-input policy record.
  - `ot_fx_presets`: effect snapshots remain independently owned and are not geometry.
- The pair-store is shared by boot recovery, runtime save, reset and activation. It commits and verifies nine durable phases: lane stage, map stage, intent, lane active, map active, marker, lane-stage erase, map-stage erase and intent erase.
- The v4 marker/intent/active records establish authority. Once any authoritative v4 record exists, invalid or missing halves fail closed; legacy records are never used as a fallback. A power cut before the initial intent leaves legacy data authoritative and the staged v4 records are safely replaceable.
- Legacy v1/v2 combined output profiles and the v3 combined Panel/Zone journal/tombstone remain read-only migration inputs. Their bytes are not rewritten or erased during migration. The first successful migration splits the joined runtime configuration into lane-only and map-only records.
- Pair snapshots, readback and split/join scratch use one bounded static workspace (compile-time capped at 16 KiB); journal CRC calculation no longer creates full-structure stack copies.
- Trigger policy migration writes only approved policy values: active-low polarity, four 10 ms samples for sampled-input assertion qualification, and a 400,000 µs non-blocking release hold for physical IN3/IN4 (mask `0x0C`). IN3/IN4 retain immediate ISR assertion; the 400 ms hold applies only to release. It never stores live timer, edge timestamp or debouncer state. Existing mapping and BLE profile wire formats remain unchanged. A corrupt, unsupported or build-mismatched policy record fails closed rather than being overwritten.
- Section 6C reset is a two-step authenticated, revision-bound challenge/confirmation on the existing Panel command characteristic. Request is [v3, FF, AA, lane, revision LE16]; response returns a 32-bit random challenge, revision and 5-second TTL; confirmation is [v3, FF, AB, lane, challenge LE32, revision LE16]. It rejects active physical inputs, Show/Demo/Simulation, diagnostics/preview, pending configuration, a pending STOP render, and watchdog/fault inhibit.
- Reset clears only the selected lane's Panel/Zone geometry through the v4 pair transaction while preserving that lane's hardware record and saved effect presets. The reset mask is carried in the map journal and verified active pair; existing v3 reset tombstones are read-only migration inputs, not newly written. Reset reports success only after authoritative active lane/map records and the reset mask are re-read and validated. Reconfirming an already durable reset bit is idempotent and does not consume another NVS commit.

### Commissioning fallback and diagnostics

- Idle with no input, diagnostic session or authorized show is dark.
- Unassigned pixels and unmapped gaps use the global fixed-input fallback: Brake red on both lanes; Parking dim red on both; Reverse white on both; Left amber on physical Output 1; Right amber on physical Output 2; Hazards amber on both.
- Explicit OFF remains distinct from UNASSIGNED.
- The exact five-byte Pixel Probe remains authenticated and has a 2-second volatile lease.
- The cumulative Fill-to-Index primitive exists in CORE and reports endpoint+1 pixels. Its proposed separate BLE command is not accepted or advertised pending owner review. No visual sweep is implemented.

### Live arrangement preview — dedicated 0xFE transaction protocol

The earlier draft-only 0x10–0x14 preview upload shim has been removed. Preview now uses separate BLE GATT characteristics, so durable Panel v3 configuration commands and transient arrangement traffic cannot share upload state:

| Characteristic UUID suffix | Access | Purpose |
|---|---|---|
| 0x35 | Write with response | BEGIN, COMMIT, CANCEL, HEARTBEAT control |
| 0x36 | Write Without Response | Sequenced real-time range UPDATE |
| 0x37 | Read | Transaction status, revisions, candidate CRC32 and render acknowledgement |

All three characteristics require the active authenticated owner connection and a prior Panel capability read. The session ID is a fresh random 32-bit value bound to that BLE connection, lane and expected base revision.

**Control packets (little-endian multi-byte fields):**

| Operation | Exact payload |
|---|---|
| BEGIN (0x01) | `FE 01 lane base_revision:u16 capacity:u16 schema_version` — 8 bytes |
| COMMIT (0x03) | `FE 03 lane session_id:u32 candidate_crc32:u32` — 11 bytes |
| CANCEL (0x04) | `FE 04 lane session_id:u32` — 7 bytes |
| HEARTBEAT (0x05) | `FE 05 lane session_id:u32` — 7 bytes |

**UPDATE (0x02) packet:**

- Fixed 11-byte header: `FE 02 lane session_id:u32 sequence:u32`.
- Followed by 1–4 fixed 8-byte descriptors: `panel_id:u8, zone_id:u8, range_slot:u8, start:u16, count:u16, role_flags:u8`.
- The entire packet must fit one ATT write command. Maximum descriptors are `min(4, floor((ATT_MTU - 3 - 11) / 8))`: one at MTU 23, two at MTU 31, and four at MTU 46 or higher.
- `range_slot` is the current zero-based range-array slot within the zone. `role_flags` bits 0–3 carry the zone role, bits 4–5 carry action (0 upsert, 1 delete range, 2 delete zone, 3 delete panel), and bits 6–7 must be zero. Delete actions require zero start/count and role fields.
- UPDATE is applied as one candidate snapshot. Invalid IDs, duplicate targets, range overlap, out-of-capacity endpoints, cross-panel moves, and invalid role/action combinations are rejected without changing the previous preview snapshot.

Sequence starts at 1 and only the contiguous next nonzero 32-bit sequence is accepted (UINT32_MAX wraps directly to 1; zero is reserved). Identical same-sequence/same-packet duplicates are counted and never reapplied. Forward gaps, out-of-order packets and same-sequence/different-payload conflicts are rejected; a hard sequence-sync fault blocks subsequent UPDATE and COMMIT until CANCEL, authoritative map reload and a new session. Duplicate packets raise a warning and require status/CRC reconciliation before continuing. The 5,000 ms geometry-preview lease is renewed only by HEARTBEAT, not by UPDATE. The separate five-byte Pixel Probe diagnostic lease remains 2,000 ms.

### Owner-mandated protected COMMITTING and telemetry

- BLE UPDATE admission checks COMMITTING before session/sequence classification. Every delayed UPDATE received after COMMITTING is rejected with explicit error code **5 / COMMITTING_LOCKED** before calling the shadow update API.
- The COMMITTING state and immutable candidate remain intact after rejected updates. The same error is returned for conflicting BEGIN/CANCEL/HEARTBEAT/COMMIT and legacy geometry writes; only an exact duplicate COMMIT with the same session and CRC is idempotent.
- The legacy Panel write path and CORE geometry save/reset paths remain blocked while the preview candidate is active/committing. The NVS worker remains the only owner of the preview's durable write.
- UUID suffix 0x37 now returns a 61-byte version-2 status value. The v1 prefix retains lane/session identity, last accepted sequence, candidate CRC, active/staged/render revisions, error and lease fields. The extension exposes last rejected sequence/reason, saturating missing/out-of-order/duplicate/conflict/COMMITTING-rejection counters, and sequence warning/fault flags. Panel capability feature bit 3 advertises this telemetry.
- Error codes 5–10 are documented in ISSUE126-PREVIEW-FAULT-INJECTION-ANDROID-TELEMETRY.md. Android must surface sequence warnings immediately in the existing Settings → System area, reconcile the authoritative status/map, and never blindly retry or commit after a hard sequence fault. The approved six-screen UI is unchanged.
- Host protocol tests and exact-source contract checks for commit-locked delayed, duplicate, gap and out-of-order packets **PASS** at code SHA `d5ed5cf7ae74386d545c1cea3ae9219ac603165f`. A subsequent logging-only firmware commit and documentation update are queued for exact-HEAD rerun.
- CORE now assigns a random 32-bit per-boot diagnostic identity, logs it at startup, and exposes it as `boot_id` in the existing `/api/v1/status` JSON. The brownout log must capture pre-cut and post-reboot values on the exact tested SHA.

- BEGIN binds the preview to the committed lane revision and its validated active pixel capacity. Unknown/unconfigured capacity cannot start a geometry preview.
- UPDATE changes only the DRAM shadow candidate; it does not write NVS. CORE snapshots the candidate at a render-frame boundary and continues the existing global Show Mode/effect session.
- COMMIT supplies the CRC32 of the canonical full two-output Panel v3 wire image. CORE independently recomputes it and queues an immutable candidate for the existing configuration worker; the BLE callback performs no NVS write. The worker stages it through the crash-safe journal and the promotion task publishes it. Status remains COMMITTING until the new active revision has been rendered by a successful RMT completion. A disconnect before the worker starts discards the transient request; once the worker has begun durable staging, recovery/promotion continues independently of the BLE session.
- CANCEL discards only a non-committing preview. Once COMMITTING begins, cancellation is rejected; a disconnect does not erase a durable staged commit.
- HEARTBEAT extends the lease only for the matching authenticated session, lane and base revision.
- Expiry, disconnect, STOP, watchdog decorative inhibit and output fault cancel volatile preview state. Physical input frames always use committed geometry and physical safety arbitration; the preview may resume after release only while its lease remains valid.
- Save during Show Mode does not stop or restart playback. Trigger and legacy Output promotion retain their existing Show Mode exclusion gates.

No automatic visual sweep is implemented. The cumulative Fill-to-Index BLE command remains unadvertised pending its separately approved protocol.

### Trigger ISR telemetry (Issue #126 section 6D)

- The existing per-lane SPSC turn-edge queues and writer-active protocol are preserved; no generic MPSC replacement was introduced.
- The ISR now rejects invalid input indices before pin/counter lookup and uses the existing IRAM-safe GPIO LL read.
- Aligned DRAM 32-bit atomics count raw, normalized asserted and deasserted edges for IN1–IN5.
- IN3/IN4 queue depth, high-water, overflow flag and cumulative overflow count are exposed through an authenticated read-only characteristic in the existing BLE service (UUID suffix 0x34). There is no new unauthenticated HTTP endpoint.
- A compile-time lock-free assertion guards 32-bit ISR atomics. The high-water update uses one bounded compare/exchange attempt (best-effort under contention) rather than an unbounded ISR retry loop. ESP32-S3 disassembly/IRAM-map confirmation remains a CI evidence gate.

### Brightness contract and bench protocol (Issue #126 section 6B)

- The build-time ceiling is `CONFIG_OPENTAIL_MAX_BRIGHTNESS_LIMIT` / `OT_MAX_BRIGHTNESS_LIMIT`, Kconfig range 1–255, default 32. The legacy `OT_MAX_BRIGHTNESS` name aliases this setting.
- The current effective ceiling remains 32/255. It is a conservative software ceiling, NOT a measured current limiter. It has not been raised.
- All color and master-brightness values remain 8-bit RGB (0–255). Firmware reports the effective ceiling in REST status/capabilities and the authenticated Panel v3 capability descriptor.
- Physical trigger, unassigned fallback and diagnostic paths produce full-scale RGB values and share the same final ratio-preserving ceiling immediately before frame capture/RMT swizzle. Decorative calibration uses an 8-bit gamma domain (0–255); it no longer contains a fixed 32/255 gamma ceiling.
- The Kconfig ceiling is configurable for controlled bench builds, but do not select 64, 128 or 255 until the corresponding electrical/thermal step has been measured and approved. The existing default stays 32.
- Host tests cover requested values 0, 1, 32, 64, 128, 200 and 255; trigger/fallback colors; gamma; the final RGB cap; and a separate 255/255 build.

**Required physical bench sequence — NOT RUN:**

1. Use a current-limited, appropriately fused supply and measure the actual strip rail at the source and furthest injection point. Record voltage and current at each permitted step.
2. Test a representative 944-pixel strip at ceilings 32, 64, 128 and 255 only while the supply, fuse, wiring, connectors and strip remain within verified ratings. Stop escalation immediately at the approved current, voltage-drop or thermal limit.
3. Run continuous white-fill for up to 15 minutes at each approved step. Record controller/driver, connector, wire and strip temperatures with an IR thermometer; do not assume a MOSFET is present unless the actual board has one.
4. The installed bench strip is WS2815 12 V. Measure its 12 V LED rail at the furthest power injection point against the strip's specified operating range. The proposed 4.75 V threshold applies only to a separate 5 V logic/bus supply; monitor that rail separately and do not apply 4.75 V to the 12 V LED rail.
5. Repeat the comparison against BlueGhozt using identical strip sections, power supply/voltage at the strip, color/pattern, illuminated pixel count, geometry and fixed camera exposure or a lux meter. Record actual voltage, current and optical readings.

No 64/128/255 electrical measurements, 15-minute soak, controlled BlueGhozt A/B or full-brightness parity result is claimed. The 20 A bench fuse is protection, not a current regulator or proof of safe maximum load.

## STOP/system-inhibit audit

The existing STOP command stops Show/Demo/Simulation and clears diagnostics/preview. It intentionally leaves physical Brake/Turn/Reverse authority intact. The current baseline has no separate physical global system-inhibit input/path. No GPIO or hardware interlock is invented here. Issue #126's all-pixels-black global inhibit behavior requires the owner to identify/approve an actual inhibit source before implementation; it must not silently suppress vehicle safety signals.

## Capacity and timing estimates

- Compile-time raw buffer capacity: 2,048 pixels per output lane; this is not a detected strip length.
- At 800 kHz, 2,048 pixels require at least 61.44 ms of 24-bit wire time per lane before reset/latch overhead (about 16.3 FPS theoretical). This is protocol math, not measured frame timing.
- New render working arrays are approximately 34,816 bytes for two lanes at 2,048 pixels: physical RGB frames, ownership map, logical indices, prior ownership and one logical RGB scratch frame. This excludes effect states, palettes, controller snapshots, RMT buffers and task stacks.
- Host tests print the sizes of the Panel/Zone controller and preview objects. ESP-IDF linker/map, heap and powered maximum-capacity timing measurements remain pending.

## Verification status

- The v4 cross-namespace lane/map runtime integration, nine-phase pair-store fault matrix, reset-mask recovery, trigger-policy migration/CRC, protected COMMITTING protocol and telemetry contracts are implemented in CORE.
- Full CORE host regressions and exact-source contracts **PASS** at code SHA `d5ed5cf7ae74386d545c1cea3ae9219ac603165f`; this includes the v4 pair-store restart/fault tests, marker read-error fail-closed test, reset single-write contract, and the versioned trigger-policy record tests.
- Trigger integration **PASS** at the same source checkpoint. ESP-IDF v5.5.5 ESP32-S3 build, linked ISR IRAM/DRAM audit and exact-head artifact are still running for that checkpoint; no device is flashed by CI.
- The current PR branch head is a documentation-only descendant of that source checkpoint; the latest firmware logging change is queued for exact-HEAD rerun.
- Independent exact-HEAD review, physical bench/electrical preflight, controlled brightness A/B, 944-pixel RMT/optical/thermal tests and 100-cycle brownout endurance remain open.
- Passing software CI is not physical or road certification.


## Owner-mandated brownout endurance gate

The mandatory 100-cycle physical power-cut/recovery test under a measured full-white 944-pixel WS2815 load is specified in ISSUE126-BROWNOUT-100-CYCLE-ACCEPTANCE.md. **NOT RUN.** The exact tested v4 cross-namespace journal must be built and flashed only after verified electrical preflight; the run must capture boot-ID evidence and physical phase instrumentation for every cycle. A CI build or host fault simulation cannot satisfy this gate.
