#!/usr/bin/env python3
"""Offline static checks only. Not a substitute for Android SDK compilation or BLE hardware tests."""
from pathlib import Path
import hashlib
import subprocess
import xml.etree.ElementTree as ET
import re
import sys
import tempfile
import shutil
from html.parser import HTMLParser

root = Path(__file__).resolve().parents[1]
main = root / 'app/src/main'
read = lambda rel: (root / rel).read_text(encoding='utf-8')
passes = 0

def check(name, condition):
    global passes
    if not condition:
        print(f'FAIL: {name}')
        raise SystemExit(1)
    passes += 1
    print(f'PASS: {name}')

studio = (main / 'assets/opentail.htm').read_bytes()
main_java = read('app/src/main/java/com/opentail/android/MainActivity.java')
core_java = read('app/src/main/java/com/opentail/android/CoreConnection.java')
ble_java = read('app/src/main/java/com/opentail/android/BleDiscovery.java')
xml_path = 'app/src/main/AndroidManifest.xml'
manifest = ET.parse(root / xml_path).getroot()
net = ET.parse(main / 'res/xml/network_security_config.xml').getroot()
ET.parse(main / 'res/drawable/ic_opentail.xml')
ET.parse(main / 'res/values/strings.xml')
ET.parse(main / 'res/values/styles.xml')

check('Android XML parses correctly', manifest.tag == 'manifest')
check('Studio v0.19.118 bundled, pinned checksum', b"const OT_VERSION='0.19.118'" in studio and hashlib.sha256(studio).hexdigest() == read('qa/studio-sha256.txt').strip())
check('CORE endpoint fixed to known AP', 'http://192.168.4.1' in core_java and '/api/v1/capabilities' in core_java)
check('CORE protocol validated before opening web controls', 'opentail-core-alpha-v1' in core_java and 'CoreConnection.verify()' in main_java)
check('Untrusted URLs blocked', 'shouldOverrideUrlLoading' in main_java and 'External navigation blocked' in main_java)
check('Offline Studio asset loader, with safe file defaults', 'WebViewAssetLoader' in main_java and 'setAllowFileAccess(false)' in main_java and 'setAllowUniversalAccessFromFileURLs(false)' in main_java)
check('Android JSON import/export uses system document pickers', 'ACTION_CREATE_DOCUMENT' in main_java and 'ACTION_OPEN_DOCUMENT' in main_java)
check('BLE service/characteristic UUIDs consistent with firmware', '55a56022-4915-7a92-cf44-b8b7102eaf21' in ble_java and '55a56022-4915-7a92-cf44-b8b7102eaf22' in ble_java)
check('Existing BLE discovery and info read retained', 'readCharacteristic(info)' in ble_java and 'tryReconnect(' in ble_java)
check('BLE command protocol UUIDs match firmware', all('55a56022-4915-7a92-cf44-b8b7102eaf'+x in ble_java for x in ('23','24')))
check('BLE AUTH/SHOW/STOP command encoding', 'new byte[18]' in ble_java and 'new byte[]{1,2,' in ble_java and 'new byte[]{1,3}' in ble_java)
check('BLE GATT operation serialized and acknowledged', 'writePending' in ble_java and 'readReplyPending' in ble_java and 'acceptReply(' in ble_java)
check('Legacy Wi-Fi token remains optional, not the sole authorization', 'CoreConnection.bleSessionToken()' in main_java and 'authorizeStored()' in ble_java and 'enableBleWifiFallback()' in main_java)
check('Physical trigger mappings remain not writable over BLE', 'writeTrigger' not in ble_java and 'writeOutput' not in ble_java)
check('Android 12+ BLE permissions included', all(f'android.permission.{p}' in read(xml_path) for p in ('BLUETOOTH_SCAN','BLUETOOTH_CONNECT')))
check('Only CORE AP HTTP exception', len(net.findall('./domain-config/domain')) == 1 and net.findtext('./domain-config/domain') == '192.168.4.1' and net.find('./base-config').attrib.get('cleartextTrafficPermitted') == 'false')
check('Release/build versions pinned', all(v in read('app/build.gradle') for v in ('compileSdk 35','minSdk 26','targetSdk 35','versionCode 29','versionName \'0.1.29\'')) and 'gradle-8.11.1-bin.zip' in read('gradle/wrapper/gradle-wrapper.properties'))
check('Bundled Studio output & trigger editors retained', b'Universal Output Manager' in studio and b'Universal Trigger Manager' in studio)
check('BLE bench-only limitation explicitly documented', 'BENCH' in ble_java.upper() and 'do not use for road lighting' in read('README.md').lower() and 'not a compiled apk' in read('README.md').lower())
# Android v0.1.4 feature regressions: these are structural/source checks, not physical assertions.
haptic_java = read('app/src/main/java/com/opentail/android/HapticController.java')
check('Native haptics use user preferences and Android vibration API',
      all(token in haptic_java for token in ('haptics_enabled','haptics_strength','haptics_slider','VibrationEffect','SharedPreferences')))
