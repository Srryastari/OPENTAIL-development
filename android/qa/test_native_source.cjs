'use strict';
/* Source-level architecture and data compatibility checks. Android SDK
 * compilation is a separate required CI job; this test does not claim that
 * phone interactions, BLE, or physical LED behavior have been exercised.
 */
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const crypto=require('node:crypto');
const firmwareSourceDir=process.env.OPENTAIL_FIRMWARE_SOURCE_DIR || 'core-firmware/main';
const effectsSourceDir=process.env.OPENTAIL_EFFECTS_SOURCE_DIR || 'core-firmware/main';
// ComposeMainActivity is the shipped UI; Java Views are historical reference only.
const compose=fs.readFileSync('android/app/src/main/java/com/opentail/android/ComposeMainActivity.kt','utf8');
const model=fs.readFileSync('android/app/src/main/java/com/opentail/android/NativeProject.java','utf8');
const palettes=fs.readFileSync('android/app/src/main/java/com/opentail/android/NativePaletteEngine.kt','utf8');
const presetRestore=fs.readFileSync('android/app/src/main/java/com/opentail/android/NativePresetRestore.java','utf8');
const approved=fs.readFileSync('android/app/src/main/java/com/opentail/android/ApprovedWledEffectsEditor.kt','utf8');
const bleSource=fs.readFileSync('android/app/src/main/java/com/opentail/android/BleDiscovery.java','utf8');
const panelProtocol=fs.readFileSync('android/app/src/main/java/com/opentail/android/PanelPreviewProtocol.java','utf8');
const panelV4=fs.readFileSync('android/app/src/main/java/com/opentail/android/PanelV4LayoutProtocol.java','utf8');
const panelDraft=fs.readFileSync('android/app/src/main/java/com/opentail/android/PanelV4DraftCodec.java','utf8');
const panelWatchdog=fs.readFileSync('android/app/src/main/java/com/opentail/android/TelemetrySequenceWatchdog.java','utf8');
const geometryTransactions=fs.readFileSync('android/app/src/main/java/com/opentail/android/GeometryTransactionManager.java','utf8');
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
  'frame!!.colorAt(sample)', 'CapsuleSlider(n, 0, 10000, t,', 'finished={ applyLiveZone() }',
  'project.previewZoneOffsets(id, n)', 'project.setPixelColor(id,activePixel,hex)',
  'project.assignMany(', 'project.setSequence(', 'project.importJson(', 'project.exportJson()',
  'ble.canControl()', 'supportedIds.value', 'previewCurrentEffect()', 'ble.savePanelV4(index, draftText)',
  'PanelV4DraftCodec.encode(index,JSONObject(draftText),saved)']) {
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
assert(bleSource.includes('frameCapture.cancel();clearPreviewCommand();palette.reset(); // GATT close may never produce a disconnect callback') &&
       bleSource.includes('infoReadPending') && bleSource.includes('frameCapture.busy()') &&
        bleSource.includes('nextPacket!=null') && bleSource.includes('cfgBusy') &&
        bleSource.includes('palette.busy()') && bleSource.includes('triggerMapBusy') &&
       bleSource.includes('queuedOutputOwner!=gatt || !canConfigure()') &&
       compose.includes('outputs.clear() // controller readback is no longer trusted; keep unsaved outputDrafts') &&
       compose.includes('private val outputDrafts = mutableStateMapOf<Int, String>()') &&
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
       compose.includes('autoEventPreviewEffect.intValue = effectId\n                        offlineReplay.intValue++\n                        enterOfflinePreview()') &&
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
       presetRestore.includes('draft.put("savedPresets",new JSONArray(project.presets().toString()))') &&
       presetBlock.includes('presetStatus = if (restored)') &&
       !presetBlock.includes('.put("studio", project.exportJson())'),
       'Presets may not recursively embed or erase their own library');
assert(homeSection.includes('remember(bleMessage.value) { ble.connectionPhase().replaceFirstChar { it.uppercase() } }') && settingsSection.includes('remember(bleMessage.value) { ble.connectionPhase().replaceFirstChar { it.uppercase() } }') && bleSource.includes('String connectionPhase()'),
       'Bluetooth status pill must reflect actual connection state, not transient messages');
assert(homeSection.includes('ble.authorizeOrPair()') && !settingsSection.includes('ble.authorizeOrPair()') && bleSource.includes('BleOwnerVault.load(activity,activeAddress)==null') && bleSource.includes('if (!connected)'),
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
const captureLoop = compose.split('private fun EffectsPreview(t: Tone, effect: Int, animateOffline: Boolean = false, settingsOverride: IntArray? = null) {')[1]
  ?.split('Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(20.dp))')[0];
const captureStart = bleSource.split('boolean captureFrame(int output) {')[1]
  ?.split('private final ArrayAdapter<String> entries;')[0];
assert(captureLoop && captureLoop.includes('withFrameNanos { now -> localClock.onFrame(now) }') &&
       !captureLoop.includes('delay(500)') &&
       !captureLoop.includes('while(foregroundActivity.value && !offlinePreview.value && !previewPaused.value)') &&
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
const previewSrc=compose.split('private fun EffectsPreview(t: Tone, effect: Int, animateOffline: Boolean = false, settingsOverride: IntArray? = null) {')[1]
  ?.split('private fun ShowModesPage(t: Tone)')[0];
const hardwareOutput=compose.split('private fun HardwareOutputEditor(t: Tone) {')[1];
assert(previewSrc &&
       previewSrc.includes('val actualFrame = coreFrame.value') &&
       previewSrc.includes('val hasLiveColors = remember(actualFrame)') &&
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
       hardwareOutput.includes('if(outputToolsOpen) Caption("Draft edits are local;'),
       'Configured-color fallback and zero-NVS pixel +/- controls must be visible without editing locked preview geometry');
// Owner's full Test 1-80 regression gates (Android-only source contracts).
assert(compose.includes('effectSettings.put(key,prefs.getInt(key,fallback))') &&
       compose.includes('NativePresetRestore.load(project,prefs,preset)') &&
       compose.includes('if (restored) {') &&
       compose.includes('refreshLoadedProject()') &&
       compose.includes('rememberSaveable(projectLoadRevision.intValue)') &&
       compose.includes('offlinePreview.value -> "Offline design preview"') &&
       compose.includes('val fill = if (selected) (if(t.bg == Color.Black)') &&
       compose.includes('firmwareVersion.value ?: "Unavailable"') &&
       compose.includes('firmwareVersion.value ?: "Version unavailable"') &&
       compose.includes('mutableStateOf<String?>(null) // collapse cards on Layout page entry') &&
       compose.includes('effectOptions = NativeEffectCatalog.generalEffects()') &&
       approved.includes('it.name.contains(effectSearch.trim(),true)') &&
       compose.includes('effectOptions = NativeEffectCatalog.generalEffects()'),
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
       autoEvents.includes('NativeEffectCatalog.autoEventChoices(key)') &&
       !autoEvents.includes('.take(15)') &&
       autoEvents.includes('previewEffect(selected.id)') &&
       autoEvents.includes('EffectsPreview(t, eventPreviewId, animateOffline=true, settingsOverride=') &&
       compose.includes('autoEventPreviewEffect.intValue = effectId') &&
       !compose.includes('selectedEffect.intValue = effectId') &&
       !compose.includes('enterOfflinePreview()\\n                        page = 1') &&
       compose.includes('effectOptions = NativeEffectCatalog.generalEffects()'),
       'Test 86: full Auto Events picker and exclusive transitions across shows, triggers and preview');
assert(bleSource.includes('setInfoListener(') &&
       bleSource.includes('infoReadQueued=true;pumpQueue()') &&
       bleSource.includes('if(infoReadQueued && coreInfo!=null && gatt!=null)') &&
       bleSource.includes('infoReadPending'),
       'Actual firmware INFO metadata must remain safely serialized after owner authorization');
assert(gradle.includes("versionName '0.2.0-alpha7'"));
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
  'CapsuleSlider(n, 0, 10000, t,', 'finished={ applyLiveZone() }',
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
  'scopeKey = "triggers:"'
]) assert(triggers.includes(required),'Assignment-only Triggers regression: '+required);
assert(triggers.includes('IconAction("Edit " + name, "edit", t)') &&
       triggers.includes('if (profile.optString("kind") == "trigger-custom")') &&
       triggers.includes('IconAction("Delete " + name, "delete", t)') &&
       triggers.includes('title = { Text("Delete custom trigger?") }') &&
       triggers.includes('project.deleteTrigger(target)') &&
       triggers.includes('scopeKey = "triggers:" + selections.sorted().joinToString("|")') &&
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
for(const required of ['EffectsPreview(t, if(chosen >= 0) chosen else -1, animateOffline=true)', 'listOf("Effects", "Presets", "Sequences")',
  'ApprovedWledEffectsEditor(', 'effectOptions = NativeEffectCatalog.generalEffects()',
  'onSelected = { id ->', 'onColor = { id,slot,rgb ->',
  'onPalette = { id,selected ->',
  'previewCurrentEffect(id)']) {
  assert(modes.includes(required),'Approved Effects binding regression: '+required);
}
for(const required of ['approved-prototype.fragment.html SHA256 37DB62A26E1622A7058B503EEB3A10B731C327269FB755E6289802AACE273D9D',
  'selectedId: Int?', 'if(picked)null else item.id',
  'Dialog(onDismissRequest=onClose,',
  'dismissOnBackPress=true', 'ColorWheel(', 'role=r.slot',
  '.fillMaxWidth().height(4.dp)', 'onParam(currentId,key,value)',
  'onColor(currentId,slot,rgb)', 'onPalette(currentId,id)']) {
  assert(approved.includes(required),'Owner WLED+ design/interaction regression: '+required);
}
// Single approved WLED+ editor across three independent model stores.
assert(compose.split('ApprovedWledEffectsEditor(').length - 1 === 3 &&
       modes.includes('effectOptions = NativeEffectCatalog.generalEffects()') &&
       triggers.includes('scopeKey = "triggers:"') &&
       events.includes('scopeKey="auto-event:"+key') &&
       triggers.includes('project.editTriggerEditorField(selections,"paletteId",id)') &&
       events.includes('applyAutoEvent(key,"paletteId",id)') &&
       model.includes('synchronized boolean editTriggerEditorField(') &&
       model.includes('private static boolean editorFieldValid(') &&
       !approved.includes('All Segments') &&
       !approved.includes('effects-live-toggle') &&
       !approved.includes('Text(if(liveEnabled)'),
       'Approved Effects interface must have three independently persisted editor contexts and no unapproved control header');
assert(palettes.includes('fun usesEditableColorSlots(id: Int): Boolean = id in 2..5') &&
       palettes.includes('fun isImmutableFixedGradient(id: Int): Boolean') &&
       palettes.includes('if (fixedPalette)') &&
       approved.includes('val allowColorSlots = def != null') &&
       approved.includes('takeIf { allowColorSlots }'),
       'Immutable WLED source gradient must never inherit Main/Set Color');

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
assert(hardware.includes('PanelV4DraftCodec.MAX_LEDS') &&
       !hardware.includes('623') &&
       hardware.includes('it.put("pixels", remaining)') &&
       hardware.includes('structureVersion++') &&
       hardware.includes('PanelV4DraftCodec.encode(index,draft,authoritative)') &&
       hardware.includes('ble.savePanelV4(index, draftText)'),
  'CORE output editor must validate universal drafts and synchronize zone totals');
// A rejected GATT save is not an in-flight hardware commit. Readback, not
// request admission, remains the only success evidence.
assert(bleSource.includes('boolean savePanelV4(int lane,String json)') &&
       bleSource.includes('PanelPreviewProtocol.LaneSnapshot base=lastConfirmedPanelSnapshot(lane)') &&
       bleSource.includes('panelSaveBusy=true;panelSaveLane=lane;') &&
       bleSource.includes('panelSavePhase=4') &&
       bleSource.includes('Arrays.equals(panelSaveWire,snapshot.wire)') &&
       hardware.includes('if (ble.savePanelV4(index, draftText))') &&
       hardware.includes('Controller busy; no Save started'),
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
assert(codec.includes('static final int MAX_LEDS_PER_OUTPUT = 2000') &&
       codec.includes('pixels>MAX_LEDS_PER_OUTPUT') &&
       codec.includes('length>MAX_LEDS_PER_OUTPUT') &&
       codec.includes('if(sum!=pixelCount)') &&
       !codec.includes('index==0?16:17') &&
       !hardware.includes('623'),
  'BLE geometry must enforce the approved 2000-pixel budget without inventing physical pins');
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
       triggers.includes('field in setOf("brightness","speed","intensity")') &&
       triggers.includes('project.editTriggerParameter(selections,field,fieldValue)') &&
       triggers.includes('project.editTriggerEditorField(selections,field,fieldValue)') &&
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

const offlineBinding=fs.readFileSync('android/app/src/main/java/com/opentail/android/OfflineCoreRenderer.java','utf8');
assert(compose.includes('var backStack by remember') &&
       compose.includes('backStack = backStack + page') &&
       compose.includes('backStack.lastOrNull() ?: 0') &&
       compose.includes('navigate(i)') &&
       compose.includes('HomePage(t, { navigate(it) })') &&
       compose.includes('SettingsPage(t, { navigate(it) }'),
       'Android Back must pop the shared six-screen history, not force Home or exit');
assert(palettes.includes('foreground: Int? = null') &&
       palettes.includes('if (fg == null) return null') &&
       palettes.includes('if (bg == null) return null') &&
       !palettes.includes('if (fg < 0) return null'),
       'Opaque ARGB colors can be signed-negative and must remain valid');
assert(presetRestore.includes('if(!editor.commit())return false;') &&
       presetRestore.indexOf('if(!editor.commit())return false;') < presetRestore.indexOf('project.importJson(draft.toString())') &&
       presetRestore.includes('rollback.commit()') &&
       fs.readFileSync('android/app/src/test/java/com/opentail/android/NativePresetRestoreTest.java','utf8')
         .includes('effectCommitFailureMustNotImportNewDesignOrChangeSelection'),
       'Preset project import must never precede an effect preference commit');
assert(compose.includes('val localSketch = animateOffline') &&
       compose.includes('renderer.render(') &&
       compose.includes('Local effect preview') &&
       offlineBinding.includes('System.loadLibrary("opentail_preview")'),
       'Local-only preview must animate known effects without fabricating CORE frames');

// Codex PR #76 P2: ensure old wheel callbacks cannot overwrite new
// brightness/role, and preset load cannot resurrect stale scoped values.
assert(approved.includes('val currentOnPick by rememberUpdatedState(onPick)') &&
       approved.includes('currentOnPick(hue,sat)') &&
       approved.includes('listOf(Color.Red,Color.Yellow,Color.Green,Color.Cyan,') &&
       approved.includes('Color.Blue,Color.Magenta,Color.Red)'),
       'Native color wheel must track current brightness and HSV direction');
assert(presetRestore.includes('final List<String> staleScoped=new ArrayList<>();') &&
       presetRestore.includes('for(String key:staleScoped) editor.remove(key);') &&
       presetRestore.includes('touched.addAll(staleScoped)') &&
       fs.readFileSync('android/app/src/test/java/com/opentail/android/NativePresetRestoreTest.java','utf8')
          .includes('legacyPresetClearsOnlyLoadedEffectStaleKeys'),
       'Loading a legacy preset must atomically remove stale per-effect editor values');

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
       bleSource.includes('Arrays.equals(panelSaveWire,snapshot.wire)'),
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
  ?.split('boolean savePanelV4(int lane,String json)')[0];
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
       bleSource.includes('CORE snapshot ATT timed out; reconnect required') &&
       bleSource.includes('clearQueuedOutputRead();') &&
       bleSource.includes('if(panelSaveBusy||pixelProbeWritePending||writePending||readReplyPending||cfgBusy||fxCapsBusy||infoReadPending||triggerMapBusy||'),
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
 '!wholeNumber(z.opt("length"), 0, 10000)',
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
const liveStrip=compose.split('private fun EffectsPreview(t: Tone, effect: Int, animateOffline: Boolean = false, settingsOverride: IntArray? = null) {')[1]
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
       compose.includes('project.addZoneAt("Zone " + (project.zoneCount() + 1), 0, position)') && compose.includes('editingId=created') && compose.includes('val id=project.zone(position)?.optString("id")') &&
       !compose.includes('project.moveZone(project.zoneCount() - 1, index + 1)'),
       'Add next zone must insert and reindex with one persisted transaction');
assert(compose.includes('if(project.assignMany(selections,zoneId,!checked)) refresh()') && compose.includes('if (project.name(it)) refresh()') && compose.includes('else name = project.name() // restore last committed value') && compose.includes('if(project.setPresets(next))') && compose.includes('Could not remove preset'),
       'Multi-trigger, name, and preset failure handling must not show false saved state');

// Palette engine: native data-only catalog, exact stop-data swatches, and
// separate effect, sequence-step and shared-sequence state. No invented
// gradient preview or unadvertised CORE palette commands are allowed.
for (const required of ['private fun PalettePicker(', 'NativePaletteEngine.lookup(project.raw(),',
  'Field(search, "Search palette name or ID"', 'PaletteSwatch(chosen, t)',
  'ApprovedWledEffectsEditor(', 'putInt("paletteId",selected)',
  'PalettePicker("Shared sequence palette"', 'project.updateSequenceOption("sharedPaletteId", id)',
  'PalettePicker("Step " + (i + 1) + " palette"',
  'settings.put("pal", id)', '.put("settings", JSONObject()',
  '!NativePaletteEngine.coreSupports(effectId, paletteId)']) {
  assert(compose.includes(required), 'Native palette authoring regressed: ' + required);
}
for (const required of ['"Default", "Random Cycle", "Color 1", "Colors 1&2", "Color Gradient"',
  'project.optJSONObject("palettePreviews")?.optJSONArray(id.toString())',
  'entry.length() != 4', 'if (parts[0] <= previous',
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
assert(preview.includes('frame.paletteId') &&
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

assert(captureLoop.includes('val localClock = remember(effect,offlineReplay.intValue,outputId)') &&
       captureLoop.includes('withFrameNanos { now -> localClock.onFrame(now) }') &&
       !captureLoop.includes('delay(33L)') && !captureLoop.includes('localTick++'),
       'Local animation must use display timestamps and preserve phase across edits');
const drawPreview=previewSrc.split('Canvas(Modifier.fillMaxWidth().height(24.dp))')[1].split('Row(Modifier.fillMaxWidth()')[0];
assert(!drawPreview.includes('prefs.getInt(') && !drawPreview.includes('renderer.render(') &&
       drawPreview.includes('val drawnPixels=localColors.value') &&
       captureLoop.includes('withContext(Dispatchers.Default)') &&
       captureLoop.includes('renderer.render(sample.elapsedMs,sample.frameMs'),
       'Draw must only consume cached output-specific frames; JNI renders once per vsync');
for(const call of ['this::authorizeStored','this::pairWithBoot','this::readTriggerMap',
                  '()->saveTriggerMap(expectedRevision,requested)','()->saveOutput(index,json)',
                  '()->fxPreset(action,slot)','()->authorize(tokenHex)'])
    assert(bleSource.includes('deferBehindFrame('+call+')'),'Capture must yield for '+call);
assert(frameClient.includes('clearTimers();') && frameClient.includes('watchOperation();') &&
       frameClient.includes('byte[] complete=pixels;'), 'Bound snapshot timers and hand off owned RGB');

const tertiaryCommand=fs.readFileSync('android/app/src/main/java/com/opentail/android/TertiaryFxCommand.java','utf8');
const bleCommand=fs.readFileSync(path.join(firmwareSourceDir,'ble_fx_command.c'),'utf8');
const bleHeader=fs.readFileSync(path.join(firmwareSourceDir,'ble_fx_command.h'),'utf8');
const bleStatus=fs.readFileSync(path.join(firmwareSourceDir,'ble_status.c'),'utf8');
assert(tertiaryCommand.includes('fx-tertiary13-v1') &&
       tertiaryCommand.includes('supportedForFirmware') &&
       bleCommand.includes('return ot_wled_fx_tertiary_supported(id);') &&
       bleCommand.includes('candidate.effect_id==38u && candidate.palette_id!=4u') &&
       bleHeader.includes('OT_BLE_FX_TERTIARY13_CAPABILITY') &&
       bleStatus.includes('OT_BLE_FX_TERTIARY13_CAPABILITY'),
       'Firmware #127 13-mode capability, Aurora palette gate and Android fail-closed tier must agree');

const nativePreview=fs.readFileSync('android/app/src/main/cpp/preview.c','utf8');
const nativeManifest=JSON.parse(fs.readFileSync('android/app/src/main/cpp/core/SOURCE.json','utf8'));
/* Issue103: actual SHA256 and byte-identity checks, not a stale source
 * comment claiming parity. Only reviewed pure renderer sources are repinned. */
assert(nativeManifest.source_commit==='601155a196a692792e60fd399505fdc676dc57d9' &&
       nativeManifest.seamless_loop_source_commit===nativeManifest.source_commit &&
       nativeManifest.color_slot_source_commit===nativeManifest.source_commit &&
       nativeManifest.core_identity_source_commit==='97f4e2fd29df5aa9249292cee2649404fc98ed48' &&
       JSON.stringify(nativeManifest.core_identity_files)===JSON.stringify(['effects_1d.c','effects_1d.h']) &&
       nativeManifest.integrated_firmware96_head==='df27209b9876630b2b10f3a189f36817a2764009' &&
       nativeManifest.integrated_android106_head==='db7bcdff187197c0493b5c38991870224f2c244a' &&
       nativeManifest.tertiary_source_proven_modes===13 &&
       nativeManifest.tertiary_transport_admitted_modes===13 &&
       nativeManifest.tertiary_legacy_transport_modes===11 &&
       nativeManifest.tertiary_transport_capability==='fx-tertiary13-v1' &&
       nativeManifest.firmware127_ble_contract_head==='97f4e2fd29df5aa9249292cee2649404fc98ed48',
       'JNI needs reconciled reviewed firmware, source and explicitly limited transport IDs');
for(const [file,expectedHash] of Object.entries(nativeManifest.files)){
    const bytes=fs.readFileSync(path.join('android/app/src/main/cpp/core',file));
    assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),expectedHash,
                 'JNI source manifest mismatch: '+file);
}
for(const file of ['effects_1d.c','effects_1d.h']){
    assert.deepEqual(
        fs.readFileSync(path.join('android/app/src/main/cpp/core',file)),
        fs.readFileSync(path.join(firmwareSourceDir,file)),
        'JNI/Firmware #127 stable-state source divergence: '+file);
}
for(const file of ['wled_fx_compat.c','wled_fx_compat.h','ot_auto_effects.c','ot_auto_effects.h']){
    assert.deepEqual(
        fs.readFileSync(path.join('android/app/src/main/cpp/core',file)),
        fs.readFileSync(path.join(effectsSourceDir,file)),
        'JNI/Effects #110 renderer divergence: '+file);
}
assert(fs.readFileSync('android/app/src/main/cpp/core/wled_fx_compat.c','utf8')
         .includes('bool ot_wled_fx_tertiary_supported(uint16_t id)') &&
       nativeManifest.source_commit==='601155a196a692792e60fd399505fdc676dc57d9' &&
       nativePreview.includes('ot_effects_1d_render(&cfg,&p->state') &&
       nativePreview.includes('ot_wled_palette_global_tick('),
       'All effects must use the pinned authoritative registry and palette producer');
assert(compose.includes('OfflinePreviewSettings.read(effect)') &&
       compose.includes('registerOnSharedPreferenceChangeListener(observer)') &&
       compose.includes('unregisterOnSharedPreferenceChangeListener(observer)') &&
       compose.includes('params[2],params[3],params[4],palette') &&
       compose.includes('val currentSettings by rememberUpdatedState(settings)') &&
       compose.includes('OfflinePreviewPalettePolicy.rendererId(effect,settings[5])') &&
       fs.readFileSync('android/app/src/main/java/com/opentail/android/OfflinePreviewPalettePolicy.kt','utf8')
           .includes('NativeEffectParameters.forId(effectId)?.palette == true'),
       'Scoped parameter edits must update native preview and reject stale palettes for non-palette effects');
const onStopBlock=(compose.split('override fun onStop() {')[1]||'').split('override fun onDestroy()')[0];
assert(manifest.includes('android.permission.RECORD_AUDIO') &&
       compose.includes('val needsMic = localSketch && effect==185 && foregroundActivity.value && !previewPaused.value') &&
       onStopBlock.includes('localMicrophone.stop()') &&
       onStopBlock.includes('super.onStop()') &&
       onStopBlock.indexOf('localMicrophone.stop()')<onStopBlock.indexOf('super.onStop()') &&
       !fs.readFileSync('android/app/src/main/java/com/opentail/android/MicAudioPeak.java','utf8').includes('.join('),
       'Owner-approved microphone must remain local, foreground-only and off the UI thread');

 // PR90 bench regression: the existing physical controls and authenticated
 // editor gestures must actually reach CORE, including after Preview Offline.
 const controllerEditor = compose.split('private fun ShowModesPage(')[1].split('private fun effectLabel(')[0];
 assert(!controllerEditor.includes('if(liveArmed.value) previewCurrentEffect(id)') &&
        !controllerEditor.includes('if(liveArmed.value && key in') &&
        !controllerEditor.includes('if(liveArmed.value && slot < 2)') &&
        controllerEditor.includes('if(key in setOf("speed","intensity","brightness"))') &&
        controllerEditor.includes('previewCurrentEffect(id)') &&
        compose.includes('tertiary = if (TertiaryFxCommand.supported(effectId)') &&
        preview.includes('ble.showContinuousTertiaryFx(') &&
        preview.includes('ble.canTertiaryFx()'),
        'Authenticated effect edits must not require an already-armed preview');
 assert(compose.includes('liveChainMask.intValue = mask\n                                offlinePreview.value = false') &&
        compose.includes('liveZoneMask.intValue = mask\n                            offlinePreview.value = false') &&
        compose.includes('previewCurrentEffect()'),
        'Explicit existing physical bench controls must leave Preview Offline');

 // Owner continuous-playback and release-control contract.
 assert(preview.includes('ble.showContinuousFx(') &&
        preview.includes('fun detach()') && !preview.includes('frame.paletteId, 5'),
        'Show Mode must not use TTL or a keepalive loop');
 assert(compose.includes('if(text=="Home") IconAction("Stop CORE show","stop",t)'), 'STOP must be available on Home');
 const lifecycle=compose.split('override fun onStop() {')[1].split('super.onDestroy()')[0];
 assert(!lifecycle.includes('livePreview.stop()') && lifecycle.includes('livePreview.detach()'),
        'Backgrounding must not stop CORE playback');
 assert(compose.includes('val localSketch = animateOffline && renderer.available()') &&
        !compose.includes('while(foregroundActivity.value && !offlinePreview.value && !previewPaused.value)'),
        'Connected preview must render locally without automatic snapshots');
 const releaseSettings=compose.split('private fun SettingsPage(')[1].split('private fun HardwareOutputEditor(')[0];
 assert(!releaseSettings.includes('ble.authorizeOrPair()') &&
        compose.includes('.then(if(label=="Firmware") Modifier.clickable(onClick=action) else Modifier)'),
        'Authorization belongs on Home; Firmware row is tappable');
 assert(!compose.includes('Title("CORE Output Manager"') &&
        compose.includes('Text("Output",color=t.fg') &&
        releaseSettings.includes('if(mappingExpanded) CoreTriggerMappingPage(t)'),
        'Output editing inline; developer input map only in Firmware');


// Owner P0: exact Issue #126 Panel v2 telemetry contract, transaction guards,
// and conditional warning in the already-approved Settings -> System card.
assert(panelProtocol.includes('STATUS_BYTES = 61') &&
       panelProtocol.includes('MAP_BYTES = 667') &&
       panelProtocol.includes('ERR_COMMITTING_LOCKED = 5') &&
       panelProtocol.includes('CAP_STATUS_V2') &&
       panelProtocol.includes('lastRejectedSequence') &&
       panelProtocol.includes('committingRejectCount') &&
       panelProtocol.includes('candidateCrc32') &&
       panelProtocol.includes('SnapshotHeader') &&
       panelProtocol.includes('crc16(byte[] bytes)'),
       'Android Panel codec must match the documented 61-byte status-v2 and 667-byte map contract');
assert(panelWatchdog.includes('missingCount>last.missingCount') &&
       panelWatchdog.includes('outOfOrderCount>last.outOfOrderCount') &&
       panelWatchdog.includes('duplicateCount>last.duplicateCount') &&
       panelWatchdog.includes('conflictCount>last.conflictCount') &&
       panelWatchdog.includes('hardSyncFault') &&
       panelWatchdog.includes('reconcile(') &&
       panelWatchdog.includes('hardFaultSession') &&
       panelWatchdog.includes('markMalformedTelemetry') &&
       panelWatchdog.includes('markReconciliationFailed') &&
       panelWatchdog.includes('requiresCancelFor') &&
       panelWatchdog.includes('awaitingFreshSession') &&
       panelWatchdog.includes('current.lastRejectedSequence!=last.lastRejectedSequence') &&
       panelWatchdog.includes('hardFault=true;hardFaultLane=last.lane;hardFaultSession=last.sessionId'),
       'Telemetry watchdog must detect sequence anomalies and require authoritative reconciliation');
for (const state of ['READY','BEGINNING','ACTIVE','UPDATE_PENDING','COMMIT_REQUESTED','COMMITTING','RECONCILING','ERROR']) {
  assert(geometryTransactions.includes(state),'Geometry transaction state missing: '+state);
}
assert(geometryTransactions.includes('ERR_COMMITTING_LOCKED') &&
       geometryTransactions.includes('lastConfirmed') &&
       geometryTransactions.includes('candidateCrc32') &&
       geometryTransactions.includes('connectionLost()') &&
       geometryTransactions.includes('markSynchronizationUncertain') &&
       geometryTransactions.includes('authoritative geometry snapshot'),
       'Geometry manager must preserve last-confirmed geometry through COMMITTING and uncertain disconnects');
assert(bleSource.includes('55a56022-4915-7a92-cf44-b8b7102eaf30') &&
       bleSource.includes('55a56022-4915-7a92-cf44-b8b7102eaf31') &&
       bleSource.includes('55a56022-4915-7a92-cf44-b8b7102eaf32') &&
       bleSource.includes('55a56022-4915-7a92-cf44-b8b7102eaf35') &&
       bleSource.includes('55a56022-4915-7a92-cf44-b8b7102eaf37') &&
       bleSource.includes('PanelPreviewProtocol.Status.parse(value)') &&
       bleSource.includes('panelSnapshotHeader.verify(panelSnapshotWire,panelSnapshotLane)') &&
       bleSource.includes('panelScheduleCommitPoll(owner)') &&
       bleSource.includes('panelWatchdog.reconcile(current,true)') &&
       bleSource.includes('panelWatchdog.requiresCancelFor(current)') &&
       bleSource.includes('panelWatchdog.awaitingFreshSession(current)') &&
       bleSource.includes('manager.observeStatus(current)') &&
       bleSource.includes('manager.markSynchronizationUncertain(decision.message)') &&
       bleSource.includes('panelConfirmedSnapshots[panelSnapshotLane]=snapshot') &&
       bleSource.includes('GATT stack performs the characteristic long-read') &&
       bleSource.includes('panelWatchdog.markMalformedTelemetry') &&
       bleSource.includes('panelScheduleTransactionPoll(owner,current.lane)') &&
       bleSource.includes('panelTransactionTimeoutLatched[current.lane]') &&
       bleSource.includes('manager.state()==GeometryTransactionManager.State.RECONCILING') &&
       bleSource.includes('panelSnapshotQueuedLane=current.lane') &&
       bleSource.includes('boolean beginPanelPreview(') &&
       bleSource.includes('boolean updatePanelPreview(') &&
       bleSource.includes('boolean commitPanelPreview(') &&
       bleSource.includes('boolean heartbeatPanelPreview(') &&
       bleSource.includes('boolean cancelPanelPreview(') &&
       bleSource.includes('GeometryTransactionManager.validateMappingAgainstSnapshots(mapping,snapshots,statuses)') &&
       bleSource.includes('manager.requestBegin()') &&
       bleSource.includes('manager.queueUpdate(candidateWire,descriptors,negotiatedMtu)') &&
       bleSource.includes('manager.requestCommit(status)') &&
       bleSource.includes('panelStatusFresh[i]') &&
       bleSource.includes('BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE') &&
       bleSource.includes('panelStatusExpectedLane=panelSnapshotLane') &&
       bleSource.includes('private void panelRetryStatusRead(BluetoothGatt owner)') &&
       bleSource.includes('panelStatusReadRetries>=3') &&
       bleSource.includes('},250L)') &&
       bleSource.includes('private void startPanelControl()') &&
       bleSource.includes('private void startPanelUpdate()') &&
       bleSource.includes('panelControlWritten(connection,status)') &&
       bleSource.includes('panelUpdateWritten(connection,status)') &&
       bleSource.includes('manager.commitWriteCompleted()') &&
       bleSource.includes('Panel UPDATE write completion timed out') &&
       bleSource.includes('panelSnapshotAbortForStop=true') &&
       bleSource.includes('Panel snapshot aborted to prioritize STOP'),
       'BLE adapter must read negotiated Panel v2 telemetry, verified committed geometry and reconcile without blind retry');
const systemSection=compose.split('private fun SettingsPage(')[1].split('private fun SettingsDiagnostics(')[0];
assert(systemSection.includes('panelSyncWarning.value?.let') &&
       systemSection.includes('Synchronization warning') &&
       !systemSection.includes('Panel Diagnostics') &&
       compose.includes('setPanelStatusListener('),
       'Sequence warnings must appear conditionally in existing Settings -> System without a new screen');
for (const file of [
  'android/app/src/test/java/com/opentail/android/PanelPreviewProtocolTest.java',
  'android/app/src/test/java/com/opentail/android/TelemetrySequenceWatchdogTest.java',
  'android/app/src/test/java/com/opentail/android/GeometryTransactionManagerTest.java'
]) assert(fs.existsSync(file),'P0 Android regression suite missing: '+file);


// Disconnect/reconnect regression: old GATT session and its authoritative
// Panel/Zone aliases must never leak into the next verified controller readback.
const disconnectBlock=bleSource.split('private void panelDisconnected() {')[1]?.split('boolean canMapTriggers()')[0]||'';
const disconnectedStatusBlock=compose.split('if (!ble.isConnected()) {')[1]?.split('if (!ble.canControl())')[0]||'';
assert(disconnectBlock.includes('panelGeometryManagers[lane]=null;') &&
       disconnectBlock.includes('panelConfirmedSnapshots[lane]=null;') &&
       bleSource.includes('new GeometryTransactionManager(') &&
       bleSource.includes('panelSnapshotHeader.verify(panelSnapshotWire,panelSnapshotLane)') &&
       disconnectedStatusBlock.includes('clearPanelGeometryPreview(') &&
       disconnectedStatusBlock.includes('panelGeometryLoadInProgress = false') &&
       disconnectedStatusBlock.includes('Refresh CORE') &&
       compose.includes('private fun queuePanelGeometryDraftUpdate()') &&
       compose.includes('panelGeometryQueuedTarget=target') &&
       compose.includes('Wait for all draft updates to be acknowledged before Commit'),
       'Live Layout must invalidate disconnected managers and shadow drafts, reload verified CORE map, and serialize rapid edits');


// Physical bench regression: Reload must not stack a local/project editor
// beneath a second CORE-output editor, and output selection must route PLAY.
const layoutEditor = compose.split('private fun LayoutPage(t: Tone) {')[1]
  ?.split('private fun refreshPanelGeometry()')[0];
const unifiedEditor = compose.split('private fun HardwareOutputEditor(t: Tone) {')[1];
assert(layoutEditor?.includes('val draftZoneCount = layoutDraftZoneCounts[captureOutput.intValue]') &&
       layoutEditor.includes('HardwareOutputEditor(t)') &&
       layoutEditor.includes('linked zones are edited inside the cards above') &&
       unifiedEditor?.includes('liveChainMask.intValue=1') &&
       unifiedEditor.includes('liveChainMask.intValue=2') &&
       unifiedEditor.includes('Pill("Test Solid Red · O"') &&
       unifiedEditor.includes('testSavedOutputSolidRed(index)') &&
       compose.includes('private fun testSavedOutputSolidRed(index: Int)') &&
       compose.includes('foreground = 0xffff0000.toInt(), background = 0xff000000.toInt()') &&
       compose.includes('effectId = 0, chainMask = 1 shl index, zoneMask = 255') &&
       compose.includes('brightness = 24, speed = 128, intensity = 128, paletteId = 0') &&
       unifiedEditor.includes('Solid Red needs a saved non-Off zone. The hardware check below does not.') &&
       unifiedEditor.includes('ble.savePanelV4(index, draftText)') &&
       unifiedEditor.includes('"Saving…" else "Save"') &&
       !unifiedEditor.includes('if (confirm) AlertDialog'),
       'Layout must display one editor, route playback to selected output and gate Saved on CORE readback');

// An explicit physical red test cannot inherit an unrelated Show Mode
// effect, palette, color, brightness, or unsaved output draft.
const savedRedTest = compose.split('private fun testSavedOutputSolidRed(index: Int) {')[1]
  ?.split('private val importer =')[0];
assert(savedRedTest &&
       savedRedTest.includes('if (0 !in supportedIds.value || !ble.canContinuousFx())') &&
       savedRedTest.includes('livePreview.update(LiveBlePreview.Frame(') &&
       savedRedTest.includes('ble.lastConfirmedPanelSnapshot(index) != null') &&
       !savedRedTest.includes('previewCurrentEffect(') &&
       !savedRedTest.includes('prefs.getInt(') &&
       !savedRedTest.includes('ble.saveOutput('),
       'Saved Output test must emit independent, gated volatile Solid Red and never write NVS');

// Readback timing must be collected from real monotonic ATT progression,
// not predicted from output pixel count or substituted for CRC/revision proof.
assert(bleSource.includes('private String cfgTimingDetail()') &&
       bleSource.includes('cfgRequestedMs=android.os.SystemClock.elapsedRealtime();') &&
       bleSource.includes('cfgTransferMs=android.os.SystemClock.elapsedRealtime();') &&
       bleSource.includes('cfgReadbackMs=android.os.SystemClock.elapsedRealtime();') &&
       bleSource.includes('cfgReadFragments++;') &&
       bleSource.includes(')+cfgTimingDetail(),output,cfgIndex') &&
       bleSource.includes('Arrays.equals(cfgBytes,cfgExpected)'),
       'Measure real readback fragments and latency without bypassing verified controller readback');

// Role Off cannot authorize a normal FX show, but Firmware #127's
// authenticated Pixel Probe can still test physical LED 0 without NVS,
// without constructing a fake zone, and without bypassing STOP priority.
const probeBlock = bleSource.split('boolean probeFirstPixel(int lane) {')[1]
  ?.split('boolean canReadPanelPreview()')[0];
assert(bleSource.includes('PIXEL_PROBE_UUID = UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf33")') &&
       bleSource.includes('boolean canProbeFirstPixel()') &&
       bleSource.includes('(panelCapabilities.flags&1)!=0') &&
       probeBlock?.includes('new byte[]{(byte)0xaa,(byte)(lane+1),0,0,1}') &&
       probeBlock.includes('BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT') &&
       probeBlock.includes('pixelProbeWritePending=true;') &&
       probeBlock.includes('recoverStalledGatt(owner,"Pixel probe ATT write timeout")') &&
       bleSource.includes('if (panelSaveBusy || pixelProbeWritePending || writePending || readReplyPending') &&
       bleSource.includes('if(panelSaveBusy||pixelProbeWritePending||writePending||readReplyPending||cfgBusy') &&
       bleSource.includes('if (PIXEL_PROBE_UUID.equals(characteristic.getUuid()))') &&
       bleSource.includes('panelDisconnected(); // also clear stale Panel/Probe capability and in-flight diagnostic lease') &&
       compose.includes('ble.setPixelProbeListener { result ->') &&
       unifiedEditor.includes('Pill("Check first LED · O"') &&
       unifiedEditor.includes('enabled=ble.canProbeFirstPixel()') &&
       unifiedEditor.includes('if(ble.probeFirstPixel(index))') &&
       !probeBlock.includes('saveOutput(') &&
       !probeBlock.includes('cfgSend('),
       'Bench Pixel Probe must use exact authorized five-byte v4 diagnostic, no NVS, serialized behind STOP');

// Regression: a lost GATT confirmation must not erase draft role/count edits,
// silently retry flash writes, or call an unverified commit "Saved".
assert(unifiedEditor.includes('var choosingRole by remember(index,i)') &&
       unifiedEditor.includes('if(i != selectedZone.coerceIn(0,zones.length()-1)) continue') &&
       unifiedEditor.includes('LaunchedEffect(index,pixelCount)') &&
       unifiedEditor.includes('LaunchedEffect(index,i,count)') &&
       compose.includes('outputDrafts[index] = confirmed.toString()') &&
       compose.includes('outputDrafts[index] = draftText') &&
       bleSource.includes('Save unverified; reconnect and Reload before retrying.') &&
       !bleSource.includes('cfgFail("BLE config write failed: "+status);saveOutput('),
       'BLE-133 reconciliation or single selected zone editing regressed');

// Overseer 4 Layout acceptance: one Output / Panel / Zone editor and
// zero-LED first-run drafts, without making a fake saved CORE output.
const finalLayout=compose.split('private fun LayoutPage(t: Tone) {')[1]
  ?.split('private fun refreshPanelGeometry()')[0];
const finalOutput=compose.split('private fun HardwareOutputEditor(t: Tone) {')[1];
assert(finalLayout && finalOutput &&
  finalLayout.includes('HardwareOutputEditor(t)') &&
  finalLayout.includes('linked zones are edited inside the cards above') &&
  finalLayout.includes('z.optInt("physicalSection",-1) in 0 until draftZoneCount) continue') &&
  finalLayout.includes('if(showingUnlinked) for(i in 0 until zoneCount)') &&
  finalOutput.includes('val index = captureOutput.intValue') &&
  finalOutput.includes('val authoritativeMap = runCatching') &&
  finalOutput.includes('coreZone.panelId') &&
  finalOutput.includes('if(linkedId!=null && linkedIndex!=null)') &&
  finalOutput.includes('ZoneDotPreview(t,localCount,reversed)') &&
  finalOutput.includes('ble.savePanelV4(index, draftText)') &&
  !finalOutput.includes('Saved Output 1') &&
  !finalOutput.includes('BLE test1'),
  'One CORE-backed editor must retain all local zone tools without duplicate cards');
assert(model.includes('wholeNumber(z.opt("length"), 0, 10000)') &&
  model.includes('zoneCount() >= MAX_ZONES || length < 0') &&
  model.includes('index < 0 || length < 0') &&
  finalLayout.includes('CapsuleSlider(n, 0, 10000, t,') &&
  finalOutput.includes('.put("pixels",0)') &&
  finalOutput.includes('mutableStateOf(outputDrafts[index] ?: outputs[index]?.toString() ?: JSONObject()') &&
  finalOutput.includes('SideEffect { layoutDraftZoneCounts[index] = zones.length() }') &&
  finalOutput.includes('if(ble.lastConfirmedPanelSnapshot(index)==null)') &&
  finalOutput.includes('if (zones.length() < 8) Pill("+ Add zone", t)') &&
  bleSource.includes('Arrays.equals(panelSaveWire,snapshot.wire)'),
  'Zero LED drafts must stay inert; saved CORE state still requires readback');

assert(!compose.includes('ble.saveOutput(index, draftText)') &&
       !compose.includes('ble.readOutputIfReady(index)') &&
       panelDraft.includes('PanelPreviewProtocol.MapIdentity.parse(snapshot)') &&
       panelDraft.includes('v4_editable') &&
       panelV4.includes('parseStartFreshChallenge') &&
       bleSource.includes('startFreshPanelV4(int lane)') &&
       bleSource.includes('panelSavePhase==7') &&
       finalOutput.includes('Pill("Start Fresh"') &&
       finalOutput.includes('ble.startFreshPanelV4(index)'),
       'User-facing Layout must use only authoritative Panel v4 and protected Start Fresh');
assert(finalLayout.includes('LightingDirectory(t)') &&
  finalLayout.includes('if(advancedOpen) {') &&
  finalOutput.includes('Pill(outputTab(0),t,index==0)') &&
  finalOutput.includes('Pill(outputTab(1),t,index==1)') &&
  finalOutput.includes('ble.readPanelLaneSnapshot(index)') &&
  finalOutput.includes('outputDirty[index] = true') &&
  compose.includes('if (completedSave || outputDirty[index] != true)') &&
  model.includes('updateZoneOrganization(String id, String field, String text)') &&
  model.includes('z.optString(key).length() > 64') &&
  finalLayout.includes('EffectsPreview(t, selectedEffect.intValue)'),
  finalOutput.includes('Pill("Remove zone",t)') &&
  finalOutput.includes('pendingRemoveSection=selectedZone.coerceIn(0,zones.length()-1)') &&
  finalOutput.includes('if(outputToolsOpen) Pill("Check first LED') &&
  finalOutput.includes('outputLoadStatus[index]=="loading"') &&
  compose.includes('lastAutoGeometryRefresh.intValue!=modelRevision'),
  'One-card Output/Zone editing must expose zone removal, restore locked preview and terminate loading attempts safely');
