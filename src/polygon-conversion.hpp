// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/polygon-scene.hpp"
namespace dome {
// One source-capture seam for topology conversion and explicit strict-UV color
// preparation. Every direct origin retains its own complete prior authority.
std::shared_ptr<const PolygonOrigin>
capture_polygon_origin(const SculptObject &, CanvasTransform);
std::shared_ptr<const PolygonOrigin> capture_polygon_origin(const Scene &,uint64_t);
std::shared_ptr<const PolygonOrigin> capture_polygon_origin(const SculptObject &,CanvasTransform,
    std::shared_ptr<const SourcePlacementCapsule>);
bool polygon_conversion_required(const SculptObject &);
struct PolygonWorkspaceSource {
  PolygonMesh topology;
  std::shared_ptr<const PolygonColorField> colors;
};
PolygonWorkspaceSource
prepare_polygon_workspace_source(const SculptObject &, bool convert_source,
                                 const std::function<bool()> & = {});
void adopt_polygon_edit(Scene &, uint64_t, const PolygonMesh &,
                        std::shared_ptr<const PolygonColorField>,
                        const SculptObject &original,
                        CanvasTransform original_world, bool convert_source,
                        const std::function<bool()> & = {});

PolygonMesh
prepare_polygon_workspace_topology(const SculptObject &, bool convert_source,
                                   const std::function<bool()> & = {});
void adopt_polygon_edit(Scene &, uint64_t, const PolygonMesh &,
                        const SculptObject &original,
                        CanvasTransform original_world, bool convert_source,
                        const std::function<bool()> & = {});
struct PolygonUvDerivative {
  std::shared_ptr<const PolygonAuthority> authority;
  Mesh mesh;
  std::vector<uint32_t> source_vertices;
};
PolygonUvDerivative
prepare_polygon_uv_derivative(const SculptObject &, const Mesh &,
                              std::span<const uint32_t> source_vertices,
                              const std::function<bool()> & = {});
void restore_polygon_origin(Scene &, uint64_t,
                            const std::function<bool()> & = {});
} // namespace dome
