// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/humanoid-creator.hpp"
#include <map>
#include <nlohmann/json.hpp>
#include <numbers>
#include <numeric>
#include <set>
namespace dome {
namespace {
struct SourceVertex {
  uint32_t vertex;
  std::array<float, 2> uv;
};
struct SourceSpan {
  uint32_t start, count;
};
struct SourceBone {
  const char *name;
  int parent;
  uint32_t head_start, head_count, tail_start, tail_count;
};
struct SourceDelta {
  uint32_t vertex;
  Vec3 delta;
};
struct SourceTarget {
  const char *name;
  uint32_t start, count;
};
#include "humanoid-base.inc"
constexpr float pi = std::numbers::pi_v<float>;
[[noreturn]] void fail(const char *s) { throw std::invalid_argument(s); }
void checkpoint(const std::function<bool()> &cancel) {
  if (cancel && cancel())
    throw std::runtime_error("Humanoid creation cancelled");
}
float smooth(float a, float b, float x) {
  float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
  return t * t * (3 - 2 * t);
}
void target(std::vector<Vec3> &positions, std::string_view name, float weight) {
  if (weight == 0)
    return;
  for (const auto &t : source_targets)
    if (t.name == name) {
      for (uint32_t i = t.start; i < t.start + t.count; ++i)
        positions[source_deltas[i].vertex] += source_deltas[i].delta * weight;
      return;
    }
  throw std::logic_error("Missing embedded humanoid target");
}
void signed_target(std::vector<Vec3> &p, const std::string &name,
                   float amount) {
  target(p, name + (amount < 0 ? "-decr" : "-incr"), std::abs(amount));
}
void shape(std::vector<Vec3> &p, const HumanoidControls &c) {
  signed_target(p, "head-scale-horiz", c.head_width * .65f);
  signed_target(p, "head-scale-vert", c.head_height * .55f);
  signed_target(p, "chin-width",
                std::clamp(c.jaw_width * .6f - c.anime * .35f, -1.f, 1.f));
  signed_target(p, "nose-scale-depth",
                std::clamp(c.nose_size * .6f - c.anime * .35f, -1.f, 1.f));
  signed_target(p, "nose-scale-horiz", c.nose_size * .45f);
  signed_target(p, "mouth-scale-horiz", c.mouth_width * .6f);
  for (std::string side : {"l", "r"}) {
    target(p, side + "-eye-scale-incr", c.eye_size);
    for (int height = 1; height <= 3; ++height)
      target(p, side + "-eye-height" + std::to_string(height) + "-incr",
             c.anime * c.eye_size * .7f);
  }
  // Original continuous ocular deformation moves body and helper/joint data
  // together; authored MakeHuman targets alone have a deliberately small range.
  for (auto &v : p)
    for (float side : {1.f, -1.f}) {
      Vec3 center{side * .3077f, 7.2842f, 1.2453f}, d = v - center;
      float w = std::exp(-((d.x / .65f) * (d.x / .65f) +
                           (d.y / .32f) * (d.y / .32f))) *
                smooth(.65f, 1.1f, v.z) * smooth(0, .20f, side * v.x);
      v.x +=
          (d.x * c.anime * c.eye_size * .45f + side * c.eye_spacing * .10f) * w;
      v.y += d.y * c.anime * c.eye_size * .80f * w;
      v.z += d.z * c.anime * c.eye_size * .18f * w;
    }
}
Vec3 body_warp(Vec3 p, const HumanoidControls &c) {
  float head =
      smooth(5.3f, 6.3f, p.y) * (1 - smooth(1.0f, 1.8f, std::abs(p.x)));
  float factor = 1 + c.anime * .23f * head;
  p = Vec3{0, 5.9f, .1f} + (p - Vec3{0, 5.9f, .1f}) * factor;
  float torso = smooth(.4f, 1.3f, p.y) * (1 - smooth(5.2f, 6.0f, p.y));
  float upper = smooth(2.8f, 5.1f, p.y) * (1 - smooth(5.4f, 6.0f, p.y));
  float waist = std::exp(-std::pow((p.y - 2.4f) / 1.2f, 2.f));
  p.x *= 1 + c.build * .08f * torso + c.shoulder_width * .12f * upper +
         c.waist_width * .16f * waist;
  p.z *= 1 + c.build * .12f * torso;
  if (p.y < .8f)
    p.y = .8f + (p.y - .8f) * (1 + c.leg_length * .16f);
  float arm = smooth(1.4f, 2.0f, std::abs(p.x));
  p.x += (p.x < 0 ? -1.f : 1.f) * std::max(0.f, std::abs(p.x) - 1.6f) *
         c.arm_length * .14f * arm;
  return p;
}
struct Frame {
  const HumanoidControls &c;
  float floor = 0, scale = 1;
  Vec3 operator()(Vec3 p) const {
    p = body_warp(p, c);
    return {p.x * scale, (p.y - floor) * scale, p.z * scale};
  }
};
Vec3 joint_point(const std::vector<Vec3> &p, uint32_t start, uint32_t count) {
  Vec3 result;
  for (uint32_t i = start; i < start + count; ++i)
    result += p[source_joint_vertices[i]];
  return result / float(count);
}
Skeleton make_rig(const std::vector<Vec3> &p, const Frame &frame) {
  Skeleton s;
  std::vector<Vec3> heads;
  std::vector<Quat> rotations;
  for (const auto &b : source_bones) {
    Vec3 h = frame(joint_point(p, b.head_start, b.head_count)),
         t = frame(joint_point(p, b.tail_start, b.tail_count));
    if (length(t - h) < 1e-5f)
      fail("Parametric humanoid produced collapsed joint");
    heads.push_back(h);
    rotations.push_back(arcball({0, 1, 0}, normalized(t - h)));
    Bone bone;
    bone.name = b.name;
    bone.parent = b.parent;
    bone.length = length(t - h);
    s.bones.push_back(bone);
  }
  for (size_t i = 0; i < s.bones.size(); ++i) {
    int parent = s.bones[i].parent;
    s.bones[i].rest.translation =
        parent < 0
            ? heads[i]
            : rotations[parent].inverse().rotate(heads[i] - heads[parent]);
    s.bones[i].rest.rotation =
        parent < 0 ? rotations[i] : rotations[parent].inverse() * rotations[i];
  }
  s.validate();
  s.inverse_bind();
  return s;
}
uint32_t bone_id(std::string_view name) {
  for (uint32_t i = 0; i < std::size(source_bones); ++i)
    if (source_bones[i].name == name)
      return i;
  throw std::logic_error("Missing embedded humanoid bone");
}
HumanoidMapping semantics(const Skeleton &s) {
  HumanoidMapping m;
  auto put = [&](std::string n, std::string b) {
    m.joints[std::move(n)] = bone_id(b);
  };
  put("hips", "root");
  put("spine", "spine05");
  put("chest", "spine03");
  put("upperChest", "spine01");
  put("neck", "neck01");
  put("head", "head");
  put("jaw", "jaw");
  for (std::string side : {"left", "right"}) {
    std::string suffix = side == "left" ? ".L" : ".R";
    for (auto [semantic, bone] :
         std::initializer_list<std::pair<const char *, const char *>>{
             {"Eye", "eye"},
             {"Shoulder", "clavicle"},
             {"UpperArm", "upperarm01"},
             {"LowerArm", "lowerarm01"},
             {"Hand", "wrist"},
             {"UpperLeg", "upperleg01"},
             {"LowerLeg", "lowerleg01"},
             {"Foot", "foot"},
             {"Toes", "toe1-1"}})
      put(side + semantic, std::string(bone) + suffix);
    for (int finger = 1; finger <= 5; ++finger)
      for (int joint = 1; joint <= 3; ++joint) {
        std::string role;
        if (finger == 1)
          role = joint == 1   ? "ThumbMetacarpal"
                 : joint == 2 ? "ThumbProximal"
                              : "ThumbDistal";
        else
          role = std::array<std::string, 4>{"Index", "Middle", "Ring",
                                            "Little"}[finger - 2] +
                 (joint == 1   ? "Proximal"
                  : joint == 2 ? "Intermediate"
                               : "Distal");
        put(side + role, "finger" + std::to_string(finger) + "-" +
                             std::to_string(joint) + suffix);
      }
  }
  m.validate(s);
  return m;
}
void normals(Mesh &m) {
  m.normals.assign(m.positions.size(), {});
  for (size_t t = 0; t < m.indices.size(); t += 3) {
    auto a = m.indices[t], b = m.indices[t + 1], c = m.indices[t + 2];
    Vec3 n =
        cross(m.positions[b] - m.positions[a], m.positions[c] - m.positions[a]);
    m.normals[a] += n;
    m.normals[b] += n;
    m.normals[c] += n;
  }
  for (auto &n : m.normals)
    n = normalized(n);
}
void finish(HumanoidPart &part, const Skeleton &s,
            const HumanoidMapping &mapping,
            std::vector<VertexWeights> weights) {
  normals(part.mesh);
  part.mesh.validate();
  part.deformation = DeformationState(part.mesh);
  part.deformation.skeleton = s;
  part.deformation.skin = SkinWeights{std::move(weights)};
  part.deformation.humanoid = mapping;
  part.deformation.validate(part.mesh);
}
struct Builder {
  HumanoidPart part;
  std::vector<VertexWeights> weights;
  uint32_t vertex(Vec3 p, std::array<float, 2> uv, uint32_t joint) {
    part.mesh.positions.push_back(p);
    part.mesh.uv.push_back(uv);
    weights.push_back({{joint, 1}});
    return uint32_t(part.mesh.positions.size() - 1);
  }
  void triangle(uint32_t a, uint32_t b, uint32_t c) {
    part.mesh.indices.insert(part.mesh.indices.end(), {a, b, c});
  }
};
void sphere(Builder &b, Vec3 center, Vec3 radius, uint32_t bone, int side) {
  constexpr int segments = 24, rings = 12;
  uint32_t start = uint32_t(b.part.mesh.positions.size());
  // Ring vertices have explicit UV seam splits; poles use triangle fans.
  auto top = b.vertex(center + Vec3{0, radius.y, 0},
                      {float(side) * .5f + .25f, 0}, bone);
  for (int r = 1; r < rings; ++r)
    for (int s = 0; s <= segments; ++s) {
      float phi = pi * float(r) / rings, theta = 2 * pi * float(s) / segments;
      b.vertex(
          center + Vec3{radius.x * std::sin(phi) * std::sin(theta),
                        radius.y * std::cos(phi),
                        radius.z * std::sin(phi) * std::cos(theta)},
          {float(side) * .5f + float(s) / segments * .5f, float(r) / rings},
          bone);
    }
  auto bottom = b.vertex(center - Vec3{0, radius.y, 0},
                         {float(side) * .5f + .25f, 1}, bone);
  for (int s = 0; s < segments; ++s) {
    b.triangle(top, start + 1 + s, start + 2 + s);
    uint32_t last = start + 1 + (rings - 2) * (segments + 1);
    b.triangle(bottom, last + s + 1, last + s);
  }
  for (int r = 0; r < rings - 2; ++r)
    for (int s = 0; s < segments; ++s) {
      uint32_t a = start + 1 + r * (segments + 1) + s, c = a + segments + 1;
      b.triangle(a, c, a + 1);
      b.triangle(a + 1, c, c + 1);
    }
}
void iris(Builder &b, Vec3 center, Vec3 radius, float extent, float offset,
          uint32_t bone, int side, bool relief = false) {
  constexpr int segments = 32, rings = 6;
  uint32_t first = b.vertex(center + Vec3{0, 0, radius.z + offset},
                            {float(side) * .5f + .25f, .5f}, bone);
  for (int r = 1; r <= rings; ++r)
    for (int s = 0; s <= segments; ++s) {
      float a = 2 * pi * float(s) / segments, f = extent * float(r) / rings,
            x = std::cos(a) * f, y = std::sin(a) * f;
      b.vertex(
          center +
              Vec3{radius.x * x, radius.y * y,
                   radius.z * (std::sqrt(1 - f * f) +
                               (relief ? .006f * smooth(.29f, .38f, f) *
                                             std::sin(7 * a) *
                                             std::sin(pi * float(r) / rings)
                                       : 0.f)) +
                       offset},
          {float(side) * .5f + .25f + x * .25f / extent,
           .5f + y * .5f / extent},
          bone);
    }
  for (int s = 0; s < segments; ++s)
    b.triangle(first, first + 1 + s, first + 2 + s);
  for (int r = 0; r < rings - 1; ++r)
    for (int s = 0; s < segments; ++s) {
      uint32_t a = first + 1 + r * (segments + 1) + s, c = a + segments + 1;
      b.triangle(a, c, c + 1);
      b.triangle(a, c + 1, a + 1);
    }
}
class FaceProjector {
  const Mesh &mesh;
  Vec3 lo, hi;
  std::array<std::vector<uint32_t>, 32 * 32> cells;
  int index(float v, float a, float b) const {
    return std::clamp(int((v - a) / (b - a) * 32), 0, 31);
  }

public:
  explicit FaceProjector(const Mesh &m)
      : mesh(m), lo(m.positions.front()), hi(lo) {
    for (auto p : m.positions) {
      lo.x = std::min(lo.x, p.x);
      lo.y = std::min(lo.y, p.y);
      hi.x = std::max(hi.x, p.x);
      hi.y = std::max(hi.y, p.y);
    }
    for (uint32_t i = 0; i < m.indices.size(); i += 3) {
      auto a = m.positions[m.indices[i]], b = m.positions[m.indices[i + 1]],
           c = m.positions[m.indices[i + 2]];
      int x0 = index(std::min({a.x, b.x, c.x}), lo.x, hi.x),
          x1 = index(std::max({a.x, b.x, c.x}), lo.x, hi.x),
          y0 = index(std::min({a.y, b.y, c.y}), lo.y, hi.y),
          y1 = index(std::max({a.y, b.y, c.y}), lo.y, hi.y);
      for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
          cells[y * 32 + x].push_back(i);
    }
  }
  float front(float x, float y) const {
    float result = -1e6f;
    if (x < lo.x || x > hi.x || y < lo.y || y > hi.y)
      fail("Humanoid face projection outside body");
    for (auto i : cells[index(y, lo.y, hi.y) * 32 + index(x, lo.x, hi.x)]) {
      auto a = mesh.positions[mesh.indices[i]],
           b = mesh.positions[mesh.indices[i + 1]],
           c = mesh.positions[mesh.indices[i + 2]];
      float det = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
      if (std::abs(det) < 1e-12f)
        continue;
      float u = ((b.y - c.y) * (x - c.x) + (c.x - b.x) * (y - c.y)) / det,
            v = ((c.y - a.y) * (x - c.x) + (a.x - c.x) * (y - c.y)) / det;
      if (u >= -1e-5f && v >= -1e-5f && u + v <= 1.00001f)
        result = std::max(result, u * a.z + v * b.z + (1 - u - v) * c.z);
    }
    if (result < -1e5f)
      fail("Humanoid face projection missed body");
    return result;
  }
};
void eyebrow(Builder &b, const FaceProjector &body, Vec3 center, Vec3 radius,
             int side) {
  uint32_t first = uint32_t(b.part.mesh.positions.size()),
           head = bone_id("head");
  for (int i = 0; i <= 8; ++i) {
    float t = float(i) / 8, x = center.x + (t - .5f) * radius.x * 1.9f,
          y = center.y + radius.y * (1.6f + .20f * std::sin(pi * t));
    for (int row = 0; row < 2; ++row) {
      float yy = y + (row ? 1 : -1) * radius.y * .07f;
      float z = body.front(x, yy) + .0008f;
      b.vertex({x, yy, z},
               {float(side) * .5f + t * .5f, .44f + float(row) * .14f}, head);
    }
  }
  for (uint32_t i = 0; i < 8; ++i) {
    uint32_t a = first + i * 2;
    b.triangle(a, a + 2, a + 1);
    b.triangle(a + 1, a + 2, a + 3);
  }
}
void upper_liner(Builder &b, const FaceProjector &body, Vec3 center,
                 Vec3 radius, int side) {
  constexpr int segments = 16;
  uint32_t first = uint32_t(b.part.mesh.positions.size()),
           head = bone_id("head");
  for (int i = 0; i <= segments; ++i) {
    float t = float(i) / segments, nx = (t - .5f) * 1.88f,
          x = center.x + nx * radius.x,
          limit = radius.y * std::sqrt(1 - nx * nx);
    auto eye_z = [&](float y) {
      float ny = (y - center.y) / radius.y;
      return center.z +
             radius.z * std::sqrt(std::max(0.f, 1 - nx * nx - ny * ny));
    };
    auto gap = [&](float y) { return body.front(x, y) - eye_z(y); };
    float lower = center.y, upper = center.y + limit * .999f;
    bool bracket = gap(lower) < 0 && gap(upper) >= 0;
    if (bracket)
      for (int step = 0; step < 18; ++step) {
        float middle = (lower + upper) * .5f;
        if (gap(middle) < 0)
          lower = middle;
        else
          upper = middle;
      }
    float y = bracket ? (lower + upper) * .5f : center.y + limit * .45f;
    float width = radius.y * (.025f + .035f * std::sin(pi * t));
    for (int row = 0; row < 2; ++row) {
      float yy = y + (row ? 1 : -1) * width;
      float z = std::max(body.front(x, yy), eye_z(yy)) + .0006f;
      b.vertex({x, yy, z},
               {float(side) * .5f + t * .5f, .62f + float(row) * .14f}, head);
    }
  }
  for (uint32_t i = 0; i < segments; ++i) {
    uint32_t a = first + i * 2;
    b.triangle(a, a + 2, a + 1);
    b.triangle(a + 1, a + 2, a + 3);
  }
}
void limbal_ring(Builder &b, Vec3 center, Vec3 radius, uint32_t eye, int side) {
  constexpr int segments = 32;
  uint32_t first = uint32_t(b.part.mesh.positions.size());
  for (int i = 0; i <= segments; ++i) {
    float a = 2 * pi * float(i) / segments;
    for (float f : {.70f, .765f}) {
      float x = std::cos(a) * f, y = std::sin(a) * f;
      b.vertex(
          center + Vec3{radius.x * x, radius.y * y,
                        radius.z * (std::sqrt(1 - f * f) + .018f)},
          {float(side) * .5f + .25f + x * .25f, .80f + (.5f + y * .5f) * .19f},
          eye);
    }
  }
  for (uint32_t i = 0; i < segments; ++i) {
    uint32_t a = first + i * 2;
    b.triangle(a, a + 1, a + 2);
    b.triangle(a + 1, a + 3, a + 2);
  }
}
float hair_phi(float angle) {
  float z = std::cos(angle);
  return 1.7f - .45f * std::max(0.f, z) + .65f * std::max(0.f, -z);
}
Vec3 scalp(float theta, float phi, float volume) {
  return {std::sin(theta) * std::sin(phi) * (.95f + volume * .12f),
          7.55f + std::cos(phi) * 1.08f,
          .35f + std::cos(theta) * std::sin(phi) * (1.20f + volume * .10f)};
}
void groom(Builder &b, const Frame &f, const HumanoidControls &c) {
  constexpr int segments = 32, rings = 10;
  uint32_t head = bone_id("head");
  auto top = b.vertex(f(scalp(0, 0, c.hair_volume)), {.5f, 0}, head);
  for (int r = 1; r <= rings; ++r)
    for (int s = 0; s <= segments; ++s) {
      float angle = 2 * pi * float(s) / segments;
      b.vertex(
          f(scalp(angle, hair_phi(angle) * float(r) / rings, c.hair_volume)),
          {float(s) / segments, float(r) / rings * .38f}, head);
    }
  for (int s = 0; s < segments; ++s)
    b.triangle(top, 1 + s, 2 + s);
  for (int r = 0; r < rings - 1; ++r)
    for (int s = 0; s < segments; ++s) {
      uint32_t a = 1 + r * (segments + 1) + s, d = a + segments + 1;
      b.triangle(a, d, a + 1);
      b.triangle(a + 1, d, d + 1);
    }
  // Closed six-sided swept locks, with separate UV cells and a pointed fan tip.
  constexpr int locks = 24, steps = 10, sides = 6;
  for (int lock = 0; lock < locks; ++lock) {
    float theta = 2 * pi * (float(lock) + .5f) / locks,
          front = std::max(0.f, std::cos(theta));
    uint32_t start = uint32_t(b.part.mesh.positions.size());
    Vec3 previous;
    for (int row = 0; row < steps; ++row) {
      float t = float(row) / (steps - 1),
            phi = .7f + (hair_phi(theta) - .7f) * t;
      Vec3 center = scalp(theta, phi, c.hair_volume);
      float drop = std::max(0.f, (t - .55f) / .45f) *
                   (front > .7f ? c.fringe * .5f : c.hair_length * 4.5f);
      center.y -= drop;
      float flare = drop * .08f;
      center.x += std::sin(theta) * flare;
      center.z += std::cos(theta) * flare;
      float width = (.14f + c.hair_volume * .05f) * (1 - .75f * t),
            depth = .07f * (1 - .6f * t);
      for (int side = 0; side <= sides; ++side) {
        float a = 2 * pi * float(side) / sides;
        Vec3 p = center + Vec3{std::cos(theta) * std::cos(a) * width +
                                   std::sin(theta) * std::sin(a) * depth,
                               0,
                               -std::sin(theta) * std::cos(a) * width +
                                   std::cos(theta) * std::sin(a) * depth};
        b.vertex(f(p),
                 {(float(lock % 6) + .06f + float(side) / sides * .88f) / 6,
                  .42f + (float(lock / 6) + .12f + t * .76f) / 4 * .58f},
                 head);
      }
      previous = center;
    }
    for (int row = 0; row < steps - 1; ++row)
      for (int side = 0; side < sides; ++side) {
        uint32_t a = start + row * (sides + 1) + side, d = a + sides + 1;
        b.triangle(a, d, a + 1);
        b.triangle(a + 1, d, d + 1);
      }
    auto tip = b.vertex(
        f(previous + Vec3{0, -.07f, 0}),
        {float(lock % 6) / 6 + .5f / 6, .42f + float(lock / 6 + 1) / 4 * .58f},
        head);
    auto cap = b.vertex(
        f(scalp(theta, .7f, c.hair_volume)),
        {float(lock % 6) / 6 + .5f / 6, .42f + float(lock / 6) / 4 * .58f},
        head);
    for (int side = 0; side < sides; ++side) {
      uint32_t last = start + (steps - 1) * (sides + 1);
      b.triangle(tip, last + side + 1, last + side);
      b.triangle(cap, start + side, start + side + 1);
    }
  }
}
} // namespace
void HumanoidControls::validate() const {
  if (!std::isfinite(height) || height < 1.2f || height > 2.2f)
    fail("Humanoid height must be within 1.2..2.2 metres");
  for (float value : {anime, eye_size, hair_length, hair_volume, fringe})
    if (!std::isfinite(value) || value < 0 || value > 1)
      fail("Humanoid style controls must be within [0,1]");
  for (float value :
       {build, shoulder_width, waist_width, leg_length, arm_length, head_width,
        head_height, eye_spacing, nose_size, mouth_width, jaw_width})
    if (!std::isfinite(value) || value < -1 || value > 1)
      fail("Humanoid proportion controls must be within [-1,1]");
  for (auto color : {skin_color, iris_color, hair_color})
    for (float v : color)
      if (!std::isfinite(v) || v < 0 || v > 1)
        fail("Invalid humanoid material color");
}
nlohmann::json HumanoidControls::encode() const {
  validate();
  return {{"version", 1},
          {"height", height},
          {"anime", anime},
          {"build", build},
          {"shoulder_width", shoulder_width},
          {"waist_width", waist_width},
          {"leg_length", leg_length},
          {"arm_length", arm_length},
          {"head_width", head_width},
          {"head_height", head_height},
          {"eye_size", eye_size},
          {"eye_spacing", eye_spacing},
          {"nose_size", nose_size},
          {"mouth_width", mouth_width},
          {"jaw_width", jaw_width},
          {"hair_length", hair_length},
          {"hair_volume", hair_volume},
          {"fringe", fringe},
          {"skin_color", skin_color},
          {"iris_color", iris_color},
          {"hair_color", hair_color}};
}
HumanoidControls HumanoidControls::decode(const nlohmann::json &j) {
  if (!j.is_object() || j.at("version") != 1 || j.size() != 21)
    fail("Unsupported humanoid controls record");
  HumanoidControls c;
#define READ(n) c.n = j.at(#n).get<decltype(c.n)>()
  READ(height);
  READ(anime);
  READ(build);
  READ(shoulder_width);
  READ(waist_width);
  READ(leg_length);
  READ(arm_length);
  READ(head_width);
  READ(head_height);
  READ(eye_size);
  READ(eye_spacing);
  READ(nose_size);
  READ(mouth_width);
  READ(jaw_width);
  READ(hair_length);
  READ(hair_volume);
  READ(fringe);
  READ(skin_color);
  READ(iris_color);
  READ(hair_color);
#undef READ
  c.validate();
  return c;
}
HumanoidControls humanoid_preset(HumanoidPreset preset) {
  HumanoidControls c;
  switch (preset) {
  case HumanoidPreset::Neutral:
    c.anime = 0;
    c.eye_size = 0;
    c.nose_size = 0;
    c.mouth_width = 0;
    c.jaw_width = 0;
    c.hair_length = .12f;
    c.fringe = .15f;
    break;
  case HumanoidPreset::Toon:
    c.height = 1.65f;
    c.anime = .7f;
    c.build = .15f;
    c.shoulder_width = .1f;
    c.waist_width = .1f;
    c.leg_length = -.25f;
    c.arm_length = -.15f;
    c.head_width = .4f;
    c.head_height = -.25f;
    c.eye_size = .8f;
    c.nose_size = -.45f;
    c.jaw_width = -.3f;
    c.mouth_width = .1f;
    c.hair_length = .15f;
    c.hair_volume = .6f;
    c.fringe = .5f;
    break;
  case HumanoidPreset::Anime:
    c.height = 1.6f;
    c.anime = 1;
    c.build = -.45f;
    c.shoulder_width = -.4f;
    c.waist_width = -.15f;
    c.leg_length = .25f;
    c.arm_length = -.15f;
    c.head_width = .35f;
    c.head_height = -.35f;
    c.eye_size = 1;
    c.eye_spacing = .1f;
    c.nose_size = -.8f;
    c.mouth_width = -.45f;
    c.jaw_width = -.6f;
    c.hair_length = .45f;
    c.hair_volume = .7f;
    c.fringe = .7f;
    break;
  default:
    fail("Unknown humanoid style preset");
  }
  c.validate();
  return c;
}
void CreatedHumanoid::validate() const {
  controls.validate();
  if (parts.size() != 5)
    fail("Incomplete humanoid parts");
  size_t vertices = 0;
  std::set<std::string> names;
  std::optional<nlohmann::json> reference_rig;
  for (const auto &p : parts) {
    if (p.name.empty() || p.name.size() > 64 || !names.insert(p.name).second)
      fail("Invalid humanoid part identity");
    p.mesh.validate();
    p.deformation.validate(p.mesh);
    if (!p.deformation.skeleton || !p.deformation.skin ||
        !p.deformation.humanoid)
      fail("Unrigged humanoid part");
    p.deformation.humanoid->validate(*p.deformation.skeleton);
    auto rig = p.deformation.skeleton->encode();
    if (reference_rig && rig != *reference_rig)
      fail("Humanoid part skeletons differ");
    reference_rig = std::move(rig);
    if (p.mesh.uv.size() != p.mesh.positions.size())
      fail("Humanoid part requires complete stable UVs");
    if (!std::isfinite(p.roughness) || p.roughness < 0 || p.roughness > 1)
      fail("Invalid humanoid material roughness");
    for (float channel : p.color)
      if (!std::isfinite(channel) || channel < 0 || channel > 1)
        fail("Invalid humanoid part color");
    vertices += p.mesh.positions.size();
    for (auto uv : p.mesh.uv)
      if (uv[0] < 0 || uv[0] > 1 || uv[1] < 0 || uv[1] > 1)
        fail("Humanoid UV outside atlas bounds");
  }
  if (vertices > 30'000)
    fail("Humanoid generation budget exceeded");
}
CreatedHumanoid create_humanoid(const HumanoidControls &c,
                                const std::function<bool()> &cancel) {
  c.validate();
  checkpoint(cancel);
  std::vector<Vec3> p(std::begin(source_positions), std::end(source_positions));
  shape(p, c);
  Frame frame{c};
  float ceiling = -1000;
  frame.floor = 1000;
  for (const auto &v : source_vertices) {
    auto q = body_warp(p[v.vertex], c);
    frame.floor = std::min(frame.floor, q.y);
    ceiling = std::max(ceiling, q.y);
  }
  frame.scale = c.height / (ceiling - frame.floor);
  auto s = make_rig(p, frame);
  auto mapping = semantics(s);
  HumanoidPart body;
  body.roughness = .85f;
  body.name = "Body";
  body.color = c.skin_color;
  body.mesh.indices.assign(std::begin(source_triangles),
                           std::end(source_triangles));
  std::vector<VertexWeights> weights;
  for (const auto &v : source_vertices) {
    body.mesh.positions.push_back(frame(p[v.vertex]));
    body.mesh.uv.push_back(v.uv);
    auto span = source_weight_spans[v.vertex];
    VertexWeights w(std::begin(source_weights) + span.start,
                    std::begin(source_weights) + span.start + span.count);
    weights.push_back(
        normalize_weights(std::move(w), s.bones.size(), {}, {12, 0}));
  }
  finish(body, s, mapping, std::move(weights));
  // Smooth body normals across original positional IDs, including UV splits.
  std::vector<Vec3> sums(std::size(source_positions));
  for (size_t i = 0; i < body.mesh.positions.size(); ++i)
    sums[source_vertices[i].vertex] += body.mesh.normals[i];
  for (size_t i = 0; i < body.mesh.positions.size(); ++i)
    body.mesh.normals[i] = normalized(sums[source_vertices[i].vertex]);
  body.deformation.morphs = MorphSet(body.mesh);
  body.deformation.expressions = AvatarExpressions{};
  for (std::string expression :
       {"aa", "ih", "ou", "ee", "oh", "blinkLeft", "blinkRight", "smile"}) {
    checkpoint(cancel);
    std::vector<Vec3> edited(std::begin(source_positions),
                             std::end(source_positions));
    if (expression == "aa")
      target(edited, "mouth-open", .75f);
    if (expression == "ih" || expression == "ee") {
      target(edited, "mouth-open", expression == "ee" ? .2f : .12f);
      target(edited, "mouth-corner-puller", expression == "ee" ? .75f : .45f);
    }
    if (expression == "ou" || expression == "oh") {
      target(edited, "mouth-open", expression == "oh" ? .55f : .25f);
      target(edited, "mouth-pursing", expression == "oh" ? .45f : .7f);
    }
    if (expression == "blinkLeft")
      target(edited, "eye-left-closure", .95f);
    if (expression == "blinkRight")
      target(edited, "eye-right-closure", .95f);
    if (expression == "smile")
      target(edited, "mouth-corner-puller", .75f);
    shape(edited, c);
    std::vector<Vec3> positions;
    positions.reserve(body.mesh.positions.size());
    for (const auto &v : source_vertices)
      positions.push_back(frame(edited[v.vertex]));
    body.deformation.morphs->set_target(body.mesh, expression, positions);
    if (expression.size() == 2)
      body.deformation.expressions->visemes[expression] = {{expression, 1}};
  }
  body.deformation.landmarks = HumanLandmarks{};
  auto segments = s.segments();
  for (auto [role, index] : mapping.joints)
    body.deformation.landmarks->points[role] = segments[index][0];
  CreatedHumanoid out;
  out.controls = c;
  out.parts.push_back(std::move(body));
  Builder sclera, irises, pupils, hair;
  sclera.part.name = "Eyes";
  irises.part.name = "Irises";
  pupils.part.name = "Pupils";
  hair.part.name = "Hair";
  sclera.part.roughness = .12f;
  irises.part.roughness = .22f;
  pupils.part.roughness = .14f;
  hair.part.roughness = .48f;
  irises.part.color = c.iris_color;
  pupils.part.color = {.008f, .008f, .012f, 1};
  hair.part.color = c.hair_color;
  FaceProjector projector(out.parts[0].mesh);
  for (int side = 0; side < 2; ++side) {
    checkpoint(cancel);
    std::span<const uint32_t> vertices =
        side == 0 ? std::span<const uint32_t>(source_l_eye)
                  : std::span<const uint32_t>(source_r_eye);
    Vec3 lo = p[vertices.front()], hi = lo;
    for (auto v : vertices) {
      lo = {std::min(lo.x, p[v].x), std::min(lo.y, p[v].y),
            std::min(lo.z, p[v].z)};
      hi = {std::max(hi.x, p[v].x), std::max(hi.y, p[v].y),
            std::max(hi.z, p[v].z)};
    }
    Vec3 raw = (lo + hi) * .5f, center = frame(raw), radius;
    Vec3 rx = frame(raw + Vec3{(hi.x - lo.x) * .5f, 0, 0}) - center,
         ry = frame(raw + Vec3{0, (hi.y - lo.y) * .5f, 0}) - center,
         rz = frame(raw + Vec3{0, 0, (hi.z - lo.z) * .5f}) - center;
    radius = {length(rx), length(ry), length(rz)};
    uint32_t eye = bone_id(side == 0 ? "eye.L" : "eye.R");
    sphere(sclera, center, radius, eye, side);
    iris(irises, center, radius, .72f, radius.z * .008f, eye, side, true);
    auto pupil_start = pupils.part.mesh.uv.size();
    iris(pupils, center, radius, .29f, radius.z * .016f, eye, side);
    for (size_t i = pupil_start; i < pupils.part.mesh.uv.size(); ++i)
      pupils.part.mesh.uv[i][1] *= .4f;
    eyebrow(pupils, projector, center, radius, side);
    upper_liner(pupils, projector, center, radius, side);
    limbal_ring(pupils, center, radius, eye, side);
  }
  checkpoint(cancel);
  groom(hair, frame, c);
  for (auto *b : {&sclera, &irises, &pupils, &hair}) {
    finish(b->part, s, mapping, std::move(b->weights));
    out.parts.push_back(std::move(b->part));
  }
  out.notes = {"Body, face controls, 163-joint rig and authored sparse weights "
               "derive from explicitly CC0 MakeHuman assets.",
               "Eye and hair geometry are native original shapes; VRoid Studio "
               "is a workflow reference only.",
               "No clothing, texture artistry, physics, hair collision solver, "
               "ethnicity/age database or full VRM exporter is supplied.",
               "Part skeletons share semantic indices; publish and pose the "
               "parts as one character transaction."};
  out.validate();
  checkpoint(cancel);
  return out;
}
} // namespace dome
