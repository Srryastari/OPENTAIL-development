/* OpenTail shared interaction motion. Presentation only; never delays BLE or triggers. */
(()=>{'use strict';if(document.getElementById('ot-motion-system'))return;
const style=document.createElement('style');style.id='ot-motion-system';
style.textContent=`
:root{--ot-motion-fast:160ms;--ot-motion-normal:210ms;--ot-motion-ease:cubic-bezier(.22,1,.36,1)}
/* Disable WebView's blue touch flash and accidental selection on controls, not editable fields. */
button,[role="button"],[role="tab"],[role="radio"],.ot-mode-tabs,.ot-output-target label,.ot-connection-card,.ot-quick-action{ -webkit-tap-highlight-color:transparent; -webkit-user-select:none;user-select:none;touch-action:manipulation; }
input,textarea,[contenteditable="true"],[role="textbox"]{ -webkit-user-select:text;user-select:text; }
/* Android WebView sliders: remove native blue touch/selection overlay without blocking drag gestures. */
input[type="range"],input[type="range"]::-webkit-slider-thumb,input[type="range"]::-webkit-slider-runnable-track{ -webkit-tap-highlight-color:transparent!important; -webkit-touch-callout:none; -webkit-user-select:none;user-select:none; }
input[type="range"]{touch-action:pan-y;}
input[type="range"]:focus:not(:focus-visible){outline:none;}
input[type="range"]:focus-visible{outline:2px solid #48d88d;outline-offset:3px;}

.ot-mode-tabs button{position:relative;z-index:1;}
.ot-native-nav button,.ot-bom button,.ot-simple-button,.ot-quick-action,.ot-connection-card,#ot-direct-ble-dashboard button{transition:background-color var(--ot-motion-normal) var(--ot-motion-ease),border-color var(--ot-motion-normal) var(--ot-motion-ease),color var(--ot-motion-normal) var(--ot-motion-ease),box-shadow var(--ot-motion-normal) var(--ot-motion-ease),transform var(--ot-motion-fast) var(--ot-motion-ease)}
.ot-native-nav button:active,.ot-bom button:active,#ot-direct-ble-dashboard button:active{transform:scale(.975)}
.ot-bom-pill,.ot-bom-health strong,#ot-direct-state{transition:color var(--ot-motion-normal) ease,background-color var(--ot-motion-normal) ease,border-color var(--ot-motion-normal) ease}
.ot-mode-tabs{position:relative;display:grid;grid-template-columns:repeat(3,minmax(0,1fr));padding:4px;background:#131515;border-radius:12px;isolation:isolate}
.ot-mode-slider{position:absolute;z-index:0;pointer-events:none;top:4px;bottom:4px;left:4px;width:calc((100% - 8px)/3);border-radius:9px;background:#3a3e3b;transform:translateX(200%);transition:transform var(--ot-motion-normal) var(--ot-motion-ease)}
.ot-mode-tabs button{min-height:40px;padding:4px 3px;border:0;background:transparent;color:#bbb;font-size:11px;font-weight:650;border-radius:9px}
.ot-mode-tabs button[aria-pressed=true]{color:#fff}
@media(prefers-reduced-motion:reduce){:root{--ot-motion-fast:0ms;--ot-motion-normal:0ms}.ot-native-nav button,.ot-bom button,.ot-simple-button,.ot-quick-action,.ot-connection-card,#ot-direct-ble-dashboard button,.ot-mode-slider{transition:none!important}}
`;document.head.appendChild(style);
// Output selection is now owned solely by the working BLE Output Manager.
// The former "Independent / Linked / Sync + Override" preview card was an
// inert development mock and falsely suggested that linked writes existed.
})();
