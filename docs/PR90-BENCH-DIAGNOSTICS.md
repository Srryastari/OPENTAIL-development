# PR90 bench follow-up: configuration and telemetry evidence
Base is frozen PR #90 SHA 68bbe544e1c56fa9331ef1bc36a75e348d27d37f. That branch is not modified.

Owner initially reported successful boot/BLE but no light after creating a local 30-pixel zone, with "Awaiting CORE frame." After the suggested CORE Output Manager readback/bench-test flow, the owner reported LEDs illuminate. This establishes a working physical-output path for that test. It does not uniquely establish which original step was missing, or certify wiring/power for full load.

Source evidence:
- Local Layout zones/physical assignments persist the local project only. They do not save output geometry to CORE.
- CORE's first-boot placeholder is 1 pixel, one OFF zone. OFF/zero-sized/unselected zones are excluded by the authoritative effect bank. An explicit hardware save/readback is required; never auto-enable it.
- Android dropped all-black snapshots from its lit-frame display preference, and its final preview status ignored the actual capture failure message. A verified black CORE frame can consequently say "Awaiting CORE frame." Background retries also overwrote errors with "Capturing" too quickly.
- The confirmed bench test used hard-coded both-output/all-zone masks rather than the existing selected masks.

Corrections are confined to Android diagnostics and bench-command admission. Status now distinguishes valid black CORE evidence (and existing design-color fallback) from missing/error snapshots; retry retains its last failure until a successful frame. Existing approved capsule rendering/fallback colors, dimensions, screens, controls, palette engine and all 140 algorithms remain unchanged. Bench tests use only selected non-OFF zones in existing CORE readback, otherwise explain that local Layout is not hardware configuration. This never saves/changes CORE outputs or bypasses its own validation, owner authorization, TTL, brightness cap, STOP or physical input priority. No firmware reflash is needed for this correction.

Regression: JVM tests cover 30-black-pixel CORE snapshots, project-vs-CORE provenance, capture-error labels, absent readback/inert OFF outputs, exact selected-output/zone masks and readback immutability. Existing Android parity/motion/JVM/static contracts and integrated CORE/source locks remain CI gates. The correction has its own reviewed Android tree and an exact seven-file Android change allowlist; frozen PR90 matrix/source are untouched.

Minimum owner acceptance after installing the corrected APK with the same working firmware:
1. Reload Output 1 and confirm actual readback count/role match the intended saved setup. Use the same successful bench test, with Output 1 selected, and record the preview status while lit.
2. STOP or allow 15-second TTL to expire. Confirm the completed black snapshot is identified as CORE black rather than "Awaiting"; configured/project fallback, if displayed, is labeled as design colors.
If capture fails, report the exact new status and firmware identity/readback instead of repeating rewiring or arbitrary firmware changes.

Bench separation:
- Correct active readback + verified nonzero CORE frame + no physical light points downstream of logical rendering (GPIO/DIN/ground/supply/driver signal); a snapshot proves logical RGB/RMT completion, not electrical/optical emission.
- OFF/no enabled readback or missing explicit save points to configuration.
- Explicit snapshot error points to capture/transport or runtime-output completion and needs that error/readback, not guessed RGB.
- Actual black frame means CORE produced black; inspect effect acceptance, active roles/inputs, STOP/TTL/calibration before power/wiring assumptions.

No new physical acceptance claim, merge, change to frozen #90, or release certification.

Additional confirmed controller-command root causes: Show Modes callbacks required liveArmed before calling previewCurrentEffect, even though that function owns arming and the approved UI advertises automatic authorized live edits. Remove that circular prerequisite only for supported gesture fields. previewCurrentEffect retains explicit Offline, authorization, advertised effect and BLE palette admission; LiveBlePreview retains brightness cap, coalescing, bounded TTL and STOP. Settings' explicit physical output/zone selectors and confirmed bench action now leave Preview Offline; connecting alone does not enable output. No geometry changes, implicit NVS writes or Android physical-frame generation.

Minimum controller-control test: with the same verified saved Output 1, authorize/read capabilities, select OUT 1 in the existing Settings physical preview controls, then select Solid and change its editable color in Show Modes. CORE should render the bounded preview without needing a prior mask toggle to set a hidden flag. STOP must restore normal physical-input behavior. Preview Offline must remain local until an explicit physical bench control is used.
