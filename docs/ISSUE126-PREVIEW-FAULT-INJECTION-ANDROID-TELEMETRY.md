# Issue #126 — Live Geometry Preview Verification and Client Contract

Status: **engineering specification; not a firmware or Android acceptance receipt**.

This document complements `ISSUE126-PANEL-ZONE-IMPLEMENTATION.md`. It defines the exact tests and Android-side behavior for the dedicated `0xFE` geometry-preview transaction and the read-only input telemetry characteristic. It does not authorize a UI redesign or a new user-facing screen.

## 1. BLE contract

All preview characteristics require the active authenticated owner connection and a successful Panel capability read.

| UUID suffix | GATT access | Purpose |
|---|---|---|
| `0x30` | Read | Panel capability descriptor |
| `0x35` | Write with response | BEGIN, COMMIT, CANCEL, HEARTBEAT |
| `0x36` | Write without response | Sequenced geometry UPDATE |
| `0x37` | Read | Preview/session status |
| `0x34` | Read | Authenticated input-edge telemetry |

The 11-byte Panel capability descriptor is:

| Offset | Size | Meaning |
|---:|---:|---|
| 0 | 1 | Panel wire version |
| 1 | 1 | Panel schema version |
| 2 | 1 | Maximum panels per output |
| 3 | 1 | Maximum zones per output |
| 4 | 1 | Maximum ranges per zone |
| 5 | 1 | Feature flags: bit 0 Pixel Probe, bit 1 live preview, bit 2 sequenced `0xFE` protocol |
| 6 | 2 | Maximum Panel wire payload, little-endian |
| 8 | 1 | Legacy configuration chunk size |
| 9 | 1 | Preview lease seconds |
| 10 | 1 | Effective reported brightness ceiling |

### 1.1 Control packets

All multi-byte integers are little-endian. The leading `FE` byte is the protocol magic; the following byte is the operation.

| Operation | Payload | Bytes |
|---|---|---:|
| BEGIN | `FE 01 lane base_revision:u16 capacity:u16 schema_version` | 8 |
| COMMIT | `FE 03 lane session_id:u32 candidate_crc32:u32` | 11 |
| CANCEL | `FE 04 lane session_id:u32` | 7 |
| HEARTBEAT | `FE 05 lane session_id:u32` | 7 |

BEGIN binds the session to the lane, current active revision, validated active pixel length and schema. CORE returns a fresh nonzero 32-bit session ID in the status characteristic. The session is bound to the authenticated connection.

### 1.2 UPDATE packet and sequence rules

UPDATE is written to UUID `0x36`:

- Header: `FE 02 lane session_id:u32 sequence:u32` (11 bytes).
- Followed by 1–4 descriptors, each 8 bytes: `panel_id:u8, zone_id:u8, range_slot:u8, start:u16, count:u16, role_flags:u8`.
- The packet must fit in one ATT write command: `11 + 8*N <= ATT_MTU - 3`.
- Maximum descriptors are `min(4, floor((ATT_MTU - 14) / 8))`: one at MTU 23, two at MTU 31, four at MTU 46 or greater.
- `range_slot` is the zero-based range-array slot in the addressed zone.
- `role_flags` bits 0–3 are the role; bits 4–5 are the action (0 upsert, 1 delete range, 2 delete zone, 3 delete panel); bits 6–7 are reserved and must be zero.
- Delete actions require zero start/count. Invalid identifiers, action combinations, overlapping ranges, invalid endpoints and cross-panel moves must not mutate the existing shadow candidate.

Sequence starts at 1 and **only the contiguous next nonzero 32-bit sequence** is accepted. Sequence zero is reserved; wrap is UINT32_MAX -> 1. A forward gap is rejected and counted as missing sequence numbers. Stale/out-of-order packets and same-sequence/different-payload conflicts are rejected. An identical same-sequence/same-packet duplicate is observed and counted but never reapplied.

A duplicate raises a synchronization warning but does not by itself freeze the session: Android must read status and reconcile the accepted sequence and candidate CRC before proceeding. A forward gap, out-of-order packet or sequence conflict latches sequence_sync_fault; subsequent UPDATE and COMMIT are rejected until the client CANCELs the volatile preview, reloads authoritative committed geometry, and begins a new session. No sequence gap is silently skipped.

A Write Without Response has no ATT acknowledgement. Android must not silently retry or blindly commit when status is uncertain. Any retry must be followed by status reconciliation; a duplicate is reported in telemetry. UPDATE does **not** renew the lease. Only a matching HEARTBEAT renews the 5,000 ms lease. Pixel Probe remains a separate 2,000 ms diagnostic lease.

