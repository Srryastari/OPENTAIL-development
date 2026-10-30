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
  'frame.colorAt(first + i)', 'NOT a simulated effect.', 'CapsuleSlider(n, 1, 10000, t,', 'finished = { applyLiveZone() }',
  'project.previewZoneOffsets(id, n)', 'project.setPixelColor(id, activePixel, hex)',
  'project.assignMany(', 'project.setSequence(', 'project.importJson(', 'project.exportJson()',
  'ble.canControl()', 'supportedIds.value', 'ble.showFx(', 'ble.saveOutput(index, draftText)',
  'BleConfigCodec.encode(JSONObject(draftText))']) {
  assert(compose.includes(required),'Active Compose implementation regressed: '+required);
}
assert(compose.includes('private fun CapsuleSlider(') &&
       compose.includes('height(12.dp)') &&
       compose.includes('shadow(5.dp, RoundedCornerShape(99.dp)') &&
       compose.includes('Field(hex, "#RRGGBB", t, Modifier.weight(1f))') &&
       !compose.includes('Slider(value = n.coerceIn(1, 10000)') &&
       compose.includes('Modifier.heightIn(min = 48.dp)'),
       'All editing controls should use the approved capsule slider and accessible pills');
assert(compose.includes('val pageScroll = remember(page) { ScrollState(0) }') &&
       compose.includes('verticalScroll(pageScroll)') &&
       compose.includes('val window = total.coerceAtMost(visiblePixels)') &&
       compose.includes('listOf(8, 16, 32, 64)') &&
       compose.includes('pendingLoad = i') &&
       compose.includes('title = { Text("Replace current design?") }') &&
       compose.includes('Local trigger color swatch only. No simulated effect'),
       'Compose destination scroll, exact pixel zoom and destructive-edit UX regressed');
assert(compose.includes('if (offlinePreview.value) return') &&
       compose.includes('if (i != 1) offlinePreview.value = false') &&
       compose.includes('if (!offlinePreview.value && ble.canCaptureFrame())') &&
       compose.includes('CORE capture is disabled in offline preview mode.') &&
       compose.includes('Offline mode: edits update only this local design; no CORE LED commands.'),
       'Preview Offline must be local-only, suppressing all effect and snapshot BLE edits');
assert(compose.includes('NavigationPill(i, title, t, page == i)') &&
         compose.includes('private fun NavigationGlyph(index: Int, color: Color)') &&
         compose.includes('1 -> { // Show Modes: sparkle') &&
         compose.includes('2 -> { // Layout: four squares') &&
         compose.includes('3 -> { // Triggers: lightning') &&
         compose.includes('4 -> { // Auto Events: sun'),
         'Native navigation icons must preserve locked meanings without emoji or WebView');
assert(bleSource.includes('frameCapture.cancel(); // GATT close may never produce a disconnect callback') &&
       (bleSource.match(/cfgBusy\|\|writePending\|\|readReplyPending\|\|fxCapsBusy\|\|triggerMapBusy\|\|frameCapture.busy\(\)\|\|authPending\|\|pairPending\|\|challengePending/g)||[]).length>=3 &&
       compose.includes('outputs.clear() // readback from a disconnected/unauthorized controller') &&
       compose.includes('captureMessage.value = "CORE frame not captured"'),
       'Clear stale CORE output UI and prevent conflicting GATT operations');
