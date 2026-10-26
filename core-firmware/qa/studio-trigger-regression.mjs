import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';

const project=process.argv[2];
const html=fs.readFileSync(`${project}/opentail-v0.19.51.htm`,'utf8');
const a=html.indexOf('const OT_TM_FUNCTIONS=');
const b=html.indexOf('/* OpenTail CORE Universal Output Manager v1.',a);
assert(a>0 && b>a,'Studio Trigger Manager must be present');
const code=html.slice(a,b);
const factory=['park','brake','left','right','reverse','aux1','aux2','aux3'];
const clone=x=>JSON.parse(JSON.stringify(x));
const device={
  revision:1,saved_on_device:false,bench_only:true,physical_inputs:8,
  inputs:factory.map((fn,i)=>({index:i+1,gpio:i+4,function:fn,active:false}))
};
let posts=0,gets=0,corrupt=false,reject=false;
const env={
 console,
 otControllerMode:'core',otCoreConnected:true,otCoreCaps:{trigger_manager_v1:true},otCoreStatus:{input_mask:0},
 render(){},
 esc(s){return String(s)},
 async otCoreRequest(path,payload){
  assert.equal(path,'/api/v1/triggers');
  if(payload===undefined){
    gets++;
    const d=clone(device);
    if(corrupt)d.inputs[1].function='disabled';
    return d;
  }
  posts++;
  if(reject)throw Error('HTTP 409 — physical input active');
  assert.equal(payload.revision,1);
  assert.equal(payload.inputs.length,8);
  payload.inputs.forEach((x,i)=>{assert.equal(x.index,i+1); assert.equal(typeof x.function,'string')});
  device.inputs=payload.inputs.map((x,i)=>({...device.inputs[i],function:x.function}));
  device.saved_on_device=true;
  return clone(device);
 }
};
vm.createContext(env); vm.runInContext(code,env);
const run=(js)=>vm.runInContext(js,env);
await run('otTMRead()');
assert.equal(run('otTMSaveState'),'default');
assert.deepEqual([...run('otTMDraft.map(x=>x.function)')],factory);
run("otTMEdit(1,'brake')");
assert.equal(run('otTMSaveState'),'unsaved');
await run('otTMSave()');
assert.equal(posts,1);
assert.equal(run('otTMSaveState'),'saved');
assert.equal(device.inputs[0].function,'brake');
assert.equal(device.inputs[1].function,'brake');
run("otTMEdit(6,'disabled')");
await run('otTMSave()');
assert.equal(posts,2);
assert.equal(device.inputs[5].function,'disabled');
run('otTMRestoreDefaults()');
assert.equal(run('otTMSaveState'),'unsaved');
assert.deepEqual([...run('otTMDraft.map(x=>x.function)')],factory);
run('otTMDiscard()');
assert.equal(run('otTMDraft[0].function'),'brake');
assert.equal(run('otTMDraft[5].function'),'disabled');
run("otTMEdit(8,'left')");
reject=true;
await run('otTMSave()');
assert.equal(run('otTMSaveState'),'failed');
assert.equal(device.inputs[7].function,'aux3');
reject=false;
await run('otTMSave()');
assert.equal(run('otTMSaveState'),'saved');
corrupt=true;
run("otTMEdit(4,'reverse')");
await run('otTMSave()');
assert.equal(run('otTMSaveState'),'failed','Corrupt readback must not be marked saved');
corrupt=false;
await run('otTMRead()');
assert.equal(run('otTMSaveState'),'saved');
assert.equal(run('otTMDraft[3].function'),'reverse');
device.revision=2;
await run('otTMRead()');
assert.equal(run('otTMSaveState'),'failed','Unknown API revision rejected');
console.log(`PASS: Studio Trigger Manager mocked API contract (${gets} GET, ${posts} POST): defaults, remap, disabled, discard, rejected save, verified readback and version check.`);