### 1.3 Status readback

UUID `0x37` returns a **61-byte version-2** status value. The first 35 bytes retain the version-1 field offsets; byte 0 is now 2. Use GATT long-read behavior when the negotiated ATT MTU is smaller than the value; do not manually apply the read offset twice. The Panel capability feature-flags byte now advertises bit 3 for the v2 transaction-sequence diagnostics.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 1 | Status format version (2) |
| 1 | 1 | State: 0 IDLE, 1 ACTIVE, 2 COMMITTING, 3 COMMITTED, 4 CANCELLED, 5 EXPIRED, 6 ERROR |
| 2 | 1 | Lane |
| 3 | 1 | Panel schema version |
| 4 | 4 | Session ID |
| 8 | 2 | Base revision |
| 10 | 2 | Validated pixel capacity |
| 12 | 2 | Lease duration in milliseconds |
| 14 | 4 | Last accepted UPDATE sequence |
| 18 | 4 | Candidate canonical CRC32 |
| 22 | 2 | Active revision |
| 24 | 2 | Staged revision |
| 26 | 2 | Render-acknowledged revision |
| 28 | 1 | Configuration pending flag |
| 29 | 1 | Persistent configuration flag |
| 30 | 1 | Last error: 0 none, 1 invalid packet/model, 2 busy, 3 stale/session/sequence conflict, 4 internal or durable-write failure |
| 31 | 4 | Remaining preview lease in milliseconds |
| 35 | 4 | Last rejected UPDATE sequence |
| 39 | 1 | Last rejected reason |
| 40 | 4 | Cumulative missing-sequence count (saturating LE32) |
| 44 | 4 | Cumulative out-of-order count (saturating LE32) |
| 48 | 4 | Cumulative duplicate count (saturating LE32) |
| 52 | 4 | Cumulative same-sequence/different-payload conflict count (saturating LE32) |
| 56 | 4 | Cumulative UPDATE/control rejections during COMMITTING (saturating LE32) |
| 60 | 1 | Sequence flags: bit 0 warning observed, bit 1 hard sync fault latched, bit 2 at least one UPDATE accepted |

The active transaction identity is the pair (lane, session_id) at offsets 2 and 4. Last accepted UPDATE sequence remains at offset 14. The last rejected UPDATE sequence and reason are at offsets 35 and 39.

**Transaction error codes** (offset 30 last_error, and offset 39 last_rejected_reason when the rejection is an UPDATE):

| Code | Meaning | Required client action |
|---:|---|---|
| 0 | No error | Normal state handling |
| 1 | Invalid packet/model | Reload status; correct the model/packet |
| 2 | Busy | Do not queue another mutation; reconcile status |
| 3 | Stale session/revision | Reload authoritative state |
| 4 | Internal/durable failure | Do not retry blindly; reconcile durable state |
| 5 | COMMITTING_LOCKED | Mutation rejected; preserve and continue observing the active commit |
| 6 | SEQUENCE_GAP | Hard synchronization fault; cancel preview and reload committed map |
| 7 | SEQUENCE_OUT_OF_ORDER | Hard synchronization fault; cancel preview and reload committed map |
| 8 | SEQUENCE_DUPLICATE | Warning; read status and verify accepted sequence/CRC |
| 9 | SEQUENCE_CONFLICT | Hard synchronization fault; cancel preview and reload committed map |
| 10 | SYNC_REQUIRED | Hard synchronization fault remains latched; no UPDATE/COMMIT |

A COMMIT is complete only when CORE reports state COMMITTED and the active and render revisions both equal the target revision. A successful BLE write response alone is not commit confirmation.

## 2. Android client state machine

This is a client behavior contract only. Preserve the approved six-screen Native Android UI exactly 1:1. Do not add a screen, alter the Layout card, or introduce new styling.

