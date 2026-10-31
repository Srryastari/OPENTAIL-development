#!/usr/bin/env node
'use strict';
/* Guard the locked universal live-edit contract on every Android change.
 * Static checks do not claim phone/BLE/pixel hardware acceptance. */
const fs=require('fs'), assert=require('assert/strict');
const compose=fs.readFileSync('android/app/src/main/java/com/opentail/android/ComposeMainActivity.kt','utf8');
const project=fs.readFileSync('android/app/src/main/java/com/opentail/android/NativeProject.java','utf8');
const preview=fs.readFileSync('android/app/src/main/java/com/opentail/android/LiveBlePreview.kt','utf8');
const ble=fs.readFileSync('android/app/src/main/java/com/opentail/android/BleDiscovery.java','utf8');
const qa=fs.readFileSync('android/qa/check_release.py','utf8');

const zoneBlock=compose.split('private fun LayoutPage(t: Tone) {')[1].split('private fun addZoneAfter(')[0];
const triggerBlock=compose.split('private fun TriggersPage(t: Tone) {')[1].split('private fun AutoEventsPage(')[0];
const effectBlock=compose.split('private fun ShowModesPage(t: Tone) {')[1].split('private fun SequencePage(')[0];
const hardwareBlock=compose.split('private fun HardwareOutputEditor(t: Tone) {')[1];
assert(zoneBlock && triggerBlock && effectBlock && hardwareBlock);

for(const required of [
  'project.previewZoneOffsets(id, n)',
  'CapsuleSlider(n, 1, 10000, t,', 'finished={ applyLiveZone() }',
  'project.editZone(id,label,n,reverse)',
  'project.setPixelColor(id,activePixel,hex)',
  'project.clearPixelColor(id,activePixel)',
  'PaletteEditor("Pixel "+activePixel',
  'name=it', 'project.name(it)',
]) assert(zoneBlock.includes(required), 'Live layout regression: '+required);
assert(compose.includes('autoEventPreviewEffect.intValue = effectId') &&
       compose.includes('EffectsPreview(t, eventPreviewId)') &&
       !compose.includes('selectedEffect.intValue = effectId') &&
       compose.includes('enterOfflinePreview()'),
       'Auto Events preview stays local and never changes general Show Modes selection.');
assert(!zoneBlock.includes('ble.saveOutput(') && !zoneBlock.includes('ble.showFx('),
  'Authoring layout must never directly write a physical output mapping.');

for(const required of [
  'var selections by remember',
  'project.assignMany(selections,zoneId,!checked)',
  'confirmButton = { Pill("Done",t,true) { editing=false }',
  'Pill("Edit Selected", t, selections.isNotEmpty(), enabled=selections.isNotEmpty())'
]) assert(triggerBlock.includes(required), 'Live assignment-only trigger regression: '+required);
assert(triggerBlock.includes('project.editTriggerStyle(selections,null,hex)') &&
       !triggerBlock.includes('ble.showFx('),
       'Trigger edit must only assign zones, never effects or colors');
assert(!triggerBlock.includes('ble.saveOutput('),
 'Trigger authoring must never modify controller NVS.');

for(const required of [
  'EffectsPreview(', 'ApprovedWledEffectsEditor(',
  'onLiveToggle = {', 'onParam = { id,key,value ->',
  'onColor = { id,slot,rgb ->', 'onPalette = { id,selected ->',
  'if(liveArmed.value) previewCurrentEffect(id)',
  'livePreview.stop()', 'ble.stopShow()'
]) assert(effectBlock.includes(required), 'Approved Live FX regression: '+required);

assert(triggerBlock.includes('project.assignMany(selections,zoneId,!checked)') &&
       !triggerBlock.includes('selections.forEach { project.assign('),
       'Multi-select assignments must commit once, not partially save per trigger');

for(const required of [
  'selectedPixels','pixelColor(String id,int pixel)',
  'setPixelColor(String id,int pixel,String hex)',
  'clearPixelColor(String id,int pixel)', 'synchronized boolean editZone',
  'painted.remove(key)','all.remove(id)',
]) assert(project.includes(required), 'v4 pixel-edit regression: '+required);

