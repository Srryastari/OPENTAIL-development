#!/usr/bin/env python3
"""Read-only, fail-closed OpenTail exact-SHA software evidence freeze.

This verifies independent GitHub Actions results for pinned team candidates and
produces a reviewable JSON receipt. It NEVER certifies physical automotive use,
unlocks release gates, merges PRs, or labels unrun measurements as PASS.
"""
import argparse
import csv
import datetime as dt
import json
import os
from pathlib import Path
import sys
from urllib.parse import urlencode
from urllib.request import Request, urlopen

REPO = "Srryastari/OPENTAIL-development"
ROOT = Path(__file__).resolve().parents[1]
TARGETS = {
    "android": (86, "5615bade61542f30a4a9c4375e2cadea30de2105",
                "Android exact-head software integration"),
    "effects": (80, "7c0fcc450a7619a57603aabf833dea855a18bcd8",
                "Effects proprietary continuity exact HEAD (software only)"),
    "triggers": (83, "0477355fdbd562cfe357569bf8f15d0e08374e8f",
                 "Trigger CORE81 pinned-source safety evidence"),
    "firmware": (84, "b13894c0550366a0e74ef6fd93c83cfa6dd19aa4",
                 "CORE backup essential policy exact-head software gates"),
}
SESSIONS = {
    "isolated-input-and-timing": (
        "inputs_gpio_mask", "brake_park_priority", "hazards_turn_priority",
        "reverse_overlay", "show_resume_phase", "pixel_chain_independence",
        "universal_zones", "color_order_brightness", "stop_restore",
        "strobe_edges",
    ),
    "instrumented-pixel-and-effects": (
        "full_1246_dual_lane", "status_led_patterns", "render_fps_low_brightness",
        "event_horizon_shutdown", "effect_source_parity",
    ),
    "config-ble-ota-faults": (
        "nvs_v1_v2_persistence", "ble_auth_reconnect",
        "ota_usb_partition_migration", "ota_success_rollback",
        "ota_authorization",
    ),
    "owner-ui-observation": ("studio_interactive_ui", "android_native_ui"),
}
REQUIRED_MEASURED_FIELDS = ("measured_values", "evidence_uri", "observer",
                            "hardware_id", "tested_at")


def physical_rows():
    baseline = json.loads((ROOT / "docs/CI-REGRESSION-BASELINE.json").read_text())
    ids = [c["id"] for c in baseline["physical_cases"]]
    with (ROOT / "docs/FAILOVER-22-PHYSICAL-ACCEPTANCE.csv").open(newline="") as f:
        rows = list(csv.DictReader(f))
    actual = [r["case_id"] for r in rows]
    if len(ids) != 22 or len(rows) != 22 or set(ids) != set(actual):
        raise ValueError("Mandatory 22-case inventory does not match approved baseline")
    grouped = [id for group in SESSIONS.values() for id in group]
    if len(grouped) != 22 or set(grouped) != set(ids):
        raise ValueError("Instrumented bench sessions do not cover 22 distinct cases")
    for row in rows:
        if not row["precondition"].strip() or not row["executable_method"].strip():
            raise ValueError("Empty bench procedure: " + row["case_id"])
        if row["status"] not in {"NOT_RUN", "BLOCKED", "PASS", "FAIL"}:
            raise ValueError("Invalid evidence state: " + row["case_id"])
        if row["status"] == "PASS" and not all(row[k].strip() for k in REQUIRED_MEASURED_FIELDS):
            raise ValueError("PASS without measured evidence: " + row["case_id"])
    return rows


def physical_gate(rows):
    passed = [r["case_id"] for r in rows if r["status"] == "PASS"]
    # No automated green light based on CSV alone: failover electrical proof,
    # independent output arbiter, luminous fallback and owner approval are
    # outside this tool and still absent in the current Rev B prototype.
    return {
        "case_count": len(rows), "evidenced_pass": len(passed),
        "not_passed": [r["case_id"] for r in rows if r["status"] != "PASS"],
        "session_groups": {name: list(ids) for name, ids in SESSIONS.items()},
        "hardware_failover_certified": False,
        "vehicle_release": "BLOCKED",
    }


