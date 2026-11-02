// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <fontconfig/fontconfig.h>
#include <stdexcept>
namespace dome::desktop {
// One owner per native application process, constructed before canonical
// appearance initialization (whose discovery intentionally reads the inherited
// config before exporting/reinitializing its private bitmap configuration).
// Declare before App or standalone Document/appearance monitor so their GL,
// widgets and worker threads finish before Fontconfig's process cleanup.
class NativeFontconfigRuntime {
public:
  NativeFontconfigRuntime() {
    if (!FcInit()) {
      FcFini();
      throw std::runtime_error("Cannot initialize native Fontconfig runtime");
    }
  }
  ~NativeFontconfigRuntime() { FcFini(); }
  NativeFontconfigRuntime(const NativeFontconfigRuntime &) = delete;
  NativeFontconfigRuntime &operator=(const NativeFontconfigRuntime &) = delete;
};
} // namespace dome::desktop
