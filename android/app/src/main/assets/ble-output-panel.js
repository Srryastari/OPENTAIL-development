/* OpenTail BLE Output Manager (Android only; Wi-Fi Studio remains intact).
 * Current bench authorization requires a token fetched over CORE WPA2 Wi-Fi.
 * This component NEVER claims success until CORE reports post-NVS readback.
 */
(()=>{
'use strict';
if(window.__otBleOutputsInstalled || !window.OpenTailNativeBLE)return;
window.__otBleOutputsInstalled=true;
const roles=[['off','Disabled'],['park_brake','Parking + Brake'],['parking','Parking'],['brake','Brake'],['left_turn','Left Turn'],['right_turn','Right Turn'],['reverse','Reverse'],['aux1','Aux 1'],['aux2','Aux 2'],['aux3','Aux 3'],['decorative','Show only']];
const orders=['RGB','RBG','GRB','GBR','BRG','BGR'];
const saved=[null,null],draft=[null,null];
let selected=0,busy=false,pendingIndex=null,lastReadAt='Not yet',message='Connect BLE, pair once using BOOT, then load device outputs.',shown=false;
const escape=v=>String(v??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
function nativeState(){try{return JSON.parse(window.OpenTailNativeBLE.getState()||'{}');}catch{return {};}}
function isLayout(){try{return typeof state!=='undefined'&&state.tab==='layout';}catch{return false;}}
function options(items,value){return items.map(([v,label])=>`<option value="${escape(v)}" ${v===value?'selected':''}>${escape(label)}</option>`).join('');}
function button(text,action){return `<button type="button" data-ble-out="${action}">${escape(text)}</button>`;}
function html(){
const ble=nativeState(),radioOn=ble.radioOn!==false;
const connected=Boolean(radioOn&&ble.connected),enabled=Boolean(connected&&ble.authorized);
const phase=ble.phase==='permission'?'permission':(radioOn?(ble.phase||'disconnected'):'off');
const phaseLabels={permission:'Permission required',off:'Off',disconnected:'Disconnected',scanning:'Scanning…',connecting:'Connecting…',connected:'Connected',pairing:'Waiting for BOOT…',authorizing:'Authorizing…',authorized:'Authorized',reading:'Reading…',saving:'Saving…'};
const phaseText=phaseLabels[phase]||'Disconnected';
const working=['scanning','connecting','pairing','authorizing','reading','saving'].includes(phase);
const enrollmentNeeded=/not paired|enrollment refused|hold BOOT/i.test(message+' '+(ble.message||''));
const health = !radioOn || phase==='permission' || (!connected && !working) ? 'red'
  : enabled && !working ? 'green' : 'amber';
const chipText = !radioOn ? 'Bluetooth off' : phase==='permission' ? 'Permission needed'
  : working ? phaseText : enabled ? 'Connected' : connected ? 'Authorize' : 'Disconnected';
const chipIcon = '<svg viewBox="0 0 24 24" width="12" height="12" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="m7 7 10 10-5 4V3l5 4L7 17"/></svg>';

const output=draft[selected];const sum=output?output.zones.reduce((s,z)=>s+Number(z.pixels||0),0):0;
return `<div class="ot-bom-head"><div><h2>CORE Outputs</h2><small>Device-saved zones · two physical channels</small></div><span class="ot-bom-pill" data-health="${health}" role="status" aria-live="polite">${chipIcon}<span>${escape(chipText)}</span></span></div>
 <p class="ot-bom-feedback" role="status" aria-live="polite">${escape(message)}</p>
 <div class="ot-bom-row">${button('Find BLE','scan')}${button(enrollmentNeeded?'Hold BOOT + Authorize':'Authorize Controller','authorize')}${button('Reload Output','reload')}${button('Legacy Wi-Fi auth','wifi')}</div>
 <p class="ot-bom-note">Initial bench enrollment may require holding BOOT. Later connections use saved authorization.</p>
 <div class="ot-bom-row">${[0,1].map(i=>`<button data-ble-out="select" data-index="${i}" class="${selected===i?'active':''}">${escape(draft[i]?.name||`Output ${i+1}`)} · GPIO${i?17:16}</button>`).join('')}</div>
 ${output?`<div class="ot-bom-fields">
 <label>Output name<input data-out="name" maxlength="24" value="${escape(output.name)}"></label>
 <label>Active pixels<input data-out="pixels" type="number" min="1" max="522" value="${output.pixels}"></label>
 <label>Color order<select data-out="color_order">${options(orders.map(o=>[o,o]),output.color_order)}</select></label></div>
 <h3>Zones (${output.zones.length}/8)</h3><div class="ot-zone-overview" aria-label="Draft zone proportions">${output.zones.map((z,i)=>`<div class="ot-zone-track" style="flex:${Math.max(0,Number(z.pixels)||0)}" title="Zone ${i+1}: ${Number(z.pixels)||0} pixels"></div>`).join('')}</div><p class="ot-bom-note">Draft pixel proportions only — not a physical LED map.</p>
 <div class="ot-bom-zones">${output.zones.map((z,i)=>`<div class="ot-bom-zone"><div class="ot-bom-zone-title"><strong>Zone ${i+1}</strong><button type="button" data-ble-out="remove" data-index="${i}" ${output.zones.length<=1?'disabled':''}>Remove</button></div><div class="ot-bom-fields"><label>Name<input data-zone="${i}" data-key="name" maxlength="24" value="${escape(z.name)}"></label><label>Pixels<input data-zone="${i}" data-key="pixels" type="number" min="0" max="522" value="${z.pixels}"></label><label>Lighting role<select data-zone="${i}" data-key="role">${options(roles,z.role)}</select></label></div></div>`).join('')}</div>
 <div class="ot-bom-row">${button('Add zone','add')}${button('Save to CORE over BLE','save')}</div>
 <p class="ot-bom-total ${sum===Number(output.pixels)?'':'bad'}">${sum} assigned / ${output.pixels} active pixels${sum===Number(output.pixels)?'':' — must match'}</p>`:
 '<p class="ot-bom-note">Load an output to view its saved zones and trigger roles.</p>'}
 <p class="ot-bom-note" aria-label="BLE readback diagnostics">GPIO${selected?17:16} · ${busy?'Working':'Idle'} · Verified read: ${escape(lastReadAt)}</p>
 <p class="ot-bom-note">Bench only. Stop shows and release trigger inputs before saving. A saved readback confirms configuration, not physical LED behavior.</p>`;
}
const css=document.createElement('style');css.textContent=`
.ot-bom{padding:16px;margin:12px 0;border:1px solid #292929;border-radius:20px;background:#080808;color:#f7f7f7}
.ot-bom *{box-sizing:border-box}.ot-bom-head{display:flex;justify-content:space-between;align-items:center;gap:12px;flex-wrap:wrap}.ot-bom h2{margin:0;font-size:18px;letter-spacing:-.02em}.ot-bom h3{font-size:14px;margin:19px 0 9px}
.ot-bom-head small,.ot-bom-note{font-size:12px;color:#999;line-height:1.5}.ot-bom-pill{display:inline-flex;align-items:center;gap:5px;font-size:11px;font-weight:650;white-space:nowrap;padding:6px 10px;border-radius:999px;border:1px solid #343434;color:#aaa;background:#101010}.ot-bom-pill svg{width:12px;height:12px;flex-shrink:0;color:currentColor}
.ot-bom-pill[data-health="red"]{color:#ff6273}.ot-bom-pill[data-health="amber"]{color:#f6bd47}.ot-bom-pill[data-health="green"]{color:#32d984}
.ot-bom-feedback{font-size:12px;color:#ddd;background:#0b0b0b;border:1px solid #292929;border-radius:13px;padding:10px 12px;line-height:1.45}
.ot-bom-row{display:flex;flex-wrap:wrap;gap:8px;margin:13px 0}.ot-bom button{color:#f3f3f3;border-radius:999px;border:1px solid #393939;background:#1b1b1b;font-weight:600;font-size:12px;min-height:43px;padding:8px 13px}.ot-bom button.active{background:#292929;border-color:#656565;color:#fff}.ot-bom button:disabled{opacity:.42}
.ot-bom-fields{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}.ot-bom-fields label{font-size:12px;color:#d5d5d5;display:flex;flex-direction:column;gap:7px;min-width:0}.ot-bom-fields input,.ot-bom-fields select{width:100%;min-height:43px;padding:8px 11px;color:#fff;border-radius:14px;border:1px solid #363636;background:#121212;font:inherit}
.ot-zone-overview{display:flex;gap:3px;height:13px;border-radius:999px;overflow:hidden;background:#141414;margin:10px 0}.ot-zone-track{min-width:0;background:#626262}.ot-zone-track:nth-child(even){background:#929292}
.ot-bom-zone{padding:13px;border:1px solid #2b2b2b;background:#090909;border-radius:16px;margin-bottom:10px}.ot-bom-zone-title{display:flex;justify-content:space-between;align-items:center;margin-bottom:10px}.ot-bom-zone-title button{min-height:32px;padding:5px 11px}.ot-bom-total{font-size:12px;color:#d7d7d7}.ot-bom-total.bad{color:#f6bd47}
[data-ot-theme=light] .ot-bom{background:#fff;color:#111;border-color:#dedede}[data-ot-theme=light] .ot-bom-head small,[data-ot-theme=light] .ot-bom-note{color:#555}[data-ot-theme=light] .ot-bom-feedback,[data-ot-theme=light] .ot-bom-zone{background:#fafafa;color:#222;border-color:#ddd}[data-ot-theme=light] .ot-bom-fields label{color:#333}[data-ot-theme=light] .ot-bom-fields input,[data-ot-theme=light] .ot-bom-fields select{color:#111;background:#fff;border-color:#ccc}
[data-ot-theme=light] .ot-bom button{color:#111;background:#f4f4f4;border-color:#d3d3d3}[data-ot-theme=light] .ot-bom button.active{color:#111;background:#e3e3e3;border-color:#aaa}[data-ot-theme=light] .ot-bom-pill{background:#fafafa;border-color:#ddd}
@media(max-width:390px){.ot-bom-fields{grid-template-columns:1fr}}
`;document.head.appendChild(css);
function paint(){const node=document.getElementById('ot-ble-output-card');if(node)node.innerHTML=html();}
function mount(){
if(!isLayout())return;
if(document.getElementById('ot-ble-output-card'))return;
const main=document.querySelector('#app main');if(!main)return;
const node=document.createElement('section');node.id='ot-ble-output-card';node.className='ot-bom';
// The approved Layout editor is the primary surface. BLE service controls
// follow it instead of pushing the actual Layout card to the bottom.
main.append(node);
paint();
}
function announceSelection(){document.dispatchEvent(new CustomEvent('opentail:ble-output-selected',{detail:{index:selected}}));}
function load(index){if(busy)return;if(index!==0&&index!==1)return;pendingIndex=index;busy=true;message='Reading output '+(index+1)+' over BLE…';paint();window.OpenTailNativeBLE.readOutput(index);}
window.OpenTailBLEOutputs={
 getSelectedIndex(){return selected;},
 // Only controller readback/verified saves are shared across the app.
 getConfirmedOutputs(){return saved.map(o=>o?JSON.parse(JSON.stringify(o)):null);},
 connectionChanged(status){
  const st=nativeState();
  message=st.radioOn===false?'Bluetooth switched off · disconnected':(status||st.message||'BLE status updated');
  if (st.radioOn===false || !st.connected){busy=false;pendingIndex=null;}
  mount();paint();
 },onResult(raw){
  let r;try{r=JSON.parse(raw);}catch{return;}
  // Ignore late callbacks after disconnect or completion; only a pending request owns a result.
  if(pendingIndex===null)return;
  if(r.index!==pendingIndex){message='BLE response index mismatch; no configuration applied. Reload the selected output.';busy=false;pendingIndex=null;paint();return;}
  busy=false;pendingIndex=null;message=r.message||'BLE operation complete';
  if(r.ok&&r.output&&(r.index===0||r.index===1)){
   lastReadAt=new Date().toLocaleTimeString();
   saved[r.index]=JSON.parse(JSON.stringify(r.output));
   draft[r.index]=JSON.parse(JSON.stringify(r.output));
   document.dispatchEvent(new CustomEvent('opentail:ble-outputs-updated',{detail:{index:r.index}}));
  }
  paint();
 },refresh(){mount();paint();}
};
document.addEventListener('click',e=>{
const b=e.target.closest('#ot-ble-output-card [data-ble-out]');if(!b)return;
const action=b.dataset.bleOut;
if(action==='scan')window.OpenTailNativeBLE.scan();
else if(action==='authorize'){
 const st=nativeState(),needsBoot=/not paired|enrollment refused|hold BOOT/i.test(message+' '+(st.message||''));
 if(needsBoot)window.OpenTailNativeBLE.pairWithBoot();
 else window.OpenTailNativeBLE.authorize();
}
else if(action==='wifi')window.OpenTailNativeBLE.authorizeWifiLegacy();
else if(action==='select'){if(busy){message='Wait for the current BLE operation before switching outputs';paint();return;}const next=Number(b.dataset.index);if(next!==0&&next!==1)return;selected=next;paint();announceSelection();load(selected);}
else if(action==='reload')load(selected);
else if(action==='add'){if(!draft[selected]||draft[selected].zones.length>=8||busy)return;draft[selected].zones.push({name:'Zone '+(draft[selected].zones.length+1),pixels:0,role:'decorative'});message='Unsaved draft — adjust pixel totals before saving';paint();}
else if(action==='remove'){if(!draft[selected]||draft[selected].zones.length<=1||busy)return;draft[selected].zones.splice(Number(b.dataset.index),1);message='Zone removed from unsaved draft';paint();}
else if(action==='save'){
 if(busy||!draft[selected])return;
 const o=draft[selected],sum=o.zones.reduce((n,z)=>n+Number(z.pixels||0),0);
 if(sum!==Number(o.pixels)||o.pixels<1||o.pixels>522){message='Zone pixel totals must equal active output count (1–522)';paint();return;}
 busy=true;pendingIndex=selected;message='Writing, committing, and verifying saved output over BLE…';paint();
 window.OpenTailNativeBLE.saveOutput(selected,JSON.stringify(o));
}
});
document.addEventListener('change',e=>{
const node=e.target.closest('#ot-ble-output-card [data-out],#ot-ble-output-card [data-zone]');if(!node||busy||!draft[selected])return;
const o=draft[selected];
if(node.dataset.out){const key=node.dataset.out;o[key]=key==='pixels'?Number(node.value):node.value;}
else{const i=Number(node.dataset.zone),key=node.dataset.key;if(!o.zones[i])return;o.zones[i][key]=key==='pixels'?Number(node.value):node.value;}
message='Unsaved BLE draft';paint();
});
let pending=false;
new MutationObserver(()=>{if(pending)return;pending=true;requestAnimationFrame(()=>{pending=false;mount();});}).observe(document.body,{childList:true,subtree:true});
mount();
})();
