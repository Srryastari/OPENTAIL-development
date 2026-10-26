# OpenTail Studio v0.19.28 — Stop Show snapshot handling and Effects layout

**October 23, 2026 · Bench-test release**

## Fixed
- **Stop Show false-positive trigger warning:** When a bench test has already been stopped, a retained pre-test snapshot no longer blocks Stop Show. The previous warning incorrectly treated any saved snapshot as an active trigger.
- **Active-trigger protection remains:** A truly active bench trigger is still protected from a Show Modes Stop command. The message now lists active trigger names.
- **WLED command ordering:** Stop Show now waits behind effect writes already in flight, clears unsent live-effect edits, sends OFF through the serialized command queue, and verifies the controller reports OFF.
- **Recovery safety:** Stopping Show does **not** discard or overwrite a previous bench-test snapshot. Restore remains an explicit, separately verified action. Old-session recovery records are preserved.
- **Stop feedback:** Reports confirmed OFF, unverified readback, or active-trigger conflicts distinctly.

## Changed
- **Effects tab:** Moved **Effect settings** to the top of the main Effects workspace, immediately below the target selector and above the effect library/search.
- Kept inline **Fx / Foreground, Bg / Background, Cs / Accent** controls, the Universal Effect Editor entry point, and Classic inline controls.
- Added a compact saved-snapshot notice with a Restore Previous Lighting action when no trigger is active.
- Optimized the Effect Settings block for narrow screens.

## Verification
- JavaScript syntax check passed.
- 17 focused simulated checks for snapshot handling, recovery preservation, serialization and Effects layout passed.
- 19 prior v0.19.27 simulated sequence/palette/startup regression checks passed on v0.19.28.
- 7 additional end-to-end simulated bench checks passed (start Brake bench trigger → Stop Trigger → Stop Show → Restore Previous Lighting).
- **43 checks passed in simulation; physical WLED and browser visual tests are not claimed.**

---

# OpenTail Studio — Development Changelog

**Current source reviewed:** `opentail-v0.19.27.htm` (derived from v0.19.26, originally uploaded v0.19.23)  
**Embedded version:** `0.19.27`  
**Review date:** October 23, 2026 (America/Chicago)  
**Status:** Pre-release; WLED bench/show interface, not validated vehicle safety software.

## How to read this log

- **Source-confirmed:** Implemented or visibly included in an available release file. It does not imply physical LEDs or electronics have been tested.
- **Historically reported:** Recorded release note or earlier conversation summary, with some early intermediate files unavailable for reinspection.
- **Design-only / pending:** App settings and plans without completed controller firmware, wiring, watchdog, vehicle I/O, or road-use verification.
- Only material changes supported by recovered artifacts/history are listed. Some early intermediate patch numbers and v0.19.1 have no sufficiently specific independent notes, so changes have not been invented for them.

## v0.19 — Progressive WLED editor and trigger/UI consolidation

