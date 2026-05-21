# domkeys

> Native Node.js NAPI global keyboard hook for **macOS** and **Windows** that delivers Chromium-compatible `KeyboardEvent` data (DOM `code`, `key`, legacy `keyCode`).

[![ci](https://img.shields.io/github/actions/workflow/status/manobendro/domkeys/ci.yml?branch=main&label=ci)](#)
[![npm](https://img.shields.io/npm/v/domkeys.svg)](https://www.npmjs.com/package/domkeys)
[![license](https://img.shields.io/npm/l/domkeys.svg)](./LICENSE)

`domkeys` listens to the OS-level keyboard stream and produces events shaped exactly like the browser's `KeyboardEvent` — so the values you read (`event.code === 'KeyA'`, `event.key === 'a'`) are identical to what a focused web page would see. The keycode conversion table is a **fork** of Chromium's `ui/events/keycodes/dom/` (trimmed to macOS + Windows), kept isolated in `src/keycodes/` rather than dragged in as a full Chromium dependency.

**Use cases**
- Text-expansion / snippet engines (Espanso-style, AutoHotkey-style)
- Global hotkeys / shortcut managers in Electron or standalone Node
- Input recorders, productivity dashboards, key-press visualizers
- Anything that wants browser-grade key event semantics without running in a browser

**Not a goal**
- Sending synthetic input (use `nut.js`, `robotjs`, or `SendInput`/`CGEventPost` directly).
- Suppressing or rewriting keystrokes (the hook is **listen-only** by design — see [Non-intrusive design](#non-intrusive-design)).
- IME composition / dead-key composed glyphs — structurally impossible from a global hook (see [Limitations](#limitations)).

---

## Table of contents

- [Install](#install)
- [Quick start](#quick-start)
- [API](#api)
- [Event shape](#event-shape)
- [Platform setup & permissions](#platform-setup--permissions)
- [Keyboard layouts & IME](#keyboard-layouts--ime)
- [Non-intrusive design](#non-intrusive-design)
- [Limitations](#limitations)
- [Build from source](#build-from-source)
- [Project layout](#project-layout)
- [How it compares](#how-it-compares)
- [FAQ](#faq)
- [License](#license)

---

## Install

```bash
npm install domkeys
```

Prebuilt binaries are shipped for:

| Platform   | Architectures |
|------------|---------------|
| macOS      | `arm64`, `x64` |
| Windows    | `x64` |

If a prebuild matches your platform, `npm install` is instant. Otherwise the package falls back to building from source via `node-gyp` (needs Python 3 + a C++17 toolchain — Xcode Command Line Tools on macOS, MSVC Build Tools on Windows).

**Requirements:** Node ≥ 18.

---

## Quick start

```js
const hook = require('domkeys');

hook.on('keydown', (ev) => {
  console.log(`↓ ${ev.code} (${JSON.stringify(ev.key)}) kc=${ev.keyCode}`);
  // ↓ KeyA ("a") kc=65
});

hook.on('keyup', (ev) => {
  console.log(`↑ ${ev.code}`);
});

// When you're done:
// hook.stop();
```

The hook starts automatically the moment the first `keydown` / `keyup` / `key` listener is attached. Call `hook.start()` / `hook.stop()` if you want explicit control.

### Text-expansion sketch

```js
const hook = require('domkeys');

const TRIGGERS = {
  ':ts:': () => new Date().toISOString(),
  ':sig:': () => '— Sent from domkeys',
};

let buf = '';
hook.on('keydown', (ev) => {
  if (ev.key.length === 1) buf = (buf + ev.key).slice(-32);
  else if (ev.code === 'Backspace') buf = buf.slice(0, -1);

  for (const trigger of Object.keys(TRIGGERS)) {
    if (buf.endsWith(trigger)) {
      const replacement = TRIGGERS[trigger]();
      console.log(`expand "${trigger}" -> "${replacement}"`);
      // ... use SendInput / CGEventPost to delete trigger + type replacement
      buf = '';
    }
  }
});
```

> `domkeys` only *observes* keys — to type the replacement you need a separate synthetic-input library or platform API.

---

## API

### `hook.on(event, listener)` / `hook.off(event, listener)`

Standard `EventEmitter` interface. Events:

| Event       | Fires for                              | Listener signature        |
|-------------|----------------------------------------|---------------------------|
| `'keydown'` | Physical key press (incl. auto-repeat) | `(ev: KeyEvent) => void`  |
| `'keyup'`   | Physical key release                   | `(ev: KeyEvent) => void`  |
| `'key'`     | Both keydown and keyup (fired alongside the typed event) | `(ev: KeyEvent) => void`  |

Modifier keys (Shift/Ctrl/Alt/Meta/Caps/Fn) fire `keydown` and `keyup` too — modifier flags on subsequent events reflect their state.

### `hook.start(): this`

Explicitly start the hook. No-op if already running. Returns `this` for chaining. Throws on failure (most commonly: missing macOS Accessibility permission).

### `hook.stop(): this`

Stop the hook and release the OS resources (CFRunLoop on macOS, message loop + `WH_KEYBOARD_LL` handle on Windows). Safe to call multiple times. Returns `this`.

### Direct converters (no hook required)

These work even before / without starting the hook — useful if you have raw OS keycodes from another source.

```js
hook.codeFromMacKeycode(0x00);            // 'KeyA'
hook.codeFromWindowsScanCode(0x001e);     // 'KeyA'
hook.codeFromWindowsScanCode(0xE04B);     // 'ArrowLeft'   (extended)
hook.legacyKeyCodeFromCode('ArrowLeft');  // 37            (Windows VK / DOM legacy keyCode)
```

---

## Event shape

```ts
interface KeyEvent {
  type: 'keydown' | 'keyup';
  code: string;     // DOM `code`     — physical key, layout-independent
  key: string;      // DOM `key`      — modifier-aware character or canonical name
  keyCode: number;  // legacy DOM `keyCode` (Windows VK value)
  which: number;    // alias of keyCode
  location: 0 | 1 | 2 | 3;  // 0=standard, 1=left, 2=right, 3=numpad
  altKey: boolean;
  ctrlKey: boolean;
  shiftKey: boolean;
  metaKey: boolean;   // Cmd on macOS, Win key on Windows
  capsLock: boolean;
  repeat: boolean;    // mac: OS auto-repeat; win: always false (not exposed by LL hook)
  nativeKeyCode: number;   // mac: Carbon VK (0–127). win: VK code.
  nativeScanCode: number;  // win: scan code with 0xE0/0xE1 extended prefix. mac: 0.
}
```

### `code` vs `key`

These distinguish *physical key* from *character produced*:

| Press                | `code`        | `key` |
|----------------------|---------------|-------|
| <kbd>A</kbd>         | `'KeyA'`      | `'a'` |
| <kbd>Shift</kbd>+<kbd>A</kbd> | `'KeyA'` | `'A'` |
| <kbd>A</kbd> on AZERTY (French) | `'KeyA'` | `'q'` (physical key still `KeyA`) |
| <kbd>1</kbd> (top row) | `'Digit1'`  | `'1'` |
| <kbd>1</kbd> (numpad)  | `'Numpad1'` | `'1'` |
| <kbd>←</kbd>          | `'ArrowLeft'` | `'ArrowLeft'` |
| <kbd>Enter</kbd>      | `'Enter'`     | `'Enter'` |
| <kbd>F1</kbd>         | `'F1'`        | `'F1'` |
| <kbd>Shift</kbd> (left) | `'ShiftLeft'` | `'Shift'` |
| <kbd>Shift</kbd> (right) | `'ShiftRight'` | `'Shift'` |

Use `code` for keyboard-shortcut detection (layout-independent), use `key` for "what did the user type."

Full reference: [MDN — `KeyboardEvent.code` values](https://developer.mozilla.org/en-US/docs/Web/API/UI_Events/Keyboard_event_code_values). The table in `src/keycodes/keycode_converter_data.inc` lists every code `domkeys` currently maps (~135 keys, US-centric with IntlYen/IntlRo, media, browser, and lock keys).

---

## Platform setup & permissions

### macOS

The first time you run `domkeys`, macOS will prompt for **two** permissions and **silently drop events** until both are granted:

1. **Accessibility** — System Settings → Privacy & Security → Accessibility
2. **Input Monitoring** — System Settings → Privacy & Security → Input Monitoring

Grant them to the **host process** that runs Node — i.e. your terminal (`Terminal.app`, `iTerm.app`, `Warp`, etc.) when developing, or your Electron app's `.app` bundle in production. After granting, you must restart that process.

For Electron apps you ship: the `.app` bundle must be **code-signed** (and ideally notarized) for permissions to persist across reinstalls.

**Detecting the failure mode:** `hook.start()` throws if `CGEventTapCreate` returns NULL (usually permissions). The thrown message tells the user where to grant access.

### Windows

`WH_KEYBOARD_LL` works for any user-mode process without elevation. Two things to know:

- **UAC-elevated windows are invisible** to a non-elevated hook. If you want to capture keystrokes inside (say) Task Manager, your hook process must also run elevated.
- **Hook timeout**: Windows can silently disable hooks that take too long to process events. `domkeys` keeps its callback fast (~µs) so this rarely triggers, but if you see events stop arriving after high CPU pressure, restart the hook.

---

## Keyboard layouts & IME

| Scenario                        | `ev.code`    | `ev.key`    | Notes |
|---------------------------------|--------------|-------------|-------|
| US layout, plain typing         | physical key | character   | Works.                  |
| Switch US → AZERTY/Cyrillic/Greek/Arabic | physical key | character in new layout | Works on both platforms after the v0.1.0 layout-detection fix. |
| US-International dead key (`'` then `a` to get `á`) | physical key | `'` then `a` (raw) | `key` is the **raw** key, not the composed glyph. The foreground app still composes correctly — `domkeys` is read-only. |
| IME composition (Japanese / Chinese / Korean) | physical key | raw key without composition | The composed glyph is delivered by the OS to the focused window's text-input client, not into the system event stream. **Cannot** be captured from any global hook. |

If your product needs IME-aware text capture, prompt users to disable IME composition for that input or run as an in-app text-input client (NSTextInputClient on macOS, TSF on Windows).

---

## Non-intrusive design

`domkeys` is engineered to **observe without affecting**:

| Concern                                | How it's handled |
|----------------------------------------|------------------|
| Listen vs. consume events              | macOS: `kCGEventTapOptionListenOnly`. Windows: always `CallNextHookEx`. |
| Dead-key state in the foreground app   | Windows: `ToUnicodeEx` is called with the "do not change kernel state" flag (bit 2, Win10 1607+). The foreground app's pending dead key is not consumed. |
| Foreground app's keyboard layout       | Windows: layout taken from `GetKeyboardLayoutList(...)[0]` — Windows moves the just-activated HKL to the head of that list, so taskbar / Win+Space switches are picked up even when the foreground host (Windows Terminal, modern Electron) routes input through TSF and skips `WM_INPUTLANGCHANGE`. Falls back to the foreground-thread HKL and then to the current thread. |
| Modifier state                         | Windows: reconstructed from real-time `GetAsyncKeyState` rather than the hook thread's queue. |
| Focus / message-queue intrusion        | `AttachThreadInput` is **never** called — its side effects on focus and message dispatch make it unsuitable for non-intrusive hooks. |
| Hook performance                       | NAPI `ThreadSafeFunction` with non-blocking enqueue. Hook thread returns to the OS in microseconds. |

---

## Limitations

- **Repeat detection on Windows** — `WH_KEYBOARD_LL` doesn't expose an auto-repeat flag. `ev.repeat` is always `false` on Windows. (Detecting repeat would need per-VK state tracking; not currently implemented.)
- **IME composition** — see [Keyboard layouts & IME](#keyboard-layouts--ime). Structural to all global hooks.
- **Dead-key composition** — see same section. `key` reports raw keys for dead-key layouts.
- **Pause/Break on Windows** — the key sends a 0xE1-prefixed two-event sequence; only the first half is currently mapped.
- **Windows ARM64** — no prebuild ships. Build from source works (Node ARM64 on Windows is supported).
- **Linux** — explicitly out of scope. Different event model entirely (evdev + X11/Wayland).

---

## Build from source

```bash
git clone https://github.com/manobendro/domkeys.git
cd domkeys
npm install        # builds via node-gyp
npm test           # conversion smoke test
npm run test:manual   # live hook — needs macOS permissions
```

Rebuilding after a source change:

```bash
npm run rebuild
```

Producing prebuilds locally (for testing what CI would publish):

```bash
npx prebuildify --napi --strip
ls prebuilds/      # darwin-arm64/domkeys.node (or your host's triple)
```

---

## Project layout

```
binding.gyp                          node-gyp build config
src/
  binding.cc                         NAPI bindings (start / stop / converters)
  hook.h                             shared C++ interface
  hook_mac.mm                        CGEventTap on a worker CFRunLoop
  hook_win.cc                        WH_KEYBOARD_LL on a worker message loop
  keycodes/                          Chromium fork — isolated, mac+win only
    keycode_converter.h
    keycode_converter.cc
    keycode_converter_data.inc       ~135-entry mapping table
lib/
  index.js                           EventEmitter wrapper, auto-starts on first listener
index.d.ts                           TypeScript types
test/
  smoke.js                           Conversion sanity checks (no permissions)
  manual.js                          Interactive live hook output
.github/workflows/
  ci.yml                             Build + test on PR (macOS, Windows × Node 18/20/22)
  prebuild.yml                       Tag-triggered: build prebuilds, publish to npm
```

---

## How it compares

| Library         | Style              | Output format                  | Platforms                  | Send keys |
|-----------------|--------------------|-------------------------------|-----------------------------|-----------|
| **`domkeys`**   | NAPI prebuild      | **DOM `KeyboardEvent` shape** | macOS, Windows              | No        |
| `iohook`        | NAN, unmaintained  | Raw codes                     | mac/win/linux               | No        |
| `node-global-key-listener` | Spawns child binaries | Raw codes              | mac/win/linux               | No        |
| `robotjs`       | Native             | n/a (sender-focused)          | mac/win/linux               | Yes       |
| `nut.js`        | Native             | n/a (automation-focused)      | mac/win/linux               | Yes       |
| `uiohook-napi`  | NAPI               | Raw codes                     | mac/win/linux               | No        |

`domkeys`' differentiation: **DOM-shaped events** out of the box, **non-intrusive design**, modern NAPI + prebuilds (drop-in install in Electron apps).

---

## FAQ

**Q: Can I use this in Electron?**
Yes — that's the primary target. Bundle the package with `electron-builder` / `electron-forge`; the prebuild matching the Electron runtime's NAPI version is picked up automatically. Remember to declare permissions in the `.app` `Info.plist` on macOS (Accessibility / Input Monitoring usage strings).

**Q: Will it work in the renderer process?**
Only if you've enabled `nodeIntegration` (not recommended). Use it from the main process and forward events to the renderer over IPC.

**Q: Does it block keys / steal Cmd-Tab?**
No — `domkeys` is listen-only by design. To suppress or rewrite keystrokes you'd need to switch the macOS event tap to `kCGEventTapOptionDefault` and return modified events, which is a different (and more invasive) library.

**Q: Why is `key` empty for a function key like F13?**
It shouldn't be — `F13` produces `key: "F13"`. If you hit a key that maps to `code: ''` and the event prints with `<-- UNMAPPED` in `test/manual.js`, please open an issue with your OS, layout, and the raw `nativeKeyCode` / `nativeScanCode` so we can extend the table.

**Q: My antivirus flags it as a keylogger.**
The library *is* a keyboard hook — heuristic AV will flag any such tool. For Electron apps that ship `domkeys`, code-signing (and Authenticode + EV cert on Windows, notarization on macOS) is what AVs use to reduce false positives.

**Q: How do I add a missing key?**
Append a `DOM_CODE(...)` line to `src/keycodes/keycode_converter_data.inc`, rebuild, and the new key flows through automatically. PRs welcome.

---

## License

[BSD-3-Clause](./LICENSE).

The `src/keycodes/` directory is a fork of Chromium's `ui/events/keycodes/dom/` and retains its original Chromium copyright. Chromium is also BSD-3-Clause.
