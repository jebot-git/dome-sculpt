// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "polygon-surface-paint.hpp"
#include "polygon-scene-archive.hpp"
namespace dome::detail {
class PolygonSurfacePreflight final {
public:
 explicit PolygonSurfacePreflight(const nlohmann::json&scene);
 ~PolygonSurfacePreflight();
 PolygonSurfacePreflight(const PolygonSurfacePreflight&)=delete;
 static const PolygonSurfacePreflight&current();
 const nlohmann::json&nodes()const;
private:
 static thread_local const PolygonSurfacePreflight*current_;
 const PolygonSurfacePreflight*previous_=nullptr;
 const nlohmann::json&scene_;
};
// Typed per-Scene graph. No scope or callback outlives its synchronous owning
// call; every read resolves actual earlier material/origin control blocks.
class PolygonSurfaceArchive final {
public:
 explicit PolygonSurfaceArchive(PolygonSceneArchive&);
 PolygonSurfaceArchive(const nlohmann::json&,PolygonSceneReader&,size_t origin_count);
 ~PolygonSurfaceArchive();
 PolygonSurfaceArchive(const PolygonSurfaceArchive&)=delete;
 static PolygonSurfaceArchive&writer();
 static PolygonSurfaceArchive&reader();
 uint32_t add(const std::shared_ptr<const SourceMaterialBindings>&);
 std::shared_ptr<const PolygonSurfaceOwner>at(uint32_t);
 nlohmann::json encode();
 nlohmann::json encode_paint(const PaintedMesh&,PaintImageArchive&,NormalSamplingArchive*);
 std::shared_ptr<PaintedMesh>decode_paint(const nlohmann::json&,const PaintImageArchive&,const NormalSamplingArchive*);
 PolygonSceneReader::MaterialResolver material_resolver(PolygonSceneReader::MaterialResolver);
 void restore_material_resolver(PolygonSceneReader::MaterialResolver)noexcept;
 size_t size()const noexcept;
 void require_complete();
private:
 static thread_local PolygonSurfaceArchive*current_;
 PolygonSurfaceArchive*previous_=nullptr;
 PolygonSceneArchive*origins_=nullptr;
 PolygonSceneReader*read_origins_=nullptr;
 std::unique_ptr<PolygonSurfaceNodeWriter>writer_;
 std::unique_ptr<PolygonSurfaceNodeReader>reader_;
 std::vector<std::shared_ptr<const SourceMaterialBindings>>bindings_;
};
}
