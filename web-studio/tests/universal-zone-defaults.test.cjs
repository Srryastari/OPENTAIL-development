// OpenTail is vehicle-agnostic: no GS400/default five-zone loader in the
// active CORE Output Manager. User-defined profiles and trigger roles persist.
const assert=require('node:assert/strict');
const fs=require('node:fs');
const source=fs.readFileSync('web-studio/opentail.htm','utf8');
const release=fs.readFileSync('web-studio/opentail-v0.19.120.htm','utf8');
const androidPinned=fs.readFileSync('web-studio/opentail-v0.19.120.htm','utf8');
const android=fs.readFileSync('android/app/src/main/assets/opentail.htm','utf8');
assert.equal(source,release,'Stable and versioned Studio files must match');
assert.equal(android,androidPinned,'Android must share the approved v0.19.120 UI styles and layout');
assert(source.includes("const OT_VERSION='0.19.120'"),'Web Studio version must be independent');
assert(!source.includes('function otOMGS400Main('),'No GS400 CORE mapping shortcut');
assert(!source.includes('function otOMExample('),'No five-zone CORE factory template');
assert(!source.includes('function otGs400Example('),'No GS400 project generator');
assert(!source.includes('function otLoadExampleLayout('),'No GS400 example loader');
assert(!source.includes('GS400 main housing · 522'),'No vehicle template button');
assert(!source.includes('Reset draft to example'),'No predefined five-zone button');
assert(!source.includes('otOMResetDraft'),'Do not introduce a destructive zone-reset shortcut');
assert(source.includes('LEGACY_GS400_ZONES'),
 'Historical geometry retained only to migrate previously saved projects');
assert(source.includes('layout:[]'),'New Studio projects begin without vehicle zones');

const start=source.indexOf('function otOMAddZone(){');
const end=source.indexOf('function otOMRemoveZone(',start);
assert(start>=0&&end>start,'Universal Add Zone function must exist');
const fn=source.slice(start,end);
const create=new Function('initial',`
 let otOMDraft=structuredClone(initial),otOMBusy=false,otOMSelected=1;
 let otOMSaveState='saved',otOMMessage='',renders=0;
 const render=()=>{renders++};
 ${fn}
 return {add:otOMAddZone,read:()=>({draft:otOMDraft,saveState:otOMSaveState,
  message:otOMMessage,renders})};
`);
const init=[
 {name:'Output 1',pixels:12,color_order:'RGB',zones:[{name:'Custom A',pixels:12,role:'brake'}]},
 {name:'User-defined Output 2',pixels:263,color_order:'GRB',
  calibration:{rgb_gain:[240,250,255],white_gain:[255,220,230],gamma_enabled:false},
  zones:[{name:'Decorative Segment',pixels:123,role:'decorative'},
   {name:'Turn Strip',pixels:140,role:'right_turn'}]}
];
const instance=create(init);
instance.add();
const state=instance.read();
assert.deepEqual(state.draft[0],init[0],'Other outputs are independent');
assert.deepEqual(state.draft[1].zones.slice(0,2),init[1].zones,
 'Adding a zone must preserve existing names, counts, and trigger roles');
assert.deepEqual(state.draft[1].zones[2],{name:'Zone 3',pixels:0,role:'off'},
 'New zones must be unassigned, never hardcoded to vehicle functions');
assert.equal(state.draft[1].pixels,263,'Existing output pixel count must be preserved');
assert.equal(state.draft[1].name,'User-defined Output 2');
assert.equal(state.draft[1].color_order,'GRB','No silent color-order regression');
assert.deepEqual(state.draft[1].calibration,init[1].calibration,'No calibration regression');
assert.equal(state.saveState,'unsaved','Only explicit Save can update controller NVS');
assert.equal(state.renders,1);
assert.deepEqual(init[1].zones.map(z=>z.role),['decorative','right_turn'],
 'The original saved profile must remain untouched');
console.log('Universal zones: neutral additions, no vehicle templates, and preserved user roles passed');
