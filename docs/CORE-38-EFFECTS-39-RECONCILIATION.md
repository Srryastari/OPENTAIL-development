# CORE #38 + Effects #39 — firmware-owned reconciliation

Status: **draft integrated software candidate, bench-only, no hardware flash or road-use authorization**.

## Exact source provenance
- Parent CORE #38: `f9614060b892f474e97501dc46a41e6242f60ec3`; inherits firmware #19 and Triggers #24 plus authenticated BLE v3 custom-palette protocol (issue #33 / firmware #36 contract).
- Effects #39: `dbb679721cb88b276ed199355e0106de0eba79f9`. Reuse exact source blobs `wled_fx_compat.c/.h` and its full parity/compatibility test suites; existing `effects_1d`, `fx_overlay_engine`, `fx_zone_bank`, all headers were already byte-identical on #38 and #39.
- Single global frame producer selects source harmonic palette generation using the new `harmonic=true` argument. The existing random producer is not duplicated.
- `palette_1d.c` intentionally remains CORE #38's version: #39's older fixed-only helper rejects custom palette IDs 73–200, while #38 has validated authenticated NVS-backed snapshots. Replacing it would regress custom palettes.
- Source gamma RGB table construction and inverse tables are imported in the pure renderer library and covered by #39's golden test. Current CORE decorative output calibration and road-trigger color bypass remain unchanged. This is **not** a claim of optical/gamma-on bus parity.
- Keep one RMT pixel writer, one physical input sampler, render snapshots, fault isolation, OTA/STOP protections and status LED patterns unchanged.

## Software proof
- Dedicated exact-commit host, ESP-IDF 5.5.5, Android debug compatibility and Studio jobs checkout `pull_request.head.sha` and assert `git rev-parse HEAD`; release evidence gate is not changed.
- Host tests include 120 direct pinned-source golden effects, sparks/flares/particles, custom palette v3 CRC/NVS fake fault injection, BLE/trigger ABI/zone and status logic.
- See branch-head workflow run for authoritative results; merged PR checkout checks are supplemental only.

## Outstanding release blockers
1. Durable active/staged NVS policy for power-loss/reboot while a physical input remains asserted (trigger mapping and output zones) is unresolved.
2. Device-side flash fault injection, measured optical gamma/color order/latency, physical RMT/OTA/STOP testing, dual-MCU failover and 22 electrical/optical acceptance checks remain unverified.
3. Cross-team approval and an actual final release image require their own gates; do not merge other teams' PRs or bypass release evidence.
