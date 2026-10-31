#!/usr/bin/env python3
"""OpenTail exact-SHA bench observation recorder. DOES NOT CERTIFY PHYSICAL TESTS.

Standard library only for GET telemetry; optional 'pyserial' and 'opencv-python'
for UART and camera recording. No write to CORE unless --simulate is deliberately
armed on an isolated bench with LED 12V disconnected. No flash, reset or NVS writes.
"""
from __future__ import annotations

import argparse
import csv
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request

REPO = Path(__file__).resolve().parents[1]
SESSIONS = REPO / "scripts" / "bench-automation-sessions.json"
BASELINE = REPO / "docs" / "CI-REGRESSION-BASELINE.json"
EXACT_FIRMWARE_SHA = "b13894c0550366a0e74ef6fd93c83cfa6dd19aa4"
READ_PATHS = ("/api/v1/status", "/api/v1/triggers", "/api/v1/outputs", "/api/v1/status-led")
MASKS = (0, 1, 2, 3, 12, 14, 15, 16, 31, 224, 255)
SIGNALS = ("park", "brake", "left", "right", "reverse", "aux1", "aux2", "aux3")


def utc():
    return dt.datetime.now(dt.timezone.utc).isoformat()


def load_contract():
    spec = json.loads(SESSIONS.read_text(encoding="utf-8"))
    baseline = json.loads(BASELINE.read_text(encoding="utf-8"))
    source_ids = [c["id"] for c in baseline["physical_cases"]]
    mapped = [c["id"] for c in spec["cases"]]
    assert len(source_ids) == len(mapped) == 22 and set(source_ids) == set(mapped)
    assert len(mapped) == len(set(mapped))
    by_session = {s["id"]: s for s in spec["sessions"]}
    assert len(by_session) == 5
    for case in spec["cases"]:
        assert case["id"] in by_session[case["session"]]["case_ids"]
    assert sorted(y for s in spec["sessions"] for y in s["case_ids"]) == sorted(mapped)
    assert all(c["operator_cue"] and c["additional_dependency"] for c in spec["cases"])
    return spec, baseline


def validate_url(s):
    p = urllib.parse.urlsplit(s.rstrip("/"))
    # Local bench AP/loopback only; prevent accidentally stimulating external hosts.
    if p.scheme != "http" or p.username or p.password or p.query or p.fragment:
        raise ValueError("Bench REST URL must be uncredentialed HTTP")
    if p.hostname not in ("192.168.4.1", "localhost", "127.0.0.1"):
        raise ValueError("Only OpenTail isolated bench AP or loopback is permitted")
    return s.rstrip("/")


def request_json(base, path, body=None):
    assert path in READ_PATHS or path == "/api/v1/simulate"
    payload = None if body is None else json.dumps(body).encode("utf-8")
    assert body is None or path == "/api/v1/simulate"
    req = urllib.request.Request(
        base + path, data=payload,
        headers={"Accept": "application/json", "Content-Type": "application/json"},
        method="POST" if payload is not None else "GET")
    with urllib.request.urlopen(req, timeout=2.5) as response:
        data = response.read(65536)
    return json.loads(data)


def sha256(p):
    d = hashlib.sha256()
    with p.open("rb") as f:
        for block in iter(lambda: f.read(65536), b""):
            d.update(block)
    return d.hexdigest()


def write_line(file, value):
    file.write(json.dumps(value, separators=(",", ":"), default=str) + "\n")
    file.flush()


def serial_worker(port, baud, stop, file, errors):
    try:
        import serial
        with serial.Serial(port, baudrate=baud, timeout=0.3) as s:
            while not stop.is_set():
                raw = s.readline()
                if raw:
                    write_line(file, {"utc": utc(), "line": raw.decode("utf-8", "replace").rstrip()})
    except Exception as exc:
        errors.append(f"Serial capture unavailable: {exc}")


def open_camera(index, out, fps=12):
    import cv2
    cap = cv2.VideoCapture(index)
    if not cap.isOpened():
        cap.release()
        raise RuntimeError("Camera not available; no optical capture created")
    w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH)) or 640
    h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT)) or 480
    writer = cv2.VideoWriter(str(out), cv2.VideoWriter_fourcc(*"mp4v"), fps, (w, h))
    if not writer.isOpened():
        cap.release()
        raise RuntimeError("Camera video writer failed")
    return cap, writer


