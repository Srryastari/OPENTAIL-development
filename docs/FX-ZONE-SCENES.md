# OpenTail CORE: independent effects and saved scenes (bench-only)

**Status (2026-10-26):** source implemented on draft PR #12; CI checks and real-device acceptance are separate. No vehicle deployment or independent road-light fallback is verified.

## Behavior

- **One effects library, one Show Modes editor.** Bluetooth transports a small 17-byte effect command. It never streams pixel frames; CORE renders and runs the internal particle engine.
- **Each present physical CORE output (2 today) has up to 8 configured 1D zones.** The `fx_zone_bank` holds one effect ID, foreground/background, speed/intensity/brightness/palette, independent time origin, and generation per zone. Selecting one zone does not restart effects or particle simulations in other zones.
- The existing BLE chain/zone masks may update one zone or multiple zones simultaneously. A broad All target selects only configured non-disabled zones. Requests selecting no valid zone return an error.
- A temporary effect expires by its own TTL (1–60 seconds) and is removed on Bluetooth disconnect. Its expiration does **not** stop a different still-running saved zone.
- **Presets tab still uses 8 CORE flash slots.** Saving now captures the *full active multi-zone scene* in one atomic version-2 NVS record. Play restores every saved zone and removes previous live assignments; it is explicitly autonomous across Bluetooth disconnect, not enabled automatically at boot. Stop cancels the whole active scene.
- Version-1 *single-effect* NVS records remain readable on playback. Re-saving upgrades to version 2 without changing Output Manager or Trigger Manager flash namespaces.
- Changing the zone layout is blocked by existing CORE busy logic while a show is active. Loading a scene whose selected zones are now missing/disabled is rejected; it does not partially replace live effects.
- Vehicle park/brake/turn/reverse arbitration still overrides decorative output in the same render task. **There is no verified independent fallback controller in this alpha.**

## Code and evidence

- `main/fx_zone_bank.[ch]`: per-zone slot state, mask/layout validation, wrap-safe temporary expiration, disconnect cleanup
- `main/opentail_core.c`: independent per-zone phase and frame scheduling, BLE dispatch, full-scene save/play
- `main/fx_preset_record.[ch]`: CRC-validated scene format, preservation of version-1 read path
- `tests/test_fx_zone_bank.c`: simultaneous distinct effects, left/right separation, untouched generation, disabled/phantom zones, reconnect/disconnect, time wraparound
- `tests/test_fx_preset_record.c`: multi-zone independent parameters, palette, NVS masks, version/CRC tampering, old single-effect compatibility
- GitHub Actions: `core-host-tests`, `core-espidf-build`, `android-debug-apk`, `web-studio-check`; judge them on the **latest head SHA**.

## Still outstanding

1. Real phone-to-CORE BLE handshake, authorized zone commands, status/readback and repeated reconnect under load.
2. On actual WS2815 chains, validate Output 1 Wipe while Output 2 Embers runs concurrently; check BRG order, zone boundaries, no cross-talk and both physical trigger preemption.
3. Reboot twice to verify NVS scene remains stored but doesn't auto-start; explicitly play it; disconnect and verify autonomous playback; reconnect and Stop.
4. Per-zone UI readback of live effect IDs/settings (currently editor controls the selected target but CORE does not expose the full active zone-bank in BLE).
5. WLED effect/palette parity, persistence of multi-step sequences and optional ACC startup/shutdown behavior, audio-reactive input, secure production pairing and dual-controller fallback.
6. Expansion from 2 tested-in-CI RMT output drivers to a 12-output hardware-validated architecture; power/current injection and thermal protections.

**Full CORE compilation is not proof of electrical safety or production readiness.**