### v0.19.27 — Sequence lifecycle recovery, shared palettes, palette previews, inline colors, automatic-event bench previews
- **Stop & Restore reliability:** Sequence manager now invalidates pending playback on Stop, prevents stale step/timer callbacks from submitting further WLED writes, and serializes the restoration command behind previously submitted device commands. Readback compares master ON/brightness, segment ON, effects, colors, palettes and available effect parameters. No success is claimed if readback disagrees. A global Stop & Restore control stays available during active playback, independent of the Sequences tab; navigating away or hiding the browser automatically requests stopping/restoration. Starting a second sequence or applying other Show Modes effects is blocked during restoration. **Limitation:** restoration cannot be guaranteed if WLED is disconnected, the tab is terminated, or power is interrupted; this remains bench/show software only.
- **Shuffle Each Loop titles:** As a new loop begins, the displayed step names follow the currently randomized effects and the active step is marked PLAYING; randomization does not rewrite the saved step definitions. Once stopped, the original saved effect titles return.
- **Shared Sequence Palette:** New opt-in checkbox and controller palette selector. With it enabled, compatible sequence effects share a selected palette while independently saved step palettes remain unchanged. Shuffle and normal playback both honor this option. Settings persist in local sequence options and schema-v4 project export/import; default is OFF.
- **WLED palette previews:** Loads original color stops from the connected WLED controller's paginated `/json/palx` endpoint. Adds gradient preview swatches and a browseable palette list in Universal Editor, classic inline Effects controls and Shared Palette settings. Missing/unsupported palette-color metadata shows **Preview unavailable** instead of a fabricated gradient. Network loading remains optional and does not block connection.
- **Effects inline colors:** Restores immediately visible, metadata-aware Fx/Foreground, Bg/Background, and Cs/Accent color pickers in Show Modes → Effects → Effect settings, while retaining the Universal Editor and optional Classic inline controls. Updates use the existing WLED queued-write/readback path.
- **Startup/Shutdown manual previews:** Explicit Preview buttons in Triggers → Automatic Events play the configured event effect, primary color and duration on *assigned and WLED-matched* LED sections, using the same cancellable bench sequence manager and automatic restoration. The buttons are manual tests only. Ignition/engine-running sensing, shutdown power hold and firmware events are still **not implemented** by this web page.
- **Test evidence:** `node --check` passed, and a Node.js simulated-device harness passed 18 checks including delayed in-flight POST cancellation, navigation after Stop, timed three-step shuffle/renamed titles, shared palette and project persistence, genuine palette stop parsing, three inline color slots, automatic-event targeting and timed restoration. Browser visual rendering and actual WLED/GS400 testing **were not completed** this revision; Chromium navigation was blocked in the session environment.
- **Unresolved:** Physical controller Stop/Restore tests under poor Wi-Fi and rapid navigation, actual `/json/palx` compatibility/version-specific palettes (especially dynamic/custom palettes), visible mobile layout, firmware-backed automatic events, physical ACC input, independent road-lighting safety fallback and dual-controller failover.


### v0.19.26 — Shared WLED+-inspired effect editor and improved Sequences
- Introduced an **original OpenTail universal effect editor** modeled after the WLED+ application's interaction patterns. The editor opens as a bottom drawer on mobile and a side panel on desktop, without embedding or copying proprietary WLED+ app code or imagery.
- **Entry points:** Show Modes → Effects, Show Modes → Sequences → Step Edit, and Triggers → selected section(s). Context-aware Save applies locally to trigger/sequence profiles; Apply on WLED issues device commands for Show Modes Effects. Legacy inline editors remain available under a collapsed Classic inline controls panel for fallback.
- Searchable WLED effect chooser, optional favorites filter, metadata-driven native speed/intensity/custom sliders and option toggles, palettes, and editable Foreground (Fx), Background (Bg), and Accent (Cs) colors. Added color wheel, swatches, RGB value adjustment, and a 160-byte segment brightness ceiling; effective device brightness remains bounded by existing live limits.
- Expanded sequence steps from effect-name and duration only into **per-step persisted native effect settings**, including active slider/checkbox values, palette, three WLED color slots, and brightness cap. Browser playback now sends these settings instead of silently generating generic effect defaults, and uses readback-aware restore of the previous lighting after Stop.
- Adds a **three-step, three-second starter sequence** when no prior sequence exists, and a Start with 3 steps button for intentionally empty projects. Existing saved sequences are preserved.
- Adds Shuffle Effects Now and Shuffle Each Loop, an optional Favorites Only shuffle pool, step duplication, moving up/down, and clearer step durations. Shuffle Now preserves individual durations; Shuffle Each Loop creates transient choices without silently overwriting saved sequence steps.
- Enhanced project exports/imports to include shuffle options alongside the existing sequence data while preserving project schema v4.
- Preserved the interactive three-color quick swatches in Trigger's Current Effect summary, Copy/Paste/Undo, universal zone IDs, 1D effect filters, and existing trigger safety checks. No physical trigger inputs, automotive watchdog, backup-controller failover, or audio-reactive CORE firmware were added.
- **Browser-level verification:** Headless Chromium rendered the modal and sequence cards in a simulated environment. Simulated WLED writes confirmed native speed/intensity/colors, brightness ceiling, sequence stop/restore, and standalone effect application. Additional offline checks confirmed multi-section trigger save, no assignment changes, cancel behavior, starter steps, and schema-v4 backup. JavaScript syntax passed. Color wheel/swatch interaction rendered and passed a click test.
- **Not validated:** physical WLED/GS400 output, real-network timing, complete WLED firmware-version compatibility, RGBW white channel control in the new shared editor, iOS native WLED+ behavior, actual vehicle lighting or CORE failover. Simulator readback is not proof of physical performance.


