// Studio now owns authoring only; CORE /dev owns all physical bench controls.
// This tests actual route behavior with safe stand-ins rather than trusting
// source comments or effect names. No browser, BLE or GPIO is exercised.
const assert=require('node:assert/strict');
const fs=require('node:fs');
const src=fs.readFileSync('web-studio/opentail.htm','utf8');
assert(src.includes("const OT_VERSION='0.19.119'"));
assert(src.includes("otCoreStoredValue('ot-controller-mode-v1','core')"),
  'New standalone Studio installs should open the CORE design workspace');
const i=src.indexOf('function otStudioRoute(tab){');
const j=src.indexOf('function device(){',i);
assert(i>0 && j>i,'Authoring router boundaries are missing');
const routeText=src.slice(i,j);
assert(!routeText.includes('otCoreQuickTest(') && !routeText.includes('otCoreDiagnostics') &&
       !routeText.includes('otOutputManagerCard(') &&
       !routeText.includes('otTriggerManagerCard(') &&
       !routeText.includes('otCoreWorkspaceBanner('),
  'CORE routes must not render duplicate developer or hardware controls');
const factory=new Function(
  'otConceptActive','otChatAutomaticEvents','otControllerMode',
  'otCoreHome','otCoreDevice','otAppearanceCard',
  'scenes','builder','layout','home','device','diagnostics',
  routeText+';return otStudioRoute;'
);
const make=(mode='core',concept='normal')=>factory(
  concept,()=>'<event-author/>',mode,
  ()=>'<project-home/>',()=>'<project-backup/>',()=>'<appearance/>',
  ()=>'<effect-editor/>',()=>'<trigger-editor/>',()=>'<layout-editor/>',
  ()=>'<legacy-home/>',()=>'<legacy-settings/>',()=>'<legacy-diag/>'
);
const core=make();
assert.equal(core('home'),'<project-home/>');
assert.equal(core('scenes'),'<effect-editor/>');
assert.equal(core('layout'),'<layout-editor/>');
assert.equal(core('builder'),'<trigger-editor/>');
assert.equal(core('device'),'<div class="ot-settings-page"><project-backup/><appearance/></div>');
assert(core('diagnostics').includes('http://192.168.4.1/dev'));
assert(!core('diagnostics').includes('otCoreInputCard'));
assert.equal(make('core','events')('builder'),'<event-author/>');
assert.equal(core('unexpected'),'<project-home/>');
const legacy=make('wled');
assert.equal(legacy('scenes'),'<effect-editor/>');
assert.equal(legacy('home'),'<legacy-home/>');

