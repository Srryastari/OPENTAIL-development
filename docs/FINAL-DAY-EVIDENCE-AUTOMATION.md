# OpenTail — final-day automation and honest acceptance

## Goal
Use **one GitHub Actions run** to consolidate independently passing exact-SHA
software evidence for four owners. Do not re-run dozens of source checks by hand,
silently merge branches, alter approved UI/palettes, or invent physical PASS.

### Pinned software inputs (immutable until explicitly revised)

| Owner | PR | Exact commit | Required workflow |
| --- | --- | --- | --- |
| Android | #86 | `5615bade61542f30a4a9c4375e2cadea30de2105` | Android exact-head software integration |
| Effects | #80 | `7c0fcc450a7619a57603aabf833dea855a18bcd8` | Effects proprietary continuity exact HEAD (software only) |
| Triggers | #83 | `0477355fdbd562cfe357569bf8f15d0e08374e8f` | Trigger CORE81 pinned-source safety evidence |
| Firmware | #84 | `b13894c0550366a0e74ef6fd93c83cfa6dd19aa4` | CORE backup essential policy exact-head software gates |

**One-run procedure:** Actions → **OpenTail final-day exact-SHA software
evidence freeze** on this feature PR. The job runs
`python3 scripts/final_day_evidence_lock.py --self-test` and
`python3 scripts/final_day_evidence_lock.py --output software-evidence-lock.json`.
The second step queries GitHub's official API with the read-only GitHub
Actions token and proves each PR's *current HEAD* matches the pinned SHA and has
the named successful pull_request workflow run for that same head.
Missing/mismatched/rebased/replaced or in-progress results return BLOCKED.
An artifact records the four exact SHAs and run URLs.

This is a **pinned software evidence receipt**, not proof of a combined
on-device app/firmware build, physical LED signal behavior, or automatic
hardware failover. No code is merged by the workflow. Any later source edit
invalidates the receipt and requires a new exact-head run.

## 22 mandatory physical cases — grouped, not waived

`docs/FAILOVER-22-PHYSICAL-ACCEPTANCE.csv` is the authoritative, unchanged
22-case inventory. `scripts/final_day_evidence_lock.py` ensures it matches
`docs/CI-REGRESSION-BASELINE.json` one-to-one, verifies every procedure and
precondition, checks that any PASS has measured values, evidence URI, observer,
hardware ID and timestamp, and exports four contiguous test groups:

1. **Isolated input/latency bench** — PC817 masks, Brake/Park, Hazards,
   Reverse, show continuation, two output lanes, universal zones, colors/
   brightness, STOP and strobe edges. Needs known safe isolated test stimuli,
   powered real optocouplers, oscilloscope/logic analyzer and independent lamps.
2. **Actual pixels/effects bench** — 623×2 physical strip length, status LED,
   measured FPS/jitter, one-shot effects, source-vs-pixel frame optical/logic
   evidence. Needs fused rated supply, the actual LED strips and measurement
   devices; host/JNI equality alone is insufficient.
3. **Power-loss/config/BLE/OTA bench** — NVS migration and held-input power
   cycle, phone BLE authorization and reconnection, OTA partition migration,
   healthy rollback and authorization rejection. Automate driving the existing
   REST/BLE test clients where supported; actually interrupt hardware power
   through a rated instrumented rig and capture readback.
4. **Visual owner acceptance** — Studio browser interaction and 1:1 Native
   Android six-screen visual/functional review, using approved reference,
   owner-visible phone and real BLE/STOP behavior.

Software CI can run all host test matrices without the user's time. A physical
fixture can automatically drive *multiple* stimulus combinations per session,
record frame values, sample GPIO and build evidence, reducing manual actions.
Such a fixture has **not been shown or demonstrated in this project**; neither
this script nor CI owns hardware and therefore cannot mark real tests PASS.
Never inject arbitrary vehicle 12 V into an ESP32 or PC817 logic signal.
Require qualified input isolation, common grounds where specified, current
limits and existing independent OEM lamps.

### Additional automatic failover evidence
The 22 general physical cases are separate from the Rev B hardware PR #32's
30 electrical cases, particularly its actual supervisor/arbiter, independent
power/input paths, physical no-overlap takeover, watchdog and recovery
acceptance. The controller still advertises `hardware_failover:false`;
the currently owned #6066 is a shifter and cannot interlock two MCU outputs.
**Do not connect both MCU data drivers to one pixel line.**

## Final freeze decision
- **Software contract evidence:** may be frozen when the automated GitHub
  receipt says `PINNED_TARGETED_CI_PASS`. Separate candidates remain draft.
- **Phone/bench evidence:** stays `NOT_RUN` until real observations are
  recorded; don't convert historical 0.1.28 tests into a current-SHA PASS.
- **Physical automatic takeover and independent luminous safety:** not built or
  qualified. Remain blocked even if every software test is green.
- **Road/vehicle release:** ALWAYS BLOCKED here; this workflow has no
  privileges to merge, approve, flash, or disable release-evidence gates.

Simplicity First. No Laziness. Minimal Impact.
