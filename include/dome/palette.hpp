// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "render.hpp"
#include <memory>
namespace dome {
namespace palette_text { class Font; }
class SpatialPalette {
  std::unique_ptr<palette_text::Font> text_font_;
  std::string font_, signature_;
  int pixels_ = 16, unit_ = 1, width_ = 640, height_ = 480;
  std::vector<uint8_t> rgba_;
  GLuint program_ = 0, vao_ = 0, vbo_ = 0, texture_ = 0;
  bool linear_output_ = false;
  void font(const EditorState &);
  void raster(const EditorState &);

public:
  SpatialPalette();
  ~SpatialPalette();
  void set_linear_output(bool enabled) { linear_output_ = enabled; }
  void draw(const EditorState &, const Mat4 &);
};
} // namespace dome
