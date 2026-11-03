# Issue 113: single-controller recovery candidate

Owner baseline: PR #112 commit `1a300767d985343d060faf344bff2da772dca88f`.
Target: **one ESP32-S3 N16R8, ESP-IDF v5.5.5**. No version bump.
This candidate is bench-only. It provides software recovery, not redundant
failover. **Road-going use remains BLOCKED.** No automatic merge or flashing.

## Watchdog audit and implementation

The baseline had IDF default watchdog settings but no application progress
subscriptions. A blocked `led_strip_refresh_async_done()` could leave other
tasks and both idle tasks running indefinitely. OTA confirmation retried on
every complete frame with no boot confirmation deadline. NVS initialization
failure panicked before diagnostics. The baseline already guarded stage→active
promotion with physical-input quiet time/edge epochs and CRC journals; that
logic is unchanged.

The candidate uses the **single IDF Task WDT** through the supported
`esp_task_wdt_init/reconfigure/add_user/reset_user/delete_user` APIs. Its 10s
timeout panics/resets. Both idle tasks remain subscribed. The interrupt WDT
(300ms configuration), bootloader RTC WDT (9s), normal IDF panic handling and
brownout detector remain enabled. IDF disables the boot RTC WDT before calling
the application; the startup TWDT user covers application initialization.
Neither watchdog is disabled to tolerate a stall. The existing 12KiB main
stack guard remains. These numbers are software settings, not measured recovery
latencies, and may require board evidence before approval.

| User | Armed | Genuine progress credited | Removed |
| --- | --- | --- | --- |
| startup | before NVS/status/GPIO/Wi-Fi/BLE initialization | no periodic feed; initialization must finish within the bound | after service/task startup decisions |
| inputs | before task creation | all eight pins sampled, debounce and quiet-window state published | only if task creation fails |
| config | before promoter creation | promotion gate examined and any selected journal operation returned | only if task creation fails |
| render/output | before task creation | one iteration finished after every attempted resize/pixel write/submission/completion returned | only if task creation fails |

Arm-before-create catches a task that never gets scheduled. A created renderer
that exits during its own initialization leaves its subscription armed and
recovers through TWDT. No timer/idle callback feeds an application user. A
sampler cannot feed the renderer. Render iterations continue for low load,
STOP, disabled shows and BLE disconnects. The renderer gives IDLE a real tick
at least once per second under sustained load; `taskYIELD` alone cannot run a
lower-priority idle task. Existing adaptive pacing otherwise remains intact.
The timeout is above a complete 8192-pixel RGB800 transfer (~246ms per lane),
but maximum-budget effects and flash operations still need powered-board tests.

Output completion credit remains after both lane waits. A blocked submission,
completion, resize or effect invocation cannot feed the render user. A returned
driver error is **isolated**, latched and exposed through the existing failed
output mask; the surviving lane continues and no retry/reset loop is introduced.
That is a terminal software lane fault, not successful output progress. It
cannot clear the failed-boot streak or confirm OTA. Software driver success
does not prove emitted pixels or electrical continuity.

## Recovery and diagnostics

RTC no-init memory stores a bounded abnormal-reset streak with a validation
seal and the renderer's last boundary with its inverse. There are **zero new
NVS writes** for watchdog diagnostics. A reset during a record update can
invalidate it; invalid/lost RTC data starts a new streak rather than inventing
history. Power loss, some brownouts and external resets can lose RTC state.
This is intentionally not a flash-persistent crash log or a guarantee against
unstable supply boot loops. Reset cause is the IDF hardware/software reason;
the last boundary is context, **not proof of the failure's cause**.

Three abnormal resets without sustained healthy operation enter a diagnostic-only
boot: no external RMT initialization/rendering, no configuration promoter or
durable configuration admission. GPIO sampling and existing Wi-Fi/BLE/HTTP
diagnostics may run. This does not send black to already latched pixels. Repair
the cause, then power cycle to retry; software/OTA restarts alone do not clear
the streak. Sixty continuous seconds of complete frames, recent input samples
and confirmed boot clear a non-quarantined streak once in RTC memory.

Boot reads only existing committed/journaled configuration. No erase-on-NVS-error
path is added. Corrupt NVS/journals fail closed with diagnostics and external
outputs uninitialized. Four real input samples prime held-trigger state before
external RMT setup; journal activation still requires the original independent
quiet-window/epoch checks. A restart never starts a decorative show or promotes
a pending edit through a held physical input.

