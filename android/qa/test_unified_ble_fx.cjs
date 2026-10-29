'use strict';
/* v0.1.30: Android's approved UI IS the packaged Studio v0.19.120.
 * No BLE/firmware connection or second Android shell is required to design.
 * Native BLE/GATT and JSON export bridges remain available, but must not
 * receive commands from the ordinary authoring paths.
 */
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const html=fs.readFileSync('web-studio/opentail.htm','utf8');
const androidHtml=fs.readFileSync('android/app/src/main/assets/opentail.htm','utf8');
const activity=fs.readFileSync('android/app/src/main/java/com/opentail/android/MainActivity.java','utf8');
assert.equal(androidHtml,html,'Android must render the same HTML/styles as the approved web UI');
assert(html.includes("const OT_VERSION='0.19.120'"));
assert(activity.includes('web.loadUrl(OFFLINE_URL)'),'Native app must load bundled HTML');
assert(activity.includes('WebViewAssetLoader'),'Native app must continue to use trusted local assets');
const pageFinished=activity.slice(activity.indexOf('onPageFinished(WebView view'),activity.indexOf('onReceivedError(',activity.indexOf('onPageFinished(WebView view')));
for(const injected of ['injectUnifiedShell(view)', 'injectDirectBleDashboard(view)',
                       'injectBleOutputControls(view)','injectOutputControlMotion(view)']){
  assert(!pageFinished.includes(injected),'Android must not overlay another UI: '+injected);
}
for(const preserved of ['addJavascriptInterface(new NativeBleBridge(), "OpenTailNativeBLE")',
                        'addJavascriptInterface(new ProjectExportBridge(), "OpenTailApp")',
                        'addJavascriptInterface(new HapticBridge(), "OpenTailHaptics")',
                        'startEffectId(int effectId','private boolean bridgePageAllowed()']){
  assert(activity.includes(preserved),'Native bridge regression: '+preserved);
}
assert(activity.includes('root.setBackgroundColor(Color.BLACK);'));
assert(activity.includes('web.setBackgroundColor(Color.BLACK);'));
assert(!activity.slice(activity.indexOf('protected void onResume()'),activity.indexOf('protected void onStop()')).includes('connectCore(true)'));
assert(!activity.slice(activity.indexOf('protected void onResume()'),activity.indexOf('protected void onStop()')).includes('ble.tryReconnect('));
const bleAt=html.indexOf('function otBleFxMode(){');
const bleEnd=html.indexOf('function otBleStatus(){',bleAt);
const isMode=new Function('window','otControllerMode',html.slice(bleAt,bleEnd)+';return otBleFxMode;')(
 {OpenTailNativeBLE:{getState:()=>JSON.stringify({connected:true,authorized:true})}},'core');
assert.equal(isMode(),false,'Design mode must not dispatch live BLE commands when an authenticated native bridge exists');
const selectAt=html.indexOf('async function applyWledEffect(id){'),selectEnd=html.indexOf('// Bench tests are color overrides',selectAt);
assert(selectAt>0&&selectEnd>selectAt);
const state={wledEdit:{},runningFx:null,runningSectionFx:null,status:''};
let selected=null,sent=0;
const select=new Function('otBleFxMode','otBleApplyEffect','otControllerMode','otStudioFxName','state',
                        'persist','render','otSimPreviewEffect','otChosenEffectId',
                        html.slice(selectAt,selectEnd)+';return {apply:applyWledEffect,selected:()=>otChosenEffectId};')(
 ()=>false,()=>{sent++;throw Error('Hardware command leaked into Studio')},'core',
 id=>id===32769?'Materialize Sparks':id===3?'Wipe':null,state,()=>true,()=>{},'',selected);
(async()=>{
  assert.equal(await select.apply(32769),true);
  assert.equal(select.selected(),32769);
  assert.equal(state.runningFx,null);
  assert.equal(sent,0,'Offline design selection cannot send BLE/physical commands');
  assert(state.status.includes('Not sent to CORE.'));
  assert.equal(await select.apply(9999),false);
  assert.equal(sent,0);
  const nav=html.slice(html.indexOf('function otConceptNav(){'),html.indexOf('function otChatPreviewFade('));
  for(const label of ['Home','Show Modes','Layout','Triggers','Auto Events','Settings'])
    assert(nav.includes("'"+label+"'"),'Approved navigation missing '+label);
  const route=html.slice(html.indexOf('function otStudioRoute(tab){'),html.indexOf('function device(){',html.indexOf('function otStudioRoute(tab){')));
  assert(route.includes('scenes,')&&route.includes('layout,')&&route.includes('builder,'));
  const settings=html.slice(html.indexOf('function otCoreDevice(){'),html.indexOf('// In CORE mode, retain the existing Studio editor'));
  assert(!settings.includes('otCoreConnect()')&&!settings.includes('otCoreDisconnect()'));
  console.log('Android/web UI parity, six-page styling, preserved native bridges and no-command authoring passed');
})().catch(err=>{console.error(err);process.exitCode=1});
