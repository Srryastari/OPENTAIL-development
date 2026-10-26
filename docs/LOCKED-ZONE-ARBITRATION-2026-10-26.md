# OpenTail locked per-zone lighting arbitration — 2026-10-26

**Status:** approved requirement, NOT implemented or hardware validated by this UI-only commit. No firmware/flash changes. Do not rely on alpha as sole vehicle lighting.

## Locked behavior

- Holding parking (IN1) is normal while driving. It must not globally block a user-selected Show Mode or full-section custom effect. It supplies idle/parking lighting only when no higher applicable layer is selected for that zone.
- Brake (IN2), Left Turn (IN3), Right Turn (IN4), Hazards (IN3+IN4), and Reverse (IN5) override **only the zones assigned to them**. Zones not assigned to an active trigger continue their independent effect without a restart or pause.
- When a trigger releases, its zone reveals the previously running effect or applicable parking appearance; an unrelated output/zone never resets. Saved autonomous scenes continue over BLE disconnect; temporary effects still expire on their own TTL/disconnect.
- For simultaneous inputs, compose zone overlays independently. Overlap on one zone must have deterministic, documented priority and physical test evidence. Do not change all outputs globally because of one input.

## Firmware acceptance checks (still pending)

1. Start Wipe with Parking continuously asserted: Wipe keeps playing on its selected zones.
2. Apply Brake while Wipe and another independent output effect are running: only Brake's assigned zones change; release restores underlying phase.
3. Repeat IN3, IN4, IN3+IN4, IN5 and IN2+IN3+IN4; verify zone-local preemption and restoration.
4. Verify same on real WS2815 strips at the physical outputs, including BRG and level-shifted data, not just onboard RGB status LED.
5. Verify saved-scene NVS, BLE disconnect, controller reconnect, and no cross-zone resets.

This supersedes older **global** parking/show arbitration as a requirement, but not as a claim that firmware has been updated or proven safe.
