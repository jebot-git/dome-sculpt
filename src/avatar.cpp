// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/avatar.hpp"
#include <limits>
#include <nlohmann/json.hpp>
#include <set>
namespace dome {
namespace {
[[noreturn]] void fail(const std::string &s) {
  throw std::runtime_error("avatar: " + s);
}
void name_ok(const std::string &s) {
  if (s.empty() || s.size() > 128 || s.find('\0') != std::string::npos)
    fail("invalid identity name");
}
const HumanRole &role(const std::string &s) {
  for (const auto &r : human_roles())
    if (r.name == s)
      return r;
  fail("unknown humanoid role: " + s);
}
std::string parent(const HumanRole &r, const auto &present) {
  std::string p = r.parent;
  if (r.needs_parent && !present.contains(p))
    fail(r.name + " requires " + p);
  while (!p.empty() && !present.contains(p))
    p = role(p).parent;
  return p;
}
void version(const nlohmann::json &j) {
  if (!j.is_object() || !j.contains("version") || j.at("version") != 1)
    fail("unsupported metadata version");
}
Vec3 read_vec(const nlohmann::json &j) {
  if (!j.is_array() || j.size() != 3)
    fail("expected three-coordinate landmark");
  Vec3 v{j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>()};
  if (!finite(v) ||
      std::max({std::abs(v.x), std::abs(v.y), std::abs(v.z)}) > 10000)
    fail("nonfinite or out-of-bounds landmark");
  return v;
}
nlohmann::json vec(Vec3 p) { return {p.x, p.y, p.z}; }
// Ericson's triangle Voronoi regions; degenerate triangles reduce to edges.
float distance_triangle(Vec3 p, Vec3 a, Vec3 b, Vec3 c) {
  auto edge = [&](Vec3 x, Vec3 y) {
    Vec3 d = y - x;
    float n = dot(d, d);
    return length(
        p - (x + d * (n > 0 ? std::clamp(dot(p - x, d) / n, 0.f, 1.f) : 0)));
  };
  Vec3 ab = b - a, ac = c - a, ap = p - a;
  if (length(cross(ab, ac)) < 1e-12f)
    return std::min({edge(a, b), edge(b, c), edge(c, a)});
  float d1 = dot(ab, ap), d2 = dot(ac, ap);
  if (d1 <= 0 && d2 <= 0)
    return length(ap);
  Vec3 bp = p - b;
  float d3 = dot(ab, bp), d4 = dot(ac, bp);
  if (d3 >= 0 && d4 <= d3)
    return length(bp);
  float vc = d1 * d4 - d3 * d2;
  if (vc <= 0 && d1 >= 0 && d3 <= 0)
    return length(p - (a + ab * (d1 / (d1 - d3))));
  Vec3 cp = p - c;
  float d5 = dot(ab, cp), d6 = dot(ac, cp);
  if (d6 >= 0 && d5 <= d6)
    return length(cp);
  float vb = d5 * d2 - d1 * d6;
  if (vb <= 0 && d2 >= 0 && d6 <= 0)
    return length(p - (a + ac * (d2 / (d2 - d6))));
  float va = d3 * d6 - d5 * d4;
  if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0)
    return edge(b, c);
  float sum = va + vb + vc;
  return length(p - (a + ab * (vb / sum) + ac * (vc / sum)));
}
} // namespace
const std::vector<HumanRole> &human_roles() {
  static const auto roles = [] {
    std::vector<HumanRole> r{
        {"hips", "", true},     {"spine", "hips", true},
        {"chest", "spine"},     {"upperChest", "chest", false, true},
        {"neck", "upperChest"}, {"head", "neck", true},
        {"leftEye", "head"},    {"rightEye", "head"},
        {"jaw", "head"}};
    for (std::string side : {"left", "right"}) {
      r.push_back({side + "UpperLeg", "hips", true});
      r.push_back({side + "LowerLeg", side + "UpperLeg", true});
      r.push_back({side + "Foot", side + "LowerLeg", true});
      r.push_back({side + "Toes", side + "Foot"});
      r.push_back({side + "Shoulder", "upperChest"});
      r.push_back({side + "UpperArm", side + "Shoulder", true});
      r.push_back({side + "LowerArm", side + "UpperArm", true});
      r.push_back({side + "Hand", side + "LowerArm", true});
      r.push_back({side + "ThumbMetacarpal", side + "Hand"});
      r.push_back(
          {side + "ThumbProximal", side + "ThumbMetacarpal", false, true});
      r.push_back({side + "ThumbDistal", side + "ThumbProximal", false, true});
      for (std::string finger : {"Index", "Middle", "Ring", "Little"}) {
        r.push_back({side + finger + "Proximal", side + "Hand"});
        r.push_back({side + finger + "Intermediate", side + finger + "Proximal",
                     false, true});
        r.push_back({side + finger + "Distal", side + finger + "Intermediate",
                     false, true});
      }
    }
    return r;
  }();
  return roles;
}
void HumanoidMapping::validate(const Skeleton &s, bool complete) const {
  s.validate();
  if (joints.size() > 55)
    fail("too many humanoid mappings");
  std::map<uint32_t, std::string> reverse;
  for (const auto &[n, i] : joints) {
    role(n);
    if (i >= s.bones.size())
      fail("joint outside skeleton: " + n);
    if (!reverse.emplace(i, n).second)
      fail("joint assigned multiple roles: " + n);
    for (int a = int(i); a >= 0; a = s.bones[a].parent) {
      Vec3 v = s.bones[a].rest.scale;
      if (v.x <= 0 || v.y <= 0 || v.z <= 0)
        fail("non-positive humanoid ancestor scale: " + n);
    }
  }
  for (const auto &r : human_roles()) {
    if (complete && r.required && !joints.contains(r.name))
      fail("missing required role: " + r.name);
    auto it = joints.find(r.name);
    if (it == joints.end())
      continue;
    std::string expected = parent(r, joints), actual;
    for (int a = s.bones[it->second].parent; a >= 0; a = s.bones[a].parent)
      if (auto x = reverse.find(uint32_t(a)); x != reverse.end()) {
        actual = x->second;
        break;
      }
    if (actual != expected)
      fail("incorrect humanoid ancestor for " + r.name + ": expected " +
           expected + ", got " + actual);
  }
}
std::vector<uint32_t> HumanoidMapping::mirror(const Skeleton &s) const {
  validate(s, false);
  std::vector<uint32_t> out(s.bones.size());
  for (uint32_t i = 0; i < out.size(); ++i)
    out[i] = i;
  for (const auto &[n, i] : joints) {
    std::string other;
    if (n.starts_with("left"))
      other = "right" + n.substr(4);
    if (n.starts_with("right"))
      other = "left" + n.substr(5);
    if (!other.empty()) {
      auto j = joints.find(other);
      if (j == joints.end())
        fail("missing mirrored role: " + other);
      out[i] = j->second;
    }
  }
  return out;
}
nlohmann::json HumanoidMapping::encode() const {
  return {{"version", 1}, {"joints", joints}};
}
HumanoidMapping HumanoidMapping::decode(const nlohmann::json &j,
                                        const Skeleton &s, bool c) {
  version(j);
  if (!j.at("joints").is_object() || j.at("joints").size() > 55)
    fail("invalid mapping object");
  HumanoidMapping m;
  for (auto it = j.at("joints").begin(); it != j.at("joints").end(); ++it) {
    if (!it.value().is_number_unsigned() && !it.value().is_number_integer())
      fail("joint index must be integral");
    auto index = it.value().get<int64_t>();
    if (index < 0 || index >= int64_t(s.bones.size()))
      fail("invalid joint index");
    m.joints[it.key()] = uint32_t(index);
  }
  m.validate(s, c);
  return m;
}
nlohmann::json HumanLandmarks::encode() const {
  nlohmann::json p = nlohmann::json::object();
  for (const auto &[n, v] : points)
    p[n] = vec(v);
  return {{"version", 1},
          {"points", p},
          {"left", vec(left)},
          {"up", vec(up)},
          {"front", vec(front)}};
}
HumanLandmarks HumanLandmarks::decode(const nlohmann::json &j) {
  version(j);
  HumanLandmarks l;
  const auto &p = j.at("points");
  if (!p.is_object() || p.size() > 55)
    fail("invalid landmark count");
  for (auto it = p.begin(); it != p.end(); ++it) {
    role(it.key());
    l.points[it.key()] = read_vec(it.value());
  }
  l.left = read_vec(j.at("left"));
  l.up = read_vec(j.at("up"));
  l.front = read_vec(j.at("front"));
  return l;
}
HumanFit fit_humanoid(const Mesh &mesh, const HumanLandmarks &l,
                      HumanFitOptions o, const std::function<bool()> &cancel) {
  mesh.validate();
  if (mesh.positions.empty() || mesh.indices.empty() ||
      mesh.positions.size() > 2'000'000 || mesh.indices.size() > 6'000'000)
    fail("empty or excessive fit surface");
  if (l.points.size() > 55 || !std::isfinite(o.maximum_surface_distance) ||
      o.maximum_surface_distance <= 0 || o.maximum_surface_distance > 10 ||
      !std::isfinite(o.minimum_segment_length) ||
      o.minimum_segment_length <= 0 || !std::isfinite(o.terminal_length) ||
      o.terminal_length < o.minimum_segment_length || o.terminal_length > 10)
    fail("invalid fit options or landmark count");
  for (Vec3 axis : {l.left, l.up, l.front})
    if (!finite(axis) || std::abs(length(axis) - 1) > 1e-4f)
      fail("body frame axes must be unit vectors");
  if (length(cross(l.left, l.up) - l.front) > 1e-4f)
    fail("body frame must be right-handed orthonormal");
  for (const auto &r : human_roles())
    if (r.required && !l.points.contains(r.name))
      fail("missing landmark: " + r.name);
  if (l.points.size() > o.maximum_work / (mesh.indices.size() / 3))
    fail("landmark fit exceeds work budget");
  for (auto p : mesh.positions)
    if (!finite(p) ||
        std::max({std::abs(p.x), std::abs(p.y), std::abs(p.z)}) > 10000)
      fail("surface outside fit bounds");
  for (const auto &[n, p] : l.points) {
    role(n);
    read_vec(vec(p));
    parent(role(n), l.points);
    float best = std::numeric_limits<float>::infinity();
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
      if (i % 3072 == 0 && cancel && cancel())
        fail("fit cancelled");
      best = std::min(best,
                      distance_triangle(p, mesh.positions[mesh.indices[i]],
                                        mesh.positions[mesh.indices[i + 1]],
                                        mesh.positions[mesh.indices[i + 2]]));
    }
    if (best > o.maximum_surface_distance)
      fail("landmark too far from surface: " + n);
  }
  Vec3 hips = l.points.at("hips");
  for (std::string side : {"left", "right"}) {
    float sign = side == "left" ? 1 : -1;
    for (std::string limb :
         {"UpperLeg", "LowerLeg", "Foot", "UpperArm", "LowerArm", "Hand"})
      if (sign * dot(l.points.at(side + limb) - hips, l.left) <=
          o.minimum_segment_length)
        fail("crossed or ambiguous body-side landmark: " + side + limb);
    if (dot(l.points.at(side + "UpperLeg") - l.points.at(side + "LowerLeg"),
            l.up) <= o.minimum_segment_length ||
        dot(l.points.at(side + "LowerLeg") - l.points.at(side + "Foot"),
            l.up) <= o.minimum_segment_length)
      fail("legs must be in an upright authoring pose: " + side);
  }
  if (dot(l.points.at("spine") - hips, l.up) <= o.minimum_segment_length ||
      dot(l.points.at("head") - l.points.at("spine"), l.up) <=
          o.minimum_segment_length)
    fail("torso must be in an upright authoring pose");
  HumanFit out;
  std::vector<Quat> world_rot;
  for (const auto &r : human_roles()) {
    auto it = l.points.find(r.name);
    if (it == l.points.end())
      continue;
    std::string p = parent(r, l.points);
    int pi = p.empty() ? -1 : int(out.mapping.joints.at(p));
    Vec3 position = it->second, tail = position + l.up * o.terminal_length;
    // Prefer the main anatomical continuation, never a side branch.
    std::string continuation;
    if (r.name == "hips")
      continuation = "spine";
    else if (r.name == "spine" || r.name == "chest" || r.name == "upperChest" ||
             r.name == "neck") {
      for (std::string candidate : {"chest", "upperChest", "neck", "head"})
        if (l.points.contains(candidate) &&
            parent(role(candidate), l.points) == r.name) {
          continuation = candidate;
          break;
        }
    } else if (r.name != "head" && !r.name.ends_with("Hand") &&
               !r.name.ends_with("Foot"))
      for (const auto &child : human_roles())
        if (l.points.contains(child.name) &&
            parent(child, l.points) == r.name) {
          continuation = child.name;
          break;
        }
    if (!continuation.empty())
      tail = l.points.at(continuation);
    else
      out.notes.push_back(r.name +
                          ": terminal length uses explicit fit option");
    float len = length(tail - position);
    if (len < o.minimum_segment_length)
      fail("collapsed segment: " + r.name);
    if (pi >= 0 && length(position - l.points.at(p)) < o.minimum_segment_length)
      fail("coincident parent landmark: " + r.name);
    Quat q = arcball({0, 1, 0}, normalized(tail - position));
    Bone b;
    b.name = r.name;
    b.parent = pi;
    b.length = len;
    b.rest.translation =
        pi < 0 ? position
               : world_rot[pi].inverse().rotate(position - l.points.at(p));
    b.rest.rotation = pi < 0 ? q : world_rot[pi].inverse() * q;
    out.mapping.joints[r.name] = uint32_t(out.skeleton.bones.size());
    out.skeleton.bones.push_back(b);
    world_rot.push_back(q);
  }
  out.mapping.validate(out.skeleton);
  return out;
}
AvatarCompaction compact_avatar(const Skeleton &s, const SkinWeights &w,
                                const HumanoidMapping &m) {
  m.validate(s, false);
  w.validate(w.vertices.size(), s.bones.size());
  std::vector<uint8_t> keep(s.bones.size());
  auto mark = [&](uint32_t i) {
    for (int p = int(i); p >= 0 && !keep[p]; p = s.bones[p].parent)
      keep[p] = 1;
  };
  for (const auto &[n, i] : m.joints) {
    (void)n;
    mark(i);
  }
  for (const auto &v : w.vertices)
    for (auto i : v)
      if (i.weight > 0)
        mark(i.joint);
  if (std::none_of(keep.begin(), keep.end(), [](auto k) { return k != 0; }))
    fail("no retained avatar joints");
  AvatarCompaction out;
  out.old_to_new.assign(s.bones.size(), UINT32_MAX);
  for (size_t i = 0; i < keep.size(); ++i)
    if (keep[i]) {
      out.old_to_new[i] = uint32_t(out.skeleton.bones.size());
      out.skeleton.bones.push_back(s.bones[i]);
    }
  for (auto &b : out.skeleton.bones)
    if (b.parent >= 0)
      b.parent = int(out.old_to_new[b.parent]);
  out.weights = w;
  for (auto &v : out.weights.vertices)
    for (auto &i : v)
      i.joint = out.old_to_new[i.joint];
  for (const auto &[n, i] : m.joints)
    out.mapping.joints[n] = out.old_to_new[i];
  out.mapping.validate(out.skeleton, false);
  out.weights.validate(w.vertices.size(), out.skeleton.bones.size());
  return out;
}
void AvatarExpressions::validate(const MorphSet &m, bool all) const {
  static const std::set<std::string> vowels{"aa", "ih", "ou", "ee", "oh"};
  if (visemes.size() > 5 || m.targets.size() > 128)
    fail("excessive expression metadata");
  std::set<std::string> shapes;
  for (const auto &t : m.targets) {
    name_ok(t.name);
    if (!shapes.insert(t.name).second)
      fail("duplicate shape identity");
  }
  for (const auto &[n, bindings] : visemes) {
    if (!vowels.contains(n) || bindings.empty() || bindings.size() > 128)
      fail("invalid vowel binding: " + n);
    std::set<std::string> bound;
    for (const auto &b : bindings)
      if (!shapes.contains(b.shape) || !bound.insert(b.shape).second ||
          !std::isfinite(b.weight) || b.weight <= 0 || b.weight > 1)
        fail("invalid or duplicate shape binding: " + n);
  }
  if (all && visemes.size() != 5)
    fail("all five vowel expressions required");
}
void AvatarExpressions::rename_shape(MorphSet &m, std::string_view before,
                                     std::string after) {
  validate(m);
  name_ok(after);
  MorphSet next = m;
  AvatarExpressions expressions = *this;
  bool found = false;
  for (auto &t : next.targets)
    if (t.name == before) {
      t.name = after;
      found = true;
    }
  if (!found)
    fail("shape to rename not found");
  for (auto &[n, v] : expressions.visemes)
    for (auto &b : v)
      if (b.shape == before)
        b.shape = after;
  expressions.validate(next);
  m = std::move(next);
  *this = std::move(expressions);
}
nlohmann::json AvatarExpressions::encode() const {
  nlohmann::json v = nlohmann::json::object();
  for (const auto &[n, b] : visemes) {
    v[n] = nlohmann::json::array();
    for (const auto &i : b)
      v[n].push_back({{"shape", i.shape}, {"weight", i.weight}});
  }
  return {{"version", 1}, {"visemes", v}};
}
AvatarExpressions AvatarExpressions::decode(const nlohmann::json &j,
                                            const MorphSet &m) {
  version(j);
  AvatarExpressions out;
  const auto &v = j.at("visemes");
  if (!v.is_object() || v.size() > 5)
    fail("invalid viseme object");
  for (auto it = v.begin(); it != v.end(); ++it) {
    if (!it.value().is_array() || it.value().size() > 128)
      fail("excessive viseme bindings");
    for (const auto &b : it.value())
      out.visemes[it.key()].push_back(
          {b.at("shape").get<std::string>(), b.at("weight").get<float>()});
  }
  out.validate(m);
  return out;
}
} // namespace dome
