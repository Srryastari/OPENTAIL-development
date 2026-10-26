# OpenTail Studio — Development Changelog

**Current source reviewed:** `opentail-v0.19.24.htm` (derived from uploaded `opentail(20261023-020915).htm`)  
**Embedded version:** `0.19.24`  
**Review date:** October 22, 2026 (America/Chicago)  
**Status:** Pre-release; WLED bench/show interface, not validated vehicle safety software.

## How to read this log

- **Source-confirmed:** Implemented or visibly included in an available release file. It does not imply physical LEDs or electronics have been tested.
- **Historically reported:** Recorded release note or earlier conversation summary, with some early intermediate files unavailable for reinspection.
- **Design-only / pending:** App settings and plans without completed controller firmware, wiring, watchdog, vehicle I/O, or road-use verification.
- Only material changes supported by recovered artifacts/history are listed. Some early intermediate patch numbers and v0.19.1 have no sufficiently specific independent notes, so changes have not been invented for them.

## v0.19 — Progressive WLED editor and trigger/UI consolidation

### v0.19.24 — Show Modes three-color editor and regression cleanup
- **Show Modes → Effects → Effect settings:** separate and immediately visible **Foreground (Fx / WLED col[0])**, **Background (Bg / col[1])**, and **Accent (Cs / col[2])** color inputs, plus independent RGB level controls. Only color slots advertised by the selected effect's WLED `/json/fxdata` metadata are shown. With metadata unavailable, all three standard slots are offered with an explicit compatibility warning. An explicitly empty metadata color section hides all color inputs.
- Reworked optional color wheel/swatches to edit whichever of the supported slots the user selects; direct pickers remain visible without repeatedly switching tabs.
- **Fixed lost quick edits:** consecutive changes to two or three color channels are batched into one WLED segment command. Subsequent edits while a request is in flight are queued; effect changes wait for a currently active request to finish.
- Confirmed effect-setting writes using a subsequent WLED `/json/state` readback; failures display a clear warning rather than implying physical confirmation. A write to an RGBW-capable color slot clears its previous white-channel value, while other color slots are preserved.
- Avoided misleading local autosave feedback for transient live effect edits; these settings are sent to WLED and can be captured into local Presets, but are not falsely reported as browser-project persistence.
- Removed the retired OpenTail Originals player, obsolete visualization code, and its CSS, while **retaining recovery-only support** for an interrupted Originals snapshot created by an earlier build. Legacy recovery remains accessible in Diagnostics.
- Updated the About information and embedded version. Retained existing universal layout/GS400 example, WLED section targeting, presets, trigger editor, diagnostics, and failover design panel.
- **Verification in this build:** JavaScript syntax passed and an isolated headless Chromium test with a simulated WLED controller passed 14 reported checks covering metadata visibility (1/2/3/0 channels), rapid color edits, preservation of unrelated colors, optional RGBW white handling, effect selection, preset capture/restore, reversible software-trigger assignments, readback failure reporting, Layout/Triggers/Diagnostics rendering, local autosave startup data, and absence of uncaught browser exceptions. A mobile-width screenshot was inspected.
- **Not tested:** physical LEDs at 10.0.0.13, BRG color fidelity, 12 V hardware triggers, overlapping real bench triggers, sequence playback on hardware, CORE failover or road-use safety.

### v0.19.23 — Previous uploaded baseline
- Added custom **software** triggers: create/rename by creation, delete and associate sections without implying additional hardware input channels.
- Streamlined trigger cards with one-tap ON/OFF, separate editing, one-tap section assignment, and batch editing for assigned sections.
- Added a saved WLED Presets section: capture active WLED state, rename, overwrite, apply, delete; up to 40 local project presets; project export/import includes them.
- Simplified Startup/Shutdown automatic-event settings (WLED effect, color, duration) as saved configuration only. Actual ACC edges require CORE firmware.
- Implemented 1D effect filtering through `/json/fxdata` compatibility metadata, filtering matrix-only and unverified effects from ordinary Show Modes and Builder effect selections.
- Show Modes now exposes Effects / Sequences / Presets. Removed the **Originals tab** from navigation; the old Originals source functions remain in the file and are not a supported current mode.
- Preserved WLED live preview, segment effects, project backups, trigger priority logic and the offline failover design panel.
- **Not validated:** software-trigger behavior through CORE hardware; actual ACC-triggered events; watchdog failover; production animation engine; full physical WLED regression.

