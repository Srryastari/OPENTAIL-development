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
const palettes=fs.readFileSync('android/app/src/main/java/com/opentail/android/NativePaletteEngine.kt','utf8');
const bleSource=fs.readFileSync('android/app/src/main/java/com/opentail/android/BleDiscovery.java','utf8');
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
  'frame!!.colorAt(sample)', 'CapsuleSlider(n, 1, 10000, t,', 'finished={ applyLiveZone() }',
  'project.previewZoneOffsets(id, n)', 'project.setPixelColor(id,activePixel,hex)',
  'project.assignMany(', 'project.setSequence(', 'project.importJson(', 'project.exportJson()',
  'ble.canControl()', 'supportedIds.value', 'ble.showFx(', 'ble.saveOutput(index, draftText)',
  'BleConfigCodec.encode(JSONObject(draftText))']) {
  assert(compose.includes(required),'Active Compose implementation regressed: '+required);
}
assert(compose.includes('private fun CapsuleSlider(') &&
       compose.includes('height(4.dp)') &&
       compose.includes('shadow(5.dp, RoundedCornerShape(99.dp)') &&
       compose.includes('Field(hex, "#RRGGBB", t, Modifier.weight(1f))') &&
       !compose.includes('Slider(value = n.coerceIn(1, 10000)') &&
       compose.includes('Modifier.heightIn(min = 48.dp)'),
       'All editing controls should use the approved capsule slider and accessible pills');
assert(compose.includes('val pageScroll = remember(page) { ScrollState(0) }') &&
       compose.includes('verticalScroll(pageScroll)') &&
       compose.includes('for (col in 0 until columns)') &&
       compose.includes('val columns = 30') &&
       compose.includes('pendingLoad = i') &&
       compose.includes('title = { Text("Replace current design?") }') &&
       compose.includes('Developer · Trigger Testing') &&
        compose.includes('project.assignMany(selections,zoneId,!checked)'),
       'Compose destination scroll, exact pixel zoom and destructive-edit UX regressed');
assert(compose.includes('if (offlinePreview.value) return') && compose.includes('if(!foregroundActivity.value || offlinePreview.value || previewPaused.value || !ble.canCaptureFrame()) return') && !compose.includes('if (i != 1) offlinePreview.value = false') && compose.includes('offlinePreview.value && hasProjectColors'),
       'Preview Offline must be local-only, suppressing all effect and snapshot BLE edits');
assert(compose.includes('NavigationPill(i, title, t, page == i)') &&
         compose.includes('private fun NavigationGlyph(index: Int, color: Color)') &&
         compose.includes('1 -> { // Show Modes: sparkle') &&
         compose.includes('2 -> { // Layout: four squares') &&
         compose.includes('3 -> { // Triggers: lightning') &&
         compose.includes('4 -> { // Auto Events: sun'),
         'Native navigation icons must preserve locked meanings without emoji or WebView');
assert(bleSource.includes('frameCapture.cancel();palette.reset(); // GATT close may never produce a disconnect callback') &&
       bleSource.includes('infoReadPending') && bleSource.includes('frameCapture.busy()') &&
        bleSource.includes('nextPacket!=null') && bleSource.includes('cfgBusy') &&
        bleSource.includes('palette.busy()') && bleSource.includes('triggerMapBusy') &&
       bleSource.includes('queuedOutputOwner!=gatt || !canConfigure()') &&
       compose.includes('outputs.clear() // readback from a disconnected/unauthorized controller') &&
       compose.includes('captureMessage.value = "CORE frame not captured"'),
       'Clear stale CORE output UI and prevent conflicting GATT operations');
