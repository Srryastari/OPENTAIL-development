# OpenTail CORE custom palettes — firmware GATT protocol v1

Status: **draft / bench-only**, implemented on firmware draft PR #36, stacked on effects #35. Do not activate as road lighting or treat unmerged feature branches as release firmware.

## Compatibility and ownership

This protocol uses two new 128-bit GATT characteristics under the existing CORE service: WRITE command with UUID byte 0x2c and READ reply with UUID byte 0x2d (same remaining 15 UUID bytes as the existing FX/CFG characteristics). The existing 261-byte Output Manager v1 BLE configuration, 17-byte FX v1 packet, v1/v2 saved scene structures, Trigger Manager mapping, and GPIO assignments are **unchanged**. The underlying BLE HMAC enrollment remains bench-only, including its initial-enrollment MITM limitation.

- Custom palette storage slots 0..127 map to WLED-compatible palette IDs 200..73. Slots never shift when others are deleted.
- Palette IDs 201..255 are reserved for third-party usermods, not overwritten by this protocol. ID 1 belongs to the separate global RNG producer.
- Only source-derived effects with verified fixed-palette sampling may use externally saved custom IDs.
- FX v2 uses the **same 17-byte FX packet** as v1, changing only byte 0 from 1 to 2. V1 cannot activate custom palettes. V2 still requires authenticated GATT ownership, a valid loaded palette, and a supported effect.
- No palette change can overwrite an actively displayed custom palette. Saved scene playback refuses missing palette references and preserves the previous scene.

## Record and NVS

Each slot has its own key `ot_palettes/palNNN`, encoded as a fixed **83-byte** record: magic `4C 50 54 4F`, version `01`, slot (1 byte), revision (uint16 little-endian), stop count (1 byte), **72 bytes** of [position, red, green, blue] stops (unused bytes zero), and CRC16-CCITT over the first 81 bytes (little-endian at offsets 81–82). The record is packed independently of C struct alignment.

Validation requires 2–18 stops, position 0 for the first stop, position 255 for the last, strictly ascending positions, matching slot, nonzero revision, exact length and CRC. Invalid/corrupt records never load. The output/trigger namespaces are never rewritten during palette load.

The active RGB cache is ~6.4 KB static (128 x 16 x RGB888, plus revisions); it contains only validated 16-entry palettes. The render task copies only its current zone palette snapshots inside the existing state lock, then renders without holding that lock or calling NVS. NVS commit happens **before** the cache is swapped; failed commits leave prior active colors and revisions intact. All storage mutations are firmware-owned.

## BLE commands (20-byte maximum including header)

Write `[01, opcode, slot, payload...]` to the palette command characteristic; read reply `[01, opcode|80, slot, status, extra...]` from the palette reply characteristic. Byte fields are unsigned. A successful status is `00`.

| Op | Request | Reply extra | Notes |
|---|---|---|---|
| 01 SNAPSHOT | `01 01 slot` | revision LE (2), count (1), stop CRC LE (2) | Freezes a per-session immutable read snapshot |
| 02 READ | `01 02 slot offset` | offset, count, up to 14 bytes | Sequential/out-of-order reads permitted from same frozen snapshot |
| 03 BEGIN | `01 03 slot expected_rev_lo expected_rev_hi count crc_lo crc_hi` | none | Creates one pending, owner-scoped upload |
| 04 APPEND | `01 04 slot offset [1..16 stop bytes]` | none | Offset must equal next expected offset; no duplicates/overlap |
| 05 COMMIT | `01 05 slot` | none | Requires complete stop bytes, CRC, matching current revision, valid gradient, no active use, durable NVS success |
| 06 CANCEL | `01 06 slot` | none | Discards staged data |
| 07 DELETE | `01 07 slot expected_rev_lo expected_rev_hi` | none | Conflict-checked NVS erase, refuses live use |

**Status codes:** `00` OK; `01` format/range; `02` CRC/partial transfer; `03` stale revision; `04` missing record; `05` unauthorized; `06` palette in use; `07` NVS failure.

The stop CRC in BEGIN and SNAPSHOT is CRC16-CCITT with init 0xFFFF and polynomial 0x1021 over exactly `4*count` bytes, not over the 83-byte NVS record. BEGIN uses expected revision `0` for a new slot. Successful COMMIT increments revision; replaying the same expected revision is rejected. Do not attempt to wrap a revision from 65535 back to zero. APPEND and COMMIT reject idle uploads after 15,000 ms. Connecting/disconnecting resets the staging and frozen read state.

The phone should discover the new GATT characteristics before presenting custom palette saving, perform owner authorization, issue SNAPSHOT to retrieve the expected revision, send BEGIN and sequential APPEND chunks, then COMMIT and check its status. Do not assume a successful ATT write means successful NVS commit; read the separate result characteristic.

## Verification boundaries

Host tests cover strict codec, corruption, malformed stops, ID mapping, upload ordering, timeout/wrap and BLE v1/v2 source-gating. CI also reruns the full 120-mode exact-RGB goldens. Device-level NVS failure injection, BLE reconnect/MTU fuzzing, and power-cycle recovery remain separate physical/firmware acceptance tests. No parity or hardware release certificate is implied by this document.
