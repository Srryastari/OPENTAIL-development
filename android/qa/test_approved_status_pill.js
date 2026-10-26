/* Assert the approved minimal status chip is implemented in both places. */
const assert=require('node:assert/strict'),fs=require('node:fs');
const shell=fs.readFileSync('app/src/main/assets/unified-shell.js','utf8');
const bom=fs.readFileSync('app/src/main/assets/ble-output-panel.js','utf8');
assert(shell.includes('max-width:58vw!important'),'compact header chip cannot overlap heading');
assert(shell.includes('min-height:23px!important'),'reference approx 22px height');
assert(shell.includes('width:12px!important;height:12px!important'),'reference small matching icon');
for(const color of ['#20d46c','#f6bd47','#ff5265'])assert(shell.includes(color)&&bom.includes(color),`missing ${color}`);
assert(shell.includes("radioOff ? 'Bluetooth off'"),'physical Bluetooth off precedence');
assert(shell.includes("bleInProgress ?"),'BLE progress should beat Wi-Fi badge fallback');
assert(shell.includes("phase === 'permission'"),'permission is not ready');
assert(bom.includes('const chipText'),'Bluetooth Output Manager compact badge');
assert(!shell.includes('setTimeout(refresh'),'native status should be event driven');
console.log('PASS: approved compact pill matches shell + BLE Output Manager, colors, icon sizing and live-state priority');
