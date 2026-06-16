#import <Cocoa/Cocoa.h>
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

#include "hook.h"
#include "keycodes/keycode_converter.h"

namespace domkeys {
namespace {

// Hook state is module-local; a single hook can be active at a time.
std::atomic<bool> g_running{false};
std::thread g_thread;
CFRunLoopRef g_runloop = nullptr;
CFMachPortRef g_tap = nullptr;
CFRunLoopSourceRef g_source = nullptr;
EventCallback g_callback;
std::mutex g_callback_mu;

// g_runloop is assigned on the worker thread but read by StopHook on the
// caller thread. Without synchronization a stop() that races a just-started
// hook can observe g_runloop == nullptr, skip CFRunLoopStop, and then block
// forever in g_thread.join() (the run loop is left running). Gate the read on
// the worker signalling that the run loop is live.
std::mutex g_runloop_mu;
std::condition_variable g_runloop_cv;
bool g_runloop_ready = false;

// Per-side device modifier bits used to distinguish ShiftLeft vs ShiftRight.
// Values come from IOKit's NX_DEVICE*KEYMASK constants — Carbon does not give
// us "is this down" directly in kCGEventFlagsChanged, so we read these bits.
struct ModifierBit {
  uint32_t mac_vk;
  uint64_t device_bit;
  uint64_t group_bit;
};

constexpr ModifierBit kMods[] = {
    {0x38, 0x00000002, kCGEventFlagMaskShift},        // ShiftLeft
    {0x3C, 0x00000004, kCGEventFlagMaskShift},        // ShiftRight
    {0x3B, 0x00000001, kCGEventFlagMaskControl},      // ControlLeft
    {0x3E, 0x00002000, kCGEventFlagMaskControl},      // ControlRight
    {0x3A, 0x00000020, kCGEventFlagMaskAlternate},    // AltLeft  (Option)
    {0x3D, 0x00000040, kCGEventFlagMaskAlternate},    // AltRight
    {0x37, 0x00000008, kCGEventFlagMaskCommand},      // MetaLeft (Cmd)
    {0x36, 0x00000010, kCGEventFlagMaskCommand},      // MetaRight
    {0x39, 0,          kCGEventFlagMaskAlphaShift},   // CapsLock
    {0x3F, 0,          kCGEventFlagMaskSecondaryFn},  // Fn
};

bool IsModifierDown(uint32_t mac_vk, CGEventFlags flags) {
  for (const auto& m : kMods) {
    if (m.mac_vk != mac_vk) continue;
    if (m.device_bit) return (flags & m.device_bit) != 0;
    return (flags & m.group_bit) != 0;
  }
  return false;
}

CGEventRef HookCallback(CGEventTapProxy /*proxy*/, CGEventType type,
                        CGEventRef event, void* /*refcon*/) {
  // The OS will disable the tap if it times out — re-enable and keep going.
  if (type == kCGEventTapDisabledByTimeout ||
      type == kCGEventTapDisabledByUserInput) {
    if (g_tap) CGEventTapEnable(g_tap, true);
    return event;
  }
  if (type != kCGEventKeyDown && type != kCGEventKeyUp &&
      type != kCGEventFlagsChanged) {
    return event;
  }

  KeyEvent ev;
  CGEventFlags flags = CGEventGetFlags(event);
  ev.shift = (flags & kCGEventFlagMaskShift) != 0;
  ev.ctrl = (flags & kCGEventFlagMaskControl) != 0;
  ev.alt = (flags & kCGEventFlagMaskAlternate) != 0;
  ev.meta = (flags & kCGEventFlagMaskCommand) != 0;
  ev.caps_lock = (flags & kCGEventFlagMaskAlphaShift) != 0;

  uint32_t mac_vk = static_cast<uint32_t>(
      CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
  ev.native_keycode = mac_vk;
  ev.code = KeycodeConverter::CodeFromMacKeycode(mac_vk);
  ev.key_code = KeycodeConverter::LegacyKeyCodeFromCode(ev.code);
  ev.location = KeycodeConverter::LocationFromCode(ev.code);

  if (type == kCGEventKeyDown) {
    ev.is_down = true;
    ev.repeat = CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0;
  } else if (type == kCGEventKeyUp) {
    ev.is_down = false;
  } else {
    ev.is_down = IsModifierDown(mac_vk, flags);
  }

  // DOM `key`: canonical name for non-text keys, otherwise OS-produced unicode.
  std::string non_text = KeycodeConverter::NonPrintableKeyFromCode(ev.code);
  if (!non_text.empty()) {
    ev.key = non_text;
  } else if (type != kCGEventFlagsChanged) {
    UniChar buf[8] = {0};
    UniCharCount actual = 0;
    CGEventKeyboardGetUnicodeString(event, 8, &actual, buf);
    if (actual > 0) {
      CFStringRef str = CFStringCreateWithCharacters(nullptr, buf, actual);
      if (str) {
        char utf8[32] = {0};
        if (CFStringGetCString(str, utf8, sizeof(utf8), kCFStringEncodingUTF8)) {
          ev.key = utf8;
        }
        CFRelease(str);
      }
    }
  }

  EventCallback cb;
  {
    std::lock_guard<std::mutex> lk(g_callback_mu);
    cb = g_callback;
  }
  if (cb) cb(ev);
  return event;
}

void RunHookThread() {
  @autoreleasepool {
    g_runloop = CFRunLoopGetCurrent();
    g_source = CFMachPortCreateRunLoopSource(nullptr, g_tap, 0);
    CFRunLoopAddSource(g_runloop, g_source, kCFRunLoopCommonModes);
    CGEventTapEnable(g_tap, true);

    // Publish the live run loop so a concurrent StopHook can stop it safely.
    {
      std::lock_guard<std::mutex> lk(g_runloop_mu);
      g_runloop_ready = true;
    }
    g_runloop_cv.notify_all();

    CFRunLoopRun();

    CGEventTapEnable(g_tap, false);
    CFRunLoopRemoveSource(g_runloop, g_source, kCFRunLoopCommonModes);
    CFRelease(g_source);
    CFRelease(g_tap);
    g_source = nullptr;
    g_tap = nullptr;
    {
      std::lock_guard<std::mutex> lk(g_runloop_mu);
      g_runloop = nullptr;
      g_runloop_ready = false;
    }
  }
}

}  // namespace

bool StartHook(EventCallback cb) {
  if (g_running.exchange(true)) return false;
  {
    std::lock_guard<std::mutex> lk(g_callback_mu);
    g_callback = std::move(cb);
  }

  CGEventMask mask = CGEventMaskBit(kCGEventKeyDown) |
                     CGEventMaskBit(kCGEventKeyUp) |
                     CGEventMaskBit(kCGEventFlagsChanged);
  g_tap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap,
                           kCGEventTapOptionListenOnly, mask, HookCallback,
                           nullptr);
  if (!g_tap) {
    // Most common cause: missing Accessibility / Input Monitoring permission.
    std::lock_guard<std::mutex> lk(g_callback_mu);
    g_callback = nullptr;
    g_running.store(false);
    return false;
  }

  {
    std::lock_guard<std::mutex> lk(g_runloop_mu);
    g_runloop_ready = false;
  }
  g_thread = std::thread(RunHookThread);
  return true;
}

void StopHook() {
  if (!g_running.exchange(false)) return;
  // Wait until the worker has published a live run loop, then stop it. The
  // tap was created successfully in StartHook, so the worker is guaranteed to
  // reach the ready signal — this can't deadlock.
  CFRunLoopRef rl = nullptr;
  {
    std::unique_lock<std::mutex> lk(g_runloop_mu);
    g_runloop_cv.wait(lk, [] { return g_runloop_ready; });
    rl = g_runloop;
  }
  if (rl) CFRunLoopStop(rl);
  if (g_thread.joinable()) g_thread.join();
  std::lock_guard<std::mutex> lk(g_callback_mu);
  g_callback = nullptr;
}

}  // namespace domkeys
