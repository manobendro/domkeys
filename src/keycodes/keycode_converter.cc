// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Forked from chromium/src/ui/events/keycodes/dom/keycode_converter.cc

#include "keycode_converter.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>

namespace domkeys {

namespace {

struct KeyEntry {
  uint32_t win_scan;
  uint32_t mac_vk;
  uint32_t win_vk;
  const char* code;
  const char* dom_key;
  int location;
};

constexpr KeyEntry kEntries[] = {
#define DOM_CODE(win, mac, vk, code, key, loc) { win, mac, vk, code, key, loc },
#include "keycode_converter_data.inc"
#undef DOM_CODE
};
constexpr size_t kEntriesCount = sizeof(kEntries) / sizeof(kEntries[0]);

const std::unordered_map<uint32_t, const KeyEntry*>& MacIndex() {
  static const auto* map = [] {
    auto* m = new std::unordered_map<uint32_t, const KeyEntry*>();
    m->reserve(kEntriesCount);
    for (const auto& e : kEntries) {
      if (e.mac_vk != 0xFFFF) m->emplace(e.mac_vk, &e);
    }
    return m;
  }();
  return *map;
}

const std::unordered_map<uint32_t, const KeyEntry*>& WinScanIndex() {
  static const auto* map = [] {
    auto* m = new std::unordered_map<uint32_t, const KeyEntry*>();
    m->reserve(kEntriesCount);
    for (const auto& e : kEntries) {
      if (e.win_scan != 0) m->emplace(e.win_scan, &e);
    }
    return m;
  }();
  return *map;
}

const std::unordered_map<std::string, const KeyEntry*>& CodeIndex() {
  static const auto* map = [] {
    auto* m = new std::unordered_map<std::string, const KeyEntry*>();
    m->reserve(kEntriesCount);
    for (const auto& e : kEntries) {
      m->emplace(e.code, &e);
    }
    return m;
  }();
  return *map;
}

}  // namespace

std::string KeycodeConverter::CodeFromWindowsScanCode(uint32_t scancode) {
  const auto& idx = WinScanIndex();
  auto it = idx.find(scancode);
  if (it != idx.end()) return it->second->code;
  return std::string();
}

std::string KeycodeConverter::CodeFromMacKeycode(uint32_t mac_vk) {
  const auto& idx = MacIndex();
  auto it = idx.find(mac_vk);
  if (it != idx.end()) return it->second->code;
  return std::string();
}

int KeycodeConverter::LegacyKeyCodeFromCode(const std::string& code) {
  if (code.empty()) return 0;
  const auto& idx = CodeIndex();
  auto it = idx.find(code);
  if (it != idx.end()) return static_cast<int>(it->second->win_vk);
  return 0;
}

std::string KeycodeConverter::NonPrintableKeyFromCode(const std::string& code) {
  if (code.empty()) return std::string();
  const auto& idx = CodeIndex();
  auto it = idx.find(code);
  if (it != idx.end()) return it->second->dom_key;
  return std::string();
}

int KeycodeConverter::LocationFromCode(const std::string& code) {
  if (code.empty()) return 0;
  const auto& idx = CodeIndex();
  auto it = idx.find(code);
  if (it != idx.end()) return it->second->location;
  return 0;
}

}  // namespace domkeys
