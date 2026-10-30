// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <fontconfig/fontconfig.h>
#include <stdexcept>
namespace dome::test {
// These standalone fixtures own Fontconfig's process-wide initialization.
// Construct after the private environment, before every font consumer; destroy
// after all monitor threads, Documents, GL text and FreeType objects finish.
class FontconfigRuntime {
public:
  FontconfigRuntime() {
    if (!FcInit())
      throw std::runtime_error("Cannot initialize fixture Fontconfig runtime");
  }
  ~FontconfigRuntime() { FcFini(); }
  FontconfigRuntime(const FontconfigRuntime &) = delete;
  FontconfigRuntime &operator=(const FontconfigRuntime &) = delete;
};
} // namespace dome::test
