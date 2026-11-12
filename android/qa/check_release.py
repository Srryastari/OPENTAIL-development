#!/usr/bin/env python3
"""Native Android v0.2.0-alpha7 release audit. Compilation is done separately."""
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
root_gradle = read("build.gradle")
compose = (source / "ComposeMainActivity.kt").read_text()
model = (source / "NativeProject.java").read_text()
effect = (source / "NativeEffectCatalog.java").read_text()
ble = (source / "BleDiscovery.java").read_text()
owner = (source / "BleOwnerVault.java").read_text()
check("Android manifest and styles parse", manifest.tag == "manifest" and style.getroot().tag == "resources")
check("Android v0.2.0-alpha7 native release numbers", 
      "versionName '0.2.0-alpha7'" in gradle and "versionCode 37" in gradle and
      all(x in gradle for x in ("minSdk 26", "compileSdk 35", "targetSdk 35")))
check("Sole Kotlin Compose launcher; no Java UI entry or network transport permission",
      manifest_xml.count('android.intent.category.LAUNCHER') == 1 and
      'android:name=".ComposeMainActivity"' in manifest_xml and
      'android:name=".MainActivity"' not in manifest_xml and
      'android.permission.INTERNET' not in manifest_xml and
      manifest_xml.index('android:name=".ComposeMainActivity"') <
      manifest_xml.index('android.intent.category.LAUNCHER'))
check("No WebView or packaged Studio HTML runtime assets",
      "WebView(" not in compose and "loadUrl(" not in compose and
      not (app / "assets/opentail.htm").exists() and
      "androidx.webkit:webkit" not in gradle)
check("No duplicate Android shell, BLE panel or injected JavaScript",
      all(not (app / "assets" / name).exists() for name in
          ("unified-shell.js", "ble-output-panel.js", "direct-ble-dashboard.js",
           "output-control-motion.js")))
check("Active Compose UI, independent project model and BLE codec exist",
      all((source / name).exists() for name in
          ("ComposeMainActivity.kt", "NativeProject.java", "NativeEffectCatalog.java",
           "BleDiscovery.java", "BleConfigCodec.java", "ExactLedFrame.java",
           "LiveBlePreview.kt")))
check("Universal zones and v4 document import/export remain local",
      all(s in model for s in ("reindex()", 'put("version", 4)', "importJson(String input)",
                               "synchronized String exportJson()")) and
      "ActivityResultContracts.OpenDocument()" in compose and
      "ActivityResultContracts.CreateDocument(" in compose)
check("No hardcoded vehicle LED zones in native project model",
      all(w not in model for w in ("GS400", "522 pixels", "1246", "GPIO16", "GPIO17")))
check("Previously implemented BLE owner authentication and encryption retained",
      all(s in ble for s in ("HmacSHA256", "pairWithBoot()", "authorizeStored()",
                              "showFx(", "canControl()")) and
      "AES/GCM/NoPadding" in owner)
check("Continuous controller playback is capability-gated with explicit Home STOP",
      'ble.showContinuousFx(' in (source/"LiveBlePreview.kt").read_text() and
      'canContinuousFx()' in ble and
      'ContinuousFxCommand.encode(' in ble and
      'IconAction("Stop CORE show","stop",t)' in compose and
      'ble.stopShow()' in (source/"LiveBlePreview.kt").read_text())
check("Native effects manifest is explicitly 140 IDs",
      len(re.findall(r'new Effect\(\d+, "', effect)) == 140)
check("Active Compose output save uses authenticated BLE revision/readback path",
      'ble.savePanelV4(lane,requested)' in compose and
      'PanelV4DraftCodec.encode(index,JSONObject(draftText),saved)' in compose and
      all(k in ble for k in ("panelSaveBusy", "PanelV4LayoutProtocol.beginSave", "panelSaveRead(",
                             "Arrays.equals(panelSaveWire,snapshot.wire)", "panelSaveFinish(true")))
check("Native source and verified project regression tests pass",
      subprocess.run(["node", str(root/"qa/test_native_source.cjs")],
                     cwd=root.parent, capture_output=True).returncode == 0)
# Source assertions cannot certify device visuals, BLE behavior or physical LED light.
check("Legacy Java Views activity is inaccessible from active Compose app",
      'android:name=".MainActivity"' not in manifest_xml and
      'MainActivity::class.java' not in compose and
      'startActivity(' not in compose)
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
check("Compose Material UI follows locked OLED/white colors",
      all(t in compose for t in (
          "MaterialTheme(colorScheme = scheme)", "lightColorScheme(", "darkColorScheme(",
          "Color.White", "ConnectPill", "RoundedCornerShape(99.dp)",
          'ApprovedWledEffectsEditor(',
          "if(editingId==id)", "ZoneDotPreview(t,n,reverse)", "HardwareOutputEditor(t)")))
check("Compose authoring preserves zone offsets and separate BLE confirmation",
      all(k in compose for k in
          ("previewZoneOffsets(id, n)", "project.editZone(",
           "project.assignMany(", "project.setSequence(", "project.importJson(",
           "project.exportJson()", "ble.canControl()", "supportedIds.value",
           "previewCurrentEffect()", "PanelV4DraftCodec.encode(index,JSONObject(draftText),saved)",
           "ble.savePanelV4(", "draftText = updated.toString()",
           "val updated = JSONObject(draftText)",
           "project.previewZoneOffsets(", "AlertDialog(containerColor=t.surface,"))
      and 'android:name=".MainActivity"' not in manifest_xml)
check("Compose has full two-output draft editing without unsaved hardware mutation",
      all(word in compose for word in (
          "BleConfigCodec.ROLES", "BleConfigCodec.ORDERS", "Fit last zone",
          "Assigned ", "if(outputToolsOpen)", "Remove zone", "+ Add zone",
          "val draft = runCatching { JSONObject(draftText) }",
          "outputMessage.value=\"Verifying current live mapping before one Panel v4 Save...\"",
          "if (savingOutput.intValue == index) Caption(\"Saving…",
          'Pill(if (savingOutput.intValue == index) "Saving…" else "Save"'))
      and 'ble.savePanelV4(lane,requested)' in compose
      and 'ble.readPanelLaneSnapshot(index)' in compose
      and 'outputDirty[index] = true' in compose
      and '(!ownerEditedAfterSave && completedSave)' in compose)
check("Native effect catalog and approved sequence toggles remain accessible",
      all(word in compose for word in ("PaletteEditor(", "Shared Palette", "Shuffle",
          "Repeat", "Edit time", "project.sequenceOptions()", "project.editTriggerStyle(",
          'scopeKey = "triggers:"', 'scopeKey="auto-event:"+key'))
      and 'new Effect(32787, "Singularity Shutdown")' in effect)
check("Universal live-edit contract and continuous CORE playback",
      subprocess.run(["node", str(root/"qa/test_live_edit.cjs")],
                     cwd=root.parent, capture_output=True).returncode == 0)
print("Compose is the sole Android activity. Physical BLE and pixel-accurate visual parity still require hardware validation.")
print(f"\\n{passed}/{passed} static checks passed. Android build and device/BLE tests are separate.")