### v0.19.22 — Native WLED parameters and speed range
- Populated trigger/section controls from WLED `/json/fxdata` effect metadata.
- Enabled native `sx`/`ix`, custom `c1`/`c2`/`c3`, options `o1`/`o2`/`o3`, palette and tertiary color in per-section and batch edits.
- Exposed full WLED 0–255 speed/intensity where appropriate.
- Expanded experimental Originals preview speed to 300%.

### v0.19.21 — Builder and screen cleanup
- Removed duplicated trigger selectors and redundant Builder rename/preview controls.
- Simplified trigger editing workflow; made active test status easier to distinguish from edit mode.
- Removed the simulated turn-flasher panel from the normal Builder UI (related code was retained).
- Simplified Connect display, reducing redundant settings.

### v0.19.20 — Defaults and trigger-state fixes
- Changed new/default trigger animations to **Solid**; defaulted park/brake to red, turn/hazards to amber and reverse to white.
- Removed default `F1 Brake` as a separate visible trigger; migrated legacy F1 Brake assignments/settings into Brake with legacy backup handling.
- Improved trigger activation/stop state persistence and blocked show playback during uncertain or active bench triggers.
- Added experimental Plasma Sweep Original and adjusted experimental streaming UI.

### v0.19.19 — Layout simplification
- Removed pixel-placement and duplicate Arrangement editing features from normal Layout UI.
- Made the Dashboard pixel view display-only instead of an arrangement editor.
- Retained section names, start values, lengths, reversals, Add/Remove/Restore, WLED section import and manual sync.

### v0.19.18 — Universal, vehicle-neutral projects
- New projects start without GS400-specific sections; retained an optional **GS400 1,246-LED example**.
- Added/strengthened project names and project import compatibility; preserved existing projects.
- Removed chassis-based restrictions from experimental Originals and clarified their hardware/pixel limits.
- Made the dual-ESP32 failover display more explicit about the two output chains being an example, not a universal production pinout.

### v0.19.17 — Trigger assignment and color reset repair
- Corrected section-to-trigger assignment state and batch assignment UI.
- Improved pure factory-color resets for parking/brake/turn/reverse style profiles without silently changing assignments.
- Retained multiple simultaneous bench triggers and explicit restoration controls.

### v0.19.16 — Concurrent trigger bench tests
- Allowed multiple software-simulated triggers to be active at once, resolved per WLED section by priority.
- Separated **Stop Trigger**, **Stop All (OFF)**, and **Restore Previous Lighting** semantics.
- Kept protected pre-test snapshots and made restore a distinct action from simply clearing trigger outputs.

### v0.19.15 — Trigger transition verification and experimental Originals
- Added checks intended to prevent old trigger colors from remaining during transitions and improve WLED readback verification.
- Improved batch editing of mixed values, and clarified selecting a trigger for editing versus activating one.
- Expanded Originals with additional spark/glitter patterns and a faster browser-canvas preview. Physical LED frame rate remained limited by browser/WLED transport.
- Note: the exclusive-trigger switching behavior of this version was revised in v0.19.16 to support concurrent priority-based triggers.

### v0.19.14 — Experimental OpenTail Originals
- Added five initial pixel-generated Original effects, a browser preview canvas and experimental frame streaming to WLED over HTTP.
- Added safety/restore handling around experimental playback.
- High-refresh browser preview was **not** proof of high-FPS LED output or a working ESP32 firmware animation engine.

