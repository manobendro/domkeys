#ifndef DOMKEYS_HOOK_H_
#define DOMKEYS_HOOK_H_

#include <functional>
#include "keycodes/keycode_converter.h"

namespace domkeys {

using EventCallback = std::function<void(const KeyEvent&)>;

// Start the platform key hook. Returns true on success. The callback fires on
// an internal hook thread — the NAPI layer is responsible for thread-safe
// dispatch back to Node.
bool StartHook(EventCallback cb);

// Stop the platform key hook. Safe to call when not started.
void StopHook();

}  // namespace domkeys

#endif  // DOMKEYS_HOOK_H_
