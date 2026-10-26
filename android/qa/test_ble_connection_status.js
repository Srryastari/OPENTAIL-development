/* Verify BLE Output Manager keeps current radio/link/auth status visible. Simulated DOM only. */
const assert = require('assert');
const fs = require('fs');
const vm = require('vm');
let radio = {radioOn:false,connected:true,authorized:true,phase:'off',message:'Previously authorized'};
let panel;
const main = {prepend(node){panel=node;},querySelector(){return null}};
const document = {
  head:{appendChild(){}},body:{},
  createElement(){return {id:'',className:'',innerHTML:''}},
  querySelector(selector){return selector==='#app main'?main:null},
  getElementById(id){return id==='ot-ble-output-card'?panel:null},
  addEventListener(){}
};
const window = {OpenTailNativeBLE:{getState(){return JSON.stringify(radio)}}};
const context = {window,document,state:{tab:'layout'},MutationObserver:class{observe(){}}};
vm.createContext(context);
vm.runInContext(fs.readFileSync('app/src/main/assets/ble-output-panel.js','utf8'),context);
assert(panel.innerHTML.includes('Bluetooth off'));
assert(panel.innerHTML.includes('data-health="red"'), 'Radio OFF must be red');
assert(panel.innerHTML.includes('Not connected'));
radio={radioOn:true,connected:false,authorized:false,phase:'scanning',message:'Scanning'};
window.OpenTailBLEOutputs.connectionChanged('Scanning for controller…');
assert(panel.innerHTML.includes('Scanning…'));
assert(panel.innerHTML.includes('data-health="amber"'), 'BLE scanning should be amber');
assert(panel.innerHTML.includes('Scanning for controller…'));
radio={radioOn:true,connected:true,authorized:false,phase:'pairing',message:'Pairing'};
window.OpenTailBLEOutputs.connectionChanged('Hold BOOT on running controller');
assert(panel.innerHTML.includes('Authorize'));
assert(panel.innerHTML.includes('Waiting for BOOT'));
radio={radioOn:true,connected:true,authorized:true,phase:'authorized',message:'Authenticated'};
window.OpenTailBLEOutputs.connectionChanged('Authorization confirmed');
assert(panel.innerHTML.includes('Connected'));
assert(panel.innerHTML.includes('data-health="green"'), 'BLE authorized green');
assert(panel.innerHTML.includes('Authorization confirmed'));
radio={radioOn:false,connected:true,authorized:true,phase:'off',message:'Prior connection stale'};
window.OpenTailBLEOutputs.connectionChanged('Bluetooth disabled');
assert(panel.innerHTML.includes('Bluetooth off'));
assert(!panel.innerHTML.includes('>Connected</span>'));
console.log('PASS: BLE Output Manager radio off, scanning, pairing, authorized, and disconnect status');
