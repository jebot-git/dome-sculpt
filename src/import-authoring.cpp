// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/import-authoring.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/import-image.hpp"
#include "dome/import-native.hpp"
#include "dome/import-source.hpp"
#include "dome/material-bindings.hpp"
#include "dome/source-paint.hpp"
#include "normal-frame-runtime.hpp"
#include "initial-source-atlas.hpp"
#include "dome/projection.hpp"
#include "dome/source-placement.hpp"
#include "import-skin.hpp"
#include "source-tangent-frames.hpp"
#include "text-utf8.hpp"
#include "vrm-import.hpp"
#include "bone-metadata.hpp"
#include "humanoid-placement-import.hpp"
#include "source-decoded-images.hpp"
#include "import-texture-transform.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <numeric>
namespace dome {
namespace {
void stop(const ImportAuthoringOptions &o) {
  if (o.limits.cancelled && o.limits.cancelled())
    throw std::runtime_error("Authoring import cancelled");
}
Mesh used_source_mesh(const Mesh &source,
                      const std::function<bool()> &cancelled) {
  std::vector<uint32_t> remap(source.positions.size(), UINT32_MAX);
  Mesh mesh;
  for (size_t c = 0; c < source.indices.size(); ++c) {
    if ((c & 4095) == 0 && cancelled && cancelled())
      throw std::runtime_error("Source vertex compaction cancelled");
    auto original = source.indices[c];
    auto &native = remap.at(original);
    if (native == UINT32_MAX) {
      native = uint32_t(mesh.positions.size());
      mesh.positions.push_back(source.positions[original]);
      if (!source.normals.empty())
        mesh.normals.push_back(source.normals[original]);
      if (!source.uv.empty())
        mesh.uv.push_back(source.uv[original]);
    }
    mesh.indices.push_back(native);
  }
  mesh.validate();
  return mesh;
}
std::string name(std::string value, std::string_view fallback) {
  if (value.empty())
    value = fallback;
  if (value.find('\0') != std::string::npos || !text::valid_utf8(value))
    throw std::invalid_argument("Imported name must be valid UTF-8");
  return text::utf8_prefix(value, 220);
}
uint32_t word(std::span<const uint8_t> b, size_t at) {
  if (at > b.size() || b.size() - at < 4)
    throw std::runtime_error("Truncated authoring source GLB");
  return uint32_t(b[at]) | uint32_t(b[at + 1]) << 8 |
         uint32_t(b[at + 2]) << 16 | uint32_t(b[at + 3]) << 24;
}
std::optional<CanvasTransform> similarity(const ImportedNode &n) {
  const auto &m = n.local;
  const Vec3 x{m[0], m[1], m[2]}, y{m[4], m[5], m[6]}, z{m[8], m[9], m[10]};
  const float sx = length(x), sy = length(y), sz = length(z),
              scale = (sx + sy + sz) / 3;
  if (!std::isfinite(scale) || scale <= 0 ||
      std::max({std::abs(sx - scale), std::abs(sy - scale),
                std::abs(sz - scale)}) > scale * 2e-6f ||
      std::abs(dot(x, y)) > scale * scale * 2e-6f ||
      std::abs(dot(y, z)) > scale * scale * 2e-6f ||
      std::abs(dot(z, x)) > scale * scale * 2e-6f || dot(x, cross(y, z)) <= 0)
    return {};
  // Matrix-to-quaternion conversion is deterministic and checked by
  // reconstructing every column, rather than relying on the source's optional
  // TRS flags.
  const float a = m[0] / scale, b = m[4] / scale, c = m[8] / scale,
              d = m[1] / scale, e = m[5] / scale, f = m[9] / scale,
              g = m[2] / scale, h = m[6] / scale, i = m[10] / scale;
  Quat q;
  const float trace = a + e + i;
  if (trace > 0) {
    float s = std::sqrt(trace + 1) * 2;
    q = {s * .25f, (h - f) / s, (c - g) / s, (d - b) / s};
  } else if (a > e && a > i) {
    float s = std::sqrt(1 + a - e - i) * 2;
    q = {(h - f) / s, s * .25f, (b + d) / s, (c + g) / s};
  } else if (e > i) {
    float s = std::sqrt(1 + e - a - i) * 2;
    q = {(c - g) / s, (b + d) / s, s * .25f, (f + h) / s};
  } else {
    float s = std::sqrt(1 + i - a - e) * 2;
    q = {(d - b) / s, (c + g) / s, (f + h) / s, s * .25f};
  }
  q = q.unit();
  for (auto pair : {std::pair{x, q.rotate({1, 0, 0}) * scale},
                    std::pair{y, q.rotate({0, 1, 0}) * scale},
                    std::pair{z, q.rotate({0, 0, 1}) * scale}})
    if (length(pair.first - pair.second) > scale * 4e-6f)
      return {};
  CanvasTransform out{{m[12], m[13], m[14]}, {q.x, q.y, q.z, q.w}, scale};
  try{validate_transform(out);}catch(const std::invalid_argument&){return {};}
  return out;
}
Material native_material(const ImportedMaterial &m) {
  Material out;
  out.name = name(m.name, "Imported material");
  out.color = m.color;
  out.metallic = m.metallic;
  out.roughness = m.roughness;
  out.emissive = m.emissive;
  for (auto &value : out.emissive) {
    value *= m.emissive_strength;
    if (!std::isfinite(value) || value<0 || value>maximum_native_emission)
      throw std::invalid_argument("Imported native emission exceeds bounded linear radiance");
  }
  out.double_sided = m.double_sided;
  out.unlit = m.unlit;
  out.alpha_cutoff = m.alpha_cutoff;
  out.alpha = m.alpha == ImportedMaterial::AlphaMode::Opaque
                  ? Material::AlphaMode::Opaque
              : m.alpha == ImportedMaterial::AlphaMode::Mask
                  ? Material::AlphaMode::Mask
                  : Material::AlphaMode::Blend;
  out.validate();
  return out;
}
std::shared_ptr<const PaintSourceMaps>
maps(const ImportedAsset &asset, const ImportedMaterial &m,
     const std::vector<std::shared_ptr<const Image>> &images,
     const ImportedPrimitive &primitive, std::span<const uint32_t> vertices) {
  auto result = std::make_shared<PaintSourceMaps>();
  auto view = [&](PaintSourceTexture &out, int index, int coordinate,
                  const ImportedTextureTransform &transform) {
    if (index < 0)
      return;
    const auto &t = asset.textures.at(size_t(index));
    out.image = images.at(size_t(t.image));
    out.wrap_s = t.wrap_s;
    out.wrap_t = t.wrap_t;
    out.min_filter = t.min_filter;
    out.mag_filter = t.mag_filter;
    out.texcoord = detail::imported_texture_coordinate(coordinate, transform);
    out.uv_transform = detail::imported_texture_matrix(transform);
  };
  view(result->color, m.color_texture, m.color_texcoord, m.color_transform);
  view(result->metal_rough, m.metal_rough_texture, m.metal_rough_texcoord,
       m.metal_rough_transform);
  view(result->normal, m.normal_texture, m.normal_texcoord, m.normal_transform);
  view(result->emission, m.emissive_texture, m.emissive_texcoord,
       m.emissive_transform);
  view(result->occlusion, m.occlusion_texture, m.occlusion_texcoord,
       m.occlusion_transform);
  if (result->uses_uv1()) {
    if (primitive.uv1.size() != primitive.mesh.positions.size())
      throw std::invalid_argument(
          "Imported texture requires missing UV1 coordinates");
    auto domain = std::make_shared<std::vector<std::array<float, 2>>>();
    domain->reserve(vertices.size());
    for (auto vertex : vertices)
      domain->push_back(primitive.uv1.at(vertex));
    result->uv1 = std::move(domain);
  }
  for (const auto *view :
       {&result->color, &result->metal_rough, &result->normal,
        &result->emission, &result->occlusion})
    if (view->image && view->texcoord >= 2) {
      const auto set = uint32_t(view->texcoord);
      if (result->extra_uv_sets.contains(set))
        continue;
      const auto &source = primitive.extra_uv_sets.at(set);
      if (source.size() != primitive.mesh.positions.size())
        throw std::invalid_argument(
            "Imported texture requires missing additional UV coordinates");
      auto domain = std::make_shared<std::vector<std::array<float, 2>>>();
      domain->reserve(vertices.size());
      for (auto vertex : vertices)
        domain->push_back(source.at(vertex));
      result->extra_uv_sets.emplace(set, std::move(domain));
    }
  result->normal_scale = m.normal_scale;
  result->occlusion_strength = m.occlusion_strength;
  result->validate();
  return result;
}
std::shared_ptr<const NormalFrameSet>
frames(const ImportedPrimitive &p, const ImportedMaterial &m,
       const Mesh &current, const std::function<bool()> &cancelled) {
  auto set = std::make_shared<NormalFrameSet>();
  auto f = std::make_shared<NormalFrames>();
  f->origin_vertex_count = uint32_t(current.positions.size());
  f->source_vertex_count = uint32_t(p.mesh.positions.size());
  f->source_corner_count = uint32_t(p.mesh.indices.size());
  f->origin_vertices = current.indices;
  f->source_vertices = p.mesh.indices;
  f->source_corners.resize(p.mesh.indices.size());
  std::iota(f->source_corners.begin(), f->source_corners.end(), 0);
  const bool tangent =
      (p.normals_authored && !p.tangents.empty()) || m.normal_texture >= 0;
  if (tangent) {
    const auto coordinate = detail::imported_texture_coordinate(
        m.normal_texcoord, m.normal_transform);
    if (p.tangents.empty() && m.normal_texture >= 0 &&
        detail::imported_texture_matrix(m.normal_transform) !=
            std::array<float, 6>{1, 0, 0, 1, 0, 0})
      throw std::invalid_argument("Generated normal tangents for transformed "
                                  "sampling need an owning frame adapter");
    const auto &uv = coordinate == 0 ? p.mesh.uv
                     : coordinate == 1
                         ? p.uv1
                         : p.extra_uv_sets.at(uint32_t(coordinate));
    auto original = detail::source_tangent_frames(p, uv, cancelled);
    f->normals = std::move(original.normals);
    f->tangents = std::move(original.base);
    f->generated_tangents = original.generated;
    if (original.generated)
      f->generated_uv = uint32_t(coordinate);
    for (size_t t = 0; t < p.morph_targets.size(); ++t)
      f->targets.push_back({t + 1, std::move(original.normal_targets[t]),
                            std::move(original.targets[t])});
  } else {
    if (p.normals_authored)
      for (auto v : p.mesh.indices) f->normals.push_back(p.mesh.normals[v]);
    else
      f->normals = detail::source_flat_mesh(p.mesh, {}, cancelled).normals;
    for (size_t t = 0; t < p.morph_targets.size(); ++t) {
      NormalFrameTarget target;
      target.id = t + 1;
      if (!p.normals_authored) {
        auto posed = detail::source_flat_mesh(p.mesh, p.morph_targets[t].positions, cancelled);
        target.normals.reserve(f->normals.size());
        for (size_t c = 0; c < f->normals.size(); ++c) {
          if ((c & 1023) == 0 && cancelled && cancelled())
            throw std::runtime_error("Generated source normals cancelled");
          target.normals.push_back(posed.normals[c] - f->normals[c]);
        }
      } else if (!p.morph_targets[t].normals.empty())
        for (auto v : p.mesh.indices)
          target.normals.push_back(p.morph_targets[t].normals[v]);
      f->targets.push_back(std::move(target));
    }
  }
  for (size_t t = 0; t < p.morph_targets.size(); ++t) {
    set->targets.push_back({t + 1, uint32_t(t), uint32_t(t), {}});
    set->current_targets.push_back(t + 1);
  }
  set->next_target = p.morph_targets.size() + 1;
  f->validate();
  set->pbr = std::move(f);
  set->validate();
  return set;
}
} // namespace
ImportedAuthoring prepare_authoring_import(const Scene &input,
                                           const std::filesystem::path &path,
                                           ImportAuthoringOptions options) {
  if (options.paint_resolution < 64 || options.paint_resolution > 2048 ||
      (options.paint_resolution & (options.paint_resolution - 1)))
    throw std::invalid_argument("Invalid paint resolution");
  stop(options);
  ImportedAuthoring output;
  output.scene = input.snapshot();
  auto extension = path.extension().string();
  for (auto &c : extension)
    c = char(std::tolower(static_cast<unsigned char>(c)));
  options.choices.validate();
  options.limits.max_file_bytes=std::min(options.limits.max_file_bytes,size_t(64*1024*1024));
  auto captured=(extension==".obj" || extension==".fbx" ||
      options.choices.format==ImportSourceFormat::Obj || options.choices.format==ImportSourceFormat::Fbx)
      ? capture_native_source(path,options.choices,options.limits)
      : capture_gltf_source(path,options.choices,options.limits);
  auto bytes=captured.canonical_glb;
  options.limits.mode = ImportMode::PreserveGltf;
  options.limits.external_resources = false;
  const auto json_size = word(*bytes, 12);
  if (json_size > 16 * 1024 * 1024)
    throw std::length_error(
        "Authoring source JSON exceeds native source archive bounds");
  size_t metadata_visits = 0;
  const auto json = nlohmann::json::parse(
      bytes->begin() + 20, bytes->begin() + 20 + json_size,
      [&](int depth, nlohmann::json::parse_event_t, const nlohmann::json &) {
        if (depth > 64 ||
            ++metadata_visits > options.limits.max_metadata_visits)
          throw std::length_error(
              "Authoring source JSON exceeds metadata bounds");
        if ((metadata_visits & 4095) == 0)
          stop(options);
        return true;
      });
  const auto vrm_version = detail::vrm_import_version(json);
  options.limits.allow_vrm_metadata =
      vrm_version != detail::VrmImportVersion::None;
  if (vrm_version == detail::VrmImportVersion::V0)
    options.limits.allow_legacy_vrm_skin_hint = true;
  auto asset =
      std::make_shared<const ImportedAsset>(import_glb(*bytes, options.limits));
  const auto vrm =
      detail::vrm_import_metadata(json, *asset, options.limits.cancelled);
  const auto bone_metadata = detail::source_bone_metadata(*asset,options.limits.cancelled);
  std::vector<uint8_t> adopted_bone_metadata(asset->nodes.size());
  output.diagnostics = captured.diagnostics;
  output.diagnostics.insert(output.diagnostics.end(),asset->warnings.begin(),asset->warnings.end());
  if (asset->nodes.empty())
    throw std::runtime_error("Imported source has no instantiated scene nodes");
  std::vector<uint32_t> roots;
  if (!asset->scenes.empty()) {
    int selected = asset->default_scene < 0 ? 0 : asset->default_scene;
    roots = asset->scenes.at(size_t(selected)).roots;
  } else
    for (size_t n = 0; n < asset->nodes.size(); ++n)
      if (asset->nodes[n].parent < 0)
        roots.push_back(uint32_t(n));
  std::vector<uint32_t> ordered;
  std::vector<uint8_t> visited(asset->nodes.size());
  auto walk = [&](auto &&self, uint32_t n) -> void {
    stop(options);
    if (n >= asset->nodes.size() || visited[n])
      throw std::runtime_error(
          "Imported selected scene has repeated or invalid node identity");
    visited[n] = 1;
    ordered.push_back(n);
    for (auto child : asset->nodes[n].children)
      self(self, child);
  };
  for (auto n : roots)
    walk(walk, n);
  size_t objects = 0;
  bool affine_graph = vrm.version != detail::VrmImportVersion::None;
  std::vector<CanvasTransform> native_locals(asset->nodes.size());
  for (auto n : ordered) {
    const auto &node = asset->nodes[n];
    // Full source worlds may contain shear from valid local TRS chains.
    // Editing/query inversion must be well defined before native adoption.
    (void)AffineTransform(node.world).inverse();
    auto local=similarity(node);if(local)native_locals[n]=*local;else affine_graph=true;
    if (node.mesh >= 0)
      objects += asset->meshes.at(size_t(node.mesh)).primitives.size();
  }
  if (objects == 0)
    throw std::runtime_error(
        "Imported selected scene has no triangle primitives");
  std::vector<uint8_t> editable_group(asset->nodes.size(), 1);
  size_t group_count = ordered.size();
  if (vrm.version != detail::VrmImportVersion::None &&
      group_count + 1 > 128 - input.groups().size()) {
    // Non-rendered rig branches already have exact node/IBM authority in the
    // retained model and native Skeleton. Do not duplicate every joint as an
    // editable SceneGroup merely to place the real surface instances.
    std::fill(editable_group.begin(), editable_group.end(), 0);
    group_count = 0;
    for (auto n : ordered)
      if (asset->nodes[n].mesh >= 0) {
        for (int ancestor = int(n); ancestor >= 0;
             ancestor = asset->nodes[size_t(ancestor)].parent) {
          if (editable_group[size_t(ancestor)])
            break;
          editable_group[size_t(ancestor)] = 1;
          ++group_count;
        }
      }
    output.diagnostics.push_back(
        "Original non-rendered rig nodes retained in the complete "
        "source/skeleton graph; native placement groups contain every mesh "
        "ancestor");
  }
  if (objects > 256 - input.objects().size() ||
      group_count + 1 > 128 - input.groups().size())
    throw std::length_error(
        "Imported hierarchy exceeds native object/group limits");
  // Each prepared object retains its original paint domain as well as the
  // editable atlas. Check the import's minimum Scene paint budget before any
  // image decoding or xatlas worker allocation, then validate the cumulative
  // result after each fully prepared primitive (including existing owners).
  if (options.prepare_texture_atlas) {
    constexpr size_t scene_texels = 16 * 1024 * 1024;
    const size_t domains =
        2 * size_t(options.paint_resolution) * options.paint_resolution;
    if (objects > scene_texels / domains)
      throw std::length_error(
          "Editable atlas import exceeds Scene paint-map budget");
  }
  auto images = detail::decode_source_images(asset->images, 64 * 1024 * 1024,
                                             options.limits.cancelled);
  std::shared_ptr<CreatorSourceModel> model;
  if (affine_graph) {
    model = std::make_shared<CreatorSourceModel>();
    model->original_bytes = bytes;
    model->asset = asset;
    model->decoded_images = images;
    model->vrm_json = vrm.vrm0_json;
    if (captured.original->root.bytes != bytes ||
        captured.original->format != ImportSourceFormat::Glb ||
        captured.original->options != ImportOptions{})
      model->source_receipt = captured.original;
  }
  const auto root =
      output.scene.add_group(name(path.stem().string(), "Imported asset"));
  std::vector<uint64_t> groups(asset->nodes.size());
  for (auto n : ordered) {
    stop(options);
    const auto &node = asset->nodes[n];
    if (!editable_group[n])
      continue;
    groups[n] = output.scene.add_group(
        name(node.name, "Imported node"),
        affine_graph ? CanvasTransform{} : native_locals[n],
        node.parent < 0 ? root : groups.at(size_t(node.parent)));
    if (affine_graph) {
      auto placement = std::make_shared<SourceNodePlacement>();
      placement->model = model;
      placement->source_node = n;
      placement->source_instance = root;
      placement->validate();
      output.scene.group(groups[n]).source_placement=std::move(placement);
    }
    if (node.mesh < 0)
      continue;
    const auto &source_mesh = asset->meshes.at(size_t(node.mesh));
    for (auto primitive : source_mesh.primitives) {
      stop(options);
      const auto &p = asset->primitives.at(primitive);
      const auto &m = asset->materials.at(p.material);
      auto id = output.scene.add(name(p.name, "Imported primitive"), Volume{});
      auto &object = output.scene.object(id);
      object.group = groups[n];
      object.material = native_material(m);
      auto appearance = std::make_shared<CreatorAppearance>();
      appearance->original_bytes = bytes;
      if(captured.original->root.bytes!=bytes || captured.original->format!=ImportSourceFormat::Glb ||
          captured.original->options!=ImportOptions{})
        appearance->source_receipt=captured.original;
      appearance->asset = asset;
      appearance->decoded_images = images;
      appearance->vrm_json = vrm.vrm0_json;
      appearance->material = int(p.material);
      appearance->source_material = int(p.material) - 1;
      appearance->source_mesh = p.source_mesh;
      appearance->source_primitive = p.source_primitive;
      appearance->source_node = int(n);
      appearance->source_vertices.resize(p.mesh.positions.size());
      std::iota(appearance->source_vertices.begin(),
                appearance->source_vertices.end(), 0);
      if(p.material)appearance->material_json=json.at("materials").at(p.material-1).dump();
      if (vrm.version != detail::VrmImportVersion::None)
        appearance->mtoon_json = vrm.material_properties.at(p.material);
      appearance->source_basis_quaternion = Quat{};
      Mesh mesh = p.normals_authored
          ? used_source_mesh(p.mesh, options.limits.cancelled)
          : detail::source_flat_mesh(p.mesh, {}, options.limits.cancelled);
      // Generate only genuinely missing UV0. An explicit editable-atlas
      // request must first capture every existing original coordinate exactly.
      const bool generated_uv = mesh.uv.empty();
      if (generated_uv)
        unwrap(mesh, options.paint_resolution, 4, options.limits.cancelled);
      // The atlas retains exact ordered corners, not just coincident positions.
      if (mesh.indices.size() != p.mesh.indices.size())
        throw std::runtime_error("Imported atlas changed source corner count");
      std::vector<uint32_t> remap(mesh.positions.size(), UINT32_MAX);
      for (size_t c = 0; c < mesh.indices.size(); ++c) {
        auto native = mesh.indices[c], source = p.mesh.indices[c];
        if (remap[native] != UINT32_MAX && remap[native] != source)
          throw std::runtime_error(
              "Imported atlas merged distinct source vertex identities");
        remap[native] = source;
        auto a = mesh.positions[native], b = p.mesh.positions[source];
        if (a.x != b.x || a.y != b.y || a.z != b.z)
          throw std::runtime_error("Imported atlas changed original positions");
      }
      for (size_t v = 0; v < remap.size(); ++v)
        if (remap[v] == UINT32_MAX) {
          if (generated_uv)
            throw std::runtime_error(
                "Imported atlas contains unproven unused vertex identity");
          remap[v] = uint32_t(v);
        }
      appearance->source_vertices = remap;
      PaintSurfaceOptions paint_options;
      paint_options.policy = PaintUvPolicy::SourceAuthored;
      paint_options.cancelled = options.limits.cancelled;
      auto source_maps = maps(*asset, m, *images, p, remap);
      object.painted = capture_source_sampling(std::move(mesh),source_maps,
                                                options.paint_resolution,paint_options);
      object.mesh_active = true;
      object.appearance = appearance;
      if (!p.morph_targets.empty()) {
        DeformationState deformation(object.painted->mesh());
        deformation.morphs = MorphSet(object.painted->mesh());
        for (size_t t = 0; t < p.morph_targets.size(); ++t) {
          stop(options);
          MorphTarget target;
          target.name = name(t < source_mesh.target_names.size()
                                 ? source_mesh.target_names[t]
                                 : "Source target " + std::to_string(t),
                             "Source target");
          target.name += " [" + std::to_string(t) + "]";
          target.delta.reserve(remap.size());
          for (auto v : remap)
            target.delta.push_back(p.morph_targets[t].positions.empty()
                                       ? Vec3{}
                                       : p.morph_targets[t].positions[v]);
          deformation.morphs->targets.push_back(std::move(target));
          appearance->source_morph_targets.push_back(uint32_t(t));
        }
        deformation.morph_weights =
            node.weights.empty() ? source_mesh.weights : node.weights;
        deformation.validate(object.painted->mesh());
        object.deformation =
            std::make_shared<const DeformationState>(std::move(deformation));
      }
      if (node.skin >= 0 && !p.influences.empty()) {
        auto rig = detail::imported_rig(*asset, n, options.limits.cancelled,
                                        vrm.rig_nodes);
        detail::apply_bone_lengths(rig.skeleton,rig.source_nodes,bone_metadata,
                                   options.limits.cancelled);
        for(auto source:rig.source_nodes)
          if(bone_metadata.lengths[source])adopted_bone_metadata[source]=1;
        DeformationState deformation =
            object.deformation ? *object.deformation
                               : DeformationState(object.painted->mesh());
        deformation.skin =
            detail::imported_weights(p, rig, remap, options.limits.cancelled);
        if (vrm.version != detail::VrmImportVersion::None)
          detail::adopt_humanoid_placement_prefix(rig,*asset,n,vrm.human_nodes,
                                                 *deformation.skin,options.limits.cancelled);
        deformation.skeleton = rig.skeleton;
        appearance->source_joint_nodes = rig.source_nodes;
        if (vrm.version != detail::VrmImportVersion::None)
          deformation.humanoid = detail::vrm_import_humanoid(vrm, rig);
        deformation.validate(object.painted->mesh());
        object.deformation =
            std::make_shared<const DeformationState>(std::move(deformation));
      }
      if (vrm.version != detail::VrmImportVersion::None && object.deformation &&
          object.deformation->morphs) {
        auto deformation =
            std::make_shared<DeformationState>(*object.deformation);
        deformation->expressions = detail::vrm_import_visemes(
            vrm, p.source_mesh, int(n), *deformation->morphs);
        deformation->validate(object.painted->mesh());
        object.deformation = std::move(deformation);
      }
      auto binding = std::make_shared<SourceMaterialBindings>();
      binding->has_mtoon = false;
      binding->normal_frame = SourceMaterialNormalFrame::OriginalAttributes;
      binding->normal_frames =
          frames(p, m, object.painted->mesh(), options.limits.cancelled);
      std::optional<MToonMaterial> imported_mtoon;
      if(vrm.version!=detail::VrmImportVersion::None&&vrm.materials.at(p.material))imported_mtoon=*vrm.materials[p.material];
      else if(p.material){
        const auto &raw=json.at("materials").at(p.material-1);
        if(raw.contains("extensions")&&raw.at("extensions").contains("VRMC_materials_mtoon"))
          imported_mtoon=MToonMaterial::from_vrm1(raw,asset->textures.size());
      }
      if (imported_mtoon) {
        binding->has_mtoon = true;
        binding->material = *imported_mtoon;
        binding->roles =
            detail::vrm_import_roles(binding->material, *asset, *images);
        auto resolved =
            std::make_shared<NormalFrameSet>(*binding->normal_frames);
        auto normal = m;
        const auto &role =
            binding->material.textures[size_t(MToonSlot::Normal)];
        normal.normal_texture =
            binding->material.parameters.normal_enabled ? role.index : -1;
        normal.normal_texcoord = binding->material.version() == MToonVersion::V0
                                     ? 0
                                     : int(role.texcoord);
        normal.normal_transform = {};
        if (binding->material.version() == MToonVersion::V1) {
          normal.normal_transform.authored = true;
          normal.normal_transform.offset = role.offset;
          normal.normal_transform.scale = role.scale;
          normal.normal_transform.rotation = role.rotation;
          normal.normal_transform.texcoord = role.transform_texcoord;
        }
        resolved->mtoon =
            (m.normal_texture >= 0) == (normal.normal_texture >= 0) &&
                    detail::imported_texture_coordinate(m.normal_texcoord,
                                                        m.normal_transform) ==
                        detail::imported_texture_coordinate(
                            normal.normal_texcoord, normal.normal_transform) &&
                    detail::imported_texture_matrix(m.normal_transform) ==
                        detail::imported_texture_matrix(normal.normal_transform)
                ? resolved->pbr
                : frames(p, normal, object.painted->mesh(),
                         options.limits.cancelled)
                      ->pbr;
        binding->normal_frames = std::move(resolved);
        object.material.toon = true;
      }
      binding->native_reference.color = object.material.color;
      binding->native_reference.emission = object.material.emissive;
      binding->native_reference.alpha = int(object.material.alpha);
      binding->native_reference.alpha_cutoff = object.material.alpha_cutoff;
      binding->native_reference.double_sided = object.material.double_sided;
      auto origin = std::make_shared<SourceUvOrigin>();
      origin->paint = object.painted->snapshot();
      origin->appearance = appearance;
      origin->deformation = object.deformation;
      if (binding->has_mtoon) {
        origin->material = binding->material;
        origin->roles = binding->roles;
      }
      binding->uv_origin = origin;
      binding->validate();
      object.material_bindings = binding;
      if(options.prepare_texture_atlas){
        std::shared_ptr<const std::vector<std::array<float,2>>> authored_uv1;
        if(!p.uv1.empty()&&!source_maps->uv1){
          if(p.uv1.size()!=p.mesh.positions.size())throw std::invalid_argument("Imported authored UV1 domain differs");
          auto coordinates=std::make_shared<std::vector<std::array<float,2>>>();coordinates->reserve(appearance->source_vertices.size());
          for(size_t v=0;v<appearance->source_vertices.size();++v){if((v&1023)==0)stop(options);coordinates->push_back(p.uv1.at(appearance->source_vertices[v]));}
          authored_uv1=std::move(coordinates);
        }
        SourcePaintOptions paint;paint.cancelled=options.limits.cancelled;
        detail::prepare_initial_source_atlas(object,options.paint_resolution,std::move(authored_uv1),paint);
        output.diagnostics.push_back("Prepared editable paint atlas at "+std::to_string(options.paint_resolution)+" pixels; original image sampling, material roles and source coordinates retained");
      }
      if(options.prepare_texture_atlas)output.scene.validate(false);
      output.objects.push_back(id);
    }
  }
  for(auto source:ordered)
    if(bone_metadata.lengths[source]&&!adopted_bone_metadata[source])
      throw std::invalid_argument("Bone authoring metadata needs an associated adopted skin; unbound bone import remains required");
  if(std::any_of(adopted_bone_metadata.begin(),adopted_bone_metadata.end(),[](auto value){return value!=0;}))
    output.diagnostics.push_back("Retained authored bone lengths; other applications may display bones differently");
  output.scene.validate();
  if(!options.prepare_texture_atlas && std::any_of(output.objects.begin(),output.objects.end(),[&](uint64_t id){const auto &o=output.scene.object(id);return o.painted && o.painted->source_maps() && (o.painted->source_maps()->uses_alternate_uv()||std::any_of(o.painted->mesh().uv.begin(),o.painted->mesh().uv.end(),[](const auto &uv){return uv[0]<0||uv[0]>1||uv[1]<0||uv[1]>1;}));}))
    output.diagnostics.insert(output.diagnostics.begin(),"Original source coordinates retained; prepare an editable texture atlas before painting");
  if (vrm.version != detail::VrmImportVersion::None)
    output.diagnostics.push_back(
        "VRM source version, humanoid/node/material/morph identities, "
        "expressions and physics metadata retained; native vowel preview is a "
        "subset and general original-space VRM re-export/physics authoring "
        "remain required.");
  stop(options);
  return output;
}
} // namespace dome
