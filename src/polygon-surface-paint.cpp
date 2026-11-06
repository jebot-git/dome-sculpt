// SPDX-License-Identifier: GPL-3.0-or-later
#include "polygon-surface-paint.hpp"
#include "dome/source-paint.hpp"
#include <nlohmann/json.hpp>
namespace dome::detail {namespace {
void stop(const PolygonSurfaceTransferLimits&l){if(l.geometry.cancelled&&l.geometry.cancelled())throw std::runtime_error("Polygon surface paint codec cancelled");}
size_t reference(const nlohmann::json&v,size_t count){if((!v.is_number_unsigned()&&(!v.is_number_integer()||v.get<int64_t>()<0))||v.get<uint64_t>()>=count)throw std::invalid_argument("Polygon Paint6 node reference exceeds its actual owning pool");return size_t(v.get<uint64_t>());}
void simultaneous(size_t current,size_t reserved,const PolygonSurfaceTransferLimits&l){
 if(!l.maximum_working_bytes||l.maximum_working_bytes>512*1024*1024||reserved>l.maximum_working_bytes||current>(l.maximum_working_bytes-reserved)/3)throw std::length_error("Polygon Paint6 payload plus owning replay exceeds prospective working quota");
}
}
size_t PolygonSurfacePaintCodec::preflight(const nlohmann::json&input,size_t nodes,const PolygonSurfaceTransferLimits&l){
 stop(l);if(!input.is_object()||input.size()!=4||!input.at("version").is_number_integer()||input.at("version")!=6||!input.at("physical_paint").is_boolean()||!input.at("payload").is_object()||!nodes||nodes>128)throw std::invalid_argument("Invalid typed polygon Paint6 envelope");
 (void)reference(input.at("node"),nodes);const auto&payload=input.at("payload");
 for(const char*key:{"version","payload","domain","source_maps","normal_projection","color_projection","source_images","uv_policy"})if(payload.contains(key))throw std::invalid_argument("Polygon Paint6 payload contradicts its actual independent native grid model");
 // Borrow JSON. Charge every binary/string/container node before any current
 // mesh/layer copy or replay. The limit is conservative, includes metadata,
 // and does not replace aggregate Scene graph/image/payload preflight.
 size_t bytes=0,entries=0;constexpr size_t cap=128*1024*1024;
 const auto charge=[&](size_t n){if(n>cap-bytes)throw std::length_error("Polygon Paint6 encoded current payload exceeds its bounded copy quota");bytes+=n;};
 const auto scan=[&](auto&&self,const nlohmann::json&value,size_t depth)->void{
  stop(l);if(depth>16||++entries>8'000'000)throw std::length_error("Polygon Paint6 current payload has excessive depth or metadata");charge(sizeof(nlohmann::json)+32);
  if(value.is_binary())charge(value.get_binary().size());
  else if(value.is_string())charge(value.get_ref<const std::string&>().size());
  else if(value.is_array())for(const auto&child:value)self(self,child,depth+1);
  else if(value.is_object())for(auto i=value.begin();i!=value.end();++i){charge(i.key().size()+64);self(self,i.value(),depth+1);}
 };
 scan(scan,payload,0);simultaneous(bytes,0,l);stop(l);return bytes;
}
nlohmann::json PolygonSurfacePaintCodec::encode(const PaintedMesh&paint,
 const std::shared_ptr<const SourceMaterialBindings>&binding,PolygonSurfaceNodeWriter&nodes,
 PaintImageArchive&images,NormalSamplingArchive*normal,const PolygonSurfaceTransferLimits&l){
 stop(l);if(!binding||!binding->polygon_surface_transfer)throw std::invalid_argument("Polygon Paint6 writer requires its actual current material owner");
 binding->validate();
 const auto&owner=binding->polygon_surface_transfer;owner->validate_binding(*binding,l);owner->validate_current(paint,l);
 simultaneous(paint.storage_bytes(),nodes.reserved_working_bytes(),l);
 const auto id=nodes.add(owner);simultaneous(paint.storage_bytes(),nodes.reserved_working_bytes(),l);
 auto payload=paint.encode_native_payload(&images,normal,nullptr,owner.get());
 auto result=nlohmann::json{{"version",6},{"node",id},{"physical_paint",paint.has_physical_emission()},{"payload",std::move(payload)}};
 const auto bytes=preflight(result,nodes.size(),l);simultaneous(bytes,nodes.reserved_working_bytes(),l);stop(l);return result;
}
std::shared_ptr<PaintedMesh> PolygonSurfacePaintCodec::decode(const nlohmann::json&input,
 PolygonSurfaceNodeReader&nodes,const PaintImageArchive&images,const NormalSamplingArchive*normal,
 const PolygonSurfaceTransferLimits&l){
 const auto bytes=preflight(input,nodes.size(),l);const auto&stage=nodes.at(uint32_t(reference(input.at("node"),nodes.size())));
 simultaneous(bytes,nodes.reserved_working_bytes(),l);
 auto paint=PaintedMesh::decode_native_payload(input.at("payload"),&images,false,normal,nullptr,
     SourcePaintDecodeContext{false,input.at("physical_paint").get<bool>()},&stage);
 if(paint->has_physical_emission()!=input.at("physical_paint").get<bool>())throw std::invalid_argument("Polygon Paint6 physical flag differs from actual current float planes");
 stage.owner->validate_current(*paint,l);stop(l);return paint;
}
} // namespace dome::detail
