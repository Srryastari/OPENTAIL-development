#!/usr/bin/env python3
"""The exact 22 release-evidence IDs must be executable and remain unverified."""
import csv
import json
from pathlib import Path

root=Path(__file__).resolve().parents[2]
cfg=json.loads((root/"docs/CI-REGRESSION-BASELINE.json").read_text())
cases=cfg["physical_cases"]
assert len(cases)==22
with (root/"docs/FAILOVER-22-PHYSICAL-ACCEPTANCE.csv").open(newline="") as file:
    rows=list(csv.DictReader(file))
assert len(rows)==22
assert {r["case_id"] for r in rows}=={c["id"] for c in cases}
assert all(r["status"]=="NOT_RUN" for r in rows)
for row in rows:
    assert row["executable_method"].strip(), row["case_id"]
    assert row["precondition"].strip(), row["case_id"]
    assert not any(row[field] for field in
                   ("measured_values","evidence_uri","observer","hardware_id","tested_at"))
# Hardware-only Rev B H01–H30 are supplemental and cannot satisfy these cases.
print("PASS: exactly 22 mandatory hardware acceptance cases, all NOT_RUN; no fabricated evidence")
