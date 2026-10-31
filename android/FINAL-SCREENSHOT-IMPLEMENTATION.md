# Final approved screenshot implementation

Authority: the seven user-supplied screenshots in this task. The combined reference determines the visible six-screen structure; earlier detailed editors remain relevant where supplied. This is a native Kotlin/Compose implementation, not a reconstructed HTML runtime.

## Implemented

- Ordered, horizontally scrolling Home / Show Modes / Layout / Triggers / Auto Events / Settings navigation; 36dp capsules, 15sp text, 9dp horizontal padding and 4dp spacing.
- Neutral OLED and light themes, rounded borderless cards, outlined pill fields, native vector controls and thin capsule sliders with dark glowing thumbs.
- Home Connection, 30-capsule LED Preview and Controller Status cards. Each preview capsule is 24dp high with 3dp gaps. Read-only verified CORE snapshots refresh while the screen is foregrounded; refresh/pause are functional. Local indexed colors appear only during explicitly selected offline preview. Unavailable CORE frames remain unlit.
- Show Modes order Effects / Presets / Sequences. Source-pinned effect parameter flags control displayed colors, motion and palettes. All 140 definitions are checked against CORE #49. Existing runtime BLE effect and palette admission gates remain separate.
- Preset name selection, inline pencil rename and confirmed trash removal. Selection preserves the preset collection. The effect identity is stored with newly created presets.
- Layout project card, inline zone expansion, output assignment without displacing another zone, per-output displayed offsets, step-one sizing and +/-1, reverse, insert-after and confirmed removal. Exact-pixel authoring and physical Output Manager remain available. Editing never writes CORE NVS.
- Full-width selectable trigger rows with individual pencil editing and bulk Edit Selected. Effect/color edits are local copy-on-write transactions and preserve unedited mixed bulk values. No trigger trash control.
- Compact Startup/Shutdown rows, pencil editor, On/Off, duration, fade/snap and metadata-gated colors.
- Settings Connection / App Preferences / System landing cards. Real BLE authorization and connection state; appearance selection; existing diagnostics, import/export, palette transfer and STOP retained behind Firmware details.

## Software validation

Run from repository root:

- `node android/qa/test_native_source.cjs`
- `node android/qa/test_live_edit.cjs`
- `python3 android/qa/check_release.py`
- `python3 android/qa/check_pinned_core49_contract.py` with its three immutable source worktrees
- Gradle `:app:assembleDebug :app:testDebugUnitTest`

The JVM suite includes transactional bulk-style/event-color failure cases and effect metadata coverage. The pre-existing disk rollback assertion now compares normalized JSON instead of comparing pretty/compact formatting.

## Acceptance limits

Passing source checks or compilation is not pixel-perfect certification. Native screenshots and expanded interaction states must be compared with the approved screenshots before visual sign-off. A disconnected test has unlit CORE indicators rather than a fabricated rainbow animation.

Universal per-trigger hardware playback, controller reset and volatile physical zone/pixel geometry are not available through the pinned Android BLE contract. The trigger play target is disabled and controller reset reports its unavailable protocol. This change does not invent a command or falsely report controller activation. CORE's two-output wire format is retained; future output counts require a capability/protocol change.

Physical BLE/LED timing and vehicle acceptance are untested. No firmware, renderer or trigger-engine source is modified. Publication was blocked by automatic approval review; local commits alone are not a published PR.
