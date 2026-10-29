// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from OpenBrush Native's BDF palette and texture renderer
// (Apache-2.0).
#include "dome/palette.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_FONT_FORMATS_H
namespace dome {
namespace {
GLuint shader(GLenum kind, const char *text) {
  GLuint s = glCreateShader(kind);
  glShaderSource(s, 1, &text, nullptr);
  glCompileShader(s);
  GLint ok;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    glDeleteShader(s);
    throw std::runtime_error("Spatial menu shader failed");
  }
  return s;
}
} // namespace
SpatialPalette::SpatialPalette() {
  const char *vertex = R"(#version 330 core
layout(location=0)in vec2 position;layout(location=1)in vec2 uv;uniform mat4 matrix;out vec2 texcoord;void main(){texcoord=uv;gl_Position=matrix*vec4(position,0,1);})";
  const char *fragment = R"(#version 330 core
in vec2 texcoord;uniform sampler2D image;out vec4 color;void main(){color=texture(image,texcoord);})";
  auto v = shader(GL_VERTEX_SHADER, vertex),
       f = shader(GL_FRAGMENT_SHADER, fragment);
  program_ = glCreateProgram();
  glAttachShader(program_, v);
  glAttachShader(program_, f);
  glLinkProgram(program_);
  glDeleteShader(v);
  glDeleteShader(f);
  GLint good = 0;
  glGetProgramiv(program_, GL_LINK_STATUS, &good);
  if (!good)
    throw std::runtime_error("Spatial menu program failed");
  const float vertices[] = {-.3f, -.225f, 0, 1, .3f,  -.225f, 1, 1,
                            -.3f, .225f,  0, 0, -.3f, .225f,  0, 0,
                            .3f,  -.225f, 1, 1, .3f,  .225f,  1, 0};
  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        reinterpret_cast<void *>(2 * sizeof(float)));
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glBindVertexArray(0);
  glGenTextures(1, &texture_);
}
SpatialPalette::~SpatialPalette() {
  glDeleteTextures(1, &texture_);
  glDeleteBuffers(1, &vbo_);
  glDeleteVertexArrays(1, &vao_);
  glDeleteProgram(program_);
}
void SpatialPalette::font(const EditorState &s) {
  if (s.font_file == font_ && s.font_pixels == pixels_ && !glyphs_.empty())
    return;
  FT_Library library{};
  FT_Face face{};
  if (FT_Init_FreeType(&library))
    throw std::runtime_error("Cannot initialize menu font");
  if (FT_New_Face(library, s.font_file.c_str(), 0, &face)) {
    FT_Done_FreeType(library);
    throw std::runtime_error("Spatial menus need the selected DomeD BDF font");
  }
  try {
    auto format = FT_Get_Font_Format(face);
    if (!format || std::string(format) != "BDF" || face->num_fixed_sizes < 1)
      throw std::runtime_error("Spatial menus support BDF bitmap fonts only");
    int strike = 0;
    for (int i = 1; i < face->num_fixed_sizes; ++i)
      if (std::abs(face->available_sizes[i].height - s.font_pixels) <
          std::abs(face->available_sizes[strike].height - s.font_pixels))
        strike = i;
    if (FT_Select_Size(face, strike))
      throw std::runtime_error("Cannot select menu font size");
    glyphs_.clear();
    for (uint32_t c = 32; c < 127; ++c) {
      if (FT_Load_Char(face, c,
                       FT_LOAD_RENDER | FT_LOAD_MONOCHROME |
                           FT_LOAD_TARGET_MONO))
        continue;
      auto &b = face->glyph->bitmap;
      Glyph g{int(b.width),
              int(b.rows),
              face->glyph->bitmap_left,
              face->glyph->bitmap_top,
              int(face->glyph->advance.x / 64),
              {}};
      g.alpha.resize(size_t(g.width) * g.height);
      for (int y = 0; y < g.height; ++y)
        for (int x = 0; x < g.width; ++x) {
          auto row = b.buffer +
                     (b.pitch >= 0 ? y : g.height - y - 1) * std::abs(b.pitch);
          g.alpha[y * g.width + x] =
              b.pixel_mode == FT_PIXEL_MODE_MONO
                  ? ((row[x / 8] & (128 >> (x % 8))) ? 255 : 0)
                  : row[x];
        }
      glyphs_[c] = std::move(g);
    }
    font_ = s.font_file;
    pixels_ = s.font_pixels;
    unit_ = std::max(1, (pixels_ + 15) / 16);
    width_ = 640 * unit_;
    height_ = 480 * unit_;
  } catch (...) {
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    throw;
  }
  FT_Done_Face(face);
  FT_Done_FreeType(library);
}
void SpatialPalette::raster(const EditorState &s) {
  font(s);
  rgba_.assign(size_t(width_) * height_ * 4, 0);
  using Color = std::array<uint8_t, 4>;
  Color bg = s.light ? Color{229, 233, 239, uint8_t(s.opacity)}
                     : Color{31, 37, 45, uint8_t(s.opacity)},
        fg = s.light ? Color{30, 37, 46, 255} : Color{231, 235, 241, 255};
  Color accent{uint8_t(s.accent >> 16), uint8_t(s.accent >> 8),
               uint8_t(s.accent), 255};
  auto rect = [&](int x, int y, int w, int h, Color c) {
    for (int row = std::max(0, y); row < std::min(height_, y + h); ++row)
      for (int col = std::max(0, x); col < std::min(width_, x + w); ++col)
        for (int k = 0; k < 4; ++k)
          rgba_[(size_t(row) * width_ + col) * 4 + k] = c[k];
  };
  auto text = [&](int center, int baseline, const std::string &label,
                  Color color) {
    int advance = 0;
    for (unsigned char c : label)
      if (glyphs_.contains(c))
        advance += glyphs_.at(c).advance;
    int x = center - advance / 2;
    for (unsigned char c : label) {
      auto found = glyphs_.find(c);
      if (found == glyphs_.end())
        continue;
      auto &g = found->second;
      for (int y = 0; y < g.height; ++y)
        for (int col = 0; col < g.width; ++col)
          if (g.alpha[y * g.width + col])
            rect(x + g.left + col, baseline - g.top + y, 1, 1, color);
      x += g.advance;
    }
  };
  rect(0, 0, width_, height_, bg);
  const char *labels[] = {"Clay", "Carve", "Smooth", "Flatten",
                          "Undo", "Redo",  "Save",   "Export glTF"};
  constexpr float pi = 3.14159265358979323846f;
  for (int i = 0; i < 8; ++i) {
    float a = i * pi / 4;
    int x = int(320 + 190 * std::cos(a)) * unit_,
        y = int(240 - 155 * std::sin(a)) * unit_;
    rect(x - 76 * unit_, y - 28 * unit_, 152 * unit_, 56 * unit_,
         i == s.menu_index ? accent
         : s.light         ? Color{200, 209, 220, 255}
                           : Color{55, 65, 80, 255});
    text(x, y + pixels_ / 3, labels[i], fg);
  }
  text(320 * unit_, 230 * unit_, "Stick to choose", fg);
  text(320 * unit_, 255 * unit_, "Press to confirm", fg);
  text(320 * unit_, 34 * unit_, "Dome Sculpt", fg);
}
void SpatialPalette::draw(const EditorState &s, const Mat4 &vp) {
  if (!s.menu)
    return;
  std::string signature = s.font_file + std::to_string(s.font_pixels) +
                          std::to_string(s.menu_index) +
                          std::to_string(s.light) + std::to_string(s.accent) +
                          std::to_string(s.opacity);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_);
  if (signature != signature_) {
    raster(s);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, rgba_.data());
    signature_ = signature;
  }
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                      GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(program_);
  auto m = multiply(vp, model_matrix(s.panel));
  glUniformMatrix4fv(glGetUniformLocation(program_, "matrix"), 1, GL_FALSE,
                     m.data());
  glUniform1i(glGetUniformLocation(program_, "image"), 0);
  glBindVertexArray(vao_);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);
  glUseProgram(0);
  glDisable(GL_BLEND);
  if (glGetError() != GL_NO_ERROR)
    throw std::runtime_error("Spatial palette GPU rendering failed");
}
} // namespace dome
