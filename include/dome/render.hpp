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
struct SurfaceTextureLimits {
  size_t max_bytes=1024ull*1024*1024,max_images=1024,max_samplers=128;
};
// Final RGB transfer only; alpha stays linear. Linear requires a linear target.
enum class SurfaceOutputEncoding { Srgb, Linear };
struct SurfaceTextureStats {size_t images=0,samplers=0,bytes=0;uint64_t uploads=0;};
class SurfaceRenderer {
  GLuint program_ = 0, vao_ = 0, vertices_ = 0, indices_ = 0;
  size_t count_ = 0;
  std::shared_ptr<const MeshSnapshot> cached_;
  struct TextureState;
  std::unique_ptr<TextureState> textures_;

public:
  SurfaceRenderer();
  explicit SurfaceRenderer(SurfaceTextureLimits);
  SurfaceTextureStats texture_stats() const;
  void set_frame_time(double seconds);
  void set_output_encoding(SurfaceOutputEncoding);
  // Deferred draw leaves a linear framebuffer bound for Cursor/Palette overlays.
  // Resolve before releasing the XR swapchain image or starting another view.
  void set_deferred_resolve(bool);
  void resolve();
  ~SurfaceRenderer();
  void abandon();
  SurfaceRenderer(const SurfaceRenderer &) = delete;
  void draw(std::shared_ptr<const MeshSnapshot>, CanvasTransform,
            const Mat4 &view_projection, ob::Vec3 camera, int width, int height,
            bool transparent = false, bool toon = false, bool light = false);
};
class CursorRenderer {
  GLuint program_ = 0, vao_ = 0, buffer_ = 0;
  bool linear_output_ = false;

public:
  CursorRenderer();
  ~CursorRenderer();
  void set_linear_output(bool value) { linear_output_=value; }
  void draw(const EditorState &, const Mat4 &);
};
} // namespace dome
