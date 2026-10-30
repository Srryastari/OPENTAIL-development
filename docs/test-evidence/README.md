# OpenTail hardware regression evidence (not an automated hardware pass)

The canonical list of physical acceptance cases is in
`docs/CI-REGRESSION-BASELINE.json`. The older observed passes recorded in
`docs/verified-bench-regression-matrix.md` remain historical and **are not**
hardware evidence for a new firmware commit. Hardware cannot be certified by
GitHub-hosted Linux/Windows jobs without a wired test fixture.

For a candidate to pass the mandatory `release-evidence-gate`:

1. Build and flash the **exact candidate commit** to the isolated CORE bench.
   Keep the baseline software commit separate from the candidate SHA.
2. Run each named physical test on real GPIO4–11 PC817 inputs, GPIO16/17 pixel
   outputs, the 12V strips, BLE phone, status LED, and the OTA/rollback fixture.
   Capture the actual measured values, screenshots/video/logs, operator, and
   hardware revision. Do not infer a physical pass from host tests.
3. Store the evidence at `docs/test-evidence/current-candidate.json`. It must
   contain schema 1, the full 40-character `candidate_sha`, and one entry per
   case in `physical_cases`: `id`, `result: "pass"`, `observer`,
   `hardware_id`, `tested_at` (timezone-aware ISO 8601), and a `proof`
   reference to actual retained test evidence.
4. Record failures and regressions honestly. Missing, failed, duplicated or
   stale-commit cases block release. Never copy passes forward from an earlier
   build; new changes require new evidence.
5. CI checks these documents for completeness, exact candidate SHA, and other
   structural consistency. CI **cannot establish that photos, measurements or
   operator claims are authentic** without instrumented hardware in the loop.

## Sample structure only (not a pass record)

```json
{
  "schema": 1,
  "candidate_sha": "EXACT_40_CHARACTER_COMMIT_SHA",
  "tests": [
    {
      "id": "inputs_gpio_mask",
      "result": "unverified",
      "observer": "",
      "hardware_id": "",
      "tested_at": "",
      "proof": ""
    }
  ]
}
```

Do not add a fabricated file that marks all cases `pass`. In particular,
`ota_usb_partition_migration` must test that NVS and user-authored
universally configurable zones survive the one-time USB partition-table change.
`ota_success_rollback` must cover both good and interrupted firmware updates.
Neither the new OTA code nor GitHub one-command flashing has been physically
validated yet.

**Current state:** no evidence file for v0.1.30. Its release gate is expected
to fail until real tests occur. Software build success is separate from
hardware/release approval.
