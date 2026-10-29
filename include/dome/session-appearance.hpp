// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "document.hpp"
namespace dome {
struct SessionAppearance {
  std::string file;
  int pixels = 16;
  bool light = false;
  uint32_t accent = 0x6babff;
  int opacity = 255;
};
int chrome_opacity(int);
SessionAppearance session_appearance();
// Used by standalone XR, whose native frame loop does not run FLTK timers.
class SessionAppearanceMonitor {
  std::jthread worker_;

public:
  explicit SessionAppearanceMonitor(Document &);
};
} // namespace dome
