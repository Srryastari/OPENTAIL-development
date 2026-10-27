/**
 * OpenTail -> native WLED state adapter.
 * This module contains ZERO pixel-generation code. It forwards canonical
 * WLED effect IDs and controls to the upstream WLED /json/state engine.
 * Source pin: wled/WLED@20e7a717664295eefc6ad77972cee1888d2da072
 * API: https://kno.wled.ge/interfaces/json-api/
 */
const integer=(v,min,max,name)=>{
  if(!Number.isInteger(v)||v<min||v>max)
    throw new RangeError(`${name} must be an integer in ${min}..${max}`);
  return v;
};
function color(value,name){
  if(!Array.isArray(value)||(value.length!==3&&value.length!==4))
    throw new TypeError(`${name} must be an RGB or RGBW array`);
  return value.map((n,i)=>integer(n,0,255,`${name}[${i}]`));
}
/**
 * Outputs MUST use the bus starts configured in actual WLED hardware.
 * WLED stop is exclusive. Zones are 1D and receive distinct global IDs.
 */
export function mapNativeWledZones(outputs,maxSegments=32){
  if(!Array.isArray(outputs)||outputs.length<1||outputs.length>12)
    throw new RangeError('Expected 1..12 configured physical outputs');
  integer(maxSegments,1,64,'maxSegments');
  const result=[],buses=[];let nextId=0;
  outputs.forEach((o,output)=>{
    if(!o||!Array.isArray(o.zones)||o.zones.length<1||o.zones.length>8)
      throw new RangeError(`Output ${output}: expected 1..8 non-empty zones`);
    const start=integer(o.start,0,65534,`output ${output} start`);
    const pixels=integer(o.pixels,1,65535-start,`output ${output} pixels`);
    const stop=start+pixels;
    if(buses.some(b=>start<b.stop&&stop>b.start))
      throw new RangeError('Physical output pixel ranges overlap');
    buses.push({start,stop});
    let index=start;
    o.zones.forEach((z,zone)=>{
      const size=integer(z?.pixels,1,pixels,`output ${output} zone ${zone} pixels`);
      if(index+size>stop)throw new RangeError(`Output ${output} zones exceed configured pixels`);
      if(nextId>=maxSegments)throw new RangeError('WLED maxseg exceeded');
      result.push(Object.freeze({id:nextId++,output,zone,start:index,stop:index+size}));
      index+=size;
    });
    if(index!==stop)
      throw new RangeError(`Output ${output} zones do not cover its configured pixel count`);
  });
  return Object.freeze(result);
}
/**
 * accepted assignments: output, zone, effectId, speed, intensity, paletteId,
 * brightness, foreground, background, optional tertiary/reverse/mirror.
 * IDs MUST come from verified native WLED 1D capability discovery.
 * This is deliberately NOT a replacement for CORE's offline BLE transport.
 */
export function buildNativeWledState(outputs,assignments,{
  supported1DIds,maxSegments=32,transition=0
}={}){
  if(!(supported1DIds instanceof Set)||supported1DIds.size===0)
    throw new TypeError('A verified WLED-native 1D effect capability Set is required');
  if(!Array.isArray(assignments)||assignments.length===0)
    throw new RangeError('At least one output/zone assignment is required');
  integer(transition,0,65535,'transition');
  const zones=mapNativeWledZones(outputs,maxSegments),seen=new Set(),seg=[];
  for(const a of assignments){
    const out=integer(a?.output,0,outputs.length-1,'output');
    const zone=integer(a?.zone,0,outputs[out].zones.length-1,'zone');
    const mapping=zones.find(z=>z.output===out&&z.zone===zone);
    if(!mapping||seen.has(mapping.id))
      throw new RangeError('Missing or duplicate segment assignment');
    seen.add(mapping.id);
    const fx=integer(a.effectId,0,65535,'effectId');
    if(!supported1DIds.has(fx))
      throw new RangeError(`WLED 1D mode ${fx} is not verified in this firmware`);
    const sx=integer(a.speed,0,255,'speed');
    const ix=integer(a.intensity,0,255,'intensity');
    const pal=integer(a.paletteId,0,255,'paletteId');
    const bri=integer(a.brightness,0,255,'brightness');
    const col=[color(a.foreground,'foreground'),color(a.background,'background')];
    if(a.tertiary!==undefined)col.push(color(a.tertiary,'tertiary'));
    seg.push({id:mapping.id,start:mapping.start,stop:mapping.stop,fx,sx,ix,pal,bri,col,
      on:true,rev:Boolean(a.reverse),mi:Boolean(a.mirror),frz:false});
  }
  return {on:true,transition,seg};
}
/** Fail before playback if real WLED bus allocation cannot host the layout. */
export function assertNativeWledCapacity(info,outputs,maxSegmentsFallback=32){
  const leds=info?.leds;
  if(!leds||!Number.isInteger(leds.count)||!Number.isInteger(leds.maxseg))
    throw new TypeError('Expected native WLED /json/info with leds.count and leds.maxseg');
  const zones=mapNativeWledZones(outputs,Math.min(maxSegmentsFallback,leds.maxseg));
  if(zones.some(z=>z.stop>leds.count))
    throw new RangeError('Physical layout exceeds WLED configured LED count');
  return true;
}


/**
 * Discover every compiled native WLED 1D-capable mode from /json/eff and
 * /json/fxdata metadata, not a hard-coded subset. WLED 1D flag defaults to 1;
 * 2D and 3D flags exclude the effect unless 1 or single-LED flag 0 is set.
 * v/f indicate effects whose native behavior depends on sound data.
 */
export function discoverNativeWled1DEffects(effectNames,fxdata,info){
  if(!Array.isArray(effectNames)||!Array.isArray(fxdata)||
     !Number.isInteger(info?.fxcount)||
     effectNames.length!==info.fxcount||fxdata.length!==info.fxcount)
    throw new TypeError('Native WLED effect catalog and metadata must match info.fxcount');
  const effects=[];
  for(let id=0;id<info.fxcount;id++){
    const name=effectNames[id],meta=fxdata[id];
    if(typeof name!=='string'||!name.trim()||typeof meta!=='string')continue;
    const fields=meta.split(';');
    const flags=fields.length>3&&fields[3]!==''?fields[3]:'1';
    if(!/^[0123vf]+$/.test(flags))
      throw new TypeError(`WLED effect ${id}: unrecognized metadata flags`);
    if((flags.includes('2')||flags.includes('3'))&&!flags.includes('1')&&!flags.includes('0'))
      continue;
    effects.push(Object.freeze({
      id,name,metadata:meta,requiresAudio:flags.includes('v')||flags.includes('f'),
      flags,supportsPalette:fields.length>2?fields[2]!=='' : true
    }));
  }
  return Object.freeze({effects:Object.freeze(effects),
    supported1DIds:new Set(effects.map(x=>x.id))});
}
