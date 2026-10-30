# Android / CORE exact-frame preview contract

**Current status (development branch, not flashed):** Android now implements authenticated, optional CORE v1 post-render **single-frame** capture. The currently flashed bench firmware does not have this new characteristic. Until CORE firmware PR #19 is built, physically tested and approved, Android continues to show its exact *local project pixel map*, not a claimed animation.

## Sources of pixels

- `ExactLedFrame.Origin.PROJECT`: exact zero-based local authored pixels (including manual gaps, direction reversal and paint). No fabricated FX/particle/spark animation.
- `ExactLedFrame.Origin.CORE`: complete RGB888 frame received from authenticated CORE firmware, captured **after rendering, overlays and calibration, before driver color-order swizzle**. One display cell corresponds to one actual logical LED index. Core frame sequence and output channel are displayed.
- A verified CORE frame is immutable and is invalidated on effect edits, project import and background. No interpolation, averaging, fallback swizzle, or copying approximations into `Origin.CORE`.

## Wire compatibility

The optional, versioned BLE characteristics are implemented on firmware PR #19:

- Command write `55a56022-4915-7a92-cf44-b8b7102eaf2a`
- Snapshot reply read `55a56022-4915-7a92-cf44-b8b7102eaf2b`

See `docs/CORE-FRAME-SNAPSHOT-V1.md` on the CORE branch for exact binary format. Android only enables Capture when authenticated and both characteristics are discovered. Capture is on demand, not a continuous stream.

The Android client requires exact channel, frame sequence, zero-based chunk start/count, complete RGB888 bytes, and CRC-16/CCITT-FALSE integrity. Partial, stale, reordered, duplicated, or malformed frames fail closed. Android GATT operations are serialized with existing BLE FX, authorization and output configuration commands; STOP preempts unfinished diagnostic reads. No diagnostic read writes controller NVS or modifies vehicle trigger arbitration.

## What the implementation does NOT prove

- 120+ effects are not automatically 1:1 with pinned upstream WLED because the CORE compatibility renderer still has parity gaps. A valid snapshot proves Android displayed CORE's actual RGB for that **one moment**, not that CORE's renderer matches upstream WLED.
- Captured RGB is logical data before physical WS2815 encoding. It is not measured LED brightness, color, current, PWM edge timing, or optical latency.
- BLE cannot be assumed to stream thousands of RGB LEDs at 53–120 frames per second without hurting rendering or battery/connection reliability. Real-time fluid 1:1 playback requires a separate bounded high-bandwidth transport or a deterministic shared rendering engine and timestamp alignment; neither is certified here.
- The v1 snapshot path must pass ESP-IDF compile, host tests, actual Android BLE chunk/MTU tests, trigger priority overlays and physical dual-output comparison before being called verified on hardware.

**Release state:** PR #19 and #21 stay drafts. Neither a screenshot nor passing Gradle/host CI alone certifies optical or continuous animation parity.
