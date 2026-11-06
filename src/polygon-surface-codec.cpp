// SPDX-License-Identifier: GPL-3.0-or-later
#include "polygon-surface-codec.hpp"
#include "dome/paint-sampling.hpp"
#include "paint-sampling-access.hpp"
#include <nlohmann/json.hpp>
#include <limits>
namespace dome::detail {namespace {
void stop(const PolygonSurfaceTransferLimits&l){if(l.geometry.cancelled&&l.geometry.cancelled())throw std::runtime_error("Polygon surface node codec cancelled");}
bool tag(const nlohmann::json&j,std::string_view value){return j.is_string()&&j.get_ref<const std::string&>()==value;}
uint64_t integer(const nlohmann::json&j,uint64_t limit){if((!j.is_number_unsigned()&&(!j.is_number_integer()||j.get<int64_t>()<0))||j.get<uint64_t>()>limit)throw std::invalid_argument("Polygon surface node integer exceeds its actual reference/scalar range");return j.get<uint64_t>();}
std::string_view relation(const PolygonOrigin&before){
 if(!before.paint)throw std::invalid_argument("Polygon surface alias witness needs its actual before Paint");
 const auto domain=before.paint->paint_sampling_domain();if(domain&&domain->version()!=1)throw std::invalid_argument("Polygon surface prior-domain composition needs its actual owning adapter");
 return domain?"captured-retained-grid":"captured-native-grid";
}
}
nlohmann::json encode_polygon_surface_node(const std::shared_ptr<const PolygonSurfaceOwner>&owner,
 const PolygonSurfaceOriginWriter&origin,const PolygonSurfaceTransferLimits&l){
 stop(l);if(!origin||!owner||!owner->valid_owner(owner)||!owner->current_attachment()||!owner->current_attachment()->domain()||owner->current_attachment()->domain()->version()!=3)throw std::invalid_argument("Polygon surface writer requires a genuine current geometry/paint owner");
 owner->validate(l);auto record=nlohmann::json{{"version",1},{"before_origin",origin(owner->before())},{"basis",relation(*owner->before())},{"recipe",owner->encode_recipe(l)}};stop(l);return record;
}
size_t preflight_polygon_surface_node(const nlohmann::json&record,size_t origins,const PolygonSurfaceTransferLimits&l){
 stop(l);if(!l.maximum_steps||l.maximum_steps>128||!l.maximum_entries||l.maximum_entries>8'000'000||!l.maximum_working_bytes||l.maximum_working_bytes>512*1024*1024||!origins||origins>512)throw std::invalid_argument("Invalid polygon surface node resource/origin context");
 if(!record.is_object()||record.size()!=4||!record.at("version").is_number_integer()||record.at("version")!=1||!record.at("basis").is_string()||(!tag(record.at("basis"),"captured-retained-grid")&&!tag(record.at("basis"),"captured-native-grid")))throw std::invalid_argument("Invalid polygon surface owning node shape/version/basis");
 integer(record.at("before_origin"),origins-1);const auto&recipe=record.at("recipe");
 if(!recipe.is_object()||recipe.size()!=4||!recipe.at("version").is_number_integer()||recipe.at("version")!=1||!tag(recipe.at("model"),"polygon-original-field-current-surface-v1")||(!tag(recipe.at("normal_evaluation"),"core-pbr-unit-columns")&&!tag(recipe.at("normal_evaluation"),"native-normal-only"))||!recipe.at("operations").is_array()||recipe.at("operations").empty()||recipe.at("operations").size()>l.maximum_steps)throw std::invalid_argument("Invalid polygon surface owning edit recipe");
 size_t entries=0,bytes=0;const auto charge=[&](size_t n,size_t unit){if(bytes>l.maximum_working_bytes||n>(l.maximum_working_bytes-bytes)/unit)throw std::length_error("Polygon surface recipe exceeds prospective working quota");bytes+=n*unit;};
 for(const auto&s:recipe.at("operations")){
  stop(l);if(!s.is_object()||s.size()!=7||(!tag(s.at("operation"),"move")&&!tag(s.at("operation"),"flip"))||!s.at("selection").is_object()||s.at("selection").size()!=2||!s.at("selection").at("ids").is_array())throw std::invalid_argument("Invalid polygon surface operation record");
  const auto&selection=s.at("selection");integer(selection.at("mode"),2);const auto&ids=selection.at("ids");if(ids.size()>l.maximum_entries-entries)throw std::length_error("Polygon surface recipe exceeds aggregate edit-entry quota");entries+=ids.size();charge(1,sizeof(PolygonSurfaceStep));charge(ids.size(),sizeof(PolygonId));for(const auto&id:ids){stop(l);integer(id,UINT64_MAX);}
  const auto&delta=s.at("delta");if(!delta.is_array()||delta.size()!=3)throw std::invalid_argument("Polygon surface delta dimensions differ");for(const auto&v:delta){if(!v.is_number()||!std::isfinite(v.get<double>())||!std::isfinite(v.get<float>()))throw std::invalid_argument("Polygon surface delta is outside finite float storage");}
  if(!s.at("amount").is_number()||!std::isfinite(s.at("amount").get<float>()))throw std::invalid_argument("Nonfinite polygon surface amount");
  integer(s.at("cuts"),std::numeric_limits<unsigned>::max());integer(s.at("first_id"),UINT64_MAX);
  if(!s.at("world").is_null()){const auto&world=s.at("world");if(!world.is_array()||world.size()!=16)throw std::invalid_argument("Polygon surface world dimensions differ");for(const auto&v:world)if(!v.is_number()||!std::isfinite(v.get<double>()))throw std::invalid_argument("Nonfinite polygon surface world witness");}
 }
 stop(l);return entries;
}
PolygonSurfaceStage decode_polygon_surface_node(const nlohmann::json&record,size_t origins,
 const PolygonSurfaceOriginReader&origin,const PolygonSurfaceTransferLimits&l){
 (void)preflight_polygon_surface_node(record,origins,l);if(!origin)throw std::invalid_argument("Polygon surface node has no actual owning origin reader");
 auto before=origin(uint32_t(integer(record.at("before_origin"),origins-1)));if(!before||!tag(record.at("basis"),relation(*before)))throw std::invalid_argument("Polygon surface basis witness differs from its actual source ownership");
 // The basis is selected from this authentic source root, not reconstructed by
 // equal-position/UV merging with an unrelated Mesh table. The real factory
 // replays the full before graph and edit program, then seals its SurfaceMap.
 auto owner=PolygonSurfaceOwner::replay(std::move(before),record.at("recipe"),l);auto stage=owner->stage_paint(l);stage.owner->validate_current(*stage.paint,l);stop(l);return stage;
}
namespace {
void preflight_pool(const nlohmann::json&input,size_t origins,const PolygonSurfaceTransferLimits&l){
 stop(l);if(!input.is_object()||input.size()!=2||!input.at("version").is_number_integer()||input.at("version")!=1||!input.at("nodes").is_array()||input.at("nodes").empty()||input.at("nodes").size()>128)throw std::invalid_argument("Invalid bounded polygon surface owning-node pool");
 size_t entries=0;for(const auto&node:input.at("nodes")){const auto n=preflight_polygon_surface_node(node,origins,l);if(n>l.maximum_entries-entries)throw std::length_error("Polygon surface pool exceeds aggregate edit-entry quota");entries+=n;}stop(l);
}
bool same_owner(const auto&a,const auto&b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);}
size_t replay_reservation(const nlohmann::json&node,const PolygonOrigin&before,const PolygonSurfaceTransferLimits&l){
 size_t bytes=PolygonSurfaceOwner::source_working_bytes(before,l);
 const auto charge=[&](size_t count,size_t unit){if(bytes>l.maximum_working_bytes||count>(l.maximum_working_bytes-bytes)/unit)throw std::length_error("Polygon surface pool exceeds prospective replay quota");bytes+=count*unit;};
 // This first boundary changes no source triangle count. Reserve the complete
 // raster constructor, not only final map storage. Keeping every node's peak
 // reserved after success is conservative and never aliases equal geometry.
 const auto corners=before.paint->mesh().indices.size();
 charge(paint_sampling_surface_working_bytes(corners,corners,before.paint->resolution()),1);
 for(const auto&step:node.at("recipe").at("operations")){charge(1,sizeof(PolygonSurfaceStep)+4096);charge(step.at("selection").at("ids").size(),sizeof(PolygonId)+3*sizeof(nlohmann::json));}
 return bytes;
}
}
PolygonSurfaceNodeWriter::PolygonSurfaceNodeWriter(const PolygonSurfaceTransferLimits&l):limits_(l){
 // Covers bounded index vector capacity and reallocation overlap. Source and
 // raster reservations are charged separately before committing each owner.
 reserved_=4096;if(reserved_>limits_.maximum_working_bytes)throw std::length_error("Polygon surface writer index exceeds prospective working quota");stop(limits_);
}
uint32_t PolygonSurfaceNodeWriter::add(std::shared_ptr<const PolygonSurfaceOwner>owner){
 stop(limits_);if(!owner||!owner->valid_owner(owner)||!owner->current_attachment())throw std::invalid_argument("Polygon surface writer has no genuine connected owner");
 owner->validate(limits_);
 for(size_t i=0;i<owners_.size();++i)if(same_owner(owners_[i],owner))return uint32_t(i);
 if(owners_.size()>=128)throw std::length_error("Polygon surface owning-node pool exceeds its count limit");
 const auto node=nlohmann::json{{"version",1},{"before_origin",0},{"basis",relation(*owner->before())},{"recipe",owner->encode_recipe(limits_)}};
 const auto count=preflight_polygon_surface_node(node,1,limits_);if(count>limits_.maximum_entries-entries_)throw std::length_error("Polygon surface writer exceeds aggregate edit-entry quota");
 const auto bytes=replay_reservation(node,*owner->before(),limits_);if(bytes>limits_.maximum_working_bytes-reserved_)throw std::length_error("Polygon surface writer exceeds aggregate replay quota");
 owners_.push_back(std::move(owner));reserved_+=bytes;entries_+=count;return uint32_t(owners_.size()-1);
}
nlohmann::json PolygonSurfaceNodeWriter::encode(const PolygonSurfaceOriginWriter&origin)const{
 stop(limits_);if(owners_.empty()||!origin)throw std::invalid_argument("Polygon surface writer needs actual owning nodes and origin pool");
 nlohmann::json output={{"version",1},{"nodes",nlohmann::json::array()}};
 for(const auto&owner:owners_)output["nodes"].push_back(encode_polygon_surface_node(owner,origin,limits_));
 stop(limits_);return output;
}
PolygonSurfaceNodeReader::PolygonSurfaceNodeReader(const nlohmann::json&input,size_t origins,
 PolygonSurfaceOriginReader origin,const PolygonSurfaceTransferLimits&l,PolygonSurfaceGeometryReader geometry)
 :input_(input),origin_count_(origins),origin_(std::move(origin)),geometry_(std::move(geometry)),limits_(l){
 preflight_pool(input_,origin_count_,limits_);if(!origin_)throw std::invalid_argument("Polygon surface pool has no actual origin resolver");
 const auto count=input_.at("nodes").size();const size_t per_node=sizeof(std::optional<PolygonSurfaceStage>)+sizeof(uint8_t);
 if(count>limits_.maximum_working_bytes/per_node)throw std::length_error("Polygon surface pool index exceeds prospective working quota");
 reserved_=count*per_node;
 stages_.resize(input_.at("nodes").size());busy_.resize(stages_.size());
}
const PolygonSurfaceStage&PolygonSurfaceNodeReader::at(uint32_t id){
 stop(limits_);preflight_pool(input_,origin_count_,limits_);
 if(input_.at("nodes").size()!=stages_.size()||id>=stages_.size())throw std::invalid_argument("Polygon surface owning-node identity is outside its actual pool");
 if(busy_[id])throw std::invalid_argument("Polygon surface source/material history contains a cycle");
 busy_[id]=1;struct Reset{uint8_t&value;~Reset(){value=0;}}reset{busy_[id]};
 const auto&node=input_.at("nodes")[id];auto before=origin_(uint32_t(integer(node.at("before_origin"),origin_count_-1)));
 if(!before||!tag(node.at("basis"),relation(*before)))throw std::invalid_argument("Polygon surface pool basis differs from its actual source ownership");
 const auto geometry=geometry_?geometry_(id):std::shared_ptr<const Mesh>{};
 if(stages_[id]){
  auto&stage=*stages_[id];if(geometry&&!same_owner(geometry,stage.paint->mesh_owner()))throw std::invalid_argument("Polygon surface shared geometry changed its actual owning context");if(!same_owner(stage.owner->before(),before)||stage.owner->encode_recipe(limits_)!=node.at("recipe"))throw std::invalid_argument("Polygon surface completed node changed its exact owning context");
  stage.owner->validate_binding(*stage.binding,limits_);stage.owner->validate_current(*stage.paint,limits_);stop(limits_);return stage;
 }
 const auto bytes=replay_reservation(node,*before,limits_);
 if(bytes>limits_.maximum_working_bytes-reserved_)throw std::length_error("Polygon surface retained nodes plus replay exceed aggregate working quota");
 // Charge before allocating the next source/mesh/grid. Failure does not commit
 // either the slot or its reservation, so an exact retry can succeed.
 auto remaining=limits_;remaining.maximum_working_bytes-=reserved_;
 auto owner=PolygonSurfaceOwner::replay(before,node.at("recipe"),remaining);auto stage=owner->stage_paint(geometry,remaining);stage.owner->validate_current(*stage.paint,remaining);stop(limits_);
 stages_[id]=std::move(stage);reserved_+=bytes;return *stages_[id];
}
void PolygonSurfaceNodeReader::require_complete(){
 stop(limits_);if(std::any_of(stages_.begin(),stages_.end(),[](const auto&v){return !v;}))throw std::invalid_argument("Unreferenced polygon surface owning node");
 for(size_t id=0;id<stages_.size();++id)(void)at(uint32_t(id));
}
} // namespace dome::detail