assert(!/WebView\(|BottomNavigation|NavigationBar\(/.test(compose),
  'Active Compose must have no WebView or bottom navigation');
const homeSection=compose.split('private fun HomePage(t: Tone, changePage: (Int) -> Unit) {')[1]
                         .split('private fun EffectsPreview(')[0];
const settingsSection=compose.split('private fun SettingsPage(')[1]
                             .split('private fun HardwareOutputEditor(')[0];
assert(settingsSection.includes('Pill("Preview Offline", t)') &&
       settingsSection.includes('enterOfflinePreview()') &&
       settingsSection.includes('changePage(1)') &&
       settingsSection.includes('ConnectPill {') &&
       compose.includes('private fun enterOfflinePreview()') &&
       compose.includes('offlinePreview.value = true') &&
       compose.includes('livePreview.stop()'),
       'Locked Settings Connect and offline preview regression');
assert(compose.includes('4 -> AutoEventsPage(t) { effectId ->') &&
       compose.includes('autoEventPreviewEffect.intValue = effectId\n                        enterOfflinePreview()') &&
       !compose.includes('selectedEffect.intValue = effectId') &&
       compose.includes('Offline preview inspects exact design pixels; CORE provides animation.'),
       'Auto Events offline preview must use the same BLE-isolated session as Settings');
const presetBlock=compose.split('private fun PresetsPage(t: Tone) {')[1]
                         .split('private fun LayoutPage(t: Tone) {')[0];
assert(model.includes('JSONArray previous = data.optJSONArray("savedPresets")') &&
       model.includes('data.put("savedPresets", previous == null ? new JSONArray() : previous)') &&
       model.includes('if (save()) return true;'),
       'Persisted preset updates must roll back after a failed disk commit');
assert(presetBlock.includes('snapshot.put("savedPresets", JSONArray())') &&
       presetBlock.includes('design.put("savedPresets", JSONArray(project.presets().toString()))') &&
       presetBlock.includes('presetStatus = if (restored)') &&
       !presetBlock.includes('.put("studio", project.exportJson())'),
       'Presets may not recursively embed or erase their own library');
assert(homeSection.includes('remember(bleMessage.value) { ble.connectionPhase().replaceFirstChar { it.uppercase() } }') && settingsSection.includes('remember(bleMessage.value) { ble.connectionPhase().replaceFirstChar { it.uppercase() } }') && bleSource.includes('String connectionPhase()'),
       'Bluetooth status pill must reflect actual connection state, not transient messages');
assert(homeSection.includes('ble.authorizeOrPair()') && settingsSection.includes('ble.authorizeOrPair()') && bleSource.includes('BleOwnerVault.load(activity,activeAddress)==null') && bleSource.includes('if (!connected)'),
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
       compose.includes('ExactLedFrame.fromProjectModel(project)') &&
       compose.includes('Color(frame!!.colorAt(sample))') &&
        compose.includes('captureMessage.value = "Verified CORE RGB888 snapshot · output "'),
  'Compose must show verified CORE frame provenance, not invented animation');
assert(bleSource.includes('FRAME_CMD_UUID') &&
       bleSource.includes('FRAME_RSP_UUID') &&
       bleSource.includes('frameCapture.configure(') &&
       bleSource.includes('frameCapture.read(connection') &&
       bleSource.includes('frameCapture.written(connection') &&
       bleSource.includes('frameCapture.busy()') &&
       bleSource.includes('frameCapture.abortForStop()'),
  'BLE CORE snapshot must be capability-gated and serialize GATT requests');
// Owner physical Test 38: read-only Output 1 reload now PASS. Restore
// responsive preview between explicit reads without turning BLE BUSY into a
// 60Hz status flood or inventing/smoothing unverified frame pixels.
const captureLoop = compose.split('private fun EffectsPreview(t: Tone, effect: Int) {')[1]
  ?.split('Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(20.dp))')[0];
const captureStart = bleSource.split('boolean captureFrame(int output) {')[1]
  ?.split('private final ArrayAdapter<String> entries;')[0];
assert(captureLoop && captureLoop.includes('delay(16L)') &&
       !captureLoop.includes('delay(500)') &&
       captureLoop.includes('while(foregroundActivity.value && !offlinePreview.value && !previewPaused.value)') &&
       captureStart &&
       captureStart.includes('if(!canCaptureFrame())return false;') &&
       captureStart.includes('queuedOutputRead>=0') &&
       captureStart.includes('frameCallbackPending') &&
       captureStart.includes('nextPacket!=null') &&
       captureStart.includes('palette.busy()') &&
       !captureStart.includes('notifyStatus(') &&
       captureStart.includes('frameCapture.request(output)') &&
       compose.includes('Color(frame!!.colorAt(sample))'),
       'Fast live preview must be GATT-paced, silently backpressured, STOP-safe and CORE-authoritative');

// Owner-confirmed UI regression after PR #71: empty/cold CORE frames must
// not erase the approved 30-capsule preview's known configured colors; fresh
// uninstalls can also erase all local project-painted pixels. Role colors must
// originate from explicit CORE Output Manager readback, never claimed to be
// a physical LED frame. Restore existing pill +/- controls on loaded hardware.
const previewSrc=compose.split('private fun EffectsPreview(t: Tone, effect: Int) {')[1]
  ?.split('private fun ShowModesPage(t: Tone)')[0];
const hardwareOutput=compose.split('private fun HardwareOutputEditor(t: Tone) {')[1];
assert(previewSrc &&
       previewSrc.includes('val actualFrame = coreFrame.value') &&
       previewSrc.includes('val hasLiveColors = actualFrame != null') &&
       previewSrc.includes('val readback = outputs[captureOutput.intValue]') &&
       previewSrc.includes('configuredZones.optJSONObject(i)') &&
       previewSrc.includes('rolePixelColor(zone.optString("role","off"))') &&
       previewSrc.includes('Configured zone colors') &&
       previewSrc.includes('Project pixel colors') &&
       previewSrc.includes('Color(frame!!.colorAt(sample))') &&
       previewSrc.includes('val columns = 30') &&
       !previewSrc.includes('SIMULATED') &&
       hardwareOutput &&
       hardwareOutput.includes('ValueSlider("Total output pixels"') &&
       hardwareOutput.includes('ValueSlider("Next start · zero-based"') &&
       (hardwareOutput.match(/PixelStepPill\("−",t\)/g)||[]).length >= 2 &&
       (hardwareOutput.match(/PixelStepPill\("\+",t\)/g)||[]).length >= 2 &&
       compose.includes('Modifier.size(width=56.dp,height=48.dp)') &&
       hardwareOutput.includes('adjustZone(i,n)') &&
       hardwareOutput.includes('adjustOutput(n)') &&
       hardwareOutput.includes('Physical boundaries change only after confirmed NVS Save'),
       'Configured-color fallback and zero-NVS pixel +/- controls must be visible without editing locked preview geometry');
// Owner's full Test 1-80 regression gates (Android-only source contracts).
assert(compose.includes('effectSettings.put(key,prefs.getInt(key,fallback))') &&
       compose.includes('selected?.optJSONObject("effectSettings")') &&
       compose.includes('if(editor.commit()) refreshLoadedProject()') &&
       compose.includes('rememberSaveable(projectLoadRevision.intValue)') &&
       compose.includes('offlinePreview.value -> "Offline design preview"') &&
       compose.includes('val fill = if (selected) (if(t.bg == Color.Black)') &&
       compose.includes('firmwareVersion.value ?: "Unavailable"') &&
       compose.includes('firmwareVersion.value ?: "Version unavailable"') &&
       compose.includes('mutableStateOf<String?>(null) // collapse cards on Layout page entry') &&
       compose.includes('NativeEffectCatalog.generalEffects().filter {') &&
       compose.includes('NativeEffectCatalog.generalEffects().forEach { effect ->'),
       'Owner Tests 49/79/80, firmware version, +/-, event exclusivity and collapsed Layout cards regressed');
// Owner Test 86: never truncate the transition picker to the first fifteen,
// or leak Auto Event previews into general Show Modes effect selection.
const autoEvents=compose.split('private fun AutoEventsPage(t: Tone, previewEffect: (Int) -> Unit) {')[1]
  ?.split('private fun CorePaletteTransferPanel(')[0];
const generalEffects=compose.split('private fun ShowModesPage(t: Tone) {')[1]
  ?.split('private fun SequencePage(')[0];
assert(catalog.includes('static List<Effect> generalEffects()') &&
       catalog.includes('if (!autoEventOnly(effect.id)) general.add(effect)') &&
       catalog.includes('static List<Effect> autoEventChoices()') &&
       catalog.includes('choices.sort(Comparator.comparing((Effect e) -> !autoEventOnly(e.id))') &&
       catalog.includes('id==32769 || id==32770 || id==32771 || id==32773') &&
       catalog.includes('id==32787') &&
       generalEffects.includes('NativeEffectCatalog.generalEffects()') &&
       autoEvents.includes('NativeEffectCatalog.autoEventChoices()') &&
       !autoEvents.includes('.take(15)') &&
       autoEvents.includes('previewEffect(selected.id)') &&
       autoEvents.includes('EffectsPreview(t, eventPreviewId)') &&
       compose.includes('autoEventPreviewEffect.intValue = effectId') &&
       !compose.includes('selectedEffect.intValue = effectId') &&
       !compose.includes('enterOfflinePreview()\\n                        page = 1') &&
       compose.includes('NativeEffectCatalog.generalEffects().forEach { effect ->'),
       'Test 86: full Auto Events picker and exclusive transitions across shows, triggers and preview');
assert(bleSource.includes('setInfoListener(') &&
       bleSource.includes('infoReadQueued=true;pumpQueue()') &&
       bleSource.includes('if(infoReadQueued && coreInfo!=null && gatt!=null)') &&
       bleSource.includes('infoReadPending'),
       'Actual firmware INFO metadata must remain safely serialized after owner authorization');
assert(gradle.includes("versionName '0.2.0-alpha4'"));
// Verify the actual released Compose UI; obsolete Java Views are NOT a release oracle.
const layout=compose.split('private fun LayoutPage(t: Tone) {')[1].split('private fun addZoneAfter(')[0];
const triggers=compose.split('private fun TriggersPage(t: Tone) {')[1].split('private fun AutoEventsPage(')[0];
const modes=compose.split('private fun ShowModesPage(t: Tone) {')[1].split('private fun SequencePage(')[0];
const events=compose.split('private fun AutoEventsPage(t: Tone, previewEffect: (Int) -> Unit) {')[1]
                    .split('private fun SettingsPage(')[0];
const hardware=compose.split('private fun HardwareOutputEditor(t: Tone) {')[1];
// Locked Layout: Add Zone and Delete Zone stay side-by-side, never a
// destructive one-tap action. Target selection precedes confirmation.
assert(layout.includes('Pill("+ Add",t) {') && layout.includes('val created = addZoneAfter(i)') && layout.includes('Pill("Remove",t) { deleteId=id }') && layout.includes('title = { Text("Remove this zone?") }') && layout.includes('if (project.removeZone(id)) {'),
       'Layout Add/Delete placement and two-stage safe zone removal regressed');
assert(layout && triggers && modes && events && hardware,
       'A shipped Compose design page is missing');
for(const required of ['project.previewZoneOffsets(id, n)',
  'CapsuleSlider(n, 1, 10000, t,', 'finished={ applyLiveZone() }',
  'project.editZone(id,label,n,reverse)',
  'project.setPixelColor(id,activePixel,hex)',
  'project.clearPixelColor(id,activePixel)',
  'project.removeZone(id)', 'project.moveZone(',
  'Pill("+ Add"', 'Pill("Remove"']) {
  assert(layout.includes(required),'Compose layout regression: '+required);
}
assert(!layout.includes('ble.saveOutput(') && !layout.includes('ble.showFx('),
 'Local layout authoring must not persist or command physical outputs');
for(const required of [
  'var selections by remember', 'selections=if(selected) selections-name else selections+name',
  'editing = true', 'project.customTrigger(newTrigger)',
  'project.assignMany(selections,zoneId,!checked)',
  'Pill("Edit Selected", t, selections.isNotEmpty(), enabled=selections.isNotEmpty())',
  'CoreTriggerMappingPage(t)'
]) assert(triggers.includes(required),'Assignment-only Triggers regression: '+required);
assert(triggers.includes('IconAction("Edit " + name, "edit", t)') &&
       triggers.includes('if (profile.optString("kind") == "trigger-custom")') &&
       triggers.includes('IconAction("Delete " + name, "delete", t)') &&
       triggers.includes('title = { Text("Delete custom trigger?") }') &&
       triggers.includes('project.deleteTrigger(target)') &&
       triggers.includes('remember(selections, modelRevision)') &&
       triggers.includes('disabled()'),
       'Builtins cannot be deleted; custom trigger confirmation and bulk editing refresh required');
const settings=compose.split('private fun SettingsPage(')[1]
  .split('private fun HardwareOutputEditor(')[0];
assert(settings.includes('Developer · Trigger Testing') &&
       settings.includes('var simulatedTriggers by remember') &&
       settings.includes('defaults.chunked(2).forEach') &&
       settings.includes('Clear test states') &&
       settings.includes('Local simulation only') &&
       !settings.includes('ble.simulate'),
       'Developer-only trigger testing belongs in Settings and must not touch physical GPIO');
assert(model.includes('.put("zones", new JSONArray())') &&
       model.includes('"Park", "Brake", "Left Turn", "Right Turn", "Hazards", "Reverse"') &&
       model.includes('!"trigger-custom".equals(p.optString("kind"))'),
       'Universal trigger defaults must remain unassigned and undeletable');
assert(!triggers.includes('ble.saveOutput('),'Offline trigger design must not write NVS');
for(const required of ['EffectsPreview(t, effect)', 'listOf("Effects", "Presets", "Sequences")',
  'PaletteEditor("Color"', 'PaletteEditor("Background"',
  'previewCurrentEffect(item.id)', 'NativeEffectParameters.forId(effect)']) {
  assert(modes.includes(required),'Compose effects regression: '+required);
}
assert(events.includes('listOf("Startup", "Shutdown")') &&
       events.includes('eventUiEnabled') &&
       events.includes('eventUiFadeMs') &&
       events.includes('eventDuration') &&
       events.includes('Preview offline') &&
       events.includes('project.setAutoEvent(event, field, value)') &&
       events.includes('if (saved) refresh()') &&
       events.includes('Could not save '),
  'Optional startup/shutdown design and explicit persistence errors regressed');
assert(model.includes('synchronized boolean setAutoEvent(String event, String key, Object value)') &&
       model.includes('if (!("Startup".equals(event) || "Shutdown".equals(event))) return false;') &&
       model.includes('if (!Double.isFinite(seconds) || seconds < 0.2 || seconds > 30.0)') &&
       model.includes('return save();'),
       'Event authoring must be validated and saved atomically');
assert(hardware.includes('BleConfigCodec.MAX_LEDS_PER_OUTPUT') &&
       !hardware.includes('623') &&
       hardware.includes('it.put("pixels", remaining)') &&
       hardware.includes('structureVersion++') &&
       hardware.includes('BleConfigCodec.encode(draft)') &&
       hardware.includes('ble.saveOutput(index, draftText)'),
  'CORE output editor must validate universal drafts and synchronize zone totals');
// A rejected GATT save is not an in-flight hardware commit. Readback, not
// request admission, remains the only success evidence.
assert(bleSource.includes('boolean saveOutput(int index,String json)') &&
       bleSource.includes('Load this output over BLE before saving",null,index);return false;') &&
       bleSource.includes('BLE is busy; retry after current transfer",null,index);return false;') &&
       hardware.includes('if (ble.saveOutput(index, draftText))') &&
       hardware.includes('Save request rejected · see BLE status'),
  'Output Manager must not claim a rejected save is verifying hardware');
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
assert(model.includes('data = previous; // a failed disk commit') &&
       model.includes('pixel>=zone(zoneIndex).optInt("length",0)'),
       'Project import must be atomic and pixel editing must respect zone bounds');
assert(!/GS400|GPIO16|GPIO17|522 pixels|1246/i.test(model),
  'Native project is universal, not specific to bench vehicle');
// Shared root-cause guard: project JSON is not Compose State. Each editor
// must observe committed revisions directly without destroying editor state.
const presetsEditor = compose.split('private fun PresetsPage(t: Tone) {')[1]
  ?.split('private fun LayoutPage(')[0];
assert(layout.includes('val zoneCount = remember(modelRevision) { project.zoneCount() }') &&
       layout.includes('for(i in 0 until zoneCount)') &&
       triggers.includes('val profiles = remember(modelRevision) { JSONObject(project.profiles().toString()) }') &&
       triggers.includes('val assigned=profiles.optJSONObject(name)?.optJSONArray("zones")') &&
       events.includes('val eventProfiles = remember(modelRevision)') &&
       events.includes('val event = eventProfiles.getValue(key)') &&
       presetsEditor?.includes('val presets = remember(modelRevision) { JSONArray(project.presets().toString()) }'),
       'Project editors must redraw from a committed revision without app restart');
assert(compose.includes('var dragValue by remember { mutableFloatStateOf(committed) }') &&
       compose.includes('value = if (dragging) dragValue else committed') &&
       triggers.includes('project.editTriggerParameter(selections, "speed", value)') &&
       triggers.includes('project.editTriggerParameter(selections, "intensity", value)') &&
       triggers.includes('project.editTriggerParameter(selections, "brightness", value)') &&
       events.includes('eventEditing=if(eventEditing==key) null else key') &&
       compose.includes('formatDurationSeconds(item.optInt("ms", 1500))') &&
       compose.includes('BigDecimal(editedDuration).movePointRight(3).intValueExact()') &&
       layout.includes('PixelStepPill("−",t)') &&
       layout.includes('PixelStepPill("+",t)'),
       'Requested live sliders, card toggle, decimal seconds and pixel hit areas regressed');
assert(palettes.includes('parseVerifiedCustom(verifiedCustomStops)') &&
       palettes.includes('coreFixedStops(id, foreground, background)') &&
       compose.includes('val chosen = sourcePalette(value)') &&
       compose.includes('val palette = sourcePalette(id)'),
       'Palette gradients must use real CORE or verified readback metadata only');
const numericTest=fs.readFileSync('android/app/src/test/java/com/opentail/android/NativeProjectCommitTest.java','utf8');
const stopTest=fs.readFileSync('android/app/src/test/java/com/opentail/android/NativePaletteMetadataTest.kt','utf8');
for(const scenario of ['triggerLiveParametersPersistImmediatelyAndRollbackAtomically',
 'offlineEventFadeAndDurationPersistAcrossReopen',
 'presetRenameCommitsImmediatelyAndReopenSeesSameText'])
  assert(numericTest.includes(scenario),'Numeric/offline local commit test missing: '+scenario);
assert(stopTest.includes('pinnedCoreFixedColorPalettesUseActualForegroundAndBackground') &&
       stopTest.includes('onlyVerifiedCustomReadbackCanRenderDevicePalette'),
       'Source-verified gradient tests missing');

// The storage rollback invariant must be exercised with real JSON and
// deterministic failed SharedPreferences.commit(), not only static source text.
const storageTest=fs.readFileSync(
  'android/app/src/test/java/com/opentail/android/NativeProjectCommitTest.java','utf8');
for (const required of [
 'failedCommitRestoresEveryNestedEditAndExport',
 'triggerMultiSelectCommitsOnceOrNotAtAll',
 'triggerStyleBatchIsAtomicAndRejectsIncompleteSelection',
 'autoEventsAreValidatedAndCommittedAtomically',
 'failedReindexBeforeCommitCannotLeavePartialLayout',
 'store.fail = true',
 'assertEquals(before, project.exportJson())'
]) assert(storageTest.includes(required), 'Storage transaction JUnit proof missing: '+required);
const buildFile=fs.readFileSync('android/app/build.gradle','utf8');
assert(buildFile.includes("testImplementation 'org.json:json:20240303'"),
  'Native project failure-injection tests require executable JSONObject implementation');

// Output Manager readbacks are asynchronous UI deliveries, too: a reply
// from a retired CORE must not resurrect its saved output on a new device.
// Committed-but-pending geometry is never described as an active save.
assert(bleSource.includes('final BluetoothGatt owner=gatt;') &&
       bleSource.includes('if(!stopped && owner==gatt && configListener!=null)') &&
       bleSource.includes('CORE active output differs; may be pending trigger release.') &&
       bleSource.includes('Reload after release; no active change verified') &&
       bleSource.includes('cfgSaving && (cfgExpected==null||!Arrays.equals(cfgBytes,cfgExpected))'),
       'Output Manager must reject stale controller callbacks and fail closed on pending geometry');

// STOP queued behind a GATT transaction must drain before any new capture,
// owner/auth, effect-capability, or output-config GATT operation starts.
assert(bleSource.includes('frameCapture.busy() || palette.busy() || frameCallbackPending || cfgBusy || queuedOutputRead>=0 || infoReadPending || infoReadQueued || writePending || nextPacket!=null ||') &&
       (bleSource.match(/frameCapture\.busy\(\)\|\|palette\.busy\(\)\|\|nextPacket!=null\|\|authPending/g)||[]).length >= 3 &&
       (bleSource.match(/frameCapture\.busy\(\)\|\|palette\.busy\(\)\|\|nextPacket!=null\|\|pairPending/g)||[]).length === 2 &&
       bleSource.includes('private boolean cfgSend(byte[] packet)') &&
       bleSource.includes('return cfgSend(new byte[]{2,3,(byte)index') &&
       bleSource.includes('if(!ok){cfgFail("BLE configuration write did not start");return false;}') &&
       bleSource.includes('fxCaps=null;fxCapsBusy=false;triggerMapping=null;'),
  'Queued STOP must have priority and an immediate BLE write refusal must not be acknowledged as sent');

// Physical phone Test 38: 140 effects read successfully over BLE but
// Layout Reload Output1 was repeatedly blocked by the 500ms frame-preview
// capture loop. Explicit READ must win without changing locked visual UI.
const outputRead = bleSource.split('void readOutput(int index){')[1]
  ?.split('boolean saveOutput(int index,String json)')[0];
const queuedReadPump = bleSource.split('private void pumpQueue(){')[1]
  ?.split('private String preferredPending')[0];
assert(outputRead &&
       outputRead.includes('if(frameCapture.busy()){') &&
       outputRead.includes('queuedOutputRead=index;') &&
       outputRead.includes('queuedOutputOwner=gatt;') &&
       outputRead.includes('frameCapture.abortForStop();') &&
       outputRead.includes('CORE snapshot still busy; GATT recovery required') &&
       outputRead.includes('beginOutputRead(index);') &&
       queuedReadPump &&
       queuedReadPump.indexOf('if(nextPacket!=null)') < queuedReadPump.indexOf('if(queuedOutputRead>=0)') &&
       queuedReadPump.includes('queuedOutputOwner!=gatt || !canConfigure()') &&
       queuedReadPump.includes('beginOutputRead(index);') &&
       bleSource.includes('cfgBusy || queuedOutputRead>=0 || infoReadPending || infoReadQueued || writePending') &&
       bleSource.includes('CORE snapshot ATT timed out during output reload') &&
       bleSource.includes('clearQueuedOutputRead();') &&
       bleSource.includes('if(writePending||readReplyPending||cfgBusy||fxCapsBusy||infoReadPending||triggerMapBusy||'),
       'Output reload must suspend telemetry, drain ATT, honor STOP, and fail closed on timeout');

// Failed output saves can have committed before readback failed. Require a
// fresh revision from CORE, never reuse the possibly stale revision.
assert(bleSource.includes('if(cfgSaving && index>=0 && index<cfgKnownRevision.length)') &&
       bleSource.includes('cfgKnownRevision[index]=-1;') &&
       bleSource.includes('cfgLatest[index]=null;') &&
       bleSource.includes('cfgBusy=false;cfgSaving=false;cfgWaitingWrite=false;'),
       'Failed output save must invalidate cached firmware revision before retry');

// When ATT cannot start a STOP, or its write/readback fails, deferred FX
// must not restart the show. An earlier FX failure may still drain a STOP.
for (const guard of [
  'private void discardFollowingFxIfStopFailed()',
  'if (isStopPacket(pendingPacket)) {',
  'nextPacket=null;',
  'afterStopPacket=null;',
  'ui.post(this::pumpQueue);',
  'if(!ok)commandWriteStartFailed("BLE write could not start")',
  'catch(SecurityException ex){commandWriteStartFailed("BLE permission missing");}',
  'if (response.startsWith("ERR")) discardFollowingFxIfStopFailed();'
]) assert(bleSource.includes(guard), 'Failed STOP may release deferred FX: '+guard);
const stopFailureBlock=bleSource.split('void onCharacteristicWrite(')[1]?.split('private Runnable thisAutoAuth')[0];
assert(stopFailureBlock &&
       (stopFailureBlock.match(/discardFollowingFxIfStopFailed\(\)/g)||[]).length >= 4,
       'Failed write and readback paths must not release deferred FX following STOP');

// CORE's current ble_command.c returns ERR AUTH for expired/rejected owner
// sessions. A later command error must revoke stale Android authorization.
const replyPath=bleSource.split('private void acceptReply(byte[] raw,int status)')[1]
  ?.split('@SuppressWarnings("MissingPermission")')[0];
const revokePath=bleSource.split('private void revokeOwnerSession(){')[1]
  ?.split('private void acceptReply(')[0];
const cfgErrorPath=bleSource.split('private void cfgResponse(byte[] packet,int status)')[1]
  ?.split('private void cfgReadNext()')[0];
assert(replyPath && replyPath.includes('if("ERR AUTH".equals(response)) revokeOwnerSession();') &&
       cfgErrorPath && cfgErrorPath.includes('if(code==5) revokeOwnerSession();') &&
       revokePath && revokePath.includes('authorized=false;') &&
       revokePath.includes('Arrays.fill(cfgKnownRevision,-1);') &&
       revokePath.includes('Arrays.fill(cfgLatest,null);') &&
       revokePath.includes('fxNotify("[]");') &&
       revokePath.includes('nextPacket=null;') &&
       revokePath.includes('afterStopPacket=null;'),
       'Both CORE auth-error protocols must invalidate owner state and output revisions');

// Repeated Authorize/Pair is idempotent. A legacy token auth cannot turn
// an in-flight FX/STOP reply into a false failed authentication.
const combinedAuth=bleSource.split('void authorizeOrPair() {')[1]?.split('void pairWithBoot()')[0];
const tokenAuth=bleSource.split('void authorize(String tokenHex) {')[1]?.split('void show(')[0];
assert(combinedAuth && combinedAuth.includes('if (canControl()) { notifyStatus("BLE already authorized"); return; }') &&
       tokenAuth && tokenAuth.includes('if (cfgBusy || writePending || readReplyPending || fxCapsBusy || infoReadPending || triggerMapBusy ||') &&
       tokenAuth.includes('frameCapture.busy() || palette.busy() || nextPacket != null || pairPending ||') &&
       tokenAuth.includes('challengePending || authPending) {') &&
       tokenAuth.indexOf('BLE busy; retry authorization') < tokenAuth.indexOf('authPending=true;'),
       'Repeated authorization must not overlap active ATT operations or waiting STOP');

// A rejected capability read must release queued STOP; a missing long-read
// callback cannot leave ATT and config reads wedged indefinitely. Recovery
// closes the stale GATT owner rather than issuing a concurrent ATT command.
const capsStart=bleSource.split('private void readFxCapabilitiesInternal() {')[1]?.split('private void fxNotify')[0];
const capsCallback=bleSource.split('private void fxRead(byte[] raw,int status){')[1]?.split('private BluetoothGattCharacteristic enroll')[0];
assert(capsStart && capsStart.includes('fxCapsBusy=false;') &&
       capsStart.includes('catch(SecurityException ex){') &&
       capsStart.includes('pumpQueue();') &&
       capsStart.includes('lease!=fxCapsWatchLease || !fxCapsBusy') &&
       capsStart.includes('recoverStalledGatt(owner,"CORE effects catalog ATT read timed out")') &&
       capsCallback && capsCallback.includes('++fxCapsWatchLease;') &&
       capsCallback.includes('String ids=BleFxCapabilities.decode(raw);') &&
       capsCallback.includes('pumpQueue();') &&
       bleSource.includes('readFxCapabilitiesAfterAuth()') &&
       bleSource.includes('if (fxCapsAutoReadSuppressed) {'),
       'Capability-read rejection, timeout and completion must not strand a queued STOP');

// A rejected optional CORE info read must not strand auto-auth. Normal
// successful INFO callbacks still handle owner authentication once.
const servicesBlock=bleSource.split('void onServicesDiscovered(')[1]
  ?.split('void onCharacteristicRead(')[0];
assert(servicesBlock && servicesBlock.includes('infoStarted=connection.readCharacteristic(info);') &&
       servicesBlock.includes('if (!infoStarted) {') &&
       servicesBlock.includes('ui.post(thisAutoAuth(connection));') &&
       servicesBlock.includes('catch (SecurityException ex)') &&
       bleSource.includes('if (challenge!=null && BleOwnerVault.load(activity,activeAddress)!=null)') &&
       bleSource.includes('if(status==BluetoothGatt.GATT_SUCCESS && value!=null)') &&
       bleSource.includes('finishInfoRead(connection,value,status);') &&
       bleSource.includes('finishInfoRead(connection,characteristic.getValue(),status);') &&
       bleSource.includes('if (!stopped && owner==gatt && connected && !canControl()) authorizeStored();'),
       'Rejected optional CORE info read must preserve owner auto-auth');

// MTU negotiation failures fall through to guarded service discovery.
const mtuFlow=bleSource.split('void onConnectionStateChange(')[1]
  ?.split('void onServicesDiscovered(')[0];
assert(mtuFlow && mtuFlow.includes('if(!mtuNegotiating) discoverCoreServices(connection);') &&
       mtuFlow.includes('discoverCoreServices(connection);') &&
       mtuFlow.includes('if(!connection.discoverServices())') &&
       mtuFlow.includes('catch(SecurityException ex)') &&
       mtuFlow.includes('BLE service discovery could not start; reconnect CORE'),
       'MTU/service discovery failure must not silently strand the GATT session');

// Android connectGatt can reject without a callback or exception. Never
// leave the app in a phantom connecting state on a null GATT handle.
const connectBlock=bleSource.split('private void connectReadOnly(BluetoothDevice device,boolean knownDevice) {')[1]
  ?.split('private final BluetoothGattCallback callback')[0];
assert(connectBlock &&
       connectBlock.includes('if(gatt==null){') && connectBlock.includes('watchGattReady(gatt,attempt)') &&
       connectBlock.includes('connecting=false;scanFallbackAllowed=false;notifyStatus("BLE connection permission denied");'),
       'Rejected GATT connection start must clear the connecting state');

// Final approved Compose controls retain their styling while using #58 BLE.
assert(compose.includes('ble.connectFromApp(prefs.getString("preferred_ble_address", null))') &&
       compose.includes('ble.setPreferredListener { address ->') &&
       compose.includes('ble.tryReconnect(it)') &&
       bleSource.includes('private void scheduleReconnect(String address, boolean afterScan)') &&
       bleSource.includes('scheduleReconnect(connectingAddress,fallback);') &&
       bleSource.includes('private static final long OWNER_TRANSACTION_TIMEOUT_MS=8000L;') &&
       bleSource.includes('private static final long COMMAND_TRANSACTION_TIMEOUT_MS=8000L;') &&
       !/removeBond\s*\(/.test(bleSource),
       'Approved UI must use BLE #58 known CORE reconnect and secure bounded recovery');
assert(layout.includes('if (created != null) { editingId=created; layoutMessage="" }') &&
       layout.includes('else layoutMessage="Could not add zone"') &&
       compose.includes('private fun addZoneAfter(index: Int): String?') &&
       compose.includes('val id=project.zone(position)?.optString("id")'),
       'Add Zone must select its committed ID or show persistence failure');

// A retired scan's delayed timeout must not cancel a new scan. A queued
// scan result must never reopen GATT after scan stop or Activity shutdown.
assert(bleSource.includes('private volatile int scanGeneration=0;') &&
       bleSource.includes('final int scanLease=++scanGeneration;') &&
       bleSource.includes('if (!scanning || scanGeneration!=scanLease || stopped) return;') &&
       bleSource.includes('if(stopped || !scanning || scanGeneration!=scanLease)return;') &&
       (bleSource.match(/final int scanLease=scanGeneration;/g)||[]).length === 2 &&
       bleSource.includes('scanning = false;\n        scanGeneration++;'),
       'BLE scan generations and shutdown must invalidate deferred callbacks');

// GATT callback ownership must be checked on both modern/legacy Android
// callback overloads. A retired controller must not erase live new-device
// state, reuse FX capabilities, or deliver old authorization/config replies.
assert(bleSource.includes('private volatile BluetoothGatt gatt;') &&
       bleSource.includes('if (stopped || connection != gatt) {') &&
       (bleSource.match(/if\(stopped \|\| connection!=gatt\)return;/g)||[]).length >= 5 &&
       bleSource.includes('final BluetoothGatt owner=gatt;') &&
       bleSource.includes('if(owner==gatt && fxCapabilitiesListener!=null)') &&
       bleSource.includes('fxCapabilitiesListener.accept(connected && authorized ? json : "[]");'),
       'BLE must drop stale disconnected-controller GATT callbacks and capabilities');

// Trigger-team contract V1 is optional on older CORE. Android may edit a
// physical IN1–IN8 map only after owner auth, explicit Save, readback and
// revision/mapping verification. No speculative GPIO hardcoding or writes.
const triggerCodec=fs.readFileSync(
  'android/app/src/main/java/com/opentail/android/NativeTriggerMappingCodec.java','utf8');
const triggerCodecTest=fs.readFileSync(
  'android/app/src/test/java/com/opentail/android/NativeTriggerMappingCodecTest.java','utf8');
for (const required of [
 'static final int READ_BYTES = 15;',
 'static final int WRITE_BYTES = 11;',
 'wire[0] = VERSION;',
 'wire[1] = (byte)expectedRevision;',
 'wire[2] = (byte)(expectedRevision >>> 8);',
 'functions[i] < 0 || functions[i] > 8',
 'final boolean persisted;',
 'return Arrays.equals(functions, expected);',
 'boolean confirmsApplied(int baseRevision, int[] expected)',
 'baseRevision >= 1 && baseRevision < 65535',
 'revision == baseRevision + 1 && matches(expected);'
]) assert(triggerCodec.includes(required), 'Trigger team GATT V1 codec regressed: '+required);
for (const required of [
 'decodesExactFifteenByteReadbackIncludingAuxAndPendingApply',
 'encodesExactlyElevenBytesWithRevisionAndDuplicateFunctions',
 'rejectsMalformedVersionsAndInvalidFunctionIds',
 'exposesBusyStaleAndNvsFailureAsFailuresNotSuccess',
 'requiresAuthoritativeAppliedReadbackNotJustAttWriteAck'
]) assert(triggerCodecTest.includes(required),
  'Byte-accurate trigger-mapping JUnit contract missing: '+required);
for (const required of [
 'TRIGGER_MAP_UUID = UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf2c")',
 'boolean canMapTriggers()',
 'boolean readTriggerMap()',
 'boolean saveTriggerMap(int expectedRevision,int[] mapping)',
 'if(connection!=gatt || !triggerMapBusy || !canMapTriggers())return;',
 'nextPacket!=null||authPending||pairPending||challengePending',
 'if(connection==gatt && canMapTriggers() && triggerMapListener!=null)',
 'result.confirmsApplied(triggerMapBaseRevision,triggerMapExpected)',
 'triggerMapBusy',
 'NativeTriggerMappingCodec.decode(raw)',
 'triggerMapBaseRevision+1',
 'result.confirmsApplied(triggerMapBaseRevision,triggerMapExpected)',
 'if (TRIGGER_MAP_UUID.equals(characteristic.getUuid())) {triggerMapWritten(connection,status);return;}'
]) assert(bleSource.includes(required), 'Authenticated GATT trigger map integration regressed: '+required);
assert(compose.includes('CoreTriggerMappingPage(t)') &&
       compose.includes('val canMap = remember(bleMessage.value) { ble.canMapTriggers() }') &&
       compose.includes('if (!canMap) return // Older firmware keeps offline authoring intact') &&
       compose.includes('ble.readTriggerMap()') &&
       compose.includes('ble.saveTriggerMap(snapshot.revision, draft.toIntArray())') &&
       compose.includes('Save mapping to CORE · verify readback') &&
       compose.includes('Save physical input mapping?') &&
       !compose.includes('GPIO4–11') &&
       !compose.includes('gpio = '),
       'Input mapping must stay native, optional and explicitly saved after readback');

// Provider-controlled OpenDocument streams must be bounded before parsing
// and CreateDocument must report failure when no writable stream is returned.
const documentIo=fs.readFileSync(
  'android/app/src/main/java/com/opentail/android/NativeProjectDocumentIo.java','utf8');
const documentIoTest=fs.readFileSync(
  'android/app/src/test/java/com/opentail/android/NativeProjectDocumentIoTest.java','utf8');
for(const required of ['static final int MAX_CHARS = 2_000_000;',
  'static String readBounded(Reader input) throws IOException',
  'if (count > MAX_CHARS - text.length())',
  'text.append(chunk, 0, count);']) {
  assert(documentIo.includes(required), 'Untrusted v4 import size guard regressed: '+required);
}
for(const required of ['acceptsExactImportLimitAndPreservesCharacters',
  'rejectsOneCharacterOverTheLimitWithoutReturningPartialData',
  'propagatesRealDocumentIoErrors']) {
  assert(documentIoTest.includes(required), 'Bounded project reader unit tests missing: '+required);
}
assert(compose.includes('NativeProjectDocumentIo.readBounded(reader)') &&
       compose.includes('?: throw java.io.IOException("Cannot open selected document")') &&
       compose.includes('?: throw java.io.IOException("Cannot open output document")') &&
       compose.includes('output.bufferedWriter().use { it.write(project.exportJson()) }') &&
       !compose.includes('openInputStream(uri)?.bufferedReader()?.use { it.readText() }') &&
       !compose.includes('openOutputStream(uri)?.bufferedWriter()?.use'),
       'Project import/export must reject unavailable document providers without false success');

// Imported zero-based LED coordinates and sequence milliseconds must be
// typed whole JSON numbers. JSONObject.optInt silently truncates decimals.
for (const required of [
 'private static boolean wholeNumber(Object value, long min, long max)',
 '!wholeNumber(z.opt("start"), 0, 999999)',
 '!wholeNumber(z.opt("length"), 1, 10000)',
 '!wholeNumber(step.opt("ms"), 50, 600000)'
]) assert(model.includes(required), 'Strict universal layout/sequence import regression: '+required);
assert(storageTest.includes('importedPixelOffsetsAndDurationsRejectFractionalOrTextNumbers'),
  'Executable invalid-import regression missing');

// Imported/restored project names must refresh Compose's remembered field,
// and a failed zone write must not close the editor or remove confirmation.
assert(compose.includes('private val projectLoadRevision = mutableIntStateOf(0)') && compose.includes('var name by remember(projectLoadRevision.intValue)') && compose.includes('if (imported) refreshLoadedProject()') && compose.includes('refreshLoadedProject()') && compose.includes('if (project.removeZone(id)) {') && compose.includes('Could not save zone'),
       'Layout must follow committed project revisions and retain failed edit dialogs');

// The approved design must not be replaced by a green-tinted modal editor.
const layoutPart=compose.split('private fun LayoutPage(t: Tone) {')[1]
  ?.split('private fun addZoneAfter(')[0];
const settingsPart=compose.split('private fun SettingsPage(')[1]
  ?.split('private fun HardwareOutputEditor(')[0];
assert(!/0xFF79D99B|0xFF226E45|0xFF71DFA0/.test(compose) && compose.includes('Color(0xFF303030)'),
       'OLED/white UI must use neutral controls, never the rejected green accents');
assert(layoutPart && layoutPart.includes('if(editingId==id)') &&
       layoutPart.includes('ZoneDotPreview(t,n,reverse)') &&
       !layoutPart.includes('AlertDialog(onDismissRequest = { editingId = null }') &&
       layoutPart.includes('HardwareOutputEditor(t)') &&
       settingsPart && !settingsPart.includes('HardwareOutputEditor(t)'),
       'Layout owns inline zone expansion, dot preview, and CORE output manager');
assert(model.includes('synchronized boolean mapZoneToOutputSlot(') && compose.includes('project.mapZoneToOutputSlot(') && compose.includes('it !in occupied') && storageTest.includes('physicalOutputLinksAreUniqueBidirectionalAndLocalOnly') && !layoutPart.includes('ble.saveOutput('),
       'Bidirectional design zone/CORE physical output links must remain unique and saved locally');
const liveStrip=compose.split('private fun EffectsPreview(t: Tone, effect: Int) {')[1]
  .split('private fun ShowModesPage(')[0];
assert(liveStrip.includes('val columns = 30') && liveStrip.includes('val capsuleHeight = 24.dp.toPx()') && liveStrip.includes('val gap=3.dp.toPx()') && liveStrip.includes('Color(frame!!.colorAt(sample))') && !liveStrip.includes('for(row'),
       'Locked live preview: exactly three rows of unlabeled rounded LED capsules');

// Imported trigger dictionaries may omit older built-in defaults, but they
// must not contain malformed entries. A read must never persist new profiles.
assert(model.includes('if (inputProfiles.optJSONObject(profileName) == null)') &&
       model.includes('throw new JSONException("Invalid trigger profile: " + profileName);') &&
       model.includes('try { return defaultTrigger(name); }') &&
       !model.includes('profiles().put(name, p);save();') &&
       storageTest.includes('malformedTriggerProfilesNeverOverwriteSavedDataOnRead'),
       'Trigger project imports/readers must never silently replace invalid profiles');
assert(model.includes('if (candidate.has(key) && !(candidate.opt(key) instanceof JSONArray))') &&
       model.includes('if (candidate.has("selectedPixels")') &&
       model.includes('throw new JSONException("Invalid selectedPixels object");') &&
       storageTest.includes('malformedOptionalDesignerSectionsCannotEraseImportedWork'),
  'Malformed optional v4 designer data must fail without replacing saved work');

// Local editor mutations must not masquerade as saved after a failed
// SharedPreferences commit, including multi-selected trigger zone assignments.
for (const required of ['private String committedJson;',
  'committedJson = data.toString();', 'committedJson = next;',
  'data = new JSONObject(committedJson);',
  'catch (RuntimeException ex) { persisted = false; }',
  'synchronized boolean assignMany(Set<String> triggers, String zoneId, boolean enabled)',
  'JSONObject draft = new JSONObject(data.toString());',
  'data = draft;', 'return save();',
  'synchronized boolean styleTriggers(Set<String> triggers, String effect, String color)',
  'private void restoreCommitted()',
  'synchronized boolean name(String name)']) {
  assert(model.includes(required), 'Shared native project rollback/atomic-edit contract regressed: '+required);
}
// Pre-commit JSON/reindex errors must restore state too, not only failed
// SharedPreferences writes after mutation.
for (const method of ['addZoneAt', 'editZone', 'removeZone', 'moveZone']) {
  const block=model.split('synchronized boolean '+method+'(')[1]
    ?.split('synchronized ')[0];
  assert(block && block.includes('catch (JSONException e) { restoreCommitted(); return false; }'),
    'Zone '+method+' must restore last committed state when reindex fails');
}
assert(model.includes('return addZoneAt(label, length, zoneCount());') &&
       model.includes('synchronized boolean addZoneAt(String label, int length, int insertAt)') &&
       model.includes('if (i == insertAt) layout.put(z);') &&
       model.includes('if (i < zoneCount()) layout.put(zone(i));') &&
       compose.includes('project.addZoneAt("Zone " + (project.zoneCount() + 1), 30, position)') && compose.includes('editingId=created') && compose.includes('val id=project.zone(position)?.optString("id")') &&
       !compose.includes('project.moveZone(project.zoneCount() - 1, index + 1)'),
       'Add next zone must insert and reindex with one persisted transaction');
assert(compose.includes('if(project.assignMany(selections,zoneId,!checked)) refresh()') && compose.includes('if (project.name(it)) refresh()') && compose.includes('else name = project.name() // restore last committed value') && compose.includes('if(project.setPresets(next))') && compose.includes('Could not remove preset'),
       'Multi-trigger, name, and preset failure handling must not show false saved state');

// Palette engine: native data-only catalog, exact stop-data swatches, and
// separate effect, sequence-step and shared-sequence state. No invented
// gradient preview or unadvertised CORE palette commands are allowed.
for (const required of ['private fun PalettePicker(', 'NativePaletteEngine.lookup(project.raw(),',
  'Field(search, "Search palette name or ID"', 'PaletteSwatch(chosen, t)',
  'PalettePicker("Palette",paletteId,t)', 'putInt("paletteId",it)',
  'PalettePicker("Shared sequence palette"', 'project.updateSequenceOption("sharedPaletteId", id)',
  'PalettePicker("Step " + (i + 1) + " palette"',
  'settings.put("pal", id)', '.put("settings", JSONObject()',
  '!NativePaletteEngine.coreSupports(effectId, paletteId)']) {
  assert(compose.includes(required), 'Native palette authoring regressed: ' + required);
}
for (const required of ['"Default", "Random Cycle", "Color 1", "Colors 1&2", "Color Gradient"',
  'project.optJSONObject("palettePreviews")?.optJSONArray(id.toString())',
  'entry.length() != 4', 'stops.sortBy { it.first }',
  'return NativePalette(id, name, null)',
  'fun effectiveStepPalette(']) {
  assert(palettes.includes(required), 'Palette catalog or authentic-stop parser regressed: ' + required);
}
function previewCurrentGate(){return bleSource.includes('!BleFxPalettePolicy.isWireCandidate(effectId,paletteId)') && compose.includes('livePreview.stop()');}
// Firmware has a narrow existing palette BLE byte. This must not be
// generalized to WLED palette parity or new wire operations.
assert(palettes.includes('BleFxPalettePolicy.isWireCandidate(effectId, paletteId)') && compose.includes('!NativePaletteEngine.coreSupports(effectId, paletteId)') && compose.includes('NativePaletteEngine.corePreviewPalette(') && previewCurrentGate(),
       'Only the verified existing CORE palette subset may be previewed over BLE');
const preview=fs.readFileSync(
  'android/app/src/main/java/com/opentail/android/LiveBlePreview.kt','utf8');
assert(preview.includes('frame.paletteId, 5') &&
       preview.includes('NativePaletteEngine.coreSupports(frame.effectId, frame.paletteId)'),
       'BLE preview may not send unsupported palettes');
const corePaletteTest=fs.readFileSync(
  'android/app/src/test/java/com/opentail/android/NativePaletteCoreContractTest.kt','utf8');
assert(corePaletteTest.includes('corePreviewPalette(32768, palette)') &&
       corePaletteTest.includes('sourcePaletteEffectIdsMirrorPinnedCoreMetadata'),
       'Executable narrow-palette contract tests missing');

// Native sequence/palette import boundaries are strict unsigned bytes;
// source-backed RGB stop gradients must not round fractional samples.
for (const required of ['static boolean validPaletteId(Object value)',
  'number >= 0 && number <= 255 && number == Math.floor(number)',
  'private static void validateSequenceStep(JSONObject step)',
  'validateSequenceStep(steps.getJSONObject(i))',
  'settings.has("pal") && !validPaletteId(settings.opt("pal"))',
  'options.has("sharedPaletteId")',
  'for (int i = 0; i < steps.length(); i++)',
  'Invalid shared palette ID']) {
  assert(model.includes(required),'Palette import/save schema validation regressed: '+required);
}
assert(palettes.includes('number !in 0.0..255.0 || number % 1.0 != 0.0') &&
       palettes.includes('entry.length() != 4'),
       'Native palette swatches must preserve exact integer RGB stop values');
assert(compose.includes('val modelRevision = revision.intValue') &&
       compose.includes('val steps = remember(modelRevision) { project.sequence() }') &&
       compose.includes('val opts = remember(modelRevision) { project.sequenceOptions() }'),
       'Native sequence editor must redraw after palette and option persistence');
const paletteTest=fs.readFileSync(
  'android/app/src/test/java/com/opentail/android/NativePaletteValidationTest.java','utf8');
assert(paletteTest.includes('NativeProject.validPaletteId(Double.NaN)') &&
       paletteTest.includes('NativeProject.validPaletteId(255L)') &&
       paletteTest.includes('NativeProject.validPaletteId("42")'),
       'Executable palette byte regression cases missing');

// Palette options/step edits must survive disk failure without changing the
// live project. Reordering must not leave a positional editor on the wrong row.
assert(model.includes('synchronized boolean updateSequenceOption(String key, Object value)') &&
       model.includes('new JSONObject(previous.toString())') &&
       model.includes('data.put("sequenceOptions", previous == null ? new JSONObject() : previous)') &&
       model.includes('data.put("sequence", new JSONArray(steps.toString()))') &&
       model.includes('data.put("sequence", previous == null ? new JSONArray() : previous)'),
       'Sequence and shared palette persistence must roll back failed commits');
assert(compose.includes('project.updateSequenceOption(key, !enabled)') &&
       compose.includes('project.updateSequenceOption("sharedPaletteId", id)') &&
       compose.includes('val edited = sequenceList()') &&
       compose.includes('JSONObject(entry.toString())') &&
       compose.includes('if (updateSequence(updated)) editingStep = -1') &&
       compose.includes('Could not save sequence; previous design retained') &&
       compose.includes('editingStep = -1; editingPaletteStep = -1; moveSequence(i, -1)') &&
       compose.includes('editingStep = -1; editingPaletteStep = -1; removeSequence(i)'),
       'Local palette and sequence editing must be copy-on-write with save feedback');

// A completed BLE frame can arrive after a palette/effect/zone edit or after
// output switching. Prevent that older frame from being displayed as current.
const captureCallback=compose.split('ble.setFrameListener { output, sequence, rgb, error ->')[1]
  ?.split('ble.setScanResultListener')[0];
assert(captureCallback && captureCallback.includes('pendingFrameRevision != frameRevision') &&
       captureCallback.includes('pendingFrameOutput != output') &&
       captureCallback.includes('offlinePreview.value') &&
       captureCallback.includes('!ble.canCaptureFrame()') &&
       captureCallback.includes('return@runOnUiThread') &&
       captureCallback.includes('pendingFrameRevision = -1L'),
       'Stale or unauthorized asynchronous RGB frames must be rejected before display');
for (const expected of ['private fun invalidateCoreFrame()', 'pendingFrameRevision = -1L',
  'pendingFrameOutput = -1', 'frameRevision++', 'invalidateCoreFrame() // local design changes',
  'invalidateCoreFrame() // any parameter change', '!NativePaletteEngine.coreSupports(effectId, paletteId)',
  'invalidateCoreFrame() // the saved/readback output may have changed',
  'if(ble.captureFrame(captureOutput.intValue))', 'pendingFrameRevision=frameRevision',
  'pendingFrameOutput=captureOutput.intValue']) {
  assert(compose.includes(expected),'CORE frame invalidation or capture admission regressed: '+expected);
}
assert(compose.includes('private fun capturePreviewFrame()') && compose.includes('pendingFrameRevision=frameRevision') && compose.includes('private fun enterOfflinePreview()') && frameClient.includes('operationPending'),
       'Capture completion must serialize with new requests and STOP/output/offline changes');

assert(!compose.includes('ble.setPalette(') && !compose.includes('ble.sendPalette(') &&
       !palettes.includes('WLED firmware'), 'Unadvertised firmware palette writes or branding');
assert(compose.includes('Preview unavailable') && compose.includes('Unknown or dynamic palettes') && compose.includes('CORE frame snapshots'),
       'Palette UI must label unknown previews, restoration and BLE limitations');

// A started trigger read or save must invalidate any older "active" mapping
// until its originating CORE session supplies a fresh authenticated readback.
assert(bleSource.includes('private void invalidateTriggerReadback(BluetoothGatt owner)') &&
       bleSource.includes('if(!stopped && owner==gatt && triggerMapListener!=null)') &&
       bleSource.includes('triggerMapListener.accept(null);') &&
       bleSource.includes('if(gatt.readCharacteristic(triggerMapping)){') &&
       bleSource.includes('if(started){invalidateTriggerReadback(gatt);return true;}'),
       'Android cannot present a stale trigger snapshot as active during BLE edit/readback');

// Firmware #49 durable uint16 revisions are monotonic and reject wrap.
// A no-op upload may legitimately retain the base revision. A changed save
// can only advance exactly once, and the active output bytes must still match.
assert(bleSource.includes('if(cfgKnownRevision[index]>=65535)') &&
       bleSource.includes('cfgSaveBase=cfgRevision;') &&
       bleSource.includes('cfgRevision<cfgSaveBase || cfgRevision>cfgSaveBase+1') &&
       bleSource.includes('CORE active output already matched; no new revision committed') &&
       bleSource.includes('CORE active output differs; may be pending trigger release'),
       'CORE #49 output readback must distinguish no-op, staged and new active revisions');
assert(bleSource.includes('if(expectedRevision>=65535)') &&
       bleSource.includes('CORE trigger revision exhausted; cannot save or wrap') &&
       compose.includes('snapshot.revision in 1..65534') &&
       compose.includes('snapshot.persisted && snapshot.revision in 1..65534') &&
       compose.includes('active map read; last save rejected'),
       'CORE #49 trigger saves must require active profile and non-exhausted revision');

// CORE #38, exact pinned contract: f9614060b892f474e97501dc46a41e6242f60ec3
// Transport and UI are optional/authenticated. A v3 palette transfer never
// implies the selected FX supports it or that any optical LED pixel matched.
const paletteV3=fs.readFileSync(
  'android/app/src/main/java/com/opentail/android/NativePaletteV3.java','utf8');
const paletteTransport=fs.readFileSync(
  'android/app/src/main/java/com/opentail/android/NativePaletteBleTransport.java','utf8');
const paletteV3Test=fs.readFileSync(
  'android/app/src/test/java/com/opentail/android/NativePaletteV3Test.java','utf8');
for (const field of ['VERSION=3','SLOTS=128','STOPS_MAX=18','CHUNK_MAX=15','READ_MAX=6',
  'FIRST_ID=200','LAST_ID=73','return BleConfigCodec.crc16(stops)',
  'static byte[] begin(','static byte[] data(','static byte[] commit(',
  'static byte[] delete(','static byte[] query(','static byte[] read(',
  'static Reply reply(', 'validateStops(received)']) {
  assert(paletteV3.includes(field)||paletteTransport.includes(field),
    'Missing pinned CORE #38 palette BLE v3 contract: '+field);
}
for(const guard of ['if(r.status!=0){rejected(r.status);return;}',
  'if(kind==UPLOAD && (r.revision!=baseRevision+1 || !r.present))',
  'NativePaletteV3.crc(received)!=crc',
  'kind==UPLOAD&&!Arrays.equals(received,expected)',
  'if(abort){finish(false,false,null,"Palette transfer canceled for STOP");return;}',
  'abort=true;generation++;',
  'disconnect and reconnect before retry',
  'if(!busy||kind==CAPS||owner!=gatt)return;',
  'finish(true,false,null,"CORE palette deleted and tombstone verified")']) {
  assert(paletteTransport.includes(guard),'BLE v3 readback/cancel safety regressed: '+guard);
}
for(const guard of ['PALETTE_CMD_UUID','PALETTE_RSP_UUID','PALETTE_CAP_UUID',
  'palette.abortForStop();','palette.busy()','palette.negotiate()',
  'void cancelPaletteTransfer(){palette.abortForStop();}',
  'palette.upload(','palette.load(','palette.delete(',
  'palette.configure(connection,paletteCommand,paletteReply,paletteCapabilities)',
  'palette.written(connection,status);return;',
  'palette.capability(connection,value,status);return;',
  'palette.read(connection,value,status);return;']) {
  assert(bleSource.includes(guard),'Missing GATT v3 integration: '+guard);
}
for(const control of ['private fun CorePaletteTransferPanel(',
  'ble.negotiatePaletteV3()','ble.downloadPalette(slot!!)',
  'ble.uploadPalette(slot!!, paletteLoadedRevision.longValue, stopsText)',
  'ble.deletePalette(slot!!, paletteLoadedRevision.longValue)',
  'paletteLoadedSlot.intValue = -1 // no save success until CRC/readback confirms',
  'CorePaletteTransferPanel(t)',
  'ble.cancelPaletteTransfer() // no further NVS mutation after backgrounding',
  'Unsupported effect/palette combinations stay local']) {
  assert(compose.includes(control),'Missing native v3 palette UX/readback gate: '+control);
}
for(const exercise of ['capabilityNegotiationFailsClosed',
  'exactStopsAndCrc','uploadPacketsRespectTwentyByteAttBoundary',
  'authenticatedRepliesValidateVersionOpIndexAndStatus',
  'readChunkRequiresExactOffsetAndBounds']) {
  assert(paletteV3Test.includes(exercise),'BLE v3 executable codec coverage missing: '+exercise);
}

console.log('PASS: shipped Kotlin/Compose navigation, screens, universal geometry, BLE security, verified CORE frame path and project v4 contracts (static only).');
