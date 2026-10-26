/* Smoke test of Android-injected panel event routing, not a WebView or hardware test. */
const fs = require('fs');
const vm = require('vm');
const assert = require('assert');
const actions = [];
let showMode = true;
const handlers = {};
const nativePanel = { style: {} };
const main = {
  firstChild: {},
  panel: null,
  querySelector(s) {
    if (s === '#ot-ble-inline') return this.panel;
    if (s === '.ot-native-show') return nativePanel;
    return null;
  },
  insertBefore(el) { this.panel = el; },
};
nativePanel.parentElement = main;
const app = {};
const document = {
  head: {appendChild(){}},
  createElement() { return {style: {},innerHTML:'', setAttribute(){}}; },
  querySelector(s) {
    if (s === '.tabs .tab.active') return showMode ? {textContent: 'Show Modes'} : {textContent:'Layout'};
    if (s === '#app main') return main;
    if (s === '.ot-ble-status') return null;
    return null;
  },
  getElementById(s) {
    if (s === 'app') return app;
    if (s === 'ot-ble-inline') return main.panel;
    return null;
  },
  addEventListener(name,fn) { handlers[name] = fn; }
};
class MutationObserver { observe() {} }
const bridge = {
  getState: () => JSON.stringify({connected:true,authorized:true,message:'BLE · OK AUTH'}),
  scan: () => actions.push(['scan']),
  authorize: () => actions.push(['authorize']),
  stop: () => actions.push(['stop']),
  start: (...args) => actions.push(['start',...args]),
};
const ctx = {document, MutationObserver, requestAnimationFrame(fn){fn();}, window:{OpenTailNativeBLE:bridge}};
vm.createContext(ctx);
vm.runInContext(fs.readFileSync('app/src/main/assets/ble-show-panel.js','utf8'),ctx);
assert(main.panel, 'BLE controls must mount inside Show Modes');
assert(main.panel.innerHTML.includes('Wi-Fi') && main.panel.innerHTML.includes('Bluetooth'));
assert(main.panel.innerHTML.includes('Start effect'));
assert.strictEqual(nativePanel.style.display, 'none','BLE tab hides duplicate Wi-Fi CORE panel');
function eventFor(key, value, type='click') {
  const field = type === 'click' ? 'data-ot-ble' : 'data-ot-field';
  handlers[type]({target:{closest(){return {getAttribute(name){return name===field?key:null},value};}}});
}
eventFor('start');
assert.deepStrictEqual(actions.pop(), ['start',5,2,1,255,0,0,5,50,15]);
eventFor('effect','5','change');
eventFor('zone','1','change');
eventFor('color','#2255AA','change');
eventFor('brightness','10','change');
eventFor('ttl','5','change');
eventFor('start');
assert.deepStrictEqual(actions.pop(), ['start',5,2,1,34,85,170,3,50,5]);
eventFor('stop');
assert.deepStrictEqual(actions.pop(), ['stop']);
eventFor('authorize');
assert.deepStrictEqual(actions.pop(), ['authorize']);
eventFor('wifi');
assert.strictEqual(nativePanel.style.display, '', 'Wi-Fi native show panel must be restored');
assert(main.panel.innerHTML.includes('Use the existing CORE Native Effects'));
eventFor('ble');
assert.strictEqual(nativePanel.style.display,'none');
showMode = false;
console.log('PASS: Inline BLE controls mount, map commands, stop, authorize, switch Wi-Fi/BLE without modal.');
