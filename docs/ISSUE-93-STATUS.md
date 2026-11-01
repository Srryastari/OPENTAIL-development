# Issue93 — partial candidate

Base PR92: 8cfd6d020e4d0acc466947f7f3adc00e68a05b5f.

Implemented, phone acceptance pending: true-black swatch (#000000); Show Modes effect-switch settings isolation; legacy outgoing settings snapshot on switch/deselection with JVM regression.

PARTIALLY FIXED: Chase3 third circle and numeric labels1,2,3. Shared renderer lacks tertiary input, so actual third-color output remains unresolved.

UNRESOLVED: full140 metadata audit; Colors1&2 labels; all-context isolation audit; Solid Glitter GL; live palettes; valid custom73–255 swatches; wheel accuracy; full preview brightness; speed mapping and frame scheduling; selection continuity; reverse and overlay proposals; Blink/Strobe extreme speed; BPM/Breathe/Fade/Fireworks1D smoothness; Chase-family emission; Event Horizon; Fusion Core; TriFade; TriWipe; TVSimulator; second-output preview stutter.

Tests: exact-head CI required. No physical/phone tests performed. Renderer/palette engine and firmware unchanged. All physical release gates remain blocked. Do not close Issue93.
