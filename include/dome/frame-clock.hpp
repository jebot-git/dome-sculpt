// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <chrono>
namespace dome {
// Shared process epoch for desktop and XR. Sample once per stereo frame so
// material animation cannot acquire a different phase between the two eyes.
inline double render_frame_time() {
  using Clock = std::chrono::steady_clock;
  static const auto start = Clock::now();
  return std::chrono::duration<double>(Clock::now() - start).count();
}
} // namespace dome
