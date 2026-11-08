# Issue #121 — timestamped turn-transition history

## Scope

This change adds a non-blocking C-only release hold for IN3 / Left (GPIO6,
mask `0x04`) and IN4 / Right (GPIO7, mask `0x08`). The 10ms input sampler
and four stable-sample assertion qualification remain unchanged. Only turn
signal deassertion is extended: an already-qualified turn remains logically
asserted until 400ms of continuous inactive time have elapsed.

## Edge capture and timing

The GPIO ISR records every ISR-delivered Left/Right transition into an
independent 32-entry single-producer/single-consumer ring. Each event contains
the physical active/inactive level and the monotonic `esp_timer_get_time()`
timestamp in microseconds. The ISR publishes its queue head with release
semantics; the sampler reads it with acquire semantics. The ISR uses the
ESP32-S3 low-level GPIO register read (`gpio_ll_get_level`) and an ISR-resident
pin lookup, avoiding a non-IRAM driver call in the IRAM interrupt path.

The sampler replays all queued events in timestamp order before evaluating
either turn deadline. The state machine uses an absolute 64-bit deadline:
`falling_edge_time_us + 400000`. A rising edge strictly before that deadline
bridges the flash gap. A rising edge at or after the deadline first expires
the old assertion and must qualify as a new assertion through the original
four stable sampler samples. A subsequent falling edge starts a new absolute
deadline. This event-by-event replay correctly handles multiple complete
flash pulses between sampler wakeups; a latest-edge snapshot or one-bit latch
cannot represent that history.

The boot GPIO baseline is timestamped before the physical input snapshot.
Queued events at or before that baseline are ignored; later events are replayed.
The ring never overwrites unread events. If a lane's queue overflows, the
sampler flushes the ambiguous history, clears the held assertion, resynchronizes
to the current raw level, and requires four stable samples to assert again.
The overflow is logged. This is an explicit fail-closed recovery path, not
silent extension of a turn assertion.

Brake priority is deliberately not implemented by clearing the turn queue or
resetting its debouncer. Brake overrides are applied by the existing downstream
arbitration; retaining the physical turn history ensures the turn resumes
correctly when Brake releases. Expired events are consumed in order, and
deadline expiry clears the pending release state. No Arduino dependency,
blocking delay, new task, timer callback, NVS field, BLE protocol change,
Effects change or Android change is introduced. The
deadline is evaluated against a 64-bit monotonic microsecond clock, so there
is no 32-bit millisecond rollover arithmetic.

## Reproducible host checks

From `core-firmware/`:

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Imain \
  main/core_logic.c main/arbitration_logic.c \
  tests/test_signal_debouncer.c -o /tmp/ot-signal-debouncer
/tmp/ot-signal-debouncer
python3 tests/test_trigger_runtime_contract.py
bash tests/run-host-tests.sh
```

The focused deterministic test covers four-sample assertion, 100/200/300/399ms
inactive gaps, exact 400ms expiry, reassertion at and after the deadline,
sampler jitter with edge-time replay, multiple complete unseen pulses between
sampler ticks, independent Left/Right, Hazards, all 256 boot masks, unchanged
Brake/Reverse/Park/AUX debounce, arbitration priorities and overflow resync.

## Acceptance evidence

| Check | Status |
|---|---|
| Focused SignalDebouncer host test | CI required |
| Independent Trigger source contract | CI required |
| Full CORE host regression | CI required |
| Exact-head ESP-IDF v5.5.5 build | CI required |
| Independent Codex review of transition queue and boundary semantics | NOT_RUN |
| Powered-board ISR/ring/watchdog timing | NOT_RUN |
| Actual GS400 optocoupler off-gap waveform, both turns and hazards | NOT_RUN |
| Phone / optical / vehicle testing | NOT_RUN |

The queue captures every transition delivered to the GPIO ISR. Like any software
ISR recorder, it cannot promise to count arbitrarily fast electrical edges
that the GPIO interrupt hardware coalesces before ISR service. Automotive turn
signals are low-frequency; the 32-entry queue also provides 16 seconds of
buffering at two edges per second. Physical waveform and overflow behavior
must still be checked on the bench. Do not silently change the approved 400ms
duration if the measured GS400 inactive gap exceeds it.

## Immutable boundaries

Based on Firmware PR #115 exact HEAD
`f97bceec7445a958f735df17ca0842b97bf8e062`. No merge, firmware version bump,
Android/pixel-validation change, BLE, NVS, OTA, Effects renderer or unrelated
physical-safety behavior change is included. Single-ESP architecture and the
approved Android UI remain untouched.
