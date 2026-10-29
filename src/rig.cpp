// SPDX-License-Identifier: GPL-3.0-or-later
// Locked-weight normalization semantics adapt Blender BKE_defvert_normalize_ex;
// see docs/RIGGING.md for pinned source, copyright, license and deviations.
#include "dome/rig.hpp"
#include <bit>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <numeric>
#include <queue>
#include <set>
namespace dome {
namespace {
constexpr RigMatrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
void stop(const std::function<bool()> &cancelled) {
  if (cancelled && cancelled())
    throw std::runtime_error("Rig operation cancelled");
}
void name_valid(const std::string &n) {
  if (n.empty() || n.size() > 256 || n.find('\0') != std::string::npos)
    throw std::invalid_argument("Invalid rig/morph name");
}
void mesh_valid(const Mesh &m) {
  m.validate();
  if (m.positions.size() > 2'000'000 || m.indices.size() > 6'000'000)
    throw std::length_error("Rig mesh resource limit exceeded");
  for (auto p : m.positions)
    if (std::max({std::abs(p.x), std::abs(p.y), std::abs(p.z)}) > 10000)
      throw std::invalid_argument(
          "Rig mesh coordinate exceeds10000 metre workspace");
}
float distance2(Vec3 a, Vec3 b) { return dot(a - b, a - b); }
Vec3 point(const RigMatrix &m, Vec3 p) {
  return {m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
          m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
          m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]};
}
RigMatrix multiply(const RigMatrix &a, const RigMatrix &b) {
  RigMatrix c{};
  for (int j = 0; j < 4; j++)
    for (int i = 0; i < 4; i++)
      for (int k = 0; k < 4; k++)
        c[j * 4 + i] += a[k * 4 + i] * b[j * 4 + k];
  return c;
}
RigMatrix matrix(JointTransform t) {
  float norm = t.rotation.w * t.rotation.w + t.rotation.x * t.rotation.x +
               t.rotation.y * t.rotation.y + t.rotation.z * t.rotation.z;
  if (!finite(t.translation) || !finite(t.scale) || !std::isfinite(norm) ||
      std::abs(norm - 1) > 1e-4f ||
      std::max({std::abs(t.translation.x), std::abs(t.translation.y),
                std::abs(t.translation.z)}) > 10000 ||
      std::min({std::abs(t.scale.x), std::abs(t.scale.y),
                std::abs(t.scale.z)}) < 1e-4f ||
      std::max({std::abs(t.scale.x), std::abs(t.scale.y),
                std::abs(t.scale.z)}) > 10000)
    throw std::invalid_argument("Invalid joint transform");
  auto m = t.rotation.matrix();
  for (int i = 0; i < 3; i++) {
    m[i] *= t.scale.x;
    m[4 + i] *= t.scale.y;
    m[8 + i] *= t.scale.z;
  }
  m[12] = t.translation.x;
  m[13] = t.translation.y;
  m[14] = t.translation.z;
  return m;
}
RigMatrix inverse(const RigMatrix &m) {
  double a = m[0], b = m[4], c = m[8], d = m[1], e = m[5], f = m[9], g = m[2],
         h = m[6], i = m[10];
  double det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
  double column_product = std::sqrt(a * a + d * d + g * g) *
                          std::sqrt(b * b + e * e + h * h) *
                          std::sqrt(c * c + f * f + i * i);
  if (!std::isfinite(det) || !std::isfinite(column_product) ||
      column_product == 0 || std::abs(det) <= column_product * 1e-12)
    throw std::runtime_error("Singular or ill-conditioned joint matrix");
  auto n = identity;
  n[0] = (e * i - f * h) / det;
  n[4] = (c * h - b * i) / det;
  n[8] = (b * f - c * e) / det;
  n[1] = (f * g - d * i) / det;
  n[5] = (a * i - c * g) / det;
  n[9] = (c * d - a * f) / det;
  n[2] = (d * h - e * g) / det;
  n[6] = (b * g - a * h) / det;
  n[10] = (a * e - b * d) / det;
  auto p = point(n, {m[12], m[13], m[14]});
  n[12] = -p.x;
  n[13] = -p.y;
  n[14] = -p.z;
  for (float v : n)
    if (!std::isfinite(v))
      throw std::runtime_error("Inverse joint matrix overflow");
  return n;
}
void validate_map(std::span<const uint32_t> map, size_t count,
                  const char *what) {
  if (map.size() != count)
    throw std::invalid_argument(std::string(what) + " map size mismatch");
  for (size_t i = 0; i < count; i++)
    if (map[i] >= count || map[map[i]] != i)
      throw std::invalid_argument(std::string(what) +
                                  " map is not an involution");
}
float segment_distance(Vec3 p, Vec3 a, Vec3 b) {
  auto d = b - a;
  float q = dot(d, d);
  return length(
      p -
      (a + d * (q > 1e-20f ? std::clamp(dot(p - a, d) / q, 0.f, 1.f) : 0.f)));
}
void normals(Mesh &m) {
  m.normals.assign(m.positions.size(), {});
  for (size_t t = 0; t < m.indices.size(); t += 3) {
    auto a = m.indices[t], b = m.indices[t + 1], c = m.indices[t + 2];
    auto n =
        cross(m.positions[b] - m.positions[a], m.positions[c] - m.positions[a]);
    m.normals[a] += n;
    m.normals[b] += n;
    m.normals[c] += n;
  }
  for (auto &n : m.normals)
    n = normalized(n);
}
// Closest point on a triangle, expressed in its convex barycentric regions.
std::array<float, 3> closest(Vec3 p, Vec3 a, Vec3 b, Vec3 c) {
  auto ab = b - a, ac = c - a, ap = p - a;
  float d1 = dot(ab, ap), d2 = dot(ac, ap);
  if (d1 <= 0 && d2 <= 0)
    return {1, 0, 0};
  auto bp = p - b;
  float d3 = dot(ab, bp), d4 = dot(ac, bp);
  if (d3 >= 0 && d4 <= d3)
    return {0, 1, 0};
  float vc = d1 * d4 - d3 * d2;
  if (vc <= 0 && d1 >= 0 && d3 <= 0) {
    float v = d1 / (d1 - d3);
    return {1 - v, v, 0};
  }
  auto cp = p - c;
  float d5 = dot(ab, cp), d6 = dot(ac, cp);
  if (d6 >= 0 && d5 <= d6)
    return {0, 0, 1};
  float vb = d5 * d2 - d1 * d6;
  if (vb <= 0 && d2 >= 0 && d6 <= 0) {
    float w = d2 / (d2 - d6);
    return {1 - w, 0, w};
  }
  float va = d3 * d6 - d5 * d4;
  if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) {
    float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
    return {0, 1 - w, w};
  }
  float inv = 1 / (va + vb + vc);
  float v = vb * inv, w = vc * inv;
  return {1 - v - w, v, w};
}
struct SurfaceIndex {
  const Mesh &mesh;
  const std::function<bool()> &cancelled;
  std::vector<uint32_t> triangles;
  struct Node {
    Vec3 lo, hi;
    uint32_t first = 0, count = 0, left = 0, right = 0;
  };
  std::vector<Node> nodes;
  explicit SurfaceIndex(const Mesh &m, const std::function<bool()> &c)
      : mesh(m), cancelled(c) {
    triangles.resize(m.indices.size() / 3);
    std::iota(triangles.begin(), triangles.end(), 0);
    nodes.reserve(triangles.size() * 2);
    build(0, triangles.size());
  }
  uint32_t build(uint32_t first, uint32_t count) {
    stop(cancelled);
    auto idx = uint32_t(nodes.size());
    nodes.emplace_back();
    Node n;
    n.first = first;
    n.count = count;
    float inf = std::numeric_limits<float>::infinity();
    n.lo = {inf, inf, inf};
    n.hi = {-inf, -inf, -inf};
    for (uint32_t k = first; k < first + count; k++)
      for (int v = 0; v < 3; v++) {
        auto p = mesh.positions[mesh.indices[triangles[k] * 3 + v]];
        n.lo = {std::min(n.lo.x, p.x), std::min(n.lo.y, p.y),
                std::min(n.lo.z, p.z)};
        n.hi = {std::max(n.hi.x, p.x), std::max(n.hi.y, p.y),
                std::max(n.hi.z, p.z)};
      }
    if (count > 8) {
      auto extent = n.hi - n.lo;
      int axis = extent.y > extent.x ? 1 : 0;
      if (extent.z > (axis ? extent.y : extent.x))
        axis = 2;
      auto coordinate = [&](uint32_t t) {
        Vec3 p{};
        for (int i = 0; i < 3; i++)
          p += mesh.positions[mesh.indices[t * 3 + i]];
        return axis == 0 ? p.x : axis == 1 ? p.y : p.z;
      };
      uint32_t half = count / 2;
      std::nth_element(triangles.begin() + first,
                       triangles.begin() + first + half,
                       triangles.begin() + first + count, [&](auto a, auto b) {
                         float x = coordinate(a), y = coordinate(b);
                         return x == y ? a < b : x < y;
                       });
      n.left = build(first, half);
      n.right = build(first + half, count - half);
      n.count = 0;
    }
    nodes[idx] = n;
    return idx;
  }
  float box(uint32_t i, Vec3 p) const {
    auto &n = nodes[i];
    return distance2(p, {std::clamp(p.x, n.lo.x, n.hi.x),
                         std::clamp(p.y, n.lo.y, n.hi.y),
                         std::clamp(p.z, n.lo.z, n.hi.z)});
  }
  void query(uint32_t index, Vec3 p, float &best, uint32_t &triangle,
             std::array<float, 3> &bary) const {
    auto &n = nodes[index];
    if (box(index, p) > best)
      return;
    if (n.count) {
      for (uint32_t k = n.first; k < n.first + n.count; k++) {
        auto t = triangles[k];
        auto a = mesh.positions[mesh.indices[t * 3]],
             b = mesh.positions[mesh.indices[t * 3 + 1]],
             c = mesh.positions[mesh.indices[t * 3 + 2]];
        auto w = closest(p, a, b, c);
        float d = distance2(p, a * w[0] + b * w[1] + c * w[2]);
        if (d < best || (d == best && t < triangle)) {
          best = d;
          triangle = t;
          bary = w;
        }
      }
    } else {
      auto a = n.left, b = n.right;
      if (box(a, p) > box(b, p))
        std::swap(a, b);
      query(a, p, best, triangle, bary);
      query(b, p, best, triangle, bary);
    }
  }
};
} // namespace
void Skeleton::validate() const {
  if (bones.empty() || bones.size() > 256)
    throw std::length_error("Skeleton must have1..256 bones");
  std::set<std::string> names;
  for (size_t i = 0; i < bones.size(); i++) {
    auto &b = bones[i];
    name_valid(b.name);
    if (!names.insert(b.name).second || b.parent < -1 ||
        b.parent >= int(bones.size()) || b.parent == int(i) ||
        !std::isfinite(b.length) || b.length <= 1e-6f || b.length > 1000)
      throw std::invalid_argument("Invalid bone hierarchy/name/length");
    matrix(b.rest);
    std::set<int> seen;
    for (int p = int(i); p >= 0; p = bones[p].parent) {
      if (p >= int(bones.size()) || p < -1 || !seen.insert(p).second)
        throw std::invalid_argument("Cyclic or invalid skeleton hierarchy");
    }
  }
}
std::vector<RigMatrix>
Skeleton::world(std::span<const JointTransform> local) const {
  validate();
  if (!local.empty() && local.size() != bones.size())
    throw std::invalid_argument("Pose joint count mismatch");
  std::vector<RigMatrix> out(bones.size());
  std::vector<bool> done(bones.size());
  std::function<void(size_t)> visit = [&](size_t i) {
    if (done[i])
      return;
    auto m = matrix(local.empty() ? bones[i].rest : local[i]);
    if (bones[i].parent >= 0) {
      visit(bones[i].parent);
      m = multiply(out[bones[i].parent], m);
    }
    for (float v : m)
      if (!std::isfinite(v) || std::abs(v) > 1e12f)
        throw std::runtime_error("Joint transform overflow");
    out[i] = m;
    done[i] = true;
  };
  for (size_t i = 0; i < bones.size(); i++)
    visit(i);
  return out;
}
std::vector<std::array<Vec3, 2>>
Skeleton::segments(std::span<const JointTransform> local) const {
  auto matrices = world(local);
  std::vector<std::array<Vec3, 2>> out;
  for (size_t i = 0; i < bones.size(); i++)
    out.push_back(
        {point(matrices[i], {}), point(matrices[i], {0, bones[i].length, 0})});
  return out;
}
std::vector<RigMatrix> Skeleton::inverse_bind() const {
  auto out = world();
  for (auto &m : out)
    m = inverse(m);
  return out;
}
std::vector<RigMatrix>
Skeleton::skin_matrices(std::span<const JointTransform> local) const {
  auto out = world(local), bind = inverse_bind();
  for (size_t i = 0; i < out.size(); i++)
    out[i] = multiply(out[i], bind[i]);
  return out;
}
nlohmann::json Skeleton::encode() const {
  validate();
  auto j = nlohmann::json{{"version", 1}, {"bones", nlohmann::json::array()}};
  for (auto &b : bones) {
    auto &t = b.rest;
    j["bones"].push_back(
        {{"name", b.name},
         {"parent", b.parent},
         {"translation", {t.translation.x, t.translation.y, t.translation.z}},
         {"rotation", {t.rotation.w, t.rotation.x, t.rotation.y, t.rotation.z}},
         {"scale", {t.scale.x, t.scale.y, t.scale.z}},
         {"length", b.length},
         {"deform", b.deform}});
  }
  return j;
}
Skeleton Skeleton::decode(const nlohmann::json &j) {
  if (j.at("version") != 1 || !j.at("bones").is_array() ||
      j.at("bones").empty() || j.at("bones").size() > 256)
    throw std::runtime_error("Unsupported/invalid skeleton record");
  Skeleton s;
  for (auto &v : j.at("bones")) {
    Bone b;
    b.name = v.at("name");
    auto &parent = v.at("parent");
    if (!parent.is_number_integer() ||
        (parent.is_number_unsigned() && parent.get<uint64_t>() > 255) ||
        parent.get<int64_t>() < -1 || parent.get<int64_t>() > 255)
      throw std::runtime_error("Invalid persisted bone parent");
    b.parent = parent.get<int>();
    auto p = v.at("translation").get<std::array<float, 3>>(),
         q = v.at("scale").get<std::array<float, 3>>();
    auto r = v.at("rotation").get<std::array<float, 4>>();
    b.rest = {{p[0], p[1], p[2]}, {r[0], r[1], r[2], r[3]}, {q[0], q[1], q[2]}};
    b.length = v.at("length");
    b.deform = v.at("deform");
    s.bones.push_back(std::move(b));
  }
  s.validate();
  return s;
}
VertexWeights normalize_weights(VertexWeights input, size_t bones,
                                std::span<const uint8_t> locks,
                                WeightLimits limits) {
  if (!bones || bones > 256 || !limits.max_influences ||
      limits.max_influences > 32 || !std::isfinite(limits.prune) ||
      limits.prune < 0 || limits.prune >= 1 ||
      (!locks.empty() && locks.size() != bones))
    throw std::invalid_argument("Invalid weight normalization constraints");
  if (input.size() > bones)
    throw std::invalid_argument("Too many vertex influences");
  std::set<uint32_t> ids;
  VertexWeights fixed, free;
  double sum = 0;
  for (auto i : input) {
    if (i.joint >= bones || !std::isfinite(i.weight) || i.weight < 0 ||
        i.weight > 1 || !ids.insert(i.joint).second)
      throw std::invalid_argument("Invalid/duplicate joint influence");
    bool locked = !locks.empty() && locks[i.joint];
    if (locked) {
      if (i.weight > 0) {
        fixed.push_back(i);
        sum += i.weight;
      }
    } else if (i.weight > limits.prune)
      free.push_back(i);
  }
  if (sum > 1.000001 || fixed.size() > limits.max_influences)
    throw std::invalid_argument(
        "Locked weights cannot satisfy normalization/influence cap");
  std::sort(free.begin(), free.end(), [](auto a, auto b) {
    return a.weight == b.weight ? a.joint < b.joint : a.weight > b.weight;
  });
  free.resize(
      std::min(free.size(), size_t(limits.max_influences - fixed.size())));
  double available = std::max(0.0, 1 - sum), total = 0;
  for (auto i : free)
    total += i.weight;
  if (available > 1e-6 && total <= 0)
    throw std::invalid_argument(
        "No unlocked weight can absorb remaining normalization mass");
  if (available > 1e-6)
    for (auto i : free) {
      i.weight = float(i.weight / total * available);
      fixed.push_back(i);
    }
  std::sort(fixed.begin(), fixed.end(),
            [](auto a, auto b) { return a.joint < b.joint; });
  return fixed;
}
VertexWeights paint_weight(VertexWeights input, uint32_t joint, float value,
                           size_t bones, std::span<const uint8_t> locks,
                           WeightLimits limits) {
  if (joint >= bones || !std::isfinite(value) || value < 0 || value > 1 ||
      (!locks.empty() && locks.size() != bones))
    throw std::invalid_argument("Invalid painted joint/value");
  if (!locks.empty() && locks[joint])
    throw std::logic_error("Cannot paint a hard-locked influence");
  // First validate the original assignment without pruning away locks.
  SkinWeights original;
  original.vertices = {input};
  original.validate(1, bones);
  std::vector<uint8_t> constraints(bones, 0);
  if (!locks.empty())
    std::copy(locks.begin(), locks.end(), constraints.begin());
  constraints[joint] = 1;
  auto it = std::find_if(input.begin(), input.end(),
                         [&](auto i) { return i.joint == joint; });
  if (it == input.end())
    input.push_back({joint, value});
  else
    it->weight = value;
  return normalize_weights(std::move(input), bones, constraints, limits);
}
void SkinWeights::validate(size_t count, size_t bones) const {
  if (!count || count > 2'000'000 || !bones || bones > 256 ||
      vertices.size() != count)
    throw std::invalid_argument("Skin dimensions/resource limit invalid");
  for (auto &w : vertices) {
    if (w.empty() || w.size() > 32)
      throw std::invalid_argument("Unweighted/overfull skin vertex");
    std::set<uint32_t> ids;
    double sum = 0;
    for (auto i : w) {
      if (i.joint >= bones || !std::isfinite(i.weight) || i.weight <= 0 ||
          i.weight > 1 || !ids.insert(i.joint).second)
        throw std::invalid_argument("Invalid skin influence");
      sum += i.weight;
    }
    if (std::abs(sum - 1) > 1e-4)
      throw std::invalid_argument("Skin weights are not normalized");
  }
}
nlohmann::json SkinWeights::encode() const {
  validate(vertices.size(), 256);
  auto j =
      nlohmann::json{{"version", 1}, {"vertices", nlohmann::json::array()}};
  for (auto &w : vertices) {
    auto row = nlohmann::json::array();
    for (auto i : w)
      row.push_back({i.joint, i.weight});
    j["vertices"].push_back(std::move(row));
  }
  return j;
}
SkinWeights SkinWeights::decode(const nlohmann::json &j, size_t count,
                                size_t bones) {
  if (j.at("version") != 1 || !j.at("vertices").is_array() ||
      j.at("vertices").size() != count || count > 2'000'000)
    throw std::runtime_error("Invalid skin record");
  SkinWeights s;
  for (auto &row : j.at("vertices")) {
    if (!row.is_array() || row.empty() || row.size() > 32)
      throw std::runtime_error("Invalid skin influence count");
    VertexWeights w;
    for (auto &v : row) {
      if (!v.is_array() || v.size() != 2 || !v[0].is_number_integer() ||
          v[0].get<int64_t>() < 0 || v[0].get<uint64_t>() >= bones)
        throw std::runtime_error("Invalid persisted skin joint");
      w.push_back({v[0].get<uint32_t>(), v[1].get<float>()});
    }
    s.vertices.push_back(std::move(w));
  }
  s.validate(count, bones);
  return s;
}
SkinWeights automatic_bind(const Mesh &m, const Skeleton &s,
                           BindOptions options,
                           const std::function<bool()> &cancelled) {
  mesh_valid(m);
  s.validate();
  stop(cancelled);
  if (!std::isfinite(options.maximum_seed_distance) ||
      options.maximum_seed_distance <= 0 ||
      options.maximum_seed_distance > 10000 ||
      !std::isfinite(options.seed_margin) || options.seed_margin < 0 ||
      options.seed_margin > options.maximum_seed_distance ||
      !std::isfinite(options.falloff) || options.falloff < 1 ||
      options.falloff > 8)
    throw std::invalid_argument("Invalid binding distances/falloff");
  normalize_weights({{0, 1}}, s.bones.size(), {}, options.limits);
  size_t n = m.positions.size(), work = 0;
  auto consume = [&] {
    if (++work > options.maximum_work)
      throw std::length_error("Automatic binding work budget exceeded");
    if ((work & 1023) == 0)
      stop(cancelled);
  };
  std::map<std::array<float, 3>, uint32_t> welded;
  std::vector<Vec3> p;
  std::vector<uint32_t> remap(n);
  for (size_t v = 0; v < n; v++) {
    if ((v & 1023) == 0)
      stop(cancelled);
    auto q = m.positions[v];
    auto [it, added] = welded.emplace(std::array{q.x, q.y, q.z}, p.size());
    if (added)
      p.push_back(q);
    remap[v] = it->second;
  }
  std::vector<std::vector<std::pair<uint32_t, float>>> edges(p.size());
  for (size_t t = 0; t < m.indices.size(); t += 3)
    for (int k = 0; k < 3; k++) {
      auto a = remap[m.indices[t + k]], b = remap[m.indices[t + (k + 1) % 3]];
      float d = length(p[a] - p[b]);
      edges[a].push_back({b, d});
      edges[b].push_back({a, d});
    }
  for (auto &e : edges) {
    std::sort(e.begin(), e.end());
    e.erase(std::unique(e.begin(), e.end()), e.end());
  }
  std::vector<int> component(p.size(), -1);
  std::vector<std::vector<uint32_t>> components;
  for (uint32_t v = 0; v < p.size(); v++)
    if (component[v] < 0) {
      int id = components.size();
      components.emplace_back();
      std::vector<uint32_t> todo{v};
      component[v] = id;
      while (!todo.empty()) {
        auto a = todo.back();
        todo.pop_back();
        components.back().push_back(a);
        for (auto [b, d] : edges[a])
          if (component[b] < 0) {
            component[b] = id;
            todo.push_back(b);
          }
      }
    }
  auto worlds = s.world();
  std::vector<VertexWeights> candidate(p.size());
  std::vector<bool> seeded(components.size());
  float scale = 1e-5f;
  for (size_t b = 0; b < s.bones.size(); b++)
    if (s.bones[b].deform) {
      auto a = point(worlds[b], {}),
           end = point(worlds[b], {0, s.bones[b].length, 0});
      scale = std::max(scale, length(end - a) * .02f);
      std::vector<float> direct(p.size()),
          best(components.size(), std::numeric_limits<float>::infinity());
      for (size_t v = 0; v < p.size(); v++) {
        consume();
        direct[v] = segment_distance(p[v], a, end);
        best[component[v]] = std::min(best[component[v]], direct[v]);
      }
      std::vector<float> dist(p.size(), std::numeric_limits<float>::infinity());
      using Item = std::pair<float, uint32_t>;
      std::priority_queue<Item, std::vector<Item>, std::greater<Item>> queue;
      for (size_t c = 0; c < components.size(); c++)
        if (best[c] <= options.maximum_seed_distance) {
          seeded[c] = true;
          for (auto v : components[c])
            if (direct[v] <= best[c] + options.seed_margin) {
              dist[v] = direct[v];
              queue.push({dist[v], v});
            }
        }
      while (!queue.empty()) {
        auto [d, v] = queue.top();
        queue.pop();
        if (d != dist[v])
          continue;
        consume();
        for (auto [u, cost] : edges[v])
          if (d + cost < dist[u]) {
            dist[u] = d + cost;
            queue.push({dist[u], u});
          }
      }
      for (size_t v = 0; v < p.size(); v++)
        if (std::isfinite(dist[v])) {
          auto &row = candidate[v];
          Influence next{uint32_t(b), dist[v]};
          if (row.size() < options.limits.max_influences)
            row.push_back(next);
          else {
            auto worst =
                std::max_element(row.begin(), row.end(), [](auto a, auto b) {
                  return a.weight == b.weight ? a.joint < b.joint
                                              : a.weight < b.weight;
                });
            if (next.weight < worst->weight)
              *worst = next;
          }
        }
    }
  for (size_t c = 0; c < seeded.size(); c++)
    if (!seeded[c])
      throw std::runtime_error(
          "No plausible deform-bone seed for surface component " +
          std::to_string(c));
  SkinWeights out;
  out.vertices.reserve(n);
  for (auto v : remap) {
    consume();
    auto w = candidate[v];
    float nearest = std::numeric_limits<float>::infinity();
    for (auto i : w)
      nearest = std::min(nearest, i.weight);
    for (auto &i : w)
      i.weight =
          std::pow((nearest + scale) / (i.weight + scale), options.falloff);
    out.vertices.push_back(
        normalize_weights(std::move(w), s.bones.size(), {}, options.limits));
  }
  out.validate(n, s.bones.size());
  stop(cancelled);
  return out;
}
SkinWeights transfer_weights(const Mesh &source, const SkinWeights &w,
                             const Mesh &dest, size_t bones, float maximum,
                             WeightLimits limits,
                             const std::function<bool()> &cancelled) {
  mesh_valid(source);
  mesh_valid(dest);
  w.validate(source.positions.size(), bones);
  if (!std::isfinite(maximum) || maximum <= 0 || maximum > 10000)
    throw std::invalid_argument("Invalid weight transfer radius");
  stop(cancelled);
  SurfaceIndex index(source, cancelled);
  SkinWeights out;
  out.vertices.reserve(dest.positions.size());
  for (size_t v = 0; v < dest.positions.size(); v++) {
    if ((v & 255) == 0)
      stop(cancelled);
    float best = maximum * maximum;
    uint32_t t = UINT32_MAX;
    std::array<float, 3> bary{};
    index.query(0, dest.positions[v], best, t, bary);
    if (t == UINT32_MAX)
      throw std::runtime_error(
          "No source triangle within weight transfer radius at vertex " +
          std::to_string(v));
    std::map<uint32_t, float> sum;
    for (int k = 0; k < 3; k++)
      for (auto i : w.vertices[source.indices[t * 3 + k]])
        sum[i.joint] += i.weight * bary[k];
    VertexWeights row;
    for (auto [joint, value] : sum)
      row.push_back({joint, value});
    out.vertices.push_back(
        normalize_weights(std::move(row), bones, {}, limits));
  }
  out.validate(dest.positions.size(), bones);
  stop(cancelled);
  return out;
}
std::vector<uint32_t> mirror_vertices(const Mesh &m, float tolerance,
                                      const std::function<bool()> &cancelled) {
  mesh_valid(m);
  if (!std::isfinite(tolerance) || tolerance <= 0 || tolerance > 1)
    throw std::invalid_argument("Invalid symmetry tolerance");
  using Key = std::array<int64_t, 3>;
  std::map<Key, std::vector<uint32_t>> bins;
  auto cell = [&](Vec3 p) {
    return Key{int64_t(std::floor(p.x / tolerance)),
               int64_t(std::floor(p.y / tolerance)),
               int64_t(std::floor(p.z / tolerance))};
  };
  for (uint32_t v = 0; v < m.positions.size(); v++) {
    auto p = m.positions[v];
    if (std::max({std::abs(p.x), std::abs(p.y), std::abs(p.z)}) / tolerance >
        double(INT64_MAX) / 2)
      throw std::invalid_argument("Symmetry coordinates overflow");
    bins[cell(p)].push_back(v);
  }
  std::vector<uint32_t> map(m.positions.size(), UINT32_MAX);
  for (uint32_t v = 0; v < m.positions.size(); v++) {
    if ((v & 255) == 0)
      stop(cancelled);
    if (map[v] != UINT32_MAX)
      continue;
    auto p = m.positions[v];
    p.x = -p.x;
    auto key = cell(p);
    uint32_t chosen = UINT32_MAX;
    float best = std::numeric_limits<float>::infinity();
    auto normal = m.normals[v];
    normal.x = -normal.x;
    if (distance2(p, m.positions[v]) <= tolerance * tolerance) {
      map[v] = v;
      continue;
    }
    for (int x = -1; x <= 1; x++)
      for (int y = -1; y <= 1; y++)
        for (int z = -1; z <= 1; z++) {
          auto found = bins.find({key[0] + x, key[1] + y, key[2] + z});
          if (found == bins.end())
            continue;
          for (auto u : found->second)
            if (map[u] == UINT32_MAX &&
                distance2(p, m.positions[u]) <= tolerance * tolerance) {
              float score = distance2(normal, m.normals[u]);
              if (score < best || (score == best && u < chosen)) {
                chosen = u;
                best = score;
              }
            }
        }
    if (chosen == UINT32_MAX)
      throw std::runtime_error(
          "No bijective symmetric counterpart for vertex " + std::to_string(v));
    map[v] = chosen;
    map[chosen] = v;
  }
  validate_map(map, m.positions.size(), "Vertex symmetry");
  return map;
}
SkinWeights mirror_weights(const Mesh &m, const SkinWeights &w,
                           std::span<const uint32_t> bones, float tolerance) {
  w.validate(m.positions.size(), bones.size());
  validate_map(bones, bones.size(), "Bone symmetry");
  auto vertices = mirror_vertices(m, tolerance);
  SkinWeights out;
  out.vertices.resize(w.vertices.size());
  for (size_t v = 0; v < vertices.size(); v++) {
    auto row = w.vertices[vertices[v]];
    for (auto &i : row)
      i.joint = bones[i.joint];
    std::sort(row.begin(), row.end(),
              [](auto a, auto b) { return a.joint < b.joint; });
    out.vertices[v] = std::move(row);
  }
  return out;
}
Mesh deform_mesh(const Mesh &m, const SkinWeights &w, const Skeleton &s,
                 std::span<const JointTransform> local) {
  mesh_valid(m);
  w.validate(m.positions.size(), s.bones.size());
  auto matrices = s.skin_matrices(local);
  Mesh out = m;
  for (size_t v = 0; v < m.positions.size(); v++) {
    Vec3 p{};
    for (auto i : w.vertices[v])
      p += point(matrices[i.joint], m.positions[v]) * i.weight;
    out.positions[v] = p;
  }
  normals(out);
  out.validate();
  return out;
}
uint64_t morph_topology_id(const Mesh &m) {
  mesh_valid(m);
  uint64_t hash = 1469598103934665603ULL;
  auto word = [&](uint32_t n) {
    for (int i = 0; i < 4; i++) {
      hash ^= uint8_t(n >> (i * 8));
      hash *= 1099511628211ULL;
    }
  };
  word(m.positions.size());
  word(m.indices.size());
  for (auto p : m.positions)
    for (float v : {p.x, p.y, p.z})
      word(std::bit_cast<uint32_t>(v));
  for (auto i : m.indices)
    word(i);
  return hash;
}
MorphSet::MorphSet(const Mesh &m) : topology(morph_topology_id(m)) {}
void MorphSet::validate(const Mesh &m) const {
  if (topology != morph_topology_id(m) || targets.size() > 128)
    throw std::invalid_argument("Morph topology identity/count mismatch");
  std::set<std::string> names;
  for (auto &t : targets) {
    name_valid(t.name);
    if (!names.insert(t.name).second || t.delta.size() != m.positions.size())
      throw std::invalid_argument("Invalid morph target identity/dimensions");
    for (auto d : t.delta)
      if (!finite(d) ||
          std::max({std::abs(d.x), std::abs(d.y), std::abs(d.z)}) > 1000)
        throw std::invalid_argument("Invalid morph displacement");
  }
  if (m.positions.size() * targets.size() > 8'000'000)
    throw std::length_error("Morph displacement budget exceeded");
}
void MorphSet::set_target(const Mesh &m, std::string name,
                          std::span<const Vec3> edited) {
  validate(m);
  name_valid(name);
  if (edited.size() != m.positions.size())
    throw std::invalid_argument("Morph vertex count mismatch");
  MorphSet next = *this;
  MorphTarget target;
  target.name = std::move(name);
  for (size_t i = 0; i < edited.size(); i++)
    target.delta.push_back(edited[i] - m.positions[i]);
  auto it = std::find_if(next.targets.begin(), next.targets.end(),
                         [&](auto &t) { return t.name == target.name; });
  if (it == next.targets.end())
    next.targets.push_back(std::move(target));
  else
    *it = std::move(target);
  next.validate(m);
  *this = std::move(next);
}
void MorphSet::mirror_target(const Mesh &m, size_t index, std::string name,
                             float tolerance) {
  validate(m);
  if (index >= targets.size())
    throw std::out_of_range("Unknown morph target");
  auto map = mirror_vertices(m, tolerance);
  std::vector<Vec3> edited;
  for (size_t v = 0; v < map.size(); v++) {
    auto d = targets[index].delta[map[v]];
    d.x = -d.x;
    edited.push_back(m.positions[v] + d);
  }
  set_target(m, std::move(name), edited);
}
Mesh MorphSet::evaluate(const Mesh &m, std::span<const float> weights) const {
  validate(m);
  if (weights.size() != targets.size())
    throw std::invalid_argument("Morph value count mismatch");
  Mesh out = m;
  for (size_t t = 0; t < targets.size(); t++) {
    if (!std::isfinite(weights[t]) || std::abs(weights[t]) > 10)
      throw std::invalid_argument("Invalid morph weight");
    for (size_t v = 0; v < m.positions.size(); v++)
      out.positions[v] += targets[t].delta[v] * weights[t];
  }
  normals(out);
  out.validate();
  return out;
}
nlohmann::json MorphSet::encode() const {
  nlohmann::json j = {{"version", 1},
                      {"topology", topology},
                      {"targets", nlohmann::json::array()}};
  for (auto &t : targets) {
    auto delta = nlohmann::json::array();
    for (auto d : t.delta)
      delta.push_back({d.x, d.y, d.z});
    j["targets"].push_back({{"name", t.name}, {"delta", std::move(delta)}});
  }
  return j;
}
MorphSet MorphSet::decode(const nlohmann::json &j, const Mesh &m) {
  if (j.at("version") != 1 || !j.at("targets").is_array() ||
      j.at("targets").size() > 128)
    throw std::runtime_error("Invalid morph record");
  MorphSet set(m);
  if (!j.at("topology").is_number_unsigned() ||
      j.at("topology").get<uint64_t>() != set.topology)
    throw std::runtime_error("Persisted morph topology mismatch");
  for (auto &v : j.at("targets")) {
    MorphTarget target;
    target.name = v.at("name");
    auto &data = v.at("delta");
    if (!data.is_array() || data.size() != m.positions.size() ||
        (set.targets.size() + 1) * data.size() > 8'000'000)
      throw std::length_error("Invalid persisted morph dimensions");
    for (auto &p : data) {
      auto d = p.get<std::array<float, 3>>();
      target.delta.push_back({d[0], d[1], d[2]});
    }
    set.targets.push_back(std::move(target));
  }
  set.validate(m);
  return set;
}
} // namespace dome
