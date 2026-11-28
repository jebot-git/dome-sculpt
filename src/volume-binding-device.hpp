// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>

namespace dome::detail::volume_device {
struct InteriorNode {
  uint32_t grid, links, clear_cubes;
};
struct Edge {
  uint32_t to;
  float cells;
};
struct Grid {
  int32_t offset[26];
};
struct Graph {
  std::span<const InteriorNode> interior;
  std::span<const int32_t> grid_to_node;
  std::span<const uint32_t> starts;
  std::span<const Edge> edges;
  Grid grid;
};
struct Seed {
  uint32_t node;
  float cells;
};
struct Propagation {
  size_t remaining_nodes;
  size_t passes = 0;
};

class Solver {
public:
  virtual ~Solver() = default;
  // Read back only the boundary tail of the converged field.
  virtual void solve(std::span<const Seed>,std::span<float> boundary,
                     Propagation &,const std::function<bool()> &cancelled) = 0;
};
std::unique_ptr<Solver> upload(const Graph &,size_t maximum_seeds);
} // namespace dome::detail::volume_device