### v0.19.25 — Trigger editor card UI and animation copy/paste
- Restyled the **Triggers** page to use compact, readable cards: trigger overview, LED section assignment tiles, selected-animation summary/color swatches, and larger touch-friendly controls. The existing broader OpenTail navigation/theme was preserved.
- Added separate **Assign**, **Edit**, and **multi-select** actions per section. Batch selection supports Edit All Assigned, Select All, and Clear, without changing the underlying trigger assignments just by selecting a section.
- Added a session-only **Copy** button that captures one section's effective animation (including inherited profile settings). The clipboard survives switching triggers/sections within the same open app but is **not** written to local storage, exported, or carried to another session.
- Added **Paste** with four modes: Full Animation; Colors Only; Effect & Motion; and Choose Settings (selectable individual properties). Paste can target one or multiple sections of another trigger. Copies applicable foreground/background/accent colors based on the destination effect's WLED `/json/fxdata` color-slot metadata. With metadata absent, only foreground color is assumed for copy/paste (conservative fallback).
- Full/effect paste includes compatible WLED effect, native speed/intensity, custom controls/options, palette, direction, brightness (Full only), and step/loop/hold timing where represented by the profile. Pattern incompatibility is blocked if connected-effect discovery is available.
- Added a one-step **Undo** that restores the complete previous override for every pasted destination, including removing a newly created override to restore inherited behavior. Local save failure rolls the transaction back. Clipboard and undo are transient.
- Paste **never modifies trigger-to-section assignments, trigger names, priorities, physical input mappings, nor activates an inactive trigger**. When its destination trigger is already active and connected, it requests a bench-trigger refresh; failed/unconfirmed refreshes are explicitly reported as local-only saves.
- Polished **Automatic Events** container styling, while retaining its design-only hardware-ACC disclaimer. No hardware input, watchdog, or road-lighting logic was added.
- Removed the previous implicit `otCopySectionAnimation()` helper that silently propagated limited settings to all assigned sections (it was not referenced by the current UI).
- **Verification:** JavaScript syntax check passed. A Node.js isolated program-state harness passed 12 checks for cross-trigger copying, field-specific and batch paste, supported-color filtering, persistence, Undo, navigation, unsupported effects and ensuring pastes do not start inactive triggers. Generated Builder markup was checked for relevant controls; this is **not** a rendered browser-layout test.
- **Not tested:** actual browser visual/touch behavior (browser runner blocked local loads), physical WLED output and readback, live trigger playback, CORE firmware or 12 V safety validation.

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

## Current source capabilities (v0.19.27)

**Present in HTML source (not proof of working hardware):** Dashboard, Connect, Show Modes (Effects/Sequences/Presets), Triggers, Layout, Diagnostics, browser-local autosave, JSON project export/import, WLED REST/WebSocket integration, live pixel peek, optional GS400 template, 1D effect filtering, native WLED effect controls, editable section assignments/batch settings, browser-driven sequences, and offline failover profile.

**Project constraints preserved:** GS400 example uses 1,246 WS2815 pixels and BRG color order. Actual production app is universal/vehicle-neutral. Stable WLED-hosted path is `/opentail.htm`; use the versioned downloadable release basename `opentail-v0.19.27.htm` for archives.

**Failover panel is design-only:** Its example currently shows two ESP32-S3 devices, **8** vehicle inputs and **two buffered chains** totaling up to **6,000** pixels, with an exclusive-selector/manual-handback contract. This local two-chain example is **not** a claim that the production OpenTail CORE 12-channel or switched-output hardware has been designed or wired.

## Verification and known blockers

- JavaScript syntax verified for v0.19.27. The HTML title and `OT_VERSION` both read **0.19.27**.
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
