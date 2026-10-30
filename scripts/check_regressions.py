#!/usr/bin/env python3
"""Diff-aware software coverage and exact-candidate physical release gates."""
import argparse
import datetime as dt
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
CONFIG=ROOT/"docs/CI-REGRESSION-BASELINE.json"
EVIDENCE=ROOT/"docs/test-evidence/current-candidate.json"

# Every modified area is classified. New unclassified areas fail, not skip.
TESTS={
 "core":("core-host-tests","core-espidf-build"),
 "render":("core-host-tests","core-espidf-build"),
 "update":("core-espidf-build","github-flash-package-check"),
 "ui":("web-studio-check",),
 "android":("android-debug-apk",),
 "reference":("core-host-tests",),
 "infrastructure":("change-impact-audit","github-flash-package-check"),
 "documentation":("change-impact-audit",)
}
REQUIRED={
 "core":["core-firmware/tests/test_verified_bench_contract.c",
         "core-firmware/tests/test_arbitration_logic.c",
         "core-firmware/tests/test_status_logic.c",
         "core-firmware/tests/test_output_manager.c",
         "core-firmware/tests/test_render_quality.c"],
 "render":["core-firmware/tests/test_proprietary_effects.c",
           "core-firmware/tests/check_effects_release.py"],
 "update":["scripts/flash-from-github.ps1","core-firmware/partitions_ota.csv",
           "core-firmware/tests/test_ota_contract.py"],
 "ui":["web-studio/tests/approved-chat-ui.test.cjs",
       "web-studio/tests/exact-live-indexed.test.cjs"],
 "android":["android/qa/check_release.py"],
 "infrastructure":["scripts/check_regressions.py",".github/workflows/verify-active-sources.yml"],
 "reference":["core-firmware/tests/test_wled_catalog.py"],
 "documentation":["docs/verified-bench-regression-matrix.md"]
}
def git(*args):
    p=subprocess.run(["git","-C",str(ROOT),*args],capture_output=True,text=True)
    if p.returncode: raise ValueError("git error: "+p.stderr.strip())
    return p.stdout.strip()

def domains_of(path):
    groups=set()
    if path.startswith("core-firmware/"):
        groups.add("core")
        if any(w in path.lower() for w in ("ota","partition","sdkconfig","cmakelists")):
            groups.add("update")
        if any(w in path.lower() for w in ("fx_","effect","render","wled","particle","palette","transition")):
            groups.add("render")
    if path.startswith("web-studio/"): groups.add("ui")
    if path.startswith("android/"): groups.add("android")
    if path.startswith("wled-native/"): groups.add("reference")
    if path.startswith("scripts/") or path.startswith(".github/"):
        groups.add("infrastructure")
        if any(w in path for w in ("flash","release")): groups.add("update")
    if path.startswith("docs/") or path.endswith(".md"): groups.add("documentation")
    return groups

def report(message):
    print(message)
    if os.getenv("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"],"a",encoding="utf8") as f:
            f.write(message+"\n")

def impact(config,head):
    base=config["software_baseline"]["commit"]
    if not re.fullmatch("[0-9a-f]{40}",base) or not re.fullmatch("[0-9a-f]{40}",head):
        raise ValueError("Expected full verified Git commit SHAs")
    paths=git("diff","--name-only",base+".."+head).splitlines()
    if not paths: raise ValueError("No candidate changes against software baseline")
    unknown=[p for p in paths if not domains_of(p)]
    if unknown: raise ValueError("Unclassified changes: "+", ".join(unknown))
    groups={"core","ui","android","infrastructure"}
    for path in paths: groups.update(domains_of(path))
    workflow=(ROOT/".github/workflows/verify-active-sources.yml").read_text()
    problems=[]
    # Require deliberate test maintenance for each product surface changed
    # since the last approved baseline; untested source edits are not green.
    changes={
        "core": any(p.startswith("core-firmware/main/") or
                    p in ("core-firmware/sdkconfig.defaults","core-firmware/partitions_ota.csv")
                    for p in paths),
        "ui": any(p.startswith("web-studio/") and "/tests/" not in p for p in paths),
        "android": any(p.startswith("android/") and "/qa/" not in p for p in paths),
    }
    probes={
        "core": any(p.startswith("core-firmware/tests/") for p in paths),
        "ui": any(p.startswith("web-studio/tests/") for p in paths),
        "android": any(p.startswith("android/qa/") for p in paths),
    }
    for product,modified in changes.items():
        if modified and not probes[product]:
            problems.append("Changed "+product+" product behavior with no changed product regression test")

    for g in sorted(groups):
        for job in TESTS[g]:
            if not re.search(r"(?m)^  "+re.escape(job)+r":\s*$",workflow):
                problems.append("CI job missing: "+job+" (for "+g+")")
        for f in REQUIRED[g]:
            if not (ROOT/f).is_file(): problems.append("Test asset missing: "+f)
    if not re.search(r"(?m)^  release-evidence-gate:\s*$",workflow):
        problems.append("Mandatory physical release evidence gate missing")
    cases=config["physical_cases"]
    if len(cases)<20 or len({c["id"] for c in cases})!=len(cases):
        problems.append("Historical physical test matrix missing or duplicate IDs")
    report("### OpenTail regression impact: "+base[:12]+" -> "+head[:12])
    report("Changed paths: "+str(len(paths))+"; mapped domains: "+", ".join(sorted(groups)))
    for path in paths:
        report("- "+path+" => "+", ".join(sorted(domains_of(path))))
    return groups,problems

