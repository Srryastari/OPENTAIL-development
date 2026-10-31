#!/usr/bin/env python3
"""Independent regression of evidence trust boundaries and real main failure paths."""
import importlib.util
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

spec=importlib.util.spec_from_file_location("lock",Path(__file__).with_name("final_day_evidence_lock.py"))
lock=importlib.util.module_from_spec(spec);spec.loader.exec_module(lock)

class EvidenceRegression(unittest.TestCase):
    def test_existing_contracts(self):
        lock.self_test()
    def test_real_main_replaces_stale_success_on_remote_failures(self):
        cases=[None,{},{"head":None},OSError("timeout")]
        for response in cases:
            with self.subTest(response=response),tempfile.TemporaryDirectory() as tmp:
                out=Path(tmp)/"receipt.json"
                out.write_text(json.dumps({"software_evidence":"PINNED_TARGETED_CI_PASS"}))
                def api(*args):
                    if isinstance(response,Exception):raise response
                    return response
                with patch.object(sys,"argv",["lock","--output",str(out)]),patch.dict(os.environ,{"GITHUB_TOKEN":"test-fixture"}),patch.object(lock,"api_json",side_effect=api):
                    with self.assertRaises(SystemExit):lock.main()
                self.assertEqual("BLOCKED",json.loads(out.read_text())["software_evidence"])
    def test_missing_token_invalidates_stale_success(self):
        with tempfile.TemporaryDirectory() as tmp:
            out=Path(tmp)/"receipt.json"
            out.write_text('{"software_evidence":"PINNED_TARGETED_CI_PASS"}')
            with patch.object(sys,"argv",["lock","--output",str(out)]),patch.dict(os.environ,{},clear=True):
                with self.assertRaises(SystemExit):lock.main()
            self.assertEqual("BLOCKED",json.loads(out.read_text())["software_evidence"])

if __name__=="__main__":unittest.main()
