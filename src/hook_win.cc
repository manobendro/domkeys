#include <windows.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include "hook.h"
#include "keycodes/keycode_converter.h"

namespace domkeys {
namespace {

std::atomic<bool> g_running{false};
std::thread g_thread;
DWORD g_thread_id = 0;
HHOOK g_hook = nullptr;
EventCallback g_callback;
std::mutex g_callback_mu;

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode != HC_ACTION) {
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
  }

  const KBDLLHOOKSTRUCT* kb = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);

  KeyEvent ev;
  bool extended = (kb->flags & LLKHF_EXTENDED) != 0;
  uint32_t scan = kb->scanCode & 0xFF;
  uint32_t scan_full = extended ? (0xE000u | scan) : scan;

  ev.native_keycode = kb->vkCode;
  ev.native_scancode = scan_full;

  ev.code = KeycodeConverter::CodeFromWindowsScanCode(scan_full);
  if (ev.code.empty() && extended) {
    // Fallback for the non-extended variant.
    ev.code = KeycodeConverter::CodeFromWindowsScanCode(scan);
  }
  ev.key_code = KeycodeConverter::LegacyKeyCodeFromCode(ev.code);
  if (ev.key_code == 0) ev.key_code = static_cast<int>(kb->vkCode);
  ev.location = KeycodeConverter::LocationFromCode(ev.code);

  ev.shift     = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;
  ev.ctrl      = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
  ev.alt       = (GetAsyncKeyState(VK_MENU)    & 0x8000) != 0;
  ev.meta      = ((GetAsyncKeyState(VK_LWIN)   & 0x8000) != 0) ||
                 ((GetAsyncKeyState(VK_RWIN)   & 0x8000) != 0);
  ev.caps_lock = (GetKeyState(VK_CAPITAL) & 0x1) != 0;

  if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
    ev.is_down = true;
  } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
    ev.is_down = false;
  } else {
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
  }

  std::string non_text = KeycodeConverter::NonPrintableKeyFromCode(ev.code);
  if (!non_text.empty()) {
    ev.key = non_text;
  } else if (ev.is_down) {
    // Layout + state must come from the *foreground* app, not our hook
    // thread — otherwise layout switches in the user's app are invisible.
    HKL layout = nullptr;
    HWND fg = GetForegroundWindow();
    if (fg) {
      DWORD fg_tid = GetWindowThreadProcessId(fg, nullptr);
      if (fg_tid) layout = GetKeyboardLayout(fg_tid);
    }
    if (!layout) layout = GetKeyboardLayout(0);

    // Reconstruct keyboard state from real-time physical keys instead of
    // reading our thread's stale queue (GetKeyboardState).
    BYTE keystate[256] = {0};
    for (int vk = 0; vk < 256; ++vk) {
      if (GetAsyncKeyState(vk) & 0x8000) keystate[vk] = 0x80;
    }
    // Toggle bits for lock keys are system-wide, safe to query directly.
    if (GetKeyState(VK_CAPITAL) & 0x0001) keystate[VK_CAPITAL] |= 0x01;
    if (GetKeyState(VK_NUMLOCK) & 0x0001) keystate[VK_NUMLOCK] |= 0x01;
    if (GetKeyState(VK_SCROLL)  & 0x0001) keystate[VK_SCROLL]  |= 0x01;

    // Bit 2 in wFlags (Win10 1607+) = "do not change kernel keyboard state".
    // Critical: without this we'd consume the foreground app's pending dead
    // key on layouts like US-International, breaking composition there.
    wchar_t buf[8] = {0};
    int n = ToUnicodeEx(kb->vkCode, kb->scanCode, keystate, buf,
                        sizeof(buf) / sizeof(buf[0]), 1 << 2, layout);
    if (n > 0) {
      char utf8[32] = {0};
      int u8 = WideCharToMultiByte(CP_UTF8, 0, buf, n, utf8,
                                   sizeof(utf8) - 1, nullptr, nullptr);
      if (u8 > 0) {
        utf8[u8] = '\0';
        ev.key = utf8;
      }
    }
  }

  EventCallback cb;
  {
    std::lock_guard<std::mutex> lk(g_callback_mu);
    cb = g_callback;
  }
  if (cb) cb(ev);

  return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void RunHookThread() {
  g_thread_id = GetCurrentThreadId();
  g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc,
                             GetModuleHandleW(nullptr), 0);
  if (!g_hook) {
    g_running.store(false);
    return;
  }
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  if (g_hook) {
    UnhookWindowsHookEx(g_hook);
    g_hook = nullptr;
  }
}

}  // namespace

bool StartHook(EventCallback cb) {
  if (g_running.exchange(true)) return false;
  {
    std::lock_guard<std::mutex> lk(g_callback_mu);
    g_callback = std::move(cb);
  }
  g_thread = std::thread(RunHookThread);
  return true;
}

void StopHook() {
  if (!g_running.exchange(false)) return;
  if (g_thread_id) {
    PostThreadMessageW(g_thread_id, WM_QUIT, 0, 0);
  }
  if (g_thread.joinable()) g_thread.join();
  g_thread_id = 0;
  std::lock_guard<std::mutex> lk(g_callback_mu);
  g_callback = nullptr;
}

}  // namespace domkeys
