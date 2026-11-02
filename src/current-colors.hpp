// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/material-bindings.hpp"
#include "dome/polygon-color.hpp"
#include "dome/scene.hpp"
#include "prepared-color-state.hpp"
#include "source-colors.hpp"
#include "polygon-color-target.hpp"
namespace dome::detail {
// All matching happens against ordered REST geometry. Evaluated pose positions
// are deliberately not an alternative identity for source/corner attributes.
inline std::vector<PolygonRgba> current_vertex_colors(
    const Mesh &mesh, const PolygonColorSurface *current,
    const CreatorAppearance *appearance, const SourceMaterialBindings *binding,
    const TextureMaps *textures, const std::function<bool()> &cancelled = {}) {
  if (!current) {
    if (prepared_color_state(binding, textures, false))
      return {};
    return source_vertex_colors(appearance, mesh.positions.size(), cancelled);
  }
  if(prepared_color_state(binding,textures,false)){
    const auto &recipe=*binding->color_preparation;
    if(!recipe.polygon_colors||!current->topology||!current->field||
       current->field!=recipe.polygon_colors->field||
       current->field.owner_before(recipe.polygon_colors->field)||recipe.polygon_colors->field.owner_before(current->field)||
       current->topology->encode()!=recipe.polygon_colors->topology->encode())
      throw std::invalid_argument("Prepared corner pixels lost their exact current geometry/field authority");
    (void)polygon_target_colors(mesh,*current,cancelled);
    recipe.validate_current(mesh,appearance,cancelled);
    // Resolved PBR maps already contain this authenticated field. An empty
    // stream intentionally suppresses duplicate raw vertex modulation.
    return {};
  }
  if (!current->topology || !current->field)
    throw std::invalid_argument(
        "Polygon colors require complete immutable geometry authority");
  PolygonLimits limits;
  limits.cancelled = cancelled;
  current->field->validate(*current->topology, limits);
  auto derivative = current->topology->triangulate(limits);
  const auto &a = derivative.mesh, &b = mesh;
  if (a.indices != b.indices || a.positions.size() != b.positions.size() ||
      a.normals.size() != b.normals.size() || a.uv != b.uv)
    throw std::invalid_argument(
        "Current polygon colors require their exact ordered rest derivative");
  for (size_t i = 0; i < a.positions.size(); ++i) {
    if ((i & 1023) == 0 && cancelled && cancelled())
      throw std::runtime_error("Current polygon color validation cancelled");
    auto p = a.positions[i], q = b.positions[i], n = a.normals[i],
         m = b.normals[i];
    if (p.x != q.x || p.y != q.y || p.z != q.z || n.x != m.x || n.y != m.y ||
        n.z != m.z)
      throw std::invalid_argument(
          "Current polygon color rest geometry changed");
  }
  return current->field->packed(*current->topology, limits);
}
inline std::vector<PolygonRgba>
current_vertex_colors(const ObjectSurface &surface,
                      const std::function<bool()> &cancelled = {}) {
  return current_vertex_colors(
      surface.mesh, surface.polygon_colors.get(), surface.appearance.get(),
      surface.material_bindings.get(), surface.textures.get(), cancelled);
}
// A validated REST stream may be expanded for independent normal/tangent
// corners. The expanded mesh is not relabelled as a new REST authority.
inline std::vector<PolygonRgba>
remap_current_vertex_colors(std::span<const PolygonRgba> input,
                            std::span<const uint32_t> vertices,
                            const std::function<bool()> &cancelled = {}) {
  if (input.empty())
    return {};
  if (vertices.size() > 256 * 1024 * 1024 / sizeof(PolygonRgba))
    throw std::length_error(
        "Expanded current color stream exceeds native byte budget");
  std::vector<PolygonRgba> result;
  result.reserve(vertices.size());
  for (size_t i = 0; i < vertices.size(); ++i) {
    if ((i & 1023) == 0 && cancelled && cancelled())
      throw std::runtime_error("Current color expansion cancelled");
    if (vertices[i] >= input.size())
      throw std::invalid_argument(
          "Expanded current color vertex is outside validated rest authority");
    result.push_back(input[vertices[i]]);
  }
  return result;
}
} // namespace dome::detail
