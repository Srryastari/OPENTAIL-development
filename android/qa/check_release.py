#!/usr/bin/env python3
"""Offline static checks only. Not a substitute for Android SDK compilation or BLE hardware tests."""
from pathlib import Path
import hashlib
import subprocess
import xml.etree.ElementTree as ET
import re
import sys
import tempfile
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
check('Studio v0.19.51 bundled, pinned checksum', hashlib.sha256(studio).hexdigest() == read('qa/studio-sha256.txt').strip())
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
check('Release/build versions pinned', all(v in read('app/build.gradle') for v in ('compileSdk 35','minSdk 26','targetSdk 35','versionName \'0.1.15\'')) and 'gradle-8.11.1-bin.zip' in read('gradle/wrapper/gradle-wrapper.properties'))
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

# v0.1.4: integrated Show Modes BLE UI and unified mobile shell, no native control popup.
ble_ui = (main / 'assets/ble-show-panel.js').read_text(encoding='utf-8')
check('Embedded Bluetooth Show Modes script bundled', 'ot-ble-inline' in ble_ui and 'showBleControls()' not in main_java)
check('One-screen connection transport selector', 'data-ot-ble="wifi"' in ble_ui and 'data-ot-ble="ble"' in ble_ui)
check('BLE auth, effect start, and stop remain accessible', all(x in ble_ui for x in ('OpenTailNativeBLE.authorize()', 'OpenTailNativeBLE.start(', 'OpenTailNativeBLE.stop()')))
check('BLE control UI persists across Studio render/navigation', 'MutationObserver' in ble_ui and 'isShows()' in ble_ui and 'requestAnimationFrame' in ble_ui)
check('BLE controls shown on both CORE Wi-Fi and offline Studio', 'injectInlineBleControls(view)' in main_java and 'OFFLINE_URL.equals' in main_java)
check('Bridge disallows write commands on unrelated pages', 'bridgePageAllowed()' in main_java and 'if (!bridgePageAllowed() || ble == null) return;' in main_java)
check('BLE-only BOOT enrollment and encrypted local key retention',
      'pairWithBoot()' in ble_java and 'BleOwnerVault.save(' in ble_java and 'AES/GCM/NoPadding' in read('app/src/main/java/com/opentail/android/BleOwnerVault.java')
      and 'android:allowBackup="false"' in read(xml_path))
check('BLE HMAC authorization uses challenge and limits routine key exposure',
      'HmacSHA256' in ble_java and 'OpenTail-BLE-v2' in ble_java and 'CHALLENGE_UUID' in ble_java and 'ENROLL_UUID' in ble_java)
check('Legacy Wi-Fi authorization remains manually accessible',
      'authorizeWifiLegacy()' in main_java and 'CoreConnection.bleSessionToken()' in main_java)
check('Android BOOT pairing button visible',
      "button('Pair with BOOT','pair')" in read('app/src/main/assets/ble-output-panel.js'))
check('Native BLE bridge has no physical trigger mapping writes', 'writeTrigger' not in main_java and 'writeOutput' not in main_java)
check('BLE brightness maps to firmware cap 32', 'state.brightness * 32 / 100' in ble_ui)
check('Separate BLE Show Modes popup and top toolbar button removed', 'showBleControls()' not in main_java and 'addButton(row3' not in main_java)
check('Version bump for in-place Android update', "versionCode 15" in read('app/build.gradle') and "versionName '0.1.15'" in read('app/build.gradle'))
# v0.1.4: a single Android-native-feeling shell wraps the existing WebView Studio.
unified = (main / 'assets/unified-shell.js').read_text(encoding='utf-8')
check('Unified OpenTail shell bundled and injected into CORE and offline Studio',
      'injectUnifiedShell(view)' in main_java and 'unified-shell.js' in main_java and 'OpenTailNativeApp' in main_java)
check('Old native duplicate header and connection-button rows removed',
      'root.addView(row1)' not in main_java and 'root.addView(row2)' not in main_java and 'root.addView(title)' not in main_java)
check('Unified connection cards and compact Connected badge',
      all(x in unified for x in ('ot-mobile-header','ot-connected-badge','ot-connection-grid','ot-wifi-card','ot-ble-card')))
check('Unified bottom navigation has five required destinations',
      all(x in unified for x in ("['home','Home'","['scenes','Shows'","['builder','Triggers'","['layout','Layout'","['settings','Settings'")))
check('Native Settings preserves Wi-Fi, BLE, haptics, appearance, diagnostics and offline access',
      all(x in unified for x in ('connectWifi','wifiSettings','scanBle','authorizeBle','appSettings','offlineStudio','diagnostics','appearance')))
check('Show Modes retains Bluetooth effect selectors, native commands and Wi-Fi alternative',
      all(x in ble_ui for x in ('Show Modes','data-ot-field="output"','data-ot-field="zone"','data-ot-field="effect"','data-ot-ble="wifi"','data-ot-ble="ble"')))
