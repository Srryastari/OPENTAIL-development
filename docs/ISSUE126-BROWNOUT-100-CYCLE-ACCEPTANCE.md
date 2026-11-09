# Issue #126 — Mandatory 100-Cycle Brownout Endurance Acceptance

Status: **NOT RUN — physical Hardware + QA gate; production readiness prohibited until 100/100 PASS.**

This procedure is mandatory for Firmware, Hardware and QA. A host test, simulated power cut, CI build, successful flash, or single manual reboot does not count as a physical cycle.

## 1. Fixed acceptance target

- Load: 944 WS2815 pixels, 12 V, full-white output (R=255, G=255, B=255 at the approved firmware ceiling).
- Endurance: 100 controlled power-cut/recovery cycles.
- Commit activity: a real geometry transaction and NVS commit in every cycle.
- Cut timing: randomized across the eight durable cross-namespace commit phases below, with phase coverage recorded.
- Recovery: reboot after each interruption and verify partition, NVS, journal, CRC, active geometry and safety behavior.
- Required result: 100/100 recovered cycles, zero unrecoverable corruption, zero invalid active configurations, zero silent configuration loss, and no safety arbitration regression.

The present PR does not yet implement the final hardware-only/map-only cross-namespace journal. Therefore this test is blocked until that implementation and its host crash matrix are complete.

## 2. Electrical preflight — mandatory before energizing the endurance run

Do not infer current from pixel count or assume the existing bench fuse proves the load is safe.

1. Confirm the exact 944-pixel WS2815 strip/section, wiring topology, injection points, connector ratings, conductor gauge, polarity and actual LED supply.
2. Measure full-white LED-rail current at the supply and voltage at both the source and the furthest injection point. Record steady-state and startup/inrush behavior with suitable instruments.
3. Verify the supply has adequate continuous and transient capacity for the measured load.
4. Select fuse protection from measured load, conductor ampacity, connector ratings and supply characteristics. Confirm fuse interrupt/rating suitability. A fuse is protection, not a current regulator.
5. Measure controller/buck input and 5 V output under the same conditions. Record minimum/maximum voltage during normal operation and controlled interruption.
6. Record ambient temperature and baseline temperatures of the ESP32, buck converter, pixel shifter, fuse holders, connectors, wiring and LED strip. Verify every component remains within its documented rating.
7. Stop before the endurance run if any voltage, current, fuse, conductor, connector or temperature limit is unknown or exceeded. Do not raise the firmware brightness ceiling to force the target load.

### 2.1 Rail topology and interruption point

The intended design separates the controller supply path (fused 12 V into the 4R7 buck, powering the ESP32/pixel shifter logic) from the protected 12 V WS2815 LED rail. This is a design intent, **not proof of the current bench wiring**.

Before cycle 1, attach an as-built wiring drawing and explicitly mark:

- Controller rail source, fuse, interruption switch/relay, 4R7 input and 5 V output.
- LED rail source, fuse, distribution/injection points and the 944-pixel load.
- Signal ground and LED power return paths; confirm the small signal-ground chain is not carrying LED current.
- Which rail is interrupted in each cycle and which rail remains energized.
- Whether common ground remains connected during the interruption.
- Whether the ESP32, pixel shifter and LED strip lose power together or independently.

**Default controller-brownout setup, only if the as-built wiring confirms separate rails:** interrupt the controller/4R7 input rail while the separately fused WS2815 LED rail remains energized at the measured full-white load. Record that the LEDs may retain their last latched frame while the controller is off; verify actual behavior before cycle 1. Do not interrupt signal ground as a test shortcut.

If the as-built bench does not match this topology, stop and have Hardware/Overseer approve and document the actual interruption arrangement before testing. If additional both-rails-off testing is desired, record it as a separate test class; it must not be silently mixed into the 100 full-white-load cycles.

## 3. Durable commit phases and randomized cut coverage

The final cross-namespace implementation must identify these eight durable phases in firmware diagnostics/test hooks. Names must be reconciled with the actual implementation before the physical run; do not claim these hooks exist until verified.

| Phase | Durable boundary to interrupt after |
|---:|---|
| 1 | Hardware-lane staged record committed and read back in `ot_output_cfg` |
| 2 | Map-only staged record committed and read back in `ot_zone_map` |
| 3 | Cross-namespace pair-intent marker committed and read back |
| 4 | Hardware-lane active record committed and read back |
| 5 | Map active record committed and read back |
| 6 | Hardware-lane staged marker erased and absence verified |
| 7 | Map staged marker erased and absence verified |
| 8 | Pair-intent marker cleared and absence verified |

Also include a controlled interruption after phase 8 but before the final quiet-window/edge-epoch check and RAM publication. This is a separate pre-publication recovery boundary, not a ninth durable NVS phase.

Use a recorded random seed to choose the cut phase and intra-phase delay. Ensure all eight phases receive physical coverage across the 100 cycles. Do not inject a cut merely by sleeping a guessed number of milliseconds; use a verified phase marker/diagnostic hook so the log identifies the actual phase reached.

## 4. Per-cycle procedure

For each cycle:

