// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/source-morph.hpp"
#include "cgltf.h"
#include "dome/gltf-export.hpp"
#include <fstream>
#include <iostream>
#include <unistd.h>
using namespace dome;
using namespace dome::detail;
namespace {
void check(bool b, const char *s) {
  if (!b)
    throw std::runtime_error(s);
  std::cout << "PASS " << s << '\n';
}
template <class F> bool rejects(F f) {
  try {
    f();
    return false;
  } catch (const std::exception &e) {
    std::cout << "REJECT " << e.what() << '\n';
    return true;
  }
}
ObjectSurface fixture(float scale = 1, bool original = false,
                      bool reorder = false) {
  auto asset = std::make_shared<ImportedAsset>();
  asset->mode = ImportMode::PreserveGltf;
  ImportedPrimitive p;
  p.source_mesh = p.source_primitive = 0;
  p.mesh.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
  p.mesh.normals.assign(3, {0, 0, 1});
  p.mesh.uv = {{0, 0}, {1, 0}, {0, 1}};
  p.mesh.indices = {0, 1, 2};
  p.tangents.assign(3, {1, 0, 0, -1});
  ImportedMorphTarget target;
  target.positions = {{0, 0, .1f}, {0, .1f, 0}, {.02f, 0, 0}};
  target.normals = {{.1f, 0, 0}, {0, .2f, 0}, {0, 0, .3f}};
  target.tangents = {{.01f, .02f, 0}, {0, .03f, .04f}, {.05f, 0, .06f}};
  p.morph_targets = {target};
  asset->primitives = {p};
  auto appearance = std::make_shared<CreatorAppearance>();
  appearance->asset = asset;
  appearance->source_mesh = appearance->source_primitive = 0;
  appearance->source_vertices =
      reorder ? std::vector<uint32_t>{2, 0, 1} : std::vector<uint32_t>{0, 1, 2};
  appearance->source_morph_targets = {0};
  appearance->source_basis_quaternion = Quat{0, 0, 1, 0};
  ObjectSurface o;
  o.id = 1;
  o.name = "Source triangle";
  o.appearance = appearance;
  Quat basis = original ? Quat{} : *appearance->source_basis_quaternion;
  Vec3 offset{.3f, -.2f, .1f};
  for (auto v : appearance->source_vertices) {
    o.mesh.positions.push_back(basis.rotate(p.mesh.positions[v]) * scale +
                               offset);
    o.mesh.normals.push_back(basis.rotate(p.mesh.normals[v]));
    o.mesh.uv.push_back(p.mesh.uv[v]);
  }
  o.mesh.indices =
      reorder ? std::vector<uint32_t>{1, 2, 0} : std::vector<uint32_t>{0, 1, 2};
  auto d = std::make_shared<DeformationState>(o.mesh);
  d->morphs = MorphSet(o.mesh);
  std::vector<Vec3> edited = o.mesh.positions;
  for (size_t v = 0; v < edited.size(); ++v)
    edited[v] +=
        basis.rotate(target.positions[appearance->source_vertices[v]]) * scale;
  d->morphs->set_target(o.mesh, "Label deliberately unrelated to source index",
                        edited);
  o.deformation = d;
  return o;
}
std::shared_ptr<CreatorAppearance> appearance(ObjectSurface &o) {
  auto a = std::make_shared<CreatorAppearance>(*o.appearance);
  o.appearance = a;
  return a;
}
std::shared_ptr<ImportedAsset> asset(ObjectSurface &o) {
  auto a = appearance(o);
  auto source = std::make_shared<ImportedAsset>(*a->asset);
  a->asset = source;
  return source;
}
} // namespace
int main() try {
  auto o = fixture();
  SourceMorphTransport native(o);
  auto channels = native.target(0);
  auto tangents = native.base_tangents();
  check(native.frame_valid() && channels.normals.size() == 3 &&
            length(channels.normals[0] - Vec3{-.1f, 0, 0}) < 1e-6f,
        "Authored NORMAL morphs rotate through explicit source basis");
  check(channels.tangents.size() == 3 &&
            length(channels.tangents[2] - Vec3{-.05f, 0, -.06f}) < 1e-6f,
        "Authored TANGENT morph directions retain exact identity and "
        "orientation");
  check(tangents.size() == 3 && tangents[0][0] == -1 && tangents[0][3] == -1,
        "Base tangent handedness survives proper coordinate rotation");
  auto scaled = fixture(2);
  SourceMorphTransport positive(scaled);
  check(positive.frame_valid() &&
            length(positive.target(0).normals[0] - channels.normals[0]) < 1e-6f,
        "Positive uniform scale/translation does not scale direction deltas");
  auto reordered = fixture(1, false, true);
  auto reordered_channels = SourceMorphTransport(reordered).target(0);
  check(length(reordered_channels.normals[0] - Vec3{0, 0, -.3f}) < 1e-6f &&
            length(reordered_channels.normals[1] - Vec3{-.1f, 0, 0}) < 1e-6f,
        "Compacted/reordered native vertices retain original direction "
        "accessor mapping");
  auto original = fixture(1, true);
  auto restored =
      SourceMorphTransport(original, SourceMorphSpace::Original).target(0);
  check(length(restored.normals[0] - Vec3{.1f, 0, 0}) < 1e-6f,
        "VRM source-space output explicitly avoids applying basis twice");
  auto missing = o;
  appearance(missing)->source_basis_quaternion.reset();
  check(rejects([&] { SourceMorphTransport x(missing); }),
        "Missing source basis refuses silent direction loss");
  missing = o;
  appearance(missing)->source_morph_targets.clear();
  check(rejects([&] { SourceMorphTransport x(missing); }),
        "Display labels/counts cannot replace explicit source morph identity");
  missing = o;
  appearance(missing)->source_morph_targets = {99};
  check(rejects([&] { SourceMorphTransport x(missing); }),
        "Out-of-range morph identity rejects");
  missing = o;
  appearance(missing)->source_vertices = {0, 1, 99};
  check(rejects([&] { SourceMorphTransport x(missing); }),
        "Out-of-range compacted source vertex rejects");
  auto changed = o;
  auto d = std::make_shared<DeformationState>(*o.deformation);
  d->morphs->targets[0].delta[0].y += .04f;
  changed.deformation = d;
  check(
      rejects([&] {
        SourceMorphTransport x(changed);
        x.target(0);
      }),
      "Changed source direction targets require an explicit transport policy");
  check(rejects([&] {
          SourceMorphTransport x(changed);
          x.target(0, true);
        }),
        "Changed TANGENT morphs reject until tangent recomputation exists");
  asset(changed)->primitives[0].morph_targets[0].tangents.clear();
  auto recomputed = SourceMorphTransport(changed).target(0, true);
  std::array<float, 1> weights{1};
  auto expected = changed.deformation->morphs->evaluate(changed.mesh, weights);
  bool matches = recomputed.normals_recomputed;
  for (size_t v = 0; v < 3; ++v)
    matches &= length(recomputed.normals[v] -
                      (expected.normals[v] - changed.mesh.normals[v])) < 1e-6f;
  check(matches,
        "Authorized changed NORMAL deltas match actual native target geometry");
  auto uv = o;
  uv.mesh.uv[0][0] = .1f;
  check(rejects([&] {
          SourceMorphTransport x(uv);
          x.base_tangents();
        }),
        "Edited source UV tangent frames reject explicitly");
  auto warped = o;
  warped.mesh.positions[1].y += .1f;
  auto warped_d = std::make_shared<DeformationState>(*warped.deformation);
  warped_d->source = morph_topology_id(warped.mesh);
  warped_d->morphs->topology = warped_d->source;
  warped.deformation = warped_d;
  check(rejects([&] {
          SourceMorphTransport x(warped);
          x.base_tangents();
        }),
        "Nonuniform rest changes cannot silently preserve stale base tangents");
  check(rejects([&] {
          SourceMorphTransport x(o, SourceMorphSpace::Native,
                                 [] { return true; });
        }),
        "Cancellation precedes source transport publication");
  check(rejects([&] {
          SourceMorphTransport x(o, SourceMorphSpace::Native, {}, 2);
        }),
        "Explicit source transport work budget is enforced");
  auto nonfinite = o;
  asset(nonfinite)->primitives[0].morph_targets[0].normals[0].x =
      std::numeric_limits<float>::infinity();
  check(rejects([&] {
          SourceMorphTransport x(nonfinite);
          x.target(0);
        }),
        "Nonfinite authored direction payloads reject");
  auto bad_position = o;
  asset(bad_position)->primitives[0].morph_targets[0].positions[0].x =
      std::numeric_limits<float>::quiet_NaN();
  check(rejects([&] {
          SourceMorphTransport x(bad_position);
          x.target(0);
        }),
        "Nonfinite authored morph positions cannot accidentally pass direction "
        "matching");
  auto plain = o;
  plain.appearance.reset();
  auto empty = SourceMorphTransport(plain).target(0);
  check(empty.normals.empty() && empty.tangents.empty(),
        "Native POSITION-only targets retain backward-compatible export "
        "semantics");
  auto path = std::filesystem::temp_directory_path() /
              ("dome-source-morph-" + std::to_string(getpid()) + ".glb");
  export_glb({reordered}, path);
  cgltf_options options{};
  cgltf_data *raw = nullptr;
  check(cgltf_parse_file(&options, path.c_str(), &raw) == cgltf_result_success,
        "Independent parser accepts generic GLB with source morph directions");
  std::unique_ptr<cgltf_data, decltype(&cgltf_free)> parsed(raw, cgltf_free);
  check(cgltf_load_buffers(&options, raw, path.c_str()) ==
                cgltf_result_success &&
            cgltf_validate(raw) == cgltf_result_success,
        "Independent validator accepts source morph direction accessors");
  auto &primitive = raw->meshes[0].primitives[0];
  auto accessor = [&](cgltf_attribute_type type) -> cgltf_accessor * {
    if (primitive.targets_count != 1)
      return nullptr;
    for (size_t i = 0; i < primitive.targets[0].attributes_count; ++i)
      if (primitive.targets[0].attributes[i].type == type)
        return primitive.targets[0].attributes[i].data;
    return nullptr;
  };
  float value[4];
  auto normal = accessor(cgltf_attribute_type_normal),
       tangent = accessor(cgltf_attribute_type_tangent);
  check(normal && cgltf_accessor_read_float(normal, 0, value, 3) &&
            length(Vec3{value[0], value[1], value[2]} - Vec3{0, 0, -.3f}) <
                1e-6f,
        "Generic GLB writes source NORMAL in native basis and compacted order");
  check(tangent && cgltf_accessor_read_float(tangent, 0, value, 3) &&
            length(Vec3{value[0], value[1], value[2]} - Vec3{-.05f, 0, -.06f}) <
                1e-6f,
        "Generic GLB writes source TANGENT morphs without silent omission");
  auto base_tangent =
      cgltf_find_accessor(&primitive, cgltf_attribute_type_tangent, 0);
  check(base_tangent && cgltf_accessor_read_float(base_tangent, 0, value, 4) &&
            value[0] == -1 && value[3] == -1,
        "Generic GLB preserves correctly rotated base tangent and handedness");
  auto read = [&] {
    std::ifstream f(path, std::ios::binary);
    return std::vector<uint8_t>{std::istreambuf_iterator<char>(f), {}};
  };
  auto bytes = read();
  check(rejects([&] { export_glb({changed}, path); }) && read() == bytes,
        "Unsupported changed source directions preserve existing generic GLB "
        "transactionally");
  GltfExportOptions allow;
  allow.recompute_changed_normals = true;
  auto report = export_glb({changed}, path, allow);
  check(report.meshes == 1 && report.normal_targets_recomputed == 1 &&
            !report.diagnostics.empty(),
        "Additive generic GLB API explicitly reports authorized normal "
        "recomputation");
  bytes = read();
  GltfExportOptions budget = allow;
  budget.max_output_bytes = bytes.size() - 1;
  check(rejects([&] { export_glb({changed}, path, budget); }) &&
            read() == bytes,
        "Generic GLB output budget includes container headers and alignment");
  int polls = 0;
  GltfExportOptions cancelled = allow;
  cancelled.cancelled = [&] { return ++polls >= 3; };
  check(rejects([&] { export_glb({changed}, path, cancelled); }) &&
            read() == bytes,
        "Late generic GLB cancellation preserves existing output exactly");
  std::filesystem::remove(path);
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
