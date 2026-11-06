// SPDX-License-Identifier: GPL-3.0-or-later
#include "polygon-surface-codec.hpp"
#include "dome/paint-sampling.hpp"
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
  if(!s.at("amount").is_number()||!std::isfinite(s.at("amount").get<float>()))throw std::invalid_argument("Nonfinite polygon surface amount");integer(s.at("cuts"),std::numeric_limits<unsigned>::max());integer(s.at("first_id"),UINT64_MAX);
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
} // namespace dome::detail
