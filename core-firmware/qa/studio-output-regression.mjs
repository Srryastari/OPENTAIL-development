import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';

const project = process.argv[2];
if (!project) throw Error('Usage: node studio-regression.mjs <OpenTail-project-folder>');
const html = fs.readFileSync(`${project}/opentail-v0.19.51.htm`, 'utf8');

// Execute the genuine, unchanged Output Manager functions in a mocked device environment.
const start = html.indexOf('let otOMData=null,otOMDraft=null');
const stop = html.indexOf('function otOutputManagerCard()', start);
assert(start > 0 && stop > start, 'Studio Output Manager definitions must be present');
const omSource = html.slice(start, stop);

const zones = (count, role='decorative') => Array.from({length: count}, (_,i) => ({name:`Zone ${i+1}`,pixels:i===0?30:0,role,trigger_active:false}));
const device = {
  revision:2, saved_on_device:true, bench_only:true, physical_outputs:2,
  future_outputs_target:12, allocated_pixels_per_output:522,
  max_zones_per_output:8, dynamic_zones:true, effective_mask:0,
  outputs:[
    {index:0,pin:16,name:'Pixel OUT 1',pixels:522,zone_count:4,
     calibration:{rgb_gain:[255,255,255],white_gain:[255,255,255],gamma_enabled:false},
     color_order:'GRB',zones:[
      {name:'Top',pixels:118,role:'park_brake',trigger_active:false},
      {name:'Turn',pixels:109,role:'left_turn',trigger_active:false},
      {name:'Reverse',pixels:42,role:'reverse',trigger_active:false},
      {name:'Bottom',pixels:253,role:'park_brake',trigger_active:false}]},
    {index:1,pin:17,name:'Pixel OUT 2',pixels:30,zone_count:1,
     calibration:{rgb_gain:[255,255,255],white_gain:[255,255,255],gamma_enabled:false},
     color_order:'GRB',zones:zones(1)}
  ]
};
let posts=0, reads=0, corruptReadback=false;
function clone(o){return JSON.parse(JSON.stringify(o));}
const env={
  console,
  otControllerMode:'core', otCoreCaps:{output_manager_v1:true}, otCoreConnected:true,
  state:{tab:'layout'}, render(){},
  document:{getElementById(){return null;}},
  async otCoreRequest(path, payload){
    assert.equal(path,'/api/v1/outputs');
    if (payload === undefined) {
      reads++;
      const d=clone(device);
      if (corruptReadback) d.outputs[1].pixels=999;
      return d;
    }
    posts++;
    assert.equal(payload.index,1,'Changes must target only OUT 2');
    assert(Number.isInteger(payload.pixels) && payload.pixels>=1 && payload.pixels<=522);
    assert(payload.zones.length>=1 && payload.zones.length<=8);
    assert.equal(payload.zones.reduce((s,z)=>s+z.pixels,0),payload.pixels);
    const untouched=clone(device.outputs[0]);
    device.outputs[1]={...device.outputs[1],...clone(payload),zone_count:payload.zones.length,
      zones:payload.zones.map(z=>({...z,trigger_active:false}))};
    assert.deepEqual(device.outputs[0],untouched,'OUT 1 cannot be modified by OUT 2 save');
    return clone(device);
  }
};
vm.createContext(env);
vm.runInContext(omSource,env,{filename:'Studio Output Manager'});
const run = async code => vm.runInContext(code,env);
const value = code => vm.runInContext(code,env);

await run('otOMRead()');
assert.equal(value('otOMSaveState'),'saved');
assert.equal(value('otOMDraft[0].pixels'),522);
assert.equal(value('otOMDraft[1].zones.length'),1);
assert.equal(value('otOMDraft[1].color_order'),'GRB');

// Dynamic add, zero-length zones, validation and successful NVS-style readback.
await run('otOMSelected=1;otOMAddZone()');
assert.equal(value('otOMDraft[1].zones.length'),2);
assert.equal(value('otOMDraft[1].zones[1].pixels'),0);
assert.equal(value('otOMSaveState'),'unsaved');
await run('otOMSave()');
assert.equal(value('otOMSaveState'),'saved');
assert.equal(posts,1);
assert.equal(value('otOMDraft[1].zones.length'),2);

// Delete zone, save and reload; this is the exact UI path previously suspect.
await run('otOMRemoveZone(1)');
assert.equal(value('otOMDraft[1].zones.length'),1);
await run('otOMSave()');
assert.equal(value('otOMSaveState'),'saved');
assert.equal(value('otOMDraft[1].zones.length'),1);
assert.equal(posts,2);

// Draft mismatch must block POST; preserve last device values.
await run("otOMEdit('pixels','31',0)");
assert.equal(value('otOMSaveState'),'unsaved');
await run('otOMSave()');
assert.equal(posts,2);
assert.equal(value('otOMSaveState'),'unsaved');
await run("otOMEdit('pixels','30',0)");
await run('otOMSave()');
assert.equal(value('otOMSaveState'),'saved');
assert.equal(posts,3);

// Device readback mismatch must be reported as failure, never as Saved.
corruptReadback=true;
await run("otOMEdit('name','New Test Strip')");
await run('otOMSave()');
assert.equal(value('otOMSaveState'),'failed');
assert(value('otOMMessage').includes('Readback did not match'));
corruptReadback=false;
await run('otOMRead()');
assert.equal(value('otOMSaveState'),'saved');
assert.equal(value('otOMDraft[1].name'),'New Test Strip');

// Reject older API revisions instead of silently misinterpreting them.
device.revision=1;
await run('otOMRead()');
assert.equal(value('otOMSaveState'),'failed');
assert(value('otOMMessage').includes('Unsupported output configuration version'));

console.log(`PASS: Studio Output Manager mocked API contract (${reads} GET, ${posts} POST): revision checking, add/delete, zero-length zone, counts, isolated OUT 2 updates, save/readback, rejected readback.`);
