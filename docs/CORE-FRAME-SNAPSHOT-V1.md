# OpenTail CORE rendered-frame snapshot v1 (diagnostic, read-only)

This capability allows native Android to inspect an **actual CORE-rendered frame**, not a locally simulated WLED or OpenTail effect. It is attached to the existing owner-authenticated NimBLE service; it does not change the command, output configuration, or trigger arbitration ABI.

- BLE characteristic `55a56022-4915-7a92-cf44-b8b7102eaf2a`: write request.
- BLE characteristic `55a56022-4915-7a92-cf44-b8b7102eaf2b`: read response.
- Values are byte arrays; all multibyte integers are **little-endian**.
- Captures are unavailable until owner authentication succeeds. Older CORE images do not expose these UUIDs; Android must disable capture.
- Only one snapshot can be pending/active. A new request replaces an already completed snapshot, and disconnect invalidates its provenance.
- Capture occurs only in the render task, after effects, transitions, trigger overrides and logical per-output calibration, before RGB/GRB/BRG driver swizzle. It is published only after the requested channel's asynchronous RMT refresh reports completion. Driver completion **does not certify optical output**.

## Wire ABI

1. Write `[1,1,output:u8]`. Read `[1,1,status:u8,output:u8,pixels:u16,sequence:u32,crc16:u16]` (12 bytes). Status `0` = capture pending, `1` = ready, `2` = unavailable. Poll a pending capture without issuing a new begin command.
2. For a ready snapshot, write `[1,2,output:u8,sequence:u32,first:u16,count:u8]` (10 bytes); count is **1–56 pixels**. Read `[1,2,1,output:u8,sequence:u32,first:u16,count:u8,R,G,B,...]` (11 + 3·count bytes). GATT Read Blob offsets are honored for negotiated MTUs below a complete chunk.
3. Concatenate in increasing zero-based index order. Reject wrong channel, sequence, offset, count, duplicate, partial/missing bytes, or CRC mismatch. CRC uses CRC-16/CCITT-FALSE (initial 0xFFFF, polynomial 0x1021) over **every RGB byte**. Do not re-swap a canonical frame using strip driver order.
4. The published frame is frozen until the next capture request or BLE disconnect. A request never writes output mapping to NVS and does not synthesize effect pixels.

## Current scope / remaining parity gates

- This verifies the **pixel values in one real CORE frame** with explicit origin, not WLED upstream source equivalence, continuous animation timing, or light emitted by the strip.
- The complete 120+ effect source parity and particle/flare/spark render golden fixtures remain separate CORE engine gates. Do not label modes pixel-identical to WLED without independent source-derived golden comparisons for each claimed configuration.
- BLE is not a 53–120 FPS full-frame display transport at thousands of LEDs. On-demand snapshots do not run every frame and should not delay input arbitration or pixel refreshing. For true real-time 1:1 playback, a future bandwidth-budgeted transport and synchronized frame timestamps/latency measurements are required.
- **Not for flashing or production approval** until ESP-IDF build, host tests, BLE hardware, trigger/arbitration, color order, capture integrity, full frame timing and optical measurements pass.
