'use strict';
const fs=require('fs');
const vm=require('vm');
const assert=require('node:assert/strict');
const code=fs.readFileSync('android/app/src/main/assets/ble-show-panel.js','utf8');
const sent=[];
let connected=true, authorized=true,core=true;
const doc={getElementById(){return null}};
const native={
 getState(){return JSON.stringify({connected,authorized})},
 readEffectCapabilities(){sent.push(['caps'])},
 startEffectId(...args){sent.push(['fx',...args])},
 stop(){sent.push(['stop'])}
};
const s={connected:false,status:'',wledEffects:[],wledEdit:{fx:3,sx:80,ix:140,bri:160,color:'#ff0000',background:'#000000'}};
const context={window:{OpenTailNativeBLE:native},document:doc,state:s,
  setTimeout,clearTimeout,otControllerMode:'core',
  otEffectSectionId:'__all__',otChosenEffectId:null,
  otUE:null,render(){},statusLine(){},
  scenes(){return '<div><h2>Effects</h2><select aria-label="Target for WLED effects"><option value="__all__">All</option></select><details class="card ot-core-advanced ot-core-native-show"><summary>duplicate</summary></details></div>'},
  applyWledEffect(){throw Error('WLED Wi-Fi selected')},
  otCompatibleEffectChoices(){throw Error('WLED catalog selected')},
  otEffectCompatible(){return false},otEffectMeta(){return null},
  otSelectEffectSection(){throw Error('Wi-Fi zone')},otReadEffectSection(){throw Error('Wi-Fi read')},
  setWledEdit(){throw Error('Wi-Fi write')},otStopShow(){throw Error('Wi-Fi stop')},
  async otUEApply(){throw Error('Wi-Fi FX editor')}
};
context.window.scenes=context.scenes;
for(const name of ['applyWledEffect','otCompatibleEffectChoices','otEffectCompatible',
 'otEffectMeta','otSelectEffectSection','otReadEffectSection','setWledEdit',
 'otStopShow','otUEApply'])context.window[name]=context[name];
vm.createContext(context);
vm.runInContext(code,context,{filename:'ble-show-panel.js'});
assert.equal(sent[0][0],'caps','BLE capabilities must be requested');
const api=context.window.OpenTailBLEEffects;
api.capabilities([0,1,3,6,12,23,999,3]);
assert.deepEqual(Array.from(api.choices(),x=>x.id),[0,1,3,6,12,23]);
assert.equal(api.select(999),false,'Unimplemented FX must be rejected');
assert.equal(api.select(3),true,'Confirmed Wipe should send a BLE FX command');
const fx=sent.at(-1);
assert.deepEqual(Array.from(fx.slice(0,4)),['fx',3,3,255]);
assert.equal(fx[4],255);assert.equal(fx[5],0);assert.equal(fx[6],0);
assert.equal(fx[10],Math.round(160*32/255));
assert.equal(fx[13],0,'Unsupported nondefault palettes must not be sent');
assert.equal(fx[14],60,'Temporary bench playback TTL is 60s');
assert.equal(context.window.otCompatibleEffectChoices().length,6);
assert.equal(context.window.otEffectMeta(3).palette,false);
const html=context.window.scenes();
assert.match(html,/CORE effects run locally/);
assert(!html.includes('duplicate'),'No duplicate CORE effects card allowed');
assert(!html.includes('ot-ble-inline'),'No second BLE effects picker allowed');
context.window.otSelectEffectSection('core-1-2');
assert.equal(api.select(12),true);
const zone=sent.at(-1);
assert.equal(zone[2],2,'CORE output 2 only');assert.equal(zone[3],4,'CORE zone 3 only');
context.window.otStopShow();
assert.equal(sent.at(-1)[0],'stop');
authorized=false;
assert.equal(api.select(3),false,'Unauthorized BLE must not execute');
console.log('Unified BLE effects editor: single library, capability gating, output zone, auth and stop PASS');
