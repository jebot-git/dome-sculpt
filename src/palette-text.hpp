// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
namespace dome::palette_text {
struct Glyph {
  int width = 0, height = 0, left = 0, top = 0;
  std::vector<uint8_t> alpha;
};
struct PlacedGlyph {
  std::shared_ptr<const Glyph> bitmap;
  int x = 0, y = 0;
};
struct Line {
  std::vector<PlacedGlyph> glyphs;
  std::string text; // Valid UTF-8, clipped at extended grapheme boundaries.
  int advance = 0;
};
// Private palette renderer: selected BDF first, Fontconfig monospace fallback.
// Independent of GL, making layout and fallback testable without a GPU.
class Font {
  struct Impl;
  std::unique_ptr<Impl> impl_;
public:
  Font(const std::string &bdf, int pixels);
  ~Font();
  Font(const Font &) = delete;
  Font &operator=(const Font &) = delete;
  Line layout(std::string_view utf8, int maximum_advance);
  size_t cached_glyphs() const;
  size_t cached_bytes() const;
  size_t loaded_faces() const;
  std::string source_manifest() const;
};
} // namespace dome::palette_text
