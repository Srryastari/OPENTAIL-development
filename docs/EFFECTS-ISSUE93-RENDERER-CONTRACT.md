# Issue #93 — Authoritative shared Effects RGB/color-role contract

**Ownership:** Effects-owned pure C renderer and effect metadata only. Based on PR #94 `b970f92c9155ac9e2d2c9ffa54afa2d1ca3502cd`. All 140 IDs retained. **Draft, software-only.** No Android UI changes, BLE command changes, NVS schema changes, firmware version bump, physical LEDs or failover certification.

## Shared configuration contract

`ot_wled_fx_config_t` retains existing field order and appends:
```c
ot_wled_rgb_t tertiary;
bool tertiary_set;
```
These are **local renderer inputs**, not an existing BLE wire extension. Old designated/zeroed configurations use black third color in three-color Chase/Fade/Wipe, and legacy white glitter in Solid Glitter. Explicit `tertiary_set=true, tertiary={0,0,0}` permits a **true black glitter color**, with no artificial minimum emission. Color scaling uses the same configured brightness as other RGB colors; physical bench caps are separate. The field is not persisted by the current legacy two-color transport or automatically available in already-shipped Android JNI copies.

`ot_wled_fx_color_roles(id, roles[3])` and `ot_effects_1d_color_roles(id, roles[3])` provide **three ordinal positions** with `enabled` and a short `label`. They parse source-owned WLED mode metadata; disabled positions are **never compacted**. Existing metadata booleans and palette capability flag remain authoritative; effects without source role grammar fall back only to advertised foreground/background slots. The 19 native effects and Embers get their existing declared first/second roles; no unsupported third is added.

### Verified named IDs

| ID | Mode | Enabled roles | Source correction |
|---|---|---|---|
| 54 | Chase 3 | 1 / 2 / 3 | Three independent colors, legacy third black |
| 55 | Tri Wipe | 1 / 2 / 3 | Third wipe region supplied by tertiary |
| 56 | Tri Fade | 1 / 2 / 3 | Three fades connect first→second→third→first |
| 103 | Solid Glitter | BG / disabled / GL | Third glitter color changes actual pixels; legacy omitted GL white |
| 84 | Solid Pattern Tri | 1 / 2 / 3 | Optional third stripe color |
| 34 | Colorful | 1 / 2 / 3 | Optional third in high-intensity authored three-color mode |
| 78 | Railway (Colors 1 & 2) | 1 / 2 | Second is not a fabricated background role |

**Additional proven third-slot bindings (legacy defaults unchanged):** Chase ID28, Chase Random ID29, Two Dots ID50, Running Dual ID52 and Glitter ID87. These join Chase 3, Tri Fade, Tri Wipe, Solid Glitter, Solid Pattern Tri and Colorful. Issue #102 additionally source-proved Aurora ID38 (only when an explicitly selected three-color palette consumes Color 3) and Popcorn ID95 (preserving its source RNG slot selection): **13 independently authored tertiary consumers**. All 13 retain their original behavior when `tertiary_set=false`.

**Six metadata-declared third slots remain NOT SUPPORTED in the accepted isolated 1D profile:** Scan IDs10/11, Scanner IDs40/60, Rolling Balls ID48 and Bouncing Balls ID91. Their third metadata positions describe unimplemented source options or overlays, not a verified third RGB paint path. The 140-row CSV marks these separately. A visible third editor circle is never proof that CORE consumes it.

**Explicit black regression:** `tertiary_set=true, tertiary=#000000` is a valid authored third color. Aurora palette ID5 must retain a black third stop instead of inferring presence from nonzero RGB; Popcorn and Solid Glitter must emit exact black for their third-color particle/glitter slot. Legacy unset behavior remains unchanged.

**No additive highlight:** `#000000` remains zero; pixel backgrounds/off slots stay unchanged unless the effect's *authored source* fills them.

## All-140 evidence matrix

See `docs/EFFECTS-ISSUE93-140-ACCEPTANCE.csv` for one row per actual registered effect ID, source identity, metadata-derived color roles, palette flag, exhaustive host render admission at Speed 0/1/128/254/255 and Intensity 0/128/255 and brightness 0/255, upstream source/one-shot golden status, and separate **phone NOT_RUN** evidence. Host matrix = 140×5×3=2100 profiles, both extreme brightness states per profile. Custom palette IDs 73–255 are **not** inferred to be valid colors; they require caller-owned resolved palette data for eligible effects and independent integration proof. The existing source/host golden suite is the independent source-of-truth; this CSV is an index, not a replacement for its logs.

