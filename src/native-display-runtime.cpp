// SPDX-License-Identifier: GPL-3.0-or-later
#include "native-display-runtime.hpp"
// Keep backend headers and Xlib's generic macros out of editor translation units.
#include <FL/platform.H>

namespace dome::desktop {
NativeDisplayRuntime::~NativeDisplayRuntime() { fl_close_display(); }
} // namespace dome::desktop