### v0.19.13 — Bench power and sequence preview
- Added user-adjustable physical color-test brightness with a conservative, supply-aware estimate. This is not a hardware current limiter.
- Added direct access to full WLED controls.
- Implemented browser-driven sequence playback preview; unattended/vehicle-grade timing was not implemented.

### v0.19.12 — Effect color/palette state repair
- Reset stale segment colors, palette and brightness when changing effects so previous sections would not contaminate a new effect.
- Improved pure RGB/white bench tests, low-brightness isolation, and previous-lighting restoration.
- Preserved previously confirmed BTF WS2815 **BRG** color order rather than changing it to mask palette issues.

### v0.19.11 — Saving/status UI cleanup
- Removed repetitive persistent “Saved locally” labels; retained delayed confirmation toasts, meaningful failure alerts and local autosave.
- Continued v0.19.10 effects/slider UI improvements.

### v0.19.10 — Color editor and touch usability
- Clarified Primary (Fx), Background (Bg), Accent color controls; redesigned sliders with live value labels.
- Moved wheel/advanced color controls into expandable area and improved mobile touch targets.
- Added debounced local-save toast and less noisy feedback.

### v0.19.9 — Effect target/default repair
- Defaulted Show Modes Effects to **all configured WLED segments**.
- Fixed applying effects/settings to partially configured bench layouts.
- Exposed effect errors in the effects area rather than silently leaving old behavior on the lights.

### v0.19.8 — Multi-section effect selection repair
- Removed duplicate effect naming such as `Solid,Solid` in batch selections.
- Showed a common effect once, or “Mixed” when selected sections differ.
- Ensured the selected effect is applied cleanly to all selected sections.

### v0.19.7 — Calculated LED totals and effect normalization
- Automatically calculated layout LED total from section ranges; did not attempt to autodetect physical strip length.
- Improved auto-offset and section range syncing when WLED's configured pixel capacity is sufficient.
- Normalized old duplicate effect names such as `Solid,Solid` or `Strobe,Strobe`.

### v0.19.6 — UI-state and readback repair
- Fixed Stop Trigger editing-state cleanup and Builder selections after navigation.
- Improved per-section effect selection/readback and retained partial Arrangement syncing.

### v0.19.5 — GS400 trigger/partial-layout fixes
- Repaired inherited GS400 trigger assignments, section mapping for partial WLED configurations and effect targeting/readback.
- Improved WLED sync conflict handling and trigger status.

### v0.19.4 — Failover planning re-integrated
- Retained v0.19.3 Studio base while integrating the **offline dual-ESP32-S3 failover design profile**, warnings and project backup schema v4.
- Added design settings, not active heartbeat firmware, selector switching or certified safety monitoring.

### v0.19.3 (preview) — Multi-section editing
- Strengthened batch selection/animation operations. Included targeted selection regression checks.
- No physical WLED/real-browser validation was reported for the preview.

### v0.19.2 (preview) — Local autosave
- Added browser-local save states and targeted autosave regression checks.
- Distinguishes browser-local persistence from WLED controller persistence.

### v0.19.0 (preview) — Bench baseline recovery
- Improved RGB/RGBW pre-test baseline comparison, tolerated small readback quantization differences, and made recovery warnings clearer.
- Earlier behavior and the About box described Stop Trigger/Stop All restoring the pre-test WLED snapshot; the explicit OFF-versus-Restore behavior was refined in v0.19.16.
- v0.19.1: no independent trustworthy version-specific release note recovered.

## v0.18 — Mobile source and failover configuration

### v0.18.1
- Added/configured an **offline design** for dual-ESP32 watchdog, heartbeat/timeout/recovery, backup output mappings and manual handback.
- Added failover warning/diagnostic UI, design JSON export and project import/export v4.
- Clearly marked hardware takeover and telemetry as unverified.