def capture_window(args, folder, label, telemetry, events):
    start = time.monotonic()
    video_path = folder / (label + ".mp4") if args.camera is not None else None
    cap = writer = None
    frames = 0
    if video_path:
        cap, writer = open_camera(args.camera, video_path)
    write_line(events, {"utc": utc(), "event": "capture_start", "label": label,
                        "type": "telemetry/video only, NOT physical test PASS"})
    try:
        next_query = start
        next_frame = start
        while time.monotonic() - start < args.seconds:
            now = time.monotonic()
            if args.http and now >= next_query:
                for path in READ_PATHS:
                    try:
                        obj = request_json(args.base_url, path)
                        write_line(telemetry, {"utc": utc(), "label": label,
                                               "path": path, "payload": obj})
                    except (OSError, ValueError, urllib.error.URLError) as err:
                        write_line(telemetry, {"utc": utc(), "label": label,
                                               "path": path, "error": str(err)})
                next_query = time.monotonic() + args.interval
            if cap and time.monotonic() >= next_frame:
                ok, frame = cap.read()
                if ok:
                    writer.write(frame)
                    frames += 1
                next_frame = time.monotonic() + 1 / 12
            time.sleep(0.02)
    finally:
        if cap:
            cap.release()
            writer.release()
            if not frames:
                video_path.unlink(missing_ok=True)
        write_line(events, {"utc": utc(), "event": "capture_end", "label": label,
                            "camera_frames": frames, "duration_requested_s": args.seconds})
    return frames


def simulation_preflight(args):
    if not args.simulate:
        return
    if not (args.ack_off_vehicle and args.ack_led_power_off and args.ack_isolated_fixture):
        raise ValueError("Simulation requires all three bench isolation acknowledgements")
    if args.session != "S1" or not args.http:
        raise ValueError("Simulated GPIO masks are S1-only and require REST access")
    status = request_json(args.base_url, "/api/v1/status")
    if not status.get("bench_only") or status.get("hardware_failover") is not False:
        raise ValueError("Expected bench-only CORE with hardware failover unavailable")
    if int(status.get("simulated_mask", -1)) != 0:
        raise ValueError("Pre-existing simulated input active; refusing to overwrite it")
    if int(status.get("physical_function_mask", -1)) != 0:
        raise ValueError("Real vehicle input active; refusing to simulate")
    return status


def run_simulation(args, folder, telemetry, events):
    # Firmware auto-expires each bounded synthetic request. This is NEVER
    # evidence of true optocoupler electrical stimulus or automotive safety.
    armed = False
    try:
        for mask in MASKS:
            signal_names = [name for index, name in enumerate(SIGNALS) if mask & (1 << index)]
            write_line(events, {"utc": utc(), "event": "synthetic_input",
                                "mask": mask, "signals": signal_names,
                                "physical_proof": False})
            request_json(args.base_url, "/api/v1/simulate",
                         {"signals": signal_names, "ttl_ms": 1500})
            armed = True
            capture_window(args, folder, "SIM_%03d" % mask, telemetry, events)
    finally:
        if armed:
            # Explicitly clear synthetic inputs; do not call STOP, which would
            # cancel unrelated local shows/effects.
            try:
                request_json(args.base_url, "/api/v1/simulate",
                             {"signals": [], "ttl_ms": 100})
            except Exception as exc:
                write_line(events, {"utc": utc(), "event": "synthetic_cleanup_error",
                                    "message": str(exc)})


