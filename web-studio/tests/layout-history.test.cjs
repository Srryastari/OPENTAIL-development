// Regression for approved Layout undo history, including removed-zone archive consistency.
const assert=require('node:assert/strict');
const fs=require('node:fs');
const html=fs.readFileSync('web-studio/opentail.htm','utf8');
const first=html.indexOf('let otLayoutSelected=');
const last=html.indexOf('function otBaseArrangementLayout()',first);
assert(first>0 && last>first);
const code=html.slice(first,last);
const initial=[
 {id:'zone_a',name:'Parking',start:0,length:10,reverse:false},
 {id:'zone_b',name:'Turn',start:10,length:6,reverse:false},
 {id:'zone_c',name:'Reverse',start:16,length:4,reverse:false}
];
const state={layout:structuredClone(initial),removedSections:[],segmentsSynced:true};
const save=()=>true;
const rerender=()=>{};
const mockRemove=(id)=>{const z=state.layout.find(x=>x.id===id);if(!z)return;
 state.removedSections.push(z);state.layout=state.layout.filter(x=>x.id!==id)};
const mockSetLength=(id,n)=>{const i=state.layout.findIndex(x=>x.id===id),old=state.layout[i].length,delta=n-old;
 state.layout[i].length=n;for(let j=i+1;j<state.layout.length;j++)state.layout[j].start+=delta;};
const make=new Function('state','persist','render','removeSection','otSetSectionLength',
 'otRenameZone','editLayout','confirm','clearInterval','setInterval','document',
 code+';return {snapshot:otLayoutSnapshot,remove:otLayoutRemove,undo:otLayoutUndoChange,redo:otLayoutRedoChange,rename:otLayoutRename,resize:otLayoutResize,move:otLayoutMove,select:otLayoutSelect,identify:otLayoutIdentify};');
let identifyCalls=[];
const buttons=['off','all','first','last','sweep'].map(mode=>({dataset:{otIdentify:mode},setAttribute:(key,value)=>identifyCalls.push([mode,key,value])}));
const pixels=Array.from({length:5},()=>({classList:{toggle:()=>{}}}));
const doc={querySelector:(selector)=>selector==='.ot-zone-preview'?{children:pixels}:null,
 querySelectorAll:()=>buttons};
const app=make(state,save,rerender,mockRemove,mockSetLength,
 (id,name)=>{state.layout.find(x=>x.id===id).name=name},
 (id,key,value)=>{state.layout.find(x=>x.id===id)[key]=value},
 ()=>true,()=>{},()=>1,doc);
app.remove('zone_b');
assert.equal(state.layout.length,2);
assert.equal(state.removedSections.length,1);
app.undo();
assert.deepEqual(state.layout,initial,'Undo remove should restore sections');
assert.deepEqual(state.removedSections,[],'Undo remove must clear duplicate archive entry');
app.redo();
assert.equal(state.layout.length,2);
assert.equal(state.removedSections[0].id,'zone_b');
app.undo();
app.rename('zone_a','Tail');
assert.equal(state.layout[0].name,'Tail');
app.undo();
assert.equal(state.layout[0].name,'Parking');
app.resize('zone_a',13);
assert.equal(state.layout[1].start,13);
app.undo();
assert.equal(state.layout[0].length,10);
assert.equal(state.layout[1].start,10);
app.move('zone_c',-1);
assert.deepEqual(state.layout.map(z=>z.id),['zone_a','zone_c','zone_b']);
assert.deepEqual(state.layout.map(z=>z.start),[0,10,14]);
app.undo();
assert.deepEqual(state.layout,initial);
app.identify('first');
assert(identifyCalls.some(([mode,key,value])=>mode==='first'&&key==='aria-pressed'&&value==='true'));
app.identify('off');
assert(identifyCalls.some(([mode,key,value])=>mode==='off'&&key==='aria-pressed'&&value==='true'));
console.log('Layout undo/redo, archived zones, reorder, resize, rename, identify tests passed');
