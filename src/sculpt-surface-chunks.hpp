// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/local-surface.hpp"
#include <optional>

namespace dome::detail {
// Private preview geometry authority. This is deliberately not a Paint domain,
// SourceMaterialBindings field, serialized asset, or packed triangle number.
// Its factory retains the actual immutable extraction owner. Consumers must
// retain the returned owner while using its chunks or triangle references.
struct SculptChunkLimits {
  LocalSurfaceLimits extraction;
  size_t working_bytes = 1024 * 1024 * 1024;
  size_t retained_bytes = 512 * 1024 * 1024;
  size_t dependency_edges = 1'000'000, dependency_chunks = 16'384;
  size_t steps = 16'000'000;
};
struct SculptChunkBounds {
  Vec3 minimum{INFINITY, INFINITY, INFINITY};
  Vec3 maximum{-INFINITY, -INFINITY, -INFINITY};
  bool empty() const;
  void include(Vec3);
  void include(const SculptChunkBounds &);
};
struct SculptTriangleOrigin {
  Int3 cell;
  uint32_t triangle = 0;
  auto operator<=>(const SculptTriangleOrigin &) const = default;
};
struct SculptSurfaceChunk {
  Int3 coordinate;
  Mesh mesh;
  // Exactly one provenance entry per mesh triangle. Ordering is the original
  // global cell order restricted to this chunk, then original cell triangle.
  std::vector<SculptTriangleOrigin> triangles;
  SculptChunkBounds bounds;
  size_t bytes = 0;
};
struct SculptChunkReport {
  enum class Kind { Reused, Local, Rebuild };
  Kind kind = Kind::Rebuild;
  size_t extracted_tiles = 0, dependency_edges = 0, assembled_chunks = 0;
  size_t vertices = 0, indices = 0, retained_bytes = 0, working_bytes = 0;
  // Full cache directory copying is still present in LocalSurfaceCache. This
  // private stage does not claim the complete pipeline has bounded metadata.
  bool extraction_directory_copy = false;
};
class SculptSurfaceChunks
    : public std::enable_shared_from_this<SculptSurfaceChunks> {
public:
  using Ptr = std::shared_ptr<const SculptSurfaceChunks>;
  using Chunk = std::shared_ptr<const SculptSurfaceChunk>;
  using Cancel = LocalSurfaceCache::Cancel;
  enum class Representation { Chunks, ExactLegacyRepair };
  static Ptr build(const Volume &, SculptChunkLimits = {}, Cancel = {});
  Ptr update(const Volume &, Cancel = {}) const;
  Representation representation() const;
  const SculptChunkReport &report() const;
  const LocalSurfaceCache::Owner &source_owner() const;
  const SculptChunkBounds &bounds() const;
  // No borrowed span survives the callback. The actual shared Chunk owner is
  // provided so query/GPU clients cannot confuse recycled addresses or packed
  // offsets with geometry identity. Never invokes full materialize().
  void for_each_chunk(const std::function<void(const Chunk &)> &,
                      Cancel = {}) const;
  // Sealed base->next changes, including removals (null Chunk). Returns false
  // without callbacks if this is not the actual retained base owner; callers
  // must then reconcile the complete manifest. No revision/directory address
  // shortcut authorizes a delta, and the weak base does not retain history.
  bool for_each_change(const Ptr &base,
                       const std::function<void(Int3,const Chunk &)> &,
                       Cancel = {}) const;
  // A concrete query/framing primitive: actual immutable corner values plus
  // original cell/triangle identity. Callers may build per-chunk acceleration;
  // no fictitious global triangle ID is manufactured for old packed readers.
  void for_each_triangle(
      const std::function<void(const Chunk &, size_t,
                               const std::array<Vec3, 3> &,
                               const std::array<Vec3, 3> &)> &,
      Cancel = {}) const;
  // Explicit O(surface) operation boundary for Cut/export/legacy consumers.
  // Exact repair delegates to the existing authenticated cache materializer.
  // Ordinary chunk flatten preserves its global first-use vertex ordering.
  Mesh materialize(Cancel = {}) const;

private:
  struct State;
  explicit SculptSurfaceChunks(std::shared_ptr<const State>);
  std::shared_ptr<const State> state_;
};
} // namespace dome::detail
