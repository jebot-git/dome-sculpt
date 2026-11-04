// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/bake-recipe.hpp"
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
  return {{"version",1},{"source",encode_reference(source)},
    {"cage",cage?nlohmann::json(encode_reference(*cage)):nlohmann::json(nullptr)},
    {"settings",{{"front_mm",s.front_mm},{"back_mm",s.back_mm},{"ao_distance_mm",s.ao_distance_mm},
      {"ao_samples",s.ao_samples},{"padding",s.padding},{"allow_partial",s.allow_partial},
      {"allow_backfaces",s.allow_backfaces},{"prepare_colors",s.prepare_colors},
      {"color_error",s.color_error},{"prepare_normals",s.prepare_normals},{"normal_error",s.normal_error}}}};
}
std::shared_ptr<const ObjectBakeRecipe> ObjectBakeRecipe::decode(const nlohmann::json &j){
  if(!j.is_object()||j.size()!=4||!j.at("version").is_number_integer()||j.at("version")!=1)
    throw std::invalid_argument("Invalid bake recipe record");
  auto result=std::make_shared<ObjectBakeRecipe>();result->source=decode_reference(j.at("source"));
  if(!j.at("cage").is_null())result->cage=decode_reference(j.at("cage"));
  const auto &v=j.at("settings");auto &s=result->settings;
  if(!v.is_object()||v.size()!=11)throw std::invalid_argument("Incomplete bake recipe settings");
  auto real=[&](const char *key){if(!v.at(key).is_number())throw std::invalid_argument("Invalid bake recipe scalar");return v.at(key).get<float>();};
  auto flag=[&](const char *key){if(!v.at(key).is_boolean())throw std::invalid_argument("Invalid bake recipe flag");return v.at(key).get<bool>();};
  s.front_mm=real("front_mm");s.back_mm=real("back_mm");s.ao_distance_mm=real("ao_distance_mm");
  const auto samples=number(v.at("ao_samples")),padding=number(v.at("padding"));
  if(samples>256||padding>64)throw std::invalid_argument("Invalid bake recipe integer quality");
  s.ao_samples=uint32_t(samples);s.padding=uint32_t(padding);
  s.allow_partial=flag("allow_partial");s.allow_backfaces=flag("allow_backfaces");
  s.prepare_colors=flag("prepare_colors");s.color_error=real("color_error");
  s.prepare_normals=flag("prepare_normals");s.normal_error=real("normal_error");
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
