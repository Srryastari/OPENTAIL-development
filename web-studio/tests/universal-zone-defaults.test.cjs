// Regression: OpenTail configures the vehicle; it never installs a GS400 map
// when a user opens, adds, clears or saves a generic output profile.
const assert=require('node:assert/strict');
const fs=require('node:fs');
const source=fs.readFileSync('web-studio/opentail.htm','utf8');
const release=fs.readFileSync('web-studio/opentail-v0.19.118.htm','utf8');
const android=fs.readFileSync('android/app/src/main/assets/opentail.htm','utf8');
assert.equal(source,release,'Stable and versioned Studio files must match');
assert.equal(source,android,'Android must bundle the same universal UI');

assert(!source.includes('function otOMGS400Main('),'No vehicle-specific CORE mapping loader');
assert(!source.includes('function otOMExample('),'No five-zone CORE factory template');
assert(!source.includes('function otGs400Example('),'No vehicle-specific project generator');
assert(!source.includes('function otLoadExampleLayout('),'No vehicle-specific project generator entry');
assert(!source.includes('GS400 main housing · 522'),'Vehicle template button must be gone');
assert(!source.includes('Reset draft to example'),'Five-zone template button must be gone');
assert(source.includes("o.zones.push({name:`Zone ${o.zones.length+1}`,pixels:0,role:'off'})"),
  'New zones are unassigned, not automatically decorative or vehicle triggers');

const start=source.indexOf('function otOMResetDraft(){');
const end=source.indexOf('async function otOMSave(){',start);
assert(start>=0&&end>start,'Neutral Output Manager draft reset must exist');
const fn=source.slice(start,end);
const create=new Function('initial','approve',`
 let otOMDraft=structuredClone(initial),otOMBusy=false,otOMSelected=1;
 let otOMSaveState='saved',otOMMessage='',renderCalls=0;
 const confirm=()=>approve();
 const render=()=>{renderCalls++};
 ${fn}
 return {run:otOMResetDraft,read:()=>({draft:otOMDraft,saveState:otOMSaveState,
  message:otOMMessage,renderCalls})};
`);
const init=[
 {name:'Output 1',pixels:12,color_order:'RGB',zones:[{name:'Custom A',pixels:12,role:'brake'}]},
 {name:'User-defined Output 2',pixels:263,color_order:'GRB',
  calibration:{rgb_gain:[240,250,255],white_gain:[255,220,230],gamma_enabled:false},
  zones:[{name:'Decorative Segment',pixels:123,role:'decorative'},
   {name:'Turn Strip',pixels:140,role:'right_turn'}]}
];
const denied=create(init,()=>false);
denied.run();
assert.deepEqual(denied.read().draft,init,'Cancel must not mutate the draft');
assert.equal(denied.read().renderCalls,0);
const accepted=create(init,()=>true);
accepted.run();
const state=accepted.read();
assert.deepEqual(state.draft[0],init[0],'Other outputs are independent');
assert.equal(state.draft[1].name,'Pixel OUT 2');
assert.equal(state.draft[1].pixels,1,'Reset is an inert one-pixel placeholder');
assert.deepEqual(state.draft[1].zones,[{name:'Zone 1',pixels:1,role:'off'}]);
assert.equal(state.draft[1].color_order,'GRB','No silent color-order regression');
assert.deepEqual(state.draft[1].calibration,init[1].calibration,'No calibration regression');
assert.equal(state.saveState,'unsaved','Never claim profile was saved');
assert.match(state.message,/before saving to CORE/);
assert.equal(state.renderCalls,1);
assert.deepEqual(init[1].zones.map(z=>z.role),['decorative','right_turn'],
  'Existing source profile must remain untouched');
console.log('Universal zones: no vehicle-specific defaults, neutral drafts, and nonmutating cancel passed');
