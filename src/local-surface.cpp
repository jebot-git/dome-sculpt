// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/local-surface.hpp"
#include "surface-topology.hpp"
#include <limits>
#include <queue>
#include <set>
namespace dome {
namespace {
void stop(const LocalSurfaceCache::Cancel &cancel) {
  if (cancel && cancel())
    throw std::runtime_error("Local surface meshing cancelled");
}
struct Work {
  LocalSurfaceLimits limits;
  LocalSurfaceStats stats;
  LocalSurfaceCache::Cancel cancel;
  void cell() {
    if (++stats.cells_evaluated > limits.evaluated_cells)
      throw std::length_error("Local surface cell evaluation limit exceeded");
    if ((stats.cells_evaluated & 63) == 0)
      stop(cancel);
  }
};
std::array<double, 3> face_cross(Vec3 a, Vec3 b, Vec3 c) {
  double x = double(b.x) - a.x, y = double(b.y) - a.y, z = double(b.z) - a.z;
  double u = double(c.x) - a.x, v = double(c.y) - a.y, w = double(c.z) - a.z;
  return {y * w - z * v, z * u - x * w, x * v - y * u};
}
bool zero_face(const std::array<double, 3> &n) {
  return n[0] == 0 && n[1] == 0 && n[2] == 0;
}
static constexpr Int3 corner[] = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
                                  {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};
static constexpr int tetra[][4] = {{0, 5, 1, 6}, {0, 1, 2, 6}, {0, 2, 3, 6},
                                   {0, 3, 7, 6}, {0, 7, 4, 6}, {0, 4, 5, 6}};

SurfaceCell extract_cell(const SurfaceGeometry &field, Int3 cell) {
  SurfaceCell result;
  result.coordinate = cell;
  Int3 p[8];
  float d[8];
  for (int i = 0; i < 8; ++i) {
    p[i] = cell + corner[i];
    d[i] = field.at(p[i]);
  }

  bool negative = false, nonnegative = false;
  for (float value : d) {
    negative |= value < 0;
    nonnegative |= value >= 0;
  }
  if (!negative || !nonnegative)
    return result;
  result.vertices.reserve(19);
  result.indices.reserve(36);
  result.outward.reserve(12);
  using Edge = SurfaceEdge;
  std::map<Edge, uint32_t> vertices;
  auto vertex = [&](Int3 a, Int3 b, float da, float db) {
    if (da == 0)
      b = a;
    else if (db == 0)
      a = b;
    Edge edge = a < b ? Edge{a, b} : Edge{b, a};
    auto found = vertices.find(edge);
    if (found != vertices.end())
      return found->second;
    Vec3 pa{a.x * field.voxel_size(), a.y * field.voxel_size(),
            a.z * field.voxel_size()},
        pb{b.x * field.voxel_size(), b.y * field.voxel_size(),
           b.z * field.voxel_size()};
    float t = da == db ? 0 : std::clamp(da / (da - db), 0.f, 1.f);
    Vec3 position = pa + (pb - pa) * t;
    uint32_t index = uint32_t(result.vertices.size());
    vertices.emplace(edge, index);
    result.vertices.push_back({edge, position, field.gradient(position)});
    return index;
  };
  auto triangle = [&](uint32_t a, uint32_t b, uint32_t c, Vec3 outward) {
    if (a == b || b == c || a == c)
      return;
    result.indices.insert(result.indices.end(), {a, b, c});
    result.outward.push_back(outward);
  };
  for (auto &tet : tetra) {
    int inside[4], outside[4], ni = 0, no = 0;
    Vec3 in{}, out{};
    for (auto v : tet) {
      // Winding depends only on relative corner positions. Large grid
      // coordinates must not erase the centroid difference through rounding.
      Vec3 point{float(p[v].x - cell.x), float(p[v].y - cell.y),
                 float(p[v].z - cell.z)};
      if (d[v] < 0) {
        inside[ni++] = v;
        in += point;
      } else {
        outside[no++] = v;
        out += point;
      }
    }
    if (!ni || !no)
      continue;
    Vec3 outward = out / float(no) - in / float(ni);
    auto edge = [&](int a, int b) { return vertex(p[a], p[b], d[a], d[b]); };
    if (ni == 1)
      triangle(edge(inside[0], outside[0]), edge(inside[0], outside[1]),
               edge(inside[0], outside[2]), outward);
    else if (no == 1)
      triangle(edge(outside[0], inside[0]), edge(outside[0], inside[1]),
               edge(outside[0], inside[2]), outward);
    else {
      uint32_t a = edge(inside[0], outside[0]), b = edge(inside[0], outside[1]),
               c = edge(inside[1], outside[1]), e = edge(inside[1], outside[0]);
      triangle(a, b, c, outward);
      triangle(a, c, e, outward);
    }
  }
  return result;
}
constexpr size_t index_node_bytes = sizeof(Int3) +
                                    sizeof(std::shared_ptr<const SurfaceTile>) +
                                    4 * sizeof(void *);
size_t tile_bytes(const SurfaceTile &t) {
  size_t bytes = sizeof(SurfaceTile) + t.cells.capacity() * sizeof(SurfaceCell);
  for (auto &c : t.cells)
    bytes += c.vertices.capacity() * sizeof(SurfaceEdgeVertex) +
             c.indices.capacity() * sizeof(uint32_t) +
             c.outward.capacity() * sizeof(Vec3);
  return bytes;
}
std::shared_ptr<const SurfaceTile> make_tile(const SurfaceGeometry &field,
                                             Int3 key, Work &work,
                                             size_t available) {
  stop(work.cancel);
  // Preflight the exact fixed maximum before allocating transient packets.
  constexpr size_t maximum =
      sizeof(SurfaceTile) +
      512 * (sizeof(SurfaceCell) + 19 * sizeof(SurfaceEdgeVertex) +
             36 * sizeof(uint32_t) + 12 * sizeof(Vec3)) +
      4096;
  if (available < maximum)
    throw std::length_error("Local surface tile working-memory limit exceeded");
  auto result = std::make_shared<SurfaceTile>();
  result->coordinate = key;
  result->cells.reserve(512);
  Int3 origin{key.x * 8, key.y * 8, key.z * 8};
  for (int x = 0; x < 8; ++x)
    for (int y = 0; y < 8; ++y)
      for (int z = 0; z < 8; ++z) {
        work.cell();
        auto cell = extract_cell(field, origin + Int3{x, y, z});
        for (auto &vertex : cell.vertices) {
          // Reproduce the exact float coordinates used by
          // Volume::gradient/sample. At large lattice coordinates
          // multiplication/division rounding can need more support than the
          // mathematical two-cell halo. Derive it, never guess.
          for (auto offset :
               {Vec3{field.voxel_size(), 0, 0}, Vec3{-field.voxel_size(), 0, 0},
                Vec3{0, field.voxel_size(), 0}, Vec3{0, -field.voxel_size(), 0},
                Vec3{0, 0, field.voxel_size()},
                Vec3{0, 0, -field.voxel_size()}}) {
            auto q = (vertex.position + offset) / field.voxel_size();
            std::array<int, 3> lower{int(std::floor(q.x)), int(std::floor(q.y)),
                                     int(std::floor(q.z))};
            std::array<int, 3> origin{cell.coordinate.x, cell.coordinate.y,
                                      cell.coordinate.z};
            for (int axis = 0; axis < 3; ++axis)
              result->support_halo = std::max(
                  {result->support_halo, std::abs(lower[axis] - origin[axis]),
                   std::abs(lower[axis] + 1 - origin[axis])});
          }
        }
        if (!cell.vertices.empty())
          result->cells.push_back(std::move(cell));
      }
  ++work.stats.tiles_built;
  if (result->cells.empty())
    return {};
  result->bytes = tile_bytes(*result);
  return result;
}
void validate_limits(const LocalSurfaceLimits &l) {
  if (!l.samples || !l.tiles || !l.dirty_tiles || !l.evaluated_cells ||
      !l.bytes || !l.vertices || !l.indices || !l.regions ||
      l.vertices > UINT32_MAX)
    throw std::invalid_argument("Invalid local surface limits");
}
int floor_div8(int n) {
  int q = n / 8;
  return q - (n % 8 < 0);
}
int bounded(int64_t n) {
  // Reserve arithmetic support headroom and respect Volume's finite workspace.
  if (n < int64_t(INT32_MIN) + 16 || n > int64_t(INT32_MAX) - 16)
    throw std::invalid_argument("Local surface region coordinate overflow");
  return int(n);
}
void insert_tile(std::set<Int3> &keys, Int3 key, size_t limit) {
  keys.insert(key);
  if (keys.size() > limit)
    throw std::length_error("Local surface dirty tile limit exceeded");
}
} // namespace
SurfaceGeometry::SurfaceGeometry(const Volume &v)
    : samples_(v.geometry_owner()), voxel_(v.voxel_size()), band_(v.band()) {}
float SurfaceGeometry::at(Int3 p) const {
  auto found = samples_->find(p);
  return found == samples_->end() ? band_ : found->second;
}
float SurfaceGeometry::sample(Vec3 p) const {
  if (!finite(p) ||
      std::max({std::abs(p.x), std::abs(p.y), std::abs(p.z)}) > 1001)
    throw std::invalid_argument("Invalid field sample position");
  p = p / voxel_;
  Int3 q{int(std::floor(p.x)), int(std::floor(p.y)), int(std::floor(p.z))};
  Vec3 f = p - Vec3{float(q.x), float(q.y), float(q.z)};
  float result = 0;
  for (int z = 0; z < 2; ++z)
    for (int y = 0; y < 2; ++y)
      for (int x = 0; x < 2; ++x)
        result += at(q + Int3{x, y, z}) * (x ? f.x : 1 - f.x) *
                  (y ? f.y : 1 - f.y) * (z ? f.z : 1 - f.z);
  return result;
}
Vec3 SurfaceGeometry::gradient(Vec3 p) const {
  return normalized(
      {sample(p + Vec3{voxel_, 0, 0}) - sample(p - Vec3{voxel_, 0, 0}),
       sample(p + Vec3{0, voxel_, 0}) - sample(p - Vec3{0, voxel_, 0}),
       sample(p + Vec3{0, 0, voxel_}) - sample(p - Vec3{0, 0, voxel_})});
}
Int3 LocalSurfaceCache::tile_coordinate(Int3 cell) {
  return {floor_div8(cell.x), floor_div8(cell.y), floor_div8(cell.z)};
}
LocalSurfaceCache::Ptr LocalSurfaceCache::build(const Volume &field,
                                                LocalSurfaceLimits limits,
                                                Cancel cancel) {
  validate_limits(limits);
  stop(cancel);
  if (field.voxels().size() > limits.samples)
    throw std::length_error("Local surface sample limit exceeded");
  auto result = std::shared_ptr<LocalSurfaceCache>(new LocalSurfaceCache);
  result->source_ = std::make_shared<const SurfaceGeometry>(field);
  result->limits_ = limits;
  Work work{limits, {}, std::move(cancel)};
  std::set<Int3> keys;
  for (auto [p, value] : result->source_->voxels()) {
    if ((++work.stats.samples_scanned & 255) == 0)
      stop(work.cancel);
    if (value < 0)
      for (auto offset : corner)
        insert_tile(keys, tile_coordinate(p - offset), limits.tiles);
  }
  const size_t source_bytes =
      sizeof(Volume::Voxels) + result->source_->voxels().size() * 64 +
      result->source_->voxels().bucket_count() * sizeof(void *);
  if (source_bytes > limits.bytes)
    throw std::length_error(
        "Local surface retained sample-map byte limit exceeded");
  work.stats.source_bytes = source_bytes;
  size_t bytes =
      sizeof(LocalSurfaceCache) + sizeof(SurfaceGeometry) + source_bytes;
  for (auto key : keys) {
    auto tile = make_tile(*result->source_, key, work,
                          limits.bytes > bytes ? limits.bytes - bytes : 0);
    if (tile) {
      bytes += tile->bytes + index_node_bytes;
      if (bytes > limits.bytes)
        throw std::length_error("Local surface cache byte limit exceeded");
      result->support_halo_ =
          std::max(result->support_halo_, tile->support_halo);
      result->tiles_.emplace(key, std::move(tile));
    }
  }
  work.stats.bytes = bytes;
  result->stats_ = work.stats;
  stop(work.cancel);
  return result;
}
LocalSurfaceCache::Ptr
LocalSurfaceCache::update(const Volume &field, Cancel cancel,
                          LocalSurfaceUpdateReport *report) const {
  stop(cancel);
  auto change = field.geometry_change(source_->geometry_owner());
  LocalSurfaceUpdateReport result;
  result.ancestry_events = change.events;
  if (field.voxel_size() == source_->voxel_size() &&
      field.band() == source_->band() &&
      change.kind == Volume::GeometryChange::Kind::Unchanged) {
    result.kind = LocalSurfaceUpdateReport::Kind::Reused;
    if (report)
      *report = result;
    return shared_from_this();
  }
  if (field.voxel_size() == source_->voxel_size() &&
      field.band() == source_->band() &&
      change.kind == Volume::GeometryChange::Kind::Local) {
    try {
      auto next = update_regions(field, change.regions(), cancel);
      result.kind = LocalSurfaceUpdateReport::Kind::Local;
      if (report)
        *report = result;
      return next;
    } catch (const std::length_error &) {
      stop(cancel);
      result.capacity_fallback = true;
    }
  }
  auto next = build(field, limits_, cancel);
  result.kind = LocalSurfaceUpdateReport::Kind::Rebuild;
  if (report)
    *report = result;
  return next;
}
LocalSurfaceCache::Ptr
LocalSurfaceCache::update_regions(const Volume &field,
                                  std::span<const SurfaceSampleRegion> regions,
                                  Cancel cancel) const {
  stop(cancel);
  if (field.voxel_size() != source_->voxel_size())
    throw std::invalid_argument(
        "Local surface voxel size changed; rebuild required");
  if (field.voxels().size() > limits_.samples)
    throw std::length_error("Local surface sample limit exceeded");
  if (regions.size() > limits_.regions)
    throw std::length_error("Local surface region limit exceeded");
  // Held immutable source keeps the map alive, so raw map identity cannot be
  // accidentally reused for another allocation (unlike a revision or weak ptr).
  if (&field.voxels() == &source_->voxels())
    return shared_from_this();
  if (regions.empty())
    throw std::invalid_argument("Changed geometry requires complete dirty "
                                "sample regions or a full rebuild");
  std::set<Int3> dirty;
  for (auto r : regions) {
    stop(cancel);
    if (r.minimum.x > r.maximum.x || r.minimum.y > r.maximum.y ||
        r.minimum.z > r.maximum.z)
      throw std::invalid_argument("Invalid local surface dirty region");
    // Every cached vertex records the exact lattice footprint used by its
    // float central-difference/trilinear sampling. Expanding by the retained
    // maximum invalidates all affected normals, even at large coordinates.
    // New crossings depend on at most one adjacent sample and are covered too.
    const int halo = support_halo_;
    auto lo = tile_coordinate({bounded(int64_t(r.minimum.x) - halo),
                               bounded(int64_t(r.minimum.y) - halo),
                               bounded(int64_t(r.minimum.z) - halo)});
    auto hi = tile_coordinate({bounded(int64_t(r.maximum.x) + halo),
                               bounded(int64_t(r.maximum.y) + halo),
                               bounded(int64_t(r.maximum.z) + halo)});
    uint64_t nx = uint64_t(int64_t(hi.x) - lo.x + 1),
             ny = uint64_t(int64_t(hi.y) - lo.y + 1),
             nz = uint64_t(int64_t(hi.z) - lo.z + 1);
    if (nx > limits_.dirty_tiles || ny > limits_.dirty_tiles / nx ||
        nz > limits_.dirty_tiles / (nx * ny))
      throw std::length_error(
          "Local surface dirty tile limit exceeded; rebuild required");
    for (int64_t x = lo.x; x <= hi.x; ++x)
      for (int64_t y = lo.y; y <= hi.y; ++y)
        for (int64_t z = lo.z; z <= hi.z; ++z)
          insert_tile(dirty, {int(x), int(y), int(z)}, limits_.dirty_tiles);
  }
  auto result = std::shared_ptr<LocalSurfaceCache>(new LocalSurfaceCache);
  result->source_ = std::make_shared<const SurfaceGeometry>(field);
  result->limits_ = limits_;
  result->tiles_ = tiles_;
  result->support_halo_ = support_halo_;
  Work work{limits_, {}, std::move(cancel)};
  const size_t source_bytes =
      sizeof(Volume::Voxels) + result->source_->voxels().size() * 64 +
      result->source_->voxels().bucket_count() * sizeof(void *);
  if (source_bytes > limits_.bytes)
    throw std::length_error(
        "Local surface retained sample-map byte limit exceeded");
  work.stats.source_bytes = source_bytes;
  size_t bytes = stats_.bytes - stats_.source_bytes + source_bytes,
         reused = tiles_.size();
  for (auto key : dirty) {
    auto old = result->tiles_.find(key);
    if (old != result->tiles_.end()) {
      bytes -= old->second->bytes + index_node_bytes;
      --reused;
      result->tiles_.erase(old);
    }
    auto tile = make_tile(*result->source_, key, work,
                          limits_.bytes > bytes ? limits_.bytes - bytes : 0);
    if (tile) {
      if (result->tiles_.size() >= limits_.tiles)
        throw std::length_error("Local surface tile limit exceeded");
      bytes += tile->bytes + index_node_bytes;
      if (bytes > limits_.bytes)
        throw std::length_error("Local surface cache byte limit exceeded");
      result->support_halo_ =
          std::max(result->support_halo_, tile->support_halo);
      result->tiles_.emplace(key, std::move(tile));
    }
  }
  work.stats.bytes = bytes;
  work.stats.tiles_reused = reused;
  result->stats_ = work.stats;
  stop(work.cancel);
  return result;
}
Mesh LocalSurfaceCache::materialize(Cancel cancel) const {
  stop(cancel);
  struct Cursor {
    const SurfaceTile *tile;
    size_t cell;
  };
  auto later = [](Cursor a, Cursor b) {
    return a.tile->cells[a.cell].coordinate > b.tile->cells[b.cell].coordinate;
  };
  std::priority_queue<Cursor, std::vector<Cursor>, decltype(later)> queue(
      later);
  size_t enqueued = 0;
  for (auto &[key, tile] : tiles_) {
    if ((++enqueued & 255) == 0)
      stop(cancel);
    queue.push({tile.get(), 0});
  }
  std::map<SurfaceEdge, uint32_t> vertices;
  std::map<std::array<float, 3>, uint32_t> exact_positions;
  std::vector<SurfaceEdgeVertex> requests;
  Mesh result;
  std::vector<surface_detail::Triangle> collapsed;
  size_t visited = 0;
  while (!queue.empty()) {
    if ((++visited & 63) == 0)
      stop(cancel);
    auto cursor = queue.top();
    queue.pop();
    auto &cell = cursor.tile->cells[cursor.cell];
    std::vector<uint32_t> local;
    local.reserve(cell.vertices.size());
    for (auto &v : cell.vertices) {
      auto found = vertices.find(v.edge);
      if (found == vertices.end()) {
        if (vertices.size() >= limits_.vertices)
          throw std::length_error("Local surface vertex limit exceeded");
        // Include the global edge map's conservative node overhead, request
        // storage, final compact output and remap in the export working budget.
        constexpr size_t per_vertex = 320;
        size_t available =
            limits_.bytes > stats_.bytes ? limits_.bytes - stats_.bytes : 0;
        size_t index_bytes = result.indices.size() * 64;
        if (index_bytes > available ||
            vertices.size() + 1 > (available - index_bytes) / per_vertex)
          throw std::length_error(
              "Local surface materialization byte limit exceeded");
        auto key =
            std::array<float, 3>{v.position.x, v.position.y, v.position.z};
        auto duplicate = exact_positions.find(key);
        uint32_t id;
        if (duplicate != exact_positions.end())
          id = duplicate->second;
        else {
          id = uint32_t(requests.size());
          requests.push_back(v);
          exact_positions.emplace(key, id);
        }
        found = vertices.emplace(v.edge, id).first;
      }
      local.push_back(found->second);
    }
    if (cell.indices.size() > limits_.indices - result.indices.size())
      throw std::length_error("Local surface index limit exceeded");
    size_t available =
        limits_.bytes > stats_.bytes ? limits_.bytes - stats_.bytes : 0;
    size_t vertex_bytes = vertices.size() * 320;
    if (vertex_bytes > available ||
        result.indices.size() + cell.indices.size() >
            (available - vertex_bytes) / 64)
      throw std::length_error(
          "Local surface materialization byte limit exceeded");
    for (size_t t = 0; t < cell.indices.size(); t += 3) {
      auto a = local[cell.indices[t]], b = local[cell.indices[t + 1]],
           c = local[cell.indices[t + 2]];
      if (a == b || b == c || a == c)
        continue;
      auto n = face_cross(requests[a].position, requests[b].position,
                          requests[c].position);
      if (zero_face(n)) {
        collapsed.push_back({a, b, c});
        continue;
      }
      auto out = cell.outward[t / 3];
      if (n[0] * out.x + n[1] * out.y + n[2] * out.z < 0)
        std::swap(b, c);
      result.indices.insert(result.indices.end(), {a, b, c});
    }
    if (++cursor.cell < cursor.tile->cells.size())
      queue.push(cursor);
  }
  Mesh geometry;
  geometry.positions.reserve(requests.size());
  for (auto &v : requests)
    geometry.positions.push_back(v.position);
  geometry.indices = std::move(result.indices);
  surface_detail::complete_collinear_surface(geometry, collapsed, cancel,
                                             16'000'000, limits_.indices);
  result.indices = std::move(geometry.indices);
  std::vector<uint32_t> remap(requests.size(), UINT32_MAX);
  for (auto &i : result.indices) {
    if ((++visited & 255) == 0)
      stop(cancel);
    if (remap[i] == UINT32_MAX) {
      remap[i] = uint32_t(result.positions.size());
      result.positions.push_back(requests[i].position);
      result.normals.push_back(requests[i].normal);
    }
    i = remap[i];
  }
  stop(cancel);
  return result;
}
} // namespace dome
