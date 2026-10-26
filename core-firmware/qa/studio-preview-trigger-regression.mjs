import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const dir=process.argv[2];
const html=fs.readFileSync(`${dir}/opentail-v0.19.51.htm`,'utf8');
const extract=(a,b)=>{const st=html.indexOf(a),end=html.indexOf(b,st);assert(st>0&&end>st,`Extract ${a}`);return html.slice(st,end)};
const engine=extract('const OT_SIM_TRIGGERS=', 'function otSimStatus()');
const trigger=extract('function otSimTrigger(name){','function otSimPause()');
const adapter=extract('function otPreviewConfiguredZones(){','function otPixelPreview(){');
const outputs=[
 {pixels:10,zones:[
  {name:'Tail',pixels:2,role:'park_brake'},
  {name:'Turn L',pixels:2,role:'left_turn'},
  {name:'Reverse',pixels:2,role:'reverse'},
  {name:'Brake only',pixels:2,role:'brake'},
  {name:'Decoration',pixels:2,role:'decorative'}]},
 {pixels:8,zones:[
  {name:'Right turn',pixels:4,role:'right_turn'},
  {name:'Park only',pixels:2,role:'parking'},
  {name:'Off',pixels:2,role:'off'}]}
];
const ctx={
 console,performance:{now:()=>0},cancelAnimationFrame(){},requestAnimationFrame(){},
 state:{layout:[{id:'legacy',name:'Old WLED',length:1,start:0}],profiles:{Brake:{zones:['legacy'],color:'#ff0000',brightness:100}},wledEffects:[],wledEdit:{}},
 otControllerMode:'core',otOMData:{outputs},otUE:null,otSequence:[],otChosenEffectId:0,
 otPreviewSynced:false,render(){},otSimAnimate(){},
 hexRgb(s){return [parseInt(s.slice(1,3),16),parseInt(s.slice(3,5),16),parseInt(s.slice(5,7),16)]},
};
vm.createContext(ctx);vm.runInContext(engine+'\n'+trigger+'\n'+adapter,ctx);
const zones=()=>Array.from(vm.runInContext('otPreviewConfiguredZones()',ctx));
const byRole=(role)=>zones().find(z=>z.otCoreRole===role);
const run=(code)=>vm.runInContext(code,ctx);
const rgb=(role,i=0,t=0)=>{ctx.zoneUnderTest=byRole(role);return run(`otSimColor(otSimEffectForZone(zoneUnderTest),${i},12,${t})`)};
const t=(name)=>{ctx.inputName=name;run('otSimTrigger(inputName)')};
assert.equal(zones().length,8,'Read saved device zones');
assert.equal(zones()[0].otCoreRole,'park_brake','Roles carried through preview');
assert(zones().every(z=>z.id.startsWith('core-')),'No stale WLED IDs');
t('Park');
assert.notEqual(rgb('park_brake'),'#000000');
assert.notEqual(rgb('parking'),'#000000');
assert.equal(rgb('brake'),'#000000');
assert.equal(rgb('decorative'),'#000000');
const dim=rgb('park_brake');
t('Brake');
const bright=rgb('park_brake');
assert.notEqual(dim,bright,'Brake overrides park on shared zone');
assert.notEqual(rgb('brake'),'#000000','Brake-only lights');
t('Park');t('Brake');
assert.equal(run('otSim.active'),false,'Toggling last role stops the local simulation');
t('Left Turn');
assert.notEqual(rgb('left_turn',0,0),'#000000','Left turn lights amber');
assert.equal(rgb('left_turn',0,600),'#000000','Left turn pulses on local simulation timeline');
assert.equal(rgb('right_turn',0,0),'#000000','Left input must not light right zone');
t('Left Turn');t('Right Turn');
assert.notEqual(rgb('right_turn',0,0),'#000000','Right turn independently lights');
t('Right Turn');t('Hazards');
assert.notEqual(rgb('left_turn',0,0),'#000000','Hazards reach left saved role');
assert.notEqual(rgb('right_turn',0,0),'#000000','Hazards reach right saved role');
assert.equal(rgb('reverse',0,0),'#000000','Hazards do not turn on reverse');
t('Hazards');t('Reverse');
assert.equal(rgb('reverse',0,0),'#d9d9d9','Reverse white local visualization');
assert.equal(rgb('off',0,0),'#000000','Disabled roles not triggered');
assert.equal(rgb('decorative',0,0),'#000000','Show-only roles not trigger assigned');
t('Reverse');
assert.equal(run('otSim.active'),false,'Toggling last signal stops preview');
ctx.otControllerMode='wled';
assert.equal(zones()[0].id,'legacy','Existing WLED section data untouched');
t('Brake');
assert.notEqual(run('otSimEffectForZone(state.layout[0]).brightness'),0,'WLED uses its legacy profile path');
assert.equal(run('otSimEffectForZone({id:"core-0-0",name:"not assigned"}).brightness'),0,'WLED role-free unassigned remains off');
console.log('PASS: CORE local triggers use saved output roles, brake/park priority, side-specific blinking/hazards, reverse, isolated decorative, toggling, WLED unaffected.');
