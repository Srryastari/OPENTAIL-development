# Native LED preview optimization — source and host evidence

Status: scoped software optimization, physical acceptance pending. No merge or
auto-merge. PR #76 and its existing phone/release gates remain protected.

## Immutable scope

- Requested baseline: `619298f1a574a502a06d769b9c7e0a02925ee8dd` (PR #76).
- Tested implementation: `5a97d13d7ec78a6419b18c8f8f205702f80afb0a`.
- This report/evidence is a subsequent documentation-only commit. The PR body
  identifies its exact final head; production/test sources match the implementation
  above. The benchmark records the implementation SHA and a clean tracked Android tree.
- CORE protocol: `adcdff0b2f43061bc3e222d1d77de0a34b095c0a` (#49).
- Trigger gate: `e44a98fcaa51ecbf5e9c15bf85b8d129adb4e1a9` (#48).
- Effects oracle: `1767b144e193ed6978816899ebb5eba237ee6e2d` (#50).
- Upstream WLED: `20e7a717664295eefc6ad77972cee1888d2da072`.

No firmware, effect catalog, approved Effects editor, other screen, physical
output geometry, BLE opcode, authorization rule, TTL, brightness safety cap,
NVS rule, or STOP acknowledgement contract is changed.

## Diagnosis and changes

**Local animation.** The baseline increments a synthetic tick every 33 ms
(at most 30.3 updates/s before scheduling overhead), independent of display
timestamps. Two SharedPreferences lookups occur per capsule per draw, and
the animation reads a delegated tick inside a captured draw lambda. The
baseline already attempts draw-phase observation; no measured baseline
per-frame recomposition count is claimed. Time drifts with delayed callbacks.
The five sketches ignore speed, intensity and brightness; Rainbow uses Android
HSV and Fade uses a sine approximation rather than pinned CORE arithmetic.

The candidate uses Compose `withFrameNanos`, accumulates elapsed timestamps
without replaying missed ticks, and observes changing time only in Canvas draw.
Settings are read once per edit, with one reusable 30-color buffer and no
per-capsule preference lookup. Solid does not schedule animation ticks. Effect
selection resets the local renderer; parameter changes, ordinary reconnects,
pause/resume and background/foreground transitions retain its elapsed time
while this composable remains mounted. Time freezes while inactive. Navigation
that disposes the preview, process death, and CORE's own effect restart semantics
are outside this continuity guarantee. No 60 FPS device result is claimed.

**Offline fidelity.** Only the existing five modes (Solid/Blink/Wipe/Rainbow/Fade)
are adapted, directly from pinned `wled_fx_compat.c`. Integer wheel/triangle,
blend rounding, brightness, Wipe interpolation, speed/intensity and Blink's
sparse-frame rule replace guessed behavior. Palette 0 only: unsupported
effects/palettes remain unavailable using the existing label. Blink uses a
nominal local FRAMETIME of 16 ms; CORE supplies its own runtime interval.
Differential results prove the tested raw-RGB subset at that interval, not
all WLED behavior or optical equivalence. No gamma, global/bus brightness,
custom switches, audio, installed-zone rendering, or 140-effect parity claim.
WLED attribution and EUPL notices are included in source and APK assets.

**Connected preview.** The baseline requests a complete frozen RGB888 snapshot,
then reads 56-pixel chunks using serial command-write/response-read pairs.
Sequence, offset, output, length and whole-frame CRC validation remain intact.
No sample-only transport, invented frames, interpolation, or protocol extension
is introduced. Local paint and configured-role fallbacks retain their existing
static-authoring labels, including the owner's existing black-frame fallback.

Admission now follows the display clock with a stable capability boolean,
instead of restarting its coroutine whenever BLE status wording changes.
Immutable frame/project color-presence scans are cached. Completed snapshot
RGB storage transfers ownership instead of being copied once more.
The command count and frame bytes are unchanged; larger outputs remain
BLE-throughput bound. Updating a parameter still invalidates an old snapshot.
Capture remains opportunistic and may be canceled repeatedly during active
editing; no incomplete capture is displayed.

**BLE priority and timers.** Baseline captures yield for STOP and output reload,
but can delay FX until the full transfer ends or reject other user commands as
busy. Candidate FX yields after the current ATT callback. One bounded deferred
action covers auth/pairing, trigger read/save, output save, palette operations,
presets and capability reads; execution revalidates the same GATT session.
STOP retains existing priority and successor/failed-STOP rules. Preset PLAY
uses that STOP-aware queue, so a later STOP can supersede queued PLAY.
Deferred actions are discarded on session retirement, never replayed on reconnect.
A second action cannot silently replace the pending one. Existing unrelated
transfers may still report busy; preemption does not interrupt an active ATT request.

Baseline watchdog lambdas remain queued for eight seconds after successful
operations. The candidate removes completed/canceled timers and bounds active
watchdogs to one; each read receives its own timeout. Frame read/write callbacks
advance capture on the UI looper with admission and STOP. A missing callback
revokes connection/control admission immediately and retires GATT, rather than
starting another transaction on an uncertain ATT channel. Physical STOP delay
still depends on Android/radio callbacks; the eight-second timeout is recovery,
not an instant-STOP guarantee. Trigger arbitration and safety remain in CORE.

## Before/after measurements and limits

Environment: Linux cloud x86-64, Temurin JDK 21.0.8+9, Gradle 8.11.1,
Android SDK 35/build-tools 35.0.0, gcc host C oracle. No attached phone or CORE.
Raw evidence: [led-preview-5a97d13.json](qa/evidence/led-preview-5a97d13.json).

| Metric | Baseline | Candidate | Evidence type |
|---|---:|---:|---|
| Pending watchdogs after 522-pixel capture | 11 | 0 | Executed production capture, fake Handler/ATT |
| Peak watchdogs during 6,000-pixel capture | 109 | 1 | Same host harness; capacity is not a device allocation claim |
| FX dispatch during 522-pixel capture | 220 ms | 10 ms | Production enqueue methods + capture, simulated 10 ms/ATT operation |
| ATT operations for 522 pixels | 22 | 22 | Host protocol execution |
| ATT operations for 6,000 pixels | 218 | 218 | Host protocol execution |
| RGB bytes / redundant completion copy, 522 pixels | 1,566 / 1,566 | 1,566 / 0 | Owned buffer/code path; no radio byte reduction |
| Candidate 30-pixel Rainbow render p50 / p95 / p99 | Not measured | 237 / 259 / 319 ns | 10,000 warmed host JVM calls; excludes Android drawing/GC/UI |
| Actual display FPS / missed deadlines / stutter | Not measured | Not measured | Requires the same physical phone and workload |
| Actual BLE capture age / STOP latency / radio airtime | Not measured | Not measured | Requires installed exact firmware and phone |

The simulated 220 ms transfer is still 220 ms in both versions; the optimization
is bounded queue/timer overhead and earlier command service, not a claim of
faster physical snapshot delivery. A 60 Hz display offers 60 local draw
opportunities/s with the new clock; that is a scheduling target, not measured FPS.
No before/after device stutter, latency or BLE-impact results are fabricated.

## Verification actually run

- `node android/qa/test_native_source.cjs`: PASS.
- `node android/qa/test_live_edit.cjs`: PASS.
- `python3 android/qa/check_release.py`: 21/21 static checks PASS.
- `python3 android/qa/check_pinned_core49_contract.py`, against the three exact
  detached source worktrees: PASS, including all 140 parameter definitions.
- `JAVA_HOME=... python3 android/qa/benchmark_preview.py`: PASS at the exact
  implementation SHA, tracked Android source clean. 7,425 C-vs-Java frames,
  2,138,400 RGB bytes, byte-exact. Tests cover supported mode/parameter extremes,
  1/30/257 pixels and 32-bit time wrap. This independently executes pinned C;
  expected results are not generated by the Java port.
- Transport host regressions PASS: full/chunk boundaries, 65,535 representation
  boundary (not device capacity), CRC corruption, wrong sequence/offset,
  truncation, failed start, write/read timeout, abort during read/write/poll,
  old session and buffer ownership. Queue tests execute extracted production
  admission/enqueue/pump methods with fake ATT, including STOP/FX/deferred/read
  ordering and session retirement. They are not Android Bluetooth stack tests.
- Source UI invariants PASS: other Compose UI and approved editor byte-unchanged,
  capsule count/gap/width/height/position/radius preserved. Colors from actual
  CORE/project/configured mappings retain the same paths. No screenshot or
  pixel-level on-phone acceptance is claimed.
- `gradle --no-daemon -Pkotlin.compiler.execution.strategy=in-process
  :app:testDebugUnitTest :app:assembleDebug`: PASS, 71 tests / 0 failures / 0 errors.
  Final local build completed in 43 s after incremental verification. Existing
  deprecation/native-symbol warnings remain. Intermediate tool provisioning and
  Kotlin-daemon failures were resolved locally; a Java self-reference introduced
  during timeout hardening was corrected before final successful verification.
- Local exact-implementation debug APK SHA256:
  `085d9f2a47176ae8b88103d0c619964776ae91ad4dc5062552236744cd6b24f2`.
  Locally signed debug artifact; not a release signing or CI-artifact claim.

## Physical gates / remaining measurements

Keep PR #76's 125-test evidence and all owner/independent release gates open.
Use baseline and candidate APKs on the same phone, fixed refresh rate, same
installed exact CORE firmware and 30/522/large supported pixel configurations.
Do not assume the repository firmware is installed.

1. Record phone model/OS/refresh, APK SHA, CORE SHA, connection interval/MTU,
   pixel count, selected effect, parameters, brightness and zone readback.
2. For local Solid/Blink/Wipe/Rainbow/Fade, capture 30-second FrameTimeline or
   equivalent traces with identical drag/pause/background workloads. Record
   presented FPS, p50/p95/p99 frame duration, missed deadlines and UI-thread
   composition/layout/draw cost; separate idle Solid from animation.
3. For connected preview, measure capture request/completion/presentation age,
   completed/canceled/CRC-failed frames, ATT counts, BLE airtime and command
   acknowledgement latency. Exercise continuous parameter edits, STOP during
   header/chunk/poll, output reload/save, trigger mapping, auth and reconnect.
   Verify no late command or retired frame appears on the next controller.
4. Compare approved screenshots/30-capsule dimensions and zone/color/brightness
   behavior. Retest affected offline/editor, STOP/reconnect/auth/output cases
   from the 125-test suite. Independently verify physical trigger precedence,
   watchdog behavior and optical output at the installed firmware SHA.

Rollback: revert implementation commit `5a97d13d7ec78a6419b18c8f8f205702f80afb0a`.
No persistent project migration or CORE flash/configuration change is required.
