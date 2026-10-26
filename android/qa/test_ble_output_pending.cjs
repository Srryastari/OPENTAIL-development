'use strict';
const fs=require('node:fs');
const vm=require('node:vm');
const assert=require('node:assert/strict');
const source=fs.readFileSync('android/app/src/main/assets/ble-output-panel.js','utf8');
const eventHandlers={};
const calls=[];
const card={innerHTML:''};
const bluetooth={radioOn:true,connected:true,authorized:true,phase:'authorized'};
const native={
  getState:()=>JSON.stringify(bluetooth),
  readOutput:index=>calls.push(['read',index]),
  saveOutput:(index,config)=>calls.push(['save',index,config]),
};
const doc={
  head:{appendChild(){}},
  body:{},
  createElement:()=>({textContent:''}),
  getElementById:id=>id==='ot-ble-output-card'?card:null,
  addEventListener:(name,fn)=>{eventHandlers[name]=fn;},
  dispatchEvent(){},
};
const ctx={
  window:{OpenTailNativeBLE:native},
  document:doc,
  state:{tab:'layout'},
  MutationObserver:class{observe(){}},
  requestAnimationFrame:fn=>fn(),
  CustomEvent:class{},
  Date,
};
vm.createContext(ctx);
vm.runInContext(source,ctx,{filename:'ble-output-panel.js'});
const manager=ctx.window.OpenTailBLEOutputs;
assert.ok(manager,'BLE output manager installed');
function clickSelect(index){
  eventHandlers.click({target:{closest:()=>({dataset:{bleOut:'select',index:String(index)}})}});
}
function response(index,name){
  return JSON.stringify({ok:true,index,message:'Saved',output:{
    name,pixels:1,color_order:'BRG',zones:[{name:'Zone',pixels:1,role:'decorative'}]
  }});
}
manager.onResult(response(0,'GHOST-UNSOLICITED'));
clickSelect(0);
assert.equal(calls.at(-1)[1],0);
assert.ok(!card.innerHTML.includes('GHOST-UNSOLICITED'),'Unsolicited BLE config must not enter Output 1 draft');
manager.onResult(response(0,'Output One'));
assert.ok(card.innerHTML.includes('Output One'),'Expected index 0 reply must update selected output');
clickSelect(1);
assert.equal(calls.at(-1)[1],1);
manager.onResult(response(0,'CROSS-OUTPUT'));
assert.ok(!card.innerHTML.includes('CROSS-OUTPUT'),'Wrong output callback must be rejected');
manager.onResult(response(1,'LATE-RESULT'));
assert.ok(!card.innerHTML.includes('LATE-RESULT'),'Late callback after mismatch must not update output');
clickSelect(1);
manager.onResult(response(1,'Output Two'));
assert.ok(card.innerHTML.includes('Output Two'),'Expected index 1 reply must update selected output');
bluetooth.connected=false;bluetooth.authorized=false;bluetooth.phase='disconnected';
manager.connectionChanged('BLE disconnected');
manager.onResult(response(1,'POST-DISCONNECT'));
assert.ok(!card.innerHTML.includes('POST-DISCONNECT'),'Late callback after disconnect must not mutate current output');
assert.ok(card.innerHTML.includes('Output Two'),'Previously confirmed output must remain displayed');
console.log('BLE pending index and stale-response isolation: PASS (mocked WebView callbacks).');
