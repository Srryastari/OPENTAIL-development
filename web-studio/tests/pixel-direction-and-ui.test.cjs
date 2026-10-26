// Regression: zone reversal alone determines turn and hazard animation direction.
// Global/legacy Hazard settings must never flip a side or override saved zone data.
const assert=require('node:assert/strict');
const fs=require('node:fs');
const html=fs.readFileSync('web-studio/opentail.htm','utf8');
const start=html.indexOf('function otTurnModel('),end=html.indexOf('function otPaintTurnPreview()',start);
assert(start>0&&end>start,'Turn model missing');
const turnModel=new Function(html.slice(start,end)+';return otTurnModel;')();
const config={mode:'sync',pulseHighMs:350,pulseLowMs:350,sweepMs:280,hazard:'sweep'};
assert.equal(turnModel(config,false,140).direction,1);
assert.equal(turnModel(config,true,140).direction,-1,'Reverse must be controlled by selected zone');
for(const legacy of ['same','mirror','static','sweep']){
 assert.equal(turnModel({...config,hazard:legacy},false,140).direction,1,'Hazard mode must never mirror a forward zone');
 assert.equal(turnModel({...config,hazard:legacy},true,140).direction,-1,'Hazard mode must never unmirror a reversed zone');
}
assert.equal(turnModel(config,true,400).active,false,'Off pulse must be dark');
assert(html.includes('const px=x; // x already applies the zone'), 'Do not reverse zone pixel coordinate twice');
assert(html.includes('otTurnModel(cfg,!!d.reverse,now)'), 'Trigger render must use the resolved zone reverse');
assert(html.includes('d.reverse=!!z.reverse'), 'Per-zone direction must be passed to the trigger animation');
assert(html.includes("hazard:'sweep',pulseHighMs:350"), 'No global hazard mirroring default');
assert(html.includes('id="ot-trigger-zone-qa-01955"'), 'Trigger menu visual fix must be bundled');
assert(html.includes('ot-trigger-card-test')&&html.includes('ot-trigger-card-edit'), 'Direct Test and Edit actions required');
const builder=html.slice(html.indexOf('function builder(){'),html.indexOf('function otTriggerColor(',html.indexOf('function builder(){')));
assert(builder.indexOf('ot-assignments-card')<builder.indexOf('ot-automatic-card'), 'Selected trigger editor must be above Automatic Events');
assert(!builder.includes("otBatchEdit('reverseEffect'"), 'Batch UI must not override zone-level mirror');
assert(!builder.includes("otEditSectionAnimation('reverseEffect'"), 'Trigger UI must not override zone-level mirror');
assert(html.includes('data-ot-sampled=')&&html.includes('LIVE · EXACT INDEXES')&&html.includes('ot-live-canvas-scroll'), 'Live pixel output remains exact by data index');

const planStart=html.indexOf('function otBuildTriggerPlan('),planEnd=html.indexOf('function otBaselineSpecs(',planStart);
assert(planStart>0&&planEnd>planStart,'Physical WLED trigger plan builder missing');
const makePlan=new Function('OT_TRIGGER_RANK','state','otResolveTriggerZone','otResolvePattern','otEffectCompatible','otNativeByte','hexRgb',
 html.slice(planStart,planEnd)+';return otBuildTriggerPlan;')(
 {Hazards:3}, {removedSections:[],zoneAliases:{}},
 (id,layout)=>layout.find(z=>z.id===id),
 ()=>0,()=>true,(v,d)=>v??d,()=>[255,192,0]);
const zones=[{id:'left',name:'Left',start:0,length:10,reverse:false},
 {id:'right',name:'Right',start:10,length:10,reverse:true}];
const physical=[{id:0,start:0,stop:10},{id:1,start:10,stop:20}];
const prof={Hazards:{zones:['left','right'],pattern:'Solid',brightness:85,
 sectionAnimations:{left:{reverseEffect:true},right:{reverseEffect:false}}}};
const plan=makePlan(['Hazards'],prof,zones,physical,[],{connectedPixels:20});
assert.deepEqual(plan.errors,[]);
assert.deepEqual(plan.updates.map(x=>x.rev),[false,true],
 'WLED commands must use zones\' physical reverse state, ignoring legacy per-trigger reverseEffect');
prof.Hazards.sectionAnimations.left.reverseEffect=false;
prof.Hazards.sectionAnimations.right.reverseEffect=true;
assert.deepEqual(makePlan(['Hazards'],prof,zones,physical,[],{connectedPixels:20}).updates.map(x=>x.rev),[false,true],
 'Trigger animation settings may not flip physical output direction');

const colorStart=html.indexOf('function otSimColor('),colorEnd=html.indexOf('function otSimStatus()',colorStart);
assert(colorStart>0&&colorEnd>colorStart,'Local pixel renderer missing');
const simColor=new Function('otTurnConfig','otTurnModel','otSimScale','otSimClamp','otSimHue','otSimMix',
 html.slice(colorStart,colorEnd)+';return otSimColor;')(
 ()=>config,turnModel,x=>x,x=>Math.min(1,Math.max(0,x)),()=>{},()=>{});
const args={triggerName:'Right Turn',fx:'solid',fg:'#ffbf00',bg:'#000000',brightness:1,speed:0,intensity:0};
assert.equal(simColor({...args,reverse:false},0,6,140),'#ffbf00','Forward zone begins at index zero');
assert.equal(simColor({...args,reverse:false},5,6,140),'#000000','Forward zone end remains dark halfway through');
assert.equal(simColor({...args,reverse:true},0,6,140),'#000000','Reversed zone begins dark at index zero');
assert.equal(simColor({...args,reverse:true},5,6,140),'#ffbf00','Reversed zone begins sweep at its final index');

console.log('Per-zone reversal, legacy config isolation, restored Trigger menus and exact live index contracts passed');
