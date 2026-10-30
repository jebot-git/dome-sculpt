// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#define GL_GLEXT_PROTOTYPES
#include "polygon-authoring.hpp"
#include <GL/gl.h>
#include <GL/glext.h>
namespace dome {
struct PolygonBufferStats {
  uint64_t geometry_uploads = 0, selection_uploads = 0, draws = 0;
  size_t geometry_bytes = 0, selection_bytes = 0;
};
// Direct GL preview of the worker's immutable canonical geometry. Component
// motion changes only a uniform; geometry/selection stores are transactional.
class PolygonRenderer {
public:
  PolygonRenderer();
  ~PolygonRenderer();
  PolygonRenderer(const PolygonRenderer &) = delete;
  void abandon();
  PolygonBufferStats stats() const { return stats_; }
  void draw(const PolygonView &, const std::array<float, 16> &object_to_clip,
            const std::array<float, 16> &object_to_world,
            uint32_t accent = 0x6babff, bool linear_output = false,
            float point_size = 7);

private:
  struct Stores;
  std::unique_ptr<Stores> stores_;
  GLuint program_ = 0, points_program_ = 0;
  PolygonBufferStats stats_;
};
} // namespace dome