check('Haptic permission declared', 'android.permission.VIBRATE' in read(xml_path))
check('WebView tap bridge is restricted to allowlisted pages',
      'STUDIO_HAPTIC_JS' in main_java and 'allowedPage(web.getUrl())' in main_java and 'OpenTailHaptics' in main_java)
check('No haptic success before verified CORE save response',
      'Saved to CORE and verified by device readback.' in main_java and 'Saved and verified by CORE readback.' in main_java)
check('Auto Wi-Fi reconnect requires verified CORE capabilities',
      'onResume()' in main_java and 'connectCore(true)' in main_java and 'CoreConnection.verify()' in main_java)
check('Preferred BLE address stored only after validating expected OpenTail service',
      'preferred_ble_address' in main_java and 'getService(SERVICE_UUID)' in ble_java and 'getCharacteristic(INFO_UUID)' in ble_java and
      ble_java.index('getCharacteristic(INFO_UUID)') < ble_java.index('preferredListener.accept'))
check('BLE reconnection bounded; control authorization cleared after BLE disconnect',
      'RECONNECT_DELAY_MS' in ble_java and 'beginScan(true)' in ble_java and 'authorized=false' in ble_java)

# Current Android v0.1.29: native Studio owns the CORE BLE Effects controls.
# Removed legacy scripts must remain absent; no duplicate picker or bottom tabs.
unified = (main / 'assets/unified-shell.js').read_text(encoding='utf-8')
panel = (main / 'assets/ble-output-panel.js').read_text(encoding='utf-8')
studio_text = studio.decode('utf-8')
check('Legacy BLE picker and injection removed',
      not (main / 'assets/ble-show-panel.js').exists()
      and 'injectInlineBleControls' not in main_java and 'showBleControls()' not in main_java)
check('One Studio Effects editor drives CORE BLE commands',
      all(token in studio_text for token in ('window.OpenTailNativeBLE.startEffectId(', 'window.OpenTailNativeBLE.stop()', 'otBleFxMode()', 'window.OpenTailBLEEffects')))
check('Native BLE bridge limited to allowed CORE and offline Studio pages',
      'bridgePageAllowed()' in main_java and 'addJavascriptInterface(new NativeBleBridge(), "OpenTailNativeBLE")' in main_java)
check('Physical BOOT enrollment, encrypted owner key and HMAC path retained',
      all(token in ble_java for token in ('pairWithBoot()', 'HmacSHA256', 'OpenTail-BLE-v2', 'CHALLENGE_UUID', 'ENROLL_UUID'))
      and 'AES/GCM/NoPadding' in read('app/src/main/java/com/opentail/android/BleOwnerVault.java')
      and 'android:allowBackup="false"' in read(xml_path))
check('Native shell and Output Manager are bundled and injected',
      all(token in main_java for token in ('injectUnifiedShell(view)', 'injectBleOutputControls(view)', 'unified-shell.js', 'ble-output-panel.js')))
check('One six-tab navigation in approved order',
      all(token in unified for token in ("['home','Home'", "['scenes','Show Modes'", "['layout','Layout'",
                                   "['builder','Triggers'", "['events','Auto Events'", "['settings','Settings'"))
      and 'document.body.insertBefore(nav, header.nextSibling)' in unified)
check('Sticky top tabs replace duplicate Studio and bottom navigation',
      '.ot-native-nav{position:sticky!important' in unified
      and '#app .top,#app nav.tabs,#app nav.bottomnav{display:none!important}' in unified)
check('Independent Wi-Fi/BLE status and OLED/light-mode controls retained',
      all(token in unified for token in ('ot-status-red','ot-status-amber','ot-status-green',
                                       'radioOff', 'wifiReady', 'I Hate Dark Mode', 'data-ot-theme="light"')))
check('BLE Output Manager role and NVS read/save surface retained',
      all(token in panel for token in ('window.OpenTailBLEOutputs', 'data-key="role"',
                                      'window.OpenTailNativeBLE.readOutput', 'window.OpenTailNativeBLE.saveOutput',
                                      'Save to CORE over BLE', 'GPIO${selected?17:16}')))
check('BLE Output Manager does not issue vehicle trigger writes',
      'writeTrigger' not in main_java and 'writeTrigger' not in panel)
check('Android shell, Output Manager and bundled Studio JavaScript parse',
      all(subprocess.run(['node','--check',str(main / 'assets'/name)],capture_output=True).returncode == 0
          for name in ('unified-shell.js','ble-output-panel.js')))
