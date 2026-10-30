// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "import.hpp"
#include "paint.hpp"
namespace dome {
// glTF core PNG/JPEG, native RGBA storage. Color interpretation stays with the
// material role (sRGB color/emission versus linear data/normal), not the decoder.
std::array<uint32_t,2> imported_image_dimensions(const ImportedImage &,size_t remaining_pixels=64*1024*1024);
Image decode_import_image(const ImportedImage &,size_t remaining_pixels=64*1024*1024,
                          const std::function<bool()> &cancelled = {});
} // namespace dome
