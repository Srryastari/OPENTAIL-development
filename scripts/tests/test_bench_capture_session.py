"""Source-only tests: NO serial, Wi-Fi, camera, electrical or OTA activity."""
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from contextlib import redirect_stdout
from unittest.mock import patch

SRC = Path(__file__).resolve().parents[1] / "bench_capture_session.py"
spec = importlib.util.spec_from_file_location("bench_capture_session", SRC)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class SessionContractTests(unittest.TestCase):
    def test_all_22_exact_mandatory_ids(self):
        mapping, baseline = mod.load_contract()
        self.assertEqual(len(mapping["cases"]), 22)
        self.assertEqual(len(mapping["sessions"]), 5)
        self.assertEqual({c["id"] for c in mapping["cases"]},
                         {c["id"] for c in baseline["physical_cases"]})
        self.assertEqual(sorted(c["id"] for c in mapping["cases"]),
                         sorted(x for s in mapping["sessions"] for x in s["case_ids"]))

    def test_no_extra_network_hosts(self):
        for addr in ("https://192.168.4.1", "http://open-tail.example",
                     "http://192.168.4.2", "http://192.168.4.1@evil.example",
                     "http://192.168.4.1?evil=1"):
            with self.assertRaises(ValueError, msg=addr):
                mod.validate_url(addr)
        self.assertEqual(mod.validate_url("http://192.168.4.1/"), "http://192.168.4.1")

    def test_post_only_allowed_to_simulation(self):
        with self.assertRaises(AssertionError):
            mod.request_json("http://localhost", "/api/v1/ota", {"x": 1})
        with self.assertRaises(AssertionError):
            mod.request_json("http://localhost", "/api/v1/status", {"x": 1})

    def test_explicit_simulation_isolated_and_physical_idle(self):
        class A:
            simulate = True
            ack_off_vehicle = ack_led_power_off = ack_isolated_fixture = True
            session = "S1"
            http = True
            base_url = "http://192.168.4.1"
        with patch.object(mod, "request_json", return_value={
            "bench_only": True, "hardware_failover": False,
            "simulated_mask": 0, "physical_function_mask": 0}):
            self.assertIsNotNone(mod.simulation_preflight(A()))
        for invalid in (
            {"simulated_mask": 1, "physical_function_mask": 0},
            {"simulated_mask": 0, "physical_function_mask": 2},
            {"simulated_mask": 0, "physical_function_mask": 0,
             "hardware_failover": True}
        ):
            with patch.object(mod, "request_json", return_value={
                "bench_only": True, "hardware_failover": False, **invalid}):
                with self.assertRaises(ValueError):
                    mod.simulation_preflight(A())
        A.ack_led_power_off = False
        with self.assertRaises(ValueError):
            mod.simulation_preflight(A())

    def test_sim_mask_mapping_including_hazards_and_aux(self):
        self.assertEqual(mod.MASKS, (0, 1, 2, 3, 12, 14, 15, 16, 31, 224, 255))
        self.assertEqual([s for i, s in enumerate(mod.SIGNALS) if 14 & (1 << i)],
                         ["brake", "left", "right"])

    def test_dry_run_does_not_touch_hardware(self):
        with patch.object(mod, "request_json", side_effect=AssertionError("Network called")):
            with redirect_stdout(io.StringIO()) as result:
                self.assertEqual(mod.cli(["--self-test"]), 0)
                self.assertIn("22-case", result.getvalue())
            with redirect_stdout(io.StringIO()) as result:
                self.assertEqual(mod.cli(["--list"]), 0)
                self.assertIn("S5:", result.getvalue())

    def test_physical_ledger_original_is_not_edited(self):
        original = mod.REPO / "docs" / "FAILOVER-22-PHYSICAL-ACCEPTANCE.csv"
        self.assertTrue(original.exists())
        rows = original.read_text(encoding="utf-8").splitlines()
        self.assertEqual(len(rows), 23)
        self.assertEqual(sum('"NOT_RUN"' in row for row in rows), 22)

    def test_no_unsafe_device_mutation_routes(self):
        code = SRC.read_text(encoding="utf-8")
        for url in ("/api/v1/ota", "/api/v1/outputs\"",
                    "/api/v1/triggers\"", "/api/v1/stop\""):
            self.assertNotIn('request_json(args.base_url, "' + url, code)
        self.assertIn('"physical_release_certified": False', code)
        self.assertIn('"automatic_failover_certified": False', code)
        self.assertIn('"flashed_sha_verified": False', code)


if __name__ == "__main__":
    unittest.main()
