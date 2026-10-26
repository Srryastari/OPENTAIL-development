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
console.log('Per-zone reversal, legacy config isolation, restored Trigger menus and exact live index contracts passed');
