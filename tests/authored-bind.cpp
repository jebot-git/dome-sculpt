// SPDX-License-Identifier: GPL-3.0-or-later
#include "affine-matrix.hpp"
#include "dome/import.hpp"
#include "dome/material-bindings.hpp"
#include "dome/scene.hpp"
#include "normal-frame-runtime.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <unistd.h>
using namespace dome;
namespace {
unsigned checks = 0;
void check(bool value, const char *text) {
  if (!value)
    throw std::runtime_error(text);
  ++checks;
  std::cout << "PASS " << text << std::endl;
}
constexpr RigMatrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
RigMatrix product(const RigMatrix &a, const RigMatrix &b) {
  RigMatrix out{};
  for (int c = 0; c < 4; ++c)
    for (int r = 0; r < 4; ++r) {
      double total = 0;
      for (int k = 0; k < 4; ++k)
        total += double(a[k * 4 + r]) * b[c * 4 + k];
      out[c * 4 + r] = float(total);
    }
  return out;
}
Vec3 point(const RigMatrix &m, Vec3 p) {
  return {float(double(m[0]) * p.x + double(m[4]) * p.y + double(m[8]) * p.z +
                m[12]),
          float(double(m[1]) * p.x + double(m[5]) * p.y + double(m[9]) * p.z +
                m[13]),
          float(double(m[2]) * p.x + double(m[6]) * p.y + double(m[10]) * p.z +
                m[14])};
}
Vec3 direction(const RigMatrix &m, Vec3 p) {
  auto copy = m;
  copy[12] = copy[13] = copy[14] = 0;
  return point(copy, p);
}
RigMatrix normal(const RigMatrix &m) {
  double a[3][6]{};
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c)
      a[r][c] = m[c * 4 + r];
    a[r][r + 3] = 1;
  }
  for (int c = 0; c < 3; ++c) {
    int pivot = c;
    for (int r = c + 1; r < 3; ++r)
      if (std::abs(a[r][c]) > std::abs(a[pivot][c]))
        pivot = r;
    if (std::abs(a[pivot][c]) < 1e-12)
      throw std::runtime_error("Oracle singular matrix");
    for (int k = 0; k < 6; ++k)
      std::swap(a[c][k], a[pivot][k]);
    double scale = a[c][c];
    for (double &v : a[c])
      v /= scale;
    for (int r = 0; r < 3; ++r)
      if (r != c) {
        double f = a[r][c];
        for (int k = 0; k < 6; ++k)
          a[r][k] -= f * a[c][k];
      }
  }
  auto out = identity;
  for (int c = 0; c < 3; ++c)
    for (int r = 0; r < 3; ++r)
      out[c * 4 + r] = float(a[c][r + 3]);
  return out;
}
bool close(Vec3 a, Vec3 b) { return length(a - b) < 4e-6f; }
Mesh triangle() {
  Mesh m;
  m.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
  m.normals.assign(3, {0, 0, 1});
  m.uv = {{0, 0}, {1, 0}, {0, 1}};
  m.indices = {0, 1, 2};
  return m;
}
template <class F> bool refuses(F &&f) {
  try {
    f();
    return false;
  } catch (const std::exception &) {
    return true;
  }
}
} // namespace
int main() try {
  auto large_shear = identity;
  large_shear[0] = large_shear[5] = large_shear[10] = 1e25f;
  large_shear[4] = 1e24f;
  check(!detail::gltf_trs_matrix(large_shear),
        "Representability guards reject sheared finite matrices even beyond "
        "float dot-product range");
  auto mesh = triangle();
  Skeleton rig;
  Bone root;
  root.name = "root";
  root.rest_matrix = identity;
  (*root.rest_matrix)[0] = 2;
  (*root.rest_matrix)[5] = 1.25f;
  (*root.rest_matrix)[10] = .5f;
  (*root.rest_matrix)[12] = .4f;
  Bone child;
  child.name = "reflected child";
  child.parent = 0;
  child.rest.translation = {.2f, .1f, 0};
  child.rest.scale = {-1, .75f, 1.5f};
  rig.bones = {root, child};
  auto space = identity;
  space[12] = -.3f;
  space[13] = .2f;
  rig.space_matrix = space;
  auto first = identity;
  first[0] = .7f;
  first[5] = 1.2f;
  first[12] = .15f;
  auto second = identity;
  second[8] = .3f;
  second[13] = -.25f;
  rig.inverse_bind_override = std::vector<RigMatrix>{first, second};
  rig.validate();
  auto root_world = product(space, *root.rest_matrix);
  auto child_local = identity;
  child_local[0] = -1;
  child_local[5] = .75f;
  child_local[10] = 1.5f;
  child_local[12] = .2f;
  child_local[13] = .1f;
  auto child_world = product(root_world, child_local);
  std::array<RigMatrix, 2> expected{product(root_world, first),
                                    product(child_world, second)};
  check(rig.inverse_bind() == *rig.inverse_bind_override,
        "Authored nonconventional inverse binds remain exact rather than being "
        "inferred from rest hierarchy");
  auto matrices = rig.skin_matrices({});
  for (size_t b = 0; b < 2; ++b)
    for (size_t k = 0; k < 16; ++k)
      if (std::abs(matrices[b][k] - expected[b][k]) > 2e-6f)
        throw std::runtime_error(
            "Joint*authoredBind differs from independent matrix arithmetic");
  check(true, "Exact root/rest authority and reflected nonuniform joints use "
              "the original glTF matrix product at rest");
  DeformationState state(mesh);
  state.skeleton = rig;
  state.skin = SkinWeights{};
  state.skin->vertices = {{{0, 1}}, {{1, 1}}, {{0, .3f}, {1, .7f}}};
  auto posed = state.preview(mesh);
  for (size_t v = 0; v < 3; ++v) {
    Vec3 p{};
    for (auto influence : state.skin->vertices[v])
      p += point(expected[influence.joint], mesh.positions[v]) *
           influence.weight;
    if (!close(posed.positions[v], p))
      throw std::runtime_error(
          "Empty-pose imported skin was not evaluated at authored rest");
  }
  check(true, "Imported skin evaluates even with an empty pose when its "
              "authored bind space is nonidentity");
  auto implicit = rig;
  implicit.inverse_bind_override = std::vector<RigMatrix>(2, identity);
  state.skeleton = implicit;
  auto implicit_pose = state.preview(mesh);
  check(
      close(implicit_pose.positions[0], point(root_world, mesh.positions[0])) &&
          close(implicit_pose.positions[1],
                point(child_world, mesh.positions[1])),
      "Missing glTF inverse-bind accessor can use explicit identities, not "
      "inverse(rest)");
  state.skeleton = rig;
  auto frame = std::make_shared<NormalFrames>();
  frame->origin_vertex_count = frame->source_vertex_count =
      frame->source_corner_count = 3;
  frame->origin_vertices = frame->source_vertices =
      frame->source_corners = {0, 1, 2};
  frame->normals.assign(3, {0, 0, 1});
  frame->tangents.assign(3, {1, 0, 0, 1});
  NormalFrameSet frames;
  frames.pbr = frame;
  detail::NormalFrameLayout layout{{0, 1, 2}, {0, 1, 2}, {0, 1, 2}};
  auto preview = detail::normal_frame_preview(frames, layout, &state, {}, 1);
  for (size_t v = 0; v < 3; ++v) {
    Vec3 N{}, T{};
    for (auto i : state.skin->vertices[v]) {
      N += direction(normal(expected[i.joint]), {0, 0, 1}) * i.weight;
      T += direction(expected[i.joint], {1, 0, 0}) * i.weight;
    }
    auto t = preview.tangents[0][v];
    if (!close(preview.normals[0][v], N) || !close({t[0], t[1], t[2]}, T) ||
        t[3] != 1)
      throw std::runtime_error("Normal/tangent bind transport differs from "
                               "independent inverse transpose oracle");
  }
  check(true, "Rest original N/T transport uses the same authored bind "
              "matrices and declared weighted-joint inverse-transpose policy");
  state.pose = {root.rest, child.rest};
  state.pose[0].translation = {.1f, -.2f, .05f};
  state.pose[0].rotation = {float(std::cos(.2)), 0, 0, float(std::sin(.2))};
  auto change = identity;
  change[0] = change[5] = float(std::cos(.4));
  change[1] = float(std::sin(.4));
  change[4] = -change[1];
  change[12] = .1f;
  change[13] = -.2f;
  change[14] = .05f;
  auto moved_root = product(space, product(change, *root.rest_matrix));
  std::array<RigMatrix, 2> moved{
      product(moved_root, first),
      product(product(moved_root, child_local), second)};
  auto moved_mesh = state.preview(mesh);
  for (size_t v = 0; v < 3; ++v) {
    Vec3 p{};
    for (auto i : state.skin->vertices[v])
      p += point(moved[i.joint], mesh.positions[v]) * i.weight;
    if (!close(moved_mesh.positions[v], p))
      throw std::runtime_error(
          "Editable TRS pose lost exact imported rest/root authority");
  }
  check(true, "Editable local TRS poses apply relative to exact imported rest "
              "matrices without replacing their authority");
  auto moved_frames =
      detail::normal_frame_preview(frames, layout, &state, {}, 1);
  for (size_t v = 0; v < 3; ++v) {
    Vec3 N{}, T{};
    for (auto i : state.skin->vertices[v]) {
      N += direction(normal(moved[i.joint]), {0, 0, 1}) * i.weight;
      T += direction(moved[i.joint], {1, 0, 0}) * i.weight;
    }
    auto t = moved_frames.tangents[0][v];
    if (!close(moved_frames.normals[0][v], N) || !close({t[0], t[1], t[2]}, T))
      throw std::runtime_error(
          "Edited pose N/T matrices diverge from positions");
  }
  check(true, "Edited pose normals and tangents follow the same independently "
              "evaluated bind-space matrices as positions");
  state.pose.clear();
  auto restored = state.preview(mesh);
  for (size_t v = 0; v < 3; ++v)
    if (!close(restored.positions[v], posed.positions[v]))
      throw std::runtime_error(
          "Clearing native pose failed to restore imported current pose");
  check(*state.skeleton->inverse_bind_override == *rig.inverse_bind_override,
        "Resetting an editable pose restores original imported state without "
        "rewriting inverse-bind bytes");
  auto decoded = Skeleton::decode(rig.encode());
  check(decoded.encode() == rig.encode() &&
            decoded.inverse_bind() == rig.inverse_bind(),
        "Skeleton2 exact rest/root/inverse-bind metadata survives an exact "
        "codec roundtrip");
  auto invalid = rig.encode();
  invalid["version"] = 1;
  check(refuses([&] { Skeleton::decode(invalid); }),
        "Legacy Skeleton1 refuses hidden new bind fields instead of "
        "misinterpreting them");
  invalid = rig.encode();
  invalid["inverse_bind_override"][0][3] = .1;
  check(refuses([&] { Skeleton::decode(invalid); }),
        "Nonaffine authored inverse binds are rejected explicitly");
  invalid = rig.encode();
  invalid["inverse_bind_override"].push_back(identity);
  check(refuses([&] { Skeleton::decode(invalid); }),
        "Bone-aligned inverse-bind counts are bounded and exact");
  Scene scene;
  auto &o = scene.active();
  o.painted = std::make_shared<PaintedMesh>(mesh, 64);
  o.mesh_active = true;
  scene.set_deformation(o.id, state);
  auto encoded = scene.encode();
  check(encoded["version"] == 11 && Scene::decode(encoded).encode() == encoded,
        "Scene11 writer and reader preserve imported rest skinning metadata "
        "exactly");
  auto downgraded = encoded;
  downgraded["version"] = 9;
  check(refuses([&] { Scene::decode(downgraded); }),
        "An old Scene version cannot hide active Skeleton2 bind authority");
  auto origin = std::make_shared<SourceUvOrigin>();
  origin->paint = o.painted->snapshot();
  origin->deformation = o.deformation;
  auto binding = std::make_shared<SourceMaterialBindings>();
  binding->has_mtoon = false;
  binding->uv_origin = origin;
  scene.active().material_bindings = binding;
  scene.clear_deformation(o.id);
  auto retained = scene.encode();
  check(retained["version"] == 11 &&
            Scene::decode(retained).encode() == retained,
        "Dormant retained origin bind metadata still requires Scene11 after "
        "current rig removal");
  downgraded = retained;
  downgraded["version"] = 9;
  check(refuses([&] { Scene::decode(downgraded); }),
        "Old Scene versions cannot hide Skeleton2 metadata inside source "
        "origins");
  scene.active().material_bindings.reset();
  scene.set_deformation(o.id, state);
  auto path = std::filesystem::temp_directory_path() /
              ("dome-authored-bind-" + std::to_string(getpid()) + ".glb");
  export_glb(scene.surfaces(), path);
  ImportLimits limits;
  limits.mode = ImportMode::PreserveGltf;
  auto emitted = import_mesh(path, limits);
  std::filesystem::remove(path);
  check(emitted.skins.size() == 1 && emitted.skins[0].inverse_bind_matrices ==
                                         *rig.inverse_bind_override,
        "Independent GLB accessor import finds exact original authored inverse "
        "binds");
  const auto &skin = emitted.skins[0];
  for (size_t v = 0; v < 3; ++v) {
    Vec3 p{};
    for (auto i : state.skin->vertices[v])
      p += point(product(emitted.nodes.at(skin.joints[i.joint]).world,
                         skin.inverse_bind_matrices[i.joint]),
                 mesh.positions[v]) *
           i.weight;
    if (!close(p, posed.positions[v]))
      throw std::runtime_error("Exported rest world deformation differs from "
                               "native authored binding");
  }
  check(true, "Exported joint hierarchy and IBM accessors independently "
              "reproduce native reflected/nonuniform rest geometry");
  Skeleton legacy;
  legacy.bones.push_back({"old", -1, {}, 1});
  check(legacy.encode()["version"] == 1 &&
            Skeleton::decode(legacy.encode()).encode() == legacy.encode(),
        "Legacy native skeleton bytes/behavior remain version one without "
        "imported features");
  std::cout << checks << " authored-bind foundation checks passed" << std::endl;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << std::endl;
  return 1;
}
