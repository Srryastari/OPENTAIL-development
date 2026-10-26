# OpenTail Studio v0.19.34 — Animated pill-strip preview

## Appearance
- Preserves approved short vertical rounded-rectangle/pill LED segments, separated horizontal strips, near-black cards and true black OLED page background.
- Physical GS400-shaped preview remains a future optional secondary mode.

## Functionality
- Local/offline effect animation preview with representative LED samples (up to 28 per section), rendered through a live frame loop.
- Local sequence preview from saved Studio sequence steps, with step durations (does not send to controller).
- Clickable simulated Park, Brake, Left/Right Turn, Hazards, and Reverse triggers. Uses existing saved section assignments. Unassigned sections remain dark; Brake visual precedence over Park on a shared section.
- Turn/hazard animation uses the existing offline pulse/sweep timing model; distinct from validated vehicle-signal timing.
- Preview Pause/Resume and Stop. Stops the local preview only, does not change actual lighting.
- Miniature LED preview inside the existing Universal Effect Editor, responding to editable effect colors, speed, intensity, and brightness.
- Existing WLED Peek read-only RGB frames remain authoritative in the preview when received and the user has stopped simulation. The source is explicitly labeled LIVE EXACT, LIVE PARTIAL, STALE, or SIMULATED as appropriate.
- CORE Alpha has no real pixel-frame telemetry and is clearly labeled as simulated for animations.
- A single shared Studio UI remains in WLED and CORE modes; nothing about the physical LED output, trigger circuitry, or failover is changed by the preview.

## Limitations
- Local simulator is an *illustration*, not an exact implementation of WLED's effects, WLED palettes, the CORE animation engine, or a physical LED frame.
- 28 sampled LED rectangles per section, not an addressable 2D vehicle map.
- CORE Alpha cannot execute Studio effects/sequences/trigger animations physically yet.
- Does not validate road-lighting functions. Bench only.
- Offline previews never send WLED or CORE commands.

## Validation
- JS syntax check: passed (node --check).
- Existing 27 browser regression checks: passed.
- New 27 browser preview checks: passed.
- CORE host logic: 25 assertions passed.
- Turn-pulse model: sampled at 0, 160, 360, 490ms yielded 1, 16, 0, 0 lit sampled LEDs (350ms high / 350ms low, sweep 280ms).
- ZIP embedded Studio page byte-for-byte equal to v0.19.34 standalone HTML.
- No physical ESP-IDF build or flash was performed here; the new firmware source package must be compiled on Windows with ESP-IDF v5.5.5.
