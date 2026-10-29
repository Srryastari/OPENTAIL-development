# OpenTail CORE — Authenticated Trigger Mapping BLE v1 (bench only)

The CORE-owned trigger contract complements the existing Output Manager and does **not** introduce a second renderer or separate GPIO wiring model. Android (Kotlin/Compose) and any future client may use this optional GATT characteristic after owner HMAC authorization. The original bench HTTP `/api/v1/triggers` endpoint remains compatible.

## GATT

Service: `55a56022-4915-7a92-cf44-b8b7102eaf21` (existing CORE service; UUID in BLE little-endian `BLE_UUID128_INIT` format).

Characteristic: same service prefix with lowest byte **0x2c** (C initializer `0x2c,0xaf,0x2e,...,0x55`), **READ | WRITE**, only while the active connection is owner-authorized. Verify UUID using the CORE source rather than constructing it from this mnemonic.

### Read response, exactly 15 bytes

| Offset | Type | Meaning |
|---|---|---|
| 0 | u8 | protocol version = 1 |
| 1 | u8 | result of the previous mapping write: 0=OK, 1=invalid, 2=busy, 3=stale revision, 4=NVS failure |
| 2–3 | u16 LE | current mapping revision, initialized to 1 on boot |
| 4 | u8 | debounced physical IN1–IN8 bitmask, bit 0 = IN1 |
| 5 | u8 | logical effective mask (remapped physical inputs OR temporary simulations) |
| 6 | u8 | 1 if the mapping was persisted, otherwise 0 |
| 7–14 | 8 x u8 | logical function assigned to IN1..IN8, positional |

### Write request, exactly 11 bytes

| Offset | Type | Meaning |
|---|---|---|
| 0 | u8 | protocol version = 1 |
| 1–2 | u16 LE | expected revision from the last read |
| 3–10 | 8 x u8 | replacement mapping for IN1..IN8 |

Logical function IDs: 0=disabled, 1=park, 2=brake, 3=left, 4=right, 5=reverse, 6=aux1, 7=aux2, 8=aux3. Duplicates and disabled inputs are intentionally supported. These are **functions**, not GPIO numbers; the existing board owns its pin allocation.

The write is acknowledged at ATT level; clients must **read back** the characteristic and check status, revision, mapping and saved flag. Reject stale/busy results rather than assuming the change took effect. Each packet fits the default 23-byte ATT MTU; there is no multi-chunk transaction or BLE frame streaming.

## Compatible bench HTTP concurrency guard

`GET /api/v1/triggers` still reports `revision: 1` as the **storage/schema version**. It now also returns `config_revision`, the live edit revision shared with BLE. For clients that read then edit the mapping, include `expected_config_revision` with that value in the existing JSON POST body. An intervening successful HTTP or BLE edit returns HTTP 409 and never overwrites the newer mapping. The CORE `/dev` trigger editor now supplies this field automatically. Legacy HTTP requests without `expected_config_revision` retain the existing version-1 request shape; their saves are serialized but do not have stale-draft detection.

## Arbitration and persistence

- CORE alone samples physical inputs; a BLE disconnect must not disable any physical trigger.
- Four stable samples at 10ms nominal sampling remain the physical debounce contract.
- A mapping edit is rejected while any physical input, simulated input, demo, or show is active.
- HTTP and BLE saves use the same configuration mutex and NVS commit-first transaction. Failed storage writes leave the prior in-memory profile intact.
- The mapping revision increments only after a successful edit. It is a per-boot concurrency guard, not the stored schema version (NVS trigger profile remains version 1).
- Rendering owns the final pixel writer. Mapped Brake/Turn/Reverse/AUX override only assigned universal output zones; Parking yields where a selected show is active. Trigger release restores ongoing effect phase.
- This protocol **does not yet** expose per-trigger effect animation assignment, per-trigger custom colors, wiring auto-detection, independent failover, or vehicle-safe optical response measurements.

## Prior bench observations as regression fixtures

