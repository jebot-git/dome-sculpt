// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/import-authoring.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/material-bindings.hpp"
#include "dome/projection.hpp"
#include "source-tangent-frames.hpp"
#include "text-utf8.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <numeric>
namespace dome {
namespace {
void stop(const ImportAuthoringOptions &o) {
  if (o.limits.cancelled && o.limits.cancelled())
    throw std::runtime_error("Authoring import cancelled");
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
std::shared_ptr<const std::vector<uint8_t>>
original_glb(const std::filesystem::path &path,
             const ImportAuthoringOptions &o) {
  auto count = std::filesystem::file_size(path);
  // The existing native immutable-source archive is bounded at64MiB per asset.
  if (count > std::min(o.limits.max_file_bytes, size_t(64 * 1024 * 1024)))
    throw std::length_error(
        "Original source exceeds native appearance archive budget");
  auto bytes = std::make_shared<std::vector<uint8_t>>(size_t(count));
  std::ifstream file(path, std::ios::binary);
  size_t at = 0;
  while (at < bytes->size()) {
    stop(o);
    auto n = std::min(bytes->size() - at, size_t(65536));
    if (!file.read(reinterpret_cast<char *>(bytes->data() + at), n))
      throw std::runtime_error("Cannot read complete authoring source");
    at += n;
  }
  if (bytes->size() < 28 || word(*bytes, 0) != 0x46546c67 ||
      word(*bytes, 4) != 2 || word(*bytes, 8) != bytes->size() ||
      word(*bytes, 16) != 0x4e4f534a)
    throw std::runtime_error(
        "This authoring stage requires self-contained GLB; original .gltf "
        "resource bundle adoption remains required");
  return bytes;
}
CanvasTransform similarity(const ImportedNode &n) {
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
    throw std::runtime_error(
        "Imported nonuniform/reflected affine node requires native affine "
        "hierarchy adoption; source was not changed");
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
      throw std::runtime_error(
          "Imported node is not a representable native similarity transform");
  CanvasTransform out{{m[12], m[13], m[14]}, {q.x, q.y, q.z, q.w}, scale};
  validate_transform(out);
  return out;
}
Material native_material(const ImportedMaterial &m) {
  Material out;
  out.name = name(m.name, "Imported material");
  out.color = m.color;
  out.metallic = m.metallic;
  out.roughness = m.roughness;
  out.emissive = m.emissive;
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
     const std::vector<std::shared_ptr<const Image>> &images) {
  auto result = std::make_shared<PaintSourceMaps>();
  auto view = [&](PaintSourceTexture &out, int index, int coordinate) {
    if (index < 0)
      return;
    const auto &t = asset.textures.at(size_t(index));
    out.image = images.at(size_t(t.image));
    out.wrap_s = t.wrap_s;
    out.wrap_t = t.wrap_t;
    out.min_filter = t.min_filter;
    out.mag_filter = t.mag_filter;
    out.texcoord = coordinate;
  };
  view(result->color, m.color_texture, m.color_texcoord);
  view(result->metal_rough, m.metal_rough_texture, m.metal_rough_texcoord);
  view(result->normal, m.normal_texture, m.normal_texcoord);
  view(result->emission, m.emissive_texture, m.emissive_texcoord);
  view(result->occlusion, m.occlusion_texture, m.occlusion_texcoord);
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
  const bool tangent = !p.tangents.empty() || m.normal_texture >= 0;
  if (tangent) {
    const auto &uv = m.normal_texcoord == 1 ? p.uv1 : p.mesh.uv;
    auto original = detail::source_tangent_frames(p, uv, cancelled);
    f->normals = std::move(original.normals);
    f->tangents = std::move(original.base);
    f->generated_tangents = original.generated;
    if (original.generated)
      f->generated_uv = uint32_t(m.normal_texcoord);
    for (size_t t = 0; t < p.morph_targets.size(); ++t)
      f->targets.push_back({t + 1, std::move(original.normal_targets[t]),
                            std::move(original.targets[t])});
  } else {
    for (auto v : p.mesh.indices)
      f->normals.push_back(p.mesh.normals[v]);
    for (size_t t = 0; t < p.morph_targets.size(); ++t) {
      NormalFrameTarget target;
      target.id = t + 1;
      if (!p.morph_targets[t].normals.empty())
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
  if (extension == ".obj") {
    options.limits.mode = ImportMode::StaticScene;
    auto asset = import_mesh(path, options.limits);
    output.diagnostics = asset.warnings;
    for (auto &p : asset.primitives) {
      stop(options);
      const auto &m = asset.materials.at(p.material);
      if (m.color_texture >= 0 || m.metal_rough_texture >= 0 ||
          m.normal_texture >= 0 || m.occlusion_texture >= 0 ||
          m.emissive_texture >= 0)
        throw std::runtime_error(
            "Textured material authoring for OBJ requires original-resource "
            "adoption; source was not changed");
      auto mesh = std::move(p.mesh);
      if (mesh.uv.empty())
        unwrap(mesh, options.paint_resolution);
      auto id = output.scene.add(name(p.name, "Imported mesh"), Volume{});
      auto &object = output.scene.object(id);
      object.material = native_material(asset.materials.at(p.material));
      object.painted = std::make_shared<PaintedMesh>(std::move(mesh),
                                                     options.paint_resolution);
      object.mesh_active = true;
      output.objects.push_back(id);
    }
    output.scene.validate();
    return output;
  }
  auto bytes = original_glb(path, options);
  options.limits.mode = ImportMode::PreserveGltf;
  options.limits.external_resources = false;
  auto asset =
      std::make_shared<const ImportedAsset>(import_glb(*bytes, options.limits));
  const auto json = nlohmann::json::parse(
      bytes->begin() + 20, bytes->begin() + 20 + word(*bytes, 12));
  output.diagnostics = asset->warnings;
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
  for (auto n : ordered) {
    const auto &node = asset->nodes[n];
    similarity(node);
    if (node.mesh >= 0) {
      for (auto primitive : asset->meshes.at(size_t(node.mesh)).primitives) {
        const auto &p = asset->primitives.at(primitive);
        if (!p.normals_authored)
          throw std::runtime_error(
              "This model omits normals; generated flat-normal authoring is "
              "not ready yet. Export authored normals before importing.");
        if (!p.influences.empty())
          throw std::runtime_error(
              "Authored skin/inverse-bind adoption is required before "
              "importing this source; source was not changed");
        if (p.material == 0)
          throw std::runtime_error(
              "Implicit default-material provenance adoption is required "
              "before importing this source; source was not changed");
        ++objects;
      }
    }
  }
  if (objects == 0)
    throw std::runtime_error(
        "Imported selected scene has no triangle primitives");
  if (objects > 256 - input.objects().size() ||
      ordered.size() + 1 > 128 - input.groups().size())
    throw std::length_error(
        "Imported hierarchy exceeds native object/group limits");
  auto images = std::make_shared<std::vector<std::shared_ptr<const Image>>>();
  size_t pixels = 0;
  for (const auto &encoded : asset->images) {
    stop(options);
    if (encoded.mime_type != "image/png")
      throw std::runtime_error("Native authoring image decoder currently "
                               "requires PNG; JPEG adoption remains required");
    auto image = decode_projection_png(encoded.bytes);
    auto count = size_t(image.width) * image.height;
    if (count > 64 * 1024 * 1024 - pixels)
      throw std::length_error("Imported decoded image budget exceeded");
    pixels += count;
    images->push_back(std::make_shared<const Image>(std::move(image)));
  }
  const auto root =
      output.scene.add_group(name(path.stem().string(), "Imported asset"));
  std::vector<uint64_t> groups(asset->nodes.size());
  for (auto n : ordered) {
    stop(options);
    const auto &node = asset->nodes[n];
    groups[n] = output.scene.add_group(
        name(node.name, "Imported node"), similarity(node),
        node.parent < 0 ? root : groups.at(size_t(node.parent)));
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
      appearance->asset = asset;
      appearance->decoded_images = images;
      appearance->material = int(p.material);
      appearance->source_material = int(p.material) - 1;
      appearance->source_mesh = p.source_mesh;
      appearance->source_primitive = p.source_primitive;
      appearance->source_node = int(n);
      appearance->source_vertices.resize(p.mesh.positions.size());
      std::iota(appearance->source_vertices.begin(),
                appearance->source_vertices.end(), 0);
      appearance->material_json =
          json.at("materials").at(p.material - 1).dump();
      appearance->source_basis_quaternion = Quat{};
      if (json.contains("extensions") && json.at("extensions").contains("VRM"))
        throw std::runtime_error(
            "Generic glTF import must not substitute PBR for retained VRM "
            "material semantics; use the VRM authoring adapter");
      Mesh mesh = p.mesh;
      const bool generated_uv = mesh.uv.empty();
      if (generated_uv)
        unwrap(mesh, options.paint_resolution);
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
      auto source_maps = maps(*asset, m, *images);
      // The legacy codec authorizes SourceAuthored only with raw source maps.
      // Plain original UV authority is a Scene11 feature; do not emit a record
      // the current reader would refuse, or silently alter overlapping raw UVs.
      if (source_maps->empty())
        paint_options.policy = PaintUvPolicy::Strict;
      object.painted =
          source_maps->empty()
              ? PaintedMesh::from_mesh(std::move(mesh),
                                       options.paint_resolution, paint_options)
              : PaintedMesh::from_maps(std::move(mesh), source_maps,
                                       options.paint_resolution, paint_options);
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
      auto binding = std::make_shared<SourceMaterialBindings>();
      binding->has_mtoon = false;
      binding->normal_frame = SourceMaterialNormalFrame::OriginalAttributes;
      binding->normal_frames =
          frames(p, m, object.painted->mesh(), options.limits.cancelled);
      binding->native_reference.color = object.material.color;
      binding->native_reference.emission = object.material.emissive;
      binding->native_reference.alpha = int(object.material.alpha);
      binding->native_reference.alpha_cutoff = object.material.alpha_cutoff;
      binding->native_reference.double_sided = object.material.double_sided;
      auto origin = std::make_shared<SourceUvOrigin>();
      origin->paint = object.painted->snapshot();
      origin->appearance = appearance;
      origin->deformation = object.deformation;
      binding->uv_origin = origin;
      binding->validate();
      object.material_bindings = binding;
      output.objects.push_back(id);
    }
  }
  output.scene.validate();
  stop(options);
  return output;
}
} // namespace dome
