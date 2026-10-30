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
ble=(root/"main/ble_status.c").read_text(encoding="utf-8")
cmake=(root/"main/CMakeLists.txt").read_text(encoding="utf-8")
assert len(page)>5000 and page.lstrip().startswith("<!doctype html>")
assert "EMBED_TXTFILES \"assets/dev_console.htm\"" in cmake
assert "studio.htm.gz" not in cmake
assert "web-studio" not in cmake or "independent" in cmake
assert "studio_gz_start" not in core and "_binary_dev_console_htm_start" in core
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
assert 'ot_ble_apply_fx_internal(&req,false,&spark)' in core, "Auto preview must use transient spark accents"
assert 'ot_ble_apply_fx_internal(&req,false,NULL)' in core, "Regular effects must retain default accents"
assert 'fx_bank.zones[o][z].spark=*spark' in core, "Spark style must remain volatile, not saved NVS"
for control in ("eventSparkStyle","eventSparkPeak","eventSparkColor"):
    assert control in page, f"Developer console sparkle control missing: {control}"
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
# Trigger editor must expose all eight simulatable functions and a live-edit
# revision token. The BLE path must gate changes on owner authorization.
for function in ("aux1","aux2","aux3"):
    assert f'value="{function}"' in page, f"Missing {function} simulation control"
assert "expected_config_revision:triggerSnapshot.config_revision" in page
# Preserve the user's earlier successful 15-second trigger simulation. The
# demo endpoint keeps its separate 10-second policy; no physical input is
# cleared by a simulated-input expiry, Stop, or BLE disconnect.
assert "#define OT_SIM_MAX_MS 15000u" in core
simulate = core.split("static esp_err_t simulate_post(httpd_req_t *r) {",1)[1].split(
    "static esp_err_t stop_post(",1)[0]
assert 'bounded_int(obj,"ttl_ms",2000,100,OT_SIM_MAX_MS)' in simulate
assert 'ot_tm_signal_mask_of(v->valuestring)' in simulate
assert 'sim_mask=mask;' in simulate and 'sim_deadline=mask?' in simulate
assert 'live_input_mask=' not in simulate
assert 'id="simTtl" type="number" min="100" max="15000"' in page
assert "integer('simTtl',100,15000)" in page
demo = core.split("static esp_err_t demo_post(httpd_req_t *r) {",1)[1].split(
    "static esp_err_t simulate_post(",1)[0]
assert 'bounded_int(obj,"ttl_ms",3000,100,OT_DEMO_MAX_MS)' in demo
stop = core.split("static esp_err_t stop_post(httpd_req_t *r) {",1)[1].split(
    "extern const uint8_t ot_dev_console_start[]",1)[0]
assert 'sim_mask=0' in stop and 'live_input_mask=0' not in stop
disconnect = core.split("void ot_ble_disconnect_show(void){",1)[1].split(
    "static esp_err_t status_get(",1)[0]
assert 'live_input_mask=0' not in disconnect
assert 'input_sampler_task' in core and 'vTaskDelayUntil(&last_wake,period)' in core

# Raw optocoupler assertion must veto edits even before the four 10ms
# debounce samples settle. This is only a pre-save guard, not an atomic
# promise about new pin edges during a flash write.
raw = core.split("static uint8_t tm_gpio_input_mask(void){",1)[1].split(
    "/* Serialize authenticated BLE",1)[0]
assert "gpio_get_level(input_pin[i])" in raw
assert "OT_INPUT_ACTIVE_LOW?!high:high" in raw
assert "mask|=(uint8_t)(1u<<i)" in raw
commit = core.split("static int tm_commit(",1)[1].split(
    "bool ot_ble_trigger_get(",1)[0]
assert "const bool raw_active=tm_gpio_input_mask()!=0;" in commit
assert "const bool busy=om_deferred.pending || tm_deferred.pending || raw_active || live_input_mask!=0" in commit
assert commit.index("tm_gpio_input_mask()") < commit.index("portENTER_CRITICAL(&state_lock)")
assert commit.index("const bool busy=om_deferred.pending") < commit.index("tm_save(next)")
assert "if(!tm_save(next))code=4" in commit
assert commit.index("if(!tm_save(next))code=4") < commit.index("ot_tm_saved_apply(")
assert "const uint8_t pins_after=tm_gpio_input_mask();" in commit
assert "ot_tm_saved_apply(&tm_profile,&tm_deferred,next," in commit
assert "xSemaphoreGive(om_write_mutex)" in commit
sampler = core.split("static void input_sampler_task(void *arg){",1)[1].split("static void frame_wake_callback(",1)[0]
assert "ot_tm_apply_if_released(&tm_profile,&tm_deferred," in sampler
assert "instantaneous,physical" in sampler
# Editing output zone roles during a pre-debounce asserted 12V input can
# change which pixels will respond to that trigger. Both output-save APIs
# must use the same instantaneous-pin admission gate as trigger mappings.
output_http = core.split("static esp_err_t om_post(",1)[1].split(
    "bool ot_ble_config_get(",1)[0]
output_ble = core.split("int ot_ble_config_save(",1)[1].split(
    "/* Normalized instantaneous GPIO levels",1)[0]
for save_path in (output_http, output_ble):
    assert "const bool raw_active=tm_gpio_input_mask()!=0;" in save_path
    assert "bool busy=om_deferred.pending || tm_deferred.pending || raw_active || live_input_mask ||" in save_path
    assert save_path.index("tm_gpio_input_mask()") < save_path.index(
        "portENTER_CRITICAL(&state_lock)")
    assert save_path.index("bool busy=om_deferred.pending") < save_path.index("om_save(")
    assert save_path.index("om_save(") < save_path.index("ot_om_saved_apply(")
    assert "const uint8_t pins_after=tm_gpio_input_mask();" in save_path
# The original REST raw_input_mask remains the settled input mask; a new,
# separately named field exposes pre-debounce normalized GPIO diagnostic bits.
get = core.split("static esp_err_t tm_get(httpd_req_t *r){",1)[1].split(
    "static esp_err_t tm_post(",1)[0]
assert 'cJSON_AddBoolToObject(root,"config_apply_pending",pending)' in get
assert "ot_om_apply_if_released(om_outputs,&om_deferred," in sampler
assert 'cJSON_AddBoolToObject(obj,"config_apply_pending",config_pending)' in core
assert 'cJSON_AddNumberToObject(root,"raw_input_mask",physical)' in get
assert 'cJSON_AddNumberToObject(root,"gpio_input_mask",tm_gpio_input_mask())' in get
assert 'cJSON_AddNumberToObject(root,"physical_function_mask",ot_tm_resolve(&cfg,physical))' in get
assert 'cJSON_AddNumberToObject(root,"effective_mask",ot_tm_resolve(&cfg,physical)|sim)' in get
assert 'cJSON_AddNumberToObject(root,"config_revision",config_revision)' in core
assert 'tm_commit(&next,(uint16_t)expected_config_revision,require_revision)' in core
assert "ot_tm_signal_mask_of(v->valuestring)" in core
assert "ot_tm_ble_decode(packet,n,&revision,&next)" in ble
assert "conn!=active_conn||!authorized" in ble
assert ".uuid=&ot_trigger_uuid.u,.access_cb=trigger_access" in ble
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