`GET /api/v1/status` adds a `recovery` object; all prior fields and BLE bytes are
preserved. It contains `enabled`, `timeout_ms`, `reset`, IDF `raw_reset`,
`failed_boots`, `previous_phase`, `quarantined`, and `storage: "rtc_only"`.
Phase IDs: 0 boot, 1 render, 2 resize, 3/4 O1/O2 submission, 5/6 O1/O2 completion,
7 finished iteration, 8 OTA confirmation timeout; `4294967295` means unknown.
Reset names distinguish power-on, software, task/interrupt/other WDT, panic,
brownout, OTA confirmation failure and other. A real loss of power cannot be
distinguished from a deliberate power cycle using this controller alone.
Android's six approved screens and all Android files remain byte-identical.

OTA gets one 30s boot timer only for a pending candidate or unreadable OTA state.
It cannot confirm before a complete frame, initialized services and BLE radio
readiness. Attempts are limited to once per second. The expiry and flash
confirmation claim a shared atomic state, so a successful flash confirmation
cannot race into rollback. If validation is in progress at expiry, the guard
checks again one second later; a stuck renderer/flash call remains covered by
TWDT. Failed startup without a renderer is also covered by the OTA boot timer.
Expiry requests IDF rollback, then restarts if no usable rollback slot exists.
Diagnostic quarantine does not arm another OTA timer. If rollback selects an
older image, that image's software behavior applies; it may predate this policy.

## Failure classification and acceptance boundary

| Failure | Software response | Evidence still required |
| --- | --- | --- |
| task freeze/deadlock | unfed named user → TWDT panic/reset | serial user/task trace, reset cause, timed recovery |
| scheduler starvation | IDF idle TWDT; render user also stops | real scheduler stimulus/timing |
| interrupts blocked | IDF IWDT; secondary reset path remains IDF-owned | real blocked-interrupt test |
| output/RMT hang | render user stops at retained submission/completion phase | actual driver/hardware hang and lane observations |
| returned output error | latch lane fault; continue good lane; do not confirm OTA | independent O1/O2 and STOP observation |
| panic | normal IDF reset; RTC streak on next boot | crash trace, held inputs and reboot behavior |
| brownout | IDF brownout reset; retained streak if RTC survives | scoped supply stimulus and rail measurements |
| power loss/dead chip/dead data path | internal watchdog unavailable | independently powered essential-light path |
| corrupt NVS/journal | no erase/promotion, external outputs withheld, diagnostics | interrupted writes/corrupt storage on board |
| repeated resets | diagnostic quarantine after retained streak reaches three | repeat reset timeline; no optical-black claim |
| OTA confirmation failure | bounded explicit rollback/restart | real pending-slot validation and rollback |

The hardware fallback requirement is **independent brake, turn and reverse
illumination through retained OEM circuits or a separately verified essential-light
electrical path**, with no dependency on ESP firmware, its pixel-data output or
its failed supply. No such path is demonstrated by this software PR. Hardware
Team must define/verify schematics, supply domains, contention/isolation and
automotive transient behavior. A low-cost reset supervisor IC may monitor the
rail/EN line or a genuine-progress heartbeat if board evidence shows internal
recovery insufficient. Hardware owns part selection, pulse/voltage thresholds,
reset-rail design and validation. No new heartbeat pin is allocated here. A
reset supervisor is not an LED-data failover and cannot repair a dead ESP or
latched pixels. A second ESP32 is excluded.

Historical PR #84 / issue #52 / hardware PR #32 files, inert
`ot_failover_essential` model and their release safeguards remain preserved.
They are superseded design proposals for this implementation, not an active
takeover path. Their isolation and all **22 physical acceptance cases** stay
enforced, with their evidence unchanged.

## Reproducible bench procedure — all physical results NOT_RUN

Use a current-limited powered board, serial capture with timestamps, logic
analyzer on GPIO16/17, optical observations, isolated active-low inputs on
GPIO4–11 and an independently functioning essential-light path. Record exact
source/ELF identity, IDF version, sdkconfig, firmware ZIP checksum, supply rail,
input levels, STOP generations, output refresh counters, reset cause and boot
logs. Never install an injection image on a vehicle.

Production default `CONFIG_OPENTAIL_WATCHDOG_TEST_FAULT=0`. For a dedicated bench
build, put a single `CONFIG_OPENTAIL_WATCHDOG_TEST_FAULT=N` in a separate defaults
file and use a separate SDKCONFIG and build directory, for example:

