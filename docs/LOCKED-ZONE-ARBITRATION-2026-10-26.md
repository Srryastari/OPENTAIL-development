# OpenTail zone-local arbitration — CORE Alpha implementation, October 26 2026

**Source changed in PR #12; automated CI and physical acceptance are separate.**
The firmware still has only two physical RMT outputs and no independent watchdog/failover.

## Locked owner-approved policy

1. Parking is the idle layer, not a blanket override. With IN1 active, a selected
   CORE effect continues on its chosen output/zone; zones configured as parking
   without a Show Mode continue displaying their parking appearance.
2. Brake, left/right turn, hazards, reverse and AUX override **only the Output
   Manager zones assigned to their logical roles**. Other zones continue their
   effects and keep independent phase/origin even while an overlay is visible.
3. Releasing an input reveals the underlying effect at its current position
   without restarting the FX; the FX bank is never cleared for a trigger.
4. The `park_brake` role keeps deterministic Brake > Parking precedence.
   Hazards mean left and right turn inputs simultaneously, each restricted to
   its own assigned zones. Reverse affects only reverse-assigned zones.
5. Manually stopping Show Modes clears temporary or flash-loaded scene playback
   but **does not** disable physical vehicle triggers. Saved scenes persist in
   NVS without auto-start at power-up; standalone scenes continue through BLE
   disconnect, temporary tests still expire/stop.
6. CORE role assignment is currently single-role per zone except park_brake.
   Editable arbitrary overlapping trigger masks and per-zone conflict priorities
   remain a separate feature, not a completed claim.

## Automated checks / hardware gates

Host tests cover Parking + Wipe coexistence, Brake > Park, left+right/hazards,
Reverse, AUX, disabled/decorative zones, and no global show suppression.
CORE render loop uses configured role ownership per pixel. Readback of
`trigger_active` and `effect_selected` now reflects the selected zone.

Remaining hardware acceptance: hold IN1 and start Wipe, assert/release IN2,
IN3, IN4, IN3+IN4, IN5 and IN2+IN3+IN4, ensure Wipe's original phase resumes
while other zones never pause; verify this on actual level-shifted WS2815 BRG
pixel chains, not just the onboard RGB status LED. Verify independent saved
scenes / BLE reconnect / Manual Stop / NVS persistence and watchdog fallback
separately before any vehicle deployment.

**No device flashing or physical road-safety/failover test is claimed.**