assert(!/WebView\(|BottomNavigation|NavigationBar\(/.test(compose),
  'Active Compose must have no WebView or bottom navigation');
const homeSection=compose.split('private fun HomePage(t: Tone, changePage: (Int) -> Unit) {')[1]
                         .split('private fun EffectsPreview(')[0];
const settingsSection=compose.split('private fun SettingsPage(t: Tone, changePage: (Int) -> Unit) {')[1]
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
       compose.includes('enterOfflinePreview()\n                        page = 1') &&
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
assert(homeSection.includes('Text("Bluetooth · " + ble.connectionPhase()') &&
       settingsSection.includes('Text("Bluetooth · " + ble.connectionPhase()') &&
       homeSection.includes('Caption(bleMessage.value, t)') &&
       settingsSection.includes('Caption(bleMessage.value, t)') &&
       bleSource.includes('String connectionPhase()'),
       'Bluetooth status pill must reflect actual connection state, not transient messages');
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
  'CapsuleSlider(n, 1, 10000, t,', 'finished = { applyLiveZone() }',
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
  'project.deleteTrigger(', 'project.assignMany(', 'applyTriggerStyle()']) {
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

// GATT callback ownership must be checked on both modern/legacy Android
// callback overloads. A retired controller must not erase live new-device
// state, reuse FX capabilities, or deliver old authorization/config replies.
assert(bleSource.includes('private volatile BluetoothGatt gatt;') &&
       bleSource.includes('if (stopped || connection != gatt) {') &&
       (bleSource.match(/if\(stopped \|\| connection!=gatt\)return;/g)||[]).length === 5 &&
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
 'revision == ((baseRevision + 1) & 0xffff)'
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
assert(compose.includes('private val projectLoadRevision = mutableIntStateOf(0)') &&
       compose.includes('var name by remember(projectLoadRevision.intValue) { mutableStateOf(project.name()) }') &&
       compose.includes('private fun refreshLoadedProject()') &&
       compose.includes('if (imported) refreshLoadedProject()') &&
       compose.includes('if (restored) refreshLoadedProject()') &&
       !compose.includes('var name by remember(revision.intValue)') &&
       compose.includes('fun applyLiveZone(): Boolean') &&
       compose.includes('if (applyLiveZone()) editingId = null') &&
       compose.includes('if (project.removeZone(id)) {'),
       'Layout must follow committed project revisions and retain failed edit dialogs');

// Imported trigger dictionaries may omit older built-in defaults, but they
// must not contain malformed entries. A read must never persist new profiles.
assert(model.includes('if (inputProfiles.optJSONObject(profileName) == null)') &&
       model.includes('throw new JSONException("Invalid trigger profile: " + profileName);') &&
       model.includes('try { return defaultTrigger(name); }') &&
       !model.includes('profiles().put(name, p);save();') &&
       storageTest.includes('malformedTriggerProfilesNeverOverwriteSavedDataOnRead'),
       'Trigger project imports/readers must never silently replace invalid profiles');

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
       compose.includes('project.addZoneAt("Zone " + (project.zoneCount() + 1), 30, index + 1)') &&
       !compose.includes('project.moveZone(project.zoneCount() - 1, index + 1)'),
       'Add next zone must insert and reindex with one persisted transaction');
assert(compose.includes('if (project.assignMany(selections, zoneId, !checked)) refresh()') &&
       compose.includes('if (project.name(it)) refresh()') &&
       compose.includes('else name = project.name() // restore last committed value') &&
       compose.includes('if (project.setPresets(next))') &&
       compose.includes('Could not remove preset'),
       'Multi-trigger, name, and preset failure handling must not show false saved state');

// Palette engine: native data-only catalog, exact stop-data swatches, and
// separate effect, sequence-step and shared-sequence state. No invented
// gradient preview or unadvertised CORE palette commands are allowed.
for (const required of ['private fun PalettePicker(', 'NativePaletteEngine.lookup(project.raw(),',
  'Field(search, "Search palette name or ID"', 'PaletteSwatch(chosen, t)',
  'PalettePicker("Palette", paletteId, t)', 'putInt("paletteId", id)',
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
// Firmware has a narrow existing palette BLE byte. This must not be
// generalized to WLED palette parity or new wire operations.
assert(palettes.includes('paletteId == 0 || (effectId == 32768 && paletteId in 2..4)') &&
       palettes.includes('fun corePreviewPalette(effectId: Int, selectedId: Int)') &&
       compose.includes('NativePaletteEngine.corePreviewPalette(') &&
       compose.includes('!NativePaletteEngine.coreSupports(effectId, paletteId)') &&
       compose.includes('else {') &&
       compose.includes('livePreview.stop()') &&
       compose.includes('supportedIds.value = emptySet() // never reuse another CORE') &&
       compose.includes('previewCurrentEffect(effect) // one safety gate for every edit gesture') &&
       compose.includes('CORE accepts default palette 0 and Embers palettes 2–4') &&
       bleSource.includes('(effectId==32768&&(paletteId==2||paletteId==3||paletteId==4))'),
       'Only the verified existing CORE palette subset may be previewed over BLE');
const preview=fs.readFileSync(
  'android/app/src/main/java/com/opentail/android/LiveBlePreview.kt','utf8');
assert(preview.includes('frame.paletteId, 5') &&
       preview.includes('NativePaletteEngine.coreSupports(frame.effectId, frame.paletteId)'),
       'BLE preview may not send unsupported palettes');
const corePaletteTest=fs.readFileSync(
  'android/app/src/test/java/com/opentail/android/NativePaletteCoreContractTest.kt','utf8');
assert(corePaletteTest.includes('corePreviewPalette(32768, palette)') &&
       corePaletteTest.includes('otherEffectsNeverClaimEmbersPaletteSupport'),
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
  'if (ble.captureFrame(captureOutput.intValue))', 'pendingFrameRevision = frameRevision',
  'pendingFrameOutput = captureOutput.intValue']) {
  assert(compose.includes(expected),'CORE frame invalidation or capture admission regressed: '+expected);
}
assert(compose.includes('captureOutput.intValue = 0; invalidateCoreFrame()') &&
       compose.includes('captureOutput.intValue = 1; invalidateCoreFrame()') &&
       compose.includes('private fun enterOfflinePreview()') &&
       compose.includes('invalidateCoreFrame()\n        if (liveArmed.value)') &&
       bleSource.includes('private volatile boolean frameCallbackPending=false;') &&
       bleSource.includes('boolean captureFrame(int output)') &&
       bleSource.includes('frameCapture.busy() || frameCallbackPending') &&
       bleSource.includes('finally {frameCallbackPending=false;}'),
       'Capture completion must serialize with new requests and STOP/output/offline changes');

assert(!compose.includes('ble.setPalette(') && !compose.includes('ble.sendPalette(') &&
       !palettes.includes('WLED firmware'), 'Unadvertised firmware palette writes or branding');
assert(compose.includes('Preview unavailable') && compose.includes('shared option is off') &&
       compose.includes('Other palette choices stay local'),
       'Palette UI must label unknown previews, restoration and BLE limitations');

console.log('PASS: shipped Kotlin/Compose navigation, screens, universal geometry, BLE security, verified CORE frame path and project v4 contracts (static only).');
