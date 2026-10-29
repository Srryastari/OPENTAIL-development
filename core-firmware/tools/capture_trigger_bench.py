#!/usr/bin/env python3
"""Capture live CORE trigger readback for physical acceptance (not a simulator).

Run on the PC connected to the CORE bench Wi-Fi:
    python core-firmware/tools/capture_trigger_bench.py --seconds 120

While recording, apply/remove the physical inputs on the verified isolated
opto board and optionally save a trigger/output configuration through the
authorized developer controls. The tool never writes to CORE or to NVS.
HTTP polling cannot prove optical LED latency or safe isolation voltage.
"""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import time
from urllib.error import URLError
from urllib.request import Request, urlopen

SINGLES = (1, 2, 4, 8, 16, 32, 64, 128)
HISTORICAL = (12, 14, 15, 30, 31, 224, 255)


def utc_now():
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds")


def get_json(url, timeout):
    req = Request(url, headers={"User-Agent": "OpenTail-Trigger-Bench-Capture/1"})
    with urlopen(req, timeout=timeout) as response:
        doc = json.load(response)
    if not isinstance(doc, dict):
        raise ValueError("CORE returned non-object JSON")
    return doc


def mask(doc, key):
    val = doc.get(key)
    if type(val) is not int or not 0 <= val <= 255:
        raise ValueError(f"CORE /api/v1/triggers missing valid {key}: {val!r}")
    return val


def run(base_url, seconds, interval_ms, out, timeout):
    base_url = base_url.rstrip("/")
    # Validate connection and schema BEFORE creating any evidence file.
    first = get_json(base_url + "/api/v1/triggers", timeout)
    mask(first, "raw_input_mask")
    mask(first, "gpio_input_mask")
    mask(first, "effective_mask")
    if "physical_function_mask" not in first:
        raise ValueError("CORE must report physical_function_mask")
    status = get_json(base_url + "/api/v1/status", timeout)
    if status.get("bench_only") is not True:
        raise ValueError("Expected an explicitly bench-only OpenTail CORE")
    out.parent.mkdir(parents=True, exist_ok=True)
    seen_raw, seen_settled, seen_effective = set(), set(), set()
    pending_samples = 0
    counts = {"readings": 0, "errors": 0}
    start = time.monotonic()
    deadline = start + seconds
    next_poll = start
    print(f"Capturing physical trigger diagnostics: {out}")
    print("Apply/release your isolated IN1–IN8 inputs during this recording. Ctrl+C ends early.")
    try:
        with out.open("x", encoding="utf-8") as f:
            f.write(json.dumps({"type": "session", "timestamp": utc_now(),
                "status_version": status.get("version"), "bench_only": True,
                "source_url": base_url, "requested_seconds": seconds,
                "poll_interval_ms": interval_ms,
                "limitations": "HTTP samples; no optical latency/isolation proof",
                "trigger_config_revision": first.get("config_revision")}) + "\n")
            while time.monotonic() < deadline:
                now = time.monotonic()
                if now < next_poll:
                    time.sleep(min(next_poll - now, 0.05))
                    continue
                next_poll = now + interval_ms / 1000.0
                stamp = utc_now()
                start_request = time.monotonic()
                try:
                    doc = get_json(base_url + "/api/v1/triggers", timeout)
                    raw = mask(doc, "gpio_input_mask")
                    settled = mask(doc, "raw_input_mask")
                    effective = mask(doc, "effective_mask")
                    resolved = mask(doc, "physical_function_mask")
                    seen_raw.add(raw)
                    seen_settled.add(settled)
                    seen_effective.add(effective)
                    pending = doc.get("config_apply_pending") is True
                    pending_samples += int(pending)
                    record = {"type": "sample", "utc": stamp,
                        "elapsed_ms": round((start_request-start)*1000, 3),
                        "request_ms": round((time.monotonic()-start_request)*1000, 3),
                        "gpio_mask": raw, "debounced_mask": settled,
                        "physical_function_mask": resolved,
                        "effective_mask": effective,
                        "config_revision": doc.get("config_revision"),
                        "config_apply_pending": pending,
                        "saved_on_device": doc.get("saved_on_device")}
                    counts["readings"] += 1
                except (OSError, ValueError, json.JSONDecodeError, URLError) as e:
                    counts["errors"] += 1
                    record = {"type": "error", "utc": stamp,
                              "elapsed_ms": round((time.monotonic()-start)*1000, 3),
                              "error": str(e)}
                f.write(json.dumps(record) + "\n")
                f.flush()
    except KeyboardInterrupt:
        print("Capture ended by user.")
    elapsed = time.monotonic() - start
    singles_seen = [x for x in SINGLES if x in seen_raw and x in seen_settled]
    summary = {"duration_seconds": round(elapsed, 2), **counts,
        "instantaneous_masks_observed": sorted(seen_raw),
        "debounced_masks_observed": sorted(seen_settled),
        "effective_masks_observed": sorted(seen_effective),
        "individual_inputs_observed_raw_and_debounced": singles_seen,
        "historic_combination_masks_observed": [x for x in HISTORICAL if x in seen_settled],
        "pending_configuration_samples": pending_samples,
        "electrical_optical_acceptance": "NOT VERIFIED BY THIS CAPTURE"}
    with out.open("a", encoding="utf-8") as f:
        f.write(json.dumps({"type": "summary", **summary}) + "\n")
    print(json.dumps(summary, indent=2))
    print("The capture documents readback only: verify optical pixels, PC817 levels and physical timing separately.")
    return 0 if counts["readings"] else 2


def main():
    parser = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--url", default="http://192.168.4.1")
    parser.add_argument("--seconds", type=int, default=120)
    parser.add_argument("--interval-ms", type=int, default=100)
    parser.add_argument("--timeout", type=float, default=2.0)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if not 1 <= args.seconds <= 1800:
        parser.error("--seconds must be 1..1800")
    if not 20 <= args.interval_ms <= 5000:
        parser.error("--interval-ms must be 20..5000")
    if not 0.1 <= args.timeout <= 30:
        parser.error("--timeout must be 0.1..30 seconds")
    if args.output is None:
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        args.output = Path(f"opentail-trigger-bench-{stamp}.jsonl")
    try:
        raise SystemExit(run(args.url, args.seconds, args.interval_ms,
                             args.output, args.timeout))
    except (OSError, ValueError, URLError) as e:
        parser.exit(2, f"No verified bench capture: {e}\n")


if __name__ == "__main__":
    main()
