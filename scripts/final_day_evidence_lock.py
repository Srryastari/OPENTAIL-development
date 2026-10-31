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
import tempfile
from urllib.parse import urlencode
from urllib.request import Request, urlopen

REPO = "Srryastari/OPENTAIL-development"
ROOT = Path(__file__).resolve().parents[1]
# (pull request, exact PR head, exact workflow display name, workflow_id, workflow file)
# Workflow IDs are immutable identities, not merely descriptive workflow names.
TARGETS = {
    "android": (87, "449462c72f47baac2c66e56296d1166202b6174a",
                "Android exact-head software integration", 401400070,
                ".github/workflows/android-exact-head.yml"),
    "effects": (80, "7c0fcc450a7619a57603aabf833dea855a18bcd8",
                "Effects proprietary continuity exact HEAD (software only)",
                402793679, ".github/workflows/verify-effects-proprietary-continuity.yml"),
    "triggers": (83, "0477355fdbd562cfe357569bf8f15d0e08374e8f",
                 "Trigger CORE81 pinned-source safety evidence", 402818745,
                 ".github/workflows/verify-trigger-core81-pinned.yml"),
    "firmware": (84, "b13894c0550366a0e74ef6fd93c83cfa6dd19aa4",
                 "CORE backup essential policy exact-head software gates",
                 402824423, ".github/workflows/verify-core-failover-policy.yml"),
}
# The Trigger workflow itself checks out CORE #81 instead of #83's CI-only tip.
TRIGGER_TESTED_CORE_SHA = "d3ca7d036392d7709848b143a41f6bed1a26cb7b"

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
    pr_number, exact_sha, required_name, workflow_id, workflow_path = TARGETS[name]
    result = {
        "pr": pr_number,
        "pinned_head_sha": exact_sha,
        "observed_pr_head_sha": actual_pr_sha,
        "required_workflow": required_name,
        "required_workflow_id": workflow_id,
        "required_workflow_path": workflow_path,
        "result": "BLOCKED",
        "run_id": None,
        "run_url": None,
    }
    if name == "triggers":
        result["pinned_tested_core_sha"] = TRIGGER_TESTED_CORE_SHA
    if actual_pr_sha != exact_sha or not isinstance(runs, list):
        return result

    # Candidate ordering must include unsuccessful, cancelled and unfinished
    # attempts. Never search for an older successful run as fallback.
    candidates = [
        r for r in runs if isinstance(r, dict)
        and r.get("head_sha") == exact_sha
        and r.get("event") == "pull_request"
        and r.get("name") == required_name
    ]
    if not candidates:
        return result
    # Unordered or malformed matching run records cannot authorize an older PASS.
    if any(type(r.get("run_number")) is not int or r["run_number"] < 1
           for r in candidates):
        return result
    newest = max(candidates, key=lambda r: r["run_number"])
    run_id = newest.get("id")
    expected_url = (
        f"https://github.com/{REPO}/actions/runs/{run_id}"
        if type(run_id) is int and run_id > 0 else None
    )
    prs = newest.get("pull_requests")
    correct_pr = isinstance(prs, list) and any(
        isinstance(p, dict)
        and p.get("number") == pr_number
        and isinstance(p.get("head"), dict)
        and p["head"].get("sha") == exact_sha
        for p in prs
    )
    # The workflow ID and canonical path prevent another same-name workflow
    # or another PR sharing this SHA from forging a successful receipt.
    valid = (
        correct_pr
        and newest.get("workflow_id") == workflow_id
        and newest.get("path") == workflow_path
        and isinstance(newest.get("repository"), dict)
        and newest["repository"].get("full_name") == REPO
        and isinstance(newest.get("head_repository"), dict)
        and newest["head_repository"].get("full_name") == REPO
        and type(run_id) is int and run_id > 0
        and newest.get("html_url") == expected_url
        and type(newest.get("run_attempt")) is int
        and newest["run_attempt"] >= 1
        and newest.get("status") == "completed"
        and newest.get("conclusion") == "success"
    )
    if valid:
        result.update(result="PASS", run_id=run_id, run_url=expected_url,
                      run_attempt=newest["run_attempt"],
                      workflow_id=workflow_id)
    return result


def api_json(path, token):
    url = "https://api.github.com" + path
    req = Request(url, headers={"Accept": "application/vnd.github+json",
         "Authorization": "Bearer " + token, "X-GitHub-Api-Version": "2022-11-28",
         "User-Agent": "OpenTail-software-evidence-gate"})
    with urlopen(req, timeout=30) as resp:
        return json.load(resp)


def verify_remote(token):
    result = {}
    for team, (number, head, _name, workflow_id, _path) in TARGETS.items():
        pr = api_json("/repos/" + REPO + "/pulls/" + str(number), token)
        if not isinstance(pr, dict) or not isinstance(pr.get("head"), dict):
            raise ValueError(f"Malformed PR response for #{number}")
        observed = pr["head"].get("sha", "")
        if pr.get("number") != number or pr.get("merged") or pr.get("state") != "open":
            observed = "INVALID_STATE:" + str(observed)
        # Query the exact workflow identity, not the repository-wide run list.
        params = urlencode({"head_sha": head, "event": "pull_request",
                            "per_page": 100})
        path = f"/repos/{REPO}/actions/workflows/{workflow_id}/runs?{params}"
        payload = api_json(path, token)
        if not isinstance(payload, dict) or not isinstance(payload.get("workflow_runs"), list):
            raise ValueError(f"Malformed workflow runs for #{number}")
        result[team] = evaluate_target(team, observed, payload["workflow_runs"])
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


