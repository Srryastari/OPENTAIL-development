// Verify exact WLED live RGB trace renders unmirrored, contiguous indexed pixels, never invented telemetry.
const assert=require('node:assert/strict');
const fs=require('node:fs'),html=fs.readFileSync('web-studio/opentail.htm','utf8');
const a=html.indexOf('function otLiveInspectPixel('),b=html.indexOf('function otPeekPaint()',a);
assert(a>0&&b>a);
const rectangles=[],flags=[];
const pixels=['#ff0000','#ff0000','#00ff00','#0000ff'];
const context={fillStyle:'',fillRect(x,y,w,h){rectangles.push({color:this.fillStyle,x,y,w,h})}};
const label={textContent:''};
const canvas={dataset:{otLiveStart:'0',otLiveCount:'4'},width:20,getBoundingClientRect(){return {left:0,width:20}},getContext(){return context},closest(){return {querySelector(){return label}}}};
const region={hidden:true,querySelectorAll(){return [canvas]}};
const root={classList:{toggle(name,on){flags.push({name,on})}}};
const document={querySelector(){return root},querySelectorAll(){return [region]}};
const {paint,inspect,setFrame}=new Function('document','otPeekExact','otPeekPixels',
 'let otPeekFrames=1,otPeekLastCanvasFrame=-1,otPeekLastCanvasRoot=null,otPeekLastCanvasFull=false;'+
 html.slice(a,b)+';return {paint:otPeekLiveCanvases,inspect:otLiveInspectPixel,setFrame:n=>otPeekFrames=n};'
)(document,true,pixels);
paint(true,false);
assert.equal(region.hidden,false);
assert.deepEqual(rectangles,[{color:'#ff0000',x:0,y:0,w:10,h:20},{color:'#00ff00',x:10,y:0,w:5,h:20},{color:'#0000ff',x:15,y:0,w:5,h:20}]);
paint(true,false);
assert.equal(rectangles.length,3,'An unchanged exact frame must not redraw every canvas');
setFrame(2);paint(true,false);
assert.equal(rectangles.length,6,'A newly received exact frame must redraw');
inspect({clientX:13},canvas);
assert.equal(label.textContent,'Data index 2 · reported RGB #00ff00');
paint(false,false);assert.equal(region.hidden,true);
paint(true,true);assert.equal(region.hidden,true,'Local simulation must never be labeled exact physical output');
paint(true,false);assert.equal(rectangles.length,9,'Returning from simulated/idle to live must redraw');
assert(html.includes('ot-live-canvas-scroll')&&html.includes('data-ot-live-start='),'Full indexed canvas view must exist');
assert(html.includes('const first=z.start+chunk*2048'),'Every zone must use exact data index offsets');
const queueStart=html.indexOf('function otPeekQueuePaint()'),queueEnd=html.indexOf('function otPeekSchedule(',queueStart);
assert(queueStart>0&&queueEnd>queueStart);
let pending,scheduled=0,paints=0;
const queue=new Function('document','requestAnimationFrame','otPeekPaint',
 'let otPeekPaintRaf=0;'+html.slice(queueStart,queueEnd)+';return otPeekQueuePaint;'
)({hidden:false},cb=>{scheduled++;pending=cb;return scheduled;},()=>{paints++});
queue();queue();queue();assert.equal(scheduled,1,'Bursty WebSocket frames must coalesce into a single paint');
pending();assert.equal(paints,1);
queue();assert.equal(scheduled,2,'The next browser frame must remain paintable');
assert(html.includes('    otPeekQueuePaint();'),'Incoming binary frames must use the paint scheduler');
const simStart=html.indexOf('function otSimAnimate()'),simEnd=html.indexOf('function otSimControls()',simStart);
assert(simStart>0&&simEnd>simStart);
let simCallback,simFrames=0;
const sim=new Function('document','requestAnimationFrame','otPeekPaint',
 'let otSimRaf=0,otSimLastPaint=0,otUE=null;const otSim={active:true,playing:true};'+
 html.slice(simStart,simEnd)+';return otSimAnimate;'
)({hidden:false},cb=>{simCallback=cb;return 1;},()=>{simFrames++});
sim();simCallback(16);assert.equal(simFrames,0);
simCallback(34);assert.equal(simFrames,1,'Simulated preview should paint on the next ~30Hz display slot');
simCallback(50);assert.equal(simFrames,1,'Do not perform duplicate DOM work on adjacent refreshes');
simCallback(68);assert.equal(simFrames,2);
console.log('Exact indexed live RGB trace, readout, and simulation separation passed');
