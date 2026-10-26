// Regression: no CORE/WLED connection and no saved zones must still show animated trigger examples.
const assert=require('node:assert/strict');
const fs=require('node:fs');
const html=fs.readFileSync('web-studio/opentail.htm','utf8');
function section(from,to){
 const a=html.indexOf(from),b=html.indexOf(to,a+from.length);
 assert(a>=0&&b>a,'Missing source section '+from);
 return html.slice(a,b);
}
const source=section('const OT_SIM_EXAMPLE_ZONES=', 'function otSimElapsed()')+
 '\n'+section('function otPixelPreview()', 'function home()');
assert(html.includes('if(sim){const z=otSimPreviewZones().find('),
 'Simulation paint path must resolve rendered (including demo) zones');
assert(html.includes("const demo=otSim.active&&zones===OT_SIM_EXAMPLE_ZONES"),
 'Offline demo must have its own pixel capacity');
const makeFactory=new Function('otSim','state','otPreviewConfiguredZones','otUE','otSimDraft',
 'DEFAULT_PROFILE','otControllerMode','otOMData','otPreviewTabs','otSimControls',
 'esc','otSimStatus','otPeekStatusText','otChosenEffectId',
 source+';return {zones:otSimPreviewZones,example:otSimTriggerExampleMode,effect:otSimEffectForZone,html:otPixelPreview};');
const profiles=()=>({
 Brake:{zones:[],brightness:85,color:'#ff0000',pattern:'solid'},
 Park:{zones:[],brightness:35,color:'#ff0000',pattern:'solid'},
 'Left Turn':{zones:[],brightness:85,color:'#ffbf00',pattern:'solid'},
 'Right Turn':{zones:[],brightness:85,color:'#ffbf00',pattern:'solid'},
 Hazards:{zones:[],brightness:85,color:'#ffbf00',pattern:'solid'},
 Reverse:{zones:[],brightness:100,color:'#ffffff',pattern:'solid'}
});
function prepare(triggers,configured=[],p=profiles()){
 const sim={active:triggers.length>0,playing:true,triggers,source:'trigger'};
 const state={profiles:p,wledEdit:{},layout:[],connected:false,wledInfo:{}};
 const before=JSON.stringify(state);
 const factory=makeFactory(sim,state,()=>configured,null,x=>({
  fx:x.pattern||'solid',fg:x.color||'#ff0000',bg:'#000000',accent:'#ffffff',
  brightness:x.brightness/100,speed:.5,intensity:.5
 }),()=>({zones:[],brightness:85,color:'#ff0000',pattern:'solid'}),
 'core',null,()=>'',()=>'',v=>String(v),()=>'',()=>'',null);
 return {sim,state,before,...factory};
}
const single=prepare(['Brake']);
const sample=single.zones();
assert.equal(sample.length,6,'Must render six standalone, labeled, physical-output-independent demo zones');
assert.deepEqual(sample.map(z=>z.otDemoRole),['tail','turn','reverse','tail','turn','reverse']);
assert.equal(single.example(),true,'Empty assignments must use local examples');
const markup=single.html();
assert.equal((markup.match(/class="ot-preview-row ot-static-preview"/g)||[]).length,6);
assert.equal((markup.match(/class="ot-led"/g)||[]).length,144);
assert(markup.includes('DEMO OUTPUTS'),'Clear simulated-only badge required');
assert.equal(JSON.stringify(single.state),single.before,'Demo must not mutate saved project');
function lit(trigger,z){return trigger.effect(z).brightness>0}
assert.deepEqual(sample.map(z=>lit(single,z)),[true,false,false,true,false,false],
 'Brake must illuminate tail examples only');
const left=prepare(['Left Turn']);
assert.deepEqual(left.zones().map(z=>lit(left,z)),[false,true,false,false,false,false]);
const right=prepare(['Right Turn']);
assert.deepEqual(right.zones().map(z=>lit(right,z)),[false,false,false,false,true,false]);
const hazard=prepare(['Hazards']);
assert.deepEqual(hazard.zones().map(z=>lit(hazard,z)),[false,true,false,false,true,false]);
const reverse=prepare(['Reverse']);
assert.deepEqual(reverse.zones().map(z=>lit(reverse,z)),[false,false,true,false,false,true]);
const combined=prepare(['Brake','Hazards']);
assert.deepEqual(combined.zones().map(z=>combined.effect(z).fg),
 ['#ff0000','#ffbf00','#000000','#ff0000','#ffbf00','#000000'],
 'Braking and turn examples must retain different functions simultaneously');
const real=[{id:'zoneA',name:'Real zone',start:0,length:30}];
const p=profiles();p.Brake.zones=['zoneA'];
const assigned=prepare(['Brake'],real,p);
assert.equal(assigned.zones(),real,'Configured zones must not be replaced by demos');
assert.equal(assigned.example(),false,'Existing assignments must be honored');
assert(!assigned.html().includes('DEMO OUTPUTS'));
assert.equal(assigned.effect(real[0]).brightness,.85);
// CORE role routing remains independent from the temporary six-zone demo geometry.
const coreRoles=[
 {id:'core-0-0',name:'Tail',start:0,length:20,otCoreRole:'park_brake',otCoreOutput:0},
 {id:'core-0-1',name:'Left',start:20,length:20,otCoreRole:'left_turn',otCoreOutput:0},
 {id:'core-1-0',name:'Right',start:40,length:20,otCoreRole:'right_turn',otCoreOutput:1},
 {id:'core-1-1',name:'Reverse',start:60,length:20,otCoreRole:'reverse',otCoreOutput:1}
];
const coreBrake=prepare(['Brake'],coreRoles);
assert.equal(coreBrake.zones(),coreRoles,'Connected CORE zone metadata should be retained');
assert.deepEqual(coreRoles.map(z=>lit(coreBrake,z)),[true,false,false,false]);
const coreHazards=prepare(['Hazards'],coreRoles);
assert.deepEqual(coreRoles.map(z=>lit(coreHazards,z)),[false,true,true,false]);
const coreCombined=prepare(['Brake','Hazards'],coreRoles);
assert.deepEqual(coreRoles.map(z=>coreCombined.effect(z).fg),['#ff0000','#ffbf00','#ffbf00','#000000']);
const stopped=prepare([]);
assert.equal(stopped.zones().length,0,'Never display fake zones outside simulation');
assert.equal(stopped.example(),false);
console.log('Offline demo markup, role routing, combination, state isolation and configured-zone tests passed');
