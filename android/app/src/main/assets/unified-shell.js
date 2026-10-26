/* OpenTail Android v0.1.24: single-shell mobile UI. Enhances existing Studio without
   replacing its working CORE APIs, settings storage, trigger logic, or BLE GATT path. */
(function () {
  'use strict';
  if (window.__otUnifiedShell || !window.OpenTailNativeApp) return;
  window.__otUnifiedShell = true;

  const ico = {
    home:'<path d="M3 10 12 3l9 7v10H3z"/><path d="M9 20v-7h6v7"/>',
    shows:'<path d="m12 2 2.2 6.8L21 11l-6.8 2.2L12 20l-2.2-6.8L3 11l6.8-2.2z"/>',
    triggers:'<path d="m13 2-8 11h7l-1 9 8-12h-7z"/>',
    layout:'<rect x="3" y="3" width="7" height="7" rx="1"/><rect x="14" y="3" width="7" height="7" rx="1"/><rect x="3" y="14" width="7" height="7" rx="1"/><rect x="14" y="14" width="7" height="7" rx="1"/>',
    events:'<circle cx="12" cy="12" r="4"/><path d="M12 2v2m0 16v2M2 12h2m16 0h2M4.9 4.9l1.5 1.5m11.2 11.2 1.5 1.5M19.1 4.9l-1.5 1.5M6.4 17.6l-1.5 1.5"/>',
    settings:'<circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.6 1.6 0 0 0 .3 1.8l.1.1-2 2-.1-.1a1.6 1.6 0 0 0-1.8-.3 1.6 1.6 0 0 0-.9 1.5v.2h-3v-.2a1.6 1.6 0 0 0-.9-1.5 1.6 1.6 0 0 0-1.8.3l-.1.1-2-2 .1-.1a1.6 1.6 0 0 0 .3-1.8A1.6 1.6 0 0 0 6 14H5v-3h1a1.6 1.6 0 0 0 1.5-1 1.6 1.6 0 0 0-.3-1.8l-.1-.1 2-2 .1.1a1.6 1.6 0 0 0 1.8.3A1.6 1.6 0 0 0 12 5V4h3v1a1.6 1.6 0 0 0 .9 1.5 1.6 1.6 0 0 0 1.8-.3l.1-.1 2 2-.1.1a1.6 1.6 0 0 0-.3 1.8A1.6 1.6 0 0 0 21 11h.5v3H21a1.6 1.6 0 0 0-1.6 1Z"/>',
    wifi:'<path d="M2 9a15 15 0 0 1 20 0M5 12a10 10 0 0 1 14 0M8 15a5 5 0 0 1 8 0"/><circle cx="12" cy="19" r="1"/>',
    bluetooth:'<path d="m7 7 10 10-5 4V3l5 4L7 17"/>',
    x:'<path d="M18 6 6 18M6 6l12 12"/>',
    arrow:'<path d="m9 18 6-6-6-6"/>',
    info:'<circle cx="12" cy="12" r="10"/><path d="M12 10v7M12 7h.01"/>'
  };
  const icon = (key) => `<svg viewBox="0 0 24 24" width="19" height="19" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${ico[key]}</svg>`;
  const html = document.documentElement;
  const style = document.createElement('style');
  style.id = 'ot-android-unified-css';
  style.textContent = `
  html,html[data-ot-theme],body{background:#080d0a!important;color:#eaf4ee!important;color-scheme:dark!important}
  body{margin:0!important;min-width:0;font-family:Roboto,Inter,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}
  #app>.shell{padding:9px 14px 118px!important;max-width:960px}
  #app .top,#app nav.tabs,#app nav.bottomnav{display:none!important}
  #app .ot-page-intro{margin-top:4px!important}
  body.ot-show-active #app .ot-page-intro{display:none!important}
  .ot-save-warning{color:#ffc0ad;background:#44241b;padding:9px 12px;border-radius:11px;margin-top:10px;font-size:12px;font-weight:650}
  #app .ot-page-intro h1{font-size:21px!important}
  #app .card{border-radius:17px!important}
  #app main{min-width:0}
  .ot-mobile-header{padding:13px 16px 9px;max-width:960px;margin:auto;position:relative;z-index:1}
  .ot-mobile-heading{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:10px}
  .ot-mobile-heading h1{font-size:25px;line-height:1.12;letter-spacing:-.7px;margin:0;font-weight:750;color:#effaf1}
  .ot-mobile-heading p{margin:5px 0 0;font-size:11px;color:#9aa89f}
  .ot-connected-badge{border-radius:100px;background:#162a1d;color:#a9ebbd;padding:8px 11px;border:1px solid #315c3e;font-size:11px;font-weight:700;white-space:nowrap}
  .ot-connected-badge.offline{background:#2a261b;color:#e8cca0;border-color:#514930}
  .ot-connected-badge.limited{color:#e3c78d!important}
  .ot-connected-badge.limited svg{color:#e3c78d!important}
  .ot-connection-grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
  .ot-connection-card{appearance:none;display:flex;align-items:center;gap:9px;text-align:left;border:1px solid #2b3730;background:#151d18;color:#e8f2eb;border-radius:15px;padding:10px 11px;min-height:59px;font:inherit;width:100%;cursor:pointer}
  .ot-connection-card:focus-visible,.ot-native-nav button:focus-visible,.ot-quick-action:focus-visible{outline:2px solid #70e597;outline-offset:2px}
  .ot-connection-card svg{color:#71d995;flex-shrink:0;width:23px;height:23px}
  .ot-connection-card[data-active=false] svg{color:#9ca8a1}
  .ot-connection-card strong{display:block;font-size:13px;line-height:1.3}
  .ot-connection-card small{display:block;color:#a1afa6;font-size:11px;margin-top:3px}
  .ot-native-nav{position:fixed;bottom:0;left:0;right:0;z-index:1050;display:flex;justify-content:space-around;align-items:stretch;gap:3px;background:#0c140f;border-top:1px solid #293a30;padding:8px 6px calc(10px + env(safe-area-inset-bottom,0px));box-shadow:0 -8px 24px #0008}
  .ot-native-nav button{appearance:none;flex:1;min-width:0;min-height:52px;display:flex;flex-direction:column;justify-content:center;align-items:center;gap:4px;border:0;border-radius:12px;background:transparent;color:#99aca0;font:600 10px/1.15 Roboto,Inter,sans-serif;cursor:pointer}
  .ot-native-nav button[aria-current=page]{background:#1b3023;color:#84e5a9}
  .ot-native-nav button svg{width:21px;height:21px}
  .ot-settings-page{display:none;padding:10px 16px 130px;max-width:960px;margin:auto}
  body.ot-settings-active .ot-settings-page{display:block}
  body.ot-settings-active #app{display:none}
  .ot-settings-page h2{font-size:20px;margin:6px 0 7px}
  .ot-settings-page p{font-size:13px;line-height:1.5;color:#a5b1aa;margin:0 0 19px}
  .ot-settings-group{background:#151d18;border:1px solid #2d3c31;border-radius:17px;overflow:hidden;margin-bottom:14px}
  .ot-quick-action{display:flex;width:100%;background:transparent;color:#e2eee5;text-align:left;padding:17px;border:0;border-bottom:1px solid #2c3a30;font:inherit;align-items:center;justify-content:space-between;gap:12px;min-height:57px;cursor:pointer}
  .ot-quick-action:last-child{border-bottom:0}.ot-quick-action svg{color:#87d99d}
  .ot-settings-help{font-size:12px!important;line-height:1.6!important;padding:2px 3px}
  #ot-ble-inline{margin:7px 0 16px!important}
  #ot-ble-inline .ot-ble-head h2{font-size:19px!important}
  #ot-ble-inline .ot-ble-head p{font-size:11px!important}
  #ot-ble-inline .ot-ble-fields{gap:11px!important}
  #ot-ble-inline .ot-ble-actions button{min-height:50px!important}
  html[data-ot-theme="light"],html[data-ot-theme="light"] body{background:#f6faf7!important;color:#203025!important;color-scheme:light!important}
  html[data-ot-theme="light"] .ot-mobile-heading h1{color:#15261a}
  html[data-ot-theme="light"] .ot-mobile-heading p{color:#5b7061}
  html[data-ot-theme="light"] .ot-connected-badge{background:#e3f4e8;color:#2c7443;border-color:#b8d8c1}
  html[data-ot-theme="light"] .ot-connection-card,html[data-ot-theme="light"] .ot-settings-group{background:#ffffff;color:#17301d;border-color:#d6e3d8}
  html[data-ot-theme="light"] .ot-connection-card small,html[data-ot-theme="light"] .ot-settings-page p{color:#566b5a}
  html[data-ot-theme="light"] .ot-native-nav{background:#f8fbf8;border-color:#cdded0}
  html[data-ot-theme="light"] .ot-native-nav button{color:#526b58}
  html[data-ot-theme="light"] .ot-native-nav button[aria-current=page]{background:#dff4e5;color:#246b3c}
  html[data-ot-theme="light"] .ot-quick-action{color:#23442d;border-color:#d6e3d8}
    @media(min-width:700px){.ot-mobile-header,.ot-settings-page{padding-left:30px;padding-right:30px}#app>.shell{padding-left:30px!important;padding-right:30px!important}.ot-native-nav{max-width:960px;margin:auto;left:0;right:0;border-left:1px solid #293a30;border-right:1px solid #293a30}}
  @media(max-width:350px){.ot-connected-badge{padding:7px}.ot-connection-card{padding:10px 7px;gap:5px}.ot-mobile-heading h1{font-size:22px}}
  `;
  style.textContent += "\n  .ot-settings-page .ot-settings-group>summary{\n    display:flex;align-items:center;justify-content:space-between;\n    padding:15px 16px;min-height:48px;color:#ececec;font:600 14px/1.2 inherit;\n    cursor:pointer;list-style:none;\n  }\n  .ot-settings-page .ot-settings-group>summary::-webkit-details-marker{display:none}\n  .ot-settings-page .ot-settings-group>summary svg{width:15px;height:15px;color:#999;transform:rotate(90deg)}\n  .ot-settings-page .ot-settings-group[open]>summary svg{transform:rotate(-90deg)}\n  .ot-settings-page .ot-settings-copy{font-size:12px;line-height:1.5;padding:0 16px 12px;margin:0;color:#aaa}\n  .ot-settings-page .ot-quick-action{min-height:48px;padding:13px 16px;font-size:13px;background:#111}\n  .ot-settings-page input[type=file]{max-width:150px;font-size:11px;color:#aaa}\n";
  // LOCKED OCT-26: one TOP segmented navigation; OLED-black surfaces, no fixed bottom bar.
  // The existing native BLE bridge and controls remain untouched.
  style.textContent += `
    html:not([data-ot-theme="light"]),html:not([data-ot-theme="light"]) body{background:#000!important;color:#f4f4f4!important}
    html:not([data-ot-theme="light"]) .ot-mobile-header{background:#000!important}
    html:not([data-ot-theme="light"]) .ot-connection-card,
    html:not([data-ot-theme="light"]) .ot-settings-group,
    html:not([data-ot-theme="light"]) #ot-ble-inline,
    html:not([data-ot-theme="light"]) #ot-compact-pages .ot-simple-card{background:#080808!important;border-color:#252525!important;color:#f3f3f3!important}
    html:not([data-ot-theme="light"]) .ot-connection-card strong,
    html:not([data-ot-theme="light"]) .ot-mobile-heading h1{color:#f7f7f7!important}
    html:not([data-ot-theme="light"]) .ot-connection-card small{color:#aaa!important}
    html:not([data-ot-theme="light"]) .ot-connection-card svg{color:#999!important}
    html:not([data-ot-theme="light"]) #ot-ble-inline .ot-ble-fields select,
    html:not([data-ot-theme="light"]) #ot-ble-inline .ot-ble-fields input[type=color]{background:#050505!important;border-color:#333!important}
    html:not([data-ot-theme="light"]) #app .card{background:#080808!important;border-color:#252525!important}
    html:not([data-ot-theme="light"]) #ot-compact-pages .ot-simple-output,
    html:not([data-ot-theme="light"]) #ot-compact-pages .ot-simple-stat{background:#050505!important;border-color:#303030!important}
    .ot-mobile-header{padding-bottom:3px!important}
    .ot-native-nav{position:sticky!important;top:0!important;inset:auto!important;max-width:none!important;width:100%!important;
      display:flex!important;overflow-x:auto!important;overscroll-behavior-x:contain!important;scrollbar-width:none!important;
      gap:3px!important;margin:10px 0 4px!important;padding:4px!important;border:1px solid #252525!important;
      border-radius:15px!important;background:#080808!important;box-shadow:none!important;box-sizing:border-box!important}
    .ot-native-nav::-webkit-scrollbar{display:none!important}
    .ot-native-nav button{flex:1 0 auto!important;min-width:66px!important;min-height:40px!important;
      padding:8px 9px!important;border-radius:11px!important;color:#b0b0b0!important;font:600 11px/1.2 Inter,Roboto,sans-serif!important}
    .ot-native-nav button svg{display:none!important}
    .ot-native-nav button[aria-current=page]{background:#222!important;color:#fff!important}
    #app>.shell{padding-bottom:20px!important}
    #ot-compact-pages{padding-bottom:22px!important}
    body.ot-settings-active .ot-settings-page{padding-bottom:20px!important}
    html[data-ot-theme="light"] .ot-native-nav{background:#f4f4f4!important;border-color:#dedede!important}
    html[data-ot-theme="light"] .ot-native-nav button{color:#555!important}
    html[data-ot-theme="light"] .ot-native-nav button[aria-current=page]{background:#fff!important;color:#111!important}
    .ot-theme-settings{font-size:13px!important}
    @media (max-width:430px){.ot-native-nav{justify-content:flex-start!important}.ot-native-nav button{flex:0 0 auto!important}}
  `;
  document.head.appendChild(style);
  const header = document.createElement('header');
  header.className = 'ot-mobile-header';
  header.innerHTML = `<div class="ot-mobile-heading"><div><h1>OpenTail</h1><p>Lighting Studio</p></div><span class="ot-connected-badge offline ot-status-red" id="ot-connected-badge" role="status" aria-live="polite">Disconnected</span></div>
    <div id="ot-save-banner" class="ot-save-warning" hidden role="alert">Save failed · Review Studio changes</div><div class="ot-connection-grid"><button type="button" class="ot-connection-card" id="ot-wifi-card" data-active="false" aria-label="Wi-Fi connection settings">${icon('wifi')}<span><strong>Wi-Fi</strong><small id="ot-wifi-text">Not connected</small></span></button>
    <button type="button" class="ot-connection-card" id="ot-ble-card" data-active="false" aria-label="Bluetooth connection settings">${icon('bluetooth')}<span><strong>Bluetooth</strong><small id="ot-ble-text">Not connected</small></span></button></div>`;
  const settings = document.createElement('section');
  settings.className = 'ot-settings-page';
  settings.id = 'ot-unified-settings';
  settings.innerHTML = `<h2>Settings</h2>
    <details class="ot-settings-group" open>
      <summary>App Preferences ${icon('arrow')}</summary>
      <button type="button" class="ot-quick-action" data-ot-native="prefs">Haptics and reconnection ${icon('arrow')}</button>
      <button type="button" class="ot-quick-action ot-theme-settings" data-ot-native="appearance">I Hate Dark Mode / Back to Dark Mode ${icon('arrow')}</button>
      <button type="button" class="ot-quick-action" data-ot-native="offline">Work offline ${icon('arrow')}</button>
    </details>
    <details class="ot-settings-group">
      <summary>Diagnostics ${icon('arrow')}</summary>
      <p class="ot-settings-copy">Hardware status is displayed only when confirmed by the controller. Offline readings are unavailable.</p>
      <button type="button" class="ot-quick-action" data-ot-native="diagnostics">View controller diagnostics ${icon('arrow')}</button>
    </details>
    <details class="ot-settings-group">
      <summary>Project Backup ${icon('arrow')}</summary>
      <p class="ot-settings-copy">Export a backup before importing. Import replaces the local working project.</p>
      <button type="button" class="ot-quick-action" data-ot-native="backup-export">Export project ${icon('arrow')}</button>
      <label class="ot-quick-action">Import project
        <input type="file" id="ot-settings-import" accept=".json,application/json" aria-label="Import OpenTail project JSON">
      </label>
    </details>
    <details class="ot-settings-group">
      <summary>About ${icon('arrow')}</summary>
      <p class="ot-settings-copy">OpenTail Android v0.1.24 · Studio v0.19.56</p>
      <button type="button" class="ot-quick-action" data-ot-native="about">Version history and technical notes ${icon('arrow')}</button>
    </details>
    <p class="ot-settings-help">Bluetooth lighting remains bench-only. Never rely on this prototype as the sole road-lighting safety system.</p>`;
  const nav = document.createElement('nav');
  nav.className = 'ot-native-nav';
  nav.setAttribute('aria-label', 'OpenTail main navigation');
  // Canonical six-tab OpenTail navigation; Studio's competing concept nav is suppressed below.
  const destinations = [
    ['home','Home','home'],
    ['scenes','Show Modes','shows'],
    ['layout','Layout','layout'],
    ['builder','Triggers','triggers'],
    ['events','Auto Events','events'],
    ['settings','Settings','settings']
  ];
  nav.innerHTML = destinations.map(([id,label,ic]) => `<button type="button" data-ot-destination="${id}" aria-current="false">${icon(ic)}<span>${label}</span></button>`).join('');
  document.body.prepend(header);
  document.body.appendChild(settings);
  document.body.insertBefore(nav, header.nextSibling); // one segmented navigator below header
  let settingsOpen = false;
  let autoEventsOpen = false;
  const state = () => {
    try { return JSON.parse(window.OpenTailNativeApp.getState() || '{}'); } catch (_) { return {}; }
  };
  function refresh() {
    const s = state();
    const badge = document.getElementById('ot-connected-badge');
    const wifiText = document.getElementById('ot-wifi-text');
    const bleText = document.getElementById('ot-ble-text');
    const wifiCard = document.getElementById('ot-wifi-card');
    const bleCard = document.getElementById('ot-ble-card');
    const wifi = Boolean(s.wifi);
    const ble = Boolean(s.ble && s.bleRadioOn !== false);
    const phase = s.blePhase || 'disconnected';
    const busyBle = ['scanning','connecting','pairing','authorizing','saving','reading'].includes(phase);
    // Bluetooth OFF must never inherit a green last-known connection badge.
    // Wi-Fi remains represented separately in the connection card when available.
    const radioOff = s.bleRadioOn === false;
    // The pill reports LIVE transport state, not the last successful session.
    // BLE activity takes precedence over Wi-Fi for progress, and a switched-off
    // Bluetooth radio remains red even if the independent Wi-Fi path is online.
    const bleInProgress = !radioOff && busyBle;
    const bleReady = !radioOff && ble && Boolean(s.bleAuthorized) && !bleInProgress;
    const wifiReady = !radioOff && wifi && !ble && !bleInProgress;
    const label = radioOff ? 'Bluetooth off'
      : phase === 'permission' ? 'Permission needed'
      : bleInProgress ? ({scanning:'Scanning…',connecting:'Connecting…',pairing:'Pairing…',authorizing:'Authorizing…',saving:'Saving…',reading:'Syncing…'}[phase])
      : bleReady ? 'Connected'
      : ble ? 'Authorize'
      : wifiReady ? 'Wi-Fi connected'
      : 'Disconnected';
    const pending = !radioOff && (bleInProgress || (ble && !s.bleAuthorized));
    const connected = bleReady || wifiReady;
    const detail = radioOff ? 'Bluetooth is off'
      : bleInProgress ? 'Bluetooth ' + phase
      : bleReady ? 'Bluetooth connected and authorized'
      : ble ? 'Bluetooth connected; authorization required'
      : wifiReady ? 'OpenTail CORE Wi-Fi connected'
      : phase === 'permission' ? 'Bluetooth permission required'
      : 'No active OpenTail controller connection';
    const failure = document.getElementById('ot-save-failure');
    const warning = document.getElementById('ot-save-banner');
    if (warning) warning.hidden = !failure || failure.hidden;
    if (badge) {
      const mode = connected ? 'green' : pending ? 'amber' : 'red';
      const pictogram = radioOff || ble || bleInProgress ? 'bluetooth' : wifiReady ? 'wifi' : 'x';
      badge.innerHTML = icon(pictogram) + '<span>' + label + '</span>';
      badge.classList.toggle('offline', mode === 'red');
      badge.classList.toggle('limited', mode === 'amber');
      badge.classList.toggle('ot-status-red', mode === 'red');
      badge.classList.toggle('ot-status-amber', mode === 'amber');
      badge.classList.toggle('ot-status-green', mode === 'green');
      badge.dataset.health = mode;
      badge.setAttribute('aria-label', detail);
      badge.setAttribute('title', detail);
      badge.setAttribute('role', 'status');
    }
    if (wifiText) wifiText.textContent = wifi ? 'CORE Wi-Fi connected' : 'Not connected';
    if (bleText) bleText.textContent = phase === 'permission' ? 'Permission required'
      : !s.bleRadioOn && s.bleRadioOn !== undefined ? 'Bluetooth is off'
      : ble ? (s.bleAuthorized ? 'Ready · Authorized' : 'Connected · Authorization needed')
      : phase === 'permission' ? 'Permission required' : busyBle ? label : 'Not connected';
    if (wifiCard) wifiCard.dataset.active = String(wifi);
    if (bleCard) bleCard.dataset.active = String(ble);
    const activeTab = document.querySelector('.tabs .tab.active');
    let route = settingsOpen ? 'settings' : autoEventsOpen ? 'events' : 'home';
    if (!settingsOpen && !autoEventsOpen && activeTab) {
      const text = activeTab.textContent || '';
      if (/Show Modes/i.test(text)) route='scenes';
      else if (/Triggers/i.test(text)) route='builder';
      else if (/Layout/i.test(text)) route='layout';
      else if (/Connect|Diagnostics/i.test(text)) route='settings';
    }
    document.body.classList.toggle('ot-auto-events-active',!settingsOpen && route==='events');
    document.body.classList.toggle('ot-show-active',!settingsOpen && route==='scenes');
    document.body.classList.toggle('ot-home-active',!settingsOpen && route==='home');
    const heading=document.querySelector('.ot-mobile-heading h1');
    if(heading)heading.textContent='OpenTail';
    nav.querySelectorAll('button[data-ot-destination]').forEach(el => el.setAttribute('aria-current',el.dataset.otDestination===route?'page':'false'));
  }
  function navigate(target) {
    if (target === 'settings') {
      settingsOpen=true;autoEventsOpen=false;
      document.body.classList.add('ot-settings-active');
      refresh();return;
    }
    settingsOpen=false;autoEventsOpen=target==='events';
    document.body.classList.remove('ot-settings-active');
    // Route into the existing isolated Auto Events preview; never emulate an input.
    if (typeof window.otConceptNavigate === 'function') window.otConceptNavigate(target);
    else if (target!=='events' && typeof window.go === 'function') window.go(target);
    refresh();
    window.scrollTo({top:0,behavior:'instant'});
  }
  document.addEventListener('click', event => {
    const dest = event.target.closest('.ot-native-nav [data-ot-destination]');
    if (dest) {navigate(dest.dataset.otDestination);return;}
    if (event.target.closest('#ot-wifi-card')) {window.OpenTailNativeApp.connectWifi();return;}
    if (event.target.closest('#ot-ble-card')) {
      const live=state();
      if (live.ble && !live.bleAuthorized) window.OpenTailNativeApp.authorizeBle();
      else window.OpenTailNativeApp.scanBle();
      return;
    }
    const action = event.target.closest('[data-ot-native]');
    if (!action) return;
    if (action.dataset.otNative === 'appearance') {
      if (typeof window.otSetTheme === 'function') {
        window.otSetTheme(document.documentElement.dataset.otTheme === 'light' ? 'dark' : 'light');
      }
      return;
    }
    const commands={
      connect:'connectWifi',wifi:'wifiSettings',scan:'scanBle',authorize:'authorizeBle',
      prefs:'appSettings',offline:'offlineStudio'
    };
    if (commands[action.dataset.otNative]) {
      window.OpenTailNativeApp[commands[action.dataset.otNative]]();
      if (action.dataset.otNative === 'offline') navigate('home');
      return;
    }
    if (action.dataset.otNative==='blemode'||action.dataset.otNative==='wifimode'){window.OpenTailBLEPanel?.setTransport(action.dataset.otNative==='blemode'?'ble':'wifi');navigate('scenes');return;}
    if (action.dataset.otNative==='appearance' && typeof window.otSetTheme === 'function') window.otSetTheme(document.documentElement.dataset.otTheme==='light'?'dark':'light');
    if (action.dataset.otNative==='backup-export') {
      if (typeof window.exportProject === 'function') window.exportProject();
      return;
    }
    if (action.dataset.otNative==='about') {
      if (typeof window.otAbout === 'function') window.otAbout();
      return;
    }
    if (action.dataset.otNative==='diagnostics') navigate('diagnostics');
    if (action.dataset.otNative==='advanced') navigate('device');
  });
  document.addEventListener('change', event => {
    const fileInput=event.target;
    if (fileInput?.id!=='ot-settings-import' || !fileInput.files?.length) return;
    const file=fileInput.files[0];
    if (window.confirm('Import this project? Current local lighting layout, presets and effects will be replaced. Export a backup first.')) {
      if (typeof window.importProject === 'function') void window.importProject(file);
    }
    fileInput.value='';
  });
  const app = document.getElementById('app');
  if (app) {
    let queued=false;
    new MutationObserver(() => {
      if (queued) return;
      queued=true;
      requestAnimationFrame(() => {queued=false;refresh();});
    }).observe(app,{childList:true,subtree:false});
  }
  window.OpenTailShell = { refresh, navigate };
  refresh();
  // Open directly to the agreed Show Modes design, without clearing saved projects.
  if (typeof window.go === 'function') navigate('home');
  setInterval(refresh,2000);
})();

