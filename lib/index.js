'use strict';

const { EventEmitter } = require('events');

let native;
try {
  // Loaded via the root-level native.js so static bundlers (e.g. the asset
  // relocator used by electron-forge) can resolve the prebuilt binary. See the
  // comment in ../native.js for why the load must use a literal __dirname.
  native = require('../native.js');
} catch (err) {
  const e = new Error(
    `domkeys: native binding failed to load. ` +
    `Run 'npm rebuild' or ensure prebuilds are present. ${err.message}`
  );
  e.cause = err;
  throw e;
}

class Hook extends EventEmitter {
  constructor() {
    super();
    this._started = false;
    this._onEvent = (ev) => {
      this.emit(ev.type, ev);
      this.emit('key', ev);
    };
  }

  start() {
    if (this._started) return this;
    native.start(this._onEvent);
    this._started = true;
    return this;
  }

  stop() {
    if (!this._started) return this;
    native.stop();
    this._started = false;
    return this;
  }

  // Auto-start when the first keydown/keyup listener is attached. Mirrors
  // the iohook-style ergonomics the project is patterned after.
  on(event, listener) {
    super.on(event, listener);
    if ((event === 'keydown' || event === 'keyup' || event === 'key') && !this._started) {
      this.start();
    }
    return this;
  }
}

const hook = new Hook();

// Direct conversion helpers (no hook required).
hook.codeFromMacKeycode = native.codeFromMacKeycode;
hook.codeFromWindowsScanCode = native.codeFromWindowsScanCode;
hook.legacyKeyCodeFromCode = native.legacyKeyCodeFromCode;

module.exports = hook;
