// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <functional>
#include <string>
namespace dome {
struct PaintBlurOptions {
  float strength=.5f;
  size_t maximum_working_bytes=256*1024*1024;
  // Explicit validation mode. Native application commands always require a
  // hardware EGLDevice/DRM owner; there is no automatic software fallback.
  bool allow_software_validation=false;
};
struct PaintBlurReport {
  bool changed=false;
  size_t tiles=0,changed_tiles=0,uploaded_bytes=0,downloaded_bytes=0;
  std::string device;
};
}
