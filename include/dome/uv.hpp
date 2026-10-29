// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mesh.hpp"
#include <memory>
namespace dome {
struct UvEdge {
  uint32_t a, b;
  std::vector<uint32_t> corners;
  bool seam = false;
};
struct UvTopology {
  std::vector<uint32_t> face_island;
  std::vector<std::vector<uint32_t>> islands;
  std::vector<UvEdge> edges;
  explicit UvTopology(const Mesh &);
};
struct UvView {
  std::shared_ptr<const Mesh> mesh;
  std::shared_ptr<const UvTopology> topology;
  uint64_t revision = 1;
  uint32_t face = 0, vertex = UINT32_MAX;
  int tool = 0; // move, scale, turn
  bool vertices = false, valid = true, can_undo = false, can_redo = false;
  std::array<float, 2> cursor{};
  std::string error;
};
enum class UvAction {
  Pick,
  Move,
  Rotate,
  Scale,
  Next,
  VertexMode,
  Pin,
  Relax,
  Pack,
  Seam,
  Undo,
  Redo,
  Tool
};
// Both frontends operate the same draft; commit resamples paint off-thread.
class UvWorkspace {
public:
  explicit UvWorkspace(Mesh, uint32_t resolution);
  const UvView &view() const { return view_; }
  void edit(UvAction, float x = 0, float y = 0);

private:
  UvView view_;
  uint32_t resolution_;
  uint64_t revision_ = 1;
  std::vector<UvView> undo_, redo_;
  void publish(Mesh, bool topology = false);
  std::vector<uint32_t> selected() const;
};
bool uv_changed(const Mesh &, const Mesh &);
std::string validate_uv(const Mesh &);
} // namespace dome
