'use strict';

// Native binding entry point.
//
// This intentionally lives at the package root and passes a *literal* __dirname
// to node-gyp-build. Static bundlers used by Electron tooling — most notably
// @vercel/webpack-asset-relocator-loader (electron-forge / Vercel ncc) — only
// recognise the exact form `require('node-gyp-build')(__dirname)`. They cannot
// evaluate a computed argument such as `path.resolve(__dirname, '..')`, so when
// the load happens from a sub-folder with a computed path the prebuilt .node is
// never copied into the bundle and the packaged app throws
// "No native build was found … webpack=true" at runtime.
//
// Keeping the load at the root with a literal __dirname mirrors how uiohook-napi
// and node-window-manager expose their bindings, and makes domkeys bundle
// cleanly inside packaged Electron apps.
module.exports = require('node-gyp-build')(__dirname);
