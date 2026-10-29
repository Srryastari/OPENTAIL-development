# Native Android v0.2.0-alpha1 — on-device acceptance checklist

- [ ] Before upgrading, export a project backup from the old Android app.
- [ ] Install v0.2.0-alpha1 over previous app; ensure it opens immediately
      without a browser chrome, Wi-Fi prompt or second navigation bar.
- [ ] Confirm exactly six top pills in approved order, with no bottom tabs.
- [ ] Toggle OLED black ↔ white; check all cards, captions, pill buttons,
      text fields and sliders remain legible on a narrow phone.
- [ ] Show Modes: choose several effects (including original Materialize),
      change foreground/background/accent and sliders, start/stop preview.
      Verify **no physical LED change** from these design operations.
- [ ] Layout: add two arbitrary zones, resize the first, verify the
      second start index recalculates automatically from zero. Insert
      another zone between them; confirm order and trigger roles persist.
- [ ] Triggers: select two triggers simultaneously; assign zones and
      effect; confirm edits stay after app restart.
- [ ] Auto Events: configure optional Startup and Shutdown independently;
      confirm local preview and duration/fade persistence.
- [ ] Sequences: add/edit/duplicate/reorder/remove, repeat/shuffle.
      Presets: save/load/rename/delete locally; ensure no BLE calls.
- [ ] Export JSON with Android document picker, change the project, import
      the file again; verify names, zones, triggers, sequences, events and
      presets restore. Cross-import into web Studio where appropriate.
- [ ] In Settings explicitly scan/pair/authorize CORE via BLE, read FX
      capabilities, confirm a supported bench effect, and verify physical
      LEDs with short TTL and brightness cap. Stop and verify restoration.
- [ ] Open Settings → CORE Output Manager (Advanced), read Output 1 and 2
      independently, edit one draft, confirm the other remains unchanged,
      and save only after stopping shows/releasing trigger inputs.
      Require verified BLE readback and power-cycle persistence checks.
- [ ] Disable Bluetooth; confirm no stale authorized status or unintended
      operations. Re-enable and reconnect explicitly.

These are acceptance tasks, not completed physical test claims.