| State | Entry condition | Allowed actions | Exit condition |
|---|---|---|---|
| READY | Authenticated; capabilities and authoritative lane snapshot read | Begin edit | BEGIN sent |
| BEGINNING | BEGIN write in flight | Wait; no UPDATE | Status shows ACTIVE with nonzero session |
| ACTIVE | Matching session and base revision confirmed | Slider edits, UPDATE, heartbeat, cancel, save | UPDATE pending, cancel, commit, expiry, disconnect |
| UPDATE_PENDING | Write Without Response queued | Retry identical bytes only; read status | Status last sequence acknowledges packet or reports error |
| COMMIT_REQUESTED | Candidate sequence acknowledged; candidate CRC read | Send COMMIT with CORE-reported CRC | Status shows COMMITTING or ERROR |
| COMMITTING | Durable save/promotion in progress | Read status; do not send UPDATE, CANCEL, HEARTBEAT, BEGIN, legacy panel writes or a conflicting COMMIT | COMMITTED, ERROR, or disconnect |
| COMMITTED | Active revision and render revision equal target | Reload authoritative snapshot; close transaction | READY |
| CANCELLED / EXPIRED | CORE confirms terminal state | Discard local candidate; reload committed snapshot | READY |
| RECONCILING | BLE disconnect/reconnect or uncertain commit outcome | Re-authenticate; read authoritative revisions and map | READY or new BEGIN |
| ERROR | Invalid/stale/busy/durable error | Show actionable error; refresh before retry | READY after authoritative readback |

### 2.1 Required client behavior

1. Read capabilities and the authoritative lane snapshot before BEGIN. Never guess the base revision, schema or physical pixel length.
2. Serialize control writes and status reads through the Android GATT operation queue. UPDATE is Write Without Response, but it still belongs to the bounded queue; never build an unbounded backlog.
3. Debounce/coalesce slider changes to at most one latest-value update per UI frame or available GATT queue slot. The slider remains step-1 and visually immediate; pending transport must not be presented as device-acknowledged state.
4. Group mutually dependent range edits in one UPDATE whenever the ATT payload permits. If more than four descriptors are required, each intermediate packet must itself describe a valid map. If no valid intermediate sequence exists, do not send a temporarily invalid geometry.
5. After each UPDATE, read status and wait for `last_sequence` to acknowledge it before using the returned candidate CRC for COMMIT. Do not treat a no-response write as accepted merely because Android queued it.
6. Send HEARTBEAT approximately every 1.5 seconds while the Layout editor is foregrounded and the session is ACTIVE. Stop heartbeats when the editor is backgrounded; send CANCEL if the connection is still usable, otherwise let the five-second lease expire. Do not keep a hidden preview alive.
7. Stop sending UPDATE and HEARTBEAT after COMMIT is accepted. Poll status at a bounded interval (nominally 200 ms) until a terminal state; avoid tight read loops.
8. Send COMMIT with the candidate CRC returned by CORE, not a locally guessed or stale CRC.
9. CANCEL is valid only before COMMITTING. Once COMMITTING begins, the client must wait for authoritative outcome.
10. On disconnect, discard the volatile session ID and enter RECONCILING. After re-authentication, read the authoritative map and revisions. A commit may have continued after the BLE session ended; never assume disconnect means rollback.
11. If the base revision changed, reload and require a fresh user edit session. Never overwrite a newer revision silently.
12. While Show Mode is running, geometry preview must not stop, restart, or reset the active effect. CORE owns frame-boundary snapshotting and physical-trigger priority. The UI must not claim a physical output frame used preview geometry while a physical safety input owns that frame.
13. A newly observed change in duplicate, gap, out-of-order, conflict, or COMMITTING-rejection telemetry must immediately raise a synchronization warning in the existing **Settings -> System** area. Preserve the approved six-screen UI exactly 1:1; add no screen, card redesign or new visual language.
14. On any warning, read the full v2 status and compare lane/session_id, last accepted/rejected sequence, counters, flags, and candidate CRC. If the duplicate is confirmed as an already accepted identical packet and the authoritative readback agrees, the client may resume only after explicit reconciliation. If the hard-sync-fault flag is set, stop UPDATE/COMMIT, CANCEL the still-volatile preview if possible, read the committed map and revisions, then begin a new session. Never blindly retry or commit.
15. If an UPDATE is rejected with COMMITTING_LOCKED, keep the UI's last confirmed configuration, do not send another geometry mutation, and continue polling the same transaction until terminal state. Do not attempt CANCEL after COMMITTING.

### 2.2 Reconciliation rules

- If active revision and render revision both equal the expected target, the commit succeeded.
- If a staged revision is pending, continue status/readback reconciliation; do not send a second commit.
- If active revision remains at the base revision and no staged transaction exists, the old map remains authoritative. Reload it before starting another preview.
- If CORE reports EXPIRED or CANCELLED, discard the candidate and reload.
- If CORE reports ERROR, use `last_error` and authoritative revision readback; never infer whether NVS changed from the BLE transport result alone.

