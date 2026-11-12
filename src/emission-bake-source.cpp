// SPDX-License-Identifier: GPL-3.0-or-later
#include "emission-bake-source.hpp"
#include "paint-overlay-sample.hpp"
#include "source-bake-normal.hpp"
#include "normal-frame-runtime.hpp"
#include "normal-bake-encoding.hpp"
#include <numeric>
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
      // Resolved PBR roles name current carriers; typed slots name original
      // logical roles, which may independently retain artist-authored UV1.
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
SourceBakeNormalResolver source_bake_normals(const SculptObject &object,CanvasTransform relative,
    const SourceBakeNormalOptions &options){
  const auto &binding=object.material_bindings;
  if(!binding||!binding->normal_frames)return {};
  auto stop=[&]{if(options.cancelled&&options.cancelled())throw std::runtime_error("Source normal capture cancelled");};stop();
  if(!object.mesh_active||!object.painted||!object.painted->valid_source_sampling_attachment()||
     binding->canonical_attributes||binding->canonical_uv||binding->canonical_paint_sampling||binding->native_paint_sampling||binding->native_paint_frames||
     binding->normal_preparation||object.painted->normal_projection()||object.painted->color_projection()||
     (object.painted->baked()&&(!object.painted->baked()->maps.normal.rgba.empty()||object.painted->baked()->normal_field||object.painted->baked()->normal_authority)))
    throw std::invalid_argument("Artist source normal capture requires its unchanged original frame and separate current-normal adapter");
  const auto &mesh=object.painted->mesh();const auto &set=*binding->normal_frames;
  const uint64_t corners=mesh.indices.size();
  // Charge both posed role arrays, immutable evaluated corner columns and
  // layout scratch before allocating, including retained morph/frame payload.
  const uint64_t work=set.storage_bytes()+corners*(3*sizeof(uint32_t)+2*(sizeof(Vec3)+sizeof(std::array<float,4>)+sizeof(NormalBakeVertex)));
  if(!options.max_working_bytes||options.max_working_bytes>512*1024*1024||work>options.max_working_bytes)
    throw std::length_error("Artist source normal capture exceeds prospective working budget");
  normal_frame_current_authority(*binding,mesh,object.deformation.get(),options.cancelled);
  if(!set.pbr||(binding->has_mtoon&&!set.mtoon))throw std::invalid_argument("Artist source lost its explicitly declared PBR/MToon frame roles");
  Source coordinates;coordinates.mesh=object.painted->mesh_owner();coordinates.maps=object.painted->source_maps();coordinates.packet=PaintOverlayFactory::capture(*object.painted,options.cancelled);coordinates.cancelled=options.cancelled;
  if(!coordinates.maps)coordinates.maps=std::make_shared<const PaintSourceMaps>();
  coordinates.maps->validate_surface(mesh);
  const auto typed=binding->has_mtoon?std::optional<MToonMaterial>(binding->material):std::optional<MToonMaterial>{};
  if(typed&&(typed->parameters.scroll!=std::array<float,2>{}||typed->parameters.rotation_speed!=0))
    throw std::invalid_argument("Artist normal source requires an explicit frozen UV animation time");
  PaintSourceTexture typed_role;if(typed&&typed->textures[size_t(MToonSlot::Normal)].index>=0){typed_role=binding->roles[size_t(MToonSlot::Normal)];if(!typed_role.image)throw std::invalid_argument("Artist source declared typed normal has no actual image owner");}
  if((coordinates.maps->normal.image&&set.pbr->tangents.empty())||(typed&&typed->parameters.normal_enabled&&typed_role.image&&set.mtoon->tangents.empty()))
    throw std::invalid_argument("Artist normal texture requires its genuine declared tangent attribute role");
  NormalFrameLayout layout;layout.vertices=mesh.indices;layout.corners.resize(size_t(corners));std::iota(layout.corners.begin(),layout.corners.end(),0u);
  const auto posed=normal_frame_preview(set,layout,object.deformation.get(),options.cancelled,typed?3u:1u);
  const std::array<bool,2> has_tangent{!set.pbr->tangents.empty(),typed&&!set.mtoon->tangents.empty()};
  auto vertices=std::make_shared<std::array<std::vector<NormalBakeVertex>,2>>();
  for(size_t role=0;role<(typed?2u:1u);++role){
    auto &out=vertices->at(role);out.reserve(size_t(corners));
    for(size_t c=0;c<corners;++c){if((c&1023)==0)stop();auto N=posed.normals[role].at(c);Vec3 T{1,0,0};float w=1;
      if(!posed.tangents[role].empty()){const auto t=posed.tangents[role][c];T={t[0],t[1],t[2]};w=t[3];}
      auto n=ob::rotate(relative.orientation,{N.x,N.y,N.z}),t=ob::rotate(relative.orientation,{T.x,T.y,T.z});
      if(has_tangent[role])out.push_back(normal_bake_vertex({n.x,n.y,n.z},{t.x,t.y,t.z},w,NormalVertexBitangent::RawCross));
      else out.push_back({normalized(Vec3{n.x,n.y,n.z}),{},{},1});
    }
  }
  // W is a per-triangle discrete attribute, never a barycentric float sign.
  for(size_t role=0;role<(typed?2u:1u);++role)for(size_t c=0;c<corners;c+=3){if((c&1023)==0)stop();if(vertices->at(role)[c].w!=vertices->at(role)[c+1].w||vertices->at(role)[c].w!=vertices->at(role)[c+2].w)throw std::invalid_argument("Artist source tangent handedness changes inside a triangle");}
  stop();std::shared_ptr<const std::array<std::vector<NormalBakeVertex>,2>> immutable=std::move(vertices);
  return [immutable,coordinates=std::move(coordinates),typed,typed_role,has_tangent](uint32_t triangle,std::array<float,3> bary){
    coordinates.stop();const std::array<double,3> weights{bary[0],bary[1],bary[2]};std::array<Vec3,2> result;
    for(size_t role=0;role<(typed?2u:1u);++role){
      std::array<NormalBakeVertex,3> corners;for(size_t k=0;k<3;++k)corners[k]=immutable->at(role).at(size_t(triangle)*3+k);
      if(!has_tangent[role]){Vec3 N;for(size_t k=0;k<3;++k)N+=corners[k].n*bary[k];result[role]=normalized(N);continue;}
      const auto mode=role==0?NormalFragmentBasis::UnitColumns:typed->version()==MToonVersion::V0?NormalFragmentBasis::RawColumns:NormalFragmentBasis::UnitCross;
      auto basis=normal_bake_basis(corners,weights,mode);result[role]=normalized(basis.n);
      auto view=role==0?coordinates.maps->normal:typed_role;const bool enabled=role==0||typed->parameters.normal_enabled;
      if(enabled&&view.image){
        auto uv=coordinates.coordinates(triangle,bary,role==0?view.texcoord:mtoon_coordinate(typed->textures[size_t(MToonSlot::Normal)]),role!=0);
        if(role)uv=mtoon_uv(*typed,MToonSlot::Normal,uv,0);
        view.texcoord=0;
        const auto value=sample_source_texture(view,uv[0],uv[1],false);const float scale=role==0?coordinates.maps->normal_scale:typed->parameters.normal_scale;
        const auto ts=mtoon_normal_map({value[0],value[1],value[2]},scale);
        result[role]=normalized(basis.t*ts.x+basis.b*ts.y+basis.n*ts.z);
      }
    }
    if(!typed)result[1]=result[0];
    return result;
  };
}
std::shared_ptr<const TextureMaps> raw_bake_source(const SculptObject &object,
    const std::function<bool()> &cancelled){
  if(!object.mesh_active||!object.painted||!object.painted->source_maps()||
     (!object.painted->source_maps()->uses_alternate_uv()&&!object.painted->source_sampling_prepared()&&
      !(object.material_bindings&&object.material_bindings->normal_frames)))return {};
  const auto &paint=*object.painted;const auto &binding=object.material_bindings;
  if(cancelled&&cancelled())throw std::runtime_error("Raw source Bake cancelled");
  const auto framed=source_bake_normals(object,CanvasTransform{},SourceBakeNormalOptions{256*1024*1024,cancelled});
  if(paint.color_projection()||paint.normal_projection()||
     (binding&&(binding->canonical_attributes||binding->canonical_uv||binding->canonical_paint_sampling||
                binding->native_paint_sampling||binding->native_paint_frames||(binding->normal_frames&&!framed))))
    throw std::invalid_argument("Raw source Bake requires its explicit prepared attribute/frame adapter");
  if(!paint.valid_source_sampling_attachment()||
     (paint.paint_sampling_domain()&&!paint.valid_paint_sampling_attachment()))
    throw std::invalid_argument("Raw source Bake lost its actual immutable coordinate attachment");
  if(const auto &baked=paint.baked();baked&&(baked->normal_field||baked->normal_authority||
      baked->normal_interpretation!=BakedNormalInterpretation::CanonicalMikk))
    throw std::invalid_argument("Raw source Bake requires canonical legacy normal data");
  if(object.deformation&&!framed){
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
  if(!framed&&typed&&typed->textures[size_t(MToonSlot::Normal)].index>=0)
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
