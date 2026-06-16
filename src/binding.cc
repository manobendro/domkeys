#include <napi.h>

#include <functional>
#include <memory>

#include "hook.h"
#include "keycodes/keycode_converter.h"

namespace {

// One module-global tsfn; only one hook can be live at a time.
Napi::ThreadSafeFunction g_tsfn;
bool g_started = false;

void Dispatch(Napi::Env env, Napi::Function jsCallback,
              domkeys::KeyEvent* data) {
  std::unique_ptr<domkeys::KeyEvent> ev(data);
  Napi::Object obj = Napi::Object::New(env);
  obj.Set("type", ev->is_down ? "keydown" : "keyup");
  obj.Set("code", ev->code);
  obj.Set("key", ev->key);
  obj.Set("keyCode", ev->key_code);
  obj.Set("which", ev->key_code);
  obj.Set("location", ev->location);
  obj.Set("altKey", ev->alt);
  obj.Set("ctrlKey", ev->ctrl);
  obj.Set("shiftKey", ev->shift);
  obj.Set("metaKey", ev->meta);
  obj.Set("capsLock", ev->caps_lock);
  obj.Set("repeat", ev->repeat);
  obj.Set("injected", ev->injected);
  obj.Set("nativeKeyCode", static_cast<double>(ev->native_keycode));
  obj.Set("nativeScanCode", static_cast<double>(ev->native_scancode));
  jsCallback.Call({obj});
}

Napi::Value Start(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsFunction()) {
    Napi::TypeError::New(env, "domkeys: start(callback) requires a function")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  if (g_started) {
    Napi::Error::New(env, "domkeys: hook already started")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  g_tsfn = Napi::ThreadSafeFunction::New(
      env, info[0].As<Napi::Function>(), "domkeys",
      /*maxQueueSize=*/0, /*initialThreadCount=*/1);

  bool ok = domkeys::StartHook(
      [](const domkeys::KeyEvent& ev) {
        auto* heap = new domkeys::KeyEvent(ev);
        if (g_tsfn.NonBlockingCall(heap, Dispatch) != napi_ok) {
          delete heap;
        }
      });

  if (!ok) {
    g_tsfn.Release();
    Napi::Error::New(env,
                     "domkeys: failed to start hook. "
                     "On macOS, grant Accessibility / Input Monitoring "
                     "permission to the host process.")
        .ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }
  g_started = true;
  return Napi::Boolean::New(env, true);
}

Napi::Value Stop(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!g_started) return env.Undefined();
  domkeys::StopHook();
  g_tsfn.Release();
  g_started = false;
  return env.Undefined();
}

// Direct converters — exposed so callers can map raw keycodes without a hook.
Napi::Value CodeFromMac(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsNumber()) return env.Null();
  uint32_t vk = info[0].As<Napi::Number>().Uint32Value();
  return Napi::String::New(env,
                           domkeys::KeycodeConverter::CodeFromMacKeycode(vk));
}

Napi::Value CodeFromWinScan(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsNumber()) return env.Null();
  uint32_t s = info[0].As<Napi::Number>().Uint32Value();
  return Napi::String::New(
      env, domkeys::KeycodeConverter::CodeFromWindowsScanCode(s));
}

Napi::Value LegacyKeyCode(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsString()) return Napi::Number::New(env, 0);
  std::string code = info[0].As<Napi::String>().Utf8Value();
  return Napi::Number::New(
      env, domkeys::KeycodeConverter::LegacyKeyCodeFromCode(code));
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("start", Napi::Function::New(env, Start));
  exports.Set("stop", Napi::Function::New(env, Stop));
  exports.Set("codeFromMacKeycode", Napi::Function::New(env, CodeFromMac));
  exports.Set("codeFromWindowsScanCode", Napi::Function::New(env, CodeFromWinScan));
  exports.Set("legacyKeyCodeFromCode", Napi::Function::New(env, LegacyKeyCode));
  return exports;
}

}  // namespace

NODE_API_MODULE(domkeys, Init)