/* OpenTail Android v0.1.23 — compact native-first surfaces.
   Preserve all existing Studio editors and their proven save/readback functions.
   These read-only overview cards never overwrite output or trigger configuration. */
(function () {
  'use strict';
  if (!window.OpenTailShell || window.__otCompactSurfaces) return;
  window.__otCompactSurfaces = true;
  const esc = s => String(s ?? '').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  const names = {park:'Parking',brake:'Brake',left:'Left turn',right:'Right turn',reverse:'Reverse',aux1:'Auxiliary 1',aux2:'Auxiliary 2',aux3:'Auxiliary 3',disabled:'Disabled'};
  const panel = document.createElement('main');
  panel.id='ot-compact-pages';
  panel.setAttribute('aria-live','polite');
  const header=document.querySelector('.ot-mobile-header');
  document.body.insertBefore(panel, header?.nextSibling || document.body.firstChild);
  const css=document.createElement('style');
  css.id='ot-compact-surface-style';
  css.textContent=`
    .ot-mobile-header{max-width:720px!important}
    .ot-mobile-heading h1{font-size:23px!important}
    #ot-compact-pages{display:none;padding:12px 16px calc(100px + env(safe-area-inset-bottom, 0px));max-width:720px;margin:0 auto;box-sizing:border-box}
    body.ot-compact-view #ot-compact-pages{display:block}
    body.ot-compact-view #app{display:none!important}
    body.ot-compact-view .ot-mobile-header{display:block}
    body.ot-settings-active #ot-compact-pages{display:none!important}
    .ot-simple-top{display:flex;justify-content:space-between;align-items:center;gap:14px;margin-bottom:14px}
    .ot-simple-top h2{margin:0 0 4px;font-size:21px;line-height:1.2;font-weight:750;letter-spacing:-.3px}
    .ot-simple-top p{margin:0;color:#9eada4;font-size:12px;line-height:1.35}
    .ot-simple-tag{font-size:10px;color:#b0e9bd;white-space:nowrap;border-radius:999px;padding:6px 9px;background:#193021;border:1px solid #2f5e41}
    .ot-simple-card{background:#151e18;border:1px solid #2c3930;border-radius:17px;padding:15px;margin-bottom:12px}
    .ot-simple-card h3{font-size:15px;margin:0 0 11px;color:#edf9ef}
    .ot-simple-card .ot-muted{color:#9eaea4;font-size:12px;line-height:1.5}
    .ot-simple-stats{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}
    .ot-simple-stat{background:#0d1510;border:1px solid #304136;border-radius:13px;padding:13px}
    .ot-simple-stat strong{font-size:20px;font-variant-numeric:tabular-nums;display:block;line-height:1.2;margin-bottom:5px}
    .ot-simple-stat small{font-size:11px;color:#a9b9ad}
    .ot-simple-button{font:650 13px Roboto,Inter,sans-serif;appearance:none;border:0;min-height:43px;border-radius:11px;background:#27764a;color:#fff;padding:10px 14px;cursor:pointer}
    .ot-simple-button.secondary{background:#192b20;border:1px solid #426c4d;color:#c5f2d3}
    .ot-simple-button:focus-visible{outline:2px solid #75e69e;outline-offset:2px}
    .ot-simple-actions{display:grid;grid-template-columns:1fr 1fr;gap:9px;margin-top:14px}
    .ot-simple-line{display:flex;justify-content:space-between;align-items:center;gap:12px;padding:11px 1px;border-bottom:1px solid #28352c;font-size:13px}
    .ot-simple-line:last-child{border-bottom:none;padding-bottom:2px}
    .ot-simple-line strong{font-size:13px}
    .ot-simple-line small{display:block;font-size:11px;color:#9fac9f;margin-top:3px}
    .ot-simple-value{font-size:12px;font-weight:650;color:#d3f1d9;text-align:right;white-space:normal}
    .ot-simple-output{background:#0d1711;border:1px solid #314537;border-radius:12px;padding:12px;margin-top:10px}
    .ot-simple-output h4{font-size:14px;margin:0 0 5px}
    .ot-simple-tags{display:flex;flex-wrap:wrap;gap:5px;margin-top:8px}
    .ot-simple-tags span{font-size:10px;color:#b5d0bc;background:#1b2b21;border:1px solid #365042;border-radius:99px;padding:4px 7px}
    .ot-simple-readonly{font-size:11px;color:#a8b8ac;margin:8px 0 0;line-height:1.4}
    body.ot-compact-shows #app .ot-page-intro{display:none!important}
    body.ot-compact-shows #app>.shell{padding-top:4px!important}
    /* Show tab keeps Studio preview visible alongside BLE controls. */
    /* BLE transport selection must not hide Live Lighting Preview. */
    body.ot-compact-shows #app .footer{display:none!important}
    body.ot-compact-shows.ot-ble-mode #ot-ble-inline{margin-top:0!important}
    body.ot-compact-shows .ot-ble-fields{row-gap:9px!important}
    body.ot-compact-shows .ot-ble-notice{display:none!important}
    body.ot-compact-shows .ot-ble-response{font-size:11px!important;margin-top:7px!important;min-height:15px!important}
    body.ot-compact-shows .ot-ble-actions{grid-template-columns:2fr 1fr!important}
    body.ot-compact-shows .ot-ble-actions button{min-height:50px!important}
    body.ot-compact-shows .ot-ble-inline .ot-ble-status{font-size:10px!important}
    body.ot-compact-shows .ot-ble-switch button{min-height:41px!important}
    body.ot-compact-shows .ot-ble-inline .ot-ble-fields label{font-size:11px!important}
    body.ot-compact-shows .ot-ble-inline .ot-ble-head p{font-size:11px!important}
    body.ot-compact-shows .ot-ble-inline .ot-ble-head h2{margin-bottom:3px!important}

/* Approved compact Show layout: original ot-preview renderer and its controls remain intact. */
body.ot-compact-shows #app .card.ot-preview{display:block!important;order:-1;margin:6px 0 10px!important;padding:12px!important;background:#000!important;border:1px solid #303030!important;border-radius:17px!important}
body.ot-compact-shows #app .card.ot-preview .ot-preview-modes{margin:8px 0 10px!important}
body.ot-compact-shows #app .card.ot-preview .ot-preview-modes button{min-height:35px!important}
body.ot-compact-shows #app .card.ot-preview .ot-preview-row{margin:5px 0!important;padding:7px 8px!important}
body.ot-compact-shows #app .card.ot-preview .ot-sim-actions{gap:6px!important}
body.ot-compact-shows #app .card.ot-preview .ot-sim-actions button{min-height:36px!important}
body.ot-compact-shows #app .card.ot-preview .ot-sim-triggers{gap:5px!important}
body.ot-compact-shows #app .card.ot-preview .ot-sim-triggers button{min-height:32px!important}
body.ot-compact-shows #app .card.ot-preview .ot-ledstrip{min-width:0!important}
body.ot-compact-shows #app main{display:flex;flex-direction:column;gap:9px}
body.ot-compact-shows #app main > *{min-width:0}

    body.ot-editor-active #app .ot-page-intro{display:none!important}
    body.ot-editor-active #app .ot-om-card>p.small.muted,body.ot-editor-active #app .ot-tm-card>p.small.muted{display:none!important}
    body.ot-editor-active #app .ot-om-card,body.ot-editor-active #app .ot-tm-card{padding:12px!important}
    body.ot-editor-active #app .ot-om-zone{padding:12px!important;margin-top:9px!important}
    body.ot-editor-active #app .ot-om-zone-head{margin-bottom:7px!important}
    body.ot-editor-active #app .ot-om-card .field,body.ot-editor-active #app .ot-tm-card .field{margin-bottom:8px!important}
    html[data-ot-theme=light] .ot-simple-card{background:#fff;border-color:#d6e4d7}
    html[data-ot-theme=light] .ot-simple-card h3,html[data-ot-theme=light] .ot-simple-top h2{color:#1a2d20}
    html[data-ot-theme=light] .ot-simple-stat,html[data-ot-theme=light] .ot-simple-output{background:#f1f7f2;color:#193623;border-color:#d1e1d4}
    html[data-ot-theme=light] .ot-simple-line{border-color:#dce9df}
    html[data-ot-theme=light] .ot-simple-value{color:#23643b}
    @media(min-width:700px){#ot-compact-pages{padding-left:28px;padding-right:28px}}
  `;
  css.textContent += `
    /* Approved in-chat OpenTail concept: neutral surfaces, one unified page,
       compact status cards, minimal header, show card, and fixed 5-tab navigation.
       Presentation overrides only; do not alter BLE/CORE API handling. */
    html:not([data-ot-theme=light]),html:not([data-ot-theme=light]) body{background:#101112!important;color:#f0f0f1!important}
    .ot-mobile-header{padding:12px 14px 8px!important;max-width:720px!important}
    .ot-mobile-heading{margin-bottom:12px!important;align-items:center!important}
    .ot-mobile-heading h1{font-size:21px!important;line-height:1.2!important;letter-spacing:-.35px!important;color:#f2f2f3!important}
    .ot-mobile-heading p{display:none!important}
    .ot-connected-badge{padding:5px 9px!important;font-size:10px!important;background:#26382d!important;border-color:#4a6953!important;color:#bce9c9!important}
    .ot-connection-grid{gap:8px!important}
    .ot-connection-card{min-height:61px!important;padding:11px 12px!important;border:1px solid #3a3e3f!important;background:#242627!important;border-radius:13px!important;gap:9px!important}
    .ot-connection-card strong{font-size:12px!important;color:#eeeeef!important;font-weight:650!important}
    .ot-connection-card small{font-size:10px!important;color:#b9bbbd!important;margin-top:2px!important}
    .ot-connection-card svg{width:20px!important;height:20px!important;color:#a3d1b1!important}
    .ot-native-nav{background:#18191b!important;border-top:1px solid #393b3e!important;padding:7px 9px calc(7px + env(safe-area-inset-bottom,0px))!important;box-shadow:none!important}
    .ot-native-nav button{font-size:10px!important;font-weight:550!important;min-height:53px!important;border-radius:11px!important;color:#a4a6a9!important;gap:4px!important}
    .ot-native-nav button[aria-current=page]{background:#303132!important;color:#f0f0f1!important}
    .ot-native-nav button svg{width:20px!important;height:20px!important}
    #app>.shell{padding:4px 14px 122px!important}
    #ot-ble-inline{padding:14px!important;margin:5px 0 12px!important;background:#242627!important;border:1px solid #393c3d!important;border-radius:15px!important;color:#ededee!important}
    #ot-ble-inline .ot-ble-head{align-items:center!important}
    #ot-ble-inline .ot-ble-head h2{font-size:16px!important;margin:0!important;color:#f5f5f6!important;letter-spacing:-.15px!important}
    #ot-ble-inline .ot-ble-head p{display:none!important}
    #ot-ble-inline .ot-ble-status{position:absolute!important;width:1px!important;height:1px!important;clip-path:inset(50%)!important;overflow:hidden!important;padding:0!important;border:0!important}
    #ot-ble-inline .ot-ble-settings{min-height:30px;min-width:32px;border:0;background:transparent;color:#b8babd;font-size:19px;display:grid;place-items:center;border-radius:8px;cursor:pointer}
    #ot-ble-inline .ot-ble-settings:focus-visible{outline:2px solid #a5c9b0}
    #ot-ble-inline .ot-ble-switch{margin:12px 0!important;border-radius:10px!important;background:#111213!important;padding:4px!important}
    #ot-ble-inline .ot-ble-switch button{min-height:40px!important;border-radius:8px!important;font-size:13px!important;color:#c1c2c3!important}
    #ot-ble-inline .ot-ble-switch button[aria-pressed=true]{background:#3a3c3d!important;color:#f7f7f7!important}
    #ot-ble-inline .ot-ble-fields{gap:10px!important}
    #ot-ble-inline .ot-ble-fields label{font-size:11px!important;font-weight:600!important;color:#c9cacb!important;gap:6px!important}
    #ot-ble-inline .ot-ble-fields select,#ot-ble-inline .ot-ble-fields input[type=color]{background:#191a1b!important;border:1px solid #4b4d4f!important;color:#f0f0f0!important;border-radius:9px!important;height:42px!important;font-size:12px!important}
    #ot-ble-inline .ot-ble-more{background:#202223!important;border-color:#414447!important;border-radius:9px!important}
    #ot-ble-inline .ot-ble-more summary{font-size:11px!important;color:#d5d8d6!important;min-height:39px!important}
    #ot-ble-inline .ot-ble-actions{grid-template-columns:2fr 1fr!important;gap:8px!important;margin-top:11px!important}
    #ot-ble-inline .ot-ble-actions button{min-height:43px!important;border-radius:9px!important}
    #ot-ble-inline .ot-ble-actions button:first-child{background:#2e6b47!important;border-color:#2e6b47!important}
    #ot-ble-inline .ot-ble-actions button:last-child{background:#1a1b1c!important;border-color:#454749!important;color:#eeeeee!important}
    #ot-ble-inline .ot-ble-response{font-size:10px!important;color:#b7b9ba!important}
    #ot-compact-pages{padding:10px 14px calc(110px + env(safe-area-inset-bottom,0px))!important}
    #ot-compact-pages .ot-simple-top h2{font-size:18px!important;color:#f5f5f5!important}
    #ot-compact-pages .ot-simple-top p{font-size:11px!important;color:#c0c1c2!important}
    #ot-compact-pages .ot-simple-card{background:#242627!important;border-color:#3b3e40!important;border-radius:14px!important;padding:14px!important}
    #ot-compact-pages .ot-simple-stat,#ot-compact-pages .ot-simple-output{background:#1b1d1e!important;border-color:#46484b!important;color:#f1f1f1!important}
    #ot-compact-pages .ot-simple-card h3{color:#f3f3f4!important}
    #ot-compact-pages .ot-simple-line{border-color:#424446!important}
    #ot-compact-pages .ot-simple-button{background:#326d49!important}
    #ot-compact-pages .ot-simple-button.secondary{background:#252627!important;border-color:#565b58!important}
    body.ot-settings-active .ot-settings-page{padding:10px 14px 125px!important}
    .ot-settings-group{background:#242627!important;border-color:#3b3e40!important}
    /* Existing light-theme preference remains supported. */
    html[data-ot-theme=light] .ot-mobile-heading h1{color:#1b2620!important}
    html[data-ot-theme=light] .ot-connection-card{background:#fff!important;color:#152018!important;border-color:#d5e1d8!important}
    html[data-ot-theme=light] .ot-connection-card strong{color:#152018!important}
    html[data-ot-theme=light] .ot-connection-card small{color:#5f6c62!important}
    html[data-ot-theme=light] .ot-native-nav{background:#fafafa!important}
    html[data-ot-theme=light] .ot-native-nav button[aria-current=page]{background:#e5eee7!important;color:#285a38!important}
    html[data-ot-theme=light] #ot-ble-inline,html[data-ot-theme=light] #ot-compact-pages .ot-simple-card{background:#fff!important;border-color:#dce3dd!important;color:#152018!important}
    html[data-ot-theme=light] #ot-ble-inline .ot-ble-head h2,html[data-ot-theme=light] #ot-compact-pages .ot-simple-card h3,html[data-ot-theme=light] #ot-compact-pages .ot-simple-top h2{color:#1b2620!important}
    html[data-ot-theme=light] #ot-ble-inline .ot-ble-fields select{background:#fff!important;color:#1b2620!important;border-color:#cad6cd!important}
    html[data-ot-theme=light] #ot-ble-inline .ot-ble-switch{background:#e5ede7!important}
    html[data-ot-theme=light] #ot-ble-inline .ot-ble-switch button[aria-pressed=true]{background:#fff!important;color:#1b2620!important}
    html[data-ot-theme=light] #ot-compact-pages .ot-simple-stat,html[data-ot-theme=light] #ot-compact-pages .ot-simple-output{background:#f1f5f2!important;color:#17291c!important}
    html[data-ot-theme=light] .ot-settings-group{background:#fff!important}
`;
  css.textContent += `

/* v0.1.11: approved black-on-black interactive design, without altering #app .ot-preview. */
html:not([data-ot-theme=light]), html:not([data-ot-theme=light]) body{background:#050505!important;color:#f8f8f8!important}
.ot-mobile-header{background:#050505!important;padding:17px 16px 12px!important}
.ot-mobile-heading h1{font-size:24px!important;font-weight:660!important;letter-spacing:-.45px!important;color:#fff!important}
.ot-connected-badge{background:#09200f!important;color:#43d773!important;border:0!important;font-size:11px!important;letter-spacing:.15px!important}
.ot-connected-badge.offline{background:#252016!important;color:#e4c186!important}
.ot-connection-card{background:#0b0b0b!important;border:1px solid #2b2b2b!important;border-radius:16px!important;color:#fafafa!important;min-height:57px!important}
.ot-connection-card[data-active=true] svg{color:#45cf78!important}
.ot-connection-card[data-active=false] svg{color:#858585!important}
.ot-connection-card strong{color:#f8f8f8!important}
.ot-connection-card small{color:#aaa!important}
.ot-native-nav{background:#070707!important;border-top:1px solid #232323!important}
.ot-native-nav button{color:#999!important;transition:background .16s ease,color .16s ease!important}
.ot-native-nav button[aria-current=page]{background:#151515!important;color:#f9f9f9!important}
.ot-native-nav button[aria-current=page] svg{color:#43c775!important}
.ot-native-nav button:active{transform:scale(.97)}
#ot-ble-inline{background:#000!important;border:1px solid #242424!important;border-radius:24px!important;margin-top:10px!important;padding:17px!important;box-shadow:none!important}
#ot-ble-inline .ot-ble-head h2{font-size:19px!important;font-weight:640!important}
#ot-ble-inline .ot-ble-head .ot-ble-status{position:static!important;width:auto!important;height:auto!important;clip-path:none!important;overflow:visible!important;padding:7px 12px!important;background:#07200f!important;border:0!important;border-radius:999px!important;color:#39ce70!important;font-size:10px!important}
#ot-ble-inline .ot-ble-head .ot-ble-status:not(.is-ready){background:#231f17!important;color:#e2c88b!important}
#ot-ble-inline .ot-ble-settings{display:none!important}
#ot-ble-inline .ot-ble-switch{background:#0b0b0b!important;border:1px solid #202020!important;border-radius:999px!important;margin:14px 0 16px!important;padding:3px!important}
#ot-ble-inline .ot-ble-switch button{min-height:42px!important;border-radius:999px!important;color:#afafaf!important;font-weight:610!important}
#ot-ble-inline .ot-ble-switch button[aria-pressed=true]{background:#232323!important;color:#fff!important;box-shadow:0 1px 4px #000!important}
#ot-ble-inline .ot-ble-fields{gap:12px!important}
#ot-ble-inline .ot-ble-fields label{gap:7px!important;color:#aaa!important;font-weight:550!important;font-size:12px!important}
#ot-ble-inline .ot-ble-fields select{height:49px!important;border:1px solid #3c3c3c!important;border-radius:999px!important;background:#050505!important;color:#fff!important;padding:8px 18px!important;font-size:14px!important}
#ot-ble-inline .ot-ble-fields select:focus-visible{outline:2px solid #49cf80!important;outline-offset:2px!important}
#ot-ble-inline .ot-ble-range{margin-top:15px!important;gap:9px!important}
#ot-ble-inline .ot-ble-range>div{font-size:13px!important;font-weight:520!important;color:#adadad!important}
#ot-ble-inline .ot-ble-range output{color:#c4c4c4!important;font-size:13px!important}
#ot-ble-inline .ot-ble-range input[type=range]{display:block!important;width:100%!important;appearance:none!important;-webkit-appearance:none!important;height:28px!important;min-height:28px!important;background:transparent!important;accent-color:#dcdcdc!important;touch-action:pan-y!important}
#ot-ble-inline .ot-ble-range input[type=range]::-webkit-slider-runnable-track{height:5px!important;border-radius:999px!important;background:#232323!important;border:0!important}
#ot-ble-inline .ot-ble-range input[type=range]::-webkit-slider-thumb{-webkit-appearance:none!important;appearance:none!important;width:34px!important;height:24px!important;border:0!important;border-radius:14px!important;background:#fff!important;margin-top:-9.5px!important;box-shadow:0 1px 2px #000b!important}
#ot-ble-inline .ot-ble-range input[type=range]:focus-visible{outline:2px solid #48cb75!important;outline-offset:2px!important}
#ot-ble-inline .ot-ble-more{background:transparent!important;border:1px solid #2d2d2d!important;border-radius:13px!important;margin-top:14px!important}
#ot-ble-inline .ot-ble-more>summary{color:#aaa!important;font-size:12px!important;padding:2px 4px!important}
#ot-ble-inline .ot-ble-extra-fields input[type=color]{background:#111!important;border:1px solid #454545!important;border-radius:12px!important}
#ot-ble-inline .ot-ble-actions{grid-template-columns:1fr 1fr!important;margin-top:18px!important;gap:9px!important}
#ot-ble-inline .ot-ble-actions button{border-radius:999px!important;font-size:13px!important;min-height:47px!important;transition:filter .12s ease,transform .12s ease!important}
#ot-ble-inline .ot-ble-actions button:first-child{background:#183b24!important;border-color:#254f32!important;color:#bff4cc!important}
#ot-ble-inline .ot-ble-actions button:last-child{background:#080808!important;border-color:#303030!important;color:#d8d8d8!important}
#ot-ble-inline .ot-ble-actions button:active{transform:scale(.97)}
#ot-ble-inline .ot-ble-response{font-size:11px!important;color:#aaa!important}
#ot-compact-pages .ot-simple-top h2{color:#fff!important;font-weight:630!important}
#ot-compact-pages .ot-simple-card{background:#0b0b0b!important;border:1px solid #282828!important;border-radius:21px!important;padding:17px!important}
#ot-compact-pages .ot-simple-card h3{color:#f5f5f5!important}
#ot-compact-pages .ot-simple-stat,#ot-compact-pages .ot-simple-output{background:#070707!important;border-color:#333!important;border-radius:16px!important}
#ot-compact-pages .ot-simple-button{background:#1b3323!important;border:1px solid #2a5638!important;color:#d6ffe0!important;border-radius:999px!important}
#ot-compact-pages .ot-simple-button.secondary{background:#0a0a0a!important;border-color:#333!important;color:#eaeaea!important}
#ot-compact-pages .ot-simple-line{border-color:#282828!important}
.ot-settings-group{background:#0c0c0c!important;border-color:#282828!important;border-radius:20px!important}
.ot-quick-action{border-color:#242424!important;color:#f5f5f5!important}
.ot-quick-action:active{background:#171717!important}
/* Styling for legacy Studio editors is limited to those outside the already-perfect Live Lighting Preview. */
body.ot-editor-active #app .ot-om-card,body.ot-editor-active #app .ot-tm-card{background:#090909!important;border-color:#292929!important;border-radius:22px!important}
body.ot-editor-active #app .ot-om-zone{background:#080808!important;border-color:#333!important;border-radius:17px!important}
body.ot-editor-active #app .ot-om-zone-fields .field input,body.ot-editor-active #app .ot-om-zone-fields .field select{background:#070707!important;border:1px solid #414141!important;border-radius:14px!important}
body.ot-editor-active #app .ot-om-card .chip,body.ot-editor-active #app .ot-tm-card .chip{border-radius:999px!important;background:#0e0e0e!important;border-color:#3a3a3a!important}
body.ot-editor-active #app .ot-om-card .chip.active,body.ot-editor-active #app .ot-tm-card .chip.active{background:#16291b!important;border-color:#40bc6b!important;color:#d1f9da!important}
/* Keep live lighting preview, pixel renderer, simulator, and all its interaction styles untouched. */

/* Keep Show Modes controls usable above the fixed navigation on short phones. */
.ot-mobile-header{padding-top:11px!important;padding-bottom:8px!important}
#ot-ble-inline{margin-top:5px!important;padding-top:13px!important;padding-bottom:13px!important}
#ot-ble-inline .ot-ble-switch{margin:9px 0 12px!important}
#ot-ble-inline .ot-ble-fields{row-gap:8px!important}
#ot-ble-inline .ot-ble-range{margin-top:10px!important;gap:5px!important}
#ot-ble-inline .ot-ble-range input[type=range]{height:26px!important;min-height:26px!important}
#ot-ble-inline .ot-ble-actions{margin-top:11px!important}
@media(prefers-reduced-motion:reduce){.ot-native-nav button,#ot-ble-inline .ot-ble-actions button{transition:none!important}}

`;
  css.textContent += `

/* v0.1.11 visual-density + neutral-palette refinements. Keep established Live Lighting Preview untouched. */
html:not([data-ot-theme=light]),html:not([data-ot-theme=light]) body{background:#000!important;color:#f4f4f4!important}
.ot-mobile-header{padding:8px 12px 5px!important;max-width:650px!important}
.ot-mobile-heading{margin-bottom:6px!important;gap:6px!important}
.ot-mobile-heading h1{font-size:20px!important;line-height:1.05!important;letter-spacing:-.45px!important}
.ot-mobile-heading p{margin-top:2px!important;font-size:10px!important;color:#888!important}
.ot-connected-badge{display:inline-flex!important;align-items:center!important;gap:6px!important;background:#101010!important;color:#e2e2e2!important;border:1px solid #242424!important;font-size:10px!important;padding:6px 10px!important}
.ot-connected-badge:before{content:"";width:6px;height:6px;border-radius:50%;background:#3fc878;display:inline-block;flex-shrink:0}
.ot-connected-badge.offline:before{background:#888}
.ot-connected-badge.offline{color:#aaa!important}
.ot-connection-grid{gap:7px!important}
.ot-connection-card{min-height:43px!important;gap:7px!important;background:#0b0b0b!important;border-color:#252525!important;border-radius:13px!important;padding:6px 10px!important}
.ot-connection-card svg{width:17px!important;height:17px!important}
.ot-connection-card[data-active=true] svg{color:#b9b9b9!important}
.ot-connection-card[data-active=false] svg{color:#686868!important}
.ot-connection-card strong{font-size:11px!important;color:#eeeeee!important;font-weight:650!important}
.ot-connection-card small{font-size:9px!important;color:#969696!important;margin-top:0!important}
.ot-connection-card:active{background:#171717!important}
.ot-native-nav{background:#060606!important;border-color:#222!important;padding:5px 5px calc(7px + env(safe-area-inset-bottom,0px))!important;box-shadow:0 -4px 15px #0008!important}
.ot-native-nav button{min-height:46px!important;gap:2px!important;font-size:9px!important;color:#9b9b9b!important}
.ot-native-nav button svg{width:18px!important;height:18px!important}
.ot-native-nav button[aria-current=page]{background:#191919!important;color:#f3f3f3!important}
.ot-native-nav button[aria-current=page] svg{color:#f3f3f3!important}
#app>.shell{padding:5px 12px 95px!important}
#ot-ble-inline{margin:7px 0 8px!important;border:1px solid #272727!important;border-radius:18px!important;background:#050505!important;padding:12px 13px 10px!important}
#ot-ble-inline .ot-ble-head{gap:6px!important}
#ot-ble-inline .ot-ble-head h2{font-size:17px!important;line-height:1.13!important;letter-spacing:-.25px!important;font-weight:650!important}
#ot-ble-inline .ot-ble-head p{display:none!important}
#ot-ble-inline .ot-ble-head .ot-ble-status{font-size:9px!important;color:#bcbcbc!important;background:#141414!important;border:1px solid #252525!important;padding:5px 8px!important}
#ot-ble-inline .ot-ble-head .ot-ble-status.is-ready{color:#7bdd9b!important;background:#0f1611!important;border-color:#253a2a!important}
#ot-ble-inline .ot-ble-switch{margin:9px 0 12px!important;padding:2px!important;border-color:#222!important;background:#0c0c0c!important}
#ot-ble-inline .ot-ble-switch button{min-height:36px!important;font-size:12px!important;color:#959595!important}
#ot-ble-inline .ot-ble-switch button[aria-pressed=true]{background:#242424!important;color:#eee!important;box-shadow:none!important}
#ot-ble-inline .ot-ble-fields{column-gap:9px!important;row-gap:8px!important}
#ot-ble-inline .ot-ble-fields label{gap:4px!important;font-size:11px!important;font-weight:550!important;color:#b0b0b0!important}
#ot-ble-inline .ot-ble-fields select{height:40px!important;min-height:40px!important;border:1px solid #303030!important;border-radius:16px!important;background:#080808!important;color:#eee!important;padding:6px 12px!important;font-size:12px!important}
#ot-ble-inline .ot-ble-fields select:focus-visible{outline-color:#d5d5d5!important}
#ot-ble-inline .ot-ble-range{margin-top:8px!important;gap:3px!important}
#ot-ble-inline .ot-ble-range>div{font-size:11px!important;color:#b0b0b0!important}
#ot-ble-inline .ot-ble-range output{font-size:11px!important;color:#dedede!important}
#ot-ble-inline .ot-ble-range input[type=range]{height:36px!important;min-height:36px!important;margin:0!important;accent-color:#dadada!important}
#ot-ble-inline .ot-ble-range input[type=range]::-webkit-slider-runnable-track{height:4px!important;background:#272727!important}
#ot-ble-inline .ot-ble-range input[type=range]::-webkit-slider-thumb{width:23px!important;height:23px!important;border-radius:12px!important;margin-top:-9.5px!important;background:#f9f9f9!important;box-shadow:0 0 0 2px #0b0b0b!important}
#ot-ble-inline .ot-ble-more{margin-top:8px!important;border-color:#2b2b2b!important;border-radius:12px!important}
#ot-ble-inline .ot-ble-more>summary{font-size:11px!important;color:#bbb!important;min-height:37px!important;padding:0 5px!important}
#ot-ble-inline .ot-ble-actions{margin-top:11px!important;gap:7px!important}
#ot-ble-inline .ot-ble-actions button{min-height:42px!important;font-size:12px!important;border-radius:14px!important}
#ot-ble-inline .ot-ble-actions button:first-child{background:#272727!important;border-color:#383838!important;color:#f9f9f9!important}
#ot-ble-inline .ot-ble-actions button:last-child{background:#0d0d0d!important;border-color:#303030!important;color:#ddd!important}
#ot-ble-inline .ot-ble-actions button:active{background:#444!important}
#ot-ble-inline .ot-ble-response{font-size:10px!important;color:#9b9b9b!important;margin-top:5px!important;min-height:13px!important}
#ot-compact-pages{padding:9px 12px calc(88px + env(safe-area-inset-bottom,0px))!important;max-width:650px!important}
#ot-compact-pages .ot-simple-top{margin-bottom:9px!important}
#ot-compact-pages .ot-simple-top h2{font-size:18px!important;font-weight:660!important}
#ot-compact-pages .ot-simple-tag{color:#aaa!important;background:#111!important;border-color:#292929!important}
#ot-compact-pages .ot-simple-card{background:#080808!important;border-color:#272727!important;border-radius:17px!important;padding:12px!important;margin-bottom:9px!important}
#ot-compact-pages .ot-simple-card h3{font-size:13px!important;margin-bottom:8px!important}
#ot-compact-pages .ot-simple-stat,#ot-compact-pages .ot-simple-output{background:#090909!important;border-color:#292929!important;border-radius:13px!important;padding:10px!important}
#ot-compact-pages .ot-simple-stat strong{font-size:17px!important}
#ot-compact-pages .ot-simple-button{background:#262626!important;border:1px solid #393939!important;border-radius:14px!important;color:#f2f2f2!important;min-height:41px!important}
#ot-compact-pages .ot-simple-button.secondary{background:#080808!important;color:#cfcfcf!important}
#ot-compact-pages .ot-simple-value{color:#f0f0f0!important}
#ot-compact-pages .ot-simple-tags span{background:#141414!important;border-color:#2b2b2b!important;color:#bcbcbc!important}
.ot-settings-group{background:#090909!important;border-color:#292929!important;border-radius:15px!important}
.ot-quick-action{padding:12px!important;min-height:47px!important}
.ot-quick-action svg{color:#aaa!important}
body.ot-editor-active #app .ot-om-card,body.ot-editor-active #app .ot-tm-card{padding:10px!important;border-color:#292929!important;border-radius:16px!important;background:#080808!important}
body.ot-editor-active #app .ot-om-zone{padding:10px!important;border-color:#303030!important;border-radius:14px!important;background:#090909!important}
body.ot-editor-active #app .ot-om-card .chip.active,body.ot-editor-active #app .ot-tm-card .chip.active{background:#262626!important;color:#f2f2f2!important;border-color:#646464!important}
@media(prefers-reduced-motion:reduce){.ot-connection-card,#ot-ble-inline button{transition:none!important}}

`;
  css.textContent += `

/* APPROVED REFERENCE: OLED black, no tinted UI surfaces. Status green only. */
html,html body{background:#000!important;color:#fff!important}
.ot-mobile-header{background:#000!important;padding:15px 18px 5px!important;max-width:680px!important}
.ot-mobile-heading{margin:0 0 3px!important}
.ot-mobile-heading h1{font-size:clamp(22px,5.6vw,29px)!important;line-height:1.18!important;letter-spacing:-.7px!important;font-weight:680!important}
.ot-mobile-heading p{display:none!important}
.ot-connected-badge{background:transparent!important;border:0!important;padding:4px 0!important;color:#27d589!important;font-size:12px!important;display:flex!important;align-items:center!important;gap:6px!important}
.ot-connected-badge::before{content:none!important}.ot-connected-badge svg{width:21px!important;height:21px!important;color:#27d589!important}
.ot-connected-badge.offline,.ot-connected-badge.offline::before{color:#999!important}
.ot-connection-grid{display:none!important}
body.ot-home-active .ot-connection-grid{display:grid!important;gap:9px!important;margin-top:14px!important}
.ot-connection-card{background:#000!important;border:1px solid #333!important;border-radius:20px!important;min-height:69px!important;padding:11px!important}
.ot-connection-card strong{font-size:13px!important}.ot-connection-card small{font-size:11px!important;margin-top:2px!important}
.ot-connection-card[data-active=true] svg{color:#23cf88!important}
#app>.shell{padding:0 18px calc(92px + env(safe-area-inset-bottom,0px))!important;max-width:680px!important}
#ot-ble-inline{border:0!important;border-radius:0!important;background:#000!important;padding:4px 0 12px!important;margin:0!important;box-shadow:none!important}
#ot-ble-inline .ot-ble-head{display:none!important}
#ot-ble-inline .ot-ble-switch{display:none!important}
#ot-ble-inline .ot-ble-fields{gap:13px!important;row-gap:19px!important;margin-top:21px!important}
#ot-ble-inline .ot-ble-fields label{font-size:15px!important;font-weight:470!important;gap:8px!important;color:#a7a7a7!important}
#ot-ble-inline .ot-ble-fields select{background:#000!important;border:1px solid #444!important;border-radius:999px!important;height:52px!important;min-height:52px!important;font-size:16px!important;padding:7px 16px!important;color:#fafafa!important}
#ot-ble-inline .ot-ble-range{margin-top:24px!important;gap:8px!important}
#ot-ble-inline .ot-ble-range>div{font-size:15px!important;font-weight:480!important;color:#aaa!important}
#ot-ble-inline .ot-ble-range output{font-size:16px!important;color:#fff!important}
#ot-ble-inline .ot-ble-range input[type=range]{height:43px!important;min-height:43px!important;touch-action:pan-y!important}
#ot-ble-inline .ot-ble-range input[type=range]::-webkit-slider-runnable-track{height:5px!important;background:#191919!important;border-radius:99px!important}
#ot-ble-inline .ot-ble-range input[type=range]::-webkit-slider-thumb{width:40px!important;height:27px!important;border-radius:999px!important;background:#fff!important;margin-top:-11px!important;box-shadow:none!important}
#ot-ble-inline .ot-ble-actions{margin-top:25px!important;gap:12px!important;grid-template-columns:1fr 1fr!important}
#ot-ble-inline .ot-ble-actions button{border-radius:999px!important;min-height:50px!important;font-size:15px!important;font-weight:560!important}
#ot-ble-inline .ot-ble-actions button:first-child{background:#f5f5f5!important;color:#111!important;border-color:#f5f5f5!important}
#ot-ble-inline .ot-ble-actions button:last-child{background:#000!important;border-color:#303030!important;color:#9f9f9f!important}
#ot-ble-inline .ot-ble-actions button:disabled{opacity:.45!important}
#ot-ble-inline .ot-ble-more{background:#000!important;border-color:#242424!important;margin-top:16px!important}
#ot-ble-inline .ot-ble-more>summary{color:#a2a2a2!important}
#ot-ble-inline .ot-ble-response{font-size:11px!important;color:#9e9e9e!important;margin-top:11px!important}
#ot-ble-inline .ot-ble-setup{gap:8px!important;margin:10px 0!important}
#ot-ble-inline .ot-ble-setup button{border-radius:999px!important;background:#000!important;border:1px solid #444!important;color:#eee!important}
.ot-native-nav{background:#000!important;border-top:1px solid #202020!important;box-shadow:none!important;padding:8px 10px calc(10px + env(safe-area-inset-bottom,0px))!important}
.ot-native-nav button{color:#888!important;font-size:10px!important;gap:5px!important;min-height:54px!important}
.ot-native-nav button svg{width:22px!important;height:22px!important}
.ot-native-nav button[aria-current=page]{background:#151515!important;color:#fff!important}
.ot-native-nav button[aria-current=page] svg{color:#fff!important}
#ot-compact-pages{padding:12px 18px calc(94px + env(safe-area-inset-bottom,0px))!important;max-width:680px!important}
#ot-compact-pages .ot-simple-top{display:none!important}
#ot-compact-pages .ot-simple-card{background:#000!important;border:1px solid #333!important;border-radius:22px!important;padding:15px!important;margin-bottom:14px!important}
#ot-compact-pages .ot-simple-stat,#ot-compact-pages .ot-simple-output{background:#000!important;border:1px solid #343434!important;border-radius:17px!important}
#ot-compact-pages .ot-simple-button{background:#f5f5f5!important;color:#111!important;border:1px solid #f5f5f5!important;border-radius:999px!important}
#ot-compact-pages .ot-simple-button.secondary{background:#000!important;color:#f2f2f2!important;border-color:#444!important}
#ot-compact-pages .ot-simple-tags span{background:#050505!important;border-color:#343434!important;color:#bcbcbc!important}
.ot-settings-page{background:#000!important;padding:8px 18px 125px!important}
.ot-settings-page h2{display:none!important}
.ot-settings-group{background:#000!important;border:1px solid #2d2d2d!important;border-radius:23px!important}
.ot-quick-action{font-size:14px!important;color:#f6f6f6!important;padding:16px!important;min-height:58px!important}
.ot-quick-action svg{color:#999!important}
body.ot-editor-active #app .ot-om-card,body.ot-editor-active #app .ot-tm-card{background:#000!important;border:1px solid #303030!important}
@media(max-width:370px){.ot-mobile-heading h1{font-size:20px!important}#ot-ble-inline .ot-ble-fields select{height:46px!important;min-height:46px!important}#ot-ble-inline .ot-ble-range{margin-top:13px!important}}
@media(prefers-reduced-motion:reduce){#ot-ble-inline button,.ot-native-nav button{transition:none!important}}
/* Live Lighting Preview CSS, shape, animation and control behavior deliberately untouched. */

`;
  css.textContent += `
/* v0.1.14 baseline — instant, explicit, high-contrast transport health.
   These declarations intentionally override all preceding OLED/theme rules. */
.ot-connected-badge.ot-status-red,
html[data-ot-theme="light"] .ot-connected-badge.ot-status-red{color:#ff545e!important}
.ot-connected-badge.ot-status-red svg{color:#ff545e!important}
.ot-connected-badge.ot-status-amber,
html[data-ot-theme="light"] .ot-connected-badge.ot-status-amber{color:#f4c35f!important}
.ot-connected-badge.ot-status-amber svg{color:#f4c35f!important}
.ot-connected-badge.ot-status-green,
html[data-ot-theme="light"] .ot-connected-badge.ot-status-green{color:#29da8b!important}
.ot-connected-badge.ot-status-green svg{color:#29da8b!important}
.ot-connection-card[data-active="false"] svg{color:#ff545e!important}
.ot-connection-card[data-active="true"] svg{color:#29da8b!important}
.ot-connection-card[data-active="false"] small{color:#ff7079!important}
.ot-connection-card[data-active="true"] small{color:#29da8b!important}
`;
  css.textContent += `
/* v0.1.23: approved 100x22 reference status chip. ONLY the badges change. */
.ot-connected-badge,
html[data-ot-theme="light"] .ot-connected-badge {
  box-sizing:border-box!important;
  display:inline-flex!important;align-items:center!important;justify-content:center!important;
  gap:4px!important;width:max-content!important;max-width:58vw!important;
  min-height:23px!important;line-height:1.1!important;
  border-radius:999px!important;padding:4px 8px!important;
  background:#071a10!important;border:1px solid #174c2e!important;
  color:#20d46c!important;font-size:11px!important;font-weight:650!important;
  letter-spacing:0!important;white-space:nowrap!important;box-shadow:none!important;
  flex-shrink:0!important;
}
.ot-connected-badge::before{content:none!important}
.ot-connected-badge svg,
.ot-connected-badge.ot-status-green svg,
.ot-connected-badge.ot-status-amber svg,
.ot-connected-badge.ot-status-red svg {
  width:12px!important;height:12px!important;flex:0 0 12px!important;
  color:currentColor!important;stroke:currentColor!important;
}
.ot-connected-badge.ot-status-green,
html[data-ot-theme="light"] .ot-connected-badge.ot-status-green{
  background:#071a10!important;border-color:#174c2e!important;color:#20d46c!important;
}
.ot-connected-badge.ot-status-amber,
html[data-ot-theme="light"] .ot-connected-badge.ot-status-amber{
  background:#201808!important;border-color:#57431a!important;color:#f6bd47!important;
}
.ot-connected-badge.ot-status-red,
html[data-ot-theme="light"] .ot-connected-badge.ot-status-red{
  background:#200c11!important;border-color:#63303a!important;color:#ff5265!important;
}
/* Approved layouts are native Studio editing surfaces, never hidden behind overview pages.
   Pure OLED black for the three reviewed tabs; keep status badges meaningful. */
html:not([data-ot-theme="light"]) body:not(.ot-settings-active) #app .ot-approved-show,
html:not([data-ot-theme="light"]) body:not(.ot-settings-active) #app .ot-approved-triggers,
html:not([data-ot-theme="light"]) body:not(.ot-settings-active) #app .ot-layout-approved,
html:not([data-ot-theme="light"]) body:not(.ot-settings-active) #app .ot-approved-show .card,
html:not([data-ot-theme="light"]) body:not(.ot-settings-active) #app .ot-approved-triggers .card,
html:not([data-ot-theme="light"]) body:not(.ot-settings-active) #app .ot-layout-approved .card {
  background:#000!important;color:#f4f4f4!important;
}
html:not([data-ot-theme="light"]) #app .ot-approved-show .card,
html:not([data-ot-theme="light"]) #app .ot-approved-triggers .card,
html:not([data-ot-theme="light"]) #app .ot-layout-approved .card {
  border-color:#303030!important;border-radius:20px!important;
}
html:not([data-ot-theme="light"]) #app .ot-core-advanced {
  background:#000!important;border:1px solid #303030!important;
  border-radius:20px!important;padding:14px!important;margin-top:14px!important;
}
#app .ot-core-advanced>summary {
  cursor:pointer;list-style:revert;font-size:14px;font-weight:650;
  color:#eee;padding:5px 3px;min-height:36px;
}
html:not([data-ot-theme="light"]) #app .ot-core-advanced[open]>summary {border-bottom:1px solid #303030;margin-bottom:10px}
/* Keep 5-tab nav, Wi-Fi/BLE cards, OLED palette and Live Preview untouched. */
@media(max-width:350px){.ot-connected-badge{font-size:10px!important;padding:4px 6px!important}}
`;
  css.textContent += "\n\n/* UI baseline v1: lock Android wrapper to neutral OLED system UI, not Android green. */\nhtml,body,html:not([data-ot-theme=\"light\"]) body {background:#000!important;color:#ececec!important;font-family:-apple-system,BlinkMacSystemFont,\"Segoe UI\",Roboto,Arial,sans-serif!important;color-scheme:dark!important}\n.ot-mobile-header{background:#000!important;color:#eee!important;padding:12px 16px 7px!important}\n.ot-mobile-heading h1{font-size:21px!important;line-height:1.25!important;font-weight:700!important;letter-spacing:0!important;color:#eee!important}\n.ot-mobile-heading p{color:#aaa!important;font-size:12px!important}\n.ot-native-nav{background:#050505!important;border-top:1px solid #303030!important;box-shadow:none!important}\n.ot-native-nav button{background:transparent!important;color:#aaa!important;font:600 11px -apple-system,BlinkMacSystemFont,\"Segoe UI\",Roboto,sans-serif!important}\n.ot-native-nav button[aria-current=page]{background:#262626!important;color:#f3f3f3!important}\n.ot-native-nav button svg{color:inherit!important}\n#app>.shell{background:#000!important;padding-left:14px!important;padding-right:14px!important}\n#app main{background:#000!important}\n#app .ot-layout-approved .card,#app .ot-approved-triggers .card,#app .ot-approved-show .card{background:#000!important;color:#eee!important;border-color:#303030!important;box-shadow:none!important}\n#app .ot-chat-nav-pill{background:#101010!important;color:#aaa!important;border-color:#353535!important;border-radius:999px!important}\n#app .ot-chat-nav-pill.is-active{background:#242424!important;border-color:#777!important;color:#f4f4f4!important}\n#app .ot-chat-trigger-action{background:#101010!important;border-color:#333!important;border-radius:19px!important;box-shadow:none!important}\n#app .ot-chat-trigger-action.is-editing{background:#242424!important;border-color:#888!important}\n#app .ot-chat-trigger-action button{box-shadow:none!important}\n#app .ot-chat-trigger-select{background:transparent!important;border:0!important}\n#app .ot-chat-pill{border-radius:999px!important}\n#app .ot-chat-input{border-radius:999px!important;color:#eee!important}\n#app .ot-chat-length input[type=range],#app .ot-chat-events input[type=range]{accent-color:#c7c7c7!important}\n#app .ot-chat-led-strip{background:#090909!important;border-color:#303030!important}\n#app .ot-chat-led-strip span{background:#424242!important;box-shadow:none!important}\n#app .ot-chat-led-strip span.on{background:#d3d3d3!important;box-shadow:none!important}\n#app .ot-chat-event-strip span{background:#424242!important}\n#app .ot-chat-remove{background:#341517!important;color:#ffa9ac!important;border-color:#9e4044!important}\n#app .ot-approved-show .ot-subnav{display:flex;gap:7px;padding:5px!important;border:1px solid #303030!important;border-radius:999px!important;box-shadow:none!important;background:#080808!important}\n#app .ot-approved-show .ot-subnav button{border-radius:999px!important;background:transparent!important;color:#ccc!important;box-shadow:none!important;min-height:37px!important}\n#app .ot-approved-show .ot-subnav button.on{background:#292929!important;border-color:#646464!important;color:#f2f2f2!important}\n#app .ot-approved-triggers>.card,#app .ot-layout-approved>.card{border-radius:20px!important}\n#app .ot-approved-triggers .ot-assignments-card,#app .ot-approved-triggers .ot-editor-card{border:0!important;border-top:1px solid #303030!important;border-radius:0!important;box-shadow:none!important}\n#app .ot-chat-zone-list{border-radius:17px!important}\n#app .ot-chat-zone-toggle{border-radius:0!important}\n@media(max-width:390px){#app .ot-chat-nav-pill{font-size:10px!important}}\n\n";
  css.textContent += "\n  /* UI baseline v2: ONE top row of six pill tabs (wrapping on narrow screens). */\n  .ot-native-nav{\n    position:sticky!important;top:0!important;left:auto!important;right:auto!important;\n    z-index:1050!important;display:flex!important;flex-wrap:wrap!important;gap:6px!important;\n    align-items:center!important;justify-content:flex-start!important;\n    max-width:960px!important;width:100%!important;margin:0 auto!important;\n    padding:8px 14px!important;background:#000!important;\n    border:0!important;border-bottom:1px solid #292929!important;box-shadow:none!important;\n  }\n  .ot-native-nav button{\n    flex:1 1 30%!important;display:flex!important;flex-direction:row!important;\n    min-width:90px!important;min-height:44px!important;gap:6px!important;\n    align-items:center!important;justify-content:center!important;\n    background:#111!important;border:1px solid #343434!important;border-radius:999px!important;\n    padding:8px!important;color:#b7b7b7!important;\n    font:600 11px/1.2 -apple-system,BlinkMacSystemFont,\"Segoe UI\",Roboto,sans-serif!important;\n  }\n  .ot-native-nav button svg{width:14px!important;height:14px!important;flex:none!important}\n  .ot-native-nav button[aria-current=page]{background:#292929!important;border-color:#777!important;color:#f2f2f2!important}\n  .ot-native-nav button:focus-visible{outline:2px solid #d7d7d7!important;outline-offset:2px!important}\n  #app .ot-chat-nav{display:none!important}\n  body:not(.ot-home-active) .ot-connection-grid{display:none!important}\n  .ot-mobile-header{padding-bottom:7px!important}\n  #app>.shell{padding-bottom:20px!important}\n  .ot-settings-page{padding-bottom:30px!important}\n  @media(min-width:700px){.ot-native-nav{padding:9px 30px!important}.ot-native-nav button{flex:1 1 auto!important}}\n  @media(max-width:350px){.ot-native-nav{padding:7px!important;gap:4px!important}.ot-native-nav button{min-width:85px!important;font-size:10px!important}}\n  html:not([data-ot-theme=\"light\"]) .ot-settings-group{background:#0d0d0d!important;border-color:#303030!important}\n  html:not([data-ot-theme=\"light\"]) .ot-quick-action{color:#ececec!important;border-color:#303030!important}\n  html:not([data-ot-theme=\"light\"]) .ot-quick-action svg{color:#bdbdbd!important}\n";
  document.head.appendChild(css);
  let route='';
  let advanced=false;
  let requestEpoch=0;
  let outputs=null,triggers=null;
  let outputError='',triggerError='';
  function routeNow() {
    if (document.body.classList.contains('ot-settings-active')) return 'settings';
    if (document.body.classList.contains('ot-auto-events-active')) return 'events';
    const name=document.querySelector('.tabs .tab.active')?.textContent?.trim() || '';
    if (/Show Modes/i.test(name)) return 'shows';
    if (/Triggers/i.test(name)) return 'triggers';
    if (/Layout/i.test(name)) return 'layout';
    if (/Dashboard/i.test(name)) return 'home';
    return 'other';
  }
  const connection=()=>{try {return JSON.parse(window.OpenTailNativeApp.getState()||'{}')} catch(_){return {}}};
  function outputCards(){
    if (!outputs) return `<p class="ot-muted">${esc(outputError||'Connect to CORE Wi-Fi to load saved pixel outputs.')}</p>`;
    return outputs.map(o=>{
      const total=Array.isArray(o.zones)?o.zones.reduce((n,z)=>n+Number(z.pixels||0),0):null;
      return `<div class="ot-simple-output"><h4>${esc(o.name||'Pixel OUT '+(Number(o.index)+1))}</h4>
        <div class="ot-simple-line"><small>GPIO${esc(o.pin)} · ${esc(o.color_order)}</small><strong>${esc(o.pixels)} pixels</strong></div>
        <div class="ot-simple-tags">${(o.zones||[]).filter(z=>z.pixels>0).map(z=>`<span>${esc(z.name)} · ${esc(z.pixels)}</span>`).join('')||'<span>No active zones</span>'}</div>
        ${total!==null&&total!==Number(o.pixels)?'<p class="ot-simple-readonly">Assigned zone totals do not match active pixels.</p>':''}</div>`;
    }).join('');
  }
  function top(title,subtitle){return `<div class="ot-simple-top"><div><h2>${title}</h2><p>${subtitle}</p></div><span class="ot-simple-tag">CORE bench</span></div>`;}
  function summaryMarkup(){
    if (route==='home') {
      const total = outputs ? outputs.reduce((n,o)=>n+Number(o.pixels||0),0) : null;
      return top('Home','Controller status')+
        `<div class="ot-simple-card"><h3>Controller overview</h3><div class="ot-simple-stats">
          <div class="ot-simple-stat"><strong>${outputs?outputs.length:'—'}</strong><small>Physical pixel outputs</small></div>
          <div class="ot-simple-stat"><strong>${total===null?'—':total.toLocaleString()}</strong><small>Configured pixels</small></div>
        </div><p class="ot-simple-readonly">${esc(outputs?'Read from CORE output configuration.':'Connect to CORE Wi-Fi to load live configuration.')}</p>
        </div>
        <div class="ot-simple-card"><h3>Lighting setup</h3>${outputCards()}</div>`;
    }
    if (route==='triggers') {
      const rows=triggers?.inputs || [];
      return top('Triggers','Eight programmable signal inputs')+`<div class="ot-simple-card"><h3>Input assignments</h3>`+
        (rows.length?rows.map(x=>`<div class="ot-simple-line"><div><strong>IN${esc(x.index)}</strong><small>GPIO${esc(x.gpio)}</small></div><span class="ot-simple-value">${esc(names[x.function]||x.function)}</span></div>`).join(''):
        `<p class="ot-muted">${esc(triggerError||'Connect to CORE Wi-Fi to view saved trigger assignments.')}</p>`)+
        `<div class="ot-simple-actions"><button class="ot-simple-button" data-ot-open="edit">Edit assignments</button><button class="ot-simple-button secondary" data-ot-open="diagnostics">Diagnostics</button></div>
        <p class="ot-simple-readonly">Overview only. Edits use Studio's existing verified CORE save and readback process.</p></div>`;
    }
    if (route==='layout') {
      return top('Layout','Outputs, zones and pixel mapping')+`<div class="ot-simple-card"><h3>Saved output configuration</h3>`+
        outputCards()+`<div class="ot-simple-actions"><button class="ot-simple-button" data-ot-open="edit">Edit outputs & zones</button><button class="ot-simple-button secondary" data-ot-open="shows">Test Show Modes</button></div>
        <p class="ot-simple-readonly">Full editing supports renaming, add/delete zones, RGB order, calibration and trigger roles.</p></div>`;
    }
    return '';
  }
  function refreshVisual(force=false){
    const next=routeNow();
    if (next!==route){advanced=false;route=next;force=true; if(['home','layout','triggers'].includes(route)) readConfig(route);}
    // Approved Layout and Triggers are full Studio editors by default, even offline.
    // Keep the legacy read-only compact overview for Home only.
    const showing=route==='home';
    document.body.classList.toggle('ot-compact-view',showing&&!advanced);
    document.body.classList.toggle('ot-editor-active',showing&&advanced);
    document.body.classList.toggle('ot-compact-shows',route==='shows');
    const mode=document.querySelector('#ot-ble-inline [data-ot-ble="ble"]')?.getAttribute('aria-pressed');
    document.body.classList.toggle('ot-ble-mode',route==='shows'&&mode==='true');
    if (showing) {
      const current=advanced?`<div class="ot-simple-top"><div><h2>${esc(route==='layout'?'Layout editor':route==='triggers'?'Trigger editor':'Advanced Dashboard')}</h2><p>Full Studio editing tools</p></div><button class="ot-simple-button secondary" data-ot-open="back">Back</button></div>`:summaryMarkup();
      if (force||panel.innerHTML!==current) panel.innerHTML=current;
      panel.style.display=advanced?'block':'';
    } else {panel.innerHTML='';panel.style.display='';}
  }
  async function readConfig(which){
    const epoch=++requestEpoch;
    if (!connection().wifi || location.hostname!=='192.168.4.1') {
      outputs=null;triggers=null;outputError=triggerError='Connect to CORE Wi-Fi to read device settings.';
      refreshVisual(true);return;
    }
    const entries=which==='triggers'?[['triggers','/api/v1/triggers']]:[['outputs','/api/v1/outputs']];
    for(const [kind,path] of entries){
      try {
        const res=await fetch(path,{method:'GET',cache:'no-store',signal:AbortSignal.timeout(4500)});
        if (!res.ok) throw Error('HTTP '+res.status);
        const val=await res.json();
        if(epoch!==requestEpoch)return;
        if (kind==='outputs') {
          if(!Array.isArray(val.outputs)||val.outputs.length!==2) throw Error('Unexpected output response');
          outputs=val.outputs;outputError='';
        } else {
          if(!Array.isArray(val.inputs)||val.inputs.length!==8) throw Error('Unexpected trigger response');
          triggers=val;triggerError='';
        }
      } catch(err){if(epoch!==requestEpoch)return;if(kind==='outputs'){outputs=null;outputError='Output read unavailable: '+err.message;}else{triggers=null;triggerError='Trigger read unavailable: '+err.message;}}
    }
    if(epoch===requestEpoch)refreshVisual(true);
  }
  document.addEventListener('click', e=>{
    const b=e.target.closest('#ot-compact-pages [data-ot-open]');
    if(b){
      const action=b.dataset.otOpen;
      if(action==='preview'){advanced=true;refreshVisual(true);requestAnimationFrame(()=>{const preview=document.querySelector('#app .card.ot-preview');if(preview)preview.scrollIntoView({block:'start',behavior:'smooth'});});return;}
      if(action==='edit'){advanced=true;refreshVisual(true);return;}
      if(action==='back'){advanced=false;readConfig(route);refreshVisual(true);return;}
      const destination={shows:'scenes',layout:'layout',diagnostics:'diagnostics'}[action];
      if(destination&&typeof window.go==='function'){window.go(destination);refreshVisual(true);}return;
    }
    if(e.target.closest('.ot-native-nav [data-ot-destination]')) requestAnimationFrame(()=>refreshVisual(true));
    if(e.target.closest('#ot-ble-inline [data-ot-ble]'))requestAnimationFrame(()=>refreshVisual(true));
  });
  const app=document.getElementById('app');
  if(app){let waiting=false;new MutationObserver(()=>{if(waiting)return;waiting=true;requestAnimationFrame(()=>{waiting=false;refreshVisual()})}).observe(app,{childList:true,subtree:false});}
  if(typeof window.OpenTailShell.navigate==='function'){
    const old=window.OpenTailShell.navigate;
    window.OpenTailShell.navigate=function(target){old(target);refreshVisual(true)};
  }
  refreshVisual(true);
  setInterval(()=>refreshVisual(),2500);
  window.OpenTailCompactUI={refresh:refreshVisual,readConfig};
  // Final cascade after Android's legacy presentation layers.
  const approvedFinalStyle=document.createElement('style');
  approvedFinalStyle.id='ot-approved-oled-final';
  approvedFinalStyle.textContent=`
    html:not([data-ot-theme=light]),html:not([data-ot-theme=light]) body{background:#000!important;color:#f7f7f7!important}
    html:not([data-ot-theme=light]) .ot-mobile-header{background:#000!important}
    html:not([data-ot-theme=light]) .ot-connection-card,
    html:not([data-ot-theme=light]) #ot-ble-inline,
    html:not([data-ot-theme=light]) .ot-settings-group,
    html:not([data-ot-theme=light]) #ot-compact-pages .ot-simple-card{background:#080808!important;border-color:#292929!important;color:#f7f7f7!important}
    html:not([data-ot-theme=light]) #app .card{background:#080808!important;border-color:#292929!important}
    html:not([data-ot-theme=light]) .ot-mobile-heading h1{color:#fff!important}
    .ot-native-nav{position:sticky!important;top:0!important;bottom:auto!important;left:auto!important;right:auto!important;
       width:100%!important;display:flex!important;overflow-x:auto!important;justify-content:flex-start!important;
       border:1px solid #292929!important;background:#080808!important;border-radius:14px!important;
       margin:10px 0 4px!important;padding:4px!important;box-shadow:none!important;gap:3px!important;box-sizing:border-box!important}
    .ot-native-nav button{flex:1 0 auto!important;min-width:66px!important;min-height:38px!important;padding:8px!important;
      color:#aaa!important;background:transparent!important;border-radius:10px!important;font-size:11px!important}
    .ot-native-nav button svg{display:none!important}
    .ot-native-nav button[aria-current=page]{background:#252525!important;color:#fff!important}
    #app>.shell{padding-bottom:24px!important}
    #ot-compact-pages,body.ot-settings-active .ot-settings-page{padding-bottom:25px!important}
    html[data-ot-theme=light] .ot-native-nav{background:#f7f7f7!important;border-color:#ddd!important}
    html[data-ot-theme=light] .ot-native-nav button{color:#444!important}
    html[data-ot-theme=light] .ot-native-nav button[aria-current=page]{background:#fff!important;color:#111!important}
  `;
  document.head.appendChild(approvedFinalStyle);
})();
