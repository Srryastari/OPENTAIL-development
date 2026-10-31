# OpenTail CORE dual-MCU failover — software acceptance boundary

**Candidate ancestry:** Firmware PR #81 `d3ca7d036392d7709848b143a41f6bed1a26cb7b` (Effects #80 corrected renderer; Firmware #69 BLE long-read and #62 recovery; #54 boot-stack; #49 NVS safety). This extension is **software-only and inert** until a separately reviewed and physically implemented interlocked Rev B board exists. NO vehicle/sole-rear-light use is certified.

**Hardware source of truth:** Hardware PR #32 `9001c3322442268f877bf5014ed5fbcf9d610f1c`, especially `hardware/FAILOVER_ARCHITECTURE.md`, `UNIVERSAL_LAYOUT_CONTRACT.md`, `FAILOVER_POC_WIRING.md`, and `REV_B_ACCEPTANCE.csv`. Hardware #32 is a draft proposed architecture, not a built electrical interface.

## Precise gap assessment

| Requirement | Software now | Hardware or additional software still missing |
| --- | --- | --- |
| Primary rendering, real IN1..IN8 priority, STOP, BLE HMAC, NVS | Existing CORE #81 unchanged | Real dual-processor trigger input verification |
| Backup essential RGB frame algorithm | New `ot_failover_essential.c` pure C; physical mask from its *own* isolated sampler supplied by caller; only Park/Brake/L/R/Reverse | Separately built backup firmware image, independent input sampler and independently functioning RMT output path |
| Versioned essential profile + integrity | New canonical field digest, nonzero committed revisions, generation and complete five-essential-role/mapping validation | Authenticated P→B transport, durable backup-specific two-phase journal, verified readback after reboot, independent qualified NVS storage |
| Staging vs active | New two-replica equality/idle-input activation **precondition**; does not change active CORE NVS | Transaction coordinator with rollback/recovery and freeze on input assertion, interrupted transfer handling, independently acknowledged active revision |
| Owner/arbiter veto | New pure decision: no valid grant under missing interlock, dual grant, unhealthy rail/sampler/frame/watchdog, mismatched active profile or unfinished handoff | Independent supervisor and electrically interlocked, measured break-before-make pixel-data selector; live grant/health pins and validated timeouts |
| Watchdog/frame freshness | Software policy requires frame progress, not an alive timer | Hardware observer of actual completed RMT/data output and independent per-MCU rail monitoring; stuck HIGH/LOW detection |
| Primary failure/recovery | External ownership preconditions specified; no operation from main CORE | Independent takeover/handback hardware state machine, qualified recovery windows, measured DI reset and full-frame replacement |
| 12 pixel lanes, 10 protected AUX | Existing 2-lane CORE preserved | 12 isolated physical data banks/10 high-side drivers and own expansion firmware |
| Common LED strip, supply, first-pixel and harness failure | Not solved by a backup processor | Independent luminous essential-light path, qualified supply and road-lighting acceptance |

## Implemented code contracts

1. `ot_fo_profile_seal`: returns a canonical profile only from a caller-provided **committed active snapshot**, with a positive generation, verified per-output/trigger revisions, validated arbitrary contiguous user-defined zones, all five essential roles and independent mapping for all five vehicle signals. No GS400 geometry, default safety map, BLE, NVS or hardware writes.
2. `ot_fo_profile_valid`: validates format, revisions, geometry, mappings and FNV-1a digest. The digest is a corruption check, **not a signature, message authentication or replacement for the pre-existing CORE NVS journal**. A deployment must authenticate cross-MCU profile transport.
3. `ot_fo_can_activate_profile`: returns false unless both independently supplied sealed active-copy records agree and raw/debounced inputs are idle. Does **not** itself implement durable two-party commit; a crash-safe transfer journal is still required.
4. `ot_fo_assess_grant`: purely examines independently observed hardware qualification. Dual grants, unverified arbiter, missing local power/sampler, stalled completed-frame progress, missing watchdog, mismatched profile and incomplete break-before-make/full-frame handoff reject. **Never toggles GPIO/OE/mux, starts RMT or grants actual electrical ownership**.
5. `ot_fo_render_essential`: produces bounded *logical RGB* frame on any valid installed output, with `ot_tm_resolve` and `ot_om_trigger_color`, no WLED effects, gamma, decorative color correction, AUX or retained SHOW. The future driver must apply the selected existing color-order swizzle and prove a new complete frame after ownership switch. This is not a physical driver and needs its **own** real inputs; Primary must never be the sole input source.

