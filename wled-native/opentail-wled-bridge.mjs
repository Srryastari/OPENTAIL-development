/**
 * OpenTail -> native WLED state adapter.
 * Zero pixel-generation code: upstream WLED is the canonical renderer.
 * Source pin: wled/WLED@20e7a717664295eefc6ad77972cee1888d2da072
 */
const integer=(v,min,max,name)=>{
  if(!Number.isInteger(v)||v<min||v>max)
    throw new RangeError(`${name} must be an integer in ${min}..${max}`);
  return v;
};
function color(value,name){
  // WLED segment JSON colors are logical RGB values. Bus order/channel
  // conversion belongs to WLED's configured hardware bus, not this adapter.
  if(!Array.isArray(value)||value.length!==3)
    throw new TypeError(`${name} must be an RGB triplet`);
  return value.map((n,i)=>integer(n,0,255,`${name}[${i}]`));
}
const splitList=value=>String(value??'').split(',').map(x=>x.trim());
function parseDefaults(raw){
  const result={};
  for(const entry of String(raw??'').split(',')){
    if(!entry.trim())continue;
    const m=entry.trim().match(/^([a-zA-Z][a-zA-Z0-9]*)=(-?\d+)$/);
    if(m)result[m[1]]=Number(m[2]);
  }
  return Object.freeze(result);
}
/**
 * Parse the pinned WLED FX metadata format:
 * name@slider labels ; color-slot labels ; palette marker ; capability flags ; defaults
 * Raw metadata is retained for diagnostics. The parser never invents absent slots.
 */
export function parseNativeWledEffectMetadata(id,name,raw){
  integer(id,0,65535,'effect id');
  if(typeof name!=='string'||!name.trim()||typeof raw!=='string')
    throw new TypeError('Effect name and raw WLED metadata are required');
  const fields=raw.split(';');
  const head=fields[0]??'';
  const at=head.indexOf('@');
  const canonicalName=(at<0?head:head.slice(0,at)).trim()||name.trim();
  const parameterText=at<0?'':head.slice(at+1);
  const flags=fields.length>3&&fields[3]!==''?fields[3]:'1';
  if(!/^[0123vf]+$/.test(flags))
    throw new TypeError(`WLED effect ${id}: unrecognized metadata flags`);
  const parameterLabels=splitList(parameterText);
  const colorLabels=splitList(fields[1]??'').filter(x=>x!==''&&x!=='-');
  const defaults=parseDefaults(fields[4]??'');
  return Object.freeze({
    id,name:canonicalName,rawMetadata:raw,parameterLabels:Object.freeze(parameterLabels),
    colorSlots:Object.freeze(colorLabels.map((label,index)=>Object.freeze({
      index,label:label==='!'?`Color ${index+1}`:label
    }))),
    supportsPalette:fields.length>2&&fields[2]!==''&&fields[2]!=='-',
    flags,requiresAudio:flags.includes('v')||flags.includes('f'),
    dimensions:Object.freeze({
      oneD:flags.includes('1')||flags.includes('0'),
      matrix:flags.includes('2')||flags.includes('3'),
      threeD:flags.includes('3')
    }),
    defaults
  });
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
 * Each assignment may carry metadata parsed from the same native WLED build.
 * WLED's native effect runtime interprets the effect controls; this adapter
 * validates IDs and maps declared color slots to the native segment col array.
 */
export function buildNativeWledState(outputs,assignments,{
  supported1DIds,maxSegments=32,transition=0,metadataById
}={}){
  if(!(supported1DIds instanceof Set)||supported1DIds.size===0)
    throw new TypeError('A verified WLED-native 1D effect capability Set is required');
  if(!Array.isArray(assignments)||assignments.length===0)
    throw new RangeError('At least one output/zone assignment is required');
  transition=integer(transition,0,65535,'transition');
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
    const meta=metadataById instanceof Map?metadataById.get(fx):undefined;
    const provided=[a.foreground,a.background,a.tertiary].filter(v=>v!==undefined);
    if(meta&&provided.length<meta.colorSlots.length)
      throw new RangeError(`Effect ${fx} requires ${meta.colorSlots.length} declared color slots`);
    if(meta&&meta.colorSlots.length>3)
      throw new RangeError(`Effect ${fx} declares more than WLED's three segment color slots`);
    const col=[
      color(a.foreground??[255,255,255],'foreground'),
      color(a.background??[0,0,0],'background'),
      color(a.tertiary??[0,0,0],'tertiary')
    ];
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
 * Discover compiled 1D-capable effects from /json/eff and /json/fxdata.
 * Metadata comes from the actual target firmware; never hardcode parity.
 */
export function discoverNativeWled1DEffects(effectNames,fxdata,info){
  if(!Array.isArray(effectNames)||!Array.isArray(fxdata)||
     !Number.isInteger(info?.fxcount)||
     effectNames.length!==info.fxcount||fxdata.length!==info.fxcount)
    throw new TypeError('Native WLED effect catalog and metadata must match info.fxcount');
  const effects=[];
  for(let id=0;id<info.fxcount;id++){
    const name=effectNames[id],raw=fxdata[id];
    if(typeof name!=='string'||!name.trim()||typeof raw!=='string')continue;
    const metadata=parseNativeWledEffectMetadata(id,name,raw);
    if(!metadata.dimensions.oneD)continue;
    effects.push(metadata);
  }
  return Object.freeze({effects:Object.freeze(effects),
    supported1DIds:new Set(effects.map(x=>x.id)),
    metadataById:new Map(effects.map(x=>[x.id,x]))});
}