The earlier physically observed input masks are preserved explicitly in `core-firmware/tests/test_verified_bench_contract.c`. These are **historical observations from an older flashed build**, not newly measured hardware results on this source:

- Individual inputs: IN1..IN8 on GPIO4..11 produced masks `1, 2, 4, 8, 16, 32, 64, 128`; release produced `0`.
- Previously reported combinations: `3, 5, 6, 9, 10, 12, 14, 15, 17, 18, 30, 31, 224, 255`. Hazard inputs = `12`; Brake + hazards = `14`; Parking + Brake + hazards = `15`; Brake + hazards + Reverse = `30`; adding Park to that = `31`.
- Earlier user's bench observations described both turn zones amber for hazards, the Brake zone red over Parking, Reverse on its assigned zone, and an interrupted effect resuming after release. Those optical observations require **fresh physical confirmation** on the new firmware; host overlay tests do not measure effect phase or photons.
- The regression fixture applies every historical mask to the identity mapping and verifies expected role-local trigger colors **independent of geometry**, including no-show Parking baseline and uninterrupted pre-existing show pixels for nonmatching roles.
- A test-only illustrative sequence `0 → 1 → 15 → 31 → 15 → 1 → 0` asserts overlay/restoration behavior. This is a software replay, **not** a newly observed physical sequence.
- No firmware test assumes fixed GS400 zone sizes. Existing saved profile geometry is exercised separately. Optocoupler polarity, sub-40ms response accuracy, GPIO voltage, hardware failover, controller reboot persistence, and output optical timing remain on-bench test gates.

## Expanded prior-trigger test traceability (historical, not a new flash)

The evidence below is deliberately split between an **earlier user-observed bench pass**, **recorded older software/bench output**, and a **current host-level reenactment**. None upgrades the latest firmware to a physical pass. Earlier tested zone sizes and output counts are test fixtures only, never defaults.

| Earlier test / diagnostic | Earlier evidence | Automated regression or current state |
| --- | --- | --- |
| PC817 12V IN1 release → assert → release | Reported input mask `0 → 1 → 0`; browser status initially required manual refresh | Four-stable-sample assert/release in `test_trigger_manager.c`; live HTTP polling is separately covered by dashboard tests |
| Individually connect IN1..IN8, GPIO4..11 | User-confirmed masks `1,2,4,8,16,32,64,128` and all-released `0` | Every bit exercised separately through `ot_debounce` and default `ot_tm_resolve` |
| IN3 left interrupt Bluetooth Wipe, then release | Previously reported physical interruption and resumed Wipe | Actual `ot_show_pixel` Wipe phase + `ot_om_trigger_overlay` regression, including restoration using a later clock time; no physical RMT claim |
| IN3+IN4 hazards | User-confirmed simultaneous `12` | Both role-local turn overlays checked with and without show selected |
| IN1+IN2 Parking+Brake | User-confirmed combined mask `3` | Red Brake precedence over dim-red Parking, both during active show and without one |
| Brake + hazards and reverse combinations | Reported `14`, `15`, `18`, `30`, `31` with Brake red, turns amber and Reverse white | End-to-end logical mask/role overlay replay; `31` is the clean follow-up after one anomalous observed reading `231` |
| Other previously recorded combinations | `5,6,9,10,17` and `224,255` reported during older input-matrix testing | Same replay fixture; these recorded cases should not be mistaken for freshly repeated individual optical inspections |
| Show-mode priority: Parking, Brake, Left/Right, Reverse | Earlier reported overrides and effect resume after release; agreed parking-under-show behavior | Selected-zone show yields to Brake, turns, Reverse, AUX while nonmatching zones remain on the effect; no show means Parking baseline |
| /simulate Right / Hazards / Reverse | Earlier `simulated_mask` sequence `8 → 12 → 16` | Canonical `ot_tm_signal_mask_of()` tests check exact values; physical and logical simulation mask independence checked |
| /simulate TTL 15000 ms | Earlier successful 15-second request | `OT_SIM_MAX_MS=15000` restored; CORE /dev maximum follows; separate 10-second demo policy unchanged |
| Stop/Restore while input active | Older reported Stop/Restore and physically-triggered priority checks | Source guard checks Stop clears simulation/show but **not** physical input state; logical Brake remains active when show absent |
| BLE authorization/reconnect while triggers live | Earlier connection/authorization reported, with some disconnect instability | Authenticated trigger mapping GATT codec and disconnect/physical state isolation source checks; new physical BLE reconnect acceptance pending |
| Remap/duplicate/disabled physical function | Earlier user approved independently assignable trigger inputs; save/reboot of trigger map is **not** established by these bench observations | Host mapping tests cover duplicates, disabled inputs, AUX, arbitrary IN→function mappings; NVS reboot persistence must be tested on current hardware |
| Non-GS400, user-defined zones | User explicitly prohibited fixed vehicle lighting geometry | Trigger tests use role matching independently of pixel lengths; old 522/30 saved geometry lives only in separate historical output test data |
| Optocoupler contact chatter, debounce, input polarity | Earlier raw masks verified, exact 40ms timing/polarity electrical measurement **not established** | Stable-sample and chatter software tests; actual 12V input thresholds and timing remain hardware gates |