def evaluate_target(name, actual_pr_sha, runs):
    pr_number, exact_sha, required_workflow = TARGETS[name]
    matching = [r for r in runs if r.get("name") == required_workflow
                and r.get("head_sha") == exact_sha
                and r.get("event") == "pull_request"
                and r.get("status") == "completed"
                and r.get("conclusion") == "success"]
    # A synthetic PR merge cannot masquerade as a candidate head.
    ok = actual_pr_sha == exact_sha and bool(matching)
    matched = max(matching, key=lambda r: r.get("run_number", 0)) if matching else None
    return {
        "pr": pr_number, "pinned_head_sha": exact_sha,
        "observed_pr_head_sha": actual_pr_sha,
        "required_workflow": required_workflow,
        "result": "PASS" if ok else "BLOCKED",
        "run_id": matched.get("id") if ok else None,
        "run_url": matched.get("html_url") if ok else None,
    }


def api_json(path, token):
    url = "https://api.github.com" + path
    req = Request(url, headers={"Accept": "application/vnd.github+json",
         "Authorization": "Bearer " + token, "X-GitHub-Api-Version": "2022-11-28",
         "User-Agent": "OpenTail-software-evidence-gate"})
    with urlopen(req, timeout=30) as resp:
        return json.load(resp)


def verify_remote(token):
    result = {}
    for team, (number, head, _workflow) in TARGETS.items():
        pr = api_json("/repos/" + REPO + "/pulls/" + str(number), token)
        observed = pr.get("head", {}).get("sha", "")
        if pr.get("merged") or pr.get("state") != "open":
            observed = "INVALID_STATE:" + observed
        params = urlencode({"head_sha": head, "event": "pull_request", "per_page": 100})
        runs = api_json("/repos/" + REPO + "/actions/runs?" + params, token)["workflow_runs"]
        result[team] = evaluate_target(team, observed, runs)
    return result


def receipt(checks, rows):
    software_passed = len(checks) == len(TARGETS) and all(
        x["result"] == "PASS" for x in checks.values())
    return {
        "schema": 1, "repository": REPO,
        "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "software_evidence": (
            "PINNED_TARGETED_CI_PASS" if software_passed else "BLOCKED"),
        "software_contracts": checks,
        "combined_flash_and_app_on_device": "NOT_CERTIFIED",
        "physical": physical_gate(rows),
        "release_approval": False,
        "auto_merge": False,
        "interpretation": (
            "Pins passing independent software workflows only. Does not prove a "
            "single combined device release, measured failover, pixel output, "
            "physical BLE/OTA, visual UI equivalence or automotive safety."
        ),
    }


def self_test():
    good_sha = TARGETS["android"][1]
    name = TARGETS["android"][2]
    test_run = [{"name": name, "head_sha": good_sha, "status": "completed",
                 "conclusion": "success", "event": "pull_request",
                 "id": 9, "html_url": "https://github.com/example", "run_number": 2}]
    assert evaluate_target("android", good_sha, test_run)["result"] == "PASS"
    assert evaluate_target("android", "synthetic_merge_sha", test_run)["result"] == "BLOCKED"
    assert evaluate_target("android", good_sha, [{**test_run[0], "conclusion": "failure"}])["result"] == "BLOCKED"
    assert evaluate_target("android", good_sha, [{**test_run[0], "head_sha": "merge"}])["result"] == "BLOCKED"
    assert evaluate_target("android", good_sha, [{**test_run[0], "status": "in_progress"}])["result"] == "BLOCKED"
    rows = physical_rows()
    assert physical_gate(rows)["vehicle_release"] == "BLOCKED"
    assert not receipt({"android": evaluate_target("android", good_sha, test_run)}, rows)["release_approval"]
    print("PASS: exact-head synthetic-merge rejection, incomplete CI rejection, 22-case coverage and vehicle release veto")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--output", default="software-evidence-lock.json")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    token = os.environ.get("GITHUB_TOKEN")
    if not token:
        raise SystemExit("BLOCKED: GITHUB_TOKEN required for independent GitHub verification")
    rows = physical_rows()
    checks = verify_remote(token)
    report = receipt(checks, rows)
    Path(args.output).write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    print(json.dumps({
        "software_evidence": report["software_evidence"],
        "release_approval": report["release_approval"],
        "physical_cases_passed": report["physical"]["evidenced_pass"],
        "evidence_file": args.output,
    }, indent=2))
    if report["software_evidence"] != "PINNED_TARGETED_CI_PASS":
        sys.exit("BLOCKED: missing, stale, or failed exact-head CI evidence")


if __name__ == "__main__":
    main()
