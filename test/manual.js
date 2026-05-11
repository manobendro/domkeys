#!/usr/bin/env node
'use strict';

// Manual live test: starts the hook and prints each event in a readable
// column format. Press Ctrl+C to stop — the SIGINT handler shuts down
// the hook cleanly and prints stats.
//
//   node test/manual.js
//   node test/manual.js --down       # only keydown
//   node test/manual.js --up         # only keyup
//   node test/manual.js --code KeyA  # filter by DOM code substring
//   node test/manual.js --raw        # also dump nativeKeyCode/nativeScanCode

const hook = require('..');

const args = process.argv.slice(2);
const hasFlag = (name) => args.includes(name);
const flagValue = (name) => {
  const i = args.indexOf(name);
  return i >= 0 ? args[i + 1] : null;
};

const showDown = !hasFlag('--up') || hasFlag('--down');
const showUp   = !hasFlag('--down') || hasFlag('--up');
const codeFilter = flagValue('--code');
const showRaw = hasFlag('--raw');

let total = 0;
let downCount = 0;
let upCount = 0;
const startedAt = Date.now();

function pad(s, n) {
  s = String(s);
  return s.length >= n ? s : s + ' '.repeat(n - s.length);
}

function modBadge(ev) {
  const mods = [];
  if (ev.ctrlKey)  mods.push('CTRL');
  if (ev.altKey)   mods.push('ALT');
  if (ev.shiftKey) mods.push('SHIFT');
  if (ev.metaKey)  mods.push('META');
  if (ev.capsLock) mods.push('CAPS');
  return mods.length ? '[' + mods.join('+') + ']' : '';
}

function fmt(ev) {
  const arrow = ev.type === 'keydown' ? 'v' : '^';
  // JSON.stringify makes whitespace/non-printables visible: " " for space, "" etc.
  const keyStr = JSON.stringify(ev.key);
  const repeat = ev.repeat ? ' (repeat)' : '';
  const raw = showRaw
    ? ` nat=0x${ev.nativeKeyCode.toString(16).padStart(2, '0')} scan=0x${ev.nativeScanCode.toString(16).padStart(4, '0')}`
    : '';
  return [
    arrow,
    pad(ev.type,   8),
    pad(ev.code || '?',   16),
    'key=' + pad(keyStr, 10),
    'kc=' + pad(ev.keyCode, 4),
    'loc=' + ev.location,
    modBadge(ev),
    repeat + raw,
  ].join(' ');
}

function header() {
  console.log('domkeys — manual key test');
  console.log('Press keys to see events. Press Ctrl+C to exit.');
  if (codeFilter) console.log(`Filtering: code contains "${codeFilter}"`);
  if (!showDown) console.log('Filtering: keydown hidden');
  if (!showUp)   console.log('Filtering: keyup hidden');
  if (process.platform === 'darwin') {
    console.log('macOS: needs Accessibility / Input Monitoring permission for your terminal (or node).');
  }
  console.log('');
}

function shouldShow(ev) {
  if (ev.type === 'keydown' && !showDown) return false;
  if (ev.type === 'keyup'   && !showUp)   return false;
  if (codeFilter && !ev.code.includes(codeFilter)) return false;
  return true;
}

function onEvent(ev) {
  total++;
  if (ev.type === 'keydown') downCount++;
  else if (ev.type === 'keyup') upCount++;
  if (!shouldShow(ev)) return;
  // Surface anything unmapped so missing-key bugs are visible.
  if (!ev.code) console.log(fmt(ev), '  <-- UNMAPPED native keycode');
  else console.log(fmt(ev));
}

function shutdown(code = 0) {
  const seconds = ((Date.now() - startedAt) / 1000).toFixed(1);
  console.log('');
  console.log(`Stopped after ${seconds}s. ${total} events total (${downCount} down, ${upCount} up).`);
  try { hook.stop(); } catch (_) {}
  process.exit(code);
}

header();

try {
  hook.on('keydown', onEvent);
  hook.on('keyup',   onEvent);
} catch (err) {
  console.error('Failed to start hook:', err.message);
  if (process.platform === 'darwin') {
    console.error('');
    console.error('Open System Settings -> Privacy & Security and grant');
    console.error('  - Accessibility, AND');
    console.error('  - Input Monitoring');
    console.error('to your terminal (e.g. Terminal.app, iTerm) or to the node binary directly.');
  }
  process.exit(1);
}

process.on('SIGINT',  () => shutdown(0));
process.on('SIGTERM', () => shutdown(0));
