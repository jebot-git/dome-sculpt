// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace dome::desktop {
// Process-owned desktop connection. Construct after the Fontconfig owner and
// before canonical appearance initialization/App, so every widget, GL context,
// worker and menu callback is gone before the final toolkit display operation.
// FLTK explicitly forbids further toolkit calls after fl_close_display().
class NativeDisplayRuntime {
public:
  NativeDisplayRuntime() = default;
  ~NativeDisplayRuntime();
  NativeDisplayRuntime(const NativeDisplayRuntime &) = delete;
  NativeDisplayRuntime &operator=(const NativeDisplayRuntime &) = delete;
};
} // namespace dome::desktop
