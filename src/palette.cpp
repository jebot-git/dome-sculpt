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
  const float vertices[] = {-.3f, -.3f, 0, 1, .3f, -.3f, 1, 1, -.3f, .3f, 0, 0,
                            -.3f, .3f,  0, 0, .3f, -.3f, 1, 1, .3f,  .3f, 1, 0};
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
    height_ = 640 * unit_;
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
               uint8_t(s.accent), uint8_t(s.opacity)};
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
  if (s.controls.parameter == VrParameter::UvEdit && s.uv) {
    auto &v = *s.uv;
    auto &m = *v.mesh;
    text(320 * unit_, 50 * unit_,
         v.valid ? (v.tool == 1   ? "UV scale: slide / vertical fine wheel"
                    : v.tool == 2 ? "UV turn: slide / vertical fine wheel"
                    : v.vertices  ? "UV vertices: drag / stick to move"
                                  : "UV islands: drag / stick to move")
                 : v.error.substr(0, 65),
         v.valid ? fg : Color{255, 150, 100, 255});
    auto button = [&](int column, int y, const char *name,
                      bool selected = false, bool disabled = false) {
      rect((40 + 94 * column) * unit_, y * unit_, 88 * unit_,
           (y == 68 ? 34 : 42) * unit_,
           selected ? accent
                    : (s.light ? Color{200, 209, 220, uint8_t(s.opacity)}
                               : Color{55, 65, 80, uint8_t(s.opacity)}));
      text((84 + 94 * column) * unit_, (y + (y == 68 ? 25 : 29)) * unit_, name,
           disabled ? Color{uint8_t((int(fg[0]) + bg[0]) / 2),
                            uint8_t((int(fg[1]) + bg[1]) / 2),
                            uint8_t((int(fg[2]) + bg[2]) / 2), 255}
                    : fg);
    };
    const char *top[] = {"Move", "Scale", "Turn", "Turn 90", "Undo", "Redo"};
    const char *bottom[] = {v.vertices ? "Vertices" : "Islands",
                            "Pin",
                            "Relax",
                            "Pack",
                            "Seam",
                            "Next"};
    for (int i = 0; i < 6; ++i) {
      button(i, 68, top[i], i == v.tool,
             (i == 4 && !v.can_undo) || (i == 5 && !v.can_redo));
      button(i, 538, bottom[i], i == 0 && v.vertices);
    }
    rect(110 * unit_, 110 * unit_, 420 * unit_, 420 * unit_,
         s.light ? Color{208, 208, 208, uint8_t(s.opacity)}
                 : Color{48, 48, 48, uint8_t(s.opacity)});
    auto line = [&](std::array<float, 2> a, std::array<float, 2> b,
                    Color color) {
      for (int k = 0; k < 2; ++k) {
        a[k] = std::clamp(a[k], 0.f, 1.f);
        b[k] = std::clamp(b[k], 0.f, 1.f);
      }
      int x0 = int((110 + 420 * a[0]) * unit_),
          y0 = int((110 + 420 * a[1]) * unit_),
          x1 = int((110 + 420 * b[0]) * unit_),
          y1 = int((110 + 420 * b[1]) * unit_);
      int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0),
          sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
      for (;;) {
        rect(x0, y0, unit_, unit_, color);
        if (x0 == x1 && y0 == y1)
          break;
        int e = 2 * err;
        if (e >= dy) {
          err += dy;
          x0 += sx;
        }
        if (e <= dx) {
          err += dx;
          y0 += sy;
        }
      }
    };
    Color grid = s.light ? Color{170, 170, 170, uint8_t(s.opacity)}
                         : Color{70, 70, 70, uint8_t(s.opacity)};
    for (int i = 0; i <= 8; ++i) {
      float p = i / 8.f;
      line({p, 0}, {p, 1}, grid);
      line({0, p}, {1, p}, grid);
    }
    for (auto &e : v.topology->edges) {
      bool selected = false;
      for (auto c : e.corners)
        selected |=
            v.topology->face_island[c / 3] == v.topology->face_island[v.face];
      Color color = selected
                        ? Color{uint8_t(s.accent >> 16), uint8_t(s.accent >> 8),
                                uint8_t(s.accent), 255}
                        : fg;
      if (std::any_of(e.corners.begin(), e.corners.end(),
                      [&](auto c) { return m.uv_seams[c]; }))
        color = {255, 170, 80, 255};
      for (auto c : e.corners)
        line(m.uv[m.indices[c]], m.uv[m.indices[c / 3 * 3 + (c + 1) % 3]],
             color);
    }
    if (v.vertices)
      for (uint32_t i = 0; i < m.uv.size(); ++i) {
        auto p = m.uv[i];
        if (p[0] < 0 || p[0] > 1 || p[1] < 0 || p[1] > 1)
          continue;
        int size = i == v.vertex ? 9 : 5;
        Color color = m.uv_pins[i] ? Color{255, 170, 80, 255} : fg;
        rect(int((110 + 420 * p[0] - size / 2.f) * unit_),
             int((110 + 420 * p[1] - size / 2.f) * unit_), size * unit_,
             size * unit_, color);
      }
    rect(60 * unit_, 588 * unit_, 220 * unit_, 42 * unit_, grid);
    rect(360 * unit_, 588 * unit_, 220 * unit_, 42 * unit_,
         v.valid ? accent : grid);
    text(170 * unit_, 616 * unit_, "Cancel", fg);
    text(470 * unit_, 616 * unit_, "Apply / Press", fg);
    return;
  }
  if (s.controls.parameter != VrParameter::None) {
    auto &c = s.controls;
    text(320 * unit_, 80 * unit_, c.title, fg);
    auto normalized = [&](float v) {
      return std::clamp(
          c.logarithmic
              ? std::log(v / c.minimum) / std::log(c.maximum / c.minimum)
              : (v - c.minimum) / std::max(1e-8f, c.maximum - c.minimum),
          0.f, 1.f);
    };
    for (int row = 0; row < c.components; ++row) {
      int y = c.components == 1   ? 300
              : c.components == 4 ? 210 + row * 70
                                  : 240 + row * 80;
      float value = c.value[row], t = normalized(value);
      Color track = s.light ? Color{180, 190, 205, uint8_t(s.opacity)}
                            : Color{60, 70, 85, uint8_t(s.opacity)};
      rect(100 * unit_, (y - 8) * unit_, 420 * unit_, 16 * unit_, track);
      rect(100 * unit_, (y - 8) * unit_, int(420 * t) * unit_, 16 * unit_,
           accent);
      rect(int(100 + 420 * t - 12) * unit_, (y - 28) * unit_, 24 * unit_,
           56 * unit_, row == c.selected ? accent : fg);
      std::string label;
      if (c.components > 1)
        label = std::string(1, "RGBA"[row]) + " ";
      if (c.parameter == VrParameter::BrushRadius)
        label += std::to_string(int(std::lround(value * 1000))) + " mm";
      else if (c.parameter == VrParameter::LayerOrder)
        label += "Layer " + std::to_string(int(std::lround(value)) + 1);
      else
        label += std::to_string(int(std::lround(value * 100))) + "%";
      text(320 * unit_, (c.components == 1 ? 220 : y - 38) * unit_, label, fg);
    }
    if (c.components == 1) {
      rect(558 * unit_, 232 * unit_, 40 * unit_, 136 * unit_,
           s.light ? Color{200, 209, 220, uint8_t(s.opacity)}
                   : Color{55, 65, 80, uint8_t(s.opacity)});
      int offset = int(normalized(c.value[0]) * 1000) % 16;
      for (int y = 240 + offset; y < 360; y += 16)
        rect(563 * unit_, y * unit_, 30 * unit_, 3 * unit_, fg);
      rect(556 * unit_, 296 * unit_, 44 * unit_, 8 * unit_, accent);
    } else {
      Color swatch{
          uint8_t(std::clamp(linear_to_srgb(c.value[0]), 0.f, 1.f) * 255),
          uint8_t(std::clamp(linear_to_srgb(c.value[1]), 0.f, 1.f) * 255),
          uint8_t(std::clamp(linear_to_srgb(c.value[2]), 0.f, 1.f) * 255), 255};
      rect(280 * unit_, 470 * unit_, 80 * unit_, 40 * unit_, swatch);
    }
    text(320 * unit_, 530 * unit_, "Aim + trigger: drag slider", fg);
    text(320 * unit_, 560 * unit_,
         c.components == 1 ? "Stick: slide / Vertical: fine wheel"
                           : "Up/down: row / Left/right: value",
         fg);
    rect(60 * unit_, 588 * unit_, 220 * unit_, 42 * unit_,
         s.light ? Color{200, 209, 220, uint8_t(s.opacity)}
                 : Color{55, 65, 80, uint8_t(s.opacity)});
    rect(360 * unit_, 588 * unit_, 220 * unit_, 42 * unit_, accent);
    text(170 * unit_, 616 * unit_, "Cancel", fg);
    text(470 * unit_, 616 * unit_, "Done / Press", fg);
    return;
  }
  auto current = std::find_if(s.objects.begin(), s.objects.end(),
                              [&](auto &o) { return o.id == s.active_object; });
  auto marked = [&](int sector) -> std::optional<bool> {
    if (s.menu_page == 0 && sector < 4)
      return int(s.tool) == sector;
    if (s.menu_page == 1 && current != s.objects.end()) {
      if (sector == 5)
        return current->visible;
      if (sector == 6)
        return current->locked;
    }
    if (s.menu_page == 2 && current != s.objects.end()) {
      if (sector == 9)
        return current->material.toon;
      if (sector == 10)
        return !current->material.toon;
    }
    if (s.menu_page == 3) {
      if (sector == 1)
        return !s.mesh_active;
      if (sector >= 2 && sector <= 6)
        return s.mesh_active && int(s.paint.channel) == sector - 2;
      if (sector == 10)
        return s.paint.erase;
    }
    if (s.menu_page == 4 && sector == 9)
      return s.layer_visible;
    if (s.menu_page == 6 && sector == 10)
      return s.paint.erase;
    return {};
  };
  const auto &labels = spatial_labels(s.menu_page);
  constexpr float pi = 3.14159265358979323846f;
  for (int i = 0; i < 12; ++i) {
    float a = i * pi / 6;
    int x = int(320 + 220 * std::cos(a)) * unit_,
        y = int(320 - 220 * std::sin(a)) * unit_;
    rect(x - 55 * unit_, y - 20 * unit_, 110 * unit_, 40 * unit_,
         i == s.menu_index && s.menu_enabled[i] ? accent
         : s.light ? Color{200, 209, 220, uint8_t(s.opacity)}
                   : Color{55, 65, 80, uint8_t(s.opacity)});
    auto label_color = fg;
    if (!s.menu_enabled[i])
      for (int c = 0; c < 3; ++c)
        label_color[c] = uint8_t((int(fg[c]) + bg[c]) / 2);
    text(x, y + pixels_ / 3, labels[i], label_color);
    if (auto value = marked(i)) {
      int mx = x - 51 * unit_, my = y - 4 * unit_;
      rect(mx, my, 8 * unit_, unit_, label_color);
      rect(mx, my + 7 * unit_, 8 * unit_, unit_, label_color);
      rect(mx, my, unit_, 8 * unit_, label_color);
      rect(mx + 7 * unit_, my, unit_, 8 * unit_, label_color);
      if (*value)
        rect(mx + 2 * unit_, my + 2 * unit_, 4 * unit_, 4 * unit_, label_color);
    }
  }
  text(320 * unit_, 295 * unit_, "Stick to choose", fg);
  text(320 * unit_, 320 * unit_, "Press to confirm", fg);
  const char *page = s.menu_page == 1   ? "Objects"
                     : s.menu_page == 2 ? "Material"
                     : s.menu_page == 3 ? "Painting"
                     : s.menu_page == 4 ? "Brush"
                                        : "Dome Sculpt";
  text(320 * unit_, 345 * unit_, page, fg);
  auto active = std::find_if(s.objects.begin(), s.objects.end(),
                             [&](auto &o) { return o.id == s.active_object; });
  if (active != s.objects.end())
    text(320 * unit_, 580 * unit_, active->name.substr(0, 28), fg);
  if (!s.error.empty())
    text(320 * unit_, 610 * unit_, s.error.substr(0, 64), fg);
  else if (!s.message.empty())
    text(320 * unit_, 610 * unit_, s.message.substr(0, 64), fg);
}
void SpatialPalette::draw(const EditorState &s, const Mat4 &vp) {
  if (!s.menu)
    return;
  std::string signature =
      s.font_file + std::to_string(s.font_pixels) +
      std::to_string(s.menu_index) + std::to_string(s.menu_page) +
      std::to_string(s.active_object) + std::to_string(s.revision) + s.message +
      s.error + std::to_string(s.light) + std::to_string(s.accent) +
      std::to_string(s.opacity) + std::to_string(s.layer_visible) +
      std::to_string(int(s.paint.channel)) + std::to_string(s.paint.erase);
  signature += std::to_string(int(s.controls.parameter)) +
               std::to_string(s.controls.selected);
  if (s.uv)
    signature += std::to_string(s.uv->revision);
  for (float value : s.controls.value)
    signature += std::to_string(value) + ",";
  for (bool enabled : s.menu_enabled)
    signature += enabled ? "1" : "0";
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
