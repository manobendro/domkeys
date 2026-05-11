// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Forked from chromium/src/ui/events/keycodes/dom/keycode_converter.h
// Trimmed to macOS + Windows only.

#ifndef DOMKEYS_KEYCODE_CONVERTER_H_
#define DOMKEYS_KEYCODE_CONVERTER_H_

#include <cstdint>
#include <string>

namespace domkeys {

// Subset of Chromium's DomCode enum. The integer value is the USB HID code
// in the form (usage_page << 16) | usage, matching Chromium.
enum class DomCode : uint32_t {
  NONE = 0,
};

// Cross-platform key event after normalization. Fields are intentionally
// shaped to match the web `KeyboardEvent` interface that Chromium produces.
struct KeyEvent {
  bool is_down = false;
  bool repeat = false;

  // Raw OS-level inputs (kept for debugging / user inspection).
  uint32_t native_keycode = 0;   // macOS Carbon VK, or Windows VK
  uint32_t native_scancode = 0;  // Windows scan code (with ext bit), 0 on mac

  // Forked-Chromium mapped fields.
  std::string code;   // DOM `code` string, e.g. "KeyA", "ArrowLeft".
  std::string key;    // DOM `key` string, e.g. "a", "A", "ArrowLeft".
  int key_code = 0;   // Legacy DOM `keyCode` (Windows VK).
  int location = 0;   // 0=standard, 1=left, 2=right, 3=numpad.

  bool alt = false;
  bool ctrl = false;
  bool shift = false;
  bool meta = false;
  bool caps_lock = false;
};

class KeycodeConverter {
 public:
  // Returns the DOM `code` string for a Windows scan code, or "" if unknown.
  // The scan code is the value Windows reports (low byte is set-1 code, the
  // 0xE000 / 0xE100 prefix encodes the extended bit).
  static std::string CodeFromWindowsScanCode(uint32_t scancode);

  // Returns the DOM `code` string for a macOS Carbon virtual key code (0-127).
  static std::string CodeFromMacKeycode(uint32_t mac_vk);

  // Returns the legacy DOM `keyCode` (Windows VK value) for a DOM `code`.
  // Returns 0 if the code is unknown.
  static int LegacyKeyCodeFromCode(const std::string& code);

  // For non-character keys (arrows, F-keys, modifiers, etc.) returns the
  // canonical DOM `key` string ("ArrowLeft", "Enter", ...). Returns "" for
  // keys that should derive their `key` from generated text.
  static std::string NonPrintableKeyFromCode(const std::string& code);

  // Returns 1 (Left), 2 (Right), 3 (Numpad), or 0 (Standard).
  static int LocationFromCode(const std::string& code);
};

}  // namespace domkeys

#endif  // DOMKEYS_KEYCODE_CONVERTER_H_