## Physical-trigger override continuity — host-only evidence

`core-firmware/tests/test_effects_oem_safety_matrix.c` now runs the real pure-C WLED Chase renderer beside an uninterrupted reference while the visible pixel is replaced by the output-manager physical overlay. Six cases cover Brake, Left, Right, Hazards-left, Hazards-right and Reverse. The renderer state continues advancing during the override; after release, the next decorative frame must match the uninterrupted reference at the same timestamp. Exact-head host output: `Effects phase restoration: 6 Brake/Left/Right/Hazards/Reverse override cases resumed at uninterrupted phase (host only)`.

The companion source-contract check verifies that CORE renders the decorative frame before applying physical trigger overlays. This proves host-level renderer/overlay phase behavior only. It does **not** prove FreeRTOS scheduling, RMT/DMA completion, electrical override latency, optical WS2815 output, or vehicle behavior; those remain separate physical NOT_RUN gates.

## P0 remaining investigations — do not claim a phone or vehicle fix

| Symptom | Source-owner finding / next proof | Status |
|---|---|---|
| Blink ID1, Strobe ID23 at Speed255 | WLED-derived `cycle_time=2×frame_time`; `phase<=on_time` may sample ON at both 0 and frame-time on a frame-locked preview, so OFF phase becomes unobservable. Needs a source-phase sampling/scheduler contract, not a UI animation or arbitrary global change to pinned WLED frames. | OPEN / phone NOT_RUN |
| BPM ID68 stutter | Source `phase=(uint8_t)(now/20)` advances on 20 ms boundaries and beat sampling has integer granularity. Measure 30/60/120 FPS source frames before any approved visual-policy change. | OPEN |
| Breathe ID2, Fade ID12 | Source uses quantized 8-bit brightness and 16-bit time/waveforms; smoothness depends on cadence and actual display fps, not merely register presence. Do not insert interpolation into source-parity path without controlled reference frames. | OPEN |
| Fireworks 1D ID90 | Effect has retained source physics/trails and advances once per render; profile equality does not establish optical cadence. Record monotonic frame timestamps at phone and MCU. | OPEN |
| Chase ID28, Chase Rainbow ID30, Chase Random ID29 | Source code deliberately paints an overall `background`/rainbow/wheel base. Forcing black globally would change pinned WLED-authored identity. Owner-reported unwanted backlight requires independent intended-black region evidence and explicit source-behavior change approval. | OPEN |
| Event Horizon ID32786 | Native one-shot source has authored moving core and exact foreground terminal frame; prior pure-C endpoint tests PASS. Owner-reported missing local preview is **not** disproved by source-only test. Capture JNI/CORE frames and preview routing before attributing failure. | OPEN |
| Fusion Core ID32778 | Native radius/pulse + white-centered aura, independent of Solid Glitter state; existing 12-original phase golden suite PASS. Phone appearance similarity requires actual-frame/owner visual acceptance. | OPEN |
| TV Simulator ID116 | Source uses per-zone allocated TV scene state and randomized long-scene/short-transition timing; static-looking short sampling can occur. Capture comparable >scene-length traces, verify resets and active-render timing. | OPEN |

## Firmware / Android integration handoff

1. Firmware must pin exact Effects SHA and use `tertiary` only when an authorized caller can actually supply it; current BLE v1/v3 transport **does not** encode the new slot. A future explicitly approved compatible transport extension/readback is required for actual controller third-color authoring. Do not invent fallback values, overwrite NVS or change physical trigger priority. Continue independent Brake/Turn/Reverse arbiter and status/STOP tests.
2. Android Architecture must update **only the pure-C JNI renderer copy** after source review, regenerate immutable file hashes and re-run 140-ID byte-exact oracle comparison. The existing PR #94 third editor circle **does not yet transmit a third RGB channel** to CORE. Do not claim Android/Core output matches until actual third-color binding and transfer is certified. **No UI geometry, labels, palette styling or approved six-screen layout alterations authorized.**
3. For all phone reports, distinguish *Android offline preview*, actual *CORE local frame* and *optical WS2815 output*. Issue #93 observations were offline Android, not bench evidence. No physical GPIO17 claim, flash, release, auto-merge or failover certification.

**Release gates:** All 22 mandatory physical checks NOT_RUN; automatic hardware failover NOT_VERIFIED; vehicle release BLOCKED. Separate exact HEAD host golden, trigger, ESP-IDF and Android metadata CI evidence is required.