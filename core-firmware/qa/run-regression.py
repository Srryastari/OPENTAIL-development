#!/usr/bin/env python3
"""Host-only OpenTail v0.1.24 suite: source contracts, C tests, Studio mocks and embedded HTML."""
from pathlib import Path
import gzip, re, subprocess, tempfile, shutil, sys, os, json
p=Path(__file__).resolve().parents[1]
fw=(p/'main/opentail_core.c').read_text()
html=(p/'opentail-v0.19.51.htm').read_bytes()
text=html.decode()
checks=[]
def check(condition, description):
    if not condition:raise AssertionError(description)
    checks.append(description)
    print('PASS:',description)
check((p/'opentail.htm').read_bytes()==html, 'Stable and versioned Studio copies identical')
check(gzip.decompress((p/'main/assets/studio.htm.gz').read_bytes())==html, 'Embedded Studio matches standalone HTML byte-for-byte')
check("const OT_VERSION='0.19.51'" in text and 'v0.19.51</title>' in text,'Studio version and title updated')
check('#define OT_FW "0.1.24-alpha"' in fw, 'CORE firmware version bumped')
check('"trigger_manager.c"' in (p/'main/CMakeLists.txt').read_text(),'New Trigger Manager included in ESP-IDF build')
check('OT_OUT_LEFT_GPIO 16' in (p/'main/board_config.h').read_text() and 'OT_OUT_RIGHT_GPIO 17' in (p/'main/board_config.h').read_text(),'Previously verified GPIO16 and GPIO17 unchanged')
check('OT_INPUT_GPIO_LIST {4,5,6,7,8,9,10,11}' in (p/'main/board_config.h').read_text(),'Physical input pin mapping unchanged')
check('#define OT_OM_VERSION 2' in (p/'main/output_manager.h').read_text(),'Output Manager NVS v2 kept unchanged')
check('"profile_v2"' in fw and 'ot_om_store_v1_t' in fw and 'if(!om_save(next_all))' in fw,'Output Manager persistence, migration and commit-before-mutate preserved')
check('OT_TM_VERSION 1' in (p/'main/trigger_manager.h').read_text() and '"ot_trig_cfg"' in fw and '"profile_v1"' in fw,'Independent versioned trigger NVS profile')
check('if(!tm_save(&next))return respond_err' in fw and 'tm_profile=next' in fw,'Trigger changes applied only after successful commit')
check('ot_tm_resolve(&local_tm,physical)' in fw and 'ot_tm_resolve(&tm,live_input_mask)' in fw,'Physical-to-function mapping used in render and output diagnostic path')
check('ot_tm_resolve(&tm,physical)' in fw and '"physical_function_mask"' in fw,'Function mask exposed in status/diagnostics')
check('"/api/v1/triggers"' in fw and 'trigger_manager_v1' in fw,'Trigger GET/POST routes and capability advertised')
check('"/api/v1/outputs"' in fw and 'output_manager_v1' in fw,'Existing Output Manager endpoints and capabilities retained')
check('ot_st_show_active(native_rendering)' in fw and 'ot_ble_start()' in fw,'Show status indicator and BLE status startup retained')
check('otTMValidResponse' in text and 'otTMSaveState=\'failed\'' in text and 'verify.saved_on_device' in text,'Trigger Studio rejects wrong revisions and unverified saves')
check('otOMAddZone()' in text and 'otOMRemoveZone(' in text and 'Readback did not match saved output map' in text,'Previously verified dynamic-zone UI retained')
check('CONFIG_BTDM_CTRL_MODE_BLE_ONLY' not in (p/'sdkconfig.defaults').read_text(),'Unknown ESP-IDF 5.5.5 BLE config symbol removed')
check('CONFIG_OPENTAIL_AP_PASSWORD' not in (p/'sdkconfig.defaults').read_text(),'No Wi-Fi secret included in distributable SDK defaults')
check('"ble_command.c"' in (p/'main/CMakeLists.txt').read_text(), 'BLE protocol parser compiled by ESP-IDF')
check('"/api/v1/ble/session"' in fw and 'ble_session_get' in fw, 'Ephemeral authorization endpoint available only via CORE Wi-Fi HTTP server')
check('ble_owns_show=false' in fw and 'if(ble_owns_show)' in fw and 'ot_ble_stop_show' in fw,'Wi-Fi show ownership preserved when BLE session ends')
ble=(p/'main/ble_status.c').read_text()
check('ot_ble_parse(' in ble and 'BLE_GATT_CHR_F_WRITE' in ble and 'BLE_GATT_CHR_F_READ' in ble,'BLE GATT supports authorized bench-only show commands')
check('esp_fill_random(token,sizeof(token))' in ble and 'authorized=false' in ble and 'ot_ble_stop_show()' in ble,'Volatile session token and disconnect revocation')
check('"profile_v2"' in fw and '"profile_v1"' in fw,'Existing output and trigger NVS schemas retained')
# Existing suite + new test compiled with the host C compiler
if not shutil.which('cc') or not shutil.which('sh'):
    raise RuntimeError('Cannot claim C regression pass: host C compiler and sh required')
