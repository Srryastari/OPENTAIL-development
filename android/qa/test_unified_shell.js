/* Source-behavior smoke checks for OpenTail unified shell; not a real Android WebView test. */
const assert = require('assert');
const vm = require('vm');
const fs = require('fs');
const path = require('path');
const shellSource = fs.readFileSync(path.join(__dirname,'../app/src/main/assets/unified-shell.js'),'utf8');
const handlers = {};
const calls = [];
const nodes = {};
const emptyClassList = () => ({values: new Set(),add(n){this.values.add(n)}, remove(n){this.values.delete(n)},contains(n){return this.values.has(n)},toggle(n,v){ if(v)this.add(n);else this.remove(n)}});
const body = {classList:emptyClassList(),prepend(node){nodes.header=node},appendChild(node){nodes[node.className]=node},insertBefore(node){nodes[node.id||node.className]=node}};
const navButtons = ['home','scenes','layout','builder','events','settings'].map(t=>({dataset:{otDestination:t},setAttribute(k,v){this[k]=v}}));
const statusNodes = {
  'ot-connected-badge':{classList:emptyClassList(),dataset:{},textContent:'',setAttribute(k,v){this[k]=v}},
  'ot-wifi-text':{textContent:''},'ot-ble-text':{textContent:''},
  'ot-wifi-card':{dataset:{}},'ot-ble-card':{dataset:{}},
  'ot-save-banner':{hidden:true}
};
let currentPage='home';
let connectionState={wifi:false,ble:true,bleAuthorized:true,bleRadioOn:true,blePhase:'authorized'};
const document = {
  documentElement:{},head:{appendChild(){}},body,
  createElement(name){return {className:'',innerHTML:'',id:'',style:{},classList:emptyClassList(),setAttribute(){},querySelectorAll(sel){return name==='nav'?navButtons:[]}}},
  getElementById(id){return id==='app'?{}:statusNodes[id]||null},
  querySelector(sel){return sel==='.tabs .tab.active'?{textContent:{home:'Dashboard',scenes:'Show Modes',builder:'Triggers',layout:'Layout',diagnostics:'Diagnostics',device:'Connect'}[currentPage]}:null},
  addEventListener(type,handler){(handlers[type] ||= []).push(handler)}
};
const native = {
  getState(){return JSON.stringify(connectionState)},
  connectWifi(){calls.push('connectWifi')},wifiSettings(){calls.push('wifiSettings')},
  scanBle(){calls.push('scanBle')},authorizeBle(){calls.push('authorizeBle')},
  appSettings(){calls.push('appSettings')},offlineStudio(){calls.push('offlineStudio')}
};
const context = {document,window:{OpenTailNativeApp:native,go(page){currentPage=page;calls.push('go:'+page)},scrollTo(){}},MutationObserver:class {observe(){}},requestAnimationFrame(fn){fn()},setInterval(){},location:{hostname:"localhost"}};
vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(__dirname,'../app/src/main/assets/unified-shell.js'),'utf8'),context);
assert(nodes.header.innerHTML.includes('<h1>OpenTail</h1>'));
assert(nodes.header.innerHTML.includes('Bluetooth'));
assert(nodes['ot-settings-page'].innerHTML.includes('Android v0.1.29'));
assert(nodes['ot-native-nav'].innerHTML.includes('Layout'));
assert(nodes['ot-native-nav'].innerHTML.includes('Settings'));
assert(nodes['ot-native-nav'].innerHTML.includes('Auto Events'));
assert(shellSource.includes('.ot-native-nav{position:relative;'), 'Android navigation must be in-flow, not fixed to the bottom');
assert(!/\.ot-native-nav\{position:fixed;bottom:0/.test(shellSource), 'Fixed bottom-tab navigation is forbidden by the approved UI lock');
assert(shellSource.includes('border-radius:999px'), 'Unified navigation must use pill geometry');
assert(calls.includes('go:home'),'default Home view');
assert(statusNodes['ot-connected-badge'].innerHTML.includes('Connected'));
assert(statusNodes['ot-connected-badge'].dataset.health === 'green');
assert.strictEqual(statusNodes['ot-connected-badge'].title,'Bluetooth connected and authorized');
assert(!statusNodes['ot-connected-badge'].classList.contains('offline'));
function setConnection(next){connectionState={...connectionState,...next};context.window.OpenTailShell.refresh();}
setConnection({bleRadioOn:false,ble:false,bleAuthorized:false,blePhase:'off',wifi:false});
assert(statusNodes['ot-connected-badge'].innerHTML.includes('Bluetooth off'),'Bluetooth turned off must clear green Connected badge');
assert(statusNodes['ot-ble-text'].textContent==='Bluetooth is off');
assert(statusNodes['ot-ble-card'].dataset.active==='false');
assert(statusNodes['ot-connected-badge'].classList.contains('ot-status-red'), 'Bluetooth OFF is red immediately');
setConnection({wifi:true});
assert(statusNodes['ot-connected-badge'].innerHTML.includes('Bluetooth off'), 'BLE OFF must remain explicitly red even if Wi-Fi is online');
assert(statusNodes['ot-connected-badge'].classList.contains('ot-status-red'), 'Bluetooth OFF always appears red');
assert(statusNodes['ot-wifi-text'].textContent==='CORE Wi-Fi connected','Wi-Fi transport is still shown separately');
setConnection({bleRadioOn:true,blePhase:'disconnected',wifi:true});
assert(statusNodes['ot-connected-badge'].innerHTML.includes('Wi-Fi connected'), 'Wi-Fi may show green once Bluetooth radio is back on');
assert(statusNodes['ot-connected-badge'].classList.contains('ot-status-green'));
setConnection({wifi:false,bleRadioOn:true,blePhase:'connecting'});
assert(statusNodes['ot-connected-badge'].innerHTML.includes('Connecting…'));
assert(statusNodes['ot-connected-badge'].classList.contains('ot-status-amber'),'Connecting amber without a false green badge');
setConnection({wifi:true});
assert(statusNodes['ot-connected-badge'].innerHTML.includes('Connecting…'),'BLE progress takes priority over Wi-Fi');
assert(statusNodes['ot-connected-badge'].classList.contains('ot-status-amber'));
setConnection({wifi:false});
setConnection({ble:true,bleAuthorized:false,blePhase:'connected'});
assert(statusNodes['ot-connected-badge'].innerHTML.includes('Authorize'));
assert(statusNodes['ot-connected-badge'].classList.contains('ot-status-amber'),'Unauthorised BLE must not show ready-green');
setConnection({ble:true,bleAuthorized:true,blePhase:'authorized'});
assert(statusNodes['ot-connected-badge'].innerHTML.includes('Connected'));
assert(statusNodes['ot-connected-badge'].dataset.health === 'green');
assert.strictEqual(statusNodes['ot-connected-badge'].title,'Bluetooth connected and authorized');
assert(statusNodes['ot-connected-badge'].classList.contains('ot-status-green'),'Authorized BLE green');

setConnection({blePhase:'saving'});
assert(statusNodes['ot-connected-badge'].innerHTML.includes('Saving…'),'Saving should report active transfer');
assert(statusNodes['ot-connected-badge'].classList.contains('ot-status-amber'),'Saving must not show connected green');
setConnection({blePhase:'reading'});
assert(statusNodes['ot-connected-badge'].innerHTML.includes('Syncing…'),'Reading should report active transfer');
assert(statusNodes['ot-connected-badge'].classList.contains('ot-status-amber'));
setConnection({blePhase:'authorized'});
setConnection({ble:false,bleAuthorized:false,blePhase:'disconnected'});
assert(statusNodes['ot-connected-badge'].innerHTML.includes('Disconnected'),'Lost BLE connection must show disconnected');
assert(statusNodes['ot-connected-badge'].classList.contains('ot-status-red'));
setConnection({wifi:true});
assert(statusNodes['ot-connected-badge'].innerHTML.includes('Wi-Fi connected'),'Independent Wi-Fi remains available after BLE disconnect');
assert(statusNodes['ot-connected-badge'].classList.contains('ot-status-green'));
setConnection({wifi:false,ble:true,bleAuthorized:true,blePhase:'authorized'});

assert.strictEqual(statusNodes['ot-ble-text'].textContent,'Ready · Authorized');
function click(target){for(const handler of handlers.click||[])handler({target:{closest(selector){if(target.selector===selector)return target.node;return null}}})}
click({selector:'.ot-native-nav [data-ot-destination]',node:{dataset:{otDestination:'settings'}}});
assert(body.classList.values.has('ot-settings-active'));
click({selector:'[data-ot-native]',node:{dataset:{otNative:'scan'}}});
assert(calls.includes('scanBle'));
click({selector:'.ot-native-nav [data-ot-destination]',node:{dataset:{otDestination:'builder'}}});
assert(!body.classList.values.has('ot-settings-active'));
assert(calls.includes('go:builder'));
assert(!body.classList.values.has('ot-compact-view'),'Triggers must show the full Studio editor, not a redundant overview');
assert(!body.classList.values.has('ot-editor-active'),'Only Home controls its compact-mode editor state');
click({selector:'.ot-native-nav [data-ot-destination]',node:{dataset:{otDestination:'layout'}}});
assert(calls.includes('go:layout'),'Layout must route to the full Studio editor');
assert(!body.classList.values.has('ot-compact-view'),'Layout must not hide its primary editor');
click({selector:'.ot-native-nav [data-ot-destination]',node:{dataset:{otDestination:'scenes'}}});
assert(calls.includes('go:scenes'),'Show Modes remains directly accessible');
console.log('PASS: Six-tab shell, OLED statuses, Settings, BLE scan and full Triggers/Layout editors.');