## Handoff and fault recovery (required electrical implementation; NOT implemented)

1. **Reset / startup:** independently powered supervisor forces BOTH output banks disabled/high-Z. Boot GPIO levels, USB insertion, brownout and MCU resets may not override this. No software clock, BLE command, or NVS write is a safe OE.
2. **Qualification:** hardware independently verifies rails, isolated input acquisition, committed matching essential profile and *actual* completed-frame progress. Primary becomes eligible only after a hardware-defined stable interval; Backup qualifies independently.
3. **Primary loss:** supervisor detects rail failure, WDT failure, frozen frame progress or stuck DI; **external** logic disables Primary first on every populated lane, verifies both banks electrically off for a measured break interval and only then grants Backup if independently healthy.
4. **Backup ownership:** Backup must send a complete fresh essential frame from its own IN1–IN8 and last-known-good profile. Physical Brake/Left/Right/Hazards/Reverse must be asserted on assigned zones throughout transition. Measure blackout; do not invent or promise a millisecond bound. STOP affects effects, not physical input rendering.
5. **Primary recovery:** require a longer measured stable qualification than failure detection; externally revoke Backup and observe no-overlap interval before grant to Primary, followed by a fresh complete frame. Prevent oscillation across rapid reset/fault sequences.
6. **Both failed or common strip/power fault:** hardware disables contested buses; **dark outputs are NOT road-safe**. An electrically independent luminous fallback path must maintain required road lights. Independent factory lamps must remain connected during development.

No specific GPIO, mux IC, supervisor timing or pin polarity has been assigned: none is approved/measured. The Adafruit #6066 is a 2-channel level shifter, not a two-owner arbiter. With owned parts, perform P and B standalone tests only with **ALL power removed before swapping GPIO data wires**, as specified by Hardware #32.

## Verification plan and release gates

- Run `cd core-firmware && bash tests/run-host-tests.sh`, which now includes `test_failover_essential.c` and `test_failover_runtime_isolation.py`: 256 physical-input combinations on each of two outputs; exact Brake/Park, both turns, Reverse, no decorative/AUX and correct zone isolation; profile corruption and stale revision rejection; readback-equivalent two-party precondition; 32 combinations of failover readiness.
- Verify the complete previous Effects #80 golden tests, BLE long-read, HMAC, STOP, crash-safe NVS, universal zones, status logic and Trigger regressions remain unchanged. Compile IDF 5.5.5 ESP32-S3 at the **exact branch HEAD**.
- `docs/FAILOVER-22-PHYSICAL-ACCEPTANCE.csv` contains the exact **22** IDs from `docs/CI-REGRESSION-BASELINE.json` with one executable procedure and all statuses **NOT_RUN**. An observed case requires exact flashed SHA, observer, hardware ID, timestamp, measurement and proof. The CI physical release-evidence gate is untouched.
- Hardware #32 additionally has 30 separately **BLOCKED** Rev B electrical cases H01–H30 in `hardware/REV_B_ACCEPTANCE.csv`: notably H13–H22 for true ownership/MCU takeover. These do not replace or falsely satisfy the mandatory 22.
- Future two-output prototype acceptance order: inspect qualified power/input/ground/fuses first; external interlock + logic analyzer with **no LED 12V**; standalone P/B and deadtime; short strip low current; inject primary CPU/RMT/rail/DI faults; measure Brake/hazard/Reverse; recovery/rapid flaps; active/staged profile and power-cut; full 623×2 strings **only after** power and optical qualification.
- Gate remains **FAIL** for physical takeover, genuine watchdog monitoring, independent luminous fallback, and road release until hardware evidence exists. No merge/flash into main/master on the basis of this software branch.

**Principles:** Simplicity First / No Laziness / Minimal Impact. No Android/Work UI changes, no BLE/NVS ABI modifications, no direct main commits, no unsupported automotive fail-safe claim.
