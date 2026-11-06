// SPDX-License-Identifier: GPL-3.0-or-later
// Self-contained mixed39/40 appearance regression. The oracle uses authored
// fixture geometry and independent binary64 shading; it never calls the field
// evaluator, preview normal_at(), packed geometric_frames() or a product
// shader.
#include "dome/document.hpp"
#include "dome/material-visibility.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/polygon-authoring.hpp"
#include "dome/render.hpp"
#include "dome/source-placement.hpp"
#include "dome/texture-sets.hpp"
#include "graphics.h"
#include "polygon-surface-owner.hpp"
#include "polygon-surface-preview.hpp"
#include "texture-set-range.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <numbers>
using namespace dome;
namespace {
constexpr int side = 256;
constexpr double projection_scale = .6, shear = .125, rough = 128. / 255.;
size_t checks = 0;
void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
  ++checks;
  std::cout << "PASS " << message << '\n';
}
struct V {
  double x = 0, y = 0, z = 0;
  V operator+(V b) const { return {x + b.x, y + b.y, z + b.z}; }
  V operator-(V b) const { return {x - b.x, y - b.y, z - b.z}; }
  V operator*(double k) const { return {x * k, y * k, z * k}; }
};
double dot(V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V cross(V a, V b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
V unit(V a) {
  const auto n = std::sqrt(dot(a, a));
  if (!(n > 1e-12))
    throw std::runtime_error("Degenerate independent fixture basis");
  return a * (1 / n);
}
V value(Vec3 v) { return {v.x, v.y, v.z}; }
Vec3 vec(V v) { return {float(v.x), float(v.y), float(v.z)}; }
// Author complementary front/back normal fixtures so a correctly flipped face
// remains lit by the application's fixed lights. Otherwise a black back face
// cannot distinguish correct original-UV sampling from a wrong sampling grid.
V artist_for(bool flip) {
  return flip ? unit({.15, -.75, .65}) : unit({-.15, .75, .65});
}
const std::array<V, 4> authored{
    {{-.5, -.5, 0}, {.5, -.5, 0}, {.5, .5, 0}, {-.5, .5, 0}}};
const std::array<std::array<double, 2>, 4> atlas{
    {{{.1, .1}}, {{.9, .1}}, {{.9, .9}}, {{.1, .9}}}};
const std::array<std::array<unsigned, 3>, 2> faces{{{{0, 1, 2}}, {{0, 2, 3}}}};
V direction(V p, bool reflected) {
  return {(reflected ? -1. : 1.) * p.x + shear * p.y, p.y, p.z};
}
V position(V p, bool reflected, bool sets) {
  return direction(p, reflected) + V{sets ? .85 : -.85, 0, 0};
}
V normal(V n, bool reflected) {
  const double sign = reflected ? -1 : 1;
  return unit({sign * n.x, n.y - sign * shear * n.x, n.z});
}
struct Basis {
  V t, b, n;
};
Basis basis(const std::array<V, 3> &p,
            const std::array<std::array<double, 2>, 3> &uv, bool reflected) {
  const auto a = p[1] - p[0], b = p[2] - p[0];
  const double u = uv[1][0] - uv[0][0], v = uv[1][1] - uv[0][1],
               s = uv[2][0] - uv[0][0], t = uv[2][1] - uv[0][1],
               det = u * t - s * v;
  if (std::abs(det) < 1e-8)
    throw std::runtime_error("Degenerate independent fixture chart");
  const auto n = normal(unit(cross(a, b)), reflected),
             raw = direction((a * t - b * v) * (1 / det), reflected),
             raw_b = direction((b * u - a * s) * (1 / det), reflected);
  const auto T = unit(raw - n * dot(n, raw)), B = unit(cross(n, T));
  return {T, B * (dot(B, raw_b) < 0 ? -1 : 1), n};
}
std::array<unsigned, 3> order(size_t triangle, bool flip) {
  auto o = faces.at(triangle);
  if (flip)
    std::reverse(o.begin(), o.end());
  return o;
}
std::array<std::array<double, 2>, 3> chart(size_t triangle, bool prepared,
                                           bool flip) {
  std::array<std::array<double, 2>, 3> result{};
  const auto ids = order(triangle, flip);
  for (size_t c = 0; c < 3; ++c) {
    if (prepared)
      result[c] = atlas[ids[c]];
    else {
      size_t k = 0;
      while (faces[triangle][k] != ids[c])
        ++k;
      result[c] = k == 0   ? std::array<double, 2>{0, 0}
                  : k == 1 ? std::array<double, 2>{1, 0}
                           : std::array<double, 2>{0, 1};
    }
  }
  return result;
}
// The historical normal image is deliberately asymmetric in BOTH UV axes.
// Conversion normalizes/quantizes its unit columns. Reproduce that specified
// encoding independently, then bilinearly filter it in original normal UVs.
V encoded_texel(int x, int y) {
  x = std::clamp(x, 0, 63);
  y = std::clamp(y, 0, 63);
  const V raw{(95 + (x / 16) * 18) / 255. * 2 - 1,
              (110 + (y / 32) * 30) / 255. * 2 - 1, 240 / 255. * 2 - 1};
  const auto q = unit(raw);
  auto encode = [](double n) {
    return std::clamp(std::lround((n * .5 + .5) * 255), 0l, 255l) / 255. * 2 -
           1;
  };
  return {encode(q.x), encode(q.y), encode(q.z)};
}
V sample(std::array<double, 2> uv) {
  const auto x = uv[0] * 64 - .5, y = uv[1] * 64 - .5;
  const int a = int(std::floor(x)), b = int(std::floor(y));
  const auto fx = x - a, fy = y - b;
  return unit(
      (encoded_texel(a, b) * (1 - fx) + encoded_texel(a + 1, b) * fx) *
          (1 - fy) +
      (encoded_texel(a, b + 1) * (1 - fx) + encoded_texel(a + 1, b + 1) * fx) *
          fy);
}
V expected_normal(size_t triangle, std::array<double, 3> w, bool prepared,
                  bool reflected, bool flip, bool wrong_uv = false,
                  bool geometric_only = false) {
  const auto artist = artist_for(flip);
  const auto ids = order(triangle, flip);
  std::array<V, 3> old{}, now{};
  for (size_t k = 0; k < 3; ++k) {
    old[k] = authored[faces[triangle][k]];
    now[k] = authored[ids[k]];
    if (ids[k] == 2)
      now[k].z += .45;
  }
  const auto a = basis(old, chart(triangle, prepared, false), reflected),
             b = basis(now, chart(triangle, prepared, flip), reflected);
  V desired = normal(artist, reflected);
  if (prepared) {
    std::array<double, 2> uv{};
    for (size_t k = 0; k < 3; ++k)
      for (size_t d = 0; d < 2; ++d)
        uv[d] += w[k] * atlas[ids[k]][d];
    if (wrong_uv)
      for (auto &v : uv)
        v = .5 + .8 * (v - .5);
    const auto q = sample(uv);
    const V local_t = unit(V{1, 0, 0} - artist * artist.x);
    const auto t = unit(direction(local_t, reflected)),
               bitangent = unit(cross(desired, t)) * (reflected ? -1 : 1);
    desired = unit(t * q.x + bitangent * q.y + desired * q.z);
  }
  const auto result =
      geometric_only ? b.n
                     : unit(b.t * dot(a.t, desired) + b.b * dot(a.b, desired) +
                            b.n * dot(a.n, desired));
  return result * (flip ? -1 : 1);
}
V brdf(V n, V view, V light, V energy) {
  const auto h = unit(view + light);
  const auto nv = std::max(dot(n, view), .0001),
             nl = std::max(dot(n, light), 0.), nh = std::max(dot(n, h), 0.),
             vh = std::max(dot(view, h), 0.);
  const auto a = rough * rough, aa = a * a;
  const auto D = aa / (std::numbers::pi * std::pow(nh * nh * (aa - 1) + 1, 2)),
             k = std::pow(rough + 1, 2) / 8,
             G = nv / (nv * (1 - k) + k) * nl / (nl * (1 - k) + k),
             F = .04 + .96 * std::pow(1 - vh, 5);
  return energy * (D * G * F / std::max(4 * nv * nl, .0001) * nl);
}
std::array<int, 4> shade(V n, V point) {
  const auto view = unit(V{0, 0, 4} - point);
  const auto c = brdf(n, view, unit({-.4, .8, .7}), {3.8, 3.8, 3.8}) +
                 brdf(n, view, unit({.7, .2, -.4}), {.65, .8, 1});
  auto byte = [](double x) {
    x = x / (1 + x);
    return int(std::lround(std::clamp(x <= .0031308
                                          ? 12.92 * x
                                          : 1.055 * std::pow(x, 1 / 2.4) - .055,
                                      0., 1.) *
                           255));
  };
  return {byte(c.x), byte(c.y), byte(c.z), 255};
}
Image solid(std::array<uint8_t, 4> p) {
  Image out{64, 64, std::vector<uint8_t>(64 * 64 * 4)};
  for (size_t i = 0; i < 64 * 64; ++i)
    std::copy(p.begin(), p.end(), out.rgba.begin() + i * 4);
  return out;
}
Mesh quad(bool flip = false) {
  const auto artist = artist_for(flip);
  Mesh m;
  for (auto p : authored)
    m.positions.push_back(vec(p));
  m.normals.assign(4, vec(artist));
  for (auto uv : atlas)
    m.uv.push_back({float(uv[0]), float(uv[1])});
  m.indices = {0, 1, 2, 0, 2, 3};
  return m;
}
void ready(Document &d) {
  if (!d.wait_asset(std::chrono::seconds(120)) ||
      !d.wait_mesh(std::chrono::seconds(120)) ||
      !d.frame().state->error.empty())
    throw std::runtime_error("Actual mixed worker: " + d.frame().state->error);
}
void placement(SculptObject &o, bool reflected, bool sets) {
  auto m = AffineTransform{}.matrix();
  m[0] = reflected ? -1 : 1;
  m[4] = shear;
  m[12] = sets ? .85 : -.85;
  o.affine_edit = std::make_shared<const NativeAffineEdit>(AffineTransform(m));
}
struct Fixture {
  Scene scene;
  uint64_t field = 0, sets = 0;
};
Fixture fixture(bool prepared, bool reflected, bool flip) {
  Scene initial;
  auto &o = initial.active();
  const auto field = o.id;
  o.mesh_active = true;
  o.material.color = {0, 0, 0, 1};
  o.material.emissive = {0, 0, 0};
  o.material.metallic = 0;
  o.material.roughness = .5f;
  o.material.alpha = Material::AlphaMode::Opaque;
  o.material.double_sided = true;
  o.painted = std::make_shared<PaintedMesh>(quad(flip), 64);
  if (prepared) {
    BakedTextures bake;
    bake.maps.color = solid({0, 0, 0, 255});
    bake.maps.metal_rough = solid({255, 128, 0, 255});
    bake.maps.emission = solid({0, 0, 0, 255});
    bake.maps.normal = solid({128, 128, 255, 255});
    bake.coverage.assign(64 * 64, 255);
    for (int y = 0; y < 64; ++y)
      for (int x = 0; x < 64; ++x) {
        const auto n = size_t(y * 64 + x) * 4;
        bake.maps.normal.rgba[n] = uint8_t(95 + (x / 16) * 18);
        bake.maps.normal.rgba[n + 1] = uint8_t(110 + (y / 32) * 30);
        bake.maps.normal.rgba[n + 2] = 240;
      }
    o.painted->install_bake(std::move(bake));
    auto converted =
        convert_canonical_normals({o.painted, {}, {}, {}, o.material, {}});
    o.painted = std::move(converted.paint);
    o.material_bindings = std::move(converted.material_bindings);
  }
  // A real black-radiance stroke owns FLOAT coverage without adding a glow
  // term to the independent black-albedo/specular normal oracle.
  PaintSettings ink;
  ink.front_only = false;
  ink.channel = PaintChannel::Emission;
  ink.emission_units = PaintEmissionUnits::Physical;
  ink.emission = {0, 0, 0};
  Brush brush;
  brush.from = brush.to = {0, 0, 0};
  brush.normal = {0, 0, 1};
  brush.radius = 2;
  brush.strength = 1;
  brush.pressure = 1;
  o.painted->begin_stroke();
  o.painted->stamp(brush, ink);
  check(o.painted->end_stroke() && o.painted->has_physical_emission(),
        "Actual black-radiance gesture owns FLOAT paint and coverage");
  if (prepared) {
    Document uv;
    uv.replace(initial);
    ready(uv);
    uv.begin_uv();
    check(uv.wait_uv(), "Real canonical UV workspace is ready");
    uv.edit_uv(UvAction::Scale, .8f);
    check(uv.wait_uv(), "Real canonical current UV edit completes");
    uv.end_uv(true);
    ready(uv);
    initial = uv.copy_scene();
    check(bool(initial.active().material_bindings->canonical_paint_sampling),
          "Original normal and paint grids survive a genuinely different "
          "current UV atlas");
  }
  placement(initial.active(), reflected, false);
  initial.validate();
  const auto before = initial.snapshot();
  const auto owner = detail::PolygonSurfaceOwner::capture(before, field);
  PolygonWorkspace work(*owner->trace().original());
  work.mode(PolygonSelectionMode::Vertex);
  const auto selected =
      std::find_if(work.view().topology->vertices().begin(),
                   work.view().topology->vertices().end(), [](const auto &v) {
                     return length(v.position - Vec3{.5f, .5f, 0}) < 1e-6;
                   });
  check(selected != work.view().topology->vertices().end(),
        "Real topology identifies the independently authored moved vertex");
  work.select(selected->id);
  work.begin_move();
  work.move_to({0, 0, .45f});
  work.end_move(true);
  if (flip) {
    work.mode(PolygonSelectionMode::Face);
    work.select_all();
    work.apply(PolygonOperation::FlipFaces);
  }
  check(detail::adopt_polygon_surface_edit(initial, field, work.view(), before),
        "Real complete Move/Flip replay adopts its actual39 field owner");
  const auto sets =
      initial.add("Balanced primary and additional texture fields", Volume{});
  std::array<TextureSetInitialField, 2> fields;
  for (auto &f : fields) {
    f.resolution = 64;
    f.material.unlit = true;
    f.material.alpha = Material::AlphaMode::Opaque;
    f.material.double_sided = true;
  }
  fields[0].material.name = "Primary red";
  fields[0].material.color = {1, 0, 0, 1};
  fields[1].material.name = "Additional green";
  fields[1].material.color = {0, 1, 0, 1};
  initialize_texture_sets(initial, sets, quad(), fields,
                          std::array<TextureSetId, 2>{0, 1});
  select_texture_set(initial, sets, 1);
  placement(initial.object(sets), reflected, true);
  initial.select(field);
  initial.validate();
  const auto wire = initial.encode();
  check(wire.at("version") == 40 && wire.contains("polygon_surfaces") &&
            wire.contains("texture_set_geometry"),
        "Actual mixed Scene40 owns both39 and40 feature graphs");
  auto scene = Scene::decode(wire);
  check(scene.encode() == wire,
        "Mixed original grids, source roots and independent fields survive "
        "actual codec replay");
  return {std::move(scene), field, sets};
}
struct Triangle {
  std::array<V, 3> p;
  std::array<unsigned, 3> ids;
  uint64_t object;
  TextureSetId set;
  size_t face;
  bool field;
};
std::vector<Triangle> expected(const Fixture &f, bool reflected, bool flip) {
  std::vector<Triangle> out;
  for (bool field : {true, false})
    for (size_t t = 0; t < 2; ++t) {
      const auto ids = order(t, field && flip);
      Triangle r;
      r.ids = ids;
      r.object = field ? f.field : f.sets;
      r.set = field ? 0 : TextureSetId(t);
      r.face = t;
      r.field = field;
      for (size_t c = 0; c < 3; ++c) {
        auto p = authored[ids[c]];
        if (field && ids[c] == 2)
          p.z += .45;
        r.p[c] = position(p, reflected, !field);
      }
      out.push_back(r);
    }
  return out;
}
std::array<double, 3> barycentric(const Triangle &t, double x, double y) {
  const auto &a = t.p[0], &b = t.p[1], &c = t.p[2];
  const auto den = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
  const auto u = ((b.y - c.y) * (x - c.x) + (c.x - b.x) * (y - c.y)) / den,
             v = ((c.y - a.y) * (x - c.x) + (a.x - c.x) * (y - c.y)) / den;
  return {u, v, 1 - u - v};
}
struct Fbo {
  GLuint f = 0, c = 0, d = 0;
  Fbo() {
    glGenFramebuffers(1, &f);
    glBindFramebuffer(GL_FRAMEBUFFER, f);
    glGenTextures(1, &c);
    glBindTexture(GL_TEXTURE_2D, c);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, side, side, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           c, 0);
    glGenRenderbuffers(1, &d);
    glBindRenderbuffer(GL_RENDERBUFFER, d);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, side, side);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, d);
    check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
          "Independent mixed appearance framebuffer is complete");
  }
  ~Fbo() {
    glDeleteRenderbuffers(1, &d);
    glDeleteTextures(1, &c);
    glDeleteFramebuffers(1, &f);
  }
  void bind() { glBindFramebuffer(GL_FRAMEBUFFER, f); }
};
std::vector<uint8_t> render(SurfaceRenderer &r,
                            std::shared_ptr<const MeshSnapshot> s, Fbo &f) {
  f.bind();
  Mat4 vp{float(projection_scale),
          0,
          0,
          0,
          0,
          float(projection_scale),
          0,
          0,
          0,
          0,
          -.2f,
          0,
          0,
          0,
          0,
          1};
  r.draw(std::move(s), {}, vp, {0, 0, 4}, side, side, true);
  std::vector<uint8_t> p(side * side * 4);
  glReadPixels(0, 0, side, side, GL_RGBA, GL_UNSIGNED_BYTE, p.data());
  if (glGetError() != GL_NO_ERROR)
    throw std::runtime_error("Actual mixed appearance draw/read failed");
  return p;
}
void write_png(const std::filesystem::path &p,
               const std::vector<uint8_t> &pixels) {
  auto b = Image{side, side, pixels}.png();
  std::ofstream f(p, std::ios::binary);
  f.write(reinterpret_cast<const char *>(b.data()), b.size());
}
void verify_streams(const Fixture &f, const MeshSnapshot &s,
                    const std::vector<Triangle> &wanted, bool prepared,
                    bool reflected, bool flip) {
  const auto artist = artist_for(flip);
  check(s.ranges.size() == 3 && s.picking,
        "Mixed native producer publishes one39 and both genuine40 draw ranges");
  size_t primary = 0, additional = 0, field = 0;
  for (const auto &r : s.ranges) {
    const bool is_field = r.object == f.field;
    check(r.paint_winding_reversed == reflected,
          "Every packed range retains its actual reflected-winding witness");
    if (is_field) {
      ++field;
      check(r.textures && r.textures->polygon_surface &&
                r.textures->polygon_surface->matches(
                    r.material_bindings, r.paint_sampling_attachment),
            "Actual39 frame retains its exact field/material/paint owner");
      check(r.textures->polygon_surface->model() ==
                (prepared ? detail::PolygonSurfaceNormalModel::PreparedCorePbr
                          : detail::PolygonSurfaceNormalModel::NativeNOnly),
            "Normal model comes from actual before N/T ownership");
    } else {
      check(r.object == f.sets && r.texture_set_proof,
            "Actual40 range retains its distinct object/set proof");
      r.texture_set_proof->authenticate_stream(s.mesh, r);
      if (r.texture_set == 0)
        ++primary;
      else if (r.texture_set == 1)
        ++additional;
      else
        throw std::runtime_error("Foreign material slot in actual mixed frame");
    }
    for (size_t c = 0; c < r.count; c += 3) {
      std::array<Vec3, 3> p;
      for (size_t k = 0; k < 3; ++k)
        p[k] = s.mesh.positions.at(s.mesh.indices.at(r.first + c + k));
      const Triangle *e = nullptr;
      for (const auto &candidate : wanted) {
        if (candidate.object != r.object || candidate.set != r.texture_set)
          continue;
        bool match = true;
        for (auto v : p)
          match &=
              std::any_of(candidate.p.begin(), candidate.p.end(),
                          [&](auto q) { return length(v - vec(q)) < 2e-5; });
        if (match) {
          check(e == nullptr, "Every actual triangle has one independent "
                              "authored geometry identity");
          e = &candidate;
        }
      }
      check(e != nullptr, "Actual geometry matches its independent "
                          "moved/placed authored triangle");
      const auto area =
          cross(value(p[1]) - value(p[0]), value(p[2]) - value(p[0])).z;
      check((area > 0) == (!is_field || !flip),
            "Actual front/back winding follows the authored Flip plus "
            "reflected EBO compensation");
      for (size_t k = 0; k < 3; ++k) {
        size_t expected_corner = 0;
        while (length(p[k] - vec(e->p[expected_corner])) > 2e-5)
          ++expected_corner;
        const auto id = e->ids[expected_corner],
                   v = s.mesh.indices.at(r.first + c + k);
        if (is_field) {
          check(length(s.source_normals.at(v) -
                       vec(normal(artist, reflected))) < 2e-5,
                "Packed before N is the actual authored inverse-transpose "
                "normal");
          const std::array<float, 2> old_uv{float(atlas[id][0]),
                                            float(atlas[id][1])};
          check(s.paint_coordinates.at(v) == old_uv,
                "Paint keeps its actual original grid across Move/Flip and "
                "independent geometry UVs");
          if (prepared) {
            const auto n = unit(V{1, 0, 0} - artist * artist.x),
                       t = unit(direction(n, reflected));
            const auto raw = s.source_tangents.at(v);
            check(length(Vec3{raw[0], raw[1], raw[2]} - vec(t)) < 2e-5 &&
                      raw[3] == (reflected ? -1 : 1),
                  "Prepared39 range retains genuine before T and "
                  "authored/reflected W");
            check(s.normal_coordinates.at(v) ==
                          std::array<float, 4>{old_uv[0], old_uv[1], old_uv[0],
                                               old_uv[1]} &&
                      s.mesh.uv.at(v) != old_uv,
                  "Normal sampling uses its original role UV rather than the "
                  "genuinely edited current atlas");
          }
        } else
          check(s.mesh.uv.at(v) == std::array<float, 2>{float(atlas[id][0]),
                                                        float(atlas[id][1])},
                "Both40 materials retain the original logical-corner UVs");
      }
      const auto center = (value(p[0]) + value(p[1]) + value(p[2])) * (1. / 3);
      const auto hit = s.picking->raycast(vec(center + V{0, 0, 2}), {0, 0, -1},
                                          3, 0, r.object);
      check(
          hit && s.picking->material_identity(hit->triangle) ==
                     SurfaceMaterialIdentity{r.object, r.texture_set},
          "Independent centroid ray identifies the actual field/set material");
      if (!is_field)
        check(s.picking->logical_triangle(hit->triangle) == e->face,
              "Primary/additional query triangles retain their original "
              "logical face identity");
    }
  }
  check(field == 1 && primary == 1 && additional == 1,
        "Active additional set does not replace the primary range or the "
        "separate39 actor");
}
void verify_pixels(const std::vector<uint8_t> &pixels,
                   const std::vector<Triangle> &triangles, bool prepared,
                   bool reflected, bool flip) {
  std::map<std::pair<uint64_t, TextureSetId>, size_t> counts;
  size_t outside = 0;
  int max_error = 0, normal_counter = 0, uv_counter = 0;
  for (int y = 0; y < side; ++y)
    for (int x = 0; x < side; ++x) {
      const double wx = ((x + .5) * 2 / side - 1) / projection_scale,
                   wy = ((y + .5) * 2 / side - 1) / projection_scale;
      const auto off = size_t(y * side + x) * 4;
      bool near = false, interior = false;
      for (const auto &t : triangles) {
        const auto b = barycentric(t, wx, wy);
        const auto min = *std::min_element(b.begin(), b.end());
        if (min >= -.04)
          near = true;
        if (min < .12)
          continue;
        if (interior)
          throw std::runtime_error("Independent actor interiors overlap");
        interior = true;
        ++counts[{t.object, t.set}];
        V point{};
        for (size_t k = 0; k < 3; ++k)
          point = point + t.p[k] * b[k];
        std::array<int, 4> wanted{};
        if (t.field) {
          wanted = shade(expected_normal(t.face, b, prepared, reflected, flip),
                         point);
          const auto wrong =
              shade(expected_normal(t.face, b, prepared, reflected, flip, false,
                                    true),
                    point);
          for (size_t k = 0; k < 3; ++k)
            normal_counter =
                std::max(normal_counter, std::abs(wanted[k] - wrong[k]));
          if (prepared) {
            const auto wrong_grid = shade(
                expected_normal(t.face, b, prepared, reflected, flip, true),
                point);
            for (size_t k = 0; k < 3; ++k)
              uv_counter =
                  std::max(uv_counter, std::abs(wanted[k] - wrong_grid[k]));
          }
        } else
          wanted = t.set == 0 ? std::array<int, 4>{255, 0, 0, 255}
                              : std::array<int, 4>{0, 255, 0, 255};
        for (size_t k = 0; k < 4; ++k) {
          const auto error = std::abs(int(pixels[off + k]) - wanted[k]);
          max_error = std::max(max_error, error);
          if (error > (t.field ? 4 : 1))
            throw std::runtime_error(
                "Independent mixed pixel mismatch at " + std::to_string(x) +
                "," + std::to_string(y) + " component=" + std::to_string(k) +
                " actual=" + std::to_string(pixels[off + k]) +
                " expected=" + std::to_string(wanted[k]));
        }
      }
      if (!near) {
        ++outside;
        if (pixels[off + 3])
          throw std::runtime_error("Unexpected mixed silhouette outside "
                                   "independently authored geometry");
      }
    }
  check(counts.size() == 3,
        "Each39 actor and both40 slots have their own measured pixel mask");
  size_t field_pixels = 0, set_pixels = 0;
  for (const auto &t : triangles)
    if (t.field)
      field_pixels = counts[{t.object, t.set}];
    else
      set_pixels = counts[{t.object, 0}] + counts[{t.object, 1}];
  check(field_pixels > 800 && set_pixels > 800 &&
            double(field_pixels) / set_pixels > .6 &&
            double(field_pixels) / set_pixels < 1.7,
        "Both actors occupy comparable independently verified screen areas");
  for (auto [id, count] : counts)
    check(count > 400, "Each actual primary/additional material owns a "
                       "substantial distinct pixel interior");
  check(outside > side * side / 2, "Independent silhouette oracle checks "
                                   "substantial transparent background");
  check(normal_counter >= 6,
        "Independent appearance oracle would detect replacing retained N/T "
        "with edited geometric normals");
  if (prepared)
    check(uv_counter >= 6,
          "Independent asymmetric image oracle would detect sampling the "
          "edited atlas instead of original normal UVs");
  std::cout << "ORACLE max_byte_error=" << max_error
            << " normal_counter=" << normal_counter
            << " uv_counter=" << uv_counter << " field_pixels=" << field_pixels
            << " sets_pixels=" << set_pixels << '\n';
}
} // namespace
int main(int argc, char **argv) try {
  if (argc != 2)
    return 2;
  const std::filesystem::path out = argv[1];
  std::filesystem::create_directories(out);
  NativeGraphics graphics;
  std::cout << "GL_RENDERER " << glGetString(GL_RENDERER) << '\n';
  Fbo fbo;
  SurfaceRenderer renderer;
  for (bool prepared : {false, true})
    for (bool reflected : {false, true})
      for (bool flip : {false, true}) {
        const std::string name = std::to_string(prepared) +
                                 std::to_string(reflected) +
                                 std::to_string(flip);
        auto f = fixture(prepared, reflected, flip);
        const auto exact = f.scene.encode();
        Document doc;
        doc.replace(f.scene);
        ready(doc);
        const auto frame = doc.frame().surface;
        const auto reference = expected(f, reflected, flip);
        verify_streams(f, *frame, reference, prepared, reflected, flip);
        const auto pixels = render(renderer, frame, fbo);
        write_png(out / (name + ".png"), pixels);
        verify_pixels(pixels, reference, prepared, reflected, flip);
        const auto stats = renderer.texture_stats();
        const auto buffer = renderer.buffer_stats();
        check(render(renderer, frame, fbo) == pixels &&
                  renderer.texture_stats().uploads == stats.uploads &&
                  renderer.texture_stats().pack_builds == stats.pack_builds &&
                  renderer.buffer_stats().geometry_uploads ==
                      buffer.geometry_uploads,
              "Second eye reuses exact mixed appearance without another field "
              "or geometry upload");
        check(doc.copy_scene().encode() == exact,
              "Actual mixed appearance, queries and both material sets retain "
              "complete editable Scene state");
      }
  std::cout << "PASS " << checks
            << " balanced mixed39/40 appearance controls\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
