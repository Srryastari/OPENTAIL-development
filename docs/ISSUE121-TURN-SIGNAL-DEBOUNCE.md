# Issue #121 — turn-signal release-hold debouncer

## Scope

This change adds a non-blocking, C-only `ot_signal_debouncer_t` for IN3 / Left
(GPIO6, mask `0x04`) and IN4 / Right (GPIO7, mask `0x08`). The 10ms input
sampler and four stable-sample assertion qualification remain unchanged.
Only turn-signal deassertion is extended: the logical input remains asserted
until 400ms of continuous inactive time have elapsed.

The GPIO ISR timestamps each Left/Right edge, retains the previous edge time,
and publishes both with an odd/even sequence counter. The sampler takes a
bounded seqlock snapshot of the raw GPIO level, previous/current edge
timestamps and sequence, and double-reads the pin level around that snapshot;
a torn or changing snapshot is discarded and does not feed watchdog progress.
A level/sequence mismatch also withholds progress until the corresponding ISR
edge record arrives; a persistent mismatch is left to the existing sampler
watchdog rather than publishing an unverified input state. This prevents a GPIO edge racing the
timestamp read from creating an impossible future edge time. If the level changes before the ISR has published its timestamp, the
debouncer defers either assertion or release classification until the
coherent ISR edge record arrives rather than classifying a stale timestamp.

The first inactive raw sample starts the timer from its captured falling-edge
timestamp. If ISR publication is delayed, the state machine waits for the
matching edge sequence before starting or cancelling the timer. A reassertion observed by the sampler uses its captured rising-edge
timestamp, so scheduler delay cannot turn a 399ms inactive gap into a false
release. If a complete reassert-and-drop pulse occurs between sampler wakes,
the penultimate edge timestamp identifies the last reassertion. A pulse that
began before the 400ms deadline restarts the timer at its newer falling edge;
a pulse that began at or after the deadline cannot resurrect the expired
assertion and must pass the normal four-sample assertion qualification.
Reassertion before the deadline leaves the logical mask asserted, so the
renderer receives no false falling/rising edge between flashes. Left and Right have independent timer state, so one
side can expire while the other continues and hazards resolve only while both
logical turn inputs are active. If the signal remains inactive through the
deadline, the old assertion releases; a later assertion must again pass four
stable samples.

Timing uses the low 32 bits of `esp_timer_get_time()` as monotonic microseconds.
The 400,000µs interval is safely shorter than the 32-bit wrap period (about
71.6 minutes). The GPIO ISR stores previous/current edge timestamps atomically; the sampler
compares them using unsigned elapsed arithmetic. The ESP-IDF 5.5.5
`esp_timer_get_time()` implementation is ISR-safe and IRAM-backed. No Arduino
dependency, blocking delay, extra task, timer callback, NVS field or BLE
change is introduced. The transient timer state is RAM-only and is seeded
from the initial GPIO snapshot before the first RMT frame.

Brake, Reverse, Parking and AUX stay on the existing four-sample debounce.
The existing arbitration and watchdog sampling publication remain in place.
The sampler reports progress only after all eight real GPIO reads and the
updated physical mask have been published.

## Reproducible host checks

From `core-firmware/`:

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -I main \
  main/core_logic.c main/arbitration_logic.c \
  tests/test_signal_debouncer.c -o /tmp/ot-signal-debouncer-tests
/tmp/ot-signal-debouncer-tests
python3 tests/test_trigger_runtime_contract.py
bash tests/run-host-tests.sh
```

The deterministic test uses an injected `uint32_t` microsecond clock and covers four
sample assertion, 100/200/300/399ms flash gaps, 400ms and longer inactivity,
reassertion before and at expiry, repeated flashes without a logical falling
edge, independent Left/Right and Hazards, one-side expiry while the other
continues, all 256 boot masks, unchanged non-turn timing, Brake/Parking and
Reverse arbitration, 32-bit microsecond
rollover, delayed sampler wake, torn-snapshot rejection and an unseen pulse
between sampler wakes using captured raw-edge sequence. The
complete CORE host runner also covers the existing Effects, BLE, STOP, NVS
and watchdog suites.

## Acceptance evidence

| Check | Status |
|---|---|
| Focused deterministic SignalDebouncer host test | CI required |
| Full CORE host regression | CI required |
| Independent Trigger source/safety contract | CI required |
| Exact-head ESP-IDF v5.5.5 build | CI required |
| Independent code review | NOT_RUN |
| Powered-board GPIO timing / sampler watchdog | NOT_RUN |
| Actual GS400 optocoupler off-gap waveform, both turns and hazards | NOT_RUN |
| Phone testing | NOT_RUN |
| Vehicle / optical / road testing | NOT_RUN |

A 400ms hold only bridges inactive gaps up to 400ms. The actual GS400
optocoupler waveform must be measured with suitable isolated instrumentation
before deciding whether the owner-selected duration is adequate. Do not
silently increase it. Host simulation is not physical waveform evidence.

## Immutable boundaries

Based on Firmware PR #115 exact HEAD
`f97bceec7445a958f735df17ca0842b97bf8e062`. No merge, firmware version bump,
Android/pixel-validation change, BLE, NVS, OTA, Effects renderer or unrelated
physical-safety behavior change is included. Single-ESP architecture and the
approved Android UI remain untouched.
