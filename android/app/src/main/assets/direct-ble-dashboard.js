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
    <p class="ot-direct-help">First-time bench enrollment may require holding BOOT when prompted. Trusted phones reconnect automatically.</p>`;
  const css=document.createElement('style');
  css.textContent=`
    #ot-direct-ble-dashboard{display:none;box-sizing:border-box;width:calc(100% - 28px);max-width:692px;margin:10px auto 5px;padding:15px 16px;border:1px solid #353535;border-radius:19px;background:#080808;color:#f6f6f6}
    body.ot-home-active:not(.ot-settings-active) #ot-direct-ble-dashboard{display:block}
    .ot-direct-heading{display:flex;align-items:center;justify-content:space-between;gap:12px}
    .ot-direct-heading strong{font-size:16px}
    #ot-direct-state{font-size:11px;color:#bcbcbc;text-align:right}
    #ot-direct-ble-dashboard p{font-size:12px;line-height:1.45;color:#aaa;margin:9px 0 13px}
    #ot-direct-ble-dashboard button{min-height:46px;border-radius:999px;font-size:13px;font-weight:650;cursor:pointer}
    #ot-direct-connect{width:100%;border:1px solid #505050;background:#212121;color:#f5f5f5}
    #ot-direct-ble-dashboard [hidden]{display:none!important}
    #ot-direct-ble-dashboard .ot-direct-help{font-size:10px;color:#888;margin:10px 0 0}
    #ot-direct-ble-dashboard button:focus-visible{outline:2px solid #48d88d;outline-offset:2px}
    html[data-ot-theme=light] #ot-direct-ble-dashboard{background:#fff;color:#111;border-color:#dedede}
    html[data-ot-theme=light] #ot-direct-connect{background:#f1f1f1;color:#111;border-color:#c9c9c9}
    @media(max-width:370px){#ot-direct-ble-dashboard{margin:7px auto;padding:12px}}
  `;
  document.head.appendChild(css);
  const header=document.querySelector('.ot-mobile-header');
  if (!header) return;
  // Never insert Home content between the header and the six-tab navigator.
  const nav=document.querySelector('.ot-native-nav');
  (nav||header).insertAdjacentElement('afterend',card);
  const read=()=>{try{return JSON.parse(window.OpenTailNativeApp.getState()||'{}')}catch(_){return {}}};
  const connect=card.querySelector('#ot-direct-connect');
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
  }
  function actionFeedback(message){card.querySelector('#ot-direct-detail').textContent=message;card.querySelector('#ot-direct-state').textContent='Action requested';}
  connect.addEventListener('click',()=>{
    const s=read();
    if(s.ble&&s.bleAuthorized){
      window.OpenTailShell.navigate('layout');
      requestAnimationFrame(()=>requestAnimationFrame(()=>{
        const manager=document.getElementById('ot-ble-output-card')||document.querySelector('.ot-om-card');
        if(manager)manager.scrollIntoView({block:'start',behavior:'smooth'});
      }));
      return;
    }
    if(s.ble&&s.bleRadioOn!==false&&!s.bleAuthorized){
      const needsEnrollment=/not paired|enrollment refused|hold BOOT/i.test(s.bleMessage||'');
      if(needsEnrollment&&window.OpenTailNativeBLE?.pairWithBoot){
        actionFeedback('Hold BOOT on the powered CORE while enrolling this phone…');
        window.OpenTailNativeBLE.pairWithBoot();
      }else{actionFeedback('Requesting controller authorization…');window.OpenTailNativeApp.authorizeBle();}
    }
    else {actionFeedback('Requesting Bluetooth permission or opening CORE scanner…');window.OpenTailNativeApp.scanBle();}
  });
  const oldRefresh=window.OpenTailShell.refresh;
  window.OpenTailShell.refresh=function(){oldRefresh();refresh();};
  refresh();
  document.addEventListener('visibilitychange',()=>{if(!document.hidden)refresh()});
  setInterval(refresh,2000);
})();