### v0.18.0
- Packaged a shared OpenTail Studio web source with mobile-app scaffolding using Capacitor for future iOS/Android builds.
- Retained Builder, Layout/Arrangement, Diagnostics, controller-address handling and WLED web access.
- Source/syntax validation was reported; production Android/iOS packages, signing and physical WLED tests remained pending.

## v0.17 — Universal zones, auditing and recovery

### v0.17.2
- Preserved archived section identities/assignments and per-section animation records; warned before partial/missing trigger playback.
- Saved/restored an expanded pre-test WLED state snapshot (effects, segment settings, custom parameters when supported).
- Rejected WLED `success:false`/error reports; added recovery warnings and a 30-minute, same-controller/geometry-limited saved snapshot.
- Added **read-only physical-section mapping verification** and project export/import v3 including sequences, hardware metadata and favorites.
- Removed unused duplicate legacy routines and hardened storage parsing.
- Release notes report JS syntax and mock regressions (19 previous + 17 extended), but **no real GLEDOPTO playback test**.

### v0.17.1
- Unified Test Trigger, Stop Trigger and Stop All controls; removed duplicate behavior.
- Migrated legacy zone aliases, pixel paints and styles; matched imported WLED segments to stable IDs by exact pixel ranges.
- Validated triggers and effects before execution, with richer WLED readback and scene restoration checks.
- Kept live Arrangement edits local until explicit sync; fixed tertiary color slot.
- Project export v2 and 19 simulated regression checks reported; real LEDs remained untested in that release.

### v0.17.0
- Introduced a **Universal Zone Registry**: stable internal section identity separate from user-facing name, pixel range and WLED physical mapping.
- Added migration for older GS400-specific section IDs and retained assignment/animation linkage.
- Prepared projects to cover non-GS400 vehicles.

## v0.16 — Trigger engine and release stability

### v0.16.8
- Repaired saved trigger-to-section mapping using stable IDs and known ranges, rather than section display names.
- Improved section-level animation inheritance and readback error handling.

### v0.16.7
- Corrected diagnostics/export version labeling to match the installed build.

### v0.16.6
- Corrected hosted WLED controller address/auto-connect audit logic and stale version constant; rechecked active version consistency.

### v0.16.5
- Added connection-in-progress protection, WLED JSON validation and clearer connection feedback to fix a Connect button requiring a prior color test.

### v0.16.4
- Added more explicit trigger success/failure feedback and save toasts; clearer missing-segment instructions.

### v0.16.3
- Revised trigger sequencing and effect resolution, including WLED native effect-name matching and rejection of unsupported `pixel-custom` as a native effect.
- Reduced overlapping/rapid-write races through queued command handling.
- Some embedded version text still lagged the intended release; later corrected.

### v0.16.2
- Revised section Sync to preserve existing lighting, brightness and effect settings rather than turning segments off.
- Tightened segment mapping checks and displayed skipped/unavailable sections during trigger playback.

### v0.16.1
- Reordered navigation as Dashboard, Connect, Show Modes, Builder, Arrangement, Diagnostics.
- Separated selected-for-edit effects from verified running effects; separated Builder edit selection from ACTIVE trigger state.
- Cleared temporary selection on navigation/reopen while keeping saved profiles, mappings, favorites and controller URL.

### v0.16.0
- Added deterministic multi-zone WLED bench-trigger arbitration and independent per-section animation plans.
- Bench/show only; no actual protected 12V vehicle input sensing.

## v0.1–v0.15 — Foundation and iterative UI revisions

Older early-release details below come primarily from recorded project conversations; original independent artifacts for every number were not all recovered.

