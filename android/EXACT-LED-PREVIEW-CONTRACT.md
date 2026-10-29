# Android 1:1 LED preview — evidence and transport contract

**Current status:** *Pixel addresses exact for local design; effect animation parity NOT verified / not currently available.*

## What was audited

- Former Compose Show Modes Canvas generated 28 synthetic bars, advancing on a 40ms timer with made-up chase logic. It was not CORE rendering.
- Former Java `NativeLedPreview` generated 32 name-matched approximations. It also was not CORE rendering.
- The currently flashed CORE BLE commands send FX IDs and parameters, but do **not** expose a readback of the pixels after the renderer and priority overlays. Android cannot truthfully claim a 1:1 animation from that data.

## Implemented safe Android behavior

- One canonical `ExactLedFrame` model preserves one RGB value per zero-based pixel address, with no color averaging, downsampling, interpolation, or fixed vehicle pixel counts.
- `fromProjectModel` reflects all authored zones, their auto/manual starts, gaps, reversed indexing and individual painted pixels. These are **local design** colors, not optical / firmware output.
- Both native Android previews now display a 64-pixel inspection window, where **each visual segment represents exactly one indexed LED**. Paging/slider accesses every index including long strips. A zero/black pixel stays black.
- The effect picker retains the complete 140-ID catalog, but no fabricated WLED, particle, flare, spark, or proprietary animation appears as a "1:1" preview.
- A future CORE snapshot must be explicitly marked `ExactLedFrame.Origin.CORE`, with validation of actual pixel count and RGB888 input.

## CORE frame snapshot required for effect 1:1

The firmware/protocol owner must expose capability-negotiated, authenticated and bounded **post-render** frame data. Capture after all native effects/particles/sparks/flares, per-zone mapping, overlays for physical inputs and brightness/color calibration, before WS2815 channel-order encoding. The capture must declare output channel/index, actual active pixel count, monotonically increasing sequence or timestamp, exact zero-based RGB888 bytes and fragmentation integrity. Treat partial/mixed/stale frames as unavailable, never as valid. No inferred colors, never swap RGB based on physical strip driver order, no hardcoded 522/623/1246 assumptions. Stream diagnostic frames only within BLE/Wi-Fi bandwidth and controller FPS budget; do not let UI telemetry reduce 53–120 FPS operation.

**Do not invent a BLE opcode.** A plain FX command ack proves the controller *accepted a command*, not that the UI has received or validated its rendered pixels. A 1:1 frame may be confirmed only when its actual bytes come from CORE and its provenance is verified. Pixel-perfect **light output** additionally requires physical calibration and bench measurements.

## Tests and release gate

`ExactLedFrameTest.java` checks all 1,246 indexes, 2 × 623 zone example, reverse addressing and manual gaps, RGB888 byte order, immutable snapshots and invalid frame rejection. These are **address/codec unit tests**, not upstream WLED parity certification. CI runs `app:testDebugUnitTest` before APK packaging. Actual optical timing, phone graphics and CORE frame-data acceptance remain open.
