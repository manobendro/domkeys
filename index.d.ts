import { EventEmitter } from 'events';

export interface KeyEvent {
  /** "keydown" | "keyup" */
  type: 'keydown' | 'keyup';
  /** Chromium DOM `code`, e.g. "KeyA", "ArrowLeft", "ShiftLeft". */
  code: string;
  /** Chromium DOM `key`, e.g. "a", "A", "ArrowLeft", "Shift". */
  key: string;
  /** Legacy DOM `keyCode` (Windows VK value). */
  keyCode: number;
  /** Alias of `keyCode`. */
  which: number;
  /** 0=standard, 1=left, 2=right, 3=numpad. */
  location: 0 | 1 | 2 | 3;
  altKey: boolean;
  ctrlKey: boolean;
  shiftKey: boolean;
  metaKey: boolean;
  capsLock: boolean;
  /** True for OS-generated auto-repeat events (macOS only — Windows reports false). */
  repeat: boolean;
  /**
   * True if the event was synthesized rather than produced by physical
   * hardware. Windows: the `LLKHF_INJECTED` flag — SendInput, RDP / VM guest
   * tools, and key remappers (PowerToys, AutoHotkey) set it. A held modifier
   * arriving as a stream of down/up pairs is usually injected, so callers can
   * suppress it with `if (ev.injected) return`. Always `false` on macOS.
   */
  injected: boolean;
  /** Raw OS keycode (macOS Carbon VK, or Windows VK on Windows). */
  nativeKeyCode: number;
  /** Windows scan code (with extended-bit prefix); 0 on macOS. */
  nativeScanCode: number;
}

export interface Hook extends EventEmitter {
  start(): this;
  stop(): this;

  on(event: 'keydown', listener: (ev: KeyEvent) => void): this;
  on(event: 'keyup', listener: (ev: KeyEvent) => void): this;
  on(event: 'key', listener: (ev: KeyEvent) => void): this;
  on(event: string, listener: (...args: any[]) => void): this;

  /** Chromium DOM `code` for a macOS Carbon virtual key. */
  codeFromMacKeycode(macVk: number): string;
  /** Chromium DOM `code` for a Windows scan code (with 0xE0/0xE1 prefix). */
  codeFromWindowsScanCode(scanCode: number): string;
  /** Legacy DOM `keyCode` (Windows VK) for a DOM `code`. */
  legacyKeyCodeFromCode(code: string): number;
}

declare const hook: Hook;
export = hook;
