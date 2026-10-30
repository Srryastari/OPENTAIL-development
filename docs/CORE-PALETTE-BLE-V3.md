# OpenTail CORE custom palette BLE v3 — integrated candidate

Status: **bench-only integration**. Firmware owns BLE/HMAC, NVS, profile revisions and frame snapshots. Android owns the client UI. Effect rendering source is independently reviewed on Effects PR #31. No production safety, timing or hardware-failover claim.

## Identity and compatibility
- GATT service retains existing CORE service UUID; new characteristics are 128-bit UUID family ending byte **2D** (command, write), **2E** (response, read), **2F** (capabilities, read). All other UUID octets are unchanged from the CORE service family.
- The existing BLE v1 show/FX, v2 output config, trigger edit packet, 261-byte output mapping, and saved output/scene records are byte-for-byte unchanged.
- **Capabilities response (6 bytes)**: `[3,128,18,15,200,73]` (version, max slots, max stops, max payload bytes per DATA, first custom palette ID, last custom ID). Palette IDs `200-index`, index 0–127. Built-in 0,2–72 and global-random ID 1 are independent. User-mod range 201–255 remains unprovided and fails closed.
- All transactions require the **current authenticated BLE owner**. Other centrals, disconnected sessions and failed re-authentication cannot use command/response GATT.
- Write and read messages are at most 20 bytes for minimum ATT MTU. Multi-write sessions reset on disconnect or lost authorization.

## Commands, little-endian
| Opcode | Request bytes | Meaning |
|---|---|---|
| 1 BEGIN | `[3,1,index,count,base_revision:uint32,crc16:uint16]` (10 bytes) | Begins a 2–18 gradient-stop replacement; exact current revision required |
| 2 DATA | `[3,2,offset:uint16,bytes[1..15]]` | Strict sequential offsets, no overlaps/holes/replays |
| 3 COMMIT | `[3,3,index]` | Exact byte length and CRC validated, then **NVS commit before live publication** |
| 4 DELETE | `[3,4,index,base_revision:uint32]` | Reject if active FX references it; store a **revision tombstone** before deleting live palette |
| 5 QUERY | `[3,5,index]` | Read current metadata, including tombstone revision |
| 6 ABORT | `[3,6]` | Drop uncommitted transaction |
| 7 READ | `[3,7,index,offset]` | Read up to six original persisted stop bytes; absent/deleted records are rejected |

Each stop is exactly `[position,R,G,B]`. First position 0, last 255; positions strictly increasing; count 2–18. Uploads expire after 5000 ms from BEGIN. COMMIT and DELETE share CORE's NVS save mutex and fail with status 2 when the physical input sampler is unready, a raw/debounced vehicle trigger is asserted, a simulated trigger is active, or a pending output/trigger configuration must settle. A busy COMMIT leaves its validated upload intact for a retry before the original deadline; no profile mutation occurs. Successful palette mutations notify the render task, without transferring LED ownership to BLE. Invalid packets, CRCs, revisions, lengths, unsupported indices and incomplete transfers do not alter the live palette or saved NVS record.

## Response
Read characteristic returns 12 bytes:
`[3,opcode|0x80,index,status,revision:uint32,count,stops_crc:uint16,present]`.
Status: 0 OK; 1 invalid; 2 referenced/busy; 3 stale revision; 4 persistent-write failure.
For opcode 7, the same 12-byte response is `[3,0x87,index,status,offset,count,stop_data[up to 6]]`; use QUERY to obtain total `count*4` and checksum. Readback is authorized and NVS-backed, not reconstructed from interpolated renderer pixels. Querying a missing index returns `present=0`, `revision=0` (or the last committed tombstone revision).

## Persistence and rendering
- Namespace `ot_palettes`, key `p000`–`p127`; record magic/version, index, count, exact byte length, revision, 72-byte zero-padded stop region, CRC16. Read rejects invalid/tampered records. The entire NVS value is committed before cache swap; commit failure preserves the previous palette.
- Each CORE frame captures its selected palettes under one short bank lock; render uses caller-owned immutable `resolved_palette` pointers. Global random palette ID 1 has exactly one ESP32 RNG producer/tick before per-output rendering. No per-pixel NVS access, dynamic allocations, alternate physical strip writer or vehicle-specific zone configuration.
- Applying a live FX or loading a saved preset with a missing custom ID is rejected, not remapped to an unrelated color. Removing a currently referenced palette is rejected. If state becomes unavailable mid-frame, render fails closed to black in that selected zone.
- CRC and revision tests run in the firmware host suite; actual NVS flash-failure injection, BLE disconnect and optical timing remain **physical or device-integration gates**.

Scope: integration candidate of firmware #19 + Triggers #24 + palette transport and effect source adapter, not a merge of either team PR. See firmware issue #33.
