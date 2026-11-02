# Issue #93 — Authorize all 13 source-proven third-color Effects in BLE

**Software candidate only.** Exact reviewed integration parent: PR #111 `1a9cfc4b25ef57bda38a7861ac5548c3747d9605`.
Authoritative reconciled renderer: Effects PR #110 `6180e2a5ccbfbf8567513504669c6840b68c045b`.

## New capability admission (existing authenticated `fx-tertiary-v1`)

The existing 20-byte opcode **11** remains unchanged; Android and CORE now both admit **13** independently source-tested consumers. CORE continues advertising `fx-tertiary-v1` for the original 11 and **also** advertises `fx-tertiary13-v1` for the two additions. Android requires the extra token before attempting Aurora/Popcorn on a connected controller, so older 11-ID firmware is never falsely treated as 13-ID capable:

`28, 29, 34, 38, 50, 52, 54, 55, 56, 84, 87, 95, 103`.

- **Aurora ID 38**: the third color is consumed only in its authored **palette 4**. Default Aurora palette 50 remains an independently authored gradient. Reject opcode 11 with ID 38 and any palette other than 4 without changing the active zone or stored state.
- **Popcorn ID 95**: source-proven per-particle third color; permitted with its supported palette 0.
- **Six intentionally unsupported IDs**: `10, 11, 40, 48, 60, 91`. They are source-declared options/metadata but do not independently consume RGB3 in the current 1D implementation. The BLE packet is rejected before mutation; do not invent the behavior.
- Other WLED/native effects without source-proven RGB3 continue to use legacy opcode 10 as before. Explicit black `#000000` remains a valid third RGB color; old 17-byte commands stay byte-identical.
- The full 140-effect catalog and JNI C renderer remain pinned to exact Effects #110; no animation math or proprietary identities changed.

## Regression and release acceptance

- Existing controller-owned continuous playback, BLE session/HMAC authorization, physically assigned output/zone masks, gamma/color swizzle, STOP, OEM Brake/Turn/Reverse priority, staged NVS, NVS recovery and 12V brightness cap are unchanged.
- Focused C parser tests exercise all 13 allowlisted IDs, Aurora palette 4/0/50 conditions, six blocked IDs, no output mutation on rejection, true black, 17-byte compatibility, zone isolation and phase continuity.
- Android JUnit verifies 13 IDs, 38/palette4 versus palette0/50, Popcorn RGB bytes, invalid packet/ranges, capability token. Exact-HEAD workflows require all 140 CORE host/JNI frame parity, Android APK, ESP-IDF v5.5.5 and a complete four-image ZIP with OTA initialization at 0xF000, verified after artifact download.
- Earlier PR #111 immutable integration provenance check intentionally does not authorize changing BLE/parser, INFO or Android Java sources. The new scope-limited exact-head workflow is a **successor** that explicitly allows *only* these reviewed changes. It does not weaken physical release gates or rewrite old receipts.
- Owner-approved six-screen Native Android UI remains exactly 1:1. No firmware version bump. Draft PR only; do not merge or flash. Physical 22-case checks, phone GATT ACK/optical color measurements, hardware failover and vehicle release remain **NOT RUN / NOT CERTIFIED**.
