// Protected approved in-chat concept: no design reinterpretation or loss of interactions.
const assert=require('node:assert/strict');
const fs=require('node:fs');
const html=fs.readFileSync('web-studio/opentail.htm','utf8');
const shell=fs.readFileSync('android/app/src/main/assets/unified-shell.js','utf8');
const layoutStart=html.indexOf('function otBaseArrangementLayout()');
const layoutEnd=html.indexOf('function triggerEditor()',layoutStart);
const layout=html.slice(layoutStart,layoutEnd);
assert(layoutStart>0&&layoutEnd>layoutStart,'Layout must have one editable implementation');
assert.equal((layout.match(/class="card /g)||[]).length,1,'Layout must be a single main card');
assert(layout.includes('ot-chat-layout')&&layout.includes('ot-chat-zone-list'),'Zone list must nest inside Layout card');
assert(layout.indexOf('ot-chat-heading')<layout.indexOf('ot-chat-zone-list'),'Add Zone must precede zone list');
assert(layout.includes('ot-chat-pill ot-chat-primary')&&layout.includes('Add Zone'),'Pill Add Zone required');
assert(layout.includes('ot-chat-zone-toggle')&&layout.includes('aria-expanded=')&&layout.includes('ot-chat-chevron'),'Expandable zones with chevrons required');
assert(layout.includes('ot-chat-input')&&layout.includes('ot-chat-length'),'Pill project/zone names and clean length slider required');
assert(layout.includes('>Reverse</strong>'),'Control must be named Reverse without mirror');
assert(layout.includes('ot-chat-led-strip')&&layout.includes('Array.from({length:item.length}')&&html.includes('function otLayoutPreviewLength('),'One-to-one pixel preview updates during slider input');
assert(layout.indexOf('ot-chat-led-strip')<layout.indexOf('ot-chat-remove'),'Red Remove Zone pill must follow preview');
assert(html.includes('ot-chat-danger')&&html.includes('border-radius:999px'),'Neutral pill controls plus red destructive action');
assert(html.includes('otConceptNav()')&&html.includes("['events','Auto Events'"),'Four top concept icons/tabs must remain');
const builder=html.slice(html.indexOf('function builder(){'),html.indexOf('function otTriggerColor(',html.indexOf('function builder(){')));
assert(builder.includes('otToggleTriggerSelection')&&builder.includes('otEditSelectedTriggers'),'Trigger selection is independent of testing');
assert(builder.includes('otTriggerTestToggle'),'Original physical/offline safe testing retained');
assert(builder.includes('ot-chat-trigger-action'),'Rounded Trigger cards are required');
assert(html.includes('function otTriggerBatchApply(')&&html.includes('function otTriggerBatchAssign('),'Real cross-profile editing and assignments retained');
const multiStart=html.indexOf('let otSelectedTriggerNames='),multiEnd=html.indexOf('let otTriggerEditorOpen=',multiStart);
assert(multiStart>0&&multiEnd>multiStart,'Multi profile adapter missing');
const fragment=html.slice(multiStart,multiEnd);
let persisted=0,physical=0;
const profiles={
 Park:{zones:['a'],pattern:'Solid',brightness:35,color:'#ff0000',speed:50,sectionAnimations:{a:{pattern:'Solid',brightness:35,intensity:65}}},
 Brake:{zones:['a','b'],pattern:'Solid',brightness:85,color:'#ff0000',speed:60,sectionAnimations:{a:{brightness:85},b:{brightness:85}}},
 'Left Turn':{zones:[],brightness:80,sectionAnimations:{}}
};
const state={profiles,layout:[{id:'a'},{id:'b'}],triggerStatus:'',connected:false,benchSignals:{}};
const make=new Function('state','OT_MAIN_TRIGGERS','otCustomTriggerNames','otTriggerEditorOpen','otMultiSelected','otEditZone','otTriggerTestActive','otTriggerPending','render','persist','otCanonicalEffect','otShowToast','applyBenchTriggers',
fragment+';return {select:otToggleTriggerSelection,apply:otTriggerBatchApply,assign:otTriggerBatchAssign,selected:otSelectedTriggers};');
const m=make(state,['Park','Brake','Left Turn'],()=>[],false,[],null,()=>false,null,()=>{},()=>{persisted++;return true},v=>v,()=>{},()=>{physical++});
m.select('Park');m.select('Brake');
assert.deepEqual(m.selected(),['Park','Brake'],'Park and Brake must select simultaneously');
assert(m.apply('brightness',72));
assert.equal(profiles.Park.brightness,72);
assert.equal(profiles.Brake.sectionAnimations.b.brightness,72);
assert.equal(profiles.Park.sectionAnimations.a.intensity,65,'Untouched section fields preserved');
assert.equal(profiles['Left Turn'].brightness,80,'Unselected trigger preserved');
assert(m.apply('pattern','Breathe'));
assert.equal(profiles.Brake.sectionAnimations.a.pattern,'Breathe');
m.assign('b',true);
assert(profiles.Park.zones.includes('b'),'Multi-edit can assign a zone to each selected trigger');
m.assign('a',false);
assert(!profiles.Brake.zones.includes('a'),'Multi-edit can unassign a zone from each selected trigger');
assert.equal(physical,0,'No controller writes in local batch editor without connection');
assert(persisted>=3);
m.select('Park');
assert.deepEqual(m.selected(),['Brake'],'Tap selected Trigger to deselect without stopping tests');
const eventStart=html.indexOf('function otChatAutoSet('),eventEnd=html.indexOf('function otStudioRoute(tab){',eventStart);
const event=html.slice(eventStart,eventEnd);
assert(event.includes('function otChatEventStart()')&&event.includes('requestAnimationFrame(tick)'),'Offline preview must use a local animation loop');
assert(!event.includes('getJson(')&&!event.includes('queueDevice(')&&!event.includes('connectWled(')&&!event.includes('fetch('),'Offline event preview must not communicate with controllers');
assert(event.includes('Startup')&&event.includes('Shutdown')&&event.includes('SIM only.'),'Event editor must support both startup and shutdown with honest simulation labels');
assert(html.includes('id="ot-approved-chat-01956"'),'Layout style tokens are pinned');
assert(html.includes('id="ot-approved-chat-trigger-01956"'),'Trigger style tokens are pinned');
assert(shell.includes('UI baseline v1: lock Android wrapper to neutral OLED'),'Android cannot inject an alternate green theme');
console.log('Protected in-chat OLED UI geometry, multi-trigger editing, local event animation and controller isolation contracts passed');



// Approved Android six-tab nav: the native wrapper owns exactly one global nav.
const navStart=shell.indexOf('const destinations = [');
const navEnd=shell.indexOf('];',navStart);
assert(navStart>0&&navEnd>navStart,'Android canonical navigation list missing');
const androidNav=shell.slice(navStart,navEnd);
const navLabels=[...androidNav.matchAll(/\['(?:home|scenes|layout|builder|events|settings)','([^']+)'/g)].map(m=>m[1]);
assert.deepEqual(navLabels,['Home','Show Modes','Layout','Triggers','Auto Events','Settings'],'Approved Android six-tab order must be exact');
assert(shell.includes('document.body.insertBefore(nav, header.nextSibling)'),'Canonical navigation belongs below header, not at page bottom');
assert(shell.includes('position:sticky!important;top:0!important'),'Native pill navigation must be at the top');
assert(shell.includes('#app .ot-chat-nav{display:none!important}'),'Hide duplicate Studio concept tabs inside Android');
assert(shell.includes('autoEventsOpen=target===\'events\''),'Auto Events must have its own top-level nav identity');
assert(shell.includes("window.otConceptNavigate(target)"),'Route Auto Events through existing offline-safe concept handler');
assert(shell.includes('body:not(.ot-home-active) .ot-connection-grid{display:none!important}'),'Connection actions belong on Home');
assert(!androidNav.includes("'Shows'"),'Use approved full Show Modes label');
console.log('Approved Android six-tab nav placement/order and duplicate-tab suppression checks passed');


// Approved Show Modes hierarchy: tabs, then bounded preview, then one editing workspace.
const scenesStart=html.indexOf('function scenes(){');
const scenesEnd=html.indexOf('let otPresetEditorId=',scenesStart);
const sceneSource=html.slice(scenesStart,scenesEnd);
assert(scenesStart>0&&scenesEnd>scenesStart,'Show Modes source not found');
const tabsAt=sceneSource.indexOf('class="ot-subnav"');
const previewAt=sceneSource.indexOf('otPixelPreview()');
const editorAt=sceneSource.indexOf("otSub==='Effects'");
assert(tabsAt>=0&&previewAt>tabsAt&&editorAt>previewAt,
  'Show Modes hierarchy must be tabs → Live Lighting Preview → editor');
// Settings is one expandable workspace, with no duplicated primary connection actions.
const settingsStart=shell.indexOf('settings.innerHTML =');
const settingsEnd=shell.indexOf("const nav = document.createElement('nav')",settingsStart);
const settingsSource=shell.slice(settingsStart,settingsEnd);
for(const label of ['App Preferences','Diagnostics','Project Backup','About'])
  assert(settingsSource.includes(label),'Settings section missing: '+label);
assert.equal((settingsSource.match(/class="ot-settings-group"/g)||[]).length,4,
  'Settings sections belong to one expandable workspace');
assert(!settingsSource.includes('data-ot-native="scan"')&&!settingsSource.includes('data-ot-native="connect"'),
  'Home owns primary connection actions');
assert(shell.includes("window.confirm('Import this project?"),
  'Import must confirm before replacing the local working project');
console.log('Show Modes ordering, Settings groups, and project import confirmation protected');



// Custom triggers alone get a row-level trash icon with inline confirmation.
const builderCode=html.slice(html.indexOf('function builder(){'),html.indexOf('let otPresetEditorId=',html.indexOf('function builder(){'))>0?html.indexOf('function otTurnModel(',html.indexOf('function builder(){')):html.indexOf('function otTriggerColor(',html.indexOf('function builder(){')));
assert(builder.includes('ot-chat-trigger-delete'),'Each custom trigger must expose a row-level remove icon');
assert(builder.includes("kind==='trigger-custom'"),'Built-in road-lighting rows must not be removable');
assert(builder.includes('otPendingTriggerDelete===t'),'Custom deletion must confirm inline');
assert(!builder.includes('Delete trigger</button>'),'Do not duplicate Remove in the separate Trigger Settings editor');
assert(html.includes('function otCancelTriggerDelete()'),'Inline Cancel action must exist');
console.log('Custom-row-only remove icon and inline confirmation protected');

// Regression: preserve precisely one visible global nav in browser and Android.
const standaloneNav=html.slice(html.indexOf('function otConceptNav(){'),html.indexOf('function otChatAutoSet('));
const labels=[...standaloneNav.matchAll(/\['(?:home|scenes|layout|builder|events|device)','([^']+)'/g)].map(x=>x[1]);
assert.deepEqual(labels,['Home','Show Modes','Layout','Triggers','Auto Events','Settings'],'Standalone tabs must follow approved six-item order');
assert(!html.includes('<nav class="bottomnav">'),'Legacy bottom nav must not render');
assert(html.includes('#app nav.tabs,#app nav.bottomnav{display:none!important}'),'Hidden legacy route mirror must stay hidden');
assert(html.includes('scroll-snap-type:x proximity'),'Small screens use the same scrollable top segmented nav');
assert(shell.includes('#app .ot-chat-nav{display:none!important}'),'Android native navigation must suppress bundled duplicate');
console.log('OpenTail single-navigation regression checks passed');