def physical(config,head,groups):
    cases=[c for c in config["physical_cases"] if c["domain"]=="core" or c["domain"] in groups]
    if not EVIDENCE.is_file():
        return ["No physical test evidence for exact candidate "+head+": "+str(len(cases))+" checks UNVERIFIED"]
    try: data=json.loads(EVIDENCE.read_text())
    except (OSError,ValueError) as e: return ["Unreadable physical evidence: "+str(e)]
    problems=[]
    if data.get("candidate_sha")!=head: problems.append("Physical evidence for different commit")
    if data.get("schema")!=1: problems.append("Unsupported physical evidence schema")
    entries=data.get("tests",[])
    if not isinstance(entries,list): return ["Physical evidence tests not a list"]
    indexed={}
    for e in entries:
        if not isinstance(e,dict) or not isinstance(e.get("id"),str):
            problems.append("Malformed test evidence")
            continue
        if e["id"] in indexed: problems.append("Duplicated physical case "+e["id"])
        indexed[e["id"]]=e
    for c in cases:
        entry=indexed.get(c["id"])
        if not entry:
            problems.append("UNVERIFIED physical "+c["id"]+": "+c["description"])
            continue
        if entry.get("result")!="pass":
            problems.append("NOT PASSED physical "+c["id"])
        for field in ("observer","hardware_id","proof","tested_at"):
            if not isinstance(entry.get(field),str) or not entry[field].strip():
                problems.append(c["id"]+" missing "+field)
        proof=entry.get("proof","")
        if isinstance(proof,str) and proof.lower().startswith(("todo","placeholder","n/a")):
            problems.append(c["id"]+" contains placeholder evidence")
        try:
            timestamp=dt.datetime.fromisoformat(entry["tested_at"].replace("Z","+00:00"))
            if timestamp.tzinfo is None:
                problems.append(c["id"]+" needs timezone-aware timestamp")
        except (KeyError,ValueError,AttributeError):
            problems.append(c["id"]+" has invalid test date")
    report("Hardware checks required: "+str(len(cases))+"; submitted: "+str(len(indexed)))
    return problems

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--mode",required=True,choices=["audit","release"])
    parser.add_argument("--candidate-sha",default=os.getenv("OPENTAIL_CANDIDATE_SHA"))
    args=parser.parse_args()
    try:
        config=json.loads(CONFIG.read_text())
        head=args.candidate_sha or git("rev-parse","HEAD")
        groups,problems=impact(config,head)
        if args.mode=="release":
            problems.extend(physical(config,head,groups))
            if "core" in groups:
                parity=subprocess.run([sys.executable,"tests/check_effects_release.py","--release"],
                  cwd=ROOT/"core-firmware",capture_output=True,text=True)
                if parity.returncode:
                    problems.append("Borrowed effect source parity release gate failed: "+
                                    (parity.stdout+parity.stderr)[-1000:])
        for p in problems: report("BLOCKED: "+p)
        if problems: return 1
        report("PASS: "+args.mode+" gate; no untested changes promoted")
        return 0
    except (ValueError,KeyError,OSError) as e:
        report("BLOCKED: "+str(e))
        return 1

if __name__=="__main__":
    sys.exit(main())
