'use strict';

// Quick smoke test: loads the native binding, verifies the conversion table
// works, and starts the hook for 10 seconds while printing events.

const hook = require('..');

console.log('domkeys smoke test');

// Conversion sanity checks (no permissions needed).
const checks = [
  ['mac 0x00 -> code', hook.codeFromMacKeycode(0x00), 'KeyA'],
  ['mac 0x7B -> code', hook.codeFromMacKeycode(0x7b), 'ArrowLeft'],
  ['mac 0x24 -> code', hook.codeFromMacKeycode(0x24), 'Enter'],
  ['win scan 0x001e -> code', hook.codeFromWindowsScanCode(0x001e), 'KeyA'],
  ['win scan 0xE04B -> code', hook.codeFromWindowsScanCode(0xe04b), 'ArrowLeft'],
  ['code KeyA -> vk', hook.legacyKeyCodeFromCode('KeyA'), 0x41],
];

let pass = 0;
for (const [label, got, want] of checks) {
  const ok = got === want;
  console.log(`  ${ok ? 'PASS' : 'FAIL'}  ${label}: got=${JSON.stringify(got)} want=${JSON.stringify(want)}`);
  if (ok) pass++;
}
console.log(`Converter: ${pass}/${checks.length} passed.`);

if (process.argv.includes('--listen')) {
  console.log('\nListening for keydown/keyup for 10s (press keys)…');
  console.log('macOS: requires Accessibility / Input Monitoring permission for the host process.');
  hook.on('keydown', (ev) => {
    console.log('  ↓', ev.code, ev.key, `kc=${ev.keyCode}`, ev.shiftKey ? '[shift]' : '');
  });
  hook.on('keyup', (ev) => {
    console.log('  ↑', ev.code, ev.key, `kc=${ev.keyCode}`);
  });
  setTimeout(() => {
    hook.stop();
    console.log('Stopped.');
    process.exit(0);
  }, 10000);
} else {
  console.log('\n(pass --listen to also run the live hook test)');
}
