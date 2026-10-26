/* OpenTail shared interaction motion. Presentation only; never delays BLE or triggers. */
(()=>{'use strict';if(document.getElementById('ot-motion-system'))return;
const style=document.createElement('style');style.id='ot-motion-system';
style.textContent=`
:root{--ot-motion-fast:160ms;--ot-motion-normal:210ms;--ot-motion-ease:cubic-bezier(.22,1,.36,1)}
/* Disable WebView's blue touch flash and accidental selection on controls, not editable fields. */
button,[role="button"],[role="tab"],[role="radio"],.ot-mode-tabs,.ot-output-target label,.ot-connection-card,.ot-quick-action{ -webkit-tap-highlight-color:transparent; -webkit-user-select:none;user-select:none;touch-action:manipulation; }
input,textarea,[contenteditable="true"],[role="textbox"]{ -webkit-user-select:text;user-select:text; }
.ot-mode-tabs button{position:relative;z-index:1;}
.ot-native-nav button,.ot-bom button,.ot-simple-button,.ot-quick-action,.ot-connection-card,#ot-direct-ble-dashboard button{transition:background-color var(--ot-motion-normal) var(--ot-motion-ease),border-color var(--ot-motion-normal) var(--ot-motion-ease),color var(--ot-motion-normal) var(--ot-motion-ease),box-shadow var(--ot-motion-normal) var(--ot-motion-ease),transform var(--ot-motion-fast) var(--ot-motion-ease)}
.ot-native-nav button:active,.ot-bom button:active,#ot-direct-ble-dashboard button:active{transform:scale(.975)}
.ot-bom-pill,.ot-bom-health strong,#ot-direct-state{transition:color var(--ot-motion-normal) ease,background-color var(--ot-motion-normal) ease,border-color var(--ot-motion-normal) ease}
.ot-output-control{margin:12px 0;padding:14px;border:1px solid #3b3e40;border-radius:14px;background:#242627;color:#f5f5f5}
.ot-output-control h3{margin:0 0 12px;font-size:15px}
.ot-mode-tabs{position:relative;display:grid;grid-template-columns:repeat(3,minmax(0,1fr));padding:4px;background:#131515;border-radius:12px;isolation:isolate}
.ot-mode-slider{position:absolute;z-index:0;pointer-events:none;top:4px;bottom:4px;left:4px;width:calc((100% - 8px)/3);border-radius:9px;background:#3a3e3b;transform:translateX(200%);transition:transform var(--ot-motion-normal) var(--ot-motion-ease)}
.ot-mode-tabs button{min-height:40px;padding:4px 3px;border:0;background:transparent;color:#bbb;font-size:11px;font-weight:650;border-radius:9px}
.ot-mode-tabs button[aria-pressed=true]{color:#fff}
.ot-mode-description{min-height:34px;margin:10px 0;color:#aeb6b0;font-size:12px;transition:opacity var(--ot-motion-fast) ease}
.ot-output-target{display:grid;gap:10px;margin:12px 0}
.ot-output-target label{display:flex;align-items:center;gap:10px;font-size:13px}
.ot-output-status{background:#171a18;border-radius:10px;padding:12px;font-size:12px;display:grid;gap:10px}
.ot-output-status div{display:flex;justify-content:space-between}
.ot-output-status strong{color:#38cf7b}
.ot-output-control .ot-mode-note{font-size:11px;color:#a9aea9;margin:10px 0 0}
@media(prefers-reduced-motion:reduce){:root{--ot-motion-fast:0ms;--ot-motion-normal:0ms}.ot-native-nav button,.ot-bom button,.ot-simple-button,.ot-quick-action,.ot-connection-card,#ot-direct-ble-dashboard button,.ot-mode-slider{transition:none!important}}
`;document.head.appendChild(style);
const descriptions={independent:'Outputs operate separately.',linked:'Edits apply to both selected outputs.',sync:'Shared animations stay synchronized; individual trigger overrides remain active.'};
let mode='sync',target='both';
function selectedFromManager(){const index=window.OpenTailBLEOutputs?.getSelectedIndex?.();return index===1?'two':'one';}
function mount(){const manager=document.getElementById('ot-ble-output-card');if(!manager||document.getElementById('ot-output-control'))return;
const card=document.createElement('section');card.id='ot-output-control';card.className='ot-output-control';
card.innerHTML='<h3>Output Control</h3><div class="ot-mode-tabs" role="group" aria-label="Output control mode"><span class="ot-mode-slider"></span>'+[['independent','Independent'],['linked','Linked'],['sync','Sync + Override']].map(([v,t])=>'<button type="button" data-mode="'+v+'" aria-pressed="'+(v===mode)+'">'+t+'</button>').join('')+'</div><p class="ot-mode-description"></p><hr><strong>Edit target</strong><div class="ot-output-target">'+[['both','Both outputs'],['one','Output 1 · GPIO16'],['two','Output 2 · GPIO17']].map(([v,t])=>'<label><input type="radio" name="ot-output-target" value="'+v+'" '+(v===target?'checked':'')+'>'+t+'</label>').join('')+'</div><div class="ot-output-status"><div><span>Output 1 · GPIO16</span><strong>✓</strong></div><div><span>Output 2 · GPIO17</span><strong>✓</strong></div></div><p class="ot-mode-note">UI preview only — synchronization is not yet active on CORE.</p>';
manager.before(card);if(mode==='independent')target=selectedFromManager();update(card);
card.addEventListener('click',e=>{const b=e.target.closest('[data-mode]');if(!b)return;mode=b.dataset.mode;if(mode!=='independent'){target='both';}else target=selectedFromManager();update(card);});
card.addEventListener('change',e=>{if(e.target.name!=='ot-output-target')return;if(mode==='independent'&&e.target.value==='both'){update(card);return;}target=e.target.value;if(target!=='both'){if(mode==='independent'){const index=target==='two'?1:0;const button=document.querySelector('#ot-ble-output-card [data-ble-out="select"][data-index="'+index+'"]');if(button)button.click();}}update(card);});
}
function update(card){card.querySelector('.ot-mode-slider').style.transform='translateX('+(['independent','linked','sync'].indexOf(mode)*100)+'%)';card.querySelectorAll('input[name="ot-output-target"]').forEach(input=>{input.checked=input.value===target;});card.querySelectorAll('[data-mode]').forEach(b=>b.setAttribute('aria-pressed',String(b.dataset.mode===mode)));card.querySelector('.ot-mode-description').textContent=descriptions[mode];}
document.addEventListener('opentail:ble-output-selected',e=>{const selectedTarget=e.detail.index===1?'two':'one';if(mode==='independent'){target=selectedTarget;const card=document.getElementById('ot-output-control');if(card)update(card);}});
let queued=false;new MutationObserver(()=>{if(queued)return;queued=true;requestAnimationFrame(()=>{queued=false;mount();});}).observe(document.body,{childList:true,subtree:true});mount();
})();