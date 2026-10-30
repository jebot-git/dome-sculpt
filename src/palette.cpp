// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from OpenBrush Native's BDF palette and texture renderer
// (Apache-2.0).

#include "sculpt-ui-values.hpp"
#include "dome/palette.hpp"
#include <cstdio>
#include "palette-text.hpp"
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
in vec2 texcoord;uniform sampler2D image;uniform bool linearOutput;out vec4 color;
vec3 encodeSrgb(vec3 c){return mix(12.92*c,1.055*pow(max(c,vec3(0)),vec3(1./2.4))-.055,step(vec3(.0031308),c));}
void main(){color=texture(image,texcoord);if(!linearOutput)color.rgb=encodeSrgb(color.rgb);})";
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
  if (s.font_file == font_ && s.font_pixels == pixels_ && text_font_)
    return;
  auto selected = std::make_unique<palette_text::Font>(s.font_file, s.font_pixels);
  text_font_ = std::move(selected);
  font_ = s.font_file;
  pixels_ = std::clamp(s.font_pixels, 4, 128);
  unit_ = std::max(1, (pixels_ + 15) / 16);
  width_ = height_ = 640 * unit_;
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
                  Color color, int maximum = 0) {
    // Keep all labels inside their existing palette cell. User names/errors
    // are clipped by measured advance at complete grapheme boundaries.
    int room = maximum > 0 ? maximum : std::max(0, 2 * std::min(center - 8 * unit_, width_ - center - 8 * unit_));
    auto line = text_font_->layout(label, room);
    int x = center - line.advance / 2;
    for (auto &placed : line.glyphs) {
      auto &g = *placed.bitmap;
      for (int y = 0; y < g.height; ++y)
        for (int col = 0; col < g.width; ++col)
          if (g.alpha[size_t(y) * g.width + col] &&
              x + placed.x + g.left + col >= center - room / 2 &&
              x + placed.x + g.left + col < center + (room + 1) / 2)
            rect(x + placed.x + g.left + col,
                 baseline - placed.y - g.top + y, 1, 1, color);
    }
  };
  rect(0, 0, width_, height_, bg);
  if (s.controls.parameter == VrParameter::UvEdit && s.uv) {
    auto &v = *s.uv;
    auto &m = *v.mesh;
    text(320 * unit_, 50 * unit_,
         v.solving ? "Processing UV charts..."
         : v.valid ? (v.tool == 1   ? "UV scale: slide / vertical fine wheel"
                    : v.tool == 2 ? "UV turn: slide / vertical fine wheel"
                    : v.vertices  ? "UV vertices: drag / stick to move"
                                  : "UV islands: drag / stick to move")
                 : v.error,
         v.valid || v.validating || v.committing ? fg : Color{255, 150, 100, 255});
    auto button = [&](int column, int y, const char *name,
                      bool selected = false, bool disabled = false) {
      disabled = disabled || v.committing || v.solving || !v.topology;
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
    for (int i = 0; i < 2; ++i) {
      int y = 145 + i * 52;
      rect(8 * unit_, y * unit_, 94 * unit_, 42 * unit_,
           s.light ? Color{200, 209, 220, uint8_t(s.opacity)} : Color{55, 65, 80, uint8_t(s.opacity)});
      text(55 * unit_, (y + 29) * unit_, i ? "Stitch" : "Unwrap",
           v.solving || v.committing || !v.topology ? Color{140, 140, 140, 255} : fg);
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
    if (v.topology) for (auto &e : v.topology->edges) {
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
    if (v.edge < m.indices.size()) {
      auto a = v.edge, b = a / 3 * 3 + (a + 1) % 3;
      line(m.uv[m.indices[a]], m.uv[m.indices[b]], accent);
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
         !v.committing && !v.solving && (v.valid || v.validating) ? accent : grid);
    text(170 * unit_, 616 * unit_, "Cancel", fg);
    text(470 * unit_, 616 * unit_,
         v.committing ? "Applying..." : v.solving ? "Working..." : v.validating ? "Check & Apply" : "Apply / Press", fg);
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
      if(c.parameter==VrParameter::SkinDistances)label=std::string(row==0?"Seed ":row==1?"Seed margin ":"Transfer ");
      else if(c.parameter==VrParameter::BakeDistances)label=std::string(row==0?"Front ":row==1?"Back ":"AO radius ");
      else if(c.parameter==VrParameter::BakeQuality)label=row==0?"AO samples ":"Padding ";
      else if(c.parameter==VrParameter::CutDirection)label=row==0?"Yaw ":"Pitch ";
      else if (c.components > 1)
        label = std::string(1, (c.parameter == VrParameter::RigRotation ? "XYZ" : "RGBA")[row]) + " ";
      if(c.parameter==VrParameter::ProceduralPattern)label=value<.5f?"Raised panels":value<1.5f?"Recessed cuts":"Mixed detail";
      else if(c.parameter==VrParameter::ProceduralFace){const char *faces[]={"+X","-X","+Y","-Y","+Z","-Z"};label=std::string("Local face ")+faces[std::clamp(int(std::lround(value)),0,5)];}
      else if(c.parameter==VrParameter::ProceduralGrid)label=std::string(row==0?"Rows ":"Columns ")+std::to_string(int(std::lround(value)));
      else if(c.parameter==VrParameter::ProceduralCoverage)label=std::string(row==0?"Density ":row==1?"Gap ":"Rounding ")+std::to_string(int(std::lround(value*(row==1?90:100))))+"%";
      else if(c.parameter==VrParameter::ProceduralSeed)label="Seed "+std::to_string(int(std::lround(value)));
      else if(c.parameter==VrParameter::ProceduralDepth){char text[64];snprintf(text,sizeof(text),"Depth %.4g world mm",double(value));label=text;}
      else if(c.parameter==VrParameter::SculptMaskMode) label=value<.5f?"Add protection":value<1.5f?"Remove protection":"Smooth protection";
      else if(c.parameter==VrParameter::SculptResolution) {
        char spacing[80];snprintf(spacing,sizeof(spacing),"Spacing %.6g world mm",double(value));label=spacing;
      }
      else if(c.parameter==VrParameter::SculptPlanes) {
        if(row==3) label=std::string("Protection view ")+(value>=.5f?"On":"Off");
        else {char size[64];snprintf(size,sizeof(size),"%c %.2f mm", "XYZ"[row],double(sculpt_plane_metres(value))*1000);label=size;}
      }
      else if(c.parameter==VrParameter::SkinSource){
        auto index=size_t(std::clamp(std::lround(value),0l,long(s.skin_sources.empty()?0:s.skin_sources.size()-1)));
        auto id=s.skin_sources.empty()?0:s.skin_sources[index];
        auto found=std::find_if(s.objects.begin(),s.objects.end(),[&](const auto &o){return o.id==id;});
        label=found==s.objects.end()?"No reference":found->name+(found->visible?"":" [hidden]");
      }
      else if(c.parameter==VrParameter::SkinMode)label=value<.5f?"Automatic binding":"Barycentric transfer";
      else if(c.parameter==VrParameter::SkinDistances){char size[48];snprintf(size,sizeof(size),"%.2f mm",double(skin_distance_metres(value))*1000);label+=size;}
      else if(c.parameter==VrParameter::SkinFalloff){char size[48];snprintf(size,sizeof(size),"Falloff %.2f",double(value));label=size;}
      else if(c.parameter==VrParameter::SkinInfluences)label="Max influences "+std::to_string(int(std::lround(value)));
      else if(c.parameter==VrParameter::BakeSource){
        size_t index=size_t(std::clamp(std::lround(value),0l,long(s.bake_sources.size()?s.bake_sources.size()-1:0)));
        auto id=s.bake_sources.empty()?0:s.bake_sources[index];
        auto found=std::find_if(s.objects.begin(),s.objects.end(),[&](auto&o){return o.id==id;});
        label=found==s.objects.end()?"No source":found->name+(found->visible?"":" [hidden]");
      }
      else if(c.parameter==VrParameter::BakeQuality)label+=std::to_string(int(std::lround(value*(row==0?256:64))))+(row==1?" px":"");
      else if(c.parameter==VrParameter::BakeDistances){char size[48];snprintf(size,sizeof(size),"%.2f mm",double(value));label+=size;}
      else if(c.parameter==VrParameter::CutOffset){char size[48];snprintf(size,sizeof(size),"%.2f mm",double(value));label+=size;}
      else if (c.parameter == VrParameter::RigRotation || c.parameter==VrParameter::CutDirection)
        label += std::to_string(int(std::lround(value))) + " deg";
      else if (c.parameter == VrParameter::BrushRadius)
        label += std::to_string(int(std::lround(value * 1000))) + " mm";
      else if (c.parameter == VrParameter::VoxelSize || c.parameter == VrParameter::RetopoDistance) {
        char size[48];
        snprintf(size, sizeof(size), "%.2f mm", double(value));
        label += size;
      }
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
    } else if (c.parameter != VrParameter::RigRotation && c.parameter!=VrParameter::CutDirection && c.parameter!=VrParameter::CutOffset && c.parameter!=VrParameter::BakeDistances && c.parameter!=VrParameter::BakeQuality && !skin_parameter(c.parameter) && !sculpt_parameter(c.parameter) && !procedural_parameter(c.parameter) && c.parameter!=VrParameter::SculptResolution) {
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
    text(470 * unit_, 616 * unit_,
         c.parameter == VrParameter::VoxelSize ? "Remesh / Press" : "Done / Press", fg);
    if (c.parameter == VrParameter::RigRotation || c.parameter == VrParameter::MorphWeight)
      text(320 * unit_, 480 * unit_, "Apply on Done / Press", fg);
    if (c.parameter == VrParameter::VoxelSize) {
      text(320 * unit_, 470 * unit_, "New sculpt from the displayed shape", fg);
      text(320 * unit_, 505 * unit_, "Source retained and hidden", fg);
    }
    return;
  }
  auto current = std::find_if(s.objects.begin(), s.objects.end(),
                              [&](auto &o) { return o.id == s.active_object; });
  auto marked = [&](int sector) -> std::optional<bool> {
    if(s.menu_page==17&&s.cut){if(sector==0)return true;if(sector==5)return s.cut->settings.flipped;if(sector==6)return s.cut->settings.cap;}
    if (s.menu_page == 16 && s.retopology) {
      auto &r = *s.retopology;
      if (sector == 0) return true;
      if (sector == 1) return r.moving;
      if (sector == 6 && r.topology && !r.selected.empty())
        return std::all_of(r.selected.begin(), r.selected.end(), [&](auto id) { return r.topology->vertices().at(id).pinned; });
    }
    if(s.menu_page==15) {
      if(sector==0) return s.sculpt_settings.mask_edit;
      if(sector==6) return (s.sculpt_settings.symmetry_axes&1)!=0;
      if(sector==7) return (s.sculpt_settings.symmetry_axes&2)!=0;
      if(sector==9) return (s.sculpt_settings.symmetry_axes&4)!=0;
    }
    if (s.menu_page == 13) {
      if (sector == 0) return s.weights.enabled;
      if (sector == 6) return s.weights.symmetry;
      if (sector == 7) return !s.weights.locks.empty() && s.weights.locks.at(s.rig_joint);
      if (sector == 9) return s.weights.linked;
    }
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
    if (s.menu_page == 7 && sector == 3)
      return s.sculpt_layer_visible;
    if (s.menu_page == 7 && sector == 4)
      return s.sculpt_layer_locked;
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
    const std::string label = s.menu_page == 16 && i == 0 && s.retopology ? "Resume patch" : s.menu_page == 15 && i == 0 && s.sculpt_settings.mask_edit ? "Resume mask" : labels[i];
    int cell = 102 * unit_;
    auto fitted = text_font_->layout(label, cell);
    size_t split = std::string::npos;
    int balance = cell + 1;
    if (fitted.text != label)
      for (size_t at = label.find(' '); at != std::string::npos; at = label.find(' ', at + 1)) {
        auto left = text_font_->layout(label.substr(0, at), cell);
        auto right = text_font_->layout(label.substr(at + 1), cell);
        if (left.text == label.substr(0, at) && right.text == label.substr(at + 1) &&
            std::max(left.advance, right.advance) < balance) {
          split = at;
          balance = std::max(left.advance, right.advance);
        }
      }
    if (split == std::string::npos)
      text(x, y + pixels_ / 3, label, label_color, cell);
    else {
      int spacing = pixels_ + 2 * unit_;
      text(x, y + pixels_ / 3 - spacing / 2, label.substr(0, split), label_color, cell);
      text(x, y + pixels_ / 3 + spacing / 2, label.substr(split + 1), label_color, cell);
    }
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
  if (s.menu_page != 11 && s.menu_page != 12 && s.menu_page != 13 && s.menu_page != 14 && s.menu_page != 15 && s.menu_page != 16 && s.menu_page != 17) {
    text(320 * unit_, 295 * unit_, "Stick to choose", fg);
    text(320 * unit_, 320 * unit_, "Press to confirm", fg);
  }
  const char *page = s.menu_page == 18 ? "Procedural sculpt" : s.menu_page == 17 ? "Plane cut" : s.menu_page == 16 ? "Retopology" : s.menu_page == 15 ? "Masks / symmetry" : s.menu_page == 14 ? "Bind / transfer skin"
                     : s.menu_page == 13 ? "Skin weights"
                     : s.menu_page == 12 ? "Mesh baking"
                     : s.menu_page == 11 ? "Rig and shapekeys"
                     : s.menu_page == 10 ? "Mesh tools"
                     : s.menu_page == 9 ? "Avatars"
                     : s.menu_page == 8 ? "Recovery"
                     : s.menu_page == 7   ? "Sculpt layers"
                     : s.menu_page == 1 ? "Objects"
                     : s.menu_page == 2 ? "Material"
                     : s.menu_page == 3 ? "Painting"
                     : s.menu_page == 4 ? "Brush"
                                        : "Dome Sculpt";
  text(320 * unit_, 345 * unit_, page, fg);
  auto active = std::find_if(s.objects.begin(), s.objects.end(),
                             [&](auto &o) { return o.id == s.active_object; });
  if(s.menu_page==18) {
    const auto &o=s.procedural_options;
    const char *patterns[]={"Raised panels","Recessed cuts","Mixed detail"};
    text(320*unit_,285*unit_,std::string(patterns[unsigned(o.pattern)])+" / seed "+std::to_string(o.seed),fg);
    text(320*unit_,318*unit_,s.asset_busy?"Generating / Cancel work":"Apply creates a new sculpt layer",fg);
  }
  if(s.menu_page==17){auto c=s.cut;
    text(320*unit_,278*unit_,!c?"Start an editable cutting plane":c->committing?"Applying cut...":c->busy?"Updating preview...":"Apply retains the source",fg);
    text(320*unit_,310*unit_,!c?"Shell and cap are editable":c->settings.cap?"Closed cap":"Open trim",fg);
  }
  if(s.menu_page==15) {
    const char *modes[]={"Add protection","Remove protection","Smooth protection"};
    text(320*unit_,285*unit_,s.sculpt_settings.mask_edit?modes[int(s.sculpt_settings.mask_tool)]:"Geometry brush / mask protected",fg);
    std::string axes;
    for(int k=0;k<3;++k) if(s.sculpt_settings.symmetry_axes&(1u<<k)) axes+="XYZ"[k];
    text(320*unit_,318*unit_,axes.empty()?"No mirrored axes":"Mirror "+axes,fg);
  }
  if(s.menu_page==14) {
    auto source=std::find_if(s.objects.begin(),s.objects.end(),[&](const auto &o){return o.id==s.skin_options.reference;});
    text(320*unit_,285*unit_,source==s.objects.end()?"Choose reference rig":source->name,fg);
    size_t locks=s.skin_use_locks?size_t(std::count(s.weights.locks.begin(),s.weights.locks.end(),uint8_t(1))):0;
    text(320*unit_,318*unit_,std::string(s.skin_options.mode==SkinAuthoringMode::Automatic?"Automatic":"Transfer")+" / "+std::to_string(locks)+" locked",fg);
  }
  if (s.menu_page == 16) {
    auto r = s.retopology;
    text(320 * unit_, 278 * unit_, !r ? "Start a source-constrained patch" : r->moving ? "Move existing points" : "Place / select ordered corners", fg);
    text(320 * unit_, 310 * unit_, !r ? "Source retained on Apply" : r->committing ? "Applying patch..." : r->busy ? "Updating patch..."
         : std::to_string(r->selected.size()) + " selected / " + std::to_string(r->topology ? r->topology->faces().size() : 0) + " faces", fg);
  } else if(s.menu_page==12){
    auto source=std::find_if(s.objects.begin(),s.objects.end(),[&](auto&o){return o.id==s.bake_source;});
    text(320*unit_,285*unit_,source==s.objects.end()?"Choose a different source":source->name,fg);
    text(320*unit_,318*unit_,s.asset_busy?"Working "+std::to_string(std::min(99,int(s.asset_progress*100)))+"% / Cancel work":"Adjust source, bounds and quality",fg);
  } else if (s.menu_page == 13 && s.deformation && s.deformation->skeleton) {
    const char *modes[] = {"Add", "Subtract", "Replace", "Smooth"};
    text(320 * unit_, 290 * unit_, s.deformation->skeleton->bones.at(s.rig_joint).name, fg);
    text(320 * unit_, 320 * unit_, modes[int(s.weights.mode)], fg);
  }
  if (s.menu_page == 11 && s.deformation) {
    auto &d = *s.deformation;
    if (d.skeleton && !d.skeleton->bones.empty())
      text(320 * unit_, 292 * unit_, d.skeleton->bones.at(s.rig_joint).name, fg);
    if (d.morphs && !d.morphs->targets.empty())
      text(320 * unit_, 326 * unit_, d.morphs->targets.at(s.morph_target).name, fg);
  }
  if (s.menu_page == 8) {
    auto name = s.recovery_selected < s.recovery_candidates.size()
        ? s.recovery_candidates[s.recovery_selected] : "Refresh to find projects";
    text(320 * unit_, 580 * unit_, name, fg);
  } else if (s.menu_page == 7) {
    auto name = s.sculpt_layer_name;
    text(320 * unit_, 580 * unit_, name, fg);
  } else if (active != s.objects.end())
    text(320 * unit_, 580 * unit_, active->name, fg);
  if (!s.error.empty())
    text(320 * unit_, 610 * unit_, s.error, fg);
  else if (!s.recovery_error.empty())
    text(320 * unit_, 610 * unit_, s.recovery_error, fg);
  else if (!s.message.empty())
    text(320 * unit_, 610 * unit_, s.message, fg);
}
void SpatialPalette::draw(const EditorState &s, const Mat4 &vp) {
  if (!s.menu)
    return;
  std::string signature =
      s.font_file + std::to_string(s.font_pixels) +
      std::to_string(s.menu_index) + std::to_string(s.menu_page) +
      std::to_string(s.bake_source)+":"+std::to_string(s.asset_busy)+":"+std::to_string(int(s.asset_progress*100))+":"+
      std::to_string(s.skin_options.reference)+":"+std::to_string(int(s.skin_options.mode))+":"+std::to_string(s.skin_use_locks)+":"+
      std::to_string(s.rig_joint) + ":" + std::to_string(s.morph_target) + ":" +
      std::to_string(s.active_object) + std::to_string(s.revision) + s.message + std::to_string(s.recovery_selected) +
      std::to_string(s.recovery_candidates.size()) + s.recovery_error +
      s.error + std::to_string(s.light) + std::to_string(s.accent) +
      std::to_string(s.opacity) + s.sculpt_layer_name +
      std::to_string(s.active_sculpt_layer) +
      std::to_string(s.sculpt_layer_visible) +
      std::to_string(s.sculpt_layer_locked) + std::to_string(s.layer_visible) +
      std::to_string(int(s.paint.channel)) + std::to_string(s.paint.erase);
  signature += ":procedural:"+std::to_string(s.procedural_options.seed)+":"+std::to_string(unsigned(s.procedural_options.pattern));
  signature += ":sculpt:" + std::to_string(s.sculpt_settings.symmetry_axes) + ":" +
      std::to_string(s.sculpt_settings.mask_edit) + ":" + std::to_string(s.sculpt_settings.mask_visible) + ":" +
      std::to_string(int(s.sculpt_settings.mask_tool));
  signature += std::to_string(s.weights.enabled) + std::to_string(s.weights.symmetry) +
      std::to_string(s.weights.linked) + std::to_string(int(s.weights.mode));
  for (auto lock : s.weights.locks) signature += lock ? "1" : "0";
  auto label_signature = [&](const std::string &label) {
    signature += ":" + std::to_string(label.size()) + ":" + label;
  };
  for (auto &object : s.objects) {
    signature += ":" + std::to_string(object.id) + (object.visible ? "1" : "0");
    label_signature(object.name);
  }
  for (auto &candidate : s.recovery_candidates) label_signature(candidate);
  if (s.deformation) {
    if (s.deformation->skeleton && s.rig_joint < s.deformation->skeleton->bones.size())
      label_signature(s.deformation->skeleton->bones[s.rig_joint].name);
    if (s.deformation->morphs && s.morph_target < s.deformation->morphs->targets.size())
      label_signature(s.deformation->morphs->targets[s.morph_target].name);
  }
  label_signature(s.controls.title);
  signature += std::to_string(int(s.controls.parameter)) +
               std::to_string(s.controls.selected);
  if (s.uv)
    signature += std::to_string(s.uv->revision);
  if (s.retopology) {
    const auto &r = *s.retopology;
    signature += ":retopo:" + std::to_string(r.revision) + ":" +
        std::to_string(reinterpret_cast<uintptr_t>(r.topology.get())) + ":" +
        std::to_string(r.moving) + ":" + std::to_string(r.busy) + ":" + std::to_string(r.committing);
  }
  if(s.cut)signature+=":cut:"+std::to_string(s.cut->request)+":"+std::to_string(s.cut->preview_request)+":"+
      std::to_string(s.cut->busy)+":"+std::to_string(s.cut->committing);
  for (float value : s.controls.value)
    signature += std::to_string(value) + ",";
  for (bool enabled : s.menu_enabled)
    signature += enabled ? "1" : "0";
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_);
  if (signature != signature_) {
    raster(s);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, width_, height_, 0, GL_RGBA,
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
  glUniform1i(glGetUniformLocation(program_, "linearOutput"), linear_output_);
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
