#!/usr/bin/env python3
"""Negative-control checks: a regression gate must fail on missing proof."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location("gate",root/"scripts/check_regressions.py")
gate=importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)

class RegressionGateNegativeTests(unittest.TestCase):
    def setUp(self):
        self.config=json.loads((root/"docs/CI-REGRESSION-BASELINE.json").read_text())
        self.sha="a"*40
    def test_unknown_path_must_not_silently_pass(self):
        self.assertEqual(gate.domains_of("future-feature/critical-controller.c"),set())
    def test_firmware_and_ui_are_independently_classified(self):
        self.assertIn("core",gate.domains_of("core-firmware/main/ot_ota_update.c"))
        self.assertIn("update",gate.domains_of("core-firmware/main/ot_ota_update.c"))
        self.assertIn("ui",gate.domains_of("web-studio/opentail.htm"))
        self.assertIn("android",gate.domains_of("android/app/src/main/MainActivity.kt"))
    def test_missing_physical_evidence_blocks_release(self):
        with patch.object(gate,"EVIDENCE",Path("/not-an-opentail-evidence-file.json")):
            issues=gate.physical(self.config,self.sha,{"core","ui","android"})
        self.assertTrue(issues)
    def test_stale_or_fabricated_incomplete_evidence_blocks_release(self):
        with tempfile.TemporaryDirectory() as t:
            proof=Path(t)/"evidence.json"
            proof.write_text(json.dumps({"schema":1,"candidate_sha":"b"*40,
                                         "tests":[{"id":"inputs_gpio_mask","result":"pass",
                                                   "observer":"", "hardware_id":"fixture",
                                                   "tested_at":"yesterday","proof":"TODO"}]}))
            with patch.object(gate,"EVIDENCE",proof):
                issues=gate.physical(self.config,self.sha,{"core","ui","android"})
        self.assertTrue(any("different commit" in x for x in issues))
        self.assertTrue(any("UNVERIFIED" in x for x in issues))
        self.assertTrue(any("missing observer" in x for x in issues))
        self.assertTrue(any("placeholder evidence" in x for x in issues))
    def test_all_historical_test_ids_remain_distinct(self):
        cases=self.config["physical_cases"]
        self.assertGreaterEqual(len(cases),20)
        self.assertEqual(len({c["id"] for c in cases}),len(cases))

if __name__=="__main__":
    unittest.main(verbosity=2)
