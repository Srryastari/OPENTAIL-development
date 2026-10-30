# OpenTail CORE — renderer completion and release gates (October 29, 2026)

## One unified Effects catalog
The borrowed WLED 1D effects and OpenTail original effects appear together under **Effects**. The firmware owns all execution and pixel writes; no WLED runtime is flashed and Studio does not own the LED renderer. Maintain attribution for source-derived adaptations.

## Borrowed effects — mandatory source parity, NOT merely ID/name parity
- The current compatibility catalog has **120 named borrowed 1D modes**, of which **16** are implemented as direct source-derived adaptations. The other **104** still invoke approximations in `wled_1d.c`. A passing CI build does **not** make the approximations equivalent to the original effect.
- Release requires a direct adaptation of **every** borrowed mode, source-aware per-segment state, palette and option behavior, particle-dependent effects, correct timing/seed semantics and golden frame comparisons against pinned `wled/WLED@20e7a717664295eefc6ad77972cee1888d2da072`. It must retain exact effect identity, including Fireworks (42), Fireworks Starburst (89), Fireworks 1D (90), and particle-system Fireworks 1D (distinct upstream effect).
- Fireworks 1D (90) is explicitly reported failing visual comparison: the current expanding ring is **not** upstream launch/flare/gravity/explosion/sparks. A direct port and matched frames are blocking.
- The unified catalog currently distinguishes `source-derived`, `compatibility` and `original`; never relabel a compatibility approximation as source-derived.

## Approved OpenTail originals
**Preserve names and implement as distinct OpenTail-owned modes** (not WLED rebrands). The first-pass 1D effect algorithms and registration now exist, but full per-effect advanced parameter controls and physical visual signoff remain open:

1. Materialize Sparks, Random Materialize, Smooth Fade, Plasma Sweep, Smooth Fade Out
2. Dimensional Rift, Meteor Storm, Helix Drive, Crystal Growth, Fusion Core, Portal Gate, Time Fracture, Aurora Surge
3. Chromatic Shatter, Vortex, Quantum Collapse (Massive Pulse), Reality Break, **Event Horizon**, **Singularity Shutdown**
4. Embers (separately registered original)

**Special behavior lock:** Event Horizon has a traveling dark gravitational core, a compressed bright ring, spectral shockwave, and a final configured color. Singularity Shutdown collapses toward a brilliant white point and must finish at exact RGB (0,0,0). Preserve the approved ability to customize gravity/radius, color, direction, distortion, pulse/shockwave, timing and fades; generic speed/intensity is an initial baseline only.

## Performance and safety regression criteria
- All decorative movement and fades should appear ultra-smooth. The new original fade path uses Q16 smoothstep before 8-bit output quantization; this reduces intermediate rounding but **does not prove perceptual smoothness at 16/255 output or 25 FPS**. Validate on physical hardware, measured FPS, low-brightness output and actual strips. Improve renderer scheduling/interpolation and lighting output only with measurable tests.
- Strobes are hard-edged, source-timed, **not** eased or rate-limited by a decorative smoothing filter. Real output strobe accuracy must be measured on hardware; static host tests cannot prove latency.
- Brake/Turn/Hazards/Reverse, fault/off and Stop must never wait behind visual smoothing. Existing configured zone arbitration, output color order, per-zone resume, role mappings and NVS persistence are regression-critical.
- Materialize spark accent colors include original, randomized brighter shades, rainbow+white and custom; peak brightness is independently configurable and **must stay within the 32/255 bench cap**. Zero brightness stays black. Brightness caps are not substitute for measured current.
- No GS400 geometry embedded in the firmware: outputs/zones/pixel lengths/roles are universal and user configured.
- A successful firmware CI artifact is **bench only**. Require full CI, golden reference frame comparisons, physical trigger tests, status LED sequences, BLE authentication/reconnection, two physical outputs, saved configuration, Stop/restore, power/current and measured FPS before marking any release ready.

## Current release status
**INCOMPLETE — do not merge as release-ready or install as sole road taillight controller.** The v0.1.28-alpha image already flashed by the user precedes these development-branch changes. The branch must be recompiled and physically regression-tested before any replacement flash.
