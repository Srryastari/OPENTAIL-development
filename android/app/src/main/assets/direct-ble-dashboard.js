/* OpenTail Android v0.1.16 — direct BLE dashboard entry, no CORE firmware change.
 * Existing GATT, owner-key enrollment and saved configuration remain authoritative.
 */
(function () {
  'use strict';
  if (window.__otDirectBleDashboard || !window.OpenTailNativeApp || !window.OpenTailShell) return;
  window.__otDirectBleDashboard = true;
  const card = document.createElement('section');
  card.id = 'ot-direct-ble-dashboard';
  card.setAttribute('aria-label', 'Controller connection');
  card.innerHTML = `
    <div class="ot-direct-heading"><strong>CORE Controller</strong><span id="ot-direct-state" role="status" aria-live="polite">Disconnected</span></div>
    <p id="ot-direct-detail">Connect over Bluetooth without changing your Wi-Fi.</p>
    <button id="ot-direct-connect" class="ot-direct-primary" type="button">Connect to CORE</button>
    <div class="ot-direct-actions">
      <button id="ot-direct-outputs" type="button">Outputs &amp; Zones</button>
    </div>
    <p class="ot-direct-help">First-time bench enrollment may still require holding CORE BOOT while tapping Authorize. Trusted phones reconnect automatically. A no-BOOT production pairing flow is not implemented yet.</p>`;
  const css=document.createElement('style');
  css.textContent=`
    #ot-direct-ble-dashboard{display:none;box-sizing:border-box;width:calc(100% - 28px);max-width:692px;margin:10px auto 5px;padding:15px 16px;border:1px solid #353535;border-radius:19px;background:#080808;color:#f6f6f6}
    body.ot-home-active:not(.ot-settings-active) #ot-direct-ble-dashboard{display:block}
    .ot-direct-heading{display:flex;align-items:center;justify-content:space-between;gap:12px}
    .ot-direct-heading strong{font-size:16px}
    #ot-direct-state{font-size:11px;color:#bcbcbc;text-align:right}
    #ot-direct-ble-dashboard p{font-size:12px;line-height:1.45;color:#aaa;margin:9px 0 13px}
    #ot-direct-ble-dashboard button{min-height:46px;border-radius:13px;font-size:13px;font-weight:650;cursor:pointer}
    #ot-direct-connect{width:100%;border:1px solid #eee;background:#f4f4f4;color:#111}
    .ot-direct-actions{display:grid;grid-template-columns:1fr 1fr;gap:9px;margin-top:9px}
    .ot-direct-actions button{border:1px solid #404040;background:#101010;color:#e7e7e7}
    #ot-direct-ble-dashboard [hidden]{display:none!important}
    #ot-direct-ble-dashboard .ot-direct-help{font-size:10px;color:#888;margin:10px 0 0}
    #ot-direct-ble-dashboard button:focus-visible{outline:2px solid #48d88d;outline-offset:2px}
    html[data-ot-theme=light] #ot-direct-ble-dashboard{background:#fff;color:#152018;border-color:#d4dfd6}
    html[data-ot-theme=light] .ot-direct-actions button{background:#f6faf7;color:#1c3224;border-color:#c8d7cc}
    @media(max-width:370px){#ot-direct-ble-dashboard{margin:7px auto;padding:12px}.ot-direct-actions{gap:6px}}
  `;
  document.head.appendChild(css);
  const header=document.querySelector('.ot-mobile-header');
  if (!header) return;
  // Never insert Home content between the header and the six-tab navigator.
  const nav=document.querySelector('.ot-native-nav');
  (nav||header).insertAdjacentElement('afterend',card);
  const read=()=>{try{return JSON.parse(window.OpenTailNativeApp.getState()||'{}')}catch(_){return {}}};
  const connect=card.querySelector('#ot-direct-connect');
  const outputs=card.querySelector('#ot-direct-outputs');
  function refresh(){
    const s=read(),phase=s.blePhase||'disconnected',connected=Boolean(s.ble&&s.bleRadioOn!==false);
    const authorized=connected&&Boolean(s.bleAuthorized);
    const status=card.querySelector('#ot-direct-state');
    const detail=card.querySelector('#ot-direct-detail');
    const waiting=['scanning','connecting','pairing','authorizing'].includes(phase);
    status.textContent=authorized?'Connected · Authorized':connected?'Connected · Authorization needed':phase==='permission'?'Permission needed':s.bleRadioOn===false?'Bluetooth off':waiting?phase.charAt(0).toUpperCase()+phase.slice(1)+'…':'Disconnected';
    status.style.color=authorized?'#29da8b':connected||waiting?'#f6bd47':'#aaa';
    detail.textContent=authorized?'Ready to manage outputs and zones over BLE.':(s.bleMessage&&s.bleMessage!=='BLE not connected')?s.bleMessage:connected?'Authorization required. If this app was reinstalled, hold BOOT and tap Authorize Controller to enroll again.':phase==='permission'?'Grant nearby-device permission when prompted.':s.bleRadioOn===false?'Turn on Bluetooth to connect.':'Connect over Bluetooth without switching Wi-Fi.';
    connect.textContent=authorized?'Manage Outputs & Zones':waiting?'Connecting…':connected?'Authorize Controller':'Connect to CORE';
    connect.disabled=waiting;
    outputs.hidden=true; // Primary action handles navigation; avoid duplicate Home buttons.

  }
  function actionFeedback(message){card.querySelector('#ot-direct-detail').textContent=message;card.querySelector('#ot-direct-state').textContent='Action requested';}
  connect.addEventListener('click',()=>{
    const s=read();
    if(s.ble&&s.bleAuthorized){outputs.click();return;}
    if(s.ble&&s.bleRadioOn!==false&&!s.bleAuthorized){
      const needsEnrollment=/not paired|enrollment refused|hold BOOT/i.test(s.bleMessage||'');
      if(needsEnrollment&&window.OpenTailNativeBLE?.pairWithBoot){
        actionFeedback('Hold BOOT on the powered CORE while enrolling this phone…');
        window.OpenTailNativeBLE.pairWithBoot();
      }else{actionFeedback('Requesting controller authorization…');window.OpenTailNativeApp.authorizeBle();}
    }
    else {actionFeedback('Requesting Bluetooth permission or opening CORE scanner…');window.OpenTailNativeApp.scanBle();}
  });
  outputs.addEventListener('click',()=>{
    // Reuse existing output editor; no duplicate or unverified configuration path.
    window.OpenTailShell.navigate('layout');
    requestAnimationFrame(()=>{
      const edit=document.querySelector('#ot-compact-pages [data-ot-open="edit"]');
      if(edit)edit.click();
      const manager=document.querySelector('#ot-ble-output-manager, .ot-om-card');
      if(manager)manager.scrollIntoView({block:'start',behavior:'smooth'});
    });
  });
  const oldRefresh=window.OpenTailShell.refresh;
  window.OpenTailShell.refresh=function(){oldRefresh();refresh();};
  refresh();
  document.addEventListener('visibilitychange',()=>{if(!document.hidden)refresh()});
  setInterval(refresh,2000);
})();
