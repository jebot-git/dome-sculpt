// SPDX-License-Identifier: GPL-3.0-or-later
#include "emission-bake-source.hpp"
#include "paint-overlay-sample.hpp"
#include "dome/material-bindings.hpp"
#include "dome/material-visibility.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/deformation.hpp"
#include "dome/source-maps.hpp"
#include <algorithm>
#include <limits>
#include <optional>

namespace dome::detail {namespace {
struct Source {
  std::shared_ptr<const Mesh> mesh;
  std::shared_ptr<const PaintOverlayPacket> packet;
  std::shared_ptr<const PaintSourceMaps> maps;
  PaintSourceTexture role;
  std::optional<MToonMaterial> toon;
  std::array<float,3> background{};
  float legacy_strength=1;
  bool physical_background=false;
  std::function<bool()> cancelled;
  void stop()const{if(cancelled&&cancelled())throw std::runtime_error("Emission Bake cancelled");}
  std::array<float,2> coordinates(uint32_t triangle,std::array<float,3> bary,int set,bool original)const{
    if(!mesh||size_t(triangle)>=mesh->indices.size()/3)throw std::out_of_range("Emission Bake source triangle");
    const std::vector<std::array<float,2>> *values=nullptr;
    if(set==0)values=original&&packet&&packet->original_uv0()?packet->original_uv0().get():&mesh->uv;
    else if(set==1){
      if(original&&packet&&packet->original_uv1())values=packet->original_uv1().get();
      else if(maps&&maps->uv1)values=maps->uv1.get();
    }else if(maps){const auto i=maps->extra_uv_sets.find(set);if(i!=maps->extra_uv_sets.end())values=i->second.get();}
    if(!values||values->size()!=mesh->positions.size())throw std::invalid_argument("Emission Bake lacks the actual original role coordinates");
    std::array<double,2> result{};
    for(size_t k=0;k<3;++k){const auto uv=values->at(mesh->indices[size_t(triangle)*3+k]);for(size_t c=0;c<2;++c)result[c]+=double(uv[c])*bary[k];}
    return {float(result[0]),float(result[1])};
  }
  std::array<float,3> sample(uint32_t triangle,std::array<float,3> bary)const{
    stop();auto original=background;
    if(role.image&&!physical_background){
      // PBR roles already name transported current carriers. Typed slots keep
      // their original logical ordinal, including separately retained artist UV1.
      auto uv=coordinates(triangle,bary,toon?mtoon_coordinate(toon->textures[size_t(MToonSlot::Emission)]):role.texcoord,bool(toon));
      if(toon)uv=mtoon_uv(*toon,MToonSlot::Emission,uv,0);
      // coordinates() has authenticated and resolved the actual source set.
      // The ordinary image sampler accepts only a resolved coordinate pair;
      // retain its affine/filter/image, without reinterpreting UV1 as UV0.
      auto resolved=role;resolved.texcoord=0;
      const auto value=sample_source_texture(resolved,uv[0],uv[1],true);
      for(size_t k=0;k<3;++k){const double radiance=double(original[k])*value[k];if(radiance>std::numeric_limits<float>::max())throw std::overflow_error("Emission Bake source multiplication overflows FLOAT");original[k]=float(radiance);}
    }
    BakedEmissionSample replacement;
    if(packet){
      const auto uv=coordinates(triangle,bary,0,false);
      const auto values=sample_overlay_values(*packet,uv[0],uv[1],false);
      for(size_t k=0;k<3;++k){replacement.legacy[k]=values[2][k];replacement.absolute[k]=values[4][k];}
      replacement.coverage=values[3][3];
    }
    return replacement.resolve(original,legacy_strength);
  }
};
}
std::shared_ptr<const TextureMaps> raw_bake_source(const SculptObject &object,
    const std::function<bool()> &cancelled){
  if(!object.mesh_active||!object.painted||!object.painted->source_maps()||
     (!object.painted->source_maps()->uses_alternate_uv()&&!object.painted->source_sampling_prepared()))return {};
  const auto &paint=*object.painted;const auto &binding=object.material_bindings;
  if(cancelled&&cancelled())throw std::runtime_error("Raw source Bake cancelled");
  if(paint.color_projection()||paint.normal_projection()||
     (binding&&(binding->canonical_attributes||binding->canonical_uv||binding->canonical_paint_sampling||
                binding->native_paint_sampling||binding->native_paint_frames||binding->normal_frames)))
    throw std::invalid_argument("Raw source Bake requires its explicit prepared attribute/frame adapter");
  if(!paint.valid_source_sampling_attachment()||
     (paint.paint_sampling_domain()&&!paint.valid_paint_sampling_attachment()))
    throw std::invalid_argument("Raw source Bake lost its actual immutable coordinate attachment");
  if(const auto &baked=paint.baked();baked&&(baked->normal_field||baked->normal_authority||
      baked->normal_interpretation!=BakedNormalInterpretation::CanonicalMikk))
    throw std::invalid_argument("Raw source Bake requires canonical legacy normal data");
  if(object.deformation){
    const auto posed=object.deformation->preview(paint.mesh());const auto &rest=paint.mesh();
    for(size_t i=0;i<rest.positions.size();++i){
      if((i&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Raw source Bake cancelled");
      if(length(posed.positions[i]-rest.positions[i])>1e-6f||length(posed.normals[i]-rest.normals[i])>1e-5f)
        throw std::invalid_argument("Raw source Bake needs explicit posed source-frame transport");
    }
  }
  // Typed normal roles have an independent interpretation. Never silently use
  // the generic PBR role as their source-frame authority.
  const auto typed=binding&&binding->has_mtoon?std::optional<MToonMaterial>(binding->material):
      (object.appearance?retained_mtoon(object.appearance.get()):std::optional<MToonMaterial>{});
  if(typed&&typed->textures[size_t(MToonSlot::Normal)].index>=0)
    throw std::invalid_argument("Raw typed normal Bake requires its explicit source-frame adapter");
  auto maps=std::make_shared<TextureMaps>();maps->source_maps=paint.source_maps();maps->source_only=true;
  maps->source_maps->validate_surface(paint.mesh());
  maps->overlay=PaintOverlayFactory::capture(paint,cancelled);maps->baked_base=paint.baked();
  return maps;
}
std::shared_ptr<const TextureMaps> source_baked_preview(const SculptObject &object,
    const std::function<bool()> &cancelled){
  if(!object.mesh_active||!object.painted||!object.painted->source_sampling_prepared()||!object.painted->baked())return {};
  // Same authenticated original/current role and rest-frame contract as Bake.
  // This deliberately retains typed/prepared/changed-pose refusals.
  (void)raw_bake_source(object,cancelled);
  return PaintOverlayFactory::capture_source_baked(*object.painted,cancelled);
}
std::function<std::array<float,3>(uint32_t,std::array<float,3>)>
emission_bake_source(const SculptObject &object,const std::function<bool()> &cancelled){
  if(cancelled&&cancelled())throw std::runtime_error("Emission Bake cancelled");
  const auto &binding=object.material_bindings;
  std::optional<MToonMaterial> typed;
  if(binding&&binding->has_mtoon)typed=binding->material;
  else if(object.appearance)typed=retained_mtoon(object.appearance.get());
  const bool typed_hdr=typed&&(typed->parameters.emission_strength>1||
      typed->parameters.emission.x>1||typed->parameters.emission.y>1||typed->parameters.emission.z>1);
  const bool hdr=std::any_of(object.material.emissive.begin(),object.material.emissive.end(),[](float x){return x>1;});
  if(!hdr&&!typed_hdr&&!(object.painted&&object.painted->has_physical_emission())&&!(binding&&binding->physical_emission))return {};
  object.material.validate();if(binding)binding->validate();
  auto source=std::make_shared<Source>();source->background=object.material.emissive;source->cancelled=cancelled;
  if(object.mesh_active&&object.painted){
    source->mesh=object.painted->mesh_owner();source->maps=object.painted->source_maps();
    source->packet=PaintOverlayFactory::capture(*object.painted,cancelled);
    if(object.painted->paint_sampling_domain()&&!object.painted->valid_paint_sampling_attachment())
      throw std::invalid_argument("Emission Bake lost its exact current paint geometry attachment");
    if(source->maps){source->maps->validate_surface(*source->mesh);source->role=source->maps->emission;}
  }
  source->toon=std::move(typed);
  if(source->toon){
    const auto &toon=*source->toon;toon.validate();const auto &p=toon.parameters;
    if(p.scroll!=std::array<float,2>{}||p.rotation_speed!=0)
      throw std::invalid_argument("Emission Bake requires an explicit frozen source UV-animation time");
    source->legacy_strength=p.emission_strength;
    const auto reference=binding?binding->native_reference:native_source(*object.appearance);
    const std::array<float,3> typed{p.emission.x,p.emission.y,p.emission.z};
    for(size_t k=0;k<3;++k){
      const double v=double(mtoon_factor(typed[k],reference.emission[k],object.material.emissive[k]))*p.emission_strength;
      if(!std::isfinite(v)||v>std::numeric_limits<float>::max())throw std::overflow_error("Emission Bake typed background overflows FLOAT");
      source->background[k]=float(v);
    }
    const auto &slot=toon.textures[size_t(MToonSlot::Emission)];
    source->role={};
    if(slot.index>=0){
      if(binding)source->role=binding->roles[size_t(MToonSlot::Emission)];
      else{
        const auto &appearance=*object.appearance;const auto &texture=appearance.asset->textures.at(size_t(slot.index));
        if(texture.image<0||!appearance.decoded_images||size_t(texture.image)>=appearance.decoded_images->size())throw std::invalid_argument("Emission Bake lacks its actual retained typed image");
        source->role.image=appearance.decoded_images->at(size_t(texture.image));source->role.wrap_s=texture.wrap_s;source->role.wrap_t=texture.wrap_t;source->role.min_filter=texture.min_filter;source->role.mag_filter=texture.mag_filter;
      }
      if(!source->role.image)throw std::invalid_argument("Emission Bake typed image owner is absent");
    }
  }
  if(binding&&binding->physical_emission){source->background=*binding->physical_emission;source->physical_background=true;}
  source->stop();
  // Only const ownership escapes; no caller's mutable object/material is read
  // from the worker after this capture or from a rendering callback.
  std::shared_ptr<const Source> immutable=std::move(source);
  return [immutable](uint32_t triangle,std::array<float,3> bary){return immutable->sample(triangle,bary);};
}
} // namespace dome::detail