for(const required of [
  'class LiveBlePreview', 'private val intervalMs = 60L', 'handler.postDelayed(drain',
  'if (!armed) return', '!ble.canControl()', 'frame.effectId !in supportedIds()',
  'frame.brightness.coerceIn(0, 24)', 'frame.paletteId, 5', 'ble.showFx(', 'fun stop()',
  'handler.removeCallbacks(drain)', 'ble.stopShow()'
]) assert(preview.includes(required), 'Transient hardware preview regression: '+required);
assert(compose.includes('Text("Bluetooth · " + ble.connectionPhase()') &&
       compose.includes('Caption(bleMessage.value, t)'),
       'Connection status and transient diagnostics must remain separate');
const approvedPreview = compose.split('private fun EffectsPreview(t: Tone, effect: Int, animateOffline: Boolean = false) {')[1]
  .split('private fun ShowModesPage(')[0];
assert(approvedPreview.includes('val columns = 30') &&
       approvedPreview.includes('val capsuleHeight = 24.dp.toPx()') &&
       approvedPreview.includes('val actualFrame = coreFrame.value') &&
       approvedPreview.includes('hasLiveColors -> actualFrame') &&
       approvedPreview.includes('Color(frame!!.colorAt(sample))') &&
       approvedPreview.includes('offlinePreview.value && hasProjectColors -> projectPaint') &&
       approvedPreview.includes('offlinePreview.value -> null') &&
       approvedPreview.includes('val readback = outputs[captureOutput.intValue]') &&
       approvedPreview.includes('Configured zone colors'),
   'Final preview must preserve 30 capsules and distinguish genuine CORE live frames from configured role colors.');
assert(compose.includes('if (offlinePreview.value) return') &&
       compose.includes('if(!foregroundActivity.value || offlinePreview.value || previewPaused.value || !ble.canCaptureFrame()) return') &&
       compose.includes('offlinePreview.value = true') &&
       compose.includes('livePreview.stop()'),
       'Preview Offline must not issue effect or frame capture commands');
// Only palette values actually implemented by current CORE may be sent.
// Selecting a local-only palette must stop any stale physical palette demo.
const palette=fs.readFileSync('android/app/src/main/java/com/opentail/android/NativePaletteEngine.kt','utf8');
assert(palette.includes('fun coreSupports(effectId: Int, paletteId: Int): Boolean') &&
       palette.includes('BleFxPalettePolicy.isWireCandidate(effectId, paletteId)') &&
       palette.includes('fun corePreviewPalette(effectId: Int, selectedId: Int)'),
  'CORE palette support must remain a narrowly scoped protocol gate');
assert(preview.includes('val paletteId: Int,') &&
       preview.includes('NativePaletteEngine.coreSupports(frame.effectId, frame.paletteId)') &&
       preview.includes('frame.paletteId, 5'),
  'Unsupported palette IDs must never enter a physical BLE FX packet');
const previewGate=compose.split('private fun previewCurrentEffect(effectOverride: Int? = null) {')[1]
  ?.split('private val importer =')[0];
assert(previewGate &&
       previewGate.includes('effectId !in supportedIds.value') &&
       previewGate.includes('!NativePaletteEngine.coreSupports(effectId, paletteId)') &&
       previewGate.indexOf('!NativePaletteEngine.coreSupports(effectId, paletteId)') <
         previewGate.indexOf('livePreview.arm()') &&
       previewGate.includes('livePreview.stop()') &&
       previewGate.includes('liveArmed.value = false') &&
       effectBlock.includes('if(liveArmed.value) previewCurrentEffect(id)') &&
       compose.includes('NativePaletteEngine.corePreviewPalette(effectId, paletteId)'),
  'Owner-triggered Effects preview must reject unsupported palettes before arming BLE without silent fallback');
assert(compose.includes('supportedIds.value = emptySet() // never reuse another CORE\'s FX capability list'),
       'Disconnect must clear stale effect capability ownership');

assert(compose.includes('if (!livePreview.isArmed())') &&
       compose.includes('if (!ble.canControl() || supportedIds.value.isEmpty()) return'),
  'Owner-authorized live preview must retain secure gated arming; editor selection alone never starts live Show Mode.');
