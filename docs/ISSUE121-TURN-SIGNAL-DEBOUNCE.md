# Issue #121 — turn-signal release-hold debouncer

## Scope

This change adds a non-blocking, C-only `ot_signal_debouncer_t` for IN3 / Left
(GPIO6, mask `0x04`) and IN4 / Right (GPIO7, mask `0x08`). The 10ms input
sampler and four stable-sample assertion qualification remain unchanged.
Only turn-signal deassertion is extended: the logical input remains asserted
until 400ms of continuous inactive samples have elapsed.

The first observed inactive raw sample starts the timer. A raw reassertion
observed before the 400ms deadline cancels the pending release and leaves the
logical mask asserted, so the renderer receives no false falling/rising edge
between flashes. At the deadline or later, continuous inactivity releases the
old assertion; a subsequent assertion must again pass four stable samples.

Timing uses `esp_timer_get_time()/1000LL`, truncated to `uint32_t` milliseconds.
The existing GPIO edge ISR records the Left/Right transition timestamp so a
late 10ms sampler wake can distinguish a reassertion just before 400ms from
one after expiry. The ISR timestamp is read atomically by the sampler.
Unsigned elapsed subtraction is wrap-safe for the short 400ms interval. No
Arduino dependency, blocking delay, task, timer callback, NVS field or BLE
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

The deterministic test uses an injected `uint32_t` clock and covers four
sample assertion, 100/200/300/399ms flash gaps, 400ms and longer inactivity,
reassertion before and at expiry, repeated flashes without a logical falling
edge, independent Left/Right and Hazards, all 256 boot masks, unchanged
non-turn timing, Brake/Parking and Reverse arbitration, wraparound and
scheduler jitter. The complete CORE host runner also covers the existing
Effects, BLE, STOP, NVS and watchdog suites.

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
