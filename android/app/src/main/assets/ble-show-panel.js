/* OpenTail Android v0.1.10: BLE controls integrated in Studio's Show Modes screen.
   Injected only into allowlisted OpenTail Studio pages by the native Android shell.
   No modifications to firmware, saved output/trigger configuration, or Wi-Fi commands. */
(function () {
  'use strict';
  if (window.__otBleInlineVersion === '0.1.5' || !window.OpenTailNativeBLE) return;
  window.__otBleInlineVersion = '0.1.5';

  const state = {
    transport: 'ble', effect: 5, output: 2, zone: 1,
    color: '#FF0000', brightness: 15, speed: 50, ttl: 15,
    message: 'Connect BLE, pair once using BOOT in Layout, then authorize automatically.',
    running: false
  };
  const effects = ['Solid', 'Chase', 'Breathe', 'Wipe', 'Rainbow'];
  const outputs = [['1', 'Pixel OUT 1'], ['2', 'Pixel OUT 2'], ['3', 'Both outputs']];
  const durations = [5, 15, 30, 60];
  const escapeText = value => String(value).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  let status = {connected: false, authorized: false, message: ''};

  const css = document.createElement('style');
  css.id = 'ot-ble-inline-style';
  css.textContent = `
  .ot-ble-inline{border:1px solid #2d4132;border-radius:20px;padding:16px;
    background:#151e18;margin:5px 0 17px;color:#eef8f1}
  .ot-ble-inline *{box-sizing:border-box}
  .ot-ble-head{display:flex;justify-content:space-between;align-items:flex-start;gap:12px;flex-wrap:wrap}
  .ot-ble-head h2{font-size:17px;line-height:1.3;margin:0 0 4px}
  .ot-ble-head p{margin:0;color:var(--ot-muted,#9ba8a0);font-size:12px}
  .ot-ble-status{font-size:11px;padding:7px 9px;border:1px solid #5d5441;border-radius:99px;color:#e9cd8f;background:#292215;white-space:nowrap}
  .ot-ble-status.is-ready{color:#96f0bb;background:#152e20;border-color:#377b51}
  .ot-ble-switch{display:grid;grid-template-columns:1fr 1fr;gap:4px;padding:4px;background:var(--ot-recess,#0a0d0a);border-radius:13px;margin:15px 0}
  .ot-ble-switch button{min-height:43px;border:0;background:transparent;border-radius:10px;color:var(--ot-muted,#aab4b0);font:inherit;font-weight:650}
  .ot-ble-switch button[aria-pressed=true]{background:#236a40;color:#fff}
  .ot-ble-fields{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:13px}
  .ot-ble-fields label{display:flex;flex-direction:column;gap:7px;font-size:12px;font-weight:650;min-width:0}
  .ot-ble-fields .ot-ble-wide{grid-column:span 2}
  .ot-ble-fields select,.ot-ble-fields input[type=color]{width:100%;min-width:0;height:46px;border:1px solid var(--ot-stroke,#434942);border-radius:11px;background:#0c1510;color:#f5fff7;padding:8px}
  .ot-ble-fields input[type=color]{padding:4px;cursor:pointer}
  .ot-ble-range{margin-top:16px;display:grid;gap:6px}
  .ot-ble-range>div{display:flex;justify-content:space-between;align-items:center;gap:8px;font-size:12px;font-weight:650}
  .ot-ble-range output{font-variant-numeric:tabular-nums;color:var(--ot-muted,#c6cec9)}
  .ot-ble-range input{width:100%;min-height:24px;accent-color:#59d989}
  .ot-ble-actions{display:grid;grid-template-columns:1fr 1fr;gap:9px;margin-top:15px}
  .ot-ble-actions button,.ot-ble-setup button{min-height:47px;border-radius:12px;border:1px solid var(--ot-stroke,#4d6d57);font:inherit;font-size:13px;font-weight:700;cursor:pointer}
  .ot-ble-actions button:first-child{background:#27774a;border-color:#27774a;color:#fff}
  .ot-ble-actions button:last-child{background:var(--ot-recess,#192019);color:var(--ot-text,#fff)}
  .ot-ble-setup{display:flex;flex-wrap:wrap;gap:8px;margin:10px 0 14px}
  .ot-ble-setup button{background:var(--ot-recess,#1d251e);color:var(--ot-text,#fff);padding:0 14px}
  .ot-ble-notice{font-size:11px;line-height:1.5;color:var(--ot-muted,#bdc5c0);margin-top:12px}
  .ot-ble-response{font-size:12px;color:var(--ot-muted,#c4d5ca);margin-top:11px;min-height:20px;overflow-wrap:anywhere}
  .ot-ble-wifi-note{font-size:12px;line-height:1.5;color:var(--ot-muted,#b0bbb5)}
  .ot-ble-inline{padding:14px!important;border-radius:17px!important;margin:4px 0 12px!important}
  .ot-ble-head h2{font-size:17px!important}
  .ot-ble-switch{margin:12px 0!important}
  .ot-ble-fields{gap:10px!important}
  .ot-ble-fields select,.ot-ble-fields input[type=color]{height:44px!important}
  .ot-ble-more{margin-top:12px;border:1px solid #34503b;border-radius:11px;padding:0 12px}
  .ot-ble-more>summary{display:flex;align-items:center;min-height:44px;font-size:12px;font-weight:650;color:#a9e1b8;cursor:pointer}
  .ot-ble-extra-fields{margin-top:2px}
  .ot-ble-more .ot-ble-range{margin:14px 0!important}
  .ot-ble-actions{margin-top:13px!important}
  .ot-ble-notice{font-size:10px!important;margin-top:9px!important}
  [data-ot-theme=light] .ot-ble-more{border-color:#c4dfcb}
  [data-ot-theme=light] .ot-ble-inline{background:#fff;border-color:#d9dfdc;color:#1d2820}
  [data-ot-theme=light] .ot-ble-switch{background:#edf1ef}
  [data-ot-theme=light] .ot-ble-fields select,[data-ot-theme=light] .ot-ble-fields input[type=color]{background:#fff;color:#1b2920;border-color:#c6cfcb}
  [data-ot-theme=light] .ot-ble-setup button,[data-ot-theme=light] .ot-ble-actions button:last-child{background:#f2f6f3;color:#1b3023}
  [data-ot-theme=light] .ot-ble-status:not(.is-ready){background:#fff6e6;color:#855714}
  @media(max-width:365px){.ot-ble-fields{gap:9px}}
  `;
  document.head.appendChild(css);

  const options = (values, current) => values.map(([value, name]) => `<option value="${value}" ${String(value) === String(current) ? 'selected' : ''}>${escapeText(name)}</option>`).join('');
  function currentStatus() {
    try {
      const raw = window.OpenTailNativeBLE.getState();
      if (raw) status = JSON.parse(raw);
    } catch (_) { /* Preserve last known state; do not report authorization success. */ }
    return status;
  }
  function isShows() {
    return document.querySelector('.tabs .tab.active')?.textContent?.includes('Show Modes') ||
      document.querySelector('.bottomnav button.active')?.textContent?.includes('Show Modes');
  }
  function markup() {
    const s = currentStatus();
    const ready = Boolean(s.connected && s.authorized);
    const ble = state.transport === 'ble';
    return `<div class="ot-ble-head"><div><h2>Show Modes</h2><p>One place for Wi-Fi and Bluetooth lighting</p></div><button type="button" data-ot-ble="settings" class="ot-ble-settings" aria-label="OpenTail settings" title="Settings">⚙</button><span class="ot-ble-status ${ready ? 'is-ready' : ''}" role="status">${ready ? 'BLE READY' : s.connected ? 'BLE CONNECTED' : 'BLE DISCONNECTED'}</span></div>
      <div class="ot-ble-switch" role="group" aria-label="Lighting connection method">
        <button type="button" data-ot-ble="wifi" aria-pressed="${!ble}">Wi-Fi</button>
        <button type="button" data-ot-ble="ble" aria-pressed="${ble}">Bluetooth</button>
      </div>
      ${ble ? `${!ready ? `<div class="ot-ble-setup">
          ${!s.connected ? '<button type="button" data-ot-ble="find">Find controller</button>' : ''}
          <button type="button" data-ot-ble="authorize">Enable BLE lighting</button>
        </div>` : ''}
        <div class="ot-ble-fields">
          <label>Output<select data-ot-field="output">${options(outputs,state.output)}</select></label>
          <label>Zone<select data-ot-field="zone">${options([['255','All zones'],...Array.from({length:8},(_,i)=>[String(1<<i),'Zone '+(i+1)])],state.zone)}</select></label>
          <label class="ot-ble-wide">Effect<select data-ot-field="effect">${options(effects.map((v,i) => [String(i+1),v]),state.effect)}</select></label>
        </div>
        <div class="ot-ble-range"><div><span>Brightness</span><output id="ot-ble-brightness-out">${state.brightness}%</output></div>
          <input data-ot-field="brightness" type="range" min="0" max="100" value="${state.brightness}" aria-label="BLE brightness percentage (firmware bench capped)"></div>
        <div class="ot-ble-range"><div><span>Speed</span><output id="ot-ble-speed-out">${state.speed}%</output></div>
          <input data-ot-field="speed" type="range" min="0" max="100" value="${state.speed}" aria-label="BLE effect speed percentage"></div>
        <details class="ot-ble-more"><summary>Customize</summary>
          <div class="ot-ble-fields ot-ble-extra-fields">
            <label>Color<input data-ot-field="color" type="color" value="${state.color}" aria-label="BLE effect color"></label>
            <label>Automatic stop<select data-ot-field="ttl">${options(durations.map(n=>[String(n),n+' seconds']),state.ttl)}</select></label>
          </div>
        </details>
        <div class="ot-ble-actions"><button type="button" data-ot-ble="start" aria-label="Start effect">▶ Start</button><button type="button" data-ot-ble="stop">■ Stop</button></div>
        <p class="ot-ble-response" id="ot-ble-result" role="status" aria-live="polite">${escapeText(ready && state.message.startsWith('Connect BLE') ? 'BLE authorized · Ready to start' : (state.message || s.message || 'Ready'))}</p>
        <p class="ot-ble-notice">BENCH ONLY · Pair once using BOOT in Layout, then Bluetooth reauthorizes with a saved key. Initial pairing is not MITM-resistant. Bluetooth effects expire within 60s. Hardware triggers override shows. Brightness is NOT an electrical current limiter.</p>
      ` : `<p class="ot-ble-wifi-note">Use the existing CORE Native Effects controls directly below. Wi-Fi control is unchanged.</p>`}`;
  }
  function insert() {
    if (!isShows()) return;
    const main = document.querySelector('#app main');
    if (!main) return;
    let el = main.querySelector('#ot-ble-inline');
    if (!el) {
      el = document.createElement('section');
      el.id = 'ot-ble-inline';
      el.className = 'ot-ble-inline';
      el.setAttribute('aria-label', 'OpenTail Bluetooth lighting controls');
      const native = main.querySelector('.ot-native-show');
      // Always put the unified Show Modes card first, before Studio's long editing workspace.
      main.insertBefore(el, main.firstChild);
      el.innerHTML = markup();
    }
    // Keep advanced controls intact, positioned below the primary Start/Stop actions.
    const actions=el.querySelector?.('.ot-ble-actions'),extras=el.querySelector?.('.ot-ble-more');
    if(actions&&extras)actions.after(extras);
    const native = main.querySelector('.ot-native-show');
    if (native) native.style.display = state.transport === 'ble' ? 'none' : '';
  }
  function repaint() {
    const el = document.getElementById('ot-ble-inline');
    if (el) el.innerHTML = markup();
    insert();
  }
  function display(message) {
    state.message = String(message || '');
    const out = document.getElementById('ot-ble-result');
    if (out) out.textContent = state.message;
  }
  window.OpenTailBLEPanel = {
    setTransport: function(method){if(method==='wifi'||method==='ble'){state.transport=method;repaint();}},
    getTransport: function(){return state.transport;},
    statusChanged: function (message) {
      currentStatus();
      display(message);
      const chip = document.querySelector('.ot-ble-status');
      if (chip) {
        chip.textContent = status.connected && status.authorized ? 'BLE READY' : status.connected ? 'BLE CONNECTED' : 'BLE DISCONNECTED';
        chip.classList.toggle('is-ready',Boolean(status.connected && status.authorized));
      }
    }
  };

  // Delegated event handlers work even when Studio re-renders an entire tab.
  document.addEventListener('click', function (event) {
    const button = event.target.closest('#ot-ble-inline [data-ot-ble]');
    if (!button) return;
    const action = button.getAttribute('data-ot-ble');
    if (action === 'wifi' || action === 'ble') {
      state.transport = action;
      repaint();
      return;
    }
    if (action === 'settings') { window.OpenTailShell?.navigate('settings'); return; }
    if (action === 'find') { window.OpenTailNativeBLE.scan(); display('Scanning for OpenTail CORE…'); return; }
    if (action === 'authorize') { window.OpenTailNativeBLE.authorize(); display('Authorizing using saved BLE owner key…'); return; }
    if (action === 'stop') { window.OpenTailNativeBLE.stop(); display('Sending stop command…'); return; }
    if (action === 'start') {
      const rgb = parseInt(state.color.slice(1), 16);
      if (!/^[0-9a-f]{6}$/i.test(state.color.slice(1))) { display('Choose a valid RGB color.'); return; }
      const byte = Math.round(state.brightness * 32 / 100); // Existing firmware brightness cap 0..32.
      window.OpenTailNativeBLE.start(state.effect,state.output,state.zone,
        (rgb>>16)&255,(rgb>>8)&255,rgb&255,byte,state.speed,state.ttl);
      display('Sending BLE lighting command…');
    }
  });
  document.addEventListener('input', function (event) {
    const field = event.target.closest('#ot-ble-inline [data-ot-field]');
    if (!field) return;
    const key = field.getAttribute('data-ot-field');
    if (key === 'color') state.color = field.value;
    else state[key] = Number(field.value);
    if (key === 'brightness') {
      const out = document.getElementById('ot-ble-brightness-out');
      if (out) out.textContent = state.brightness + '%';
    }
    if (key === 'speed') {
      const out = document.getElementById('ot-ble-speed-out');
      if (out) out.textContent = state.speed + '%';
    }
  });
  document.addEventListener('change', function (event) {
    const field = event.target.closest('#ot-ble-inline [data-ot-field]');
    if (!field) return;
    const key = field.getAttribute('data-ot-field');
    state[key] = key === 'color' ? field.value : Number(field.value);
  });

  let queued = false;
  const observer = new MutationObserver(function () {
    if (queued) return;
    queued = true;
    requestAnimationFrame(function () { queued = false; insert(); });
  });
  const app = document.getElementById('app');
  if (app) observer.observe(app,{childList:true,subtree:true});
  insert();
})();
