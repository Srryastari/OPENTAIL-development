// Regression: disconnected Triggers tab uses the same local preview engine as Show Modes.
const assert=require('node:assert/strict');
const fs=require('node:fs');
const html=fs.readFileSync('web-studio/opentail.htm','utf8');
const a=html.indexOf('let otTriggerPending=null,otTriggerTestOpen=false;');
const b=html.indexOf('function builder(){',a);
assert(a>0&&b>a,'Missing offline trigger testing adapter');
const code=html.slice(a,b);
function setup(mode='wled',connected=false){
 const state={connected,benchSignals:{},triggerStatus:''};
 const otSim={active:false,source:'effect',triggers:[]};
 const OT_SIM_TRIGGERS=['Park','Brake','Left Turn','Right Turn','Hazards','Reverse'];
 const events={hardware:0,sim:0,renders:0};
 const mockSim=(name)=>{events.sim++;otSim.triggers=otSim.triggers.includes(name)?otSim.triggers.filter(t=>t!==name):[...otSim.triggers,name];otSim.active=otSim.triggers.length>0;otSim.source='trigger';events.renders++};
 const zones=[
 {id:'ot-example-left-tail',name:'Left Tail · Demo',start:0,length:24},
 {id:'ot-example-left-turn',name:'Left Turn · Demo',start:24,length:24},
 {id:'ot-example-right-tail',name:'Right Tail · Demo',start:48,length:24}
 ];
 const scope=new Function('otControllerMode','state','otSim','OT_SIM_TRIGGERS','otBenchUncertain','toggleTrigger','otSimStop','otSimTrigger','otSimPreviewZones','esc','render',
 code+';return {offline:otTriggerTestOffline,active:otTriggerTestActive,toggle:otTriggerTestToggle,preview:otTriggerTestPreview,isOpen:()=>otTriggerTestOpen};');
 const api=scope(mode,state,otSim,OT_SIM_TRIGGERS,false,()=>{events.hardware++;return true;},()=>{otSim.triggers=[];otSim.active=false;otSim.source='effect';},mockSim,()=>zones,x=>String(x),()=>{events.renders++});
 return {state,otSim,events,api};
}
const offline=setup();
assert.equal(offline.api.offline(),true);
assert.equal(offline.api.toggle('Brake'),true);
assert.equal(offline.events.hardware,0,'Disconnected test must never send a controller command');
assert.equal(offline.api.isOpen(),true,'Testing section should stay expanded after toggling');
assert.equal(offline.api.active('Brake'),true);
assert.match(offline.state.triggerStatus,/Local preview only/);
assert.match(offline.api.preview(),/SIMULATED · no vehicle commands/);
assert.equal((offline.api.preview().match(/data-ot-led-index=/g)||[]).length,48,'Three demo zones should display 16 pixels each');
offline.api.toggle('Hazards');
assert.deepEqual(offline.otSim.triggers,['Brake','Hazards']);
offline.api.toggle('Brake');
assert.deepEqual(offline.otSim.triggers,['Hazards']);
offline.api.toggle('Hazards');
assert.equal(offline.otSim.active,false);
assert.equal(offline.api.preview(),'','Stopped simulation must hide the inline LED strip');
assert.equal(offline.api.toggle('Custom Welcome'),false);
assert.equal(offline.events.hardware,0);
assert.match(offline.state.triggerStatus,/standard lighting/);
const core=setup('core',true);
assert.equal(core.api.offline(),true,'CORE connection does not turn a software preview into a WLED test');
core.api.toggle('Left Turn');
assert.equal(core.events.hardware,0);
const online=setup('wled',true);
assert.equal(online.api.offline(),false);
assert.equal(online.api.toggle('Brake') instanceof Promise,true);
assert.equal(online.events.hardware,1,'Connected WLED must still route via its original bench test');
assert.equal(online.events.sim,0);
console.log('Offline Triggers compact preview, simultaneous inputs, safe transport separation and test lifecycle passed');