def blocked_receipt(reason):
    return {
        "schema": 1, "repository": REPO,
        "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "software_evidence": "BLOCKED",
        "software_contracts": {},
        "error": str(reason)[:500],
        "combined_flash_and_app_on_device": "NOT_CERTIFIED",
        "physical": {"case_count": 22, "evidenced_pass": 0,
                     "hardware_failover_certified": False, "vehicle_release": "BLOCKED"},
        "release_approval": False, "auto_merge": False,
    }


def write_json_atomic(path, data):
    # A new receipt replaces an explicitly blocked placeholder only once the
    # whole verification is complete; interruption leaves a BLOCKED receipt.
    path.parent.mkdir(parents=True, exist_ok=True)
    temp_name = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8",
                                          dir=path.parent, prefix=".opentail-evidence-",
                                          delete=False) as handle:
            temp_name = handle.name
            json.dump(data, handle, indent=2, sort_keys=True)
            handle.write("\n")
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temp_name, path)
    finally:
        if temp_name and os.path.exists(temp_name):
            os.unlink(temp_name)


def self_test():
    sha = TARGETS["android"][1]
    name = TARGETS["android"][2]
    wf_id = TARGETS["android"][3]
    wf_path = TARGETS["android"][4]
    def run(number=2, *, id=9, status="completed", conclusion="success"):
        return {
            "name": name, "head_sha": sha, "event": "pull_request",
            "workflow_id": wf_id, "path": wf_path,
            "repository": {"full_name": REPO},
            "head_repository": {"full_name": REPO},
            "pull_requests": [{"number": 87, "head": {"sha": sha}}],
            "id": id, "html_url": f"https://github.com/{REPO}/actions/runs/{id}",
            "run_number": number, "run_attempt": 1,
            "status": status, "conclusion": conclusion,
        }
    assert evaluate_target("android", sha, [run()])["result"] == "PASS"
    assert evaluate_target("android", "synthetic_merge_sha", [run()])["result"] == "BLOCKED"
    for status, conclusion in (("completed", "failure"),
                               ("completed", "cancelled"),
                               ("in_progress", None), ("queued", None)):
        assert evaluate_target("android", sha, [
            run(number=1, id=7), run(number=3, id=10, status=status,
                                     conclusion=conclusion)])["result"] == "BLOCKED"
    for field, altered in (
        ("workflow_id", 99999),
        ("path", ".github/workflows/unrelated.yml"),
        ("pull_requests", [{"number": 999, "head": {"sha": sha}}]),
        ("id", None),
        ("html_url", None),
        ("html_url", "https://example.com/forged"),
        ("run_attempt", None),
        ("repository", {"full_name": "attacker/other"}),
        ("head_sha", "synthetic_merge_sha"),
    ):
        assert evaluate_target("android", sha, [{**run(), field: altered}])["result"] == "BLOCKED", field
    assert evaluate_target("android", sha, [{**run(), "run_number": None}, run(number=1, id=7)])["result"] == "BLOCKED"
    assert evaluate_target("android", sha, [run(number=1, id=7), {**run(number=4, id=8), "html_url": None}])["result"] == "BLOCKED"

    # Reusing a successful receipt is prohibited even for remote exceptions.
    with tempfile.TemporaryDirectory() as tmp:
        output = Path(tmp) / "receipt.json"
        write_json_atomic(output, {"software_evidence": "PINNED_TARGETED_CI_PASS"})
        output.unlink()
        write_json_atomic(output, blocked_receipt("verification pending"))
        try:
            raise ValueError("API response head null")
        except ValueError as exc:
            write_json_atomic(output, blocked_receipt(exc))
        assert json.loads(output.read_text())["software_evidence"] == "BLOCKED"
    rows = physical_rows()
    assert physical_gate(rows)["vehicle_release"] == "BLOCKED"
    assert receipt({"android": evaluate_target("android", sha, [run()])}, rows)["release_approval"] is False
    print("PASS: newest-run veto, PR/workflow/SHA/URL provenance, stale receipt invalidation and 22 physical gates")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--output", default="software-evidence-lock.json")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    output = Path(args.output)
    # Invalidate the previous success BEFORE reading token, local files or
    # invoking APIs. Fail closed even on null/malformed REST data or timeouts.
    output.unlink(missing_ok=True)
    write_json_atomic(output, blocked_receipt("verification in progress"))
    try:
        token = os.environ.get("GITHUB_TOKEN")
        if not token:
            raise ValueError("GITHUB_TOKEN required for independent GitHub verification")
        rows = physical_rows()
        checks = verify_remote(token)
        report = receipt(checks, rows)
        write_json_atomic(output, report)
        if report["software_evidence"] != "PINNED_TARGETED_CI_PASS":
            raise ValueError("missing, stale or failed exact-head CI evidence")
        print(json.dumps({
            "software_evidence": report["software_evidence"],
            "release_approval": report["release_approval"],
            "physical_cases_passed": report["physical"]["evidenced_pass"],
            "evidence_file": str(output),
        }, indent=2))
    except Exception as exc:
        write_json_atomic(output, blocked_receipt(exc))
        sys.exit(f"BLOCKED: {exc}")



if __name__ == "__main__":
    main()