## 3. Fault-injection and stress matrix

Host tests are software evidence only. The matrix below separates currently testable preview behavior from the cross-namespace crash matrix that cannot pass until Issue #126 section 2A is implemented.

| ID | Injection | Expected result / invariant |
|---|---|---|
| PV-01 | BEGIN without authorization or capability read | GATT access denied; no session or shadow candidate |
| PV-02 | BEGIN with invalid lane, zero capacity, capacity mismatch, stale revision, pending config or wrong schema | Rejected; committed geometry and active effect unchanged |
| PV-03 | Second BEGIN while a session is active | BUSY; first session remains the only owner |
| PV-04 | UPDATE at ATT MTU 23, 31, 46; exact-fit and one-byte-too-large packets | Accept only packets within MTU and descriptor limit |
| PV-05 | Truncated header/descriptor, 0 or 5 descriptors, reserved role bits, invalid action, unknown IDs, overlap or out-of-capacity endpoint | Reject atomically; previous shadow candidate and CRC unchanged |
| PV-06 | Sequence 1, duplicate identical packet, duplicate changed payload, stale, skipped/out-of-order and wraparound | New accepted; identical duplicate idempotent; conflict/stale rejected |
| PV-07 | Retry a Write Without Response after uncertain transport delivery | Retry exact bytes and same sequence; never create a second edit |
| PV-08 | No heartbeat at lease boundary; heartbeat just before expiry; UPDATE flood without heartbeat | Preview expires at 5,000 ms without heartbeat; heartbeat extends lease; UPDATE alone does not |
| PV-09 | Wrong session ID, wrong lane, wrong base revision heartbeat/cancel/commit | Rejected; another session cannot be changed |
| PV-10 | CANCEL during ACTIVE, then CANCEL after COMMITTING | First restores committed geometry; second is rejected |
| PV-11 | Wrong candidate CRC; correct CRC; duplicate COMMIT while COMMITTING | Wrong CRC cannot activate; correct CRC queues one immutable candidate; duplicate does not queue another commit |
| PV-12 | Disconnect before config worker starts | Volatile request is cancelled; production geometry remains unchanged |
| PV-13 | Disconnect after config worker begins durable staging | BLE session resets, but durable staged commit/recovery continues independently |
| PV-14 | Physical Brake/Turn/Hazards/Reverse assertion during preview update and during promotion | Physical safety output remains authoritative; no decorative preview can override it; promotion waits for quiet input/edge epoch |
| PV-15 | Show Mode running during BEGIN, repeated UPDATE, heartbeat and COMMIT | Same active effect instance and phase continue; no STOP, restart or global effect reset |
| PV-16 | STOP, watchdog decorative inhibit, output fault or lease expiry during ACTIVE | Volatile preview is cancelled; physical safety arbitration remains intact |
| PV-17 | NVS stage write, commit or readback failure | No candidate RAM publication; durable failure is reported; recovery state remains fail-closed |
| PV-18 | Power loss after staged write, active write, stage erase and before RAM publication | Reboot restores previous image and defers candidate until the quiet-window promoter can safely retry |
| PV-19 | Reset challenge expires, revision changes, input asserts, or reset write/readback is interrupted | Reset cannot bypass challenge/revision/input gates; no partial lane reset is reported as success |
| PV-20 | Telemetry queue saturation and simultaneous Left/Right edges | Existing SPSC queue remains bounded; overflow is counted and latched; ISR has no unbounded retry loop |
| PV-21 | Two UPDATE callbacks race with COMMIT acceptance | Exactly one order wins at the CORE mutex/critical-section boundary; after COMMITTING is visible, every later UPDATE is rejected before shadow mutation |
| PV-22 | Delayed pre-COMMIT Write Without Response arrives after COMMITTING | Rejected with error 5; COMMITTING status, immutable candidate and durable journal remain unchanged |
| PV-23 | Identical duplicate UPDATE arrives during COMMITTING | Rejected with error 5 and committing-reject telemetry increments; no sequence or candidate mutation |
| PV-24 | Out-of-order, forward-gap or same-sequence/different-payload UPDATE arrives during COMMITTING | Rejected with error 5 before sequence classification; commit remains protected and no NVS mutation is initiated |
| PV-25 | Duplicate UPDATE while ACTIVE | Candidate is not applied twice; duplicate counter and warning advance; client reconciles status/CRC |
| PV-26 | Forward sequence gap, stale packet or changed payload reuses a sequence while ACTIVE | Packet is rejected; hard sync fault latches; further UPDATE/COMMIT blocked until CANCEL and new authoritative session |

