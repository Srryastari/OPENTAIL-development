# Native Android v0.2.0-alpha1 — on-device acceptance checklist

- [ ] Before upgrading, export a project backup from the old Android app.
- [ ] Install v0.2.0-alpha1 over previous app; ensure it opens immediately
      without a browser chrome, Wi-Fi prompt or second navigation bar.
- [ ] Confirm exactly six top pills in approved order, with no bottom tabs.
- [ ] Toggle OLED black ↔ white; check all cards, captions, pill buttons,
      text fields and sliders remain legible on a narrow phone.
- [ ] Show Modes: choose several effects (including original Materialize),
      change foreground/background/accent and sliders. **Offline** design edits
      must not operate physical LEDs; **after owner BLE authorization**, only
      supported FX edits may update an existing CORE zone using a temporary
      five-second TTL and 24/255 brightness limit. Unrecognized FX must fail closed.
- [ ] Layout: add two arbitrary zones, resize the first, verify the
      second start index recalculates automatically from zero. Insert
      another zone between them; confirm order and trigger roles persist.
- [ ] Triggers: select two triggers simultaneously; assign zones and
      effect; confirm edits stay after app restart.
- [ ] Auto Events: configure optional Startup and Shutdown independently;
      confirm local preview and duration/fade persistence.
- [ ] Sequences: add/edit/duplicate/reorder/remove, repeat/shuffle.
      Presets: save/load/remove locally; ensure no BLE calls for sequences or presets.
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
- [ ] On a CORE firmware build with the new optional snapshot GATT UUIDs,
      authorize BLE, run a supported effect, then Show Modes → choose Output 1
      → Capture actual CORE frame. Verify it displays CORE origin, output index,
      frame sequence and actual RGB888 pixels across the full zero-based
      inspection range including the last pixel. Repeat on Output 2.
- [ ] With physical Brake/Hazard/Reverse test inputs active, capture another
      frame and compare the affected logical zone pixels against the actual
      controller arbitration. Confirm color order is not incorrectly swapped
      by Android. Compare independent outputs; note snapshots are NOT synchronized.
- [ ] Disconnect BLE mid-snapshot and verify no partial frame is displayed.
      Retry after reconnect/owner auth. Press Stop CORE show during a long
      capture and verify STOP takes priority over remaining snapshot chunks.
      A firmware without the new UUIDs must show 'capture unavailable'.
- [ ] Keep the app offline and background it during capture; confirm the
      diagnostic transfer is canceled, stale CORE origin disappears and
      temporary FX is stopped/expired. No NVS output writes on any preview.
- [ ] Disable Bluetooth; confirm no stale authorized status or unintended
      operations. Re-enable and reconnect explicitly.

Rendered-frame RGB equality is not a 53–120 FPS animation claim; inspect
actual LED output with measured timing/optical calibration separately.

These are acceptance tasks, not completed physical test claims.
