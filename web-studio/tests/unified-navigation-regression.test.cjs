// Unification regression: the approved six-destination OpenTail navigation is the only main navigation.
// Prevents the legacy Dashboard/Connect/Show Modes/Triggers/Layout/Diagnostics bar from returning.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const stable = fs.readFileSync('web-studio/opentail.htm', 'utf8');
const versioned = fs.readFileSync('web-studio/opentail-v0.19.62.htm', 'utf8');
assert.equal(stable, versioned, 'Stable deployment and versioned release must be identical');

const renderStart = stable.indexOf('function render(){');
const renderEnd = stable.indexOf('\nfunction ', renderStart + 1);
assert(renderStart >= 0 && renderEnd > renderStart, 'Main render function must exist');
const render = stable.slice(renderStart, renderEnd);
assert(!render.includes('<nav class="tabs">'), 'Legacy duplicate navigation must not render');
assert.equal((render.match(/\$\{otConceptNav\(\)\}/g) || []).length, 1,
  'Exactly one approved OpenTail navigation must render');
assert(stable.includes('aria-label="OpenTail main navigation"'),
  'Main navigation must remain accessible and labeled');

const navStart = stable.indexOf('function otConceptNav()');
const navEnd = stable.indexOf('\nfunction ', navStart + 1);
assert(navStart >= 0 && navEnd > navStart, 'Approved navigation builder must exist');
const nav = stable.slice(navStart, navEnd);
const labels = ['Home', 'Show Modes', 'Layout', 'Triggers', 'Auto Events', 'Settings'];
let cursor = -1;
for (const label of labels) {
  const next = nav.indexOf(`'${label}'`, cursor + 1);
  assert(next > cursor, `Approved navigation order missing/out of order: ${label}`);
  cursor = next;
}
assert(!stable.includes('<nav class="bottomnav"'), 'No bottom-tab navigation markup');
assert(stable.includes("otConceptNavigate('events')") || nav.includes("['events','Auto Events'"),
  'Auto Events must remain reachable in the same navigation');
console.log('PASS: one unified six-destination navigation; stable/versioned Studio copies match.');
