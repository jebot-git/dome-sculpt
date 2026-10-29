// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#define GL_GLEXT_PROTOTYPES
#include "document.hpp"
#include <GL/gl.h>
#include <GL/glext.h>
#include <unordered_map>
namespace dome {
using Mat4 = std::array<float, 16>;
// Matrix construction follows OpenBrush Native's projection renderer.
Mat4 model_matrix(CanvasTransform);
Mat4 view_matrix(ob::Vec3, ob::Quat);
Mat4 multiply(const Mat4 &, const Mat4 &);
Mat4 projection(float left, float right, float down, float up,
                float near = .01f, float far = 100.f);
class SurfaceRenderer {
  GLuint program_ = 0, vao_ = 0, vertices_ = 0, indices_ = 0;
  size_t count_ = 0;
  std::shared_ptr<const MeshSnapshot> cached_;
  std::unordered_map<uint64_t, std::array<GLuint, 3>> textures_;

public:
  SurfaceRenderer();
  ~SurfaceRenderer();
  void abandon() {
    program_ = vao_ = vertices_ = indices_ = 0;
    cached_.reset();
    textures_.clear();
    count_ = 0;
  }
  SurfaceRenderer(const SurfaceRenderer &) = delete;
  void draw(std::shared_ptr<const MeshSnapshot>, CanvasTransform,
            const Mat4 &view_projection, ob::Vec3 camera, int width, int height,
            bool transparent = false, bool toon = false, bool light = false);
};
class CursorRenderer {
  GLuint program_ = 0, vao_ = 0, buffer_ = 0;

public:
  CursorRenderer();
  ~CursorRenderer();
  void draw(const EditorState &, const Mat4 &);
};
} // namespace dome