```sh
idf.py -B build-watchdog-test -D SDKCONFIG=sdkconfig.watchdog-test \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;/tmp/watchdog-test.defaults' build
```

An operator separately reviews/flashes that build. At/after 5s uptime, the
selected checkpoint logs `BENCH injection=N t_ms=...`. Modes 1/2 suspend the
input/render task; 3/4 simulate a hung O1 submit/completion boundary before the
actual call; 5 suspends the promoter; 6 spins the render core to starve IDLE;
7 blocks interrupts on that core; 8 panics. Modes 1–5 leave interrupts and
scheduling running, proving idle-only supervision would be insufficient.
Injection repeats after restart until quarantine, where it is disabled. These
simulations do not substitute for an actual RMT hardware fault. Build and verify
the normal zero-injection image separately before any release review.

| Stimulus / timeline to capture | Result |
| --- | --- |
| Modes 1–8: last progress → injection → WDT/panic → reset reason → sampled held inputs → first completed O1/O2 frame | NOT_RUN |
| Three resets → quarantine status, no new external RMT traffic, no configuration promotion | NOT_RUN |
| Assert Brake/Turn/Hazards/Reverse/Park before hang, during hang and through boot; measure illumination and original priority | NOT_RUN |
| BLE disconnect/reconnect while showing; low-load, disabled show, long effects, normal staging, STOP; no false reset | NOT_RUN |
| Returned O1 error with O2 running and reverse; STOP acknowledgement and independent output continuity | NOT_RUN |
| Real RMT completion fault versus simulated boundary suspension | NOT_RUN |
| Active/staged journal interruption at each commit boundary; corrupt NVS; held input throughout reboot | NOT_RUN |
| Brownout sweep, power removal/restore, supply failure, dead MCU and disconnected pixel data; independent essential-light observation | NOT_RUN |
| Pending OTA: successful confirmation, startup fault, validation failure, bad rollback slot and rollback | NOT_RUN |
| Every pre-existing 22-case physical acceptance test | NOT_RUN |

Host tests execute production recovery/OTA code against fake IDF calls and
exercise the existing CORE/Trigger/Effects/BLE/NVS/STOP suites. They cannot
measure reset time, electrical behavior or actual lighting. Independent
Firmware/Triggers/Hardware review and powered-board acceptance remain required;
Overseer gates release. See the draft PR for exact tested HEAD and build receipts.

## Local software validation receipt

| Check | Result |
| --- | --- |
| Full `core-firmware/tests/run-host-tests.sh`, including recovery and OTA production-code fake-IDF tests | PASS |
| Trigger runtime boundaries; existing OTA contract; preserved 22-case evidence contract | PASS |
| Android native source, live-edit/protocol, 21 release source checks | PASS |
| All 140 Android JNI effects vs pinned `6180e2a5ccbfbf8567513504669c6840b68c045b` CORE oracle, 122,316 frames | PASS, byte-exact |
| Packaging/installer/regression-policy unit tests and evidence-lock self-tests | PASS |
| ESP32-S3 ESP-IDF v5.5.5 local build, injection=0, existing `0.1.31-alpha` identity | PASS |
| Four-image ZIP CRC/SHA256/offset/image validation | See exact-HEAD PR receipt |
| Canonical registry-based GitHub Actions build | PENDING at implementation time |
| Independent Firmware/Triggers/Hardware review | PENDING |
| Actual reset/lighting/rail timing, all physical stimulus tests and existing 22 acceptance cases | NOT_RUN |

Local setup: Linux, GCC host tests, Temurin JDK17 JNI oracle, ESP-IDF v5.5.5
with its installed Xtensa 14.2.0 toolchain. Docker image extraction exhausted
the daemon's storage. The local runtime denied Espressif's Python constraints
and component-registry endpoints, so IDF's supported `IDF_PYTHON_CHECK_CONSTRAINTS=no`
option installed Python requirements from PyPI. The build used the official
GitHub `idf-extra-components` commit `691c87db8e8dfede3455b0bcdd7a1c2dee50c173`,
whose LED-strip manifest declares the required **3.1.1**, under an external
`espressif__led_strip` component directory with `IDF_COMPONENT_MANAGER=0`.
The firmware now declares its existing LED-strip dependency explicitly in CMake.
No local third-party code, tools, sdkconfig, binaries or credentials are committed.
The dedicated software workflow uses the normal registry dependency and the
official v5.5.5 IDF action; its artifacts do not approve vehicle release.