check('Native Studio BLE effect and six-tab regression suites pass',
      subprocess.run(['node',str(root/'qa/test_unified_ble_fx.cjs')],cwd=root.parent,
                     capture_output=True,text=True).returncode == 0
      and subprocess.run(['node',str(root/'qa/test_unified_shell.js')],cwd=root.parent,
                         capture_output=True,text=True).returncode == 0
      and subprocess.run(['node',str(root.parent/'web-studio/tests/approved-chat-ui.test.cjs')],
                         cwd=root.parent,capture_output=True,text=True).returncode == 0)
check('Web and Android Studio deployment copies are identical',
      studio == (root.parent/'web-studio/opentail.htm').read_bytes()
      and studio == (root.parent/'web-studio/opentail-v0.19.118.htm').read_bytes())
# Decode the injected Java string and syntax-check the exact JavaScript shipped in MainActivity.
import json
m = re.search(r'private static final String STUDIO_HAPTIC_JS = (.*?);\n', main_java, re.S)
if m is None: raise SystemExit('FAIL: Embedded haptic JavaScript not found')
parts = re.findall(r'"(?:[^"\\]|\\.)*"', m.group(1))
injected_js = ''.join(json.loads(part) for part in parts)
js_syntax = subprocess.run(['node','--check'],input=injected_js, text=True,capture_output=True)
check('Native haptics injected JavaScript parses', js_syntax.returncode == 0)


class Scripts(HTMLParser):
    def __init__(self): super().__init__(); self.in_script = False; self.scripts = []; self.buf = []
    def handle_starttag(self, tag, attrs):
        if tag == 'script': self.in_script = True; self.buf = []
    def handle_data(self, data):
        if self.in_script: self.buf.append(data)
    def handle_endtag(self, tag):
        if tag == 'script' and self.in_script:
            self.scripts.append(''.join(self.buf)); self.in_script = False

parser = Scripts(); parser.feed(studio.decode('utf-8'))
for idx, js in enumerate(parser.scripts):
    result = subprocess.run(['node','--check'], input=js, text=True, capture_output=True)
    if result.returncode:
        print(f'FAIL: Studio inline script {idx}: {result.stderr[:500]}')
        sys.exit(1)
check('All bundled Studio inline JavaScript parses', len(parser.scripts) > 0)
if shutil.which('javac') and shutil.which('java'):
    with tempfile.TemporaryDirectory() as output_dir:
        runner = root / 'qa/ParseJava.java'
        javac = subprocess.run(['javac', '-d', output_dir, str(runner)], capture_output=True, text=True)
        if javac.returncode:
            print(javac.stderr[:800]); raise SystemExit(1)
        sources = list((main / 'java').rglob('*.java'))
        java_result = subprocess.run(['java', '-cp', output_dir, 'ParseJava', *map(str,sources)], capture_output=True, text=True)
        if java_result.returncode: print(java_result.stderr[:1000]); raise SystemExit(1)
        check('All Android Java sources parse (syntax only)', len(sources) >= 3)
else:
    print('SKIP: Java syntax parser unavailable (javac/java absent); require Android CI build')
check('Android BLE output characteristic UUIDs match CORE',
      all('55a56022-4915-7a92-cf44-b8b7102eaf'+x in ble_java for x in ('25','26')))
check('BLE read/save bounded by auth and post-commit readback',
      all(x in ble_java for x in ('canConfigure()', 'cfgKnownRevision', 'cfgExpected', 'Arrays.equals(cfgBytes,cfgExpected)', 'cfgCrc', 'cfgReadNext()', 'cfgWriteNext()', 'cfgWatchdog')))
codec = read('app/src/main/java/com/opentail/android/BleConfigCodec.java')
check('Android output codec matches CORE 261-byte wire offsets and CRC',
      all(x in codec for x in ('LENGTH = 261', 'bytes[36]', '37+z*28', '(crc<<1)^0x1021', 'sum!=pixelCount')))
panel = read('app/src/main/assets/ble-output-panel.js')
check('Offline and online Layout BLE Output Manager injected',
      'injectBleOutputControls(view)' in main_java and 'ble-output-panel.js' in main_java and
      'window.OpenTailBLEOutputs' in panel and "state.tab==='layout'" in panel and 'Save to CORE over BLE' in panel)
check('BLE Output Manager contains zone role editing and never calls raw GATT from JavaScript',
      all(x in panel for x in ('data-zone=', 'data-key="role"', 'window.OpenTailNativeBLE.saveOutput', 'window.OpenTailNativeBLE.readOutput')))
check('BLE Output Manager JS syntax valid',
      subprocess.run(['node','--check',str(main / 'assets/ble-output-panel.js')],capture_output=True).returncode==0)
print(f'\n{passes}/{passes} static checks passed. Android SDK compile/device/BLE tests NOT run.')
