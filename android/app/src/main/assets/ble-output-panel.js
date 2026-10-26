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
let selected=0,busy=false,pendingIndex=null,message='Connect BLE, pair once using BOOT, then load device outputs.',shown=false;
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
const health = !radioOn || phase==='permission' || (!connected && !working) ? 'red'
  : enabled && !working ? 'green' : 'amber';
const chipText = !radioOn ? 'Bluetooth off' : phase==='permission' ? 'Permission needed'
  : working ? phaseText : enabled ? 'Connected' : connected ? 'Authorize' : 'Disconnected';
const chipIcon = '<svg viewBox="0 0 24 24" width="12" height="12" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="m7 7 10 10-5 4V3l5 4L7 17"/></svg>';

const output=draft[selected];const sum=output?output.zones.reduce((s,z)=>s+Number(z.pixels||0),0):0;
return `<div class="ot-bom-head"><div><h2>Bluetooth Output Manager</h2><small>2 physical outputs · saved on CORE · bench only</small></div><span class="ot-bom-pill" data-health="${health}" role="status" aria-live="polite">${chipIcon}<span>${escape(chipText)}</span></span></div>
 <div class="ot-bom-health" data-health="${health}" aria-label="Current Bluetooth status">
   <div><span>Bluetooth</span><strong>${phase==='permission'?'Permission needed':radioOn?'On':'Off'}</strong></div>
   <div><span>Connection</span><strong>${connected && ['connected','authorized'].includes(phase)?'Connected':phaseText}</strong></div>
   <div><span>Authorization</span><strong>${enabled?'Authorized':connected?'Required':'Not connected'}</strong></div>
 </div>
 <p class="ot-bom-feedback" role="status" aria-live="polite">${escape(message)}</p>
 <div class="ot-bom-row">${button('Find BLE','scan')}${button('Pair with BOOT','pair')}${button('Authorize','authorize')}${button('Reload Output','reload')}${button('Legacy Wi-Fi auth','wifi')}</div>
 <p class="ot-bom-note">BLE-only setup: connect, then hold the powered-on CORE BOOT button while tapping Pair with BOOT. Future BLE reconnects authenticate automatically. Optional legacy Wi-Fi authorization remains available. This is bench enrollment, not production-grade anti-MITM pairing.</p>
 <div class="ot-bom-row">${[0,1].map(i=>`<button data-ble-out="select" data-index="${i}" class="${selected===i?'active':''}">${escape(draft[i]?.name||`Output ${i+1}`)} · GPIO${i?17:16}</button>`).join('')}</div>
 ${output?`<div class="ot-bom-fields">
 <label>Output name<input data-out="name" maxlength="24" value="${escape(output.name)}"></label>
 <label>Active pixels<input data-out="pixels" type="number" min="1" max="522" value="${output.pixels}"></label>
 <label>Color order<select data-out="color_order">${options(orders.map(o=>[o,o]),output.color_order)}</select></label></div>
 <h3>Zones (${output.zones.length}/8)</h3>
 <div class="ot-bom-zones">${output.zones.map((z,i)=>`<div class="ot-bom-zone"><div class="ot-bom-zone-title"><strong>Zone ${i+1}</strong><button type="button" data-ble-out="remove" data-index="${i}" ${output.zones.length<=1?'disabled':''}>Remove</button></div><div class="ot-bom-fields"><label>Name<input data-zone="${i}" data-key="name" maxlength="24" value="${escape(z.name)}"></label><label>Pixels<input data-zone="${i}" data-key="pixels" type="number" min="0" max="522" value="${z.pixels}"></label><label>Lighting role<select data-zone="${i}" data-key="role">${options(roles,z.role)}</select></label></div></div>`).join('')}</div>
 <div class="ot-bom-row">${button('Add zone','add')}${button('Save to CORE over BLE','save')}</div>
 <p class="ot-bom-total ${sum===Number(output.pixels)?'':'bad'}">${sum} assigned / ${output.pixels} active pixels${sum===Number(output.pixels)?'':' — must match'}</p>`:
 '<p class="ot-bom-note">Load an output to view its saved zones and trigger roles.</p>'}
 <p class="ot-bom-status" role="status" aria-live="polite">${escape(message)}</p>
 <p class="ot-bom-note">No physical safety or failover features are implemented. Stop shows and release all 12V triggers before saving. Reload after a save error; the draft is retained.</p>`;
}
const css=document.createElement('style');css.textContent=`
.ot-bom{padding:16px;margin:12px 0;border:1px solid #37443d;border-radius:17px;background:#0b0c0b;color:#f6f8f6}
.ot-bom *{box-sizing:border-box}.ot-bom-head{display:flex;justify-content:space-between;gap:12px;flex-wrap:wrap}.ot-bom h2{margin:0;font-size:18px}.ot-bom h3{font-size:14px;margin:19px 0 9px}
.ot-bom-head small,.ot-bom-note{font-size:12px;color:#a7ada8;line-height:1.5}.ot-bom-pill{box-sizing:border-box;display:inline-flex;align-items:center;gap:4px;font-size:11px;font-weight:650;line-height:1.1;white-space:nowrap;padding:4px 8px;border-radius:99px;border:1px solid #174c2e;color:#20d46c;background:#071a10;align-self:center}.ot-bom-pill svg{width:12px;height:12px;flex-shrink:0;color:currentColor}
.ot-bom-pill[data-health="red"],.ot-bom-health[data-health="red"] strong{color:#ff5265!important}
.ot-bom-pill[data-health="amber"],.ot-bom-health[data-health="amber"] strong{color:#f6bd47!important}
.ot-bom-pill[data-health="green"],.ot-bom-health[data-health="green"] strong{color:#20d46c!important}
.ot-bom-pill[data-health="red"]{border-color:#63303a;background:#200c11}
.ot-bom-pill[data-health="amber"]{border-color:#57431a;background:#201808}
.ot-bom-pill[data-health="green"]{border-color:#174c2e;background:#071a10}
.ot-bom-health{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:7px;margin:14px 0}
.ot-bom-health>div{border:1px solid #303a33;border-radius:10px;padding:10px 7px;background:#121512;min-width:0}
.ot-bom-health span{display:block;font-size:10px;color:#a6aea8}
.ot-bom-health strong{display:block;margin-top:4px;font-size:11px;color:#edf9ef;overflow-wrap:anywhere}
.ot-bom-feedback{font-size:12px;color:#c9e8d2;background:#111c14;border:1px solid #2e4937;border-radius:10px;padding:11px;line-height:1.4}
.ot-bom-row{display:flex;flex-wrap:wrap;gap:8px;margin:14px 0}.ot-bom button{color:#f3faf5;border-radius:10px;border:1px solid #59635c;background:#212923;font-weight:600;font-size:12px;min-height:40px;padding:7px 12px}.ot-bom button.active{background:#246d46;border-color:#246d46}.ot-bom button:disabled{opacity:.4}
.ot-bom-fields{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:9px}.ot-bom-fields label{font-size:11px;color:#cdd6d0;display:flex;flex-direction:column;gap:5px;min-width:0}.ot-bom-fields input,.ot-bom-fields select{width:100%;min-height:41px;padding:6px 9px;color:#fff;border-radius:9px;border:1px solid #505950;background:#151a17;font:inherit}
.ot-bom-zone{padding:12px;border:1px solid #333e36;border-radius:11px;margin-bottom:9px}.ot-bom-zone-title{display:flex;justify-content:space-between;align-items:center;margin-bottom:9px}.ot-bom-zone-title button{min-height:30px;padding:3px 8px}.ot-bom-status{font-size:12px;color:#aedfc2;min-height:20px}.ot-bom-total{font-size:12px;color:#abe5c0}.ot-bom-total.bad{color:#ffc48e}
[data-ot-theme=light] .ot-bom{background:white;color:#17251d;border-color:#d2dad5}[data-ot-theme=light] .ot-bom-fields input,[data-ot-theme=light] .ot-bom-fields select{color:#152b1d;background:#f8faf9;border-color:#c9d1cb}
[data-ot-theme=light] .ot-bom button{color:#162a1e;background:#edf2ef;border-color:#cbd7cd}[data-ot-theme=light] .ot-bom button.active{color:#fff;background:#236a40}
@media(max-width:370px){.ot-bom-fields{grid-template-columns:1fr}}
`;document.head.appendChild(css);
function paint(){const node=document.getElementById('ot-ble-output-card');if(node)node.innerHTML=html();}
function mount(){
if(!isLayout())return;
if(document.getElementById('ot-ble-output-card'))return;
const main=document.querySelector('#app main');if(!main)return;
const node=document.createElement('section');node.id='ot-ble-output-card';node.className='ot-bom';
const existing=main.querySelector('.ot-om-card');if(existing)existing.before(node);else main.prepend(node);
paint();
}
function announceSelection(){document.dispatchEvent(new CustomEvent('opentail:ble-output-selected',{detail:{index:selected}}));}
function load(index){if(busy)return;if(index!==0&&index!==1)return;pendingIndex=index;busy=true;message='Reading output '+(index+1)+' over BLE…';paint();window.OpenTailNativeBLE.readOutput(index);}
window.OpenTailBLEOutputs={
 getSelectedIndex(){return selected;},
 connectionChanged(status){
  const st=nativeState();
  message=st.radioOn===false?'Bluetooth switched off · disconnected':(status||st.message||'BLE status updated');
  if (st.radioOn===false || !st.connected){busy=false;pendingIndex=null;}
  mount();paint();
 },onResult(raw){
  let r;try{r=JSON.parse(raw);}catch{return;}
  if(pendingIndex!==null&&r.index!==pendingIndex){message='BLE response index mismatch; no configuration applied. Reload the selected output.';busy=false;pendingIndex=null;paint();return;}
  busy=false;pendingIndex=null;message=r.message||'BLE operation complete';
  if(r.ok&&r.output&&(r.index===0||r.index===1)){saved[r.index]=r.output;draft[r.index]=JSON.parse(JSON.stringify(r.output));}
  paint();
 },refresh(){mount();paint();}
};
document.addEventListener('click',e=>{
const b=e.target.closest('#ot-ble-output-card [data-ble-out]');if(!b)return;
const action=b.dataset.bleOut;
if(action==='scan')window.OpenTailNativeBLE.scan();
else if(action==='authorize')window.OpenTailNativeBLE.authorize();
else if(action==='pair')window.OpenTailNativeBLE.pairWithBoot();
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
