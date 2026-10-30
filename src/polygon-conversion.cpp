// SPDX-License-Identifier: GPL-3.0-or-later
#include "polygon-conversion.hpp"
#include "document-authority.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/material-bindings.hpp"
#include "dome/material-visibility.hpp"
#include "dome/polygon-source.hpp"
#include "dome/source-maps.hpp"
#include "dome/source-placement.hpp"
#include "polygon-color-source.hpp"
#include "polygon-paint.hpp"
#include "source-colors.hpp"
#include <map>
namespace dome {
namespace {
void check(const std::function<bool()> &cancelled) {
  if (cancelled && cancelled())
    throw std::runtime_error("Polygon conversion cancelled");
}
bool equal(Vec3 a, Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
void transport_policy(const SculptObject &source,
                      const std::function<bool()> &cancelled,
                      bool current_colors = false) {
  check(cancelled);
  if (!source.mesh_active)
    return;
  if (source.mesh_active && source.painted && source.painted->source_maps()) {
    const auto &maps = *source.painted->source_maps();
    for (auto slot : {&maps.color, &maps.metal_rough, &maps.emission,
                      &maps.normal, &maps.occlusion})
      if (slot->image && slot->texcoord != 0)
        throw std::logic_error(
            "Bake UV1 source maps to canonical UV0 before polygon conversion; "
            "the original source was retained");
  }
  if (source.appearance && source.appearance->asset) {
    const ImportedPrimitive *primitive = nullptr;
    for (const auto &p : source.appearance->asset->primitives) {
      check(cancelled);
      if (p.source_mesh == source.appearance->source_mesh &&
          p.source_primitive == source.appearance->source_primitive) {
        if (primitive)
          throw std::invalid_argument(
              "Polygon material source identity is ambiguous");
        primitive = &p;
      }
    }
    if (!primitive)
      throw std::invalid_argument(
          "Polygon material source identity is unavailable");
    for (auto rgba : primitive->colors) {
      check(cancelled);
      for (auto channel : rgba)
        if (channel != 1 && !current_colors)
          throw std::logic_error("Prepare current source COLOR_0 as editable "
                                 "canonical paint before polygon conversion; "
                                 "original vertex colors were retained");
    }
  }
  if (source.material_bindings && source.material_bindings->has_mtoon)
    for (size_t role = 0; role < size_t(MToonSlot::Count); ++role) {
      check(cancelled);
      const auto &texture = source.material_bindings->material.textures[role];
      if (role != size_t(MToonSlot::Matcap) && texture.index >= 0 &&
          mtoon_coordinate(texture) != 0)
        throw std::logic_error(
            "Bake MToon UV1 roles to canonical UV0 before polygon conversion; "
            "original material was retained");
    }
}
std::shared_ptr<const SourceMaterialBindings>
converted_material(const SculptObject &source,
                   const std::function<bool()> &cancelled) {
  if (!source.mesh_active)
    return {};
  if (source.material_bindings) {
    if (!source.material_bindings->has_mtoon)
      return {};
    auto binding =
        std::make_shared<SourceMaterialBindings>(*source.material_bindings);
    // This is an explicit canonical conversion, never a relabelled original
    // accessor/frame. The complete original binding remains in PolygonOrigin.
    binding->normal_frames.reset();
    binding->normal_frame = SourceMaterialNormalFrame::UvDerivative;
    binding->validate();
    return binding;
  }
  auto typed = retained_mtoon(source.appearance.get());
  if (!typed)
    return {};
  auto binding = std::make_shared<SourceMaterialBindings>();
  binding->has_mtoon = true;
  binding->material = *typed;
  binding->native_reference = native_source(*source.appearance);
  for (size_t role = 0; role < size_t(MToonSlot::Count); ++role) {
    check(cancelled);
    const auto &slot = typed->textures[role];
    if (slot.index < 0)
      continue;
    if (role != size_t(MToonSlot::Matcap) && mtoon_coordinate(slot) != 0)
      throw std::logic_error("Bake legacy MToon UV1 before polygon conversion");
    const auto &texture =
        source.appearance->asset->textures.at(size_t(slot.index));
    auto &view = binding->roles[role];
    view.image = source.appearance->decoded_images->at(size_t(texture.image));
    view.wrap_s = texture.wrap_s;
    view.wrap_t = texture.wrap_t;
    view.min_filter = texture.min_filter;
    view.mag_filter = texture.mag_filter;
  }
  binding->validate();
  return binding;
}
std::shared_ptr<const PolygonOrigin>
capture_origin(const SculptObject &source, CanvasTransform world,
               std::shared_ptr<const SourcePlacementCapsule> placement = {}) {
  auto origin = std::make_shared<PolygonOrigin>();
  origin->volume =
      std::make_shared<const Volume>(source.volume->geometry_snapshot());
  if (source.painted)
    origin->paint = source.painted->snapshot();
  origin->material = source.material;
  origin->deformation = source.deformation;
  if (source.appearance)
    origin->appearance =
        std::make_shared<const CreatorAppearance>(*source.appearance);
  origin->material_bindings = source.material_bindings;
  origin->shape_source = source.shape_source;
  origin->retopology = source.retopology;
  origin->polygon = source.polygon;
  origin->mesh_active = source.mesh_active;
  origin->world_transform = world;
  origin->placement = std::move(placement);
  origin->name = source.name;
  origin->source_object = source.id;
  return origin;
}
} // namespace
std::shared_ptr<const PolygonOrigin>
capture_polygon_origin(const SculptObject &source, CanvasTransform world) {
  return capture_origin(source, world);
}
std::shared_ptr<const PolygonOrigin> capture_polygon_origin(const SculptObject &source,CanvasTransform world,
    std::shared_ptr<const SourcePlacementCapsule> placement) {
  return capture_origin(source, world, std::move(placement));
}
std::shared_ptr<const PolygonOrigin> capture_polygon_origin(const Scene &scene,
                                                            uint64_t id) {
  auto placement = capture_source_placement(scene, id);
  const auto world = placement ? CanvasTransform{} : scene.world_transform(id);
  return capture_origin(scene.object(id), world, std::move(placement));
}
struct PolygonConversionAccess {
  static PolygonMesh evaluated(const PolygonMesh &source, const Mesh &rest,
                               const Mesh &pose, const PolygonLimits &limits) {
    auto packed = source.triangulate(limits);
    if (packed.mesh.positions.size() != rest.positions.size() ||
        rest.indices != packed.mesh.indices || rest.uv != packed.mesh.uv ||
        pose.positions.size() != rest.positions.size())
      throw std::invalid_argument(
          "Polygon pose conversion requires its exact current rest derivative");
    std::map<PolygonId, Vec3> positions;
    for (size_t i = 0; i < packed.vertices.size(); ++i) {
      check(limits.cancelled);
      if (!equal(packed.mesh.positions[i], rest.positions[i]))
        throw std::invalid_argument(
            "Polygon rest derivative changed before conversion");
      auto [p, inserted] =
          positions.emplace(packed.vertices[i], pose.positions[i]);
      if (!inserted && !equal(p->second, pose.positions[i]))
        throw std::invalid_argument(
            "A shared polygon vertex deforms inconsistently; original rig was "
            "retained");
    }
    auto out = source;
    for (auto &vertex : out.vertices_) {
      check(limits.cancelled);
      vertex.position = positions.at(vertex.id);
    }
    out.validate(limits);
    return out;
  }
  static PolygonMesh evaluated_source(const PolygonMesh &source,
                                      const Mesh &rest, const Mesh &pose,
                                      std::span<const uint32_t> mapping,
                                      const PolygonLimits &limits) {
    auto packed = source.triangulate(limits);
    if (mapping.size() != packed.mesh.positions.size() ||
        pose.positions.size() != rest.positions.size())
      throw std::invalid_argument(
          "Original polygon pose requires a complete native corner mapping");
    std::map<PolygonId, Vec3> positions;
    for (size_t i = 0; i < mapping.size(); ++i) {
      check(limits.cancelled);
      auto native = mapping[i];
      if (native >= rest.positions.size() ||
          !equal(packed.mesh.positions[i], rest.positions[native]))
        throw std::invalid_argument("Polygon pose source map changed its exact "
                                    "original REST positions");
      auto [it, inserted] =
          positions.emplace(packed.vertices[i], pose.positions[native]);
      if (!inserted && !equal(it->second, pose.positions[native]))
        throw std::invalid_argument(
            "A shared original polygon vertex deforms inconsistently; original "
            "source was retained");
    }
    auto out = source;
    for (auto &vertex : out.vertices_) {
      check(limits.cancelled);
      vertex.position = positions.at(vertex.id);
    }
    out.validate(limits);
    return out;
  }
  static PolygonMesh uv(const PolygonMesh &source,
                        const std::map<PolygonId, std::array<float, 2>> &values,
                        const PolygonLimits &limits) {
    auto result = source;
    for (auto &face : result.faces_)
      for (auto &corner : face.corners) {
        check(limits.cancelled);
        auto value = values.at(corner.id);
        if (corner.uv != value) {
          corner.uv = value;
          corner.generated_uv = false;
        }
      }
    result.validate(limits);
    return result;
  }
  static PolygonMesh atlas(const PolygonMesh &source, uint32_t resolution,
                           const PolygonLimits &limits) {
    auto before = source.triangulate(limits);
    auto after = before.mesh;
    check(limits.cancelled);
    unwrap(after, resolution, 4, limits.cancelled);
    check(limits.cancelled);
    if (after.indices.size() != before.mesh.indices.size())
      throw std::runtime_error(
          "Polygon atlas changed its triangle correspondence");
    std::map<PolygonId, std::array<float, 2>> uvs;
    for (size_t c = 0; c < after.indices.size(); ++c) {
      check(limits.cancelled);
      auto a = before.mesh.indices[c], b = after.indices[c];
      if (!equal(before.mesh.positions[a], after.positions[b]) ||
          !equal(before.mesh.normals[a], after.normals[b]))
        throw std::runtime_error(
            "Polygon atlas changed its geometric derivative");
      auto id = before.corners[a];
      auto [it, inserted] = uvs.emplace(id, after.uv[b]);
      if (!inserted && it->second != after.uv[b])
        throw std::invalid_argument(
            "UV packing split an N-gon corner internally; cut that face "
            "explicitly before packing");
    }
    auto out = source;
    for (auto &face : out.faces_)
      for (auto &corner : face.corners) {
        check(limits.cancelled);
        corner.uv = uvs.at(corner.id);
        corner.generated_uv = false;
      }
    out.validate(limits);
    return out;
  }
};
bool polygon_conversion_required(const SculptObject &object) {
  const bool current_polygon = object.polygon && object.polygon->active;
  if (current_polygon && !object.deformation && !object.appearance &&
      (!object.material_bindings || !object.material_bindings->normal_frames))
    return false;
  return bool(object.deformation) || bool(object.appearance) ||
         bool(object.material_bindings);
}
PolygonMesh
prepare_polygon_workspace_topology(const SculptObject &source, bool convert,
                                   const std::function<bool()> &cancelled) {
  check(cancelled);
  if (polygon_conversion_required(source) && !convert)
    throw std::logic_error("Convert with retained source before editing this "
                           "imported or deformed mesh");
  if (convert)
    transport_policy(source, cancelled);
  PolygonLimits limits;
  limits.cancelled = cancelled;
  if (source.polygon && source.polygon->active) {
    if (convert && source.mesh_active && source.deformation)
      return PolygonConversionAccess::evaluated(
          source.polygon->topology, source.painted->mesh(),
          source.deformation->preview(source.painted->mesh()), limits);
    return source.polygon->topology;
  }
  if (source.mesh_active && source.painted) {
    auto mesh = convert && source.deformation
                    ? source.deformation->preview(source.painted->mesh())
                    : source.painted->mesh();
    return prepare_polygon_topology(mesh, source.appearance.get(), limits)
        .topology;
  }
  return prepare_polygon_topology(surface_mesh(*source.volume, cancelled),
                                  nullptr, limits)
      .topology;
}
PolygonWorkspaceSource
prepare_polygon_workspace_source(const SculptObject &source, bool convert,
                                 const std::function<bool()> &cancelled) {
  check(cancelled);
  if (source.polygon && source.polygon->active && source.polygon->colors) {
    auto topology =
        prepare_polygon_workspace_topology(source, convert, cancelled);
    source.polygon->colors->validate(topology,
                                     PolygonLimits{.cancelled = cancelled});
    return {std::move(topology), source.polygon->colors};
  }
  if (source.mesh_active && source.painted && source.appearance &&
      !detail::source_vertex_colors(source.appearance.get(),
                                    source.painted->mesh().positions.size(),
                                    cancelled)
           .empty()) {
    if (!convert)
      throw std::logic_error(
          "Convert with retained source before editing original vertex colors");
    transport_policy(source, cancelled, true);
    PolygonLimits limits;
    limits.cancelled = cancelled;
    auto captured = prepare_polygon_source_colors(*source.painted,
                                                  source.appearance, limits);
    auto topology = std::move(captured.geometry.topology);
    if (source.deformation)
      topology = PolygonConversionAccess::evaluated_source(
          topology, source.painted->mesh(),
          source.deformation->preview(source.painted->mesh()),
          captured.geometry.source_vertices, limits);
    return {std::move(topology), std::make_shared<const PolygonColorField>(
                                     std::move(captured.field))};
  }
  return {prepare_polygon_workspace_topology(source, convert, cancelled), {}};
}
void adopt_polygon_edit(Scene &scene, uint64_t id, const PolygonMesh &input,
                        const SculptObject &original,
                        CanvasTransform original_world, bool convert,
                        const std::function<bool()> &cancelled) {
  adopt_polygon_edit(scene, id, input, {}, original, original_world, convert,
                     cancelled);
}
void adopt_polygon_edit(Scene &scene, uint64_t id, const PolygonMesh &input,
                        std::shared_ptr<const PolygonColorField> colors,
                        const SculptObject &original,
                        CanvasTransform original_world, bool convert,
                        const std::function<bool()> &cancelled) {
  check(cancelled);
  const auto &current = scene.object(id);
  if (current.id != original.id ||
      current.volume->geometry_owner() != original.volume->geometry_owner() ||
      current.volume->revision() != original.volume->revision() ||
      current.mesh_active != original.mesh_active ||
      current.deformation != original.deformation ||
      current.appearance != original.appearance ||
      current.material_bindings != original.material_bindings ||
      current.polygon != original.polygon ||
      current.affine_edit != original.affine_edit ||
      bool(current.painted) != bool(original.painted) ||
      (current.painted &&
       (current.painted->mesh_owner() != original.painted->mesh_owner() ||
        current.painted->revision() != original.painted->revision())) ||
      !detail::same_material(current.material, original.material))
    throw std::invalid_argument(
        "Polygon source authority changed before adoption");
  const bool conversion = polygon_conversion_required(current);
  if (conversion && !convert)
    throw std::logic_error("Source conversion was not accepted");
  if (conversion)
    transport_policy(current, cancelled, bool(colors));
  PolygonLimits limits;
  limits.cancelled = cancelled;
  input.validate(limits);
  if (current.polygon && current.polygon->active && current.polygon->colors) {
    if (!colors ||
        colors->source_owner() != current.polygon->colors->source_owner())
      throw std::invalid_argument("Polygon edit must preserve its current "
                                  "immutable corner color basis");
  } else if (colors && current.mesh_active && current.painted &&
             current.appearance) {
    auto captured = prepare_polygon_source_colors(*current.painted,
                                                  current.appearance, limits);
    const auto &a = colors->sources(), &b = captured.field.sources();
    if (a.size() != b.size() || !std::equal(a.begin(), a.end(), b.begin(),
                                            [](const auto &x, const auto &y) {
                                              return x.corner == y.corner &&
                                                     x.value == y.value;
                                            }))
      throw std::invalid_argument("Polygon colors do not retain the actual "
                                  "original corner accessor basis");
  } else if (colors)
    throw std::invalid_argument(
        "Polygon edit has no matching original current color source");
  if (colors)
    colors->validate(input, limits);
  auto authority = std::make_shared<PolygonAuthority>();
  authority->topology = input;
  authority->colors = std::move(colors);
  authority->origin = original.polygon && original.polygon->active &&
                              !conversion && original.polygon->origin
                          ? original.polygon->origin
                          : capture_origin(original, original_world,
                                           capture_source_placement(scene, id));
  auto result = scene;
  auto &target = result.object(id);
  if (!input.faces().empty()) {
    const bool retain_paint = current.mesh_active && bool(current.painted);
    if (!retain_paint)
      authority->topology = PolygonConversionAccess::atlas(input, 512, limits);
    auto mesh = authority->topology.triangulate(limits).mesh;
    PaintSurfaceOptions options;
    options.cancelled = cancelled;
    options.policy =
        retain_paint ? PaintUvPolicy::PolygonAuthored : PaintUvPolicy::Strict;
    target.painted =
        retain_paint
            ? polygon_paint_geometry(*current.painted, std::move(mesh), options)
            : PaintedMesh::from_mesh(std::move(mesh), 512, options);
    target.mesh_active = true;
    if (conversion) {
      target.appearance.reset();
      target.deformation.reset();
      target.material_bindings = converted_material(current, cancelled);
      target.retopology.reset();
    }
  } else {
    // An empty active authority has no paint derivative. Retain the complete
    // latest paint/rig/source in a new bounded origin whose prior authority
    // still retains the first original; never leave a stale Volume fallback.
    authority->origin = capture_polygon_origin(scene, id);
    target.mesh_active = false;
    target.painted.reset();
    target.deformation.reset();
    target.appearance.reset();
    target.material_bindings.reset();
    target.retopology.reset();
  }
  target.polygon = std::move(authority);
  result.validate();
  check(cancelled);
  scene = std::move(result);
}
PolygonUvDerivative
prepare_polygon_uv_derivative(const SculptObject &source, const Mesh &target,
                              std::span<const uint32_t> mapping,
                              const std::function<bool()> &cancelled) {
  check(cancelled);
  if (!source.polygon || !source.polygon->active || !source.painted ||
      !source.mesh_active)
    throw std::logic_error(
        "UV update requires the current active polygon derivative");
  PolygonLimits limits;
  limits.cancelled = cancelled;
  validate_polygon_derivative(*source.polygon, source.painted.get(), true,
                              limits);
  const auto &rest = source.painted->mesh();
  target.validate();
  if (target.indices.size() != rest.indices.size() ||
      target.uv.size() != target.positions.size() ||
      mapping.size() != target.positions.size() ||
      target.positions.size() > 8'000'000)
    throw std::invalid_argument(
        "Polygon UV remap changed its source triangle domain");
  auto packed = source.polygon->topology.triangulate(limits);
  std::map<PolygonId, std::array<float, 2>> uvs;
  std::map<PolygonId, uint8_t> pins;
  for (size_t c = 0; c < rest.indices.size(); ++c) {
    check(cancelled);
    auto before = rest.indices[c], after = target.indices[c];
    if (after >= mapping.size() || mapping[after] != before ||
        !equal(rest.positions[before], target.positions[after]) ||
        !equal(rest.normals[before], target.normals[after]))
      throw std::invalid_argument("Polygon UV remap changed source geometry, "
                                  "normals or ordered triangle corners");
    const auto id = packed.corners[before];
    auto [it, inserted] = uvs.emplace(id, target.uv[after]);
    if (!inserted && it->second != target.uv[after])
      throw std::invalid_argument(
          "UV packing split one N-gon corner internally; cut that face "
          "explicitly before editing its charts");
    if (!target.uv_pins.empty()) {
      auto [pin, added] = pins.emplace(id, target.uv_pins[after]);
      if (!added && pin->second != target.uv_pins[after])
        throw std::invalid_argument(
            "One N-gon corner has conflicting UV pin identities; cut that face "
            "explicitly first");
    }
  }
  auto authority = std::make_shared<PolygonAuthority>(*source.polygon);
  authority->topology =
      PolygonConversionAccess::uv(source.polygon->topology, uvs, limits);
  auto derivative = authority->topology.triangulate(limits);
  derivative.mesh.normals =
      rest.normals; // Original canonical/custom normals stay exact.
  derivative.mesh.uv_seams = target.uv_seams;
  if (!pins.empty())
    for (auto corner : derivative.corners)
      derivative.mesh.uv_pins.push_back(pins.at(corner));
  derivative.mesh.validate();
  PolygonUvDerivative out;
  out.authority = std::move(authority);
  out.mesh = std::move(derivative.mesh);
  out.source_vertices.reserve(out.mesh.positions.size());
  for (size_t i = 0; i < out.mesh.positions.size(); ++i) {
    check(cancelled);
    out.source_vertices.push_back(uint32_t(i));
  }
  return out;
}
void restore_polygon_origin(Scene &scene, uint64_t id,
                            const std::function<bool()> &cancelled) {
  check(cancelled);
  const auto &object = scene.object(id);
  if (!object.polygon || !object.polygon->origin)
    throw std::logic_error(
        "This polygon object has no retained original source");
  const auto &origin = *object.polygon->origin;
  auto result = scene;
  auto &target = result.object(id);
  target.volume = std::make_shared<Volume>(origin.volume->geometry_snapshot());
  target.painted = origin.paint ? origin.paint->snapshot() : nullptr;
  target.mesh_active = origin.mesh_active;
  target.material = origin.material;
  target.deformation = origin.deformation;
  target.appearance = origin.appearance;
  target.material_bindings = origin.material_bindings;
  target.shape_source = origin.shape_source;
  target.retopology = origin.retopology;
  target.polygon = origin.polygon;
  // Keep current object identity, placement/group and user name. Original
  // placement/name remain provenance; global Undo restores the exact old Scene.
  result.validate();
  check(cancelled);
  scene = std::move(result);
}
} // namespace dome
