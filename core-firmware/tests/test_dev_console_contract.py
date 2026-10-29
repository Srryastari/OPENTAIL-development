#!/usr/bin/env python3
"""Structural regression for CORE-owned developer dashboard and Studio isolation.

These checks validate source contracts and JavaScript syntax. They do not
exercise the live HTTP server or LEDs; ESP-IDF and physical tests are separate.
"""
from pathlib import Path
from html.parser import HTMLParser
import re
import subprocess

root=Path(__file__).resolve().parents[1]
page=(root/"main/assets/dev_console.htm").read_text(encoding="utf-8")
core=(root/"main/opentail_core.c").read_text(encoding="utf-8")
cmake=(root/"main/CMakeLists.txt").read_text(encoding="utf-8")
assert len(page)>5000 and page.lstrip().startswith("<!doctype html>")
assert "EMBED_TXTFILES \"assets/dev_console.htm\"" in cmake
assert "studio.htm.gz" not in cmake
assert "web-studio" not in cmake or "independent" in cmake
assert "studio_gz_start" not in core and "_binary_assets_dev_console_htm_start" in core
assert 'cfg.max_uri_handlers=30' in core
for route in ["/dev", "/api/v1/status", "/api/v1/status-led",
              "/api/v1/status-led/self-test", "/api/v1/status-led/session", "/api/v1/capabilities",
              "/api/v1/ble/session", "/api/v1/fx/catalog",
              "/api/v1/fx/preview", "/api/v1/auto-events/preview",
              "/api/v1/show/start", "/api/v1/show/stop", "/api/v1/demo",
              "/api/v1/simulate", "/api/v1/stop", "/api/v1/outputs",
              "/api/v1/triggers"]:
    assert ('"'+route+'"') in core, f"CORE route missing: {route}"
    assert route in page, f"Developer dashboard missing operation: {route}"
assert 'studio_embedded\\":false' in core
assert 'developer_console\\":true' in core
assert 'ot_ble_apply_fx_internal(&req,false)' in core
assert 'ot_fx_bank_apply_configured(&fx_bank' in core
assert 'ot_fx_bank_stop_all(&fx_bank)' in core
assert '"/studio"' in core and 'legacy_dev_redirect' in core
for test in ["materialize_sparks","random_materialize",
             "smooth_fade","plasma_sweep","smooth_fade_out"]:
    assert test in page and test in core, f"Auto event preview missing {test}"
assert 'full chip erase' not in page.lower()
assert "No validated safety failover" in page
assert "confirm('Save persistent output" in page
assert "confirm('Save IN1" in page
assert "confirm('Display the temporary BLE enrollment token?" in page
assert "fetch('/api/v1/ble/session')" not in page, "Never read BLE enrollment token automatically"
assert "const data=await request('/api/v1/fx/catalog')" in page
assert "catalog.some(f=>f.id===id)" in page
assert "const data={effect,chain:" in page
assert "if(!document.hidden)refreshStatus()" in page
assert "async function nativeShow()" in page and "async function demo()" in page
assert "async function sessionLed(connected)" in page
assert "Studio remains a separate animation/sequence design application" in page

class Parser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids=set()
    def handle_starttag(self,tag,attrs):
        for name,value in attrs:
            if name=="id":
                assert value not in self.ids, f"Duplicate developer page ID {value}"
                self.ids.add(value)
parser=Parser()
parser.feed(page)
for required in ["overview","lighting","effects","events","inputs","outputs","statusled","api",
                 "eventColor","fxId","fxZones","triggerEditor","outputEditor",
                 "ledJson","apiResult","tokenResult","simResult"]:
    assert required in parser.ids, f"Developer dashboard missing control: {required}"

scripts=re.findall(r"<script[^>]*>(.*?)</script>",page,re.S|re.I)
assert len(scripts)==1, "Keep self-contained local developer page with exactly one script"
check=subprocess.run(["node","--check","-"],input=scripts[0],text=True,capture_output=True)
assert check.returncode==0, f"Developer script syntax error: {check.stderr}"

# Release files must be decoupled from firmware packaging, but existing
# reviewed web Studio and Android source parity remains independently checked.
workflow=(root.parent/".github/workflows/verify-active-sources.yml").read_text()
assert "python3 core-firmware/tools/sync_studio_asset.py" not in workflow
assert re.search(r"opentail-core-v0\.1\.\d+-alpha-app\.bin",workflow), "CORE artifact must have independent firmware version"
assert "studio-v0.19.118-app.bin" not in workflow
assert 'node --test web-studio/tests/*.test.cjs' in workflow

print("CORE /dev dashboard, all bench API routes, source isolation, JS syntax, and separate Studio release gates passed.")
