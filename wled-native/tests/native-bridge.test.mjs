import assert from 'node:assert/strict';
import {test} from 'node:test';
import {
 mapNativeWledZones,buildNativeWledState,assertNativeWledCapacity,
 discoverNativeWled1DEffects,parseNativeWledEffectMetadata
} from '../opentail-wled-bridge.mjs';
const outputs=[
  {start:0,pixels:522,zones:[{pixels:118},{pixels:109},{pixels:42},{pixels:253}]},
  {start:522,pixels:30,zones:[{pixels:30}]}
];
const supported1DIds=new Set([0,1,3,6,8,9,10,12,23,41,97,205]);
const fx=(output,zone,effectId,extras={})=>({output,zone,effectId,
  speed:180,intensity:64,paletteId:11,brightness:28,
  foreground:[255,25,0],background:[0,0,0],...extras});
test('maps absolute pixels, exclusive stops, separate outputs',()=>{
  assert.deepEqual(mapNativeWledZones(outputs).map(({id,start,stop})=>[id,start,stop]),
    [[0,0,118],[1,118,227],[2,227,269],[3,269,522],[4,522,552]]);
});
test('forwards native WLED IDs and independent zone settings',()=>{
  const state=buildNativeWledState(outputs,[fx(0,3,205),fx(1,0,9,{reverse:true,brightness:10})],{supported1DIds});
  assert.deepEqual(state.seg.map(s=>[s.id,s.fx,s.start,s.stop,s.bri]),
    [[3,205,269,522,28],[4,9,522,552,10]]);
  assert.deepEqual(state.seg[0].col,[[255,25,0],[0,0,0],[0,0,0]]);
  assert.equal(state.seg[1].rev,true);assert.equal(state.seg[0].pal,11);
  assert.equal(state.seg[0].sx,180);assert.equal(state.seg[0].ix,64);
  assert.equal('i' in state.seg[0],false);
});
test('untouched running zones are not reinitialized',()=>{
  assert.deepEqual(buildNativeWledState(outputs,[fx(0,2,41)],{supported1DIds}).seg.map(s=>s.id),[2]);
});
test('unknown and 2D modes are rejected until verified as 1D',()=>{
  assert.throws(()=>buildNativeWledState(outputs,[fx(0,0,217)],{supported1DIds}),/not verified/);
  assert.throws(()=>buildNativeWledState(outputs,[fx(0,0,9)],{}),/capability Set/);
  assert.throws(()=>buildNativeWledState(outputs,[fx(0,0,9),fx(0,0,9)],{supported1DIds}),/duplicate/);
});
test('fails closed on segment limits, overlapping buses or incorrect counts',()=>{
  assert.throws(()=>mapNativeWledZones([{start:0,pixels:3,zones:[{pixels:2}]}]),/do not cover/);
  assert.throws(()=>mapNativeWledZones([{start:0,pixels:3,zones:[{pixels:4}]}]),/pixels/);
  assert.throws(()=>mapNativeWledZones([{start:0,pixels:10,zones:[{pixels:10}]},{start:9,pixels:1,zones:[{pixels:1}]}]),/overlap/);
  assert.throws(()=>mapNativeWledZones(outputs,4),/maxseg/);
  assert.equal(assertNativeWledCapacity({leds:{count:552,maxseg:16}},outputs),true);
  assert.throws(()=>assertNativeWledCapacity({leds:{count:522,maxseg:16}},outputs),/exceeds/);
});
test('rejects invalid controls and non-RGB JSON colors',()=>{
  assert.throws(()=>buildNativeWledState(outputs,[fx(0,0,9,{speed:256})],{supported1DIds}),/speed/);
  assert.throws(()=>buildNativeWledState(outputs,[fx(0,0,9,{brightness:-1})],{supported1DIds}),/brightness/);
  assert.throws(()=>buildNativeWledState(outputs,[fx(0,0,9,{foreground:[300,0,0]})],{supported1DIds}),/foreground/);
  assert.throws(()=>buildNativeWledState(outputs,[fx(0,0,9,{foreground:[0,0,0,255]})],{supported1DIds}),/RGB triplet/);
});
test('parses native metadata labels, dual colors, palette flags and defaults',()=>{
  const m=parseNativeWledEffectMetadata(31,'Chase Flash','Chase Flash@Speed;Bg,Fx;!;1;sx=64,ix=128,pal=11,c1=5,o1=1');
  assert.equal(m.name,'Chase Flash');
  assert.deepEqual(m.parameterLabels,['Speed']);
  assert.deepEqual(m.colorSlots.map(c=>c.label),['Bg','Fx']);
  assert.equal(m.supportsPalette,true);
  assert.deepEqual({...m.defaults},{sx:64,ix:128,pal:11,c1:5,o1:1});
  assert.deepEqual(m.customControls.map(x=>[x.key,x.value]),[['c1',5]]);
  assert.deepEqual(m.options.map(x=>[x.key,x.value]),[['o1',1]]);
  assert.equal(m.dimensions.oneD,true);
});
test('discovers only 1D modes from exact native metadata and preserves audio capability',()=>{
 const r=discoverNativeWled1DEffects(
  ['Solid','Rainbow','2D Plasma','Audio Scan','Mixed 1D2D','Cube','Legacy'],
  ['Solid','!,!;;!;1','!,!;;!;2','!,!;;!;1f','!,!;;!;12','!,!;;!;3',''],
  {fxcount:7});
 assert.deepEqual(r.effects.map(e=>e.id),[0,1,3,4,6]);
 assert.equal(r.effects.find(e=>e.id===3).requiresAudio,true);
 assert.equal(r.supported1DIds.has(2),false);
 assert.equal(r.supported1DIds.has(4),true);
 assert.equal(r.metadataById.get(1).colorSlots.length,2);
 assert.throws(()=>discoverNativeWled1DEffects(['Only'],[],{fxcount:1}),/match/);
});
test('validates effect color slots and applies native metadata defaults',()=>{
 const meta=parseNativeWledEffectMetadata(31,'Chase Flash','Chase Flash@Speed;Bg,Fx;!;1;sx=64,ix=128,pal=11,c1=5,o1=1');
 const metadataById=new Map([[31,meta]]),ids=new Set([31]);
 const state=buildNativeWledState(outputs,[{output:0,zone:0,effectId:31,brightness:100,
  foreground:[1,2,3],background:[4,5,6]}],{supported1DIds:ids,metadataById});
 assert.deepEqual([state.seg[0].sx,state.seg[0].ix,state.seg[0].pal,state.seg[0].c1,state.seg[0].o1],
  [64,128,11,5,1]);
 assert.deepEqual(state.seg[0].col,[[1,2,3],[4,5,6],[0,0,0]]);
 assert.throws(()=>buildNativeWledState(outputs,[{output:0,zone:0,effectId:31,brightness:100,
  foreground:[1,2,3]}],{supported1DIds:ids,metadataById}),/requires 2 declared color slots/);
});
test('refuses to silently discard unsupported metadata control fields',()=>{
 const meta=parseNativeWledEffectMetadata(31,'Example','Example@Slider;Fx;!;1;c4=20');
 assert.throws(()=>buildNativeWledState(outputs,[fx(0,0,31)],{
  supported1DIds:new Set([31]),metadataById:new Map([[31,meta]])
 }),/does not yet support c4/);
});