const homeStart=src.indexOf('function otCoreHome(){'),homeEnd=src.indexOf('function otCoreUnsupported(',homeStart);
const homeSrc=src.slice(homeStart,homeEnd);
for(const required of ['state.projectName','otSequence.length','state.layout','otPixelPreview()',"go('scenes')"]){
  assert(homeSrc.includes(required),'Designer Home lost: '+required);
}
for(const forbidden of ['otCoreQuickTest()','otCoreLiveCard()','otCoreSignalActions()','otCoreDemoActions()']){
  assert(!homeSrc.includes(forbidden),'Home still includes physical developer controls: '+forbidden);
}
const settingsStart=src.indexOf('function otCoreDevice(){');
const settingsEnd=src.indexOf('// In CORE mode, retain the existing Studio editor',settingsStart);
const settings=src.slice(settingsStart,settingsEnd);
assert(settings.includes('exportProject()')&&settings.includes('importProject(this.files[0])'),'Local project backups must work');
assert(settings.includes('otCoreConnect()')&&settings.includes('otCoreDisconnect()'),'Optional CORE connection must be preserved');
assert(settings.includes('http://192.168.4.1/dev'),'Hardware testing must link to CORE dashboard');
for(const forbidden of ['otCoreControllerPicker()','otCoreSignalActions()','otCoreDemoActions()','otCoreLiveCard()','otOutputManagerCard()']){
  assert(!settings.includes(forbidden),'CORE hardware tools were duplicated in Studio Settings: '+forbidden);
}
for(const fn of ['function scenes(){','function builder(){','function otChatAutomaticEvents(){',
                 'function otBaseArrangementLayout()','function exportProject(){','async function importProject(']){
  assert(src.includes(fn),'Approved designer functionality missing: '+fn);
}
const nav=src.slice(src.indexOf('function otConceptNav(){'),src.indexOf('function otChatPreviewFade('));
const labels=[...nav.matchAll(/\['(?:home|scenes|layout|builder|events|device)','([^']+)'/g)].map(x=>x[1]);
assert.deepEqual(labels,['Home','Show Modes','Layout','Triggers','Auto Events','Settings']);
assert(src.includes('id="ot-approved-chat-01956"')&&src.includes('id="ot-approved-chat-trigger-01956"'));
// An offline project must browse all 120 known 1D effects plus six original
// OpenTail effects without contacting CORE or linking firmware binaries.
const manifestStart='const OT_STUDIO_DESIGN_EFFECTS=Object.freeze(';
const manifestAt=src.indexOf(manifestStart);
const manifestEnd=src.indexOf(');',manifestAt);
assert(manifestAt>0&&manifestEnd>manifestAt,'Offline CORE effect manifest missing');
const manifests=JSON.parse(src.slice(manifestAt+manifestStart.length,manifestEnd));
const originalsStart='const OT_STUDIO_ORIGINAL_EFFECTS=Object.freeze(';
const originalAt=src.indexOf(originalsStart),originalEnd=src.indexOf(');',originalAt);
const originals=JSON.parse(src.slice(originalAt+originalsStart.length,originalEnd));
assert.equal(manifests.length,120);
assert.equal(new Set(manifests.map(x=>x[0])).size,120);
assert.equal(Object.keys(originals).length,6);
assert.equal(originals[32769],'Materialize Sparks');
assert.equal(originals[32773],'Smooth Fade Out');
const upstream=fs.readFileSync('core-firmware/main/wled_1d.c','utf8');
const coreModes=[...upstream.matchAll(/\{(\d+),"[^"]+","([^"]+)",\d+\}/g)].map(m=>[Number(m[1]),m[2]]);
assert.deepEqual(manifests,coreModes,'Studio must derive names/IDs from CORE, without duplicating the renderer');
const choicesStart=src.indexOf('function otCompatibleEffectChoices(){');
const choicesEnd=src.indexOf('function otCanonicalEffect(',choicesStart);
const choose=new Function('otBleFxMode','otBleEffectNames','otControllerMode',
  'otStudioEnsureDesignCatalog','OT_STUDIO_DESIGN_EFFECTS','OT_STUDIO_ORIGINAL_EFFECTS',
  'state','otEffectCompatible',src.slice(choicesStart,choicesEnd)+';return otCompatibleEffectChoices;')(
    ()=>false,()=>[], 'core',()=>{},manifests,originals,{wledEffects:[]},()=>true);
const catalog=choose();
assert.equal(catalog.length,126);
assert(catalog.some(x=>x.id===219&&x.name==='Slow Transition'));
assert(catalog.some(x=>x.id===32768&&x.name==='Embers'));
assert(catalog.some(x=>x.id===32770&&x.name==='Random Materialize'));
const selectStart=src.indexOf('async function applyWledEffect(id){');
const selectEnd=src.indexOf('// Bench tests are color overrides',selectStart);
assert(selectStart>0&&selectEnd>selectStart,'Standalone effect selection source missing');
let selected=null,preview='',persisted=0,rendered=0;
const editingState={wledEdit:null,status:'',runningFx:null,runningSectionFx:null};
const selectFn=new Function('otBleFxMode','otBleApplyEffect','otControllerMode',
 'otStudioFxName','state','persist','render','otSimPreviewEffect','otChosenEffectId',
 src.slice(selectStart,selectEnd)+';return {apply:applyWledEffect,selected:()=>otChosenEffectId,preview:()=>otSimPreviewEffect};')(
 ()=>false,()=>{throw Error('BLE command must never be invoked in offline Studio')},
 'core',id=>originals[id]||manifests.find(x=>x[0]===id)?.[1]||null,editingState,
 ()=>{persisted++},()=>{rendered++},preview,selected);
(async()=>{
 assert.equal(await selectFn.apply(32770),true);
 assert.equal(selectFn.selected(),32770);
 assert.equal(selectFn.preview(),'Random Materialize');
 assert.equal(editingState.runningFx,null,'Offline selection cannot claim active hardware output');
 assert(editingState.status.includes('Not sent to CORE.'));
 assert.equal(await selectFn.apply(9999),false,'Unknown effects must be rejected without device access');
 assert(persisted>0&&rendered>0,'Offline editor state must visibly update and save');
 console.log('Offline 126-mode data catalog and source-level non-device effect selection passed.');
})().catch(e=>{console.error(e);process.exitCode=1;});

console.log('Studio v0.19.119: design-only CORE routes, preserved authoring, offline events, project backup, old-device fallback and approved UI passed.');