check('BLE remains available after Wi-Fi loss with offline Studio fallback',
      'web.loadUrl(OFFLINE_URL)' in main_java and 'connectCore(true)' in main_java)
check('Only first-party bridge controls, no arbitrary WebView navigation',
      'if (bridgePageAllowed())' in main_java and 'Untrusted URLs blocked' not in main_java)
check('Native system haptic and saved-setting functions still present',
      'STUDIO_HAPTIC_JS' in main_java and 'showSettings()' in main_java)
check('Unified script JavaScript syntax',
      subprocess.run(['node','--check',str(main/'assets/unified-shell.js')],capture_output=True,text=True).returncode == 0)
check('Bluetooth OFF broadcast clears stale native status',
      'BluetoothAdapter.ACTION_STATE_CHANGED' in main_java and 'ble.onAdapterDisabled()' in main_java
      and 'radioOn && ble != null && ble.isConnected()' in main_java and 'onAdapterDisabled()' in ble_java)
check('Native connection phase reflects scanning/pairing/transfer state',
      'connectionPhase()' in ble_java and '"saving"' in ble_java and '"pairing"' in ble_java)
check('Wi-Fi badge requires active Wi-Fi transport, not merely a cached CORE page',
      'CoreConnection.hasWifiTransport(MainActivity.this)' in main_java and 'hasWifiTransport(Context context)' in core_java)
check('BLE Output Manager presents inline status and refreshes on radio event',
      'ot-bom-health' in read('app/src/main/assets/ble-output-panel.js')
      and 'connectionChanged(status)' in read('app/src/main/assets/ble-output-panel.js')
      and 'OpenTailBLEOutputs.connectionChanged' in main_java)
check('Offline badge red, connecting amber, authorized green (late-theme overrides)',
      all(x in unified for x in ('ot-status-red','ot-status-amber','ot-status-green','#ff545e','#f4c35f','#29da8b'))
      and "radioOff ? 'Bluetooth off'" in unified)
check('Wi-Fi status refresh uses event callback rather than periodic polling',
      'registerNetworkCallback(' in main_java and 'unregisterNetworkCallback(' in main_java
      and 'onLost(Network network)' in main_java and 'onCapabilitiesChanged(Network network' in main_java)
check('Unified shell navigation smoke tests',
      subprocess.run(['node',str(root/'qa/test_unified_shell.js')],cwd=root,capture_output=True,text=True).returncode == 0)

# v0.1.11: compact default surfaces must not replace CORE save/readback machinery.
check('Home, Triggers, and Layout have compact native-first overview cards',
      all(x in unified for x in ("top('Home'", "top('Triggers'", "top('Layout'", 'ot-compact-pages')))
check('Full existing editors remain directly accessible',
      'data-ot-open=\"edit\"' in unified and 'ot-editor-active' in unified and 'data-ot-open=\"back\"' in unified)
check('Overview pages read device data but never write configuration',
      "'/api/v1/outputs'" in unified and "'/api/v1/triggers'" in unified and "method:'GET'" in unified and "method:'POST'" not in unified)
check('BLE compact panel has progressive controls while keeping all parameters',
      'ot-ble-more' in ble_ui and all(x in ble_ui for x in ('data-ot-field=\"color\"','data-ot-field=\"brightness\"', 'data-ot-field=\"speed\"', 'data-ot-field=\"ttl\"')))
check('BLE show view hides the long legacy Studio panels',
      'main:has(#ot-ble-inline' in unified and ':not(#ot-ble-inline)' in unified)
check('Native bottom navigation stays above Android safe area',
      'env(safe-area-inset-bottom' in unified and 'position:fixed' in unified)
check('A native standalone release does not modify bundled Studio',
      hashlib.sha256(studio).hexdigest() == read('qa/studio-sha256.txt').strip())

result = subprocess.run(['node', '--check', str(main / 'assets/ble-show-panel.js')], capture_output=True, text=True)
check('Embedded Bluetooth Show Modes script syntax', result.returncode == 0)
smoke = subprocess.run(['node', str(root / 'qa/test_ble_inline.js')], cwd=root, capture_output=True, text=True)
check('BLE show commands and Wi-Fi/BLE panel switching smoke test', smoke.returncode == 0)

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
with tempfile.TemporaryDirectory() as output_dir:
    runner = root / 'qa/ParseJava.java'
    javac = subprocess.run(['javac', '-d', output_dir, str(runner)], capture_output=True, text=True)
    if javac.returncode:
        print(javac.stderr[:800]); raise SystemExit(1)
    sources = list((main / 'java').rglob('*.java'))
    java_result = subprocess.run(['java', '-cp', output_dir, 'ParseJava', *map(str,sources)], capture_output=True, text=True)
    if java_result.returncode: print(java_result.stderr[:1000]); raise SystemExit(1)
    check('All Android Java sources parse (syntax only)', len(sources) >= 3)
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