assert(!compose.includes('Preview live on CORE bench') &&
       compose.includes('Every editor is live.'),
  'No optional Live Edit Mode toggle permitted in Android.');
assert(!preview.includes('saveOutput(') && !preview.includes('fxPreset('),
  'Live FX preview cannot use NVS config or preset writes.');
assert(compose.includes('if (liveArmed.value)') && compose.includes('livePreview.stop()'),
 'Live hardware preview must STOP on activity background/exit.');
assert(hardwareBlock.includes('fun adjustZone(') &&
       hardwareBlock.includes('fun adjustOutput(') &&
       hardwareBlock.includes('ValueSlider("Next start · zero-based"') &&
       hardwareBlock.includes('next.put("pixels", total)') &&
       hardwareBlock.includes('it.put("pixels", remaining)') &&
       hardwareBlock.includes('structureVersion++') &&
       hardwareBlock.includes('BleConfigCodec.MAX_LEDS_PER_OUTPUT') &&
       !hardwareBlock.includes('623') &&
       hardwareBlock.includes('PixelStrip(t,'),
       'Live hardware draft cursor/total/visual preview must recompute with each slider gesture.');
assert(hardwareBlock.includes('ble.saveOutput(index, draftText)') &&
       hardwareBlock.includes('BleConfigCodec.encode(JSONObject(draftText))') &&
       hardwareBlock.includes('Save to CORE · verify readback'),
 'Persistent hardware mapping requires explicit validated commit and readback.');
assert(ble.includes('frameCapture.cancel();clearPreviewCommand();palette.reset(); // GATT close may never produce a disconnect callback') &&
       compose.includes('outputs.clear() // readback from a disconnected/unauthorized controller') &&
       ble.includes('||fxCapsBusy||infoReadPending||triggerMapBusy||frameCapture.busy()||palette.busy()||nextPacket!=null||authPending||pairPending||challengePending'),
       'Disconnect and GATT operation sequencing regression');
assert(ble.includes('void showFx(') && ble.includes('void stopShow()'),
 'Preview must reuse authenticated CORE FX/STOP commands.');
// A STOP waiting for a BLE GATT transaction must never be overwritten by
// a later live-edit frame. Coalesced edits may follow, but not precede STOP.
const stopQueue = ble.split('private static boolean isStopPacket(byte[] data) {')[1]
  ?.split('@SuppressWarnings({"MissingPermission","deprecation"})')[0];
const queuePump = ble.split('private void pumpQueue(){')[1]?.split('private String preferredPending')[0];
assert(stopQueue && queuePump &&
  stopQueue.includes('nextPacket=Arrays.copyOf(data,data.length);') &&
  stopQueue.includes('afterStopPacket=null;') &&
  stopQueue.includes('frameCapture.busy() || palette.busy() || nextPacket!=null) {') &&
  stopQueue.includes('else if (isStopPacket(nextPacket))') &&
  stopQueue.includes('afterStopPacket=Arrays.copyOf(data,data.length);') &&
  stopQueue.includes('frameCapture.abortForStop();') &&
  queuePump.includes('pendingPacket=nextPacket;') &&
  queuePump.includes('nextPacket=isStopPacket(pendingPacket)?afterStopPacket:null;') &&
  queuePump.includes('afterStopPacket=null;'),
  'STOP priority and latest-following-FX ordering must survive GATT queue contention');
assert(ble.includes('pendingPacket = null; nextPacket = null; afterStopPacket = null;') &&
       (ble.match(/pendingPacket=null;nextPacket=null;afterStopPacket=null;/g)||[]).length >= 2 &&
       ble.includes('private void recoverStalledGatt(BluetoothGatt owner, String reason)'),
  'Post-STOP queue must reset on adapter off, reconnect and disconnect');
assert(qa.includes('test_native_source.cjs'), 'Release QA disconnected.');
console.log('PASS: live authoring pixel/zone/trigger/effect contract; temporary authenticated bounded BLE FX; no NVS writes on edit.');