def cli(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list", action="store_true", help="Print all five sessions and their cases; no hardware access")
    parser.add_argument("--self-test", action="store_true", help="Validate exact 22-case mapping; no hardware access")
    parser.add_argument("--session", choices=["S1", "S2", "S3", "S4", "S5"])
    parser.add_argument("--candidate-sha", default=EXACT_FIRMWARE_SHA,
                        help="Declared candidate only; not proof the device is flashed with it")
    parser.add_argument("--base-url", default="http://192.168.4.1")
    parser.add_argument("--http", action="store_true", help="Opt-in read-only status/outputs/triggers/status LED REST capture")
    parser.add_argument("--serial", metavar="COM9", help="Opt-in 115200 UART serial capture (pyserial)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--camera", type=int, metavar="INDEX",
                        help="Opt-in USB webcam video (opencv-python); phone video may be imported instead")
    parser.add_argument("--seconds", type=float, default=4.0, help="Capture duration per case (min 0.2 seconds)")
    parser.add_argument("--interval", type=float, default=0.5, help="HTTP polling interval (min 0.2 seconds)")
    parser.add_argument("--interactive", action="store_true", help="Pause for safe human manipulation before each case")
    parser.add_argument("--simulate", action="store_true",
                        help="S1 software-only temporary synthetic inputs; NEVER a physical case PASS")
    parser.add_argument("--ack-off-vehicle", action="store_true")
    parser.add_argument("--ack-led-power-off", action="store_true")
    parser.add_argument("--ack-isolated-fixture", action="store_true")
    parser.add_argument("--flash-log", type=Path,
                        help="Optional original manual flash receipt; archives its bytes/hash, does NOT verify flashed SHA")
    parser.add_argument("--out", type=Path, default=Path("bench-evidence"))
    args = parser.parse_args(argv)
    spec, baseline = load_contract()
    if args.self_test:
        print("PASS: exact 22-case mapping, five sessions, all operator cues and dependencies present")
        return 0
    if args.list:
        for sess in spec["sessions"]:
            print(f'{sess["id"]}: {sess["title"]}')
            for case in spec["cases"]:
                if case["session"] == sess["id"]:
                    print(f'  {case["id"]}: {case["automation_tier"]}')
        return 0
    if not args.session:
        parser.error("--session required unless --list or --self-test")
    if len(args.candidate_sha) != 40 or any(c not in "0123456789abcdef" for c in args.candidate_sha):
        parser.error("Candidate SHA must be a full 40-character lowercase Git SHA")
    if args.seconds < 0.2 or not 0.2 <= args.interval <= 60:
        parser.error("--seconds >= 0.2 and 0.2 <= --interval <= 60 required")
    if args.simulate and not args.http:
        parser.error("--simulate requires --http")
    if args.simulate and not (args.ack_off_vehicle and args.ack_led_power_off and args.ack_isolated_fixture):
        parser.error("--simulate requires all three explicit isolation acknowledgements")
    if args.simulate and args.session != "S1":
        parser.error("--simulate is permitted for S1 only")
    if not (args.http or args.serial or args.camera is not None):
        parser.error("Select at least one capture source: --http, --serial or --camera")
    args.base_url = validate_url(args.base_url)
    if args.http:
        try:
            status = request_json(args.base_url, "/api/v1/status")
            if status.get("bench_only") is not True:
                parser.error("No bench-only CORE status; refusing this target")
        except Exception as err:
            parser.error("Bench REST preflight failed: " + str(err))
    simulation_preflight(args)
    if args.serial:
        try:
            import serial  # noqa: F401
        except ImportError:
            parser.error("Install pyserial for serial logging or omit --serial")
    if args.camera is not None:
        try:
            import cv2  # noqa: F401
        except ImportError:
            parser.error("Install opencv-python for webcam video or omit --camera")
    folder = args.out / (args.session + "_" + dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ"))
    folder.mkdir(parents=True, exist_ok=False)
    selected = [c for c in spec["cases"] if c["session"] == args.session]
    manifest = {
        "schema": 1, "session": args.session, "started_at": utc(),
        "declared_candidate_sha": args.candidate_sha, "flashed_sha_verified": False,
        "current_sha_physical_certified": False, "software_simulation_is_physical_proof": False,
        "historical_baseline": baseline["physical_baseline"],
        "historical_results_unchanged": True,
        "physical_release_certified": False, "automatic_failover_certified": False,
        "cases": [{"id": c["id"], "status": "UNVERIFIED",
                   "automation_tier": c["automation_tier"]} for c in selected],
        "capture": {"http": args.http, "serial": args.serial or None,
                    "camera": args.camera, "interval_s": args.interval,
                    "duration_s": args.seconds, "simulated": args.simulate},
        "errors": [],
    }
    if args.flash_log:
        if not args.flash_log.is_file():
            parser.error("Flash log path does not exist")
        receipt = folder / ("operator-flash-receipt" + args.flash_log.suffix)
        shutil.copyfile(args.flash_log, receipt)
        manifest["operator_flash_receipt"] = receipt.name
        manifest["operator_flash_receipt_sha256"] = sha256(receipt)
        manifest["receipt_is_not_independent_flash_proof"] = True
    stop = threading.Event()
    errors = manifest["errors"]
    thread = None
    try:
        with (folder / "telemetry.jsonl").open("w", encoding="utf-8") as telemetry, \
             (folder / "events.jsonl").open("w", encoding="utf-8") as events, \
             (folder / "serial.jsonl").open("w", encoding="utf-8") as serial_out:
            if args.serial:
                thread = threading.Thread(target=serial_worker,
                                          args=(args.serial, args.baud, stop, serial_out, errors),
                                          daemon=True)
                thread.start()
            print(f'BENCH-ONLY {args.session}: {len(selected)} cases. ALL remain UNVERIFIED.')
            print("Existing vehicle rear lighting must remain intact; never parallel two MCU data outputs.")
            if args.simulate:
                run_simulation(args, folder, telemetry, events)
            for case in selected:
                print(f'\n{case["id"]}: {case["operator_cue"]}')
                print("Dependency: " + case["additional_dependency"])
                if args.interactive:
                    input("Only after electrically safe setup, press Enter to capture (Ctrl+C to abort): ")
                capture_window(args, folder, case["id"], telemetry, events)
    except (KeyboardInterrupt, Exception) as exc:
        errors.append(repr(exc))
        if isinstance(exc, KeyboardInterrupt):
            print("Capture interrupted; evidence remains UNVERIFIED", file=sys.stderr)
        else:
            print("Capture stopped: " + str(exc), file=sys.stderr)
    finally:
        stop.set()
        if thread:
            thread.join(timeout=2)
        manifest["finished_at"] = utc()
        manifest["output_files"] = {
            p.name: sha256(p) for p in sorted(folder.iterdir()) if p.is_file()
        }
        (folder / "manifest.json").write_text(json.dumps(manifest, indent=2)+"\n",
                                               encoding="utf-8")
    print("Saved raw evidence to:", folder)
    print("NO physical PASS has been recorded. Review measured evidence + actual flashed SHA separately.")
    return 0 if not errors else 2


if __name__ == "__main__":
    raise SystemExit(cli())
