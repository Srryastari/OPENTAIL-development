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
const main=fs.readFileSync('android/app/src/main/java/com/opentail/android/MainActivity.java','utf8');
const model=fs.readFileSync('android/app/src/main/java/com/opentail/android/NativeProject.java','utf8');
const ui=fs.readFileSync('android/app/src/main/java/com/opentail/android/NativeUi.java','utf8');
const preview=fs.readFileSync('android/app/src/main/java/com/opentail/android/NativeLedPreview.java','utf8');
const output=fs.readFileSync('android/app/src/main/java/com/opentail/android/NativeOutputManager.java','utf8');
const codec=fs.readFileSync('android/app/src/main/java/com/opentail/android/BleConfigCodec.java','utf8');
const catalog=fs.readFileSync('android/app/src/main/java/com/opentail/android/NativeEffectCatalog.java','utf8');
const manifest=fs.readFileSync('android/app/src/main/AndroidManifest.xml','utf8');
const gradle=fs.readFileSync('android/app/build.gradle','utf8');
const web=fs.readFileSync('web-studio/opentail.htm','utf8');
assert(!/new WebView\(|WebViewAssetLoader|@JavascriptInterface|evaluateJavascript\(|loadUrl\(|appassets\.androidplatform/.test(main),
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
for(const widget of ['NativeUi','NativeLedPreview','ScrollView','HorizontalScrollView',
                     'SeekBar','EditText','AlertDialog','TextView','CheckBox']) {
  assert(main.includes(widget),'Required native Android screen widget missing: '+widget);
}
assert(main.includes('private static final String[] TABS={'));
const navigation=main.slice(main.indexOf('private static final String[] TABS='),main.indexOf('private static final String[] DEFAULT_EVENTS='));
const expected=['Home','Show Modes','Layout','Triggers','Auto Events','Settings'];
assert.deepEqual([...navigation.matchAll(/"([^"]+)"/g)].map(m=>m[1]),expected);
for(const route of ['home()','showModes()','layout()','triggers()','autoEvents()','settings()']) {
  assert(main.includes('private void '+route+'{')||main.includes('private void '+route+' {'),
    'Missing native route '+route);
}
assert(main.includes('private void colorDialog(')&&main.includes('chooseEffect(')&&
       main.includes('editSequenceStep(')&&main.includes('editSelectedTriggers(')&&
       main.includes('editZone(')&&main.includes('presetEditor('),
 'Native editors for all design functions must be implemented');
assert(ui.includes('0xff000000')&&ui.includes('0xffffffff')&&
       ui.includes('GradientDrawable')&&ui.includes('setCornerRadius(dp(radius))'),
 'OLED/white theme and approved capsule styling missing');
assert(preview.includes('extends View')&&preview.includes('onDraw(Canvas')&&
       preview.includes('ExactLedFrame.fromProjectModel(project)')&&
       preview.includes('frame.colorAt(first+i)')&&
       preview.includes('private static final int PAGE = 64')&&
       !preview.includes('postInvalidateDelayed')&&
       !preview.includes('NativeEffectCatalog.byId')&&
       !/Bluetooth|HttpURLConnection|WebSocket|Gatt/i.test(preview),
 'Native Canvas must inspect exact project pixel indexes, never fabricate effects');
assert(model.includes('static final int MAX_ZONES = 200')&&
       model.includes('long end = 0')&&model.includes('start", start')&&
       model.includes('"offsetMode", "auto"')&&
       model.includes('reordered.add(to, selected)'),
 'Universal zone indexing or both-direction reorder regressed');
// Issue #14: approved zone slider/offset parity. Live previews must be
// calculated from universal project state, stay unsaved until confirmation,
// and cascade through later zones without sending any hardware commands.
const previewOffsets=model.slice(model.indexOf('synchronized String previewZoneOffsets('),
  model.indexOf('synchronized boolean editZone('));
assert(previewOffsets.includes('int selected=indexOfZone(id)')&&
       previewOffsets.includes('i==selected?suggestedLength:zone.optInt("length",0)')&&
       previewOffsets.includes('Math.max(end,zone.optLong("start",end))')&&
       previewOffsets.includes('long start=manual?')&&
       !previewOffsets.includes('save();')&&!previewOffsets.includes('ble.')&&
       !previewOffsets.includes('z.put('),
  'Native live dependent-zone preview must match zero-based indexing with no persistence or BLE writes');
const zoneEditor=main.slice(main.indexOf('private void editZone(String id)'),
                            main.indexOf('private int number(',main.indexOf('private void editZone(String id)')));
assert(zoneEditor.includes('count.addTextChangedListener(new TextWatcher()')&&
       zoneEditor.includes('project.previewZoneOffsets(id,length)')&&
       zoneEditor.includes('range.setProgress(length)')&&
       zoneEditor.includes('count.setText(String.valueOf(Math.max(1,n)))')&&
       zoneEditor.includes('project.editZone(id,'),
  'Android zone pixel-count slider and typed value must preview every dependent offset before Save');
assert(ui.includes('bar.setProgressDrawable(track)')&&
       ui.includes('bar.setProgressTintList(')&&
       !ui.slice(ui.indexOf('SeekBar slider('),ui.indexOf('LinearLayout.LayoutParams lp(')).includes('setStroke('),
  'Approved slider must use a borderless rounded capsule track');

assert(!/GS400|GPIO16|GPIO17|522 pixels|1246/i.test(model),
 'Universal project model must never hardcode one car/bench wiring layout');
assert(model.includes('"version", 4')&&model.includes('validated(new JSONObject(input))')&&
       model.includes('if (start < lastEnd)')&&model.includes('if (input.length() > 2000000)'),
 'Shared v4 project export/import must validate bounds, overlap, file size');
assert(main.includes('ACTION_OPEN_DOCUMENT')&&main.includes('ACTION_CREATE_DOCUMENT')&&
       main.includes('project.importJson(')&&main.includes('project.exportJson()'),
 'Native Android system document picker must preserve project backup/export');
assert(main.includes('private void confirmBleTest()')&&
       main.includes('if(!ble.canControl())')&&main.includes('supportedBleEffects.contains(effectId)')&&
       main.includes('new AlertDialog.Builder(this).setTitle("Bench test on CORE?")')&&
       main.includes('ble.showFx(effectId,3,255,')&&
       main.includes('Math.min(32,')&&main.includes('safe,speed,intensity,0,15)'),
 'Real BLE command must require auth, verified capabilities, confirmation, brightness cap and TTL');
for(const source of ['private void effectEditor()','private void sequenceEditor()',
                     'private void layout()','private void triggers()','private void autoEvents()']) {
  const start=main.indexOf(source),end=main.indexOf('\n    private ',start+source.length);
  assert(start>0&&end>start);
  const editor=main.slice(start,end);
  assert(!editor.includes('ble.showFx(')&&!editor.includes('ble.stopShow('),
    'Authoring path may not write to a physical controller: '+source);
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
assert(!/WLED firmware|WLED UI/.test(main),'End-user native app must be OpenTail branded');
const ble=fs.readFileSync('android/app/src/main/java/com/opentail/android/BleDiscovery.java','utf8');
assert(ble.includes('pairWithBoot()')&&ble.includes('authorizeStored()')&&
       ble.includes('HmacSHA256')&&ble.includes('canControl()'),
 'Secure BLE authorization flow must remain available in native code');
assert(main.includes('new NativeOutputManager(this,ble,ui)')&&main.includes('outputManager.onBleResult(result)'),
 'Advanced native BLE output editor must connect to existing readback protocol');
assert(output.includes('class NativeOutputManager')&&output.includes('ble.readOutput(selected)')&&
       output.includes('ble.saveOutput(index,draft.toString())')&&
       output.includes('BleConfigCodec.encode(draft)')&&output.includes('ble.canConfigure()')&&
       output.includes('setTitle("Save physical output?")'),
 'Native BLE output editor must read first, validate and explicitly confirm writes');
assert(output.includes('new JSONObject(result.getJSONObject("output").toString())')&&
       output.includes('drafts[index]')&&output.includes('for(int i=0;i<2;i++)'),
 'Per-output independent drafts and verified hardware readback required');
assert(codec.includes('static final int MAX_LEDS_PER_OUTPUT = 623')&&
       codec.includes('pixels>MAX_LEDS_PER_OUTPUT')&&
       codec.includes('length>MAX_LEDS_PER_OUTPUT')&&
       output.includes('Active pixels (1–623)')&&
       output.includes('1,246 total')&&
       codec.includes('int pixelCount=json.getInt("pixels")')&&codec.includes('if(sum!=pixelCount)')&&
       codec.includes('static byte[] encode(')&&codec.includes('static JSONObject decode('),
 'Native hardware output safety and codec validation regressed');
console.log('PASS: shipped Kotlin/Compose six-page UI, source-level live-edit/LED map, plus retained Java reference, project, BLE and output codec contracts (static only).');
