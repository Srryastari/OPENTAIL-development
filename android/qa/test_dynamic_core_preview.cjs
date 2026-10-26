const fs=require('fs'),vm=require('vm'),assert=require('assert');
const source=fs.readFileSync('app/src/main/assets/opentail.htm','utf8');
const from=source.indexOf('function otPreviewConfiguredZones(){');
const to=source.indexOf('function otPixelPreview(){',from);
assert(from>0&&to>from,'Read-only CORE preview adapter found');
const fn=source.slice(from,to);
const ctx={otControllerMode:'core',state:{layout:[{id:'old',name:'Old WLED',start:0,length:10}]},otOMData:{outputs:[
 {name:'Left',pixels:522,zones:[{name:'Top',pixels:118},{name:'Turn',pixels:109},{name:'Reverse',pixels:42},{name:'Bottom',pixels:253},{name:'No pixels',pixels:0}]},
 {name:'Right',pixels:30,zones:[{name:'Test Strip',pixels:30}]}
]}};
vm.createContext(ctx);vm.runInContext(fn,ctx);
const get=()=>vm.runInContext('otPreviewConfiguredZones()',ctx);
assert.deepStrictEqual(Array.from(get(),x=>x.name),['Top','Turn','Reverse','Bottom','Test Strip']);
assert.deepStrictEqual(Array.from(get(),x=>x.length),[118,109,42,253,30]);
assert.deepStrictEqual(Array.from(get(),x=>x.start),[0,118,227,269,522]);
ctx.otOMData.outputs[1].zones[0].name='Rear DRL';
assert.strictEqual(get()[4].name,'Rear DRL');
ctx.otOMData.outputs[1].zones=[{name:'A',pixels:10},{name:'B',pixels:20}];
assert.deepStrictEqual(Array.from(get(),x=>x.name),['Top','Turn','Reverse','Bottom','A','B']);
ctx.otOMData.outputs[1].zones=[{name:'Removed all',pixels:0}];
assert.strictEqual(get().length,4,'Zero-pixel zone hidden');
ctx.otControllerMode='wled';
assert.strictEqual(get()[0].name,'Old WLED','WLED layout unaffected');
ctx.otControllerMode='core';ctx.otOMData=null;
assert.strictEqual(get().length,0,'No false CORE zones when configuration unavailable');
console.log('PASS: dynamic CORE preview mirrors saved zones across outputs, renames/add/delete/zero, WLED unchanged.');
