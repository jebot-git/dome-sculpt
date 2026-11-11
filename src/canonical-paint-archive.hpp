// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "canonical-paint-sampling.hpp"
#include "polygon-scene-archive.hpp"
#include "baked-normal-archive.hpp"
#include <nlohmann/json.hpp>
namespace dome::detail {
// One synchronous owning Scene operation. Nested operations restore the exact
// prior context; no reader, resolver or validation result escapes the call.
class CanonicalPaintArchive {
public:
 CanonicalPaintArchive(PolygonSceneArchive &,BakedNormalArchive &,PaintSamplingArchive &);
 CanonicalPaintArchive(const nlohmann::json &,PolygonSceneReader &,BakedNormalArchive &,const PaintSamplingArchive &);
 ~CanonicalPaintArchive();
 CanonicalPaintArchive(const CanonicalPaintArchive&)=delete;
 static CanonicalPaintArchive &writer();
 static CanonicalPaintArchive &reader();
 uint32_t add(std::shared_ptr<const CanonicalPaintSampling>);
 std::shared_ptr<const CanonicalPaintSampling> at(uint32_t);
 nlohmann::json encode() const;
 PolygonSceneReader::MaterialResolver material_resolver(PolygonSceneReader::MaterialResolver);
 void restore_material_resolver(PolygonSceneReader::MaterialResolver) noexcept;
 void require_complete() const;
 size_t size() const noexcept{return nodes_.size();}
 static size_t preflight(const nlohmann::json &,const nlohmann::json &polygons,
     const nlohmann::json &normals,const nlohmann::json &domains,const nlohmann::json &materials,
     const SourcePaintDecodeContext &);
private:
 static thread_local CanonicalPaintArchive *current_;
 CanonicalPaintArchive *previous_;
 PolygonSceneArchive *write_origins_=nullptr;
 PolygonSceneReader *read_origins_=nullptr;
 BakedNormalArchive &normals_;
 PaintSamplingArchive *write_domains_=nullptr;
 const PaintSamplingArchive &domains_;
 const nlohmann::json *input_=nullptr;
 std::vector<std::shared_ptr<const CanonicalPaintSampling>> nodes_;
 std::vector<uint8_t> busy_,used_;
};
}
