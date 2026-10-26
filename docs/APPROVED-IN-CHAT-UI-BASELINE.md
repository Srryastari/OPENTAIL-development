# OpenTail UI master approval baseline — 26 October 2026

**Approval scope:** User-approved visual design system and selected interaction patterns. This is a specification, **not** certification that current Android/Studio code matches it. Latest audited candidate: Studio v0.19.56 / Android v0.1.23, draft PR #6. The current application still has duplicate navigation, competing Show Modes editors and approximate simulation. No phone screenshot, BLE, LED, firmware or vehicle-safety validation has been established by this approval.

## Source of truth

The user's interactive **in-chat OpenTail concept cards** are the visual reference for spacing, pill geometry, icon placement, typography, expanding controls and selection feedback. Generated illustrative images are not accepted references. The ChatGPT UI's runtime CSS is not importable; reproduce its appearance in native Android / current Studio with explicit tokens and manual device-side comparisons. Do not claim pixel-perfect parity based on CSS, unit tests or screenshots taken at different viewports.

## Locked visual system

- **Canvas and elevations:** OLED-black `#000000` canvas. Use ultra-deep `#0b0b0b`, `#0d0d0d`, and `#141414` as quiet elevations when interactive surfaces need separation. Keep borders razor-thin, neutral and subtle (approx. `#262626`–`#343434` as a reference, not a mandate for every component). Never use a global green/blue brand accent, gradients or decorative glowing frames.
- **Semantic color and lighting feedback:** Green/amber/red only for meaningful connection, warning, error and hardware status, where backed by real state. Muted red for destructive actions. The color picker and LED preview necessarily show their real saturated RGB colors. Do not turn the whole UI green or decorate ordinary elements with glowing red.
- **Typography:** native-looking system sans font; off-white text with muted but still legible secondary labels; hierarchy primarily through weight and contrast, not oversized headings. No oversized section icons.
- **Shape:** pill/capsule navigation, selectors and touch buttons; rounded rectangles for sections and cards. Consistent radii, padding, subtle dividers. Avoid cards-inside-cards-inside-cards.
- **Interaction:** Android touch-first pressed, selected, expanded, disabled, and confirmation states; no hover-dependent actions. Accessible contrast, focus where relevant, descriptive semantics, keyboard/Back behavior, and generous touch targets (target >=48 dp in the native version).
- **Status pill:** retain the already approved compact proportions, Bluetooth icon, alignment and stateful status colors. Never label a mocked status as a real connection.
- **System bars:** OLED-black Android status/nav system areas rather than green-tinted safe areas.

## Locked top-level navigation and duplicate-action rule

Exactly **one** app-wide pill navigation in this unchangeable order:

1. Home
2. Show Modes
3. Layout
4. Triggers
5. Auto Events
6. Settings

