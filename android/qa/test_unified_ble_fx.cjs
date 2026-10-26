'use strict';
/* Exercise the native BLE transport extracted from the actual Studio source.
 * A separate Android script must not inject/override the effects editor. */
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const html=fs.readFileSync('web-studio/opentail.htm','utf8');
const activity=fs.readFileSync('android/app/src/main/java/com/opentail/android/MainActivity.java','utf8');
assert(!activity.includes('injectInlineBleControls('),'Obsolete standalone BLE FX panel is still injected');
assert(!html.includes('window.__otUnifiedBleFxV1'),'Wrapper monkey patch must not ship inside Studio');
const start=html.indexOf('const OT_CORE_FX_NAMES='),end=html.indexOf('function otCompatibleEffectChoices()',start);
assert(start>0&&end>start,'Unified BLE source must live directly inside Studio');
const fragment=html.slice(start,end);
let connected=true,authorized=true;
const calls=[],ble={
 getState:()=>JSON.stringify({connected,authorized}),
 readEffectCapabilities:()=>calls.push({cmd:'caps'}),
 startEffectId:(...args)=>calls.push({cmd:'fx',args}),
 stop:()=>calls.push({cmd:'stop'})
};
const state={status:'',wledEffects:[],wledEdit:{fx:3,color:'#ff0000',background:'#000000',bri:160,sx:80,ix:140}};
const c={window:{OpenTailNativeBLE:ble},state,otControllerMode:'core',
 otChosenEffectId:null,otEffectSectionId:'__all__',otEffectIsAll(){return c.otEffectSectionId==='__all__'},
 otPreviewConfiguredZones(){return [{id:'core-1-2',name:'Zone 3',otCoreOutput:1,otCoreZone:2}]},
 render(){},statusLine(){},setTimeout(){return 1},clearTimeout(){},console};
vm.createContext(c);vm.runInContext(fragment,c,{filename:'Studio native BLE effects'});
assert.equal(c.otBleFxMode(),true);
c.otBleRequestCapabilities();
assert.equal(calls.at(-1).cmd,'caps');
c.window.OpenTailBLEEffects.capabilities([0,1,3,6,12,23,32768,65535,3]);
assert.equal(vm.runInContext('otBleFxIds.length',c),7,'Unimplemented modes must not be listed');
assert.equal(c.otBleEffectMeta(32768).palette,true,'Embers exposes verified CORE palette subset');
assert.equal(c.otBleEffectMeta(3).palette,false,'WLED Wipe palette parity is not implemented');
assert.equal(c.otBleApplyEffect(65535),false,'Unimplemented mode must be rejected');
assert.equal(c.otBleApplyEffect(3),true);
const play=calls.at(-1);
assert.equal(play.cmd,'fx');
assert.deepEqual(play.args.slice(0,3),[3,3,255]);
assert.deepEqual(play.args.slice(3,6),[255,0,0]);
assert.equal(play.args[9],Math.round(160*32/255),'CORE brightness limited to bench cap');
assert.equal(play.args[12],0,'Only implemented palettes are enabled');
assert.equal(play.args[13],60,'Temporary playback TTL must not run forever');
c.otEffectSectionId='core-1-2';
assert.equal(c.otBleApplyEffect(32768),true,'Original particle effect uses same FX registry');
assert.deepEqual(calls.at(-1).args.slice(0,3),[32768,2,4]);
assert.equal(c.otBleApplyEffect(32768,{color:'#ff0000',background:'#0000ff',bri:160,sx:90,ix:150,pal:4}),true);
assert.equal(calls.at(-1).args[12],4,'Embers WLED-ID gradient palette must travel over BLE');
assert.equal(c.otBleStopEffect(),true);
assert.equal(calls.at(-1).cmd,'stop');
authorized=false;
assert.equal(c.otBleApplyEffect(3),false,'No unauthorized command dispatch');
connected=false;
c.otBleResult('BLE disconnected');
assert.equal(vm.runInContext('otBleFxIds.length',c),0,'Cached capabilities must clear after disconnect');
assert(!html.slice(html.indexOf('function scenes(){'),html.indexOf('let otPresetEditorId=null;')).includes('otNativeShowPanel()'),
  'No duplicate CORE Effects picker belongs in Show Modes');
console.log('Native Studio BLE transport + single Effects UI + capability/mask/auth checks passed');