1. Record cycle number, exact firmware SHA, build ID, boot ID, test operator, date/time, random seed and selected cut phase.
2. Verify the full-white 944-pixel load is active and record LED-rail voltage/current and component temperatures.
3. Read the current authoritative hardware configuration, map configuration, active/staged revisions, transaction ID, CRCs and partition-table identity.
4. Start a real geometry transaction that changes a known test boundary without changing physical trigger assignments or safety priority.
5. Allow the firmware to reach the selected durable commit phase. Confirm the phase marker before opening the designated power interruption.
6. Interrupt the documented rail. Record the rail state, voltage decay, interruption duration and whether the LED rail/common ground remained energized.
7. Restore power using the documented sequence. Capture boot log and boot ID. Do not manually erase NVS, reflash, repair partitions or reset configuration to make recovery pass.
8. Verify partition-table integrity and expected partition offsets/sizes against the approved build manifest.
9. Verify NVS initialization and integrity for every affected namespace. Read active, staged and pair-intent records; validate magic, schema, generation/transaction identity, revision pair and CRC.
10. Verify the recovered configuration is exactly the complete previous generation or the complete committed candidate generation. A mixed hardware/map generation is a failure.
11. Read back the complete hardware and Panel/Zone configuration through the supported diagnostics/BLE protocol. Verify canonical CRC and active/render revisions.
12. Exercise physical Brake, Left, Right, Hazards, Reverse and STOP behavior using the approved bench procedure. Verify physical safety authority is unchanged.
13. Record PASS/FAIL and preserve raw logs and instrument captures before beginning the next cycle.

No cycle may be marked PASS if recovery required manual repair, NVS erase, firmware reflash, default substitution, or an unrecorded operator intervention.

## 5. Required measurements and evidence

Record at minimum for every cycle:

- Firmware SHA and build identifier.
- Pre-cut and post-reboot boot IDs.
- Transaction/session ID and previous/candidate generation IDs.
- Interrupted phase and phase-marker evidence.
- Which rail was interrupted; which rails and grounds remained energized.
- Controller input voltage, buck 5 V voltage, LED-rail voltage at source and furthest injection point.
- LED-rail current under full-white load.
- Ambient, ESP32, buck, shifter, fuse holder, connector, wire and strip temperatures.
- Power-cut duration and recovery-to-ready time.
- Partition-table hash/readback result.
- NVS initialization/integrity result for `ot_output_cfg`, `ot_zone_map`, and any other namespace touched by the transaction.
- Active/staged/pair-intent generation and revision reconciliation.
- CRC and full configuration readback result.
- Boot success, BLE availability, physical-trigger/safety arbitration result.
- Any reset, watchdog, brownout, NVS or partition error log.
- Final cycle PASS/FAIL and evidence file references.

The updated CORE exposes a random per-boot 32-bit boot identity in the existing /status JSON response as boot_id and logs the same identity at startup. Record the pre-cut and post-reboot values. Confirm this field is present on the exact test SHA before cycle 1; do not substitute a timestamp or reuse an earlier boot's value.

### 5.1 Copyable cycle-log CSV header

```csv
cycle,firmware_sha,build_id,pre_boot_id,post_boot_id,random_seed,transaction_id,session_id,interrupted_phase,interrupted_rail,energized_rails,common_ground_connected,power_cut_ms,controller_input_v,buck_5v_v,led_rail_source_v,led_rail_far_v,led_current_a,ambient_temp_c,esp32_temp_c,buck_temp_c,shifter_temp_c,fuse_temp_c,connector_temp_c,wire_temp_c,strip_temp_c,partition_hash,partition_integrity,nvs_output_cfg,nvs_zone_map,nvs_other_namespaces,active_generation,staged_generation,pair_intent_generation,hardware_revision,map_revision,canonical_crc,config_readback,boot_result,ble_result,safety_arbitration,recovery_time_ms,operator_intervention,cycle_result,evidence_refs
```

## 6. Final PASS criteria

- Exactly 100 controlled cycles completed.
- 100/100 cycles recover without manual repair.
- Zero partition-table corruption.
- Zero unrecoverable NVS corruption.
- Zero invalid active hardware or map configurations.
- Zero mixed-generation hardware/map recovery.
- Every interrupted transaction resolves to the complete previous or complete committed generation.
- Zero silent configuration loss; every result is confirmed by revision, transaction identity, CRC and full readback.
- Zero physical safety arbitration regressions.
- Every cycle has complete electrical, thermal, phase, boot-ID and firmware-SHA evidence.
- All eight durable phases and the pre-RAM-publication boundary have physical interruption coverage.

Any failed or incomplete cycle blocks PASS. Fix the root cause, preserve the failed evidence, reset the acceptance run counter, and repeat a new complete 100-cycle run after review. Do not discard failed cycles or count a repaired/reflashed retry as the original cycle.

## 7. Sign-offs and release rule

- Firmware: exact commit pipeline, phase instrumentation, boot ID, NVS/CRC diagnostics and recovery evidence.
- Hardware: as-built rail drawing, load measurements, supply/fuse/wiring/thermal preflight and interruption safety.
- QA: controlled randomized execution, complete 100-cycle log, independent result review.
- Overseer: verifies all independent sign-offs and confirms the full OpenTail release gates.

This test is mandatory but is not by itself complete product certification. Firmware, Android, Effects, Triggers and Hardware sign-offs remain separate. No team may independently declare OpenTail production-ready.
