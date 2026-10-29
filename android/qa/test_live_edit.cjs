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
  'CapsuleSlider(n, 1, 10000, t,', 'finished = { applyLiveZone() }',
  'project.editZone(id, label, n, reverse)',
  'project.setPixelColor(id, activePixel, hex)',
  'project.clearPixelColor(id, activePixel)',
  'PaletteEditor("Pixel " + activePixel',
  'name = it', 'project.name(it)', 'confirmButton = { Pill("Done"',
]) assert(zoneBlock.includes(required), 'Live layout regression: '+required);
assert(compose.includes('selectedEffect.intValue = effectId') &&
       compose.includes('val effect = selectedEffect.intValue'),
       'Auto Events must preview the correct selected effect immediately.');
assert(!zoneBlock.includes('ble.saveOutput(') && !zoneBlock.includes('ble.showFx('),
  'Authoring layout must never directly write a physical output mapping.');

for(const required of [
  'fun applyTriggerStyle()','applyTriggerStyle()','project.assign(',
  'project.save()', 'confirmButton = { Pill("Done"',
]) assert(triggerBlock.includes(required), 'Live trigger regression: '+required);
assert(!triggerBlock.includes('ble.saveOutput('),
 'Trigger authoring must never modify controller NVS.');

for(const required of [
  'EffectsPreview(', 'previewCurrentEffect(effect)',
  'PaletteEditor("Foreground"', 'PaletteEditor("Background"', 'PaletteEditor("Accent"',
]) assert(effectBlock.includes(required), 'Live FX regression: '+required);

for(const required of [
  'selectedPixels','pixelColor(String id,int pixel)',
  'setPixelColor(String id,int pixel,String hex)',
  'clearPixelColor(String id,int pixel)', 'synchronized boolean editZone',
  'painted.remove(key)','all.remove(id)',
]) assert(project.includes(required), 'v4 pixel-edit regression: '+required);

for(const required of [
  'class LiveBlePreview', 'private val intervalMs = 120L', 'handler.postDelayed(drain',
  'if (!armed) return', '!ble.canControl()', 'frame.effectId !in supportedIds()',
  'frame.brightness.coerceIn(0, 24)', '0, 5', 'ble.showFx(', 'fun stop()',
  'handler.removeCallbacks(drain)', 'ble.stopShow()'
]) assert(preview.includes(required), 'Transient hardware preview regression: '+required);
assert(compose.includes('ExactLedFrame.fromProjectModel(project)') &&
       compose.includes('frame.colorAt(first + i)') &&
       compose.includes('First visible pixel') &&
       compose.includes('NOT a simulated effect.'),
  'Preview must show indexed project pixels rather than fabricated animation frames.');
assert(compose.includes('if (!livePreview.isArmed())') &&
       compose.includes('if (!ble.canControl() || supportedIds.value.isEmpty()) return'),
  'Authorized hardware preview must start automatically on gesture, with no Live Edit Mode.');
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
assert(ble.includes('void showFx(') && ble.includes('void stopShow()'),
 'Preview must reuse authenticated CORE FX/STOP commands.');
assert(qa.includes('test_native_source.cjs'), 'Release QA disconnected.');
console.log('PASS: live authoring pixel/zone/trigger/effect contract; temporary authenticated bounded BLE FX; no NVS writes on edit.');
