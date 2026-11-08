# Issue #113 — PR #114 vs PR #115 canonical watchdog review

Review date: 2026-11-08  
Locked integration baseline: PR #112, `1a300767d985343d060faf344bff2da772dca88f`  
Owner architecture: ONE ESP32-S3 N16R8; no second MCU; no redundant-light claim.

## Candidate inventory

| Candidate | Exact reviewed head | Files / diff | Dedicated evidence |
|---|---|---:|---|
| PR #114, `feature/113-single-esp-watchdog` | `3145bb37f583c28cd1ac7776031916bf712613aa` | 23 files; +1215 / -30 | watchdog workflow run `41606156743`: 2/2 jobs PASS; Trigger run `41606156767`: PASS |
| PR #115, `firmware/issue113-single-esp-progress-wdt-20261103` | initial reviewed head `1ec3ad7f41a5fa45e10314a270008cc83a4413b2` | 17 files; +895 / -20 | Issue113 exact-head run `41609323181`: 6/6 jobs PASS; Trigger run `41609323192`: PASS |

Both candidates are draft/unmerged and share the same PR #112 base SHA. Neither has a submitted independent human review at the time of this comparison.

## Why PR #115 is canonical

1. **Progress supervision is narrower and directly tied to essential work.** It reconfigures the IDF TWDT at 15s, retains both idle-core monitors and built-in IWDT/bootloader WDT, and registers separate input-sampler and render-output users. The input user is fed after all eight GPIOs are sampled/debounced/published; render progress is credited only after actual RMT async completion. A surviving output can continue while an independent output fault remains diagnosed.
2. **PR #114 contains an unfed startup watchdog user.** Its `ot_watchdog_init()` arms `OT_WATCH_STARTUP`, but the complete candidate source has no `ot_watchdog_progress(OT_WATCH_STARTUP)` or `ot_watchdog_disarm(OT_WATCH_STARTUP)` call. With its 10s TWDT, this user remains subscribed and unfed after startup; an otherwise healthy device would eventually trigger the task watchdog. This is a blocking defect, not a style preference.
3. **Reset classification is safer in #115.** #115 identifies `ESP_RST_CPU_LOCKUP` and keeps brownout distinct from the consecutive panic/WDT crash streak. #114's `ot_recovery_boot()` increments for every reset except power-on/software, so brownout is counted as an application crash.
3a. **#114's render watchdog feed is not tied to a successful RMT completion.** Its render loop calls `ot_watchdog_progress(OT_WATCH_RENDER)` at the common frame-loop tail, even after a frame in which every output submit/completion failed. That allows repeated failed output cycles to feed the render user. #115 only feeds the render user when `ot_sw_render_progress_eligible()` sees at least one actual completed lane; no completion means no feed and the 15s TWDT can reset.
4. **Held-input startup is stronger in #115.** It samples all eight physical inputs before the first RMT frame, seeds the production debouncer from that snapshot, and includes the executable 256-mask first-frame/hold/release host test. #114's tests do not provide an equivalent 256-mask startup seed gate.
5. **OTA acceptance is later in #115.** #115 defers `ot_ota_confirm_boot(true)` until a complete two-output frame, stable recovery state, and BLE/HTTP readiness. #114 calls confirmation after the first full frame and uses its 60s watchdog-health window after that, so it accepts the OTA image earlier.
6. **#115's exact-head test evidence is broader.** Its dedicated run verifies exact-source boundary, Android unchanged contracts, independent Trigger guard, full CORE host suite, ESP-IDF v5.5.5 build, and a downloaded four-image ZIP integrity check.

## #114-only changes: disposition

### BLE GATT long-read / cache work — deliberately NOT ported

PR #114 changes `ble_status.c` to cache the immutable effect catalog and alter frame snapshot reads, with `test_nimble_long_read_contract.py`. This is BLE discovery/read-path performance work, not watchdog recovery. It remains separate by owner instruction; no phone latency or throughput claim is made in this watchdog PR.

### Additional configuration-task watchdog user — NOT ported

PR #114 adds a separate config-progress user fed from the configuration promoter loop. The promoter is a low-priority durable-configuration worker; a stall must leave staged/active journal recovery safe and must not manufacture progress for the physical input or RMT users. Resetting the entire lighting controller solely because this background worker stalls is not required for the single-ESP watchdog acceptance and increases coupling. Existing NVS journal checks remain unchanged.

### OTA deadline — concept ported, not the 30s implementation

PR #114 introduces a 30s pending-OTA timer. A 30s deadline conflicts with #115's 60s healthy-runtime requirement and could roll back a candidate before it has enough time to become stable. The canonical #115 branch now uses a **120s** pending-verify deadline, while preserving the 60s continuous healthy-output/input confirmation gate. It uses an atomic WAITING/CONFIRMING/CONFIRMED/EXPIRED state to prevent a timeout racing a flash confirmation, and explicitly requests IDF rollback on expiry. Factory/non-OTA, already-valid and `ESP_ERR_NOT_FOUND` states do not arm the deadline. The default ESP-IDF bootloader rollback remains enabled as reset-time fallback.

### Fault injection — narrowly ported

#115 now has compile-time-only Kconfig injection (range 0–7, default 0): input sampler stall, render-task stall, O1 submit stall, O1 completion-wait stall, render-core idle starvation, interrupt blocking and software panic. It has no BLE/HTTP activation path. Config-task injection was omitted because #115 intentionally does not add a separate config watchdog user. Host timelines remain explicitly simulated; no physical fault injection is claimed.

## Final candidate delta

The canonical #115 branch retains its original single-ESP implementation and adds only:
- continuous 60s healthy-window accounting (full dual-lane completions + fresh eight-input samples + no output/config fault);
- disabled-by-default compile-time watchdog fault-injection hooks;
- a 120s pending-OTA rollback guard aligned with the 60s stable gate;
- runtime fake-IDF OTA confirmation/rollback state tests and stronger structural checks;
- this comparison/evidence report.

No BLE discovery optimization, Effects renderer, Android source/UI, trigger arbitration, NVS journal, GPIO mapping, output pin, partition layout or firmware version change is included.

## Acceptance boundaries

- Exact current branch SHA and fresh CI receipts must be taken after the above changes; the historical #115 run is not evidence for the new head.
- Independent Triggers and Firmware review is still required. No reviewer identity/team slug is defined in this personal repository, so review must not be fabricated.
- Powered-board watchdog timing, injected fault execution, real OTA rollback, all 22 physical checks, optical outputs, power/brownout and independent essential-light path remain NOT_RUN.
- No merge, flash, vehicle use or release certification. PR #115 remains the one canonical draft candidate; PR #114 remains historical/competing evidence and must not be merged.
