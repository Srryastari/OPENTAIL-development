# OpenTail CORE ↔ Native Android BLE software contract — 2026-10-30

**Scope:** Bench-only BLE protocol handoff on the consolidated CORE #51 tree plus firmware boot-stack fix #54 and the firmware BLE host-recovery patch. This document is not a vehicle/physical-release certificate. No approved Android UI source is changed by this firmware branch.

## Connection lifecycle

- Peripheral advertises as **OpenTail CORE**, service `55a56022-4915-7a92-cf44-b8b7102eaf21`; one active central/session. Advertising uses 30–60 ms intervals when idle.
- A successful GATT connection begins **unauthorized**, even if the Android OS retained the bond or cached the GATT handle. Android may reconnect directly using the remembered address/bond; it must not unpair or enroll again merely to reconnect.
- CORE requests a **15–30 ms connection interval, latency 0, supervision timeout 4 s**, but the phone is the central and may decline/override it. Rejection is nonfatal. Negotiated MTU may differ: CORE prefers **247**, and every operation must work with ATT MTU **23**. All writes remain at most 20 bytes; the effects list and frame slices use long reads.
- Normal disconnect and **NimBLE host reset** discard connection-local authentication, challenge, cached output/frame/palette replies and volatile session state. A host reset waits for the next stack-sync callback before advertising again. Neither path deletes the stored owner HMAC key, durable active/staged revisions, persistent effects/presets, or input mappings.
- Physical input arbitration and CORE rendering run independently of BLE. Clearing connection-owned previews must not suppress physical Brake/Turn/Reverse or a persistent scene. STOP retains its existing semantics.

## Authentication

- Owner enrollment: characteristic `...af27` (READ) exposes the 16-byte owner key **only while GPIO0 BOOT is held after startup**. First trust establishment is bench-only; do not automate a bypass.
- Routine auth: READ challenge `...af28` (16 bytes), then write `[0x01,0x04, first 16 bytes of HMAC-SHA256(owner_key, "OpenTail-BLE-v2" || challenge)]` to command `...af23` (18 bytes total). Check `...af24` last result `OK AUTH`; after each disconnect/reset, use a **fresh challenge**, keeping the existing Android Keystore owner key.
- Five failed HMAC attempts exhaust that connection's challenge capability. A failed reauthentication revokes temporary previews and cached owner replies, never GPIO inputs or durable configurations.
- Legacy WPA2 bench-token auth still exists for compatibility; it is **not** a production secure enrollment scheme. No secrets should be logged.

## GATT service characteristics

All UUIDs share prefix `55a56022-4915-7a92-cf44-b8b7102e`; suffix below is `afXX`.

| Suffix | Access | Contract |
| --- | --- | --- |
| af22 | READ | Firmware info/version |
| af23 | WRITE request | Auth, SHOW v1 11 B, FX 17 B, STOP 2 B, preset commands, release-temporary |
| af24 | READ | Last textual command result |
| af25 / af26 | WRITE / READ | Output mapping v2 transport, 20 B packet; 261 B per-output v1 payload, CRC16, revision compare-and-swap, chunked |
| af27 | READ | BOOT-gated first enrollment |
| af28 | READ | Fresh HMAC challenge |
| af29 | READ LONG | Versioned effect ID list; `[1,count, uint16LE IDs ...]`; count is unsigned, potentially 140, 254 max. Follow ATT Read Blob offsets. |
| af2a / af2b | WRITE / READ | On-demand logical RGB frame snapshot, sequence-checked, not a real-time LED stream |
| af2c | READ / WRITE | Trigger mapping 15 B read / 11 B write, revision-checked, 8 physical inputs |
| af2d / af2e / af2f | WRITE / READ / READ | Palette v3 transport, 20 B command, 12 B result, capability descriptor; CRC and slot revision checks |

**Android sequence:** discover / direct reconnect → services → MTU negotiation (best effort) → challenge/HMAC → read authoritative result → refresh capability and durable-config revisions → serialize GATT operations and read back acknowledgments. Do not infer active configuration merely from a successful staged-save acknowledgment. An explicit stale-revision response must prompt refresh rather than silent overwrite.

## Verification boundary

The host/source contract checks the reset callback, advertising state, owner guards, interval request, per-session cleanup, 23-byte MTU compatibility and unchanged BLE transport constants. ESP-IDF 5.5.5 compiling, Android unit/build tests, and physical phone–ESP32-S3 reconnect/latency measurements are separate acceptance steps. The 22-check physical release gate remains enforced.

**Handoff:** Android Architecture owns Android #56/#58 integration and the completed approved UI from Work. Firmware owns this BLE GAP patch only. The next consolidated exact SHA must be retested against the actual integrated Android client, and the final approved Android screen geometry must never be modified by Firmware.