### 3.1 Required additional crash matrix after namespace split (section 2A)

Do not mark these PASS against the current combined-snapshot journal. After separate lane-hardware and map-only journals plus cross-namespace reconciliation are implemented, inject power loss after every durable boundary:

1. Hardware-lane stage commit in `ot_output_cfg`.
2. Map stage commit in `ot_zone_map`.
3. Cross-namespace commit-intent marker.
4. Hardware active-record write and readback.
5. Map active-record write and readback.
6. Hardware stage erase and readback.
7. Map stage erase and readback.
8. Pair-intent clear and readback.
9. Quiet-window/edge-epoch check immediately before RAM publication.

For every restart, verify both namespaces resolve to the same transaction ID and revision pair. The recovered runtime must be either the complete previous lane+map pair or the complete candidate lane+map pair—never a mixed pair. A corrupt or mismatched pair must fail closed and must never resurrect legacy embedded zones. Run the full 256 physical-input-mask model at every recovery boundary.

## 4. Read-only input telemetry dashboard contract

The dashboard is an engineering diagnostic artifact, not a new approved app screen. Do not change the six-screen UI to expose it without separate owner approval.

UUID suffix `0x34` is authenticated read-only, wire version 1, fixed 82 bytes. It has no public HTTP equivalent.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 1 | Wire version (1) |
| 1 | 1 | Raw asserted mask (IN1–IN5 bits) |
| 2 | 1 | Debounced asserted mask (IN1–IN5 bits) |
| 3 | 1 | Reserved |
| 4 | 4 | Global input edge epoch |
| 8 | 20 | Raw edge counters, IN1–IN5, five LE32 values |
| 28 | 20 | Asserted edge counters, IN1–IN5 |
| 48 | 20 | Deasserted edge counters, IN1–IN5 |
| 68 | 2 | Turn queue depth: IN3, IN4 |
| 70 | 2 | Turn queue high-water: IN3, IN4 |
| 72 | 8 | Turn queue overflow counts: IN3, IN4, LE32 each |
| 80 | 2 | Turn queue overflow flags: IN3, IN4 |

Dashboard behavior:

- Poll only while the engineering diagnostics view is visible; nominal interval 250 ms. Stop polling when hidden or disconnected.
- Display raw and debounced state separately. The masks and edge counters cover IN1–IN5 only; do not display IN6–IN8 as zero-count inputs or imply they have equivalent edge instrumentation.
- Show IN3 and IN4 queue depth, high-water and cumulative overflow separately. Overflow flags are latched evidence, not a user-clear control.
- Display edge epoch and receipt time. Counter deltas are useful only within the same boot/session; reset the baseline after reconnect/reboot and handle 32-bit wrap.
- The characteristic is read-only and owner-authorized. It must not mutate trigger state, clear counters, or become an unauthenticated HTTP endpoint.
- No telemetry poll may delay the control GATT queue, preview heartbeat, STOP or safety command path.

## 5. Acceptance gates

Software preview acceptance requires all of the following on one exact commit:

- CORE host suite, Panel/Zone journal tests, preview/protocol tests, source contracts and trigger/effects/STOP regressions pass.
- ESP-IDF v5.5.5 ESP32-S3 build passes on the exact same SHA.
- Flash package includes all required images and checksum/offset manifest.
- Exact-HEAD review confirms the GATT parser is connected to the real characteristics and the NVS worker owns durable writes.
- Map-only/hardware-only namespace split and cross-namespace crash matrix pass before section 2A is called complete.
- IRAM map/disassembly verifies the GPIO ISR, lock-free atomics and bounded queue operations.
- Physical bench tests confirm actual RMT timing, trigger arbitration, reconnect behavior and optical output. Host tests do not replace these checks.

No software PASS, CI receipt or generated APK constitutes physical or road certification. The firmware remains bench-only until the owner-approved hardware acceptance is complete.


## 6. Owner-mandated 100-cycle brownout endurance gate

See [ISSUE126-BROWNOUT-100-CYCLE-ACCEPTANCE.md](ISSUE126-BROWNOUT-100-CYCLE-ACCEPTANCE.md). This is a mandatory physical Firmware + Hardware + QA release gate: 100/100 controlled power-cut/recovery cycles under the measured full-white 944-pixel WS2815 load. The eight durable commit phases must be injected at randomized points after the final cross-namespace journal is implemented. The test has **not been run** and no cycle is credited by CI or host simulation.
