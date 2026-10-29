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

## Handoff / acceptance

Native Android owns the presentation: a trigger mapping editor may add read/write/readback using this packet, preserving the approved UI. Do not edit Android from the trigger branch.

Host regression tests cover all eight logical functions, malformed BLE mapping writes, revisions decoding, remapped physical + simulated AUX and all 256 masks across all universal zone roles. **GATT on a physical phone, real GPIO sampling, optical timing, saved mapping after power loss, and live brake/turn/reverse priorities remain physical bench acceptance gates.**

Source branch: `triggers/aux-simulation-universal-arbitration`; stacked on CORE draft PR #19. Do not flash or merge it as a road-lighting controller.
