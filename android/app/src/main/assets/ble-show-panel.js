/* OpenTail native BLE bridge for the existing Show Modes editor.
 * ONE effect list: replaces legacy standalone BLE selector, not Studio UI.
 * No Wi-Fi or pixel streaming. CORE renders supported effects locally.
 * Source status: bench-only, per-session commands expire (max 60 seconds).
 */
(function(){
'use strict';
if(window.__otUnifiedBleFxV1||!window.OpenTailNativeBLE)return;
window.__otUnifiedBleFxV1=true;
const ble=window.OpenTailNativeBLE;
const names={0:'Solid',1:'Blink',3:'Wipe',6:'Sweep',12:'Fade',23:'Strobe'};
let supported=[],readAttempted=false,lastConnected=false,lastReady=false;
let lastSentId=null,liveTimer=null,sendPending=false;
const native={};
function stateOf(){
 try{return JSON.parse(ble.getState()||'{}')}catch(_){return {}}
}
function coreMode(){
 try{return typeof otControllerMode!=='undefined'&&otControllerMode==='core'}catch(_){return false}
}
function active(){
 const s=stateOf();
 try{return coreMode() || (s.connected && !state.connected)}catch(_){return coreMode()}
}
function ready(){const s=stateOf();return !!(s.connected&&s.authorized)}
function flash(message){
 try{state.status=message;if(typeof statusLine==='function')statusLine();}catch(_){}
}
function escHtml(s){return String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));}
function zoneMasks(){
 let id;try{id=otEffectSectionId}catch(_){id='__all__'}
 if(!id||id==='__all__')return [3,255];
 const match=/^core-([01])-([0-7])$/.exec(id);
 if(!match)return null;
 return [1<<Number(match[1]),1<<Number(match[2])];
}
function hex(s,fallback){
 if(!/^#[0-9a-f]{6}$/i.test(String(s||'')))s=fallback;
 const n=parseInt(String(s).slice(1),16);
 return [(n>>16)&255,(n>>8)&255,n&255];
}
function settingsFor(id,e){
 const selected=zoneMasks();if(!selected)return null;
 const fg=hex(e.color,'#ffffff'),bg=hex(e.background,'#000000');
 const clamp=(n,max,def)=>Number.isFinite(Number(n))?Math.max(0,Math.min(max,Math.round(Number(n)))):def;
 return [id,selected[0],selected[1],...fg,...bg,
   clamp(clamp(e.bri,255,160)*32/255,32,20),clamp(e.sx,255,128),
   clamp(e.ix,255,128),0,60];
}
function play(id,props){
 if(!active())return false;
 if(!supported.includes(id)){flash('This effect is not supported by the connected CORE firmware.');return false}
 if(!ready()){flash('Connect and authorize OpenTail CORE over Bluetooth first.');return false}
 const args=settingsFor(id,props||state.wledEdit||{});
 if(!args){flash('Select a confirmed CORE output zone or All connected LEDs.');return false}
 ble.startEffectId(...args);
 lastSentId=id;sendPending=true;
 try{otChosenEffectId=id;state.wledEdit??={};state.wledEdit.fx=id;}catch(_){}
 flash('BLE effect command sent: '+names[id]+'. Awaiting CORE acknowledgement.');
 return true;
}
function refresh(){
 if(!active())return;
 const s=stateOf();
 if(!s.connected){supported=[];readAttempted=false;lastConnected=false;return}
 if(!lastConnected){lastConnected=true;readAttempted=false}
 if(!readAttempted){
   readAttempted=true;
   ble.readEffectCapabilities();
 }
}
function updateCatalog(){
 if(!active())return;
 try{
  // Only these six pinned CORE FX names have implemented frame renderers.
  state.wledEffects=[];
  for(const id of supported)if(names[id])state.wledEffects[id]=names[id];
  if(supported.length && (!state.wledEdit||!supported.includes(Number(state.wledEdit.fx)))){
    state.wledEdit={...(state.wledEdit||{}),fx:supported[0],sx:128,ix:128,
       pal:0,bri:160,color:'#ffffff',background:'#000000',tertiary:'#ffffff'};
  }
  if(typeof render==='function')render();
 }catch(_){}
}
function meta(id){
 if(!supported.includes(Number(id)))return null;
 const solid=id===0,blink=id===1,strobe=id===23,fade=id===12;
 return {controls:solid?[]:[
   {key:'sx',label:'Speed',max:255},
   ...blink||id===3||id===6?[{key:'ix',label:blink?'Duty cycle':'Edge softness',max:255}]:[]
 ],colors:solid?[{i:0,label:'Fx'}]:[{i:0,label:'Fx'},{i:1,label:'Bg'}],
 palette:false,flags:'1',defaults:{sx:128,ix:128,pal:0}};
}
function statusChanged(message){
 const s=stateOf();
 if(!s.connected){
   supported=[];readAttempted=false;lastConnected=false;lastSentId=null;sendPending=false;
 } else refresh();
 if(!active())return;
 if(message&&/OK FX/.test(message)&&sendPending){
   sendPending=false;
   try{state.runningSectionFx=lastSentId;state.runningFx=lastSentId;}catch(_){}
   flash('CORE acknowledged '+(names[lastSentId]||'effect')+'. Temporary bench playback (up to 60s).');
 }else if(message&&/OK STOP/.test(message)){
   sendPending=false;lastSentId=null;
   try{state.runningSectionFx=null;state.runningFx=null;}catch(_){}
   flash('CORE acknowledged Stop.');
 }else if(message&&/ERR/.test(message)){
   sendPending=false;flash('CORE BLE: '+message);
 }
 const wasReady=lastReady;lastReady=ready();
 if(wasReady!==lastReady&&typeof render==='function')render();
}
window.OpenTailBLEEffects={
 isActive:active,ready,refresh,statusChanged,
 capabilities(ids){
  if(!Array.isArray(ids))return;
  const list=[...new Set(ids.filter(x=>Number.isInteger(x)&&names[x]!==undefined))];
  supported=list;
  updateCatalog();
  flash(list.length?'CORE Bluetooth reported '+list.length+' supported effects.':'No BLE effect capabilities available. Verify firmware and retry.');
 },
 choices:()=>supported.map(id=>({id,name:names[id]})),
 meta,select(id){if(!play(Number(id),{...state.wledEdit,fx:id}))return false;
  try{state.runningSectionFx=null;render()}catch(_){}
  return true;
 },
 edit(key,value){
   state.wledEdit??={fx:supported[0]??0,sx:128,ix:128,bri:160,color:'#ffffff',background:'#000000',pal:0};
   if(!['sx','ix','bri','color','background'].includes(key))return;
   state.wledEdit[key]=value;
   const out=document.getElementById('w-'+key);
   if(out)out.textContent=value;
   clearTimeout(liveTimer);
   if(ready()&&supported.includes(Number(state.wledEdit.fx)))
      liveTimer=setTimeout(()=>play(Number(state.wledEdit.fx),state.wledEdit),180);
 },
 stop(){
   if(!ready()){flash('Bluetooth authorization required to stop a CORE effect.');return false}
   clearTimeout(liveTimer);ble.stop();flash('BLE Stop sent — awaiting CORE confirmation.');return true;
 },
 applyEditor(){
   if(typeof otUE==='undefined'||!otUE||otUE.context!=='effects')return false;
   const d=otUE.draft,id=Number(d.fx);
   const e={fx:id,sx:d.sx,ix:d.ix,bri:d.bri,pal:0,
     color:d.col?.[0]||'#ffffff',background:d.col?.[1]||'#000000'};
   if(!play(id,e))return false;
   state.wledEdit={...state.wledEdit,...e};
   otUE=null;render();return true;
 }
};
function wrap(name,fn){
 const original=window[name];
 if(typeof original!=='function')return;
 native[name]=original;
 window[name]=function(...args){return active()?fn(...args):original.apply(this,args)};
}
wrap('otCompatibleEffectChoices',()=>window.OpenTailBLEEffects.choices());
wrap('otEffectCompatible',id=>supported.includes(Number(id)));
wrap('otEffectMeta',id=>meta(id));
wrap('applyWledEffect',id=>window.OpenTailBLEEffects.select(id));
wrap('otSelectEffectSection',id=>{
 if(id!=='__all__'&&!/^core-[01]-[0-7]$/.test(id))return;
 otEffectSectionId=id;otChosenEffectId=null;state.runningSectionFx=null;render();
});
wrap('otReadEffectSection',()=>{refresh();render();});
wrap('setWledEdit',(key,value)=>window.OpenTailBLEEffects.edit(key,value));
wrap('otStopShow',()=>window.OpenTailBLEEffects.stop());
wrap('otUEApply',keepOpen=>{
 if(typeof otUE!=='undefined'&&otUE?.context==='effects')
    return Promise.resolve(window.OpenTailBLEEffects.applyEditor());
 return native.otUEApply(keepOpen);
});
wrap('scenes',()=>{
 let html=native.scenes();
 const ss=stateOf();refresh();
 const label=ss.connected?(ss.authorized?'Bluetooth ready':'Bluetooth connected · authorize'): 'Bluetooth disconnected';
 const banner='<p class="small muted ot-ble-unified-status">CORE effects run locally. '+escHtml(label)+
   ' · <button type="button" class="btn" onclick="window.OpenTailBLEEffects.refresh()">Refresh supported effects</button></p>';
 html=html.replace('<h2>Effects</h2>','<h2>Effects</h2>'+banner);
 html=html.replace('This WLED effect editor is a local design workspace in CORE mode. Use the CORE native lighting controls below for supported physical bench effects.',
  'The same effect library sends Bluetooth commands to CORE; no WLED or Wi-Fi connection is required.');
 html=html.replace(/<details class="card ot-core-advanced ot-core-native-show"[\s\S]*?<\/details>(?=<\/div>$)/,'');
 // Read-only Output Manager zone identifiers are the only safe CORE mapping.
 let zones=[];
 try{if(coreMode()&&typeof otPreviewConfiguredZones==='function')zones=otPreviewConfiguredZones();}catch(_){}
 const opt='<option value="__all__"'+(otEffectSectionId==='__all__'?' selected':'')+'>Both CORE outputs · all zones</option>'+
  zones.map(z=>'<option value="'+escHtml(z.id)+'"'+(otEffectSectionId===z.id?' selected':'')+'>'+escHtml('OUT '+(z.otCoreOutput+1)+' · '+z.name)+'</option>').join('');
 html=html.replace(/<select aria-label="Target for WLED effects"[\s\S]*?<\/select>/,
  '<select aria-label="Target for CORE Bluetooth effects" onchange="otSelectEffectSection(this.value)">'+opt+'</select>');
 return html;
});
refresh();
})();
