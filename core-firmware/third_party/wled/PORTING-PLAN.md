# WLED → OpenTail CORE effect-port plan

**Status:** initial compatibility scaffold (host tested in CI; no pixel-output integration yet).  
**Source:** https://github.com/wled/WLED at commit `20e7a717664295eefc6ad77972cee1888d2da072` (2026-10-25).  
**WLED core source:** `wled00/FX.cpp`, `wled00/FX.h`, `wled00/FX_fcn.cpp`, palette/math dependencies as required.  
**License:** EUPL-1.2-or-later; see `LICENSE` and `README.md` here. Follow obligations and review compatibility before distributing combined binaries.

## Non-negotiable implementation contract

- **Standalone controller:** OpenTail CORE renders its own effects. Wi-Fi, WLED firmware or a separate WLED controller must *not* be necessary to play supported effects.
- **Canonical algorithm:** use the same deterministic effect math in firmware and (via portable bindings/native port) in Studio's animated per-zone preview. The local illustrative animation is not proof of parity.
- **One effect-editor UX:** preserve the approved six-tab UI, one Show Modes effect settings editor, and effect-specific metadata labels, color slots, palette/defaults and capabilities. Unsupported modes must be clearly unavailable, not silently approximated.
- **Real zones:** effect state per configured zone, including independent speed/intensity/colors/reverse and 0-to-30 sampled preview indicators computed from full zone lengths. Zones are not 2D matrices by default.
- **Automotive safety:** physical trigger arbitration and fallback must always preempt decorative shows. Preview/offline editing is command-free. There is no road-legal safety claim for current alpha hardware.
- **Performance:** never update effect frame state once per pixel; do it once per output/zone/frame, with bounds-checked memory and measured RMT refresh time. Don't claim 60–120 FPS at 6k pixels without measured per-channel output timing.
- **Source rights:** provenance comments, full EUPL text and version pin travel with derived algorithms; perform distribution/legal compliance review before release.

## What was added in this stage

`main/wled_fx_compat.[ch]` implements a **pure C, allocation-free 1D adapter** with WLED IDs 0 Solid, 1 Blink and 23 Strobe. Blink/Strobe share WLED's cycle-time, on-time, and `SEGENV.step` frame-catch-up semantics; host tests cover boundaries, skipped frames, brightness and unsupported configs. Only `palette_id=0` direct colors work. WLED palette lookup, custom controls, per-zone runtime, gamma and full frame parity are pending.

`effects-catalog.json` indexes the pinned upstream effect names and raw metadata for offline support discovery. It resolves **201 distinct mode IDs**, with **16 upstream metadata symbols requiring more mapping** (the source contains one duplicate registration). Only three IDs say `host-tested-compatibility-module-only`; all others are `not-ported`.

## Phases to reach full 1D capability

1. **Golden parity fixtures:** derive reference frame outputs from pinned WLED C++ for Solid/Blink/Strobe on small 1D strips. Compare all RGB samples at exact timestamps, speed/intensity extrema, one-frame skips and zero/one pixel boundaries. Distinguish WLED bus brightness from OpenTail brightness cap.
2. **Bench-only CORE selection:** add effect dispatch through the existing temporary, bounded CORE show API, *without* changing trigger priority, NVS output layouts or persistent road-lighting. Map WLED `uint8_t` speed/intensity exactly; reject unavailable modes and unsupported palettes. Track each zone's independent `step` and reset when configuration changes.
3. **Accurate preview:** expose/reuse the same source-proven pixel renderer in the app; render the full configured zone then sample <=30 uniform indicator positions. Show effect/sequence frame timing honestly, and no writes during preview.
4. **More 1D algorithms:** port deterministically, beginning with Fade, Wipe, Sweep, Rainbow, Theater and Chase classes. Add color/palette blend primitives and explicit seeded random state, then per-zone effect memory as needed. Do not claim parity based only on WLED metadata.
5. **Feature-gated complexity:** audio-reactive requires an independent, actually measured audio pipeline; 2D/matrix/particle modes are disabled until their dimensional and memory requirements are met. WLED usermods are not automatically included.
6. **Bench acceptance:** measured simultaneous Brake+Hazards preemption, deterministic restoration, offline disconnection, minimum/max zone lengths, output direction, memory/CPU/per-channel FPS with WS2815 full-count lighting. No automotive-safety certification is implied.

## Deliberately not changed

No CORE trigger precedence, BLE authorization, output mapping, live pixel rendering, firmware API version, user projects/presets, or approved UI layout was modified by this stage. The compatibility engine is compiled but not yet used for physical LED output. PR #6 stays in draft pending validation and owner approval.
