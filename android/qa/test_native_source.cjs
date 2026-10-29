'use strict';
/* Source-level architecture and data compatibility checks. Android SDK
 * compilation is a separate required CI job; this test does not claim that
 * phone interactions, BLE, or physical LED behavior have been exercised.
 */
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
// ComposeMainActivity is the shipped UI; Java Views are historical reference only.
const compose=fs.readFileSync('android/app/src/main/java/com/opentail/android/ComposeMainActivity.kt','utf8');
const model=fs.readFileSync('android/app/src/main/java/com/opentail/android/NativeProject.java','utf8');
const codec=fs.readFileSync('android/app/src/main/java/com/opentail/android/BleConfigCodec.java','utf8');
const catalog=fs.readFileSync('android/app/src/main/java/com/opentail/android/NativeEffectCatalog.java','utf8');
const manifest=fs.readFileSync('android/app/src/main/AndroidManifest.xml','utf8');
const gradle=fs.readFileSync('android/app/build.gradle','utf8');
const web=fs.readFileSync('web-studio/opentail.htm','utf8');
assert(!/new WebView\(|WebViewAssetLoader|@JavascriptInterface|evaluateJavascript\(|loadUrl\(|appassets\.androidplatform/.test(compose),
  'Android launch activity cannot contain WebView, JavaScript bridges, or loadUrl');
assert(!/implementation\s+['"]androidx\.webkit:/.test(gradle),
  'The Android app must not have AndroidX WebKit as a runtime dependency');
assert(!fs.existsSync('android/app/src/main/assets/opentail.htm'),
  'No HTML copy may be bundled in the native APK');
assert(!fs.existsSync('android/app/src/main/assets/unified-shell.js'),
  'No JS navigation overlay may be bundled in the native APK');
assert(!manifest.includes('android.permission.INTERNET')&&!manifest.includes('usesCleartextTraffic="true"'),
  'Native offline designer must not request arbitrary network access');
// Check the actual launch contract, not merely the retained Java source.
assert(manifest.includes('android:name=".ComposeMainActivity"'), 'Compose is not registered');
assert(!manifest.includes('android:name=".MainActivity"'), 'Legacy Java UI remains launchable');
assert.equal((manifest.match(/android.intent.category.LAUNCHER/g)||[]).length,1,
  'Exactly one Compose launcher is required');
assert(compose.includes('class ComposeMainActivity : ComponentActivity()') &&
       compose.includes('setContent { OpenTailShell() }') &&
       !compose.includes('MainActivity::class.java') &&
       !compose.includes('startActivity('),
  'Only the Compose activity may own navigation');
const composePages=['Home','Show Modes','Layout','Triggers','Auto Events','Settings'];
const pageList=compose.match(/private val pages = listOf\(([^)]*)\)/);
assert(pageList,'Compose page navigation missing');
assert.deepEqual([...pageList[1].matchAll(/"([^"]+)"/g)].map(x=>x[1]),composePages,
  'Active Compose navigation diverged from approved six-page order');
for(const route of ['HomePage(', 'ShowModesPage(', 'LayoutPage(', 'TriggersPage(',
                    'AutoEventsPage(', 'SettingsPage(']) {
  assert(compose.includes(route),'Active Compose route missing: '+route);
}
for(const required of ['MaterialTheme(colorScheme = scheme)',
  'lightColorScheme(', 'darkColorScheme(', 'RoundedCornerShape(99.dp)',
  'ConnectPill', 'WindowInsets.safeDrawing', 'ExactLedFrame.fromProjectModel(project)',
  'frame.colorAt(first + i)', 'NOT a simulated effect.', 'onValueChangeFinished = { applyLiveZone() }',
  'project.previewZoneOffsets(id, n)', 'project.setPixelColor(id, activePixel, hex)',
  'project.assign(', 'project.setSequence(', 'project.importJson(', 'project.exportJson()',
  'ble.canControl()', 'supportedIds.value', 'ble.showFx(', 'ble.saveOutput(index, draftText)',
  'BleConfigCodec.encode(JSONObject(draftText))']) {
  assert(compose.includes(required),'Active Compose implementation regressed: '+required);
}
assert(!/WebView\(|BottomNavigation|NavigationBar\(/.test(compose),
  'Active Compose must have no WebView or bottom navigation');
const bleSource=fs.readFileSync('android/app/src/main/java/com/opentail/android/BleDiscovery.java','utf8');
const homeSection=compose.split('private fun HomePage(t: Tone, changePage: (Int) -> Unit) {')[1]
                         .split('private fun EffectsPreview(')[0];
const settingsSection=compose.split('private fun SettingsPage(t: Tone) {')[1]
                             .split('private fun HardwareOutputEditor(')[0];
assert(homeSection.includes('Bluetooth · ') &&
       homeSection.includes('Pill("Authorize / Pair with BOOT"') &&
       settingsSection.includes('Pill("Authorize / Pair with BOOT"') &&
       !settingsSection.includes('Pill("Pair / BOOT"') &&
       !settingsSection.includes('Pill("Authorize saved key"') &&
       bleSource.includes('void authorizeOrPair()') &&
       bleSource.includes('BleOwnerVault.load(activity,activeAddress)==null') &&
       bleSource.includes('if (!connected)'),
       'Locked Home Bluetooth pill and one safe owner authorization/BOOT action regressed');


const frameClient=fs.readFileSync(
  'android/app/src/main/java/com/opentail/android/CoreFrameCapture.java','utf8');
for(const term of ['class CoreFrameCapture','CHUNK_PIXELS=56',
                   'if(!busy || connection!=gatt)return',
                   'u32(a,4)!=sequence', 'u16(a,8)!=offset',
                   'a.length!=11+3*n', 'BleConfigCodec.crc16(pixels)!=crc',
                   'listener.done(output,sequence,complete,null)', 'abortForStop()', 'operationPending']) {
  assert(frameClient.includes(term),'CORE snapshot client regression: '+term);
}
assert(compose.includes('ExactLedFrame.fromCoreRgb888(output, sequence, rgb)') &&
       compose.includes('ble.captureFrame(captureOutput.intValue)') &&
       compose.includes('frame?.origin == ExactLedFrame.Origin.CORE') &&
       compose.includes('NOT a simulated effect.'),
  'Compose must show verified CORE frame provenance, not invented animation');
assert(bleSource.includes('FRAME_CMD_UUID') &&
       bleSource.includes('FRAME_RSP_UUID') &&
       bleSource.includes('frameCapture.configure(') &&
       bleSource.includes('frameCapture.read(connection') &&
       bleSource.includes('frameCapture.written(connection') &&
       bleSource.includes('frameCapture.busy()') &&
       bleSource.includes('frameCapture.abortForStop()'),
  'BLE CORE snapshot must be capability-gated and serialize GATT requests');
assert(gradle.includes("versionName '0.2.0-alpha1'"));
// Verify the actual released Compose UI; obsolete Java Views are NOT a release oracle.
const layout=compose.split('private fun LayoutPage(t: Tone) {')[1].split('private fun addZoneAfter(')[0];
const triggers=compose.split('private fun TriggersPage(t: Tone) {')[1].split('private fun AutoEventsPage(')[0];
const modes=compose.split('private fun ShowModesPage(t: Tone) {')[1].split('private fun SequencePage(')[0];
const events=compose.split('private fun AutoEventsPage(t: Tone, previewEffect: (Int) -> Unit) {')[1]
                    .split('private fun SettingsPage(')[0];
const hardware=compose.split('private fun HardwareOutputEditor(t: Tone) {')[1];
assert(layout && triggers && modes && events && hardware,
       'A shipped Compose design page is missing');
for(const required of ['project.previewZoneOffsets(id, n)',
  'onValueChangeFinished = { applyLiveZone() }',
  'project.editZone(id, label, n, reverse)',
  'project.setPixelColor(id, activePixel, hex)',
  'project.clearPixelColor(id, activePixel)',
  'project.removeZone(id)', 'project.moveZone(',
  'Pill("+ Add next"', 'Pill("Remove"']) {
  assert(layout.includes(required),'Compose layout regression: '+required);
}
assert(!layout.includes('ble.saveOutput(') && !layout.includes('ble.showFx('),
 'Local layout authoring must not persist or command physical outputs');
for(const required of ['selections =', 'testing =', 'project.customTrigger(',
  'project.deleteTrigger(', 'project.assign(', 'applyTriggerStyle()']) {
  assert(triggers.includes(required),'Compose trigger regression: '+required);
}
assert(!triggers.includes('ble.saveOutput('),'Offline trigger design must not write NVS');
for(const required of ['EffectsPreview(t, effect)', 'listOf("Effects", "Sequences", "Presets")',
  'PaletteEditor("Foreground"', 'PaletteEditor("Background"', 'PaletteEditor("Accent"',
  'previewCurrentEffect(item.id)', 'Capture actual CORE frame']) {
  assert(modes.includes(required),'Compose effects regression: '+required);
}
assert(events.includes('listOf("Startup", "Shutdown")') &&
       events.includes('eventUiEnabled') &&
       events.includes('eventUiFadeMs') &&
       events.includes('eventDuration') &&
       events.includes('Preview offline'),
  'Optional startup/shutdown design regressed');
assert(hardware.includes('BleConfigCodec.MAX_LEDS_PER_OUTPUT') &&
       !hardware.includes('623') &&
       hardware.includes('it.put("pixels", remaining)') &&
       hardware.includes('structureVersion++') &&
       hardware.includes('BleConfigCodec.encode(draft)') &&
       hardware.includes('ble.saveOutput(index, draftText)'),
  'CORE output editor must validate universal drafts and synchronize zone totals');
for(const required of ['MAX_ZONES = 200', 'previewZoneOffsets(',
  'synchronized boolean editZone(', 'synchronized boolean moveZone(',
  'synchronized boolean removeZone(', 'synchronized boolean importJson(',
  'if (start < lastEnd)', 'if (input.length() > 2000000)',
  'synchronized String exportJson()']) {
  assert(model.includes(required),'Native project v4 import/index invariant missing: '+required);
}
const list=[...catalog.matchAll(/new Effect\((\d+), "([^"]+)"\)/g)].map(m=>[Number(m[1]),m[2]]);
assert.equal(list.length,140,'Native designer must include all approved effect names');
assert.equal(new Set(list.map(x=>x[0])).size,140,'Duplicate effect IDs');
const m=web.match(/const OT_STUDIO_DESIGN_EFFECTS=Object\.freeze\((\[[^\n]*\])\);/);
const o=web.match(/const OT_STUDIO_ORIGINAL_EFFECTS=Object\.freeze\((\{[^\n]*\})\);/);
assert(m&&o,'Approved web metadata catalog must remain intact');
const webModes=[...JSON.parse(m[1]),...Object.entries(JSON.parse(o[1])).map(([i,n])=>[Number(i),n])];
assert.deepEqual(list.slice(0,webModes.length),webModes,'Native effect names/IDs have drifted from approved web authoring');
assert(list.some(([id,name])=>id===32787&&name==='Singularity Shutdown'), 'Newest CORE original effects missing');
assert(!/WLED firmware|WLED UI/.test(compose),'End-user Compose UI must be OpenTail branded');
assert(bleSource.includes('pairWithBoot()') && bleSource.includes('authorizeStored()') &&
       bleSource.includes('HmacSHA256') && bleSource.includes('canControl()'),
  'BLE owner authorization regression');
assert(codec.includes('static final int MAX_LEDS_PER_OUTPUT = 65535') &&
       codec.includes('pixels>MAX_LEDS_PER_OUTPUT') &&
       codec.includes('length>MAX_LEDS_PER_OUTPUT') &&
       codec.includes('if(sum!=pixelCount)') &&
       !codec.includes('index==0?16:17') &&
       !hardware.includes('623'),
  'BLE uint16 geometry must be universal and not invent physical pins');
assert(!/GS400|GPIO16|GPIO17|522 pixels|1246/i.test(model),
  'Native project is universal, not specific to bench vehicle');
console.log('PASS: shipped Kotlin/Compose navigation, screens, universal geometry, BLE security, verified CORE frame path and project v4 contracts (static only).');
