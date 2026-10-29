// SPDX-License-Identifier: GPL-3.0-or-later
#include "vroid-groom.hpp"
#include "dome/source-maps.hpp"
#include "humanoid-groom.hpp"
#include <limits>
namespace dome::creator_detail {
namespace {
struct Triangle {
  Vec3 a, b, c;
};
class Envelope {
  Vec3 lo{1e6f, 1e6f, 0}, hi{-1e6f, -1e6f, 0};
  std::vector<Triangle> triangles;
  std::array<std::vector<uint32_t>, 4096> cells;
  int index(float p, float a, float b) const {
    return std::clamp(int((p - a) / (b - a) * 64), 0, 63);
  }

public:
  Envelope(const std::vector<HumanoidPart> &parts,
           const std::function<bool()> &cancel) {
    for (const auto &p : parts)
      for (auto v : p.mesh.positions) {
        lo.x = std::min(lo.x, v.x);
        lo.y = std::min(lo.y, v.y);
        hi.x = std::max(hi.x, v.x);
        hi.y = std::max(hi.y, v.y);
      }
    for (const auto &p : parts)
      for (size_t i = 0; i < p.mesh.indices.size(); i += 3) {
        if (i % 3072 == 0 && cancel && cancel())
          throw std::runtime_error("Groom fitting cancelled");
        Triangle t{p.mesh.positions[p.mesh.indices[i]],
                   p.mesh.positions[p.mesh.indices[i + 1]],
                   p.mesh.positions[p.mesh.indices[i + 2]]};
        auto id = uint32_t(triangles.size());
        triangles.push_back(t);
        int x0 = index(std::min({t.a.x, t.b.x, t.c.x}), lo.x, hi.x),
            x1 = index(std::max({t.a.x, t.b.x, t.c.x}), lo.x, hi.x),
            y0 = index(std::min({t.a.y, t.b.y, t.c.y}), lo.y, hi.y),
            y1 = index(std::max({t.a.y, t.b.y, t.c.y}), lo.y, hi.y);
        for (int y = y0; y <= y1; ++y)
          for (int x = x0; x <= x1; ++x)
            cells[y * 64 + x].push_back(id);
      }
  }
  std::optional<std::array<float, 2>> at(float x, float y) const {
    if (x < lo.x || x > hi.x || y < lo.y || y > hi.y)
      return {};
    float back = 1e6f, front = -1e6f;
    for (auto id : cells[index(y, lo.y, hi.y) * 64 + index(x, lo.x, hi.x)]) {
      const auto &t = triangles[id];
      float den =
          (t.b.y - t.c.y) * (t.a.x - t.c.x) + (t.c.x - t.b.x) * (t.a.y - t.c.y);
      if (std::abs(den) < 1e-12f)
        continue;
      float a = ((t.b.y - t.c.y) * (x - t.c.x) +
                 (t.c.x - t.b.x) * (y - t.c.y)) /
                den,
            b = ((t.c.y - t.a.y) * (x - t.c.x) +
                 (t.a.x - t.c.x) * (y - t.c.y)) /
                den;
      if (a >= -1e-5f && b >= -1e-5f && a + b <= 1.00001f) {
        float z = a * t.a.z + b * t.b.z + (1 - a - b) * t.c.z;
        back = std::min(back, z);
        front = std::max(front, z);
      }
    }
    if (back > front)
      return {};
    return std::array<float, 2>{back, front};
  }
};
struct Fit {
  Vec3 center, scale;
  Vec3 operator()(Vec3 p) const {
    return {center.x + p.x * scale.x, center.y + (p.y - 7.55f) * scale.y,
            center.z + (p.z - .35f) * scale.z};
  }
  Vec3 inverse(Vec3 p) const {
    return {(p.x - center.x) / scale.x, 7.55f + (p.y - center.y) / scale.y,
            .35f + (p.z - center.z) / scale.z};
  }
};
std::array<float, 4> palette(const CreatedHumanoid &model) {
  for (const auto &p : model.parts) {
    std::string name = p.name;
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    if (name.find("hair") == std::string::npos)
      continue;
    std::array<float, 4> result = p.color;
    if (p.source_maps && p.source_maps->color.image) {
      const auto &image = *p.source_maps->color.image;
      std::array<double, 3> sum{};
      double weight = 0;
      uint32_t sx = std::max(1u, image.width / 128),
               sy = std::max(1u, image.height / 128);
      for (uint32_t y = 0; y < image.height; y += sy)
        for (uint32_t x = 0; x < image.width; x += sx) {
          size_t offset = (size_t(y) * image.width + x) * 4;
          float a = image.rgba[offset + 3] / 255.f;
          if (a < .01f)
            continue;
          weight += a;
          for (int k = 0; k < 3; ++k)
            sum[k] += srgb_to_linear(image.rgba[offset + k] / 255.f) * a;
        }
      if (weight > 0)
        for (int k = 0; k < 3; ++k)
          result[k] *= float(sum[k] / weight);
    }
    result[3] = 1;
    return result;
  }
  auto c = model.controls.hair_color;
  return {.04f * c[0], .02f * c[1], .015f * c[2], 1};
}
void normals(Mesh &m) {
  m.normals.assign(m.positions.size(), {});
  for (size_t i = 0; i < m.indices.size(); i += 3) {
    auto a = m.indices[i], b = m.indices[i + 1], c = m.indices[i + 2];
    Vec3 n =
        cross(m.positions[b] - m.positions[a], m.positions[c] - m.positions[a]);
    m.normals[a] += n;
    m.normals[b] += n;
    m.normals[c] += n;
  }
  for (auto &n : m.normals)
    n = normalized(n);
}
std::shared_ptr<const PaintSourceMaps> texture(std::array<float, 4> color) {
  auto image = std::make_shared<Image>();
  image->width = image->height = 256;
  image->rgba.resize(256 * 256 * 4);
  constexpr float pi = std::numbers::pi_v<float>;
  for (uint32_t y = 0; y < 256; ++y)
    for (uint32_t x = 0; x < 256; ++x) {
      float u = (x + .5f) / 256, v = (y + .5f) / 256, factor;
      if (v < .4f)
        factor = .86f + .08f * std::cos(2 * pi * u * 24) +
                 .025f * std::cos(2 * pi * u * 48);
      else {
        float local = std::fmod(u * 6, 1.f);
        factor = .80f + .15f * std::cos(2 * pi * local - .4f);
      }
      size_t at = (size_t(y) * 256 + x) * 4;
      for (int k = 0; k < 3; ++k)
        image->rgba[at + k] = uint8_t(std::lround(
            std::clamp(linear_to_srgb(color[k] * factor), 0.f, 1.f) * 255));
      image->rgba[at + 3] = 255;
    }
  auto maps = std::make_shared<PaintSourceMaps>();
  maps->color.image = image;
  maps->color.wrap_s = maps->color.wrap_t = 33071;
  maps->color.mag_filter = maps->color.min_filter = 9729;
  return maps;
}
} // namespace
void append_vroid_groom(CreatedHumanoid &model,
                        const std::function<bool()> &cancel) {
  const auto &c = model.controls;
  if (c.hair_length == 0 && c.hair_volume == 0 && c.fringe == 0)
    return;
  if (cancel && cancel())
    throw std::runtime_error("Groom creation cancelled");
  const auto &s = *model.parts.front().deformation.skeleton;
  const auto &mapping = *model.parts.front().deformation.humanoid;
  auto world = s.world();
  uint32_t head = mapping.joints.at("head"), eye = mapping.joints.at("leftEye");
  Vec3 pivot{world[head][12], world[head][13], world[head][14]};
  float eye_y = world[eye][13];
  Vec3 lo{1e6f, 1e6f, 1e6f}, hi{-1e6f, -1e6f, -1e6f};
  float forehead = -1e6f;
  for (const auto &part : model.parts)
    for (auto p : part.mesh.positions)
      if (p.y > pivot.y - .035f && std::abs(p.x - pivot.x) < .25f) {
        lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
        hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
        if (p.y > eye_y + .02f)
          forehead = std::max(forehead, p.z);
      }
  if (hi.y <= lo.y || forehead <= lo.z)
    throw std::runtime_error("Cannot fit groom to source head bounds");
  float bottom = pivot.y - .035f;
  Vec3 center{(lo.x + hi.x) * .5f, (hi.y + bottom) * .5f,
              (lo.z + forehead) * .5f};
  Vec3 radii{
      std::max(.05f, (hi.x - lo.x) * .5f) + .008f + c.hair_volume * .020f,
      (hi.y - bottom) * .5f + .008f + c.hair_volume * .010f,
      std::max(.04f, (forehead - lo.z) * .5f) + .008f + c.hair_volume * .015f};
  Fit fit{center,
          {radii.x / (.95f + c.hair_volume * .12f), radii.y / 1.08f,
           radii.z / (1.20f + c.hair_volume * .10f)}};
  Envelope body(model.parts, cancel);
  HumanoidPart part;
  part.name = "Original fitted groom";
  part.toon = true;
  part.unlit = true;
  part.roughness = .5f;
  SkinWeights weights;
  auto cap_fit = [&](Vec3 p) {
    if (auto z = body.at(p.x, p.y)) {
      if (p.z >= center.z)
        p.z = std::max(p.z, (*z)[1] + .004f);
      else
        p.z = std::min(p.z, (*z)[0] - .004f);
    }
    return p;
  };
  auto guide_fit = [&](Vec3 raw, float width, float depth, float theta, float) {
    Vec3 p = fit(raw);
    float facing = std::cos(theta);
    float extent_x = (std::abs(std::cos(theta)) * width +
                      std::abs(std::sin(theta)) * depth) *
                     fit.scale.x,
          extent_z = (std::abs(std::sin(theta)) * width +
                      std::abs(std::cos(theta)) * depth) *
                     fit.scale.z;
    if (facing < .7f && std::abs(std::sin(theta)) > .4f) {
      float t = std::clamp((pivot.y + .05f - p.y) / .13f, 0.f, 1.f);
      p.x += (std::sin(theta) < 0 ? -1.f : 1.f) * .04f *
             std::sin(std::numbers::pi_v<float> * t);
      p.z -= .10f * t * t * (3 - 2 * t);
    }
    float back = 1e6f, front = -1e6f;
    for (float x : {p.x - extent_x, p.x, p.x + extent_x})
      for (float y : {p.y - .004f, p.y, p.y + .004f})
        if (auto z = body.at(x, y)) {
          back = std::min(back, (*z)[0]);
          front = std::max(front, (*z)[1]);
        }
    if (back <= front) {
      bool behind = facing < 0 || (facing < .7f && p.y < pivot.y + .025f);
      float margin = .006f + (p.y < pivot.y ? .03f * c.hair_length : 0.f);
      if (behind)
        p.z = std::min(p.z, back - extent_z - margin);
      else
        p.z = std::max(p.z, front + extent_z + margin);
    }
    return fit.inverse(p);
  };
  original_groom(
      {c.hair_length, c.hair_volume, c.fringe},
      [&](Vec3 p, std::array<float, 2> uv) {
        part.mesh.positions.push_back(p);
        part.mesh.uv.push_back(uv);
        weights.vertices.push_back({{head, 1}});
        return uint32_t(part.mesh.positions.size() - 1);
      },
      [&](uint32_t a, uint32_t b, uint32_t d) {
        part.mesh.indices.insert(part.mesh.indices.end(), {a, b, d});
      },
      fit, cap_fit, guide_fit);
  // Preserve each closed strand cross-section while resolving all its finite
  // width against the source envelope; per-vertex clamping would flatten tubes.
  for (size_t lock = 0; lock < 24; ++lock) {
    size_t start = 331 + lock * 72;
    for (size_t row = 0; row < 10; ++row) {
      float back = 1e6f, front = -1e6f, zmin = 1e6f, zmax = -1e6f;
      for (size_t side = 0; side < (row == 0 || row == 9 ? 8 : 7); ++side) {
        size_t vertex = side < 7 ? start + row * 7 + side
                                 : start + (row == 0 ? 71 : 70);
        auto p = part.mesh.positions[vertex];
        zmin = std::min(zmin, p.z);
        zmax = std::max(zmax, p.z);
        for (float x : {p.x - .008f, p.x, p.x + .008f})
          for (float y : {p.y - .03f, p.y - .008f, p.y,
                          p.y + .008f, p.y + .03f})
            if (auto z = body.at(x, y)) {
              back = std::min(back, (*z)[0]);
              front = std::max(front, (*z)[1]);
            }
      }
      constexpr float clearance = .010f;
      if (back <= front && zmax > back - clearance && zmin < front + clearance) {
        float negative = back - clearance - zmax,
              positive = front + clearance - zmin;
        float shift = std::abs(negative) < std::abs(positive) ? negative : positive;
        for (size_t side = 0; side < 7; ++side)
          part.mesh.positions[start + row * 7 + side].z += shift;
        if (row == 0)
          part.mesh.positions[start + 71].z += shift;
        if (row == 9)
          part.mesh.positions[start + 70].z += shift;
      }
    }
  }
  normals(part.mesh);
  part.mesh.validate();
  part.deformation = DeformationState(part.mesh);
  part.deformation.skeleton = s;
  part.deformation.skin = std::move(weights);
  part.deformation.humanoid = mapping;
  part.deformation.validate(part.mesh);
  part.source_maps = texture(palette(model));
  model.parts.push_back(std::move(part));
  model.notes.push_back(
      "Nonzero hair controls add an original strict-UV swept-lock groom fitted "
      "to native head/body envelopes; all authored source parts remain "
      "unchanged. The groom is head-rigid, with no spring simulation or "
      "arbitrary-pose collision guarantee.");
  if (cancel && cancel())
    throw std::runtime_error("Groom creation cancelled");
}
} // namespace dome::creator_detail
