# OpenTail CORE — physical evidence vs universal-zone regression matrix

**Scope:** Recovery PR #19, bench-only firmware. This document compares earlier
user-observed physical tests against the current *unflashed* source. Earlier
passes are not attributed to this new build. Automated CI is not proof of
physical LEDs, supply behavior, status-LED colors, phone BLE, or redundancy.

## Locked architecture

- **Universal means user-configurable today**, not GS400 defaults. Firmware
  startup on a device with no saved output profile has one inert placeholder
  zone named `Zone 1`, one pixel, role `off`, on each *currently available*
  physical output. No automatic brake, park, turn or reverse assumptions.
- The present board prototype has two RMT outputs on GPIO16/17 and currently
  allocates up to 522 LEDs per output. The planned 12-pixel-output/10-switch
  final hardware is not physically implemented. These are capability limits,
  not default sections or chassis geometry.
- Existing NVS v2 profiles and recognized v1 profiles remain authoritative.
  The firmware does **not** call `ot_om_defaults` over a valid restored
  profile. The v1 migration path remains available; no automatic write-back or
  destructive reset is introduced by the neutral-default change.
- Studio, Android and BLE use the same output/zone role vocabulary and saved
  user-authored configuration. A new zone begins `off` and does not inherit
  a vehicle-specific role. Only an explicit validated Save writes a profile.
- A future independent backup processor must use an authenticated,
  versioned, verified copy of that *same* user profile; hardware failover
  synchronization does not yet exist and is NOT implied by these tests.

## Prior physical observations and current regression coverage

| Previously reported bench observation | Current automated regression | Physical confirmation on new build |
| --- | --- | --- |
| IN1..IN8 via optos on GPIO4..11; single masks 1,2,4,8,16,32,64,128; combined 3,12,14,15,18,31,224,255; release 0 | `test_verified_bench_contract.c`, `test_trigger_manager.c`, `test_core_logic.c` | **Not repeated**. Verify with genuine isolated 12V signals and observed status JSON. |
| GPIO16 Output 1 drove a 522-pixel WS2815 chain; GPIO17 Output 2 drove 30 pixels independently | `test_verified_bench_contract.c`, `test_output_manager.c` verify independently assignable 522/30 profiles and exact zero-based boundaries | **Not repeated**. Existing physical LED proof belongs to older flashed firmware only. |
| Hardware color order corrected to GRB for accurate red/white | Six-order `ot_om_swizzle` tests and explicit GRB wire-byte assertion; saved color order survives BLE roundtrip | **Not repeated**. Visual color and brightness on LED strips unverified. |
| A custom `Front DRL`, 30 pixels, BRG Output 1 persisted across USB restart; subsequent user config changed | BLE roundtrip and Output Manager tests prove data encoding. NVS load/save source still uses `profile_v2` and v1 migration | **Not repeated**. BLE codec roundtrip does not prove flash/NVS persistence. Do not overwrite saved profile. |
| Brake+Hazards mask 14, hazards mask 12; Brake over Park, both turns amber, Reverse on assigned zones | `test_verified_bench_contract.c`, `test_trigger_manager.c`, `test_output_manager.c`, `test_arbitration_logic.c` | **Not repeated**. Validate simultaneous real opto inputs and both physical pixel chains. |
| Rainbow show interrupted by physical Brake and resumed; reverse/hazards assigned-zone override | CORE arbitration + FX zone-bank host tests; post-render overlay C++ tests | **Not repeated**. Prove show phase and untouched zones on connected LEDs. |
| Earlier test of output stop/restore, with reports of 522-strip flicker and Stop problems in some builds | Host tests check logical Stop and per-zone bank behavior; firmware output refresh path builds | **Not repeated**. Stop must extinguish the LEDs and reactivation must restore expected zones reliably. |
| Status LED white/cyan/green/red/rainbow patterns approved, then regressed in v0.1.26 | `test_status_logic.c` validates timing and colors of status patterns | **Unresolved physically**. Restore and observe all six approved patterns on the actual board. |
| BLE enrollment and authorization observed; intermittent disconnects occurred earlier | BLE host/Android integration tests validate command and state contracts | **Not repeated**. Real connection stability, reconnect, saved-zone reads and writes must be checked. |
| Earlier renderer measured about 25 FPS; later 62 FPS requested | CI validates compilation and effect/bridge semantics; it does not measure real FPS | **No new FPS measurement**. Do not claim target performance achieved. |
| Approved Studio v0.19.118 single-screen navigation and zone editing | `web-studio/tests/*.test.cjs`, `android/qa/test_unified_shell.js`, exact stable/release/Android file equality, UI interaction host tests | **On-device UI not fully retested**. Controller-hosted stale page must be deployed separately. |
| Independent hardware failover for WS2815-only light sources | No physical failover test. No redundant processor with synchronized user zones in the current combined image | **Not implemented/verified**. This firmware remains bench-only and must not be relied on for road lighting. |

### Fresh-zone change isolation

The new `ot_om_defaults` applies only when there is no *valid saved output
profile*. Existing valid NVS profiles are restored without a re-layout; the
existing v1 migration path remains intact. No runtime default uses `118`,
`109`, `42`, `253`, `522`, or GS400 left/right assignments as a vehicle
template. Numbers matching earlier physical measurements appear only in
**historical regression fixtures** and hardware-capacity settings.

## Software release gates

1. The entire CORE host suite, BLE output codec, saved bench profile contract,
   Studio UI/interaction suite, Android build, and both pinned WLED-native build
   workflows must complete successfully on the **same final commit**.
2. A valid existing v2 NVS profile must boot without changing its pixels, zone
   count, names, roles, color order, calibration, or 522/30 output independence.
   Unconfigured hardware must expose one *inert* universal zone per output.
3. The approved v0.19.118 Studio source must remain byte-identical across the
   stable web deployment, versioned release and Android asset; no duplicate
   navigation, GS400 loading shortcut, or destructive default reset.
4. Hardware tests on the new build must recheck every input mask, simultaneous
   arbitration, RGB colors, zone indices, Output 1/2, persistent saves,
   status LED, Stop/recovery, BLE reconnect, brightness cap and measured FPS.
5. Fully functional failover additionally requires independent input sensing,
   secondary pixel-data generation, protected switching and profile sync.
   Until then, **no road-use safety claim**.

**Evidence rule:** label checks as `prior physical pass`, `new CI pass`,
`new hardware pass`, or `unverified`. Never promote the first two into
the third without an actual physical observation.