- **v0.1:** First browser-based OpenTail prototype: WLED connection, basic lighting controls, trigger/editor concepts and simulation.
- **v0.2:** Reworked app for WLED-hosted HTTP deployment (`/opentail.htm`) to avoid browser mixed-content limitations.
- **v0.3:** Per-section controls and immediate section-level lighting updates.
- **v0.4:** WLED section import, whole-strip tests, effects access and a Diagnostics section.
- **v0.5:** Automatic section OFF/restore handling and detailed pixel-by-pixel editing.
- **v0.6–v0.7:** UI and trigger reliability improvements; v0.7 specifically audited selection bounds, highlighting and trigger feedback with simulated tests.
- **v0.8:** Live Arrangement length sliders, automatic downstream offsets, independent section testing and tab highlighting.
- **v0.9:** Repaired sliders, section on/off, color-order settings (including BRG), scene controls, Builder interactions and live effect editing.
- **v0.10:** Developer-oriented diagnostics and exportable troubleshooting information.
- **v0.11:** Effect/Background color controls and Diagnostics improvements.
- **v0.12–v0.13:** Expanded trigger animation editor and early active-state handling; persistent trigger toggles remained imperfect.
- **v0.14:** Refined Dashboard and navigation workflow.
- **v0.15 family:** Mobile-first unified WLED interface, section naming/preview, effects and live controls; v0.15.8 metadata/width fixes, v0.15.9 per-section metadata, v0.15.10 Arrangement navigation, v0.15.11 section selection and rename fixes. Exact scope of other v0.15 patches remains undocumented here.

## Current source capabilities (v0.19.24)

**Present in HTML source (not proof of working hardware):** Dashboard, Connect, Show Modes (Effects/Sequences/Presets), Triggers, Layout, Diagnostics, browser-local autosave, JSON project export/import, WLED REST/WebSocket integration, live pixel peek, optional GS400 template, 1D effect filtering, native WLED effect controls, editable section assignments/batch settings, browser-driven sequences, and offline failover profile.

**Project constraints preserved:** GS400 example uses 1,246 WS2815 pixels and BRG color order. Actual production app is universal/vehicle-neutral. Stable WLED-hosted path is `/opentail.htm`; use the versioned downloadable release basename `opentail-v0.19.24.htm` for archives.

**Failover panel is design-only:** Its example currently shows two ESP32-S3 devices, **8** vehicle inputs and **two buffered chains** totaling up to **6,000** pixels, with an exclusive-selector/manual-handback contract. This local two-chain example is **not** a claim that the production OpenTail CORE 12-channel or switched-output hardware has been designed or wired.

## Verification and known blockers

- JavaScript syntax verified for v0.19.24. The HTML title and `OT_VERSION` both read **0.19.24**.
- An isolated headless Chromium test using simulated WLED responses passed the checks listed above, including failure-path handling. This is not a live-device test.
- No physical WLED, live vehicle input, dual ESP32, watchdog selector, or actual road-lighting failover test was performed during this revision.
- Prior release packages report synthetic mock-controller checks and syntax audits; those do **not** certify current v0.19.23 end-to-end regressions.
- Test/verify local persistence and export/import migrations (especially saved presets, archived sections and per-zone animations).
- Verify Stop Trigger / Stop All / Restore Previous Lighting against physical LEDs under overlapping brake, turn and hazard scenarios.
- Validate effect metadata mapping and 1D filtering against the installed WLED version; verify primary/secondary/tertiary colors and `BRG` behavior.
- Verify browser-driven sequences and presets restore exactly without overwriting unrelated section states.
- Retired Originals playback code was removed; the narrow legacy snapshot recovery path remains and still requires a physical-device test if needed.
- Firmware-trigger input sensing, watchdog progress heartbeat, backup firmware, exclusive data selector, automotive protection, independent safety fallback, production output pinout, CAN expansion, and IP67 hardware are not completed/validated by the HTML app.

## Next-release change control

Maintain this file with every accepted revision. For each future version, record: user-visible changes; changed files/functions; backward-compatibility impacts; syntax/static/automated tests actually run; device/bench tests actually run; unresolved blockers; release filename and stable WLED deployment copy.
