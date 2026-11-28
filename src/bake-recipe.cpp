// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/bake-recipe.hpp"
#include "bake-recipe-remap.hpp"
#include "canonical-paint-sampling.hpp"
#include "polygon-surface-preview.hpp"
#include "dome/scene.hpp"
#include "text-utf8.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
namespace dome {
namespace {
void reference(const BakeObjectReference &r){
  if(!r.object||!r.original_object||r.original_object==unavailable_bake_object||
      r.name.size()>256||!text::valid_utf8(r.name)||r.name.find('\0')!=r.name.npos)
    throw std::invalid_argument("Invalid bake recipe object reference");
}
uint64_t number(const nlohmann::json &value){
  if(!value.is_number_unsigned()&&(!value.is_number_integer()||value.get<int64_t>()<0))
    throw std::invalid_argument("Invalid bake recipe integer");
  return value.get<uint64_t>();
}
nlohmann::json encode_reference(const BakeObjectReference &r){
  return {{"object",r.object},{"original_object",r.original_object},{"name",r.name}};
}
BakeObjectReference decode_reference(const nlohmann::json &j){
  if(!j.is_object()||j.size()!=3||!j.at("name").is_string()||j.at("name").get_ref<const std::string&>().size()>256)
    throw std::invalid_argument("Invalid bake recipe reference record");
  BakeObjectReference r{number(j.at("object")),number(j.at("original_object")),j.at("name").get<std::string>()};
  reference(r);return r;
}
}
void ObjectBakeRecipe::validate()const{
  reference(source);if(cage)reference(*cage);
  if(settings.cage_object!=(cage?cage->object:0))
    throw std::invalid_argument("Bake recipe lost its explicit cage binding");
  if(settings.corresponding_topology&&cage)
    throw std::invalid_argument("Matching source triangles requires a bake without a cage");
  if(settings.mask_layer==UINT64_MAX||
     (settings.mask_layer&&(settings.prepare_colors||settings.prepare_normals)))
    throw std::invalid_argument("Invalid Bake mask layer or incompatible whole-surface preparation");
  for(float mm:{settings.front_mm,settings.back_mm,settings.ao_distance_mm})
    if(!std::isfinite(mm)||mm<0||mm>100'000)
      throw std::invalid_argument("Invalid bake recipe distance");
  if(settings.ao_samples>256||settings.padding>64||
      !std::isfinite(settings.color_error)||settings.color_error<0||settings.color_error>1||
      !std::isfinite(settings.normal_error)||settings.normal_error<0||settings.normal_error>.25f)
    throw std::invalid_argument("Invalid bake recipe quality");
}
nlohmann::json ObjectBakeRecipe::encode()const{
  validate();const auto &s=settings;
  const int version=s.mask_layer?4:s.supersample?3:s.corresponding_topology?2:1;
  nlohmann::json out={{"version",version},{"source",encode_reference(source)},
    {"cage",cage?nlohmann::json(encode_reference(*cage)):nlohmann::json(nullptr)},
    {"settings",{{"front_mm",s.front_mm},{"back_mm",s.back_mm},{"ao_distance_mm",s.ao_distance_mm},
      {"ao_samples",s.ao_samples},{"padding",s.padding},{"allow_partial",s.allow_partial},
      {"allow_backfaces",s.allow_backfaces},{"prepare_colors",s.prepare_colors},
      {"color_error",s.color_error},{"prepare_normals",s.prepare_normals},{"normal_error",s.normal_error}}}};
  if(s.corresponding_topology)out["settings"]["corresponding_topology"]=true;
  if(s.supersample)out["settings"]["supersample"]=true;
  if(s.mask_layer)out["settings"]["mask_layer"]=s.mask_layer;
  return out;
}
std::shared_ptr<const ObjectBakeRecipe> ObjectBakeRecipe::decode(const nlohmann::json &j){
  if(!j.is_object()||j.size()!=4||!j.at("version").is_number_integer()||
      (j.at("version")!=1&&j.at("version")!=2&&j.at("version")!=3&&j.at("version")!=4))
    throw std::invalid_argument("Invalid bake recipe record");
  const auto version=j.at("version").get<int>();
  auto result=std::make_shared<ObjectBakeRecipe>();result->source=decode_reference(j.at("source"));
  if(!j.at("cage").is_null())result->cage=decode_reference(j.at("cage"));
  const auto &v=j.at("settings");auto &s=result->settings;
  const bool corresponding=version==2||(version>=3&&v.contains("corresponding_topology"));
  const bool supersampled=version==3||(version==4&&v.contains("supersample"));
  if(!v.is_object()||v.size()!=11u+unsigned(corresponding)+unsigned(supersampled)+unsigned(version==4))
    throw std::invalid_argument("Incomplete bake recipe settings");
  auto real=[&](const char *key){if(!v.at(key).is_number())throw std::invalid_argument("Invalid bake recipe scalar");return v.at(key).get<float>();};
  auto flag=[&](const char *key){if(!v.at(key).is_boolean())throw std::invalid_argument("Invalid bake recipe flag");return v.at(key).get<bool>();};
  s.front_mm=real("front_mm");s.back_mm=real("back_mm");s.ao_distance_mm=real("ao_distance_mm");
  const auto samples=number(v.at("ao_samples")),padding=number(v.at("padding"));
  if(samples>256||padding>64)throw std::invalid_argument("Invalid bake recipe integer quality");
  s.ao_samples=uint32_t(samples);s.padding=uint32_t(padding);
  s.allow_partial=flag("allow_partial");s.allow_backfaces=flag("allow_backfaces");
  s.prepare_colors=flag("prepare_colors");s.color_error=real("color_error");
  s.prepare_normals=flag("prepare_normals");s.normal_error=real("normal_error");
  if(corresponding&&!flag("corresponding_topology"))
    throw std::invalid_argument("Invalid matching topology bake recipe");
  if(supersampled&&!flag("supersample"))
    throw std::invalid_argument("Invalid four-sample bake recipe");
  s.corresponding_topology=corresponding;
  s.supersample=supersampled;
  if(version==4){
    s.mask_layer=number(v.at("mask_layer"));
    if(!s.mask_layer)throw std::invalid_argument("Version four Bake recipe needs a specific target mask layer");
  }
  s.cage_object=result->cage?result->cage->object:0;
  result->validate();return result;
}
std::shared_ptr<const ObjectBakeRecipe> capture_bake_recipe(const Scene &scene,
    uint64_t source,const ObjectBakeSettings &settings){
  auto result=std::make_shared<ObjectBakeRecipe>();
  auto capture=[&](uint64_t id){const auto &o=scene.object(id);return BakeObjectReference{id,id,o.name};};
  result->source=capture(source);if(settings.cage_object)result->cage=capture(settings.cage_object);
  result->settings=settings;result->validate();return result;
}
} // namespace dome

namespace dome::detail {
std::shared_ptr<const SourceMaterialBindings> BakeRecipeRemap::binding(
    const std::shared_ptr<const SourceMaterialBindings> &original, const OriginRemap &origin) {
  if (!original || (!original->polygon_surface_transfer && !original->canonical_paint_sampling))
    return original;
  if (const auto found = bindings_.find(original.get()); found != bindings_.end())
    return found->second;

  auto polygon = original->polygon_surface_transfer;
  if (polygon) {
    const auto before = origin(polygon->before());
    if (before != polygon->before()) {
      auto found = polygon_fields_.find(polygon.get());
      if (found == polygon_fields_.end())
        found = polygon_fields_.emplace(polygon.get(), polygon->rebase_before(before)).first;
      polygon = found->second;
    }
  }
  auto canonical = original->canonical_paint_sampling;
  if (canonical) {
    const auto before = origin(canonical->before());
    if (before != canonical->before()) {
      auto found = canonical_fields_.find(canonical.get());
      if (found == canonical_fields_.end())
        found = canonical_fields_.emplace(canonical.get(), canonical->rebase_before(before)).first;
      canonical = found->second;
    }
  }
  if (polygon == original->polygon_surface_transfer && canonical == original->canonical_paint_sampling)
    return original;

  // Geometry and paint retain their authenticated owners while the before
  // references move into the destination object's namespace.
  auto result = std::make_shared<SourceMaterialBindings>(*original);
  result->polygon_surface_transfer = std::move(polygon);
  result->canonical_paint_sampling = std::move(canonical);
  bindings_.emplace(original.get(), result);
  return result;
}

std::shared_ptr<const PolygonSurfacePreview> BakeRecipeRemap::preview(
    const std::shared_ptr<const PolygonSurfacePreview> &original,
    const std::shared_ptr<const SourceMaterialBindings> &binding) {
  if (!original || original->binding_ == binding)
    return original;
  if (const auto found = previews_.find(original.get()); found != previews_.end())
    return found->second;
  auto rebased = std::shared_ptr<PolygonSurfacePreview>(new PolygonSurfacePreview(*original));
  rebased->binding_ = binding;
  rebased->owner_ = binding->polygon_surface_transfer;
  rebased->self_ = rebased;
  if (!rebased->matches(binding, original->attachment_))
    throw std::invalid_argument("Bake recipe remapping changed the polygon preview field");
  previews_.emplace(original.get(), rebased);
  return rebased;
}
} // namespace dome::detail