Content changes inside the same workspace. **No Android bottom navigation**, no duplicate Studio tabs, no second main route strip, no duplicate shortcut/editor hidden in other tabs, no redundant button executing the same global action. Context-specific actions (e.g. each zone's Remove Zone) are not duplication. `Effects / Sequences / Presets` are contextual Show Modes modes, **not** a second global nav bar.

## Approved screens and components

### Home — concept approved

Connection entry point, Wi-Fi and BLE status, and concise controller information (pixel outputs, configured LEDs, active inputs, lighting state as actually reported). It is a **dashboard**, not a second editor or second Live Lighting Preview. Avoid unnecessary headings such as "At a glance" and explanatory status phrases such as "Live when connected." Retain the approved card geometry and meaningful connection pill.

### Show Modes — structure / preview locked, editor direction approved

Hierarchy: **Show Modes heading → Effects / Sequences / Presets controls → compact Live Lighting Preview → one inline Effect Settings editor (for the selected effect or sequence step)**.

- Preview: **one strip per configured zone**, in actual configured output/zone order and with its real name/LED count; **up to 30 uniformly sized sampled indicators per strip**, not a hard cap of three strips. Sampling must be mapped to the actual zone length, not simulated on the reduced sample count. No physical geometry inference or mirroring. Show an explicit empty state for zero configured zones rather than inventing demo zones in production Show Modes.
- Animate actual chosen effects and sequences, with changes shown promptly; no trigger-animation playback in Show Modes. Preview only; no controller or trigger commands. Label simulation, stale live telemetry, and exact WLED frames honestly. The illustrative in-chat animation is **not** proof of WLED algorithm parity.
- One **inline Effect Settings** editor, no nested modal/card to reselect effects or duplicate quick-colors row beneath preview. Browse effects with a searchable alphabetical library and Favorites inside that editor; actual supported/unsupported state must reflect CORE capability.
- A metadata-driven editor should expose only applicable foreground/background/accent color slots, palette selector, speed, intensity, custom parameters and switches, with upstream labels/defaults. General brightness may remain a zone control. Empty effect metadata requires WLED-compatible normalization (Solid is one color, no palette). For standalone CORE, bundle verified metadata; WLED connection must not be required.
- Color editing is inline and touch-friendly. Color-role chips use the same picker; permanent Hue/Saturation sliders not required; HEX remains an optional advanced input. The picker form factor and full-library implementation still need final device verification.
- Retain restoration and exact indexed live WLED diagnostic inspection, but do not let diagnostic controls become a competing Show Modes editor.
- CORE's current independent five effects are not a complete port of WLED's effect algorithms. Source-provenance/licensing review, renderer parity and controller performance validation are separate engineering work.

### Layout — final visual concept approved (26 October 2026)

One Layout card, Add Zone in its header, editable pill project name, expandable zone rows with chevrons, editable names and counts, neutral sliders, zone-only Reverse, uniform 16-dot strip preview (not live telemetry), muted red Remove Zone directly under its preview. Preserve reorder, Undo/Redo, offsets, assignment semantics, and distinction between Studio draft and confirmed CORE output configuration.

- The approved Layout interaction includes an exact numeric LED-count input alongside the neutral LED slider, local immediate zone count feedback, and an inline two-step confirmation when removing a zone. Remove Zone remains beneath the sixteen-sample strip preview. This is visual approval, not evidence of on-device implementation.

### Triggers — selection/testing behavior approved

A trigger card selects/deselects independently of its test action. Preserve simultaneous offline tests and multi-profile batch selection/editing, Park+Brake and Left+Right turn, trigger priorities, arbitration/stop/restore, and untouched fields. Custom triggers may receive contextual Delete with confirmation; never offer it for built-in road-lighting triggers. Trigger previews/tests are not Show Modes decorative previews.

### Auto Events — interaction accepted, final visual polish pending

Keep Startup/Shutdown offline previews and parameters in this single dedicated tab. They must not require CORE/WLED/BLE connectivity or transmit any commands. Neutral range sliders, no unintended green theming or redundant 'ACC off' preview.

### Settings — final organization pending

Keep each controller configuration, diagnostics entry point and preference **in one authoritative location**. Do not duplicate editable settings in Home or other workspaces; preserve functionality while consolidating navigation.

## Implementation and verification — NOT YET APPROVED AS COMPLETE

- Consolidate the three existing routing/nav systems before calling the actual app UI approved.
- Preserve native BLE authorization and transport, exact-index WLED inspection, trigger arbitration, output configuration, snapshot/restore and all installed release history.
- Compare Android on-device screenshots at matching viewports against the approved in-chat concepts. Check touch sizes, Back navigation, dark system bars, Live Lighting Preview placement and all six tab contents.
- Run existing Studio regression tests, source-copy parity, JS parse, Android build, CORE host/ESP-IDF checks. Update obsolete assertions that enforce four-pill navigation or preview-before-tabs without removing their underlying behavioral coverage.
- Verify zone counts 0/1/29/30/31/large, rename/order/reverse, device-reported vs draft, metadata normalization and unsupported effects. Preview must not send BLE/HTTP writes or modify trigger states.
- CI success does **not** establish visual parity, physical LEDs, BLE reachability, automotive failsafe or vehicle safety.
- Keep **PR #6 draft**. Merge only after explicit user approval with real APK/device and separate hardware tests as appropriate.

**Change discipline:** Every already-approved component is protected; only make justified, documented improvements to unapproved elements. Do not remove, rearrange, or restyle locked items in unrelated revisions.
