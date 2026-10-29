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
console.log('Studio v0.19.119: design-only CORE routes, preserved authoring, offline events, project backup, old-device fallback and approved UI passed.');
