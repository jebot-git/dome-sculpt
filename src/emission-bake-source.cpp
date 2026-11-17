// SPDX-License-Identifier: GPL-3.0-or-later
#include "emission-bake-source.hpp"
#include "emission-authority.hpp"
#include "paint-overlay-sample.hpp"
#include "source-bake-normal.hpp"
#include "normal-frame-runtime.hpp"
#include "normal-bake-encoding.hpp"
#include "normal-projection-identity.hpp"
#include "paint-normal-access.hpp"
#include "emission-paint-access.hpp"
#include "normal-sampling-access.hpp"
#include "normal-key-derivation.hpp"
#include "canonical-normal-owner.hpp"
#include "canonical-normal-uv.hpp"
#include "baked-normal-field.hpp"
#include "source-paint-pairing.hpp"
#include "source-writable-paint.hpp"
#include "bake-source-capabilities.hpp"
#include "cut-color.hpp"
#include "native-paint-frame-source.hpp"
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
// An untextured material has no original image owner. This factory-only empty
// role bundle represents that genuine absence; it never grants UV/image data.
const std::shared_ptr<const PaintSourceMaps> &empty_source_maps(){
 static const auto empty=std::make_shared<const PaintSourceMaps>();return empty;
}
void require_original_emission_uv1(const SculptObject &object,
    const std::optional<MToonMaterial> &typed){
  if(!typed||!object.mesh_active||!object.painted)return;
  // An explicit physical background does not sample the retained emission image.
  if(object.material_bindings&&object.material_bindings->physical_emission)return;
  const auto &slot=typed->textures[size_t(MToonSlot::Emission)];
  if(slot.index<0||mtoon_coordinate(slot)!=1)return;
  const auto &paint=*object.painted;
  if(!paint.source_sampling_prepared()){
    // A normal-prepared plane authenticates normals only. Its retained input
    // still owns the original logical coordinates of other typed image roles.
    if(!paint.normal_projection()||!object.material_bindings||!object.material_bindings->normal_preparation)return;
    const auto before=object.material_bindings->normal_preparation->original_paint;
    const auto root=before&&before->source_sampling_prepared()?before->source_sampling_original():before;
    const auto maps=root?root->source_maps():nullptr;
    const bool authored=before&&before->source_sampling_prepared()&&before->source_sampling_authored_uv1();
    if(!before||(before->source_sampling_prepared()&&!before->valid_source_sampling_attachment())||(!authored&&(!maps||!maps->uv1)))
      throw std::invalid_argument("Prepared typed emission UV1 lacks genuine original source coordinates");
    return;
  }
  if(!paint.valid_source_sampling_attachment())
    throw std::invalid_argument("Typed emission lost its genuine original sampling attachment");
  const auto original=paint.source_sampling_original();
  const auto maps=original?original->source_maps():nullptr;
  if(!original||(!paint.source_sampling_authored_uv1()&&(!maps||!maps->uv1)))
    throw std::invalid_argument("Typed emission UV1 lacks genuine original source coordinates");
}
struct Source {
  std::shared_ptr<const Mesh> mesh;
  std::shared_ptr<const PaintOverlayPacket> packet;
  std::shared_ptr<const PaintSourceMaps> maps;
  PaintSourceTexture role;
  std::optional<MToonMaterial> toon;
  std::array<float,3> background{};
  float legacy_strength=1;
  bool physical_background=false,independent_source=false;
  std::function<bool()> cancelled;
  void stop()const{if(cancelled&&cancelled())throw std::runtime_error("Emission Bake cancelled");}
  std::array<float,2> coordinates(uint32_t triangle,std::array<float,3> bary,int set,bool original)const{
    if(!mesh||size_t(triangle)>=mesh->indices.size()/3)throw std::out_of_range("Emission Bake source triangle");
    const std::vector<std::array<float,2>> *values=nullptr;
    if(set==0)values=original&&packet&&packet->original_uv0()?packet->original_uv0().get():&mesh->uv;
    else if(set==1){
      if(original&&independent_source&&(!packet||!packet->original_uv1()))
        throw std::invalid_argument("Typed emission UV1 lacks genuine original source coordinates");
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
      const auto uv=source_overlay_coordinates(*packet,triangle,bary,coordinates(triangle,bary,0,false));
      const auto values=sample_overlay_values(*packet,uv[0],uv[1],false);
      for(size_t k=0;k<3;++k){replacement.legacy[k]=values[2][k];replacement.absolute[k]=values[4][k];}
      replacement.coverage=values[3][3];
    }
    return replacement.resolve(original,legacy_strength);
  }
};
}
std::shared_ptr<const PaintOverlayPacket> PaintOverlayFactory::capture_normal_source(
    const SculptObject &object,const std::function<bool()> &cancelled){
 const auto &binding=object.material_bindings;
 const auto canonical=binding?binding->canonical_attributes:nullptr;
 const auto attachment=material_paint_attachment(binding.get());
 if(!object.painted||!binding||(!canonical&&!attachment)||
    (canonical&&!canonical->valid_owner(canonical)))
   throw std::invalid_argument("Source normal packet requires its genuine current material/attribute owner");
 if(attachment&&(!object.painted->valid_paint_sampling_attachment()||
     !attachment->matches(object.painted->mesh_owner(),object.painted->paint_sampling_domain())))
   throw std::invalid_argument("Writable source packet borrowed another current paint attachment");
 source_paint_pairing(object.painted.get(),binding.get(),object.appearance.get(),true);
 if(binding->normal_frames)normal_frame_current_authority(*binding,object.painted->mesh(),object.deformation.get(),cancelled);
 auto packet=std::shared_ptr<PaintOverlayPacket>(new PaintOverlayPacket);
 packet->contribution_owner_=object.painted->normal_projection()&&attachment?capture_prepared_source(object,cancelled):capture(*object.painted,cancelled);
 packet->source_maps_=object.painted->source_maps();
 packet->normal_source_binding_=binding;packet->normal_source_deformation_=object.deformation;
 packet->source_paint_attachment_=attachment;
 packet->storage_bytes_=sizeof(PaintOverlayPacket);
 return packet;
}
bool bake_source_supported(const SculptObject &object) noexcept {
 const auto &binding=object.material_bindings;const auto &paint=object.painted;
 if(binding&&binding->native_paint_sampling){
  const auto &native=binding->native_paint_sampling;
  return object.mesh_active&&paint&&!object.appearance&&!object.polygon&&
    native->valid_owner(native)&&native->matches_current(*paint)&&!binding->has_mtoon&&
    !binding->normal_preparation&&!binding->color_preparation&&!binding->canonical_attributes&&
    (!binding->native_paint_frames||binding->native_paint_frames->valid_owner(binding->native_paint_frames));
 }
 if(object.polygon&&object.polygon->active&&object.polygon->colors&&binding&&paint&&
    paint->color_projection()&&paint->normal_projection()&&binding->color_preparation&&binding->normal_preparation){
  const auto &color=*binding->color_preparation;const auto &normal=*binding->normal_preparation;
  const auto &descriptor=color.current_descriptor();
  return object.mesh_active&&!object.appearance&&!binding->has_mtoon&&descriptor&&
    descriptor->field==object.polygon->colors&&color.paint_sampling&&color.paint_sampling->matches_current(*paint)&&
    color.projection&&paint->color_projection()->vertex_rgba==color.projection->vertex_rgba&&
    normal.projection&&normal_projection_identity(*paint->normal_projection(),*normal.projection);
 }
 return cut_color_supported(object);
}
bool SourceBakeNormalResolver::requires_source_pairing()const noexcept {
 return bool(projection_)||bool(canonical_)||(packet_&&bool(packet_->normal_source_binding()));
}
bool SourceBakeNormalResolver::matches_prepared_source(const TextureMaps &maps)const noexcept {
 auto same=[](const auto &a,const auto &b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);};
 if(!sample_||!packet_)return false;
 if(packet_->normal_source_binding()&&(!maps.overlay||
    !same(maps.overlay->normal_source_binding(),packet_->normal_source_binding())||
    !same(maps.overlay->normal_source_deformation(),packet_->normal_source_deformation())||
    !same(maps.overlay->source_paint_attachment(),packet_->source_paint_attachment())))return false;
 if(projection_){if(!same(maps.normal_projection,projection_))return false;}
 else if(canonical_||packet_->normal_source_binding()){
  if(maps.normal_projection||!maps.overlay||
     !same(maps.overlay->normal_source_binding(),packet_->normal_source_binding())||
     !same(maps.overlay->normal_source_deformation(),packet_->normal_source_deformation()))return false;
  const auto event=maps.source_only&&maps.overlay?maps.overlay->baked():maps.baked_base;
  if(!same(event,packet_->baked())||!same(event?event->normal_field:nullptr,field_)||
     (field_&&maps.source_only&&!same(maps.baked_base,event)))return false;
 }else return false;
 if(!maps.source_only)return same(maps.baked_base,packet_->baked());
 const auto &material=packet_->source_maps()?packet_->source_maps():empty_source_maps();
 return maps.overlay&&same(maps.source_maps,material)&&
   same(maps.overlay->source_maps(),packet_->source_maps())&&
   same(maps.overlay->baked(),packet_->baked())&&
   same(maps.overlay->padding(),packet_->padding())&&
   maps.overlay->resolution()==packet_->resolution();
}
SourceBakeNormalResolver source_bake_normals(const SculptObject &object,CanvasTransform relative,
    const SourceBakeNormalOptions &options){
  const auto &binding=object.material_bindings;
  const auto native=binding?binding->native_paint_sampling:nullptr;
  if(native&&!binding->normal_frames){
    auto stop=[&]{if(options.cancelled&&options.cancelled())throw std::runtime_error("Native writable source capture cancelled");};stop();
    if(!object.mesh_active||!object.painted||object.appearance||!native->valid_owner(native)||
       !native->matches_current(*object.painted)||binding->native_paint_frames||binding->key_derivation||
       binding->has_mtoon||binding->normal_preparation||binding->canonical_attributes)
      throw std::invalid_argument("Native writable source lost its actual current paint/frame authority");
    uint64_t bytes=object.painted->storage_bytes()+uint64_t(object.painted->mesh().indices.size())*sizeof(Vec3)*2;
    auto node=native;
    for(size_t depth=0;node;++depth){stop();if(depth==32)throw std::length_error("Native writable source history exceeds32 owners");
      if(!node->valid_owner(node))throw std::invalid_argument("Native source ancestor lost genuine owner");
      bytes+=node->before_paint()->storage_bytes()+node->domain()->retained_bytes();
      const auto &before=node->before_material();node=before?before->native_paint_sampling:nullptr;
    }
    if(!options.max_working_bytes||options.max_working_bytes>512*1024*1024||bytes>options.max_working_bytes)
      throw std::length_error("Native source capture exceeds prospective working budget");
    PaintSamplingLimits limits;limits.max_working_bytes=options.max_working_bytes;limits.cancelled=options.cancelled;
    native->validate_current(*object.painted,limits);binding->validate();
    const auto &mesh=object.painted->mesh();
    auto evaluated=object.deformation?object.deformation->preview(mesh):mesh;
    auto normals=std::make_shared<std::vector<Vec3>>();normals->reserve(mesh.indices.size());
    for(size_t c=0;c<mesh.indices.size();++c){if((c&1023)==0)stop();const auto n=evaluated.normals.at(mesh.indices[c]);
      const auto rotated=ob::rotate(relative.orientation,{n.x,n.y,n.z});normals->push_back(normalized(Vec3{rotated.x,rotated.y,rotated.z}));}
    auto packet=PaintOverlayFactory::capture_normal_source(object,options.cancelled);
    std::shared_ptr<const std::vector<Vec3>> immutable=std::move(normals);stop();
    auto sample=[immutable,cancelled=options.cancelled](uint32_t triangle,std::array<float,3> bary){
      if(cancelled&&cancelled())throw std::runtime_error("Native source normal sampling cancelled");
      Vec3 n{};for(size_t k=0;k<3;++k)n+=immutable->at(size_t(triangle)*3+k)*bary[k];n=normalized(n);return std::array<Vec3,2>{n,n};};
    return SourceBakeNormalResolver(std::move(sample),nullptr,std::move(packet));
  }
  if(!binding||!binding->normal_frames){
    if((binding&&binding->canonical_attributes)||(object.painted&&object.painted->baked()&&object.painted->baked()->normal_field))
      throw std::invalid_argument("Canonical normal source lost its genuine attribute frame owner");
    return {};
  }
  const auto canonical=binding->canonical_attributes;
  const auto field=object.painted&&object.painted->baked()?object.painted->baked()->normal_field:nullptr;
  const bool field_prepared=bool(canonical&&field);
  const auto normal_uv=canonical?binding->canonical_uv:nullptr;
  auto stop=[&]{if(options.cancelled&&options.cancelled())throw std::runtime_error("Source normal capture cancelled");};stop();
  const bool prepared=object.painted&&object.painted->normal_projection()&&binding->normal_preparation;
  const auto preparation=prepared?binding->normal_preparation:nullptr;
  const bool dual_planes=field_prepared;
  const auto retained=prepared&&object.painted->normal_projection()->version==2
    ?object.painted->normal_projection()->sampling_domain:nullptr;
  const auto writable=material_paint_attachment(binding.get());
  if(object.painted&&object.painted->paint_sampling_domain()&&(!writable||
      !writable->matches(object.painted->mesh_owner(),object.painted->paint_sampling_domain())))
    throw std::invalid_argument("Prepared source writable paint lacks its actual material/geometry attachment");
  if(prepared&&preparation->canonical_conversion)
    throw std::invalid_argument("Prepared source canonical conversion requires its separate intrinsic-normal adapter");
  if(prepared&&binding->sampling_reparameterized&&!preparation->uv_reprojection&&!preparation->sampling_domain)
    throw std::invalid_argument("Prepared source UV reparameterization lost its owning normal/material recipe");
  if(!object.mesh_active||!object.painted||!object.painted->valid_source_sampling_attachment()||
     (binding->canonical_uv&&!canonical)||
     (binding->canonical_paint_sampling&&!writable)||(binding->native_paint_sampling&&!writable)||
     (binding->native_paint_frames&&!native)||
     bool(binding->normal_preparation)!=bool(object.painted->normal_projection())||(!canonical&&!writable&&object.painted->color_projection())||
     (object.painted->baked()&&((!prepared&&!field_prepared&&!object.painted->baked()->maps.normal.rgba.empty())||(field&&!canonical)||(!prepared&&object.painted->baked()->normal_authority))))
    throw std::invalid_argument("Artist source normal capture requires its unchanged original frame and separate current-normal adapter");
  // Cheap owning identity checks precede every deep validation/replay. Equal
  // data or an unrelated conversion must not spend the caller's replay quota.
  if(canonical){
    if(!canonical->valid_owner(canonical)||binding->has_mtoon||object.appearance||binding->normal_preparation)
      throw std::invalid_argument("Canonical source requires its exact native conversion, not an invented typed material");
    if(field&&field->origin()!=canonical)
      throw std::invalid_argument("Canonical source field borrowed a foreign conversion");
    if(normal_uv&&(!normal_uv->valid_owner(normal_uv)||normal_uv->origin()!=canonical))
      throw std::invalid_argument("Canonical source normal coordinates borrowed a foreign conversion");
  }
  const auto &mesh=object.painted->mesh();const auto &set=*binding->normal_frames;
  const uint64_t corners=mesh.indices.size();
  // Charge both posed role arrays, immutable evaluated corner columns and
  // layout scratch before allocating, including retained morph/frame payload.
  const uint64_t preview_bytes=(prepared||field_prepared)?uint64_t(object.painted->resolution())*object.painted->resolution()*4*12:0;
  const uint64_t capture_bytes=preview_bytes+set.storage_bytes()+corners*(3*sizeof(uint32_t)+2*(sizeof(Vec3)+sizeof(std::array<float,4>)+sizeof(NormalBakeVertex)))+(retained?retained->storage_bytes():0)+(field?field->retained_bytes():0)+(normal_uv?normal_uv->storage_bytes():0);
  if(!options.max_working_bytes||options.max_working_bytes>512*1024*1024||capture_bytes>options.max_working_bytes)
    throw std::length_error("Artist source normal capture exceeds prospective working budget");
  // Replay scratch dies before posed columns/packets are captured. Respect the
  // owning recipe's declared replay cap as a separate, sequential stage rather
  // than let it spend an unbounded amount below a smaller caller quota.
  uint64_t replay_bytes=0;
  // Deep validators recurse into owning before-materials BEFORE allocating
  // their target/reprojection/transfer scratch. Bound the whole actual DAG
  // first, including key anchors: a smaller latest receipt cannot authorize a
  // larger historical replay below the caller's working limit. These are the
  // same 64-depth/256-binding limits as native material validation/archive.
  std::array<const SourceMaterialBindings *,64> active{};
  std::array<const SourceMaterialBindings *,256> complete{};size_t completed=0;
  auto visit=[&](auto &&self,const SourceMaterialBindings *node,size_t depth)->void{
    if(!node)return;stop();
    if(std::find(active.begin(),active.begin()+depth,node)!=active.begin()+depth)
      throw std::invalid_argument("Source normal replay authority is cyclic");
    if(std::find(complete.begin(),complete.begin()+completed,node)!=complete.begin()+completed)return;
    if(depth==active.size()||completed==complete.size())
      throw std::length_error("Source normal replay authority exceeds native graph limits");
    active[depth]=node;
    if(const auto &origin=node->canonical_attributes){
      if(!origin->valid_owner(origin))throw std::invalid_argument("Source normal ancestor lost its genuine canonical owner");
      replay_bytes=std::max<uint64_t>(replay_bytes,origin->maximum_working_bytes());
      self(self,origin->before_material().get(),depth+1);
    }
    if(const auto &paint=node->native_paint_sampling){
      if(!paint->valid_owner(paint))throw std::invalid_argument("Source normal ancestor lost genuine native paint owner");
      replay_bytes=std::max<uint64_t>(replay_bytes,paint->before_paint()->storage_bytes()+paint->domain()->retained_bytes());
      self(self,paint->before_material().get(),depth+1);
    }
    if(const auto &frames=node->native_paint_frames){
      if(!frames->valid_owner(frames))throw std::invalid_argument("Source normal ancestor lost native frame owner");
      replay_bytes=std::max<uint64_t>(replay_bytes,128*1024*1024);
      self(self,frames->before_material().get(),depth+1);
    }
    if(const auto &paint=node->canonical_paint_sampling){
      if(!paint->valid_owner(paint))throw std::invalid_argument("Source normal ancestor lost canonical paint owner");
      replay_bytes=std::max<uint64_t>(replay_bytes,paint->attachment()->domain()->retained_bytes());
      self(self,paint->before()->material_bindings.get(),depth+1);
    }
    if(const auto &normal=node->normal_preparation){
      if(const auto &origin=normal->canonical_conversion){
        if(!origin->valid_owner(origin))throw std::invalid_argument("Source normal ancestor lost its genuine canonical conversion");
        replay_bytes=std::max<uint64_t>(replay_bytes,origin->maximum_working_bytes());
        self(self,origin->before_material().get(),depth+1);
      }
      if(const auto &uv=normal->uv_reprojection){
        replay_bytes=std::max(replay_bytes,uv->receipt.max_working_bytes);
        self(self,uv->before_material.get(),depth+1);
      }
      if(const auto &uv=normal->sampling_domain){
        replay_bytes=std::max(replay_bytes,uv->receipt.max_working_bytes);
        self(self,uv->before_material.get(),depth+1);
      }
      if(normal->key_derivation)self(self,normal->key_derivation->before_material().get(),depth+1);
      self(self,normal->original_material.get(),depth+1);
    }
    if(node->key_derivation)self(self,node->key_derivation->before_material().get(),depth+1);
    active[depth]=nullptr;
    if(completed==complete.size())throw std::length_error("Source normal replay authority exceeds native graph limits");
    complete[completed++]=node;
  };
  if(prepared||canonical||native)visit(visit,binding.get(),0);
  const uint64_t work=std::max(capture_bytes,replay_bytes);
  if(!options.max_working_bytes||options.max_working_bytes>512*1024*1024||work>options.max_working_bytes)
    throw std::length_error("Artist source normal capture exceeds prospective working budget");
  if(writable){
    PaintSamplingLimits limits;limits.max_working_bytes=options.max_working_bytes;limits.cancelled=options.cancelled;
    binding->validate();source_paint_pairing(object.painted.get(),binding.get(),object.appearance.get(),true);
    writable->domain()->validate_current(mesh,limits);
    if(native)native->validate_current(*object.painted,limits);
    if(binding->native_paint_frames)binding->native_paint_frames->validate_binding(*binding,true,options.cancelled);
  }
  if(canonical){
    canonical->validate();source_paint_pairing(object.painted.get(),binding.get(),nullptr,true);
    if(normal_uv)normal_uv->validate_current(mesh,options.cancelled);
    else{
      const auto &root=canonical->before_paint()->mesh();
      if(root.indices.size()!=mesh.indices.size())throw std::invalid_argument("Canonical source lost ordered original normal coordinates");
      for(size_t c=0;c<mesh.indices.size();++c){if((c&1023)==0)stop();
        if(mesh.uv.at(mesh.indices[c])!=root.uv.at(root.indices[c]))
          throw std::invalid_argument("Canonical source normal UV change needs its actual correspondence owner");
      }
    }
    if(field){field->validate();if(field->origin()!=canonical)throw std::invalid_argument("Canonical source field borrowed a foreign conversion");}
  }
  normal_frame_current_authority(*binding,mesh,object.deformation.get(),options.cancelled);
  if(!set.pbr||((binding->has_mtoon||field_prepared)&&set.pbr->tangents.empty())||(binding->has_mtoon&&!set.mtoon))throw std::invalid_argument("Artist source lost its explicitly declared PBR/MToon frame roles");
  std::shared_ptr<const PaintNormalProjection> current_normals;
  if(prepared){
    if(preparation->uv_reprojection)preparation->uv_reprojection->validate(true,options.cancelled);
    if(preparation->sampling_domain){
      preparation->sampling_domain->validate(true,options.cancelled);
      preparation->sampling_domain->validate_paint(*object.painted,*binding,true,options.cancelled);
    }
    preparation->validate_current(mesh,object.appearance.get(),options.cancelled);
    if(retained&&(retained->indices!=mesh.indices||retained->coordinates.size()!=mesh.positions.size()))
      throw std::invalid_argument("Prepared source retained normal coordinates lost exact current ordered corners");
    if(!normal_projection_identity(*object.painted->normal_projection(),*binding->normal_preparation->projection))
      throw std::invalid_argument("Prepared source lost its actual intrinsic normal authority");
    // The owning accessor resolves actual intrinsic/detail normal events
    // without asking unrelated material images to composite through UV0.
    current_normals=SourceNormalPreparationAccess::resolved(*object.painted,options.cancelled);
    current_normals->validate(object.painted->resolution());
  }
  Source coordinates;coordinates.mesh=object.painted->mesh_owner();coordinates.maps=object.painted->source_maps();coordinates.packet=canonical||writable?PaintOverlayFactory::capture_normal_source(object,options.cancelled):PaintOverlayFactory::capture(*object.painted,options.cancelled);coordinates.cancelled=options.cancelled;
  if(!coordinates.maps)coordinates.maps=empty_source_maps();
  coordinates.maps->validate_surface(mesh);
  const auto typed=binding->has_mtoon?std::optional<MToonMaterial>(binding->material):std::optional<MToonMaterial>{};
  if(typed&&(typed->parameters.scroll!=std::array<float,2>{}||typed->parameters.rotation_speed!=0))
    throw std::invalid_argument("Artist normal source requires an explicit frozen UV animation time");
  PaintSourceTexture typed_role;if(!prepared&&typed&&typed->textures[size_t(MToonSlot::Normal)].index>=0){typed_role=binding->roles[size_t(MToonSlot::Normal)];if(!typed_role.image)throw std::invalid_argument("Artist source declared typed normal has no actual image owner");}
  if((coordinates.maps->normal.image&&set.pbr->tangents.empty())||(typed&&typed->parameters.normal_enabled&&typed_role.image&&set.mtoon->tangents.empty()))
    throw std::invalid_argument("Artist normal texture requires its genuine declared tangent attribute role");
  NormalFrameLayout layout;layout.vertices=mesh.indices;layout.corners.resize(size_t(corners));std::iota(layout.corners.begin(),layout.corners.end(),0u);
  const auto posed=normal_frame_preview(set,layout,object.deformation.get(),options.cancelled,typed?3u:1u);
  const std::array<bool,2> has_tangent{!set.pbr->tangents.empty(),dual_planes?!set.pbr->tangents.empty():typed&&!set.mtoon->tangents.empty()};
  const size_t roles=typed||dual_planes?2u:1u;
  auto vertices=std::make_shared<std::array<std::vector<NormalBakeVertex>,2>>();
  for(size_t role=0;role<roles;++role){
    const size_t frame_role=dual_planes&&!typed?0:role;auto &out=vertices->at(role);out.reserve(size_t(corners));
    for(size_t c=0;c<corners;++c){if((c&1023)==0)stop();auto N=posed.normals[frame_role].at(c);Vec3 T{1,0,0};float w=1;
      if(!posed.tangents[frame_role].empty()){const auto t=posed.tangents[frame_role][c];T={t[0],t[1],t[2]};w=t[3];}
      auto n=ob::rotate(relative.orientation,{N.x,N.y,N.z}),t=ob::rotate(relative.orientation,{T.x,T.y,T.z});
      if(has_tangent[role])out.push_back(normal_bake_vertex({n.x,n.y,n.z},{t.x,t.y,t.z},w,(prepared||field_prepared)?NormalVertexBitangent::UnitCross:NormalVertexBitangent::RawCross));
      else out.push_back({normalized(Vec3{n.x,n.y,n.z}),{},{},1});
    }
  }
  // W is a per-triangle discrete attribute, never a barycentric float sign.
  for(size_t role=0;role<roles;++role)for(size_t c=0;c<corners;c+=3){if((c&1023)==0)stop();if(vertices->at(role)[c].w!=vertices->at(role)[c+1].w||vertices->at(role)[c].w!=vertices->at(role)[c+2].w)throw std::invalid_argument("Artist source tangent handedness changes inside a triangle");}
  stop();std::shared_ptr<const std::array<std::vector<NormalBakeVertex>,2>> immutable=std::move(vertices);
  const auto packet=coordinates.packet;
  auto sample=[immutable,coordinates=std::move(coordinates),typed,typed_role,has_tangent,prepared,current_normals,retained,field,field_prepared,dual_planes,normal_uv,roles](uint32_t triangle,std::array<float,3> bary){
    coordinates.stop();const std::array<double,3> weights{bary[0],bary[1],bary[2]};std::array<Vec3,2> result;
    for(size_t role=0;role<roles;++role){
      std::array<NormalBakeVertex,3> corners;for(size_t k=0;k<3;++k)corners[k]=immutable->at(role).at(size_t(triangle)*3+k);
      if(!has_tangent[role]){Vec3 N;for(size_t k=0;k<3;++k)N+=corners[k].n*bary[k];result[role]=normalized(N);continue;}
      const auto mode=role==0||dual_planes?NormalFragmentBasis::UnitColumns:typed->version()==MToonVersion::V0?NormalFragmentBasis::RawColumns:NormalFragmentBasis::UnitCross;
      auto basis=normal_bake_basis(corners,weights,mode);result[role]=normalized(basis.n);
      auto view=role==0?coordinates.maps->normal:typed_role;const bool enabled=role==0||dual_planes||typed->parameters.normal_enabled;
      if(prepared||field_prepared){
        view={};view.wrap_s=view.wrap_t=33071;view.min_filter=view.mag_filter=9729;
        view.image=field_prepared?(role==0?field->pbr():field->mtoon()):(role==0?current_normals->pbr:current_normals->mtoon);
      }
      if(enabled&&view.image){
        std::array<float,2> uv{};
        if(retained){
          const auto &values=role&&!retained->mtoon_coordinates.empty()?retained->mtoon_coordinates:retained->coordinates;
          std::array<double,2> point{};
          for(size_t k=0;k<3;++k){const auto q=values.at(retained->indices.at(size_t(triangle)*3+k));for(size_t c=0;c<2;++c)point[c]+=double(q[c])*bary[k];}
          for(size_t c=0;c<2;++c){if(!std::isfinite(point[c]))throw std::invalid_argument("Nonfinite retained source normal sample");uv[c]=float(point[c]);}
        }else if(field_prepared&&normal_uv){
          const auto &mesh=*normal_uv->mesh();std::array<double,2> point{};
          for(size_t k=0;k<3;++k)for(size_t axis=0;axis<2;++axis)point[axis]+=double(normal_uv->coordinates().at(mesh.indices.at(size_t(triangle)*3+k))[axis])*bary[k];
          for(size_t axis=0;axis<2;++axis){if(!std::isfinite(point[axis]))throw std::invalid_argument("Nonfinite canonical source normal coordinate");uv[axis]=float(point[axis]);}
        }else uv=coordinates.coordinates(triangle,bary,(prepared||field_prepared)?0:role==0?view.texcoord:mtoon_coordinate(typed->textures[size_t(MToonSlot::Normal)]),!prepared&&role!=0);
        if(role&&!prepared&&!field_prepared)uv=mtoon_uv(*typed,MToonSlot::Normal,uv,0);
        view.texcoord=0;
        const auto value=sample_source_texture(view,uv[0],uv[1],false);const float scale=dual_planes?1:role==0?(prepared?1:coordinates.maps->normal_scale):typed->parameters.normal_scale;
        const auto ts=mtoon_normal_map({value[0],value[1],value[2]},scale);
        result[role]=normalized(basis.t*ts.x+basis.b*ts.y+basis.n*ts.z);
      }
    }
    if(!typed&&!dual_planes)result[1]=result[0];
    return result;
  };
  return SourceBakeNormalResolver(std::move(sample),prepared?object.painted->normal_projection():nullptr,packet,canonical,field);
}
std::shared_ptr<const TextureMaps> raw_bake_source(const SculptObject &object,
    const std::function<bool()> &cancelled){
  if(cancelled&&cancelled())throw std::runtime_error("Raw source Bake cancelled");
  const auto &binding=object.material_bindings;
  const auto typed=binding&&binding->has_mtoon?std::optional<MToonMaterial>(binding->material):
      (object.appearance?retained_mtoon(object.appearance.get()):std::optional<MToonMaterial>{});
  require_original_emission_uv1(object,typed);
  const auto canonical=binding?binding->canonical_attributes:nullptr;
  const auto field=object.painted&&object.painted->baked()?object.painted->baked()->normal_field:nullptr;
  const auto writable=material_paint_attachment(binding.get());
  if(writable&&!canonical){
    if(!object.mesh_active||!object.painted||!binding||
       !writable->matches(object.painted->mesh_owner(),object.painted->paint_sampling_domain())||
       !source_bake_normals(object,{},SourceBakeNormalOptions{256*1024*1024,cancelled}))
      throw std::invalid_argument("Writable raw source requires its actual material/normal/frame adapter");
    if(object.painted->color_projection()){
      // Composed planes belong to the retained writable paint grid. The factory
      // packet seals that correspondence separately from original typed images
      // and the normal sampling domain. Sampling remaps each hit's real corners.
      const auto view=object.painted->has_physical_emission()||hdr_emission(object.material.emissive)
        ?EmissionExportAccess::other_channels(*object.painted,object.material,cancelled)
        :object.painted->preview(object.material,cancelled);
      auto maps=std::make_shared<TextureMaps>(*view);maps->baked_base=object.painted->baked();
      maps->normal_projection=object.painted->normal_projection();
      maps->overlay=PaintOverlayFactory::capture_normal_source(object,cancelled);
      return maps;
    }
    auto maps=std::make_shared<TextureMaps>();maps->source_only=true;
    maps->source_maps=object.painted->source_maps()?object.painted->source_maps():empty_source_maps();
    maps->source_maps->validate_surface(object.painted->mesh());
    maps->overlay=PaintOverlayFactory::capture_normal_source(object,cancelled);
    maps->normal_projection=object.painted->normal_projection();maps->baked_base=object.painted->baked();
    return maps;
  }
  if(canonical||field){
    if(!object.mesh_active||!object.painted||!canonical||
       !source_bake_normals(object,{},SourceBakeNormalOptions{256*1024*1024,cancelled}))
      throw std::invalid_argument("Canonical raw source requires its actual normal-field/frame adapter");
    if(object.painted->color_projection()){
      const auto view=object.painted->has_physical_emission()||hdr_emission(object.material.emissive)
        ?EmissionExportAccess::other_channels(*object.painted,object.material,cancelled)
        :object.painted->preview(object.material,cancelled);
      auto maps=std::make_shared<TextureMaps>(*view);
      maps->baked_base=object.painted->baked();
      maps->overlay=PaintOverlayFactory::capture_normal_source(object,cancelled);
      return maps;
    }
    auto maps=std::make_shared<TextureMaps>();maps->source_only=true;
    maps->source_maps=object.painted->source_maps()?object.painted->source_maps():empty_source_maps();
    maps->source_maps->validate_surface(object.painted->mesh());
    maps->overlay=PaintOverlayFactory::capture_normal_source(object,cancelled);
    // Exact active field event, not a copied display normal or synthetic bake.
    if(field)maps->baked_base=object.painted->baked();
    return maps;
  }
  if(object.mesh_active&&object.painted&&object.painted->normal_projection()){
    // Current prepared planes are already independently composed and retain
    // their owning coordinate recipe. Never reinterpret them as raw images.
    if(!source_bake_normals(object,{},SourceBakeNormalOptions{256*1024*1024,cancelled}))
      throw std::invalid_argument("Prepared normal source requires its actual frame adapter");
    // No original image owner means ordinary composed material channels;
    // preserve that genuine absence rather than fabricate an empty bundle.
    if(!object.painted->source_maps())return object.painted->preview(object.material,cancelled);
    auto maps=std::make_shared<TextureMaps>();
    maps->source_maps=object.painted->source_maps();
    maps->source_maps->validate_surface(object.painted->mesh());
    maps->source_only=true;maps->overlay=PaintOverlayFactory::capture(*object.painted,cancelled);
    // Material-only raw roles/paint contributions, not a legacy raw normal.
    // This marker requires the separate authenticated normal resolver below.
    maps->normal_projection=object.painted->normal_projection();
    return maps;
  }
  if(!object.mesh_active||!object.painted||!object.painted->source_maps()||
     (!object.painted->source_maps()->uses_alternate_uv()&&!object.painted->source_sampling_prepared()&&
      !(object.material_bindings&&object.material_bindings->normal_frames)))return {};
  const auto &paint=*object.painted;
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
  require_original_emission_uv1(object,typed);
  const bool typed_hdr=typed&&(typed->parameters.emission_strength>1||
      typed->parameters.emission.x>1||typed->parameters.emission.y>1||typed->parameters.emission.z>1);
  const bool hdr=std::any_of(object.material.emissive.begin(),object.material.emissive.end(),[](float x){return x>1;});
  if(!hdr&&!typed_hdr&&!(object.painted&&object.painted->has_physical_emission())&&!(binding&&binding->physical_emission))return {};
  object.material.validate();if(binding)binding->validate();
  auto source=std::make_shared<Source>();source->background=object.material.emissive;source->cancelled=cancelled;
  if(object.mesh_active&&object.painted){
    source->mesh=object.painted->mesh_owner();source->maps=object.painted->source_maps();
    source->independent_source=object.painted->source_sampling_prepared();
    source->packet=material_paint_attachment(binding.get())?PaintOverlayFactory::capture_normal_source(object,cancelled):PaintOverlayFactory::capture(*object.painted,cancelled);
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
