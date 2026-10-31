# OpenTail final bench automation — exact-source, NOT road release

**Firmware binary baseline:** [CORE failover-policy PR #84](https://github.com/Srryastari/OPENTAIL-development/pull/84) exact **`b13894c0550366a0e74ef6fd93c83cfa6dd19aa4`**. Dedicated [software build run 41213287531](https://github.com/Srryastari/OPENTAIL-development/actions/runs/41213287531) passed and produced the complete bench application/bootloader/partition/flash manifest artifact `opentail-failover-software-bench-b13894c0550366a0e74ef6fd93c83cfa6dd19aa4` (ID `13047640462`). The instrumented-session PR changes **only scripts, tests, workflow and docs**; it does not create a new firmware binary or silently repin the existing artifact. If a newly built source SHA is required, build and verify it explicitly; never relabel #84's image as a newer commit.

**Physical historical baseline:** `docs/CI-REGRESSION-BASELINE.json` records previous user-observed v0.1.28-alpha checks with no proven flashed SHA. `docs/verified-bench-regression-matrix.md` keeps historical observations separately. Do not infer a current-SHA physical pass from historical PASS or a host-test PASS. The mandatory 22-case `docs/FAILOVER-22-PHYSICAL-ACCEPTANCE.csv` remains **22/22 NOT_RUN**; this bench runner **never edits it** or writes `current-candidate.json`.

## Five instrumented sessions — exact 22 cases

| Session | Physical release case IDs | Available automatic capture now | Real missing fixture / remaining human evidence |
| --- | --- | --- | --- |
| **S1** independent input / OEM priority (6) | `inputs_gpio_mask`, `brake_park_priority`, `hazards_turn_priority`, `reverse_overlay`, `show_resume_phase`, `strobe_edges` | Read-only status/raw+debounced masks, output telemetry, 115200 UART and optional camera; **opt-in firmware simulation 0/1/2/3/12/14/15/16/31/224/255** with LED 12V OFF | Real electrically isolated PC817 input stimulus still requires physically qualified opto-side voltage/polarity/current and wiring. Independent multi-channel injector/relay needed for unattended **physical** masks; scope/logic analyzer for measured real edges, optical camera for LED observation |
| **S2** outputs / FPS / power (5) | `pixel_chain_independence`, `full_1246_dual_lane`, `universal_zones`, `color_order_brightness`, `render_fps_low_brightness` | Output-zone JSON, counts, revisions, refresh counters, measured software FPS, UART; video if USB webcam available | Existing #6066 two-lane proof is not an automatic 2-MCU mux. 623+623 full-load requires inspected rated 12V power/harness/injection/fuses and actual current meter; oscilloscope or logic analyzer for true DI timing/isolation. **Do not presume fused 20A supply supports worst-case all-white** |
| **S3** STOP / effects / onboard status (4) | `status_led_patterns`, `stop_restore`, `event_horizon_shutdown`, `effect_source_parity` | Read-only show/state/status LED/error/frame counters, optional USB camera, timestamps and UART | Existing phone camera can record visible colors and continuity but is not a calibrated colorimeter/timing probe. Android owner-authenticated BLE RGB snapshot export or equivalent independently pinned oracle + synchronized optical capture needed for full effect source/physical parity. Camera/photodiode needed for emitted frame timing |
| **S4** Android / BLE / Studio (3) | `ble_auth_reconnect`, `studio_interactive_ui`, `android_native_ui` | Read-only CORE BLE/session/status and timestamps; existing Android phone/PC can supply screen recordings and manual reconnect | Phone owner-key authentication and BLE GATT packet capture must be performed on-device; an optional BLE sniffer or Android HCI trace is needed for RF-level timing. UI 1:1 comparison requires the user's approved references; **do not edit the UI** |
| **S5** NVS / OTA / fault recovery (4) | `nvs_v1_v2_persistence`, `ota_usb_partition_migration`, `ota_success_rollback`, `ota_authorization` | Read-only output/trigger revision snapshots and serial boot logs; safe negative-request capture only after dedicated isolation | Backed-up existing NVS, verified legacy/known-good/bad test images, safe `esptool` recovery, controlled independent power cycling and separately functioning rear lights. No automated flashing, unsafe power interruption or firmware mutation is performed by this runner |

**22 = 6 + 5 + 4 + 3 + 4.** Each case's executable operator cue, hardware tier and missing dependency is in `scripts/bench-automation-sessions.json`. A physical pass requires **observed emitted LEDs or real GPIO/device operation** plus a trace: REST simulation, status counters, software golden-frame comparison or video alone cannot satisfy timing/optical/electrical requirements.

## Already owned versus missing (no speculative buying)

**Available in the established bench inventory:** hiBCTR ESP32-S3 N16R8 boards (six were reported), two PC817 8-input boards, the Adafruit #6066 two-channel level shifter, addressable WS2815 12V strips, current bench supply with 20 A fuse, existing wiring and fuseholders, Windows host (COM9/COM10 historically), Android phone, existing camera on that phone, and the firmware's WPA2 bench AP at `192.168.4.1`. Inventory is historic; physically re-confirm device, strip SKU, rail ratings, connector condition and PC817 wiring before energizing. **A 20 A fuse is not a measured conductor or LED load approval.** No WAGO is assumed.

**Not established as owned/qualified:** calibrated multimeter/current clamp, logic analyzer/oscilloscope and timing probes, USB webcam for automated video, opto input bank auto-injector/isolated relay and qualified input driver, programmable/qualified power interrupter, independent supervisor, tri-state interlocked P/B data mux, independent backup power and isolated inputs verified under faults, backup runtime+driver, and independent legal luminous fallback. No algorithm or script can replace those physical parts.

## Execute now (Windows PowerShell, from repo checkout)

Read-only smoke test: `py -3 scripts/bench_capture_session.py --self-test`; list all sessions: `py -3 scripts/bench_capture_session.py --list`.

For genuine observed physical events, use the **human-cued recorder**. For example, S1 with existing host USB serial and bench AP, after safely validating harness and opto input conditions:

`py -3 scripts/bench_capture_session.py --session S1 --http --serial COM9 --interactive --seconds 8 --out bench-evidence`

This records all S1 raw telemetry while you safely produce each stimulus yourself; it **does not inject 12 V**. Install optional UART dependency `py -3 -m pip install pyserial`. Close any other serial monitor first. If no serial port is accessible, omit `--serial COM9` and continue with REST read-only. The PC must be connected to the dedicated bench AP for `--http`.

For a separately attached **USB webcam** and S3 optical/LED frame recording (NOT a phone camera unless connected as a supported webcam): `py -3 -m pip install opencv-python`, then `py -3 scripts/bench_capture_session.py --session S3 --http --camera 0 --interactive --seconds 8`. Otherwise use a phone video and attach it to the matching timestamped session folder as independently annotated evidence; the script does not assert it was recorded.

**Software-only synthetic trigger drive** is deliberately isolated and requires all three attestations. Only after **LED 12 V OFF, device disconnected from any vehicle and solely on an isolated bench**:

`py -3 scripts/bench_capture_session.py --session S1 --http --simulate --ack-off-vehicle --ack-led-power-off --ack-isolated-fixture --seconds 2`

That POSTs short `/api/v1/simulate` inputs in the established masks and clears them afterward. The script refuses if there is an active physical input or preexisting synthetic mask. This is **not** an automated real PC817 input test. There is **no** automated electrical injector configured; wire nothing new based only on these scripts.

For S2/S4/S5 use the same command with `--session S2`, `S4` or `S5`. All commands are **read-only unless** `--simulate` is explicitly and safely armed; no `/api/v1/ota`, NVS writes, flashing, power cutoff, STOP, UI modification or owner-key extraction. The tool never opens a vehicle connection.

## Recorded provenance and acceptance rules

Each session produces `manifest.json`, `telemetry.jsonl`, `events.jsonl`, `serial.jsonl` and optional per-case camera MP4s. Captures contain UTC wall-clock timestamps, labels, requested window duration, failures, file SHA256s, full declared 40-hex firmware SHA and **`flashed_sha_verified: false`**. All cases remain `UNVERIFIED`. The optional `--flash-log` copies an original human flash receipt and records its hash, but even that is **not an independent exact-image attestation**. To physically certify this SHA, a technician must verify the actual image flashed/partition offsets, board identity and captured evidence, then separately submit the original repository release schema with observer, hardware_id, proof and timezone-aware tested_at. Do not allow an automation to self-approve.

The exact original 22 cases remain mandatory through `.github/workflows/verify-active-sources.yml` and `scripts/check_regressions.py --mode release`. Historical observations and new CI results belong in **distinct** evidence categories. Software regressions passed on #84 and are not rerun by this new script-only gate.

## Automatic failover remains BLOCKED

Hardware PR #32 `hardware/FAILOVER_ARCHITECTURE.md` defines the supervisor-controlled exclusive output owner, measured break-before-make, true completed-frame watchdog and independent backup input/profile/power path. Existing #6066 is **not** an automatic 2-source mux. The current CORE still advertises `hardware_failover:false`; its pure `ot_failover_essential` code does not control a mux or implement backup firmware. A shared strip/supply/first-pixel fault also defeats both MCUs until independent luminous fallback exists.

**Do not connect two ESP GPIOs to the #6066 simultaneously, do not hot-move data wires, do not energize unverified 12V loads, do not disable existing OEM rear lamps. Do not merge into main, certify vehicle use, or waive release gates.** This is a bench observation tool and physical acceptance preparation only.
