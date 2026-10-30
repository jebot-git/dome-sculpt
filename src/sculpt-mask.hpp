// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <functional>
#include <stdexcept>
namespace dome::sculpt_detail {
inline void checkpoint(const std::function<bool()> &cancelled) {
  if (cancelled && cancelled()) throw std::runtime_error("Sculpt operation cancelled");
}
}
