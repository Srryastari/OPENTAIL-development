# OpenTail proprietary effects — approved design contract (2026-10-27)

Status: DESIGN APPROVED; NOT IMPLEMENTED OR HARDWARE VERIFIED.
Scope: CORE, Studio, future mobile clients. Keep PR #12 draft and do not merge without owner acceptance.

## Approved 15 effect IDs (stable slugs)
1. materialize
2. random_materialize
3. quantum_collapse (one massive final pulse)
4. reality_break
5. chromatic_shatter
6. vortex
7. dimensional_rift
8. meteor_storm
9. helix_drive
10. crystal_growth
11. fusion_core
12. portal_gate
13. time_fracture
14. aurora_surge
15. event_horizon

Event Horizon supports a dedicated Singularity Shutdown: active illumination is drawn toward a configurable gravitational center, compresses into a white point, then extinguishes. All constituent colors, timing, radii, direction, strength, flash and fade parameters are editable.

## Universal effect configuration contract
- One versioned schema shared by CORE, Studio, and future apps; stable effect IDs and named parameters; per-effect parameter metadata, bounds, defaults, migration and validation.
- Independent editable effect elements (e.g. meteor groups), each with color/palette, size, position, direction, speed, intensity, density, randomness, timing, blending, and visibility where relevant.
- Custom and built-in palettes; fixed/random per-element colors; background, foreground, accent and final colors independently editable; save/duplicate/copy/paste compatible presets.
- Startup and shutdown Auto Events are optional/off by default. Each supports independent adjustable fade-in, fade-out, transition-to-next-state and effect-specific ending. Proposed default 300ms startup fade-in, 450ms startup transition, 450ms shutdown fade-out; configurable 0–5000ms. These are design defaults, not current firmware behavior.
- Final actions: hold, restore underlying effect, transition, or black-out assigned pixels. Smooth Fade, Instant Snap and special effect endings supported where meaningful.
- TRUE BLACK: every effect can output exact RGB(0,0,0) for dark pixels, complete fades, or explicit OFF. Never enforce nonzero minimum brightness. WS2815 pixel black is not strip power disconnection.
- Zone-local arbitration remains authoritative: active brake/turn/reverse and assigned vehicle triggers override only their assigned zones; parked/idle baseline does not globally preempt user show effects. On trigger release reveal underlying effect without restarting its phase.
- Rendering pipeline: native WLED effects preserved; OpenTail effects isolated by IDs/dispatch; overlays applied after underlying render, respecting output zones, pixel color order, and global current/brightness constraints.
- No new feature may alter saved Output Manager settings, BLE authorization, 8 scene slots, existing GPIO mapping, or existing effects unintentionally.

## Implementation sequence and acceptance
1. Add shared effect catalog/schema and migration tests. Verify all 15 IDs, defaults, parameter bounds, and serialization round-trip; Studio/CORE parity.
2. Implement universal true-black/fade/end-state behavior with host tests: zero values exact, no residual glow, zone-limited OFF, fade reaches zero, active trigger overlay still visible.
3. Implement one proprietary renderer at a time with deterministic host frame tests; add editor controls and preview using same schema. Avoid introducing all 15 into firmware in one risky change.
4. Add startup/shutdown Auto Events, including Event Horizon Singularity Shutdown, with nonblocking timing and correct cancellation/retrigger handling.
5. CI and hardware acceptance: 522-pixel GPIO16 stable, GPIO17 independent, Stop Show clears both, trigger masks and release/resume, BRG colors, NVS reload, BLE auth. These remain pending until owner reports actual physical results.

No claim of firmware parity, electrical fail-safe, completed implementation, or physical validation is made by this specification.
