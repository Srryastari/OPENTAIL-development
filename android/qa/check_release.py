#!/usr/bin/env python3
"""Native Android v0.2.0-alpha1 release audit. Compilation is done separately."""
from pathlib import Path
import hashlib
import re
import subprocess
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
app = root / "app/src/main"
source = app / "java/com/opentail/android"
read = lambda p: (root / p).read_text(encoding="utf-8")
passed = 0

def check(label, success):
    global passed
    if not success:
        raise SystemExit("FAIL: " + label)
    passed += 1
    print("PASS: " + label)

manifest_xml = read("app/src/main/AndroidManifest.xml")
manifest = ET.fromstring(manifest_xml)
style = ET.parse(app / "res/values/styles.xml")
gradle = read("app/build.gradle")
main = (source / "MainActivity.java").read_text()
model = (source / "NativeProject.java").read_text()
effect = (source / "NativeEffectCatalog.java").read_text()
ble = (source / "BleDiscovery.java").read_text()
owner = (source / "BleOwnerVault.java").read_text()
check("Android manifest and styles parse", manifest.tag == "manifest" and style.getroot().tag == "resources")
check("Android v0.2.0-alpha1 native release numbers", 
      "versionName '0.2.0-alpha1'" in gradle and "versionCode 31" in gradle and
      all(x in gradle for x in ("minSdk 26", "compileSdk 35", "targetSdk 35")))
check("Manifest launches native MainActivity and has no web transport permission",
      'android:name=".MainActivity"' in manifest_xml and
      'android.permission.INTERNET' not in manifest_xml)
check("No WebView or packaged Studio HTML runtime assets",
      "new WebView(" not in main and "loadUrl(" not in main and
      not (app / "assets/opentail.htm").exists() and
      "androidx.webkit:webkit" not in gradle)
check("No duplicate Android shell, BLE panel or injected JavaScript",
      all(not (app / "assets" / name).exists() for name in
          ("unified-shell.js", "ble-output-panel.js", "direct-ble-dashboard.js",
           "output-control-motion.js")))
check("Native widget/screen classes compiled from source",
      all((source / name).exists() for name in
          ("MainActivity.java", "NativeUi.java", "NativeLedPreview.java",
           "NativeProject.java", "NativeEffectCatalog.java", "NativeOutputManager.java")))
check("Universal zones and v4 document import/export remain local",
      all(s in model for s in ("reindex()", 'put("version", 4)', "importJson(String input)",
                               "synchronized String exportJson()")) and
      "ACTION_OPEN_DOCUMENT" in main and "ACTION_CREATE_DOCUMENT" in main)
check("No hardcoded vehicle LED zones in native project model",
      all(w not in model for w in ("GS400", "522 pixels", "1246", "GPIO16", "GPIO17")))
check("Previously implemented BLE owner authentication and encryption retained",
      all(s in ble for s in ("HmacSHA256", "pairWithBoot()", "authorizeStored()",
                              "showFx(", "canControl()")) and
      "AES/GCM/NoPadding" in owner)
check("Controller writing is separate from authoring, requires confirmation",
      "private void confirmBleTest()" in main and
      'setTitle("Bench test on CORE?")' in main and
      "supportedBleEffects.contains(effectId)" in main and
      "Math.min(32," in main)
check("Native effects manifest is explicitly 126 IDs",
      len(re.findall(r'new Effect\(\d+, "', effect)) == 126)
check("Native hardware output read/save and post-commit readback retained",
      "new NativeOutputManager(this,ble,ui)" in main and
      "NativeOutputManager.java" in str(source / "NativeOutputManager.java") and
      all(k in ble for k in ("cfgKnownRevision", "cfgExpected", "Arrays.equals(cfgBytes,cfgExpected)",
                             "cfgCrc", "cfgReadNext()", "cfgWriteNext()")))
check("Native source and verified project regression tests pass",
      subprocess.run(["node", str(root/"qa/test_native_source.cjs")],
                     cwd=root.parent, capture_output=True).returncode == 0)
# This is a build-tested migration candidate, NOT the launcher or a device pass.
# Retain the earlier working Java launcher until Compose reaches functional parity.
compose = (source / "ComposeMainActivity.kt").read_text()
root_gradle = read("build.gradle")
check("Kotlin + Compose toolchain is present (not a WebView replacement)",
      "org.jetbrains.kotlin.android" in root_gradle and
      "org.jetbrains.kotlin.plugin.compose" in root_gradle and
      "id 'org.jetbrains.kotlin.plugin.compose'" in gradle and
      "activity-compose" in gradle and
      "@Composable" in compose and
      "class ComposeMainActivity : ComponentActivity()" in compose and
      "WebView(" not in compose)
check("Compose migration has approved six pages and top-only navigation",
      'listOf("Home", "Show Modes", "Layout", "Triggers", "Auto Events", "Settings")' in compose
      and all(k in compose for k in
          ("HomePage(", "ShowModesPage(", "LayoutPage(", "TriggersPage(",
           "AutoEventsPage(", "SettingsPage("))
      and "horizontalScroll(" in compose and "BottomNavigation" not in compose)
check("Compose authoring preserves zone offsets and separate BLE confirmation",
      all(k in compose for k in
          ("previewZoneOffsets(id, n)", "project.editZone(",
           "project.assign(", "project.setSequence(", "project.importJson(",
           "project.exportJson()", "ble.canControl()", "supportedIds.value",
           "ble.showFx(", "BleConfigCodec.encode(next)", "ble.saveOutput(",
           "AlertDialog(onDismissRequest")))
      and 'android:name=".MainActivity"' in manifest_xml)
print("Compose status: compilation candidate; still NOT launcher. No UI parity or device test claimed.")
print(f"\\n{passed}/{passed} static checks passed. Android build and device/BLE tests are separate.")