**Important:** A host renderer test shows that the Wipe function continues according to elapsed time. It does *not* independently establish that all 120 1D effects, particles, palette transitions or a physical LED stream preserve their exact phase. Those remain effects/integration owners' responsibilities; this trigger PR changes none of their engines.

## Additional recovered historical evidence and new host coverage

- **Physical PC817 Brake observation (Oct 26, older firmware):** applying 12V to IN2 changed the *onboard status LED* from rainbow to purple, remained purple while held, and returned to rainbow when released. Starting a Wipe while Brake was held still produced purple on the *status LED*. This established that the older controller detected the input, **not** that the external WS2815 pixels had passed Brake-over-Wipe; the newer approved status-LED pattern rules should not be overwritten just to mimic that older color.
- **Previous saved-zone diagnostics (Oct 27, older firmware):** Tail A had Rainbow selected while Brake was physically active; Tail B had Brake active; the Turn/Reverse zones did not incorrectly assert; releasing IN2 restored `effective_mask: 0` while Tail A remained show-selected. The new `test_prior_trigger_profiles.c` exercises the analogous independent, synthetic role layout on two outputs and demonstrates that clearing Brake preserves the underlying selected show pixel. It does **not** claim a new physical pixel test or duplicate the earlier vehicle's geometry.
- **Independent output selection (older test):** GPIO17/Output 2 selection was independent of GPIO16/Output 1 selection. The new host test uses distinct zone lengths and roles for both outputs and iterates their trigger priorities independently; optical selection and data transport remain firmware/hardware integration tests.
- **Universal input reassignment:** Beyond the older physical identity mask tests, the new host suite checks **8 pins × 9 supported functions × all 256 raw masks** (18,432 mapping cases), including Disabled, arbitrary duplicate functions, one-function-for-all-inputs, last-source release, and physical Brake coexisting with simulated Reverse. These are **new software tests**, not claimed as historical physical passes.
- **Previously unsuccessful initial wiring attempt:** earlier optocoupler testing included an instance where grounding GPIO48 froze the onboard status light before the correctly isolated pin map was established. This was an earlier wiring issue, not evidence that input debounce or arbitration is incorrect. Exact optocoupler output voltage, active-low polarity and on-board status pin allocation still require direct hardware validation.

## Handoff / acceptance

Native Android owns the presentation: a trigger mapping editor may add read/write/readback using this packet, preserving the approved UI. Do not edit Android from the trigger branch.

Host regression tests cover all eight logical functions, malformed BLE mapping writes, revisions decoding, remapped physical + simulated AUX and all 256 masks across all universal zone roles. **GATT on a physical phone, real GPIO sampling, optical timing, saved mapping after power loss, and live brake/turn/reverse priorities remain physical bench acceptance gates.**

Source branch: `triggers/aux-simulation-universal-arbitration`; stacked on CORE draft PR #19. Do not flash or merge it as a road-lighting controller.
