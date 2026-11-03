// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/normal-sampling.hpp"
#include "dome/scene.hpp"
#include "dome/material-bindings.hpp"
#include "source-paint-pairing.hpp"
namespace dome {
namespace {
void stop(const std::function<bool()> &cancelled){
  if(cancelled&&cancelled())throw std::runtime_error("Paired normal surface archive cancelled");
}
uint64_t object_id(const nlohmann::json &j){
  if((!j.is_number_unsigned()&&(!j.is_number_integer()||j.get<int64_t>()<=0))||j.get<uint64_t>()==0)
    throw std::invalid_argument("Paired normal surface needs a positive object identity");
  return j.get<uint64_t>();
}
void require_surface(const Scene &scene,uint64_t id){
  const auto &object=scene.object(id);
  if(!object.painted)throw std::invalid_argument("Paired normal surface has no painted geometry");
  if(object.material_bindings)object.material_bindings->validate();
  detail::source_paint_pairing(object.painted.get(),object.material_bindings.get(),object.appearance.get(),true);
}
}
nlohmann::json encode_normal_surface(const Scene &scene,uint64_t object,const std::function<bool()> &cancelled){
  stop(cancelled);require_surface(scene,object);stop(cancelled);
  auto encoded=scene.encode();stop(cancelled);
  return {{"version",1},{"object",object},{"scene",std::move(encoded)}};
}
Scene decode_normal_surface(const nlohmann::json &j,const std::function<bool()> &cancelled){
  stop(cancelled);
  if(!j.is_object()||j.size()!=3||!j.at("version").is_number_integer()||j.at("version")!=1)
    throw std::invalid_argument("Unsupported paired normal surface archive");
  const auto id=object_id(j.at("object"));
  auto decoded=Scene::decode(j.at("scene"));stop(cancelled);
  require_surface(decoded,id);decoded.select(id);stop(cancelled);return decoded;
}
} // namespace dome