c=subprocess.run(['sh',str(p/'tests/run-host-tests.sh')],cwd=p,capture_output=True,text=True)
print(c.stdout,end='')
if c.returncode: raise RuntimeError('C test failed: '+c.stderr)
checks.append('Six C host test suites')
with tempfile.TemporaryDirectory() as out:
    exe=str(Path(out)/'ble_test')
    cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(p/'main'),'-o',exe,str(p/'tests/test_ble_command.c'),str(p/'main/ble_command.c')]
    cp=subprocess.run(cmd, capture_output=True,text=True)
    if cp.returncode:raise RuntimeError('BLE protocol C compilation failed: '+cp.stderr)
    cp=subprocess.run([exe],capture_output=True,text=True)
    if cp.returncode:raise RuntimeError('BLE protocol assertions failed: '+cp.stderr)
    checks.append('BLE parser/auth/range/disconnect host C tests')
# BLE binary wire format/CRC and 522-pixel zone validation.
with tempfile.TemporaryDirectory() as out:
    exe=str(Path(out)/'ble_cfg_test')
    cp=subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(p/'main'),'-o',exe,
          str(p/'tests/test_ble_config.c'),str(p/'main/ble_config.c'),str(p/'main/output_manager.c'),
          str(p/'main/core_logic.c'),str(p/'main/arbitration_logic.c')],capture_output=True,text=True)
    if cp.returncode:raise RuntimeError('BLE config codec build failed: '+cp.stderr)
    cp=subprocess.run([exe],capture_output=True,text=True)
    if cp.returncode:raise RuntimeError('BLE config codec regression failed: '+cp.stderr)
    print(cp.stdout,end='');checks.append('BLE config binary codec 522-pixel/CRC validation')
check('ble_output_config_v1' in fw and 'ot_ble_config_save(' in fw and 'om_revision[index]++' in fw,'BLE output save bridge and stale-revision prevention enabled')
check('cfg_received' in ble and 'cfg_upload_crc' in ble and 'cfg_reset()' in ble,'BLE chunk receipt, checksum, and disconnect resets enabled')
check('gpio_get_level(GPIO_NUM_0)!=0' in ble and 'owner_create()' in ble and 'ot_blepair' in ble,
      'Physical BOOT enrollment gate and separate persistent key namespace')
check('mbedtls_md_hmac' in ble and 'challenge_ready=false' in ble and 'pair_failures>=5' in ble,
      'Owner challenge HMAC, one-time use and rate limit present')
check('ot_enroll_uuid' in ble and 'ot_challenge_uuid' in ble and 'm bedtls'.replace(' ','') in (p/'main/CMakeLists.txt').read_text(),
      'BLE enrollment/challenge characteristics and crypto component compiled')
check('if(n==18 && buf[0]==1 && buf[1]==4)' in ble and 'ot_ble_parse(buf,n,token,&authorized,&show)' in ble,
      'New HMAC authorization preserves legacy Wi-Fi-authenticated show protocol')
# Entire Studio JS block syntax, including the previously large WLED editor
if not shutil.which('node'):raise RuntimeError('Cannot claim Studio regression pass: Node required')
scripts=re.findall(r'<script(?:\s+[^>]*)?>(.*?)</script>',text,re.I|re.S)
with tempfile.NamedTemporaryFile(suffix='.js',mode='w',delete=False) as f:
    f.write('\n;\n'.join(scripts)); tmp=f.name
try:
    syntax=subprocess.run(['node','--check',tmp],capture_output=True,text=True)
    if syntax.returncode: raise RuntimeError('Studio syntax: '+syntax.stderr)
finally:os.unlink(tmp)
checks.append('Full Studio JavaScript syntax')
for name in ['studio-output-regression.mjs','studio-trigger-regression.mjs','studio-preview-trigger-regression.mjs']:
    t=subprocess.run(['node',str(p/'qa'/name),str(p)],capture_output=True,text=True)
    print(t.stdout,end='')
    if t.returncode:raise RuntimeError(name+': '+t.stderr)
    checks.append(name)
# C JSON capability literal is valid JSON; prevents previously discovered escape corruption.
m=re.search(r'static esp_err_t caps_get\(.*?return send_json\(r,("(?:[^"\\]|\\.)*")\);',fw,re.S)
if not m:raise AssertionError('No parsable C capabilities JSON literal')
import ast
cap=json.loads(ast.literal_eval(m.group(1)))
check(cap['outputs']==2 and cap['physical_inputs']==8 and cap['trigger_manager_v1'] and cap['output_manager_v1'] and cap['bench_only'] and not cap['hardware_failover'],'Capabilities JSON valid and bench-only hardware limits correct')
report=f'HOST QA PASSED: {len(checks)} gates. ESP-IDF 5.5.5 cross-build and physical electrical/hardware tests NOT run in this environment.\n'+'\n'.join('PASS: '+c for c in checks)+'\n'
(p/'REGRESSION-RESULTS.txt').write_text(report)
print(report.splitlines()[0])
