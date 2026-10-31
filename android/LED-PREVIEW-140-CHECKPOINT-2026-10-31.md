# 140-effect offline preview — validated software checkpoint

This supersedes the five-mode scope of the earlier preview report. The work is
preserved for the owner's usage checkpoint, without merge/release approval.

## Exact source and coverage

- Original required Android baseline: `619298f1a574a502a06d769b9c7e0a02925ee8dd`.
- Existing optimization/report head before this extension:
  `35a4aa4d8ac239c2da395f946b944c08b4f45aaa`.
- Validated 140-effect implementation:
  **`46cfb4491f5f88fe4d5d021665f0ee817e851178`**.
- Authoritative Effects/CORE source:
  `1767b144e193ed6978816899ebb5eba237ee6e2d` (Effects Team #50, on CORE #49).
- Upstream WLED pin: `20e7a717664295eefc6ad77972cee1888d2da072`.
- Final PR head includes a subsequent documentation/evidence-only commit; its
  full SHA is recorded in PR #77's description. Production/test sources match
  the validated implementation above.

**Implemented: 140/140 catalog IDs. Verified against pinned CORE: 140/140 in
the automated corpus. Remaining unimplemented IDs: none.** This does not claim
every upstream WLED parameter permutation or physical/on-phone acceptance.

The JNI renderer reuses 13 byte-preserved source/header files rather than
reimplementing effects: 120 registered WLED source adapters, Embers and 19
original OpenTail effects. SOURCE.json records every copied file's SHA256;
tests compare all bytes with the immutable Effects checkout. All 120 WLED IDs
resolve through source-adapter metadata, not the compatibility-recipe fallback.
No original OpenTail module is relabeled as upstream WLED behavior.

## Integration and Speed correction

- The five-mode Java renderer was removed. CMake/NDK builds the authoritative
  pure renderer and a small local-only lifecycle/JNI bridge for all four ABIs.
  CORE's firmware, BLE, physical output, NVS, authorization and trigger paths
  are not linked into that bridge and were not changed by this extension.
- Each preview owns its native state/framebuffer. Ordinary parameter edits,
  pause/resume and reconnect do not recreate it. Compose disposal frees source
  scratch allocations. Explicit Auto Events Preview can replay a finished
  one-shot; ordinary edits do not silently restart it.
- The prior preview read global preferences and depended on CORE snapshot
  invalidation to refresh settings. The preview now observes preference edits
  directly and reads the selected effect's scoped Speed, Intensity, Brightness,
  foreground/background and Palette. Parameters reach the same native instance
  on its next draw without changing the animation clock or issuing BLE traffic.
  Auto Events uses its own saved profile, rather than Show Modes' settings.
- Tests change Rainbow Speed 0→255 at the same timestamp on the same instance
  and require different RGB. They also require immediate brightness-zero black,
  correct scoped-setting propagation, and stateful oracle equality across edits.
- Fixed palettes use the original source tables/sampler. Palette 1 uses the
  source global harmonic producer. Custom palettes require actual validated
  stops from the project or verified BLE readback. No missing colors, synthetic
  palette gradients or unsupported effect/palette combinations are invented.
- Per owner instruction **“android mic”**, Rocktaves uses AudioRecord PCM16
  (22,050 Hz), a Hann-windowed 1,024-sample FFT and measured peak/magnitude.
  Capture/FFT run on a worker; no microphone work or joins block the BLE/UI
  thread. Only foreground, unpaused local Rocktaves starts capture, subject to
  Android's microphone permission. Stop/disposal/background retires capture.
  PCM remains in memory, never persisted, uploaded or sent over BLE. Permission
  denial, absent hardware, invalid audio or missing custom stops fail closed
  with the existing unavailable-preview label. No audio fixture runs in the app.
- The existing 30-capsule count, gap, width, height, placement, radius, theme,
  editor, controls and six-screen design are retained. Existing status labels
  are reused. No new app controls were added; microphone permission is the
  system interaction required by the owner's selected audio source.

## Executed verification

1. **Real Java/JNI vs independent pinned C build:** 121,380 frames,
   **14,565,600 ARGB bytes, byte-exact PASS**, 1,731 profiles over all 140 IDs.
   Tests check the exact registry/catalog set; parameter extremes, changed
   colors/parameters without resets, brightness-off, every admitted built-in
   palette, harmonic random palette transitions and supplied custom stops.
   Rocktaves' deterministic test input is 440 Hz / magnitude 2048; production
   Rocktaves instead consumes the live microphone. Invalid/missing audio,
   unknown IDs, wrong buffer length, absent custom palettes and repeated close
   are checked. Tests do not generate expected RGB using the Android bridge.
2. **Local Android build:** `:app:testDebugUnitTest :app:assembleDebug` PASS,
   **69 JVM tests, zero failures/errors**. The obsolete five-mode suite was
   replaced by the full native oracle test; microphone spectrum and scoped
   preference propagation have additional JVM tests. Native libraries compiled
   and packaged for arm64-v8a, armeabi-v7a, x86 and x86_64.
3. **Source/native/live-edit QA:** PASS; `check_release.py` 21/21 PASS.
   Pinned CORE #49 / Trigger #48 / Effects #50 source-SHA/ABI checks PASS,
   including all 140 parameter definitions. Existing transport/STOP/CRC/queue
   regressions PASS; capsule geometry and approved editor invariants PASS.
4. A new exact-head CI step runs the full 140-effect oracle and uploads its
   per-effect JSON, then existing ABI/JUnit/APK gates run. CI status for the
   final pushed SHA is tracked in PR #77, separately from these local results.

Environment: Linux x86-64; Temurin JDK 21.0.8+9, Gradle 8.11.1, SDK 35,
build-tools 35.0.0, NDK 27.2.12479018, CMake 3.22.1, host cc. Native renderer
uses -O2. Copied upstream source retains one original trailing-space line for
byte identity; whitespace checks pass for authored changes. No failing local
tests remain at the implementation SHA.

Local debug APK SHA256:
`e5bc0ba3c11488fd42453fc0de58847d285578e71d0dddb24b0a4f1ae366d58b`.
This is a local debug signing result, not a release/CI artifact checksum.

## Measured performance and limits

Raw [per-effect parity/timing evidence](qa/evidence/offline-preview-140-46cfb44.json)
records the exact implementation SHA with a clean tracked Android tree.
[BLE/geometry regression evidence](qa/evidence/led-preview-ble-46cfb44.json)
likewise records that SHA. All effect RGB results are consumed/compared;
native rendering work is not an unused benchmark result.

| Measurement | Result | Scope |
|---|---:|---|
| Largest per-effect mean JNI render time | 4.382 µs | Host rendering of 30 pixels, excludes UI/drawing/audio capture/BLE |
| Largest per-effect p99 JNI render time | 6.510 µs | Same host corpus; not a phone frame-duration statistic |
| Inverse mean render-cost range | 228,206–4,950,495 frames/s | Derived renderer-only throughput, **not displayed FPS** |
| Simulated FX dispatch during 522-pixel capture | 220 → 10 ms | Same production queue/capture code, fake 10 ms ATT operations |
| Pending watchdogs after 522-pixel capture | 11 → 0 | Deterministic transport host test |
| Actual Android displayed FPS / missed deadlines / stutter | **Not measured** | Physical phone or specified emulator trace still required |
| Actual microphone latency/gain and BLE airtime/STOP latency | **Not measured** | Device/CORE measurement still required |

The display-clock engine targets one local update per display frame. These host
numbers do not establish 60 FPS on Android. The extended assignment's physical
FPS/latency/visual goals remain open; no hardware telemetry or measurements are
fabricated to fill that gap.

## Source semantics and remaining dependencies

- Full **CORE** rendering coverage is established for the tested input corpus.
  Full **upstream WLED** parity for arbitrary settings is not established. Extra
  upstream custom/color-slot/switch parameters absent from CORE's config remain
  at source defaults; the bridge does not invent new firmware parameters.
- Local preview is one logical 30-pixel segment with a deterministic source
  RNG seed. It does not reproduce installed geometry, the physical device's
  random seed, OEM/trigger arbitration, gamma, RGB gain or bus calibration.
- A Speed control is not universally motion rate. Fireworks' pinned body does
  not read Speed; Fill Noise/Wavesins can hide Speed differences with direct
  default colors; Noise Pal changes targets after >4–6.55 s; TV Simulator's
  scenes last tens of seconds or longer; Slow Transition requires a target
  change and can take minutes. Short unchanged default traces are preserved,
  not altered using arbitrary speed scaling. Per-effect evidence reports the
  short-trace difference flag without treating it as universal Speed semantics.
- Effects Team coordination and these conditional-Speed findings were posted
  to [Effects #50](https://github.com/Srryastari/OPENTAIL-development/pull/50#issuecomment-6543411346).
  **No new independent Team approval is claimed.** Their confirmation/review
  remains a dependency, rather than permission to change their firmware source.
- Real microphone permission/denial, device capture availability, background
  stop/resume, spectral calibration and Rocktaves optical behavior need on-phone
  acceptance. The FFT sine/silence test passes; microphone hardware was not run.
- All 140 effects still need physical/display acceptance where required. No
  phone/CORE was attached. PR #76's 125-test evidence, owner visual comparison,
  physical STOP/auth/reconnect, output/trigger safety, firmware/flash and release
  gates remain open. PR #76 remains unchanged at the requested starting SHA.

## Resume without repeating completed work

Use existing branch `android/led-preview-engine-optimization-20261031` / PR #77.
Do not start a new implementation or replace the preserved source with sketches.

```sh
# Standard local/CI environment: JAVA_HOME, Android SDK/NDK, cc, Python, Node.
python3 android/qa/test_full_offline_preview.py --core-dir /path/to/exact-effects50-checkout
python3 android/qa/benchmark_preview.py
node android/qa/test_native_source.cjs
node android/qa/test_live_edit.cjs
python3 android/qa/check_release.py
gradle -p android --no-daemon :app:testDebugUnitTest :app:assembleDebug
```

The Effects checkout must be exactly the immutable source pin above. CI already
checks it out as `pinned-effects50`. The per-effect test and vendored SOURCE.json
guard against source drift. Next work is independent review and measured device
acceptance, not another 140-effect port. Re-run expensive gates only after new
changes or failures justify them. No merge, release or production approval.
