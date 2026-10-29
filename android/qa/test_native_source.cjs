'use strict';
/* Source-level architecture and data compatibility checks. Android SDK
 * compilation is a separate required CI job; this test does not claim that
 * phone interactions, BLE, or physical LED behavior have been exercised.
 */
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const main=fs.readFileSync('android/app/src/main/java/com/opentail/android/MainActivity.java','utf8');
const model=fs.readFileSync('android/app/src/main/java/com/opentail/android/NativeProject.java','utf8');
const ui=fs.readFileSync('android/app/src/main/java/com/opentail/android/NativeUi.java','utf8');
const preview=fs.readFileSync('android/app/src/main/java/com/opentail/android/NativeLedPreview.java','utf8');
const catalog=fs.readFileSync('android/app/src/main/java/com/opentail/android/NativeEffectCatalog.java','utf8');
const manifest=fs.readFileSync('android/app/src/main/AndroidManifest.xml','utf8');
const gradle=fs.readFileSync('android/app/build.gradle','utf8');
const web=fs.readFileSync('web-studio/opentail.htm','utf8');
assert(!/new WebView\\(|WebViewAssetLoader|@JavascriptInterface|evaluateJavascript\\(|loadUrl\\(|appassets\\.androidplatform/.test(main),
  'Android launch activity cannot contain WebView, JavaScript bridges, or loadUrl');
assert(!/implementation\\s+['"]androidx\\.webkit:/.test(gradle),
  'The Android app must not have AndroidX WebKit as a runtime dependency');
assert(!fs.existsSync('android/app/src/main/assets/opentail.htm'),
  'No HTML copy may be bundled in the native APK');
assert(!fs.existsSync('android/app/src/main/assets/unified-shell.js'),
  'No JS navigation overlay may be bundled in the native APK');
assert(!manifest.includes('android.permission.INTERNET')&&!manifest.includes('usesCleartextTraffic="true"'),
  'Native offline designer must not request arbitrary network access');
assert(manifest.includes('android:name=".MainActivity"'));
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
       preview.includes('postInvalidateDelayed(33)')&&
       !/Bluetooth|HttpURLConnection|WebSocket|Gatt/i.test(preview),
 'Native Canvas offline preview must not have controller access');
assert(model.includes('static final int MAX_ZONES = 200')&&
       model.includes('long end = 0')&&model.includes('start", start')&&
       model.includes('"offsetMode", "auto"')&&
       model.includes('reordered.add(to, selected)'),
 'Universal zone indexing or both-direction reorder regressed');
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
assert.equal(list.length,126,'Native designer must include all approved effect names');
assert.equal(new Set(list.map(x=>x[0])).size,126,'Duplicate effect IDs');
const m=web.match(/const OT_STUDIO_DESIGN_EFFECTS=Object\.freeze\((\[[^\n]*\])\);/);
const o=web.match(/const OT_STUDIO_ORIGINAL_EFFECTS=Object\.freeze\((\{[^\n]*\})\);/);
assert(m&&o,'Approved web metadata catalog must remain intact');
const webModes=[...JSON.parse(m[1]),...Object.entries(JSON.parse(o[1])).map(([i,n])=>[Number(i),n])];
assert.deepEqual(list,webModes,'Native effect names/IDs have drifted from approved web authoring');
assert(!/WLED firmware|WLED UI/.test(main),'End-user native app must be OpenTail branded');
const ble=fs.readFileSync('android/app/src/main/java/com/opentail/android/BleDiscovery.java','utf8');
assert(ble.includes('pairWithBoot()')&&ble.includes('authorizeStored()')&&
       ble.includes('HmacSHA256')&&ble.includes('canControl()'),
 'Secure BLE authorization flow must remain available in native code');
console.log('PASS: Native Android UI, 6 pages, 126 effects, v4 project, no bundled WebView, guarded BLE and universal zones (static).');
