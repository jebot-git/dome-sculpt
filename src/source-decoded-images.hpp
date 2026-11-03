// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/import-image.hpp"
namespace dome::detail {
using DecodedSourceImages=std::vector<std::shared_ptr<const Image>>;
// Worker-local immutable leaf sharing. Every raw descriptor remains represented;
// the pixel budget counts only distinct allocated decoded owners.
std::shared_ptr<const DecodedSourceImages> decode_source_images(
    std::span<const ImportedImage>,size_t maximum_pixels=64*1024*1024,
    const std::function<bool()> &cancelled={});
} // namespace dome::detail
