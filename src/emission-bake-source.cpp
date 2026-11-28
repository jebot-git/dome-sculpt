// SPDX-License-Identifier: GPL-3.0-or-later
#include "emission-bake-source.hpp"
#include "mtoon-uv.hpp"
#include "affine-surface.hpp"
#include "bake-paint-coordinates.hpp"
#include "baked-normal-field.hpp"
#include "canonical-normal-uv.hpp"
#include "canonical-paint-sampling.hpp"
#include "emission-authority.hpp"
#include "emission-paint-access.hpp"
#include "paint-overlay-sample.hpp"
#include "source-bake-normal.hpp"
#include "source-texture-grid.hpp"
#include "normal-frame-runtime.hpp"
#include "normal-bake-encoding.hpp"
#include "normal-projection-identity.hpp"
#include "normal-key-derivation.hpp"
#include "native-paint-frame-source.hpp"
#include "native-paint-bake.hpp"
#include "paint-normal-access.hpp"
#include "source-paint-pairing.hpp"
#include "texture-set-resample.hpp"
#include <numeric>
#include "dome/material-bindings.hpp"
#include "dome/material-visibility.hpp"
#include "dome/normal-sampling.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/deformation.hpp"
#include "dome/source-maps.hpp"
#include <algorithm>
#include <limits>
#include <optional>

namespace dome::detail {namespace {
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
  std::shared_ptr<const std::vector<std::array<float,2>>> authored_mtoon_uv1;
  std::shared_ptr<const PaintSamplingDomain> paint_domain;
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
      else if(original&&authored_mtoon_uv1)values=authored_mtoon_uv1.get();
      else if(maps&&maps->uv1)values=maps->uv1.get();
    }else if(maps){const auto i=maps->extra_uv_sets.find(set);if(i!=maps->extra_uv_sets.end())values=i->second.get();}
    if(!values||values->size()!=mesh->positions.size())throw std::invalid_argument("Emission Bake lacks the actual original role coordinates");
    std::array<double,2> result{};
    for(size_t k=0;k<3;++k){const auto uv=values->at(mesh->indices[size_t(triangle)*3+k]);for(size_t c=0;c<2;++c)result[c]+=double(uv[c])*bary[k];}
    return {float(result[0]),float(result[1])};
  }
  std::array<float,2> image_coordinates(uint32_t triangle,std::array<float,3> bary)const {
    // PBR roles name transported current carriers; typed slots retain the
    // logical ordinal and any separately captured artist UV1.
    constexpr auto slot=MToonSlot::Emission;
    const auto uv=coordinates(triangle,bary,toon?mtoon_coordinate(toon->textures[size_t(slot)]):role.texcoord,bool(toon));
    return toon?mtoon_uv_validated(*toon,slot,uv,0,1):uv;
  }
  std::array<float,2> paint_coordinates(uint32_t triangle,std::array<float,3> bary)const {
    return bake_paint_coordinates(paint_domain.get(),triangle,bary,coordinates(triangle,bary,0,false));
  }
  std::array<float,3> resolve(uint32_t triangle,std::array<float,3> bary,
      std::array<float,3> image_value)const {
    stop();
    auto original=background;
    for(size_t k=0;k<3;++k){
      const double radiance=double(original[k])*image_value[k];
      if(radiance>std::numeric_limits<float>::max())
        throw std::overflow_error("Emission Bake source multiplication overflows FLOAT");
      original[k]=float(radiance);
    }
    BakedEmissionSample replacement;
    if(packet){
      const auto uv=paint_coordinates(triangle,bary);
      const auto values=sample_overlay_values(*packet,uv[0],uv[1],false);
      for(size_t k=0;k<3;++k){replacement.legacy[k]=values[2][k];replacement.absolute[k]=values[4][k];}
      replacement.coverage=values[3][3];
    }
    return replacement.resolve(original,legacy_strength);
  }
  std::array<float,3> sample(uint32_t triangle,std::array<float,3> bary)const {
    stop();
    if(!role.image||physical_background)return resolve(triangle,bary,{1,1,1});
    const auto uv=image_coordinates(triangle,bary);
    auto resolved=role;
    resolved.texcoord=0;
    const auto value=sample_source_texture(resolved,uv[0],uv[1],true);
    return resolve(triangle,bary,{value[0],value[1],value[2]});
  }
};
bool framed_source_bake(const SculptObject &object) noexcept {
  const auto &paint=object.painted;
  const auto &binding=object.material_bindings;
  if(!object.mesh_active||!paint||!binding||!binding->normal_frames||
     (!binding->has_mtoon&&object.material.toon)||binding->canonical_attributes||paint->paint_sampling_domain()||
     !paint->source_sampling_prepared()||!paint->source_maps()||!paint->valid_source_sampling_attachment()||
     paint->color_projection()||paint->normal_projection())return false;
  const auto &baked=paint->baked();
  return baked&&baked->normal_interpretation==BakedNormalInterpretation::CanonicalMikk&&
      baked->color_interpretation==BakedColorInterpretation::VertexComponent&&
      !baked->normal_authority&&!baked->normal_field;
}

struct ImportedBakedNormal {
  std::shared_ptr<const Mesh> geometry;
  AffineTransform placement;
  std::vector<NormalBakeVertex> columns;

  Vec3 core_normal(const Source &source, const std::vector<NormalBakeVertex> &artist,
                   uint32_t triangle, std::array<float,3> bary,const float *filtered) const {
    Vec3 original{};
    for(size_t k=0;k<3;++k)original+=artist.at(size_t(triangle)*3+k).n*bary[k];
    return source_sampled_normal_placed(*geometry,*source.maps,triangle,bary,original,placement,filtered);
  }

  Vec3 with_detail(const Source &source,Vec3 original,uint32_t triangle,
                   std::array<float,3> bary,bool mtoon=false,bool back=false) const {
    if(columns.empty())return original;
    const auto &event=*source.packet->baked();
    const auto &image=mtoon&&!event.maps.mtoon_normal.rgba.empty()?event.maps.mtoon_normal:event.maps.normal;
    const auto uv=source.coordinates(triangle,bary,0,false);
    const auto taps=paint_bilinear_taps(source.packet->resolution(),uv,*source.packet->padding(),true);
    Vec3 premultiplied{};
    float amount=0;
    for(const auto &tap:taps){
      const auto *p=image.rgba.data()+size_t(tap.texel)*4;
      const float alpha=float(tap.weight)*event.coverage[tap.texel]/255.f;
      amount+=alpha;
      premultiplied+=Vec3{p[0]*(2.f/255)-1,p[1]*(2.f/255)-1,p[2]*(2.f/255)-1}*alpha;
    }
    if(amount<=1e-8f)return original;
    const auto first=size_t(triangle)*3;
    const auto frame=normal_bake_basis({columns[first],columns[first+1],columns[first+2]},
        {bary[0],bary[1],bary[2]},NormalFragmentBasis::RawColumns);
    const auto unit_or_z=[](Vec3 v){
      const float m=std::max({std::abs(v.x),std::abs(v.y),std::abs(v.z)});
      return m>1e-20f?normalized(v/m):Vec3{0,0,1};
    };
    const auto detail=premultiplied/amount;
    const auto n=unit_or_z(frame.t*detail.x+frame.b*(back?-detail.y:detail.y)+
                          frame.n*(back?-detail.z:detail.z));
    return unit_or_z(original*(1-amount)+n*amount);
  }
};

const char *source_normal_domain_error(const SculptObject &object,bool prepared,bool writable) noexcept {
  const auto &binding=object.material_bindings;
  const auto canonical=binding->canonical_attributes;
  const auto event=object.painted?object.painted->baked():nullptr;
  const auto field=event?event->normal_field:nullptr;
  if(canonical&&(binding->has_mtoon||object.appearance||binding->normal_preparation||
      !canonical->valid_owner(canonical)||(field&&field->origin()!=canonical)||
      (binding->canonical_uv&&binding->canonical_uv->origin()!=canonical)))
    return "Canonical source normal capture requires its actual native conversion and current field";
  if(prepared){
    const auto &normal=*binding->normal_preparation;
    if(normal.canonical_conversion||
       (!writable&&normal.sampling_domain&&normal.sampling_domain->metadata_sampling_domain))
      return "Prepared source normal fields require their actual material authority";
    if(binding->sampling_reparameterized&&!normal.uv_reprojection&&!normal.sampling_domain)
      return "Prepared source UV reparameterization lost its owning normal/material recipe";
  }
  if(!object.mesh_active||!object.painted||!object.painted->valid_source_sampling_attachment()||
     (binding->canonical_uv&&!canonical)||(object.painted->paint_sampling_domain()&&!writable)||
     (binding->native_paint_frames&&!binding->native_paint_sampling)||
     bool(binding->normal_preparation)!=bool(object.painted->normal_projection())||
     (!canonical&&!writable&&object.painted->color_projection())||
     (event&&((!prepared&&!framed_source_bake(object)&&!event->maps.normal.rgba.empty()&&
       event->normal_interpretation!=BakedNormalInterpretation::NativeAttributes)||(field&&!canonical)||
      (!prepared&&event->normal_authority))))
    return "Artist source normal capture requires its unchanged original frame and separate current-normal adapter";
  return nullptr;
}

uint64_t normal_replay_bytes(const SourceMaterialBindings &binding,const std::function<void()> &stop) {
  // The validators finish ancestral replay before allocating their own scratch.
  // Check the longest route as well as the greatest declared scratch cap.
  struct Owner { const SourceMaterialBindings *binding; size_t height; };
  std::array<Owner,256> complete{};
  std::array<const SourceMaterialBindings *,64> active{};
  size_t count=0;
  uint64_t bytes=0;
  const auto visit=[&](auto &&self,const SourceMaterialBindings *node,size_t depth)->size_t {
    if(!node)return 0;
    stop();
    if(std::find(active.begin(),active.begin()+depth,node)!=active.begin()+depth)
      throw std::invalid_argument("Source normal replay authority is cyclic");
    if(depth==active.size())
      throw std::length_error("Source normal replay authority exceeds native graph limits");
    const auto end=complete.begin()+count;
    const auto found=std::find_if(complete.begin(),end,[node](const auto &owner){return owner.binding==node;});
    if(found!=end){
      if(found->height>active.size()-depth)
        throw std::length_error("Source normal replay authority exceeds native graph limits");
      return found->height;
    }
    active[depth]=node;
    size_t height=1;
    const auto descend=[&](const auto &parent){height=std::max(height,1+self(self,parent.get(),depth+1));};
    const auto check_conversion=[&](const auto &conversion){
      if(!conversion->valid_owner(conversion))
        throw std::invalid_argument("Source normal replay lost its canonical conversion owner");
      bytes=std::max<uint64_t>(bytes,conversion->maximum_working_bytes());
      descend(conversion->before_material());
    };
    if(node->canonical_attributes)check_conversion(node->canonical_attributes);
    if(const auto &paint=node->native_paint_sampling){
      if(!paint->valid_owner(paint))throw std::invalid_argument("Bake replay lost the native paint owner");
      bytes=std::max<uint64_t>(bytes,paint->before_paint()->storage_bytes()+paint->domain()->retained_bytes());
      descend(paint->before_material());
    }
    if(const auto &frames=node->native_paint_frames){
      if(!frames->valid_owner(frames))throw std::invalid_argument("Bake replay lost the native frame owner");
      bytes=std::max<uint64_t>(bytes,128*1024*1024);
      descend(frames->before_material());
    }
    if(const auto &paint=node->canonical_paint_sampling){
      if(!paint->valid_owner(paint))throw std::invalid_argument("Bake replay lost the canonical paint owner");
      bytes=std::max<uint64_t>(bytes,paint->attachment()->domain()->retained_bytes());
      descend(paint->before()->material_bindings);
    }
    if(const auto &color=node->color_preparation){
      if(color->original_paint)bytes=std::max<uint64_t>(bytes,color->original_paint->storage_bytes());
      if(const auto &uv=color->uv_reprojection){
        bytes=std::max(bytes,uv->receipt.max_working_bytes);
        descend(uv->before_material);
      }
      if(const auto &paint=color->paint_sampling){
        bytes=std::max<uint64_t>(bytes,paint->domain()->retained_bytes()+
            paint->before_paint()->storage_bytes()+paint->layout()->storage_bytes());
        descend(paint->before_material());
      }
      descend(color->original_material);
    }
    if(const auto &normal=node->normal_preparation){
      if(normal->canonical_conversion)check_conversion(normal->canonical_conversion);
      if(const auto &uv=normal->uv_reprojection){
        bytes=std::max(bytes,uv->receipt.max_working_bytes);
        descend(uv->before_material);
      }
      if(const auto &uv=normal->sampling_domain){
        bytes=std::max(bytes,uv->receipt.max_working_bytes);
        descend(uv->before_material);
        descend(uv->sampling_material);
      }
      if(normal->key_derivation)descend(normal->key_derivation->before_material());
      descend(normal->original_material);
    }
    if(node->key_derivation)descend(node->key_derivation->before_material());
    if(count==complete.size())
      throw std::length_error("Source normal replay authority exceeds native graph limits");
    complete[count++]={node,height};
    return height;
  };
  visit(visit,&binding,0);
  return bytes;
}

std::shared_ptr<const TextureMaps> paired_bake_maps(const SculptObject &object,
    const std::function<bool()> &cancelled) {
  const auto &paint=*object.painted;
  std::shared_ptr<TextureMaps> maps;
  if(paint.color_projection()){
    const bool physical=paint.has_physical_emission()||hdr_emission(object.material.emissive)||
        bool(object.material_bindings->physical_emission);
    const auto composite=physical?EmissionExportAccess::other_channels(paint,object.material,cancelled):
        paint.composite(object.material,cancelled);
    maps=std::make_shared<TextureMaps>(*composite);
  }else{
    maps=std::make_shared<TextureMaps>();
    maps->source_only=true;
    maps->source_maps=paint.source_maps();
    if(!maps->source_maps)maps->source_maps=std::make_shared<const PaintSourceMaps>();
  }
  maps->overlay=PaintOverlayFactory::capture_paired_bake(paint,paint.normal_projection(),cancelled);
  maps->normal_projection=paint.normal_projection();
  maps->baked_base=paint.baked();
  return maps;
}
}

bool SourceBakeNormalResolver::matches_prepared_source(const TextureMaps &maps) const noexcept {
  if(paired_maps_)return paired_maps_.get()==&maps;
  const auto same=[](const auto &a,const auto &b){
    return a==b&&!a.owner_before(b)&&!b.owner_before(a);
  };
  if(!projection_||!packet_||!same(maps.normal_projection,projection_))return false;
  if(!maps.source_only)return same(maps.baked_base,packet_->baked());
  if(!maps.overlay||!same(maps.source_maps,packet_->source_maps())||
     !same(maps.overlay->source_maps(),packet_->source_maps())||
     !same(maps.overlay->baked(),packet_->baked())||!same(maps.overlay->padding(),packet_->padding())||
     !same(maps.overlay->original_uv0(),packet_->original_uv0())||
     !same(maps.overlay->original_uv1(),packet_->original_uv1())||
     maps.overlay->resolution()!=packet_->resolution())return false;
  const auto same_tiles=[&](const auto &a,const auto &b){
    return a.size()==b.size()&&std::equal(a.begin(),a.end(),b.begin(),same);
  };
  const auto own=packet_->layers(),other=maps.overlay->layers();
  return own.size()==other.size()&&std::equal(own.begin(),own.end(),other.begin(),
      [&](const auto &a,const auto &b){
        return a.opacity==b.opacity&&same_tiles(a.tiles,b.tiles)&&
            same_tiles(a.pbr_tiles,b.pbr_tiles)&&same_tiles(a.emission_tiles,b.emission_tiles);
      });
}

bool writable_bake_source_supported(const SculptObject &object) noexcept {
  const auto &binding=object.material_bindings;
  const auto &paint=object.painted;
  if(!object.mesh_active||!paint||!binding||object.appearance||!paint->paint_sampling_domain()||
     binding->has_mtoon||binding->polygon_surface_transfer)return false;
  const bool corner_colors=object.polygon&&object.polygon->active&&object.polygon->colors;
  if(const auto &owner=binding->native_paint_sampling)
    return !corner_colors&&owner->valid_owner(owner)&&owner->matches_current(*paint)&&
        !binding->canonical_attributes&&!binding->color_preparation&&!binding->normal_preparation&&
        (!binding->native_paint_frames||binding->native_paint_frames->valid_owner(binding->native_paint_frames));
  if(const auto &owner=binding->canonical_paint_sampling)
    return !corner_colors&&owner->valid_owner(owner)&&owner->matches_current(*paint)&&
        binding->canonical_attributes&&binding->normal_frames;
  const auto &color=binding->color_preparation;
  const auto &normal=binding->normal_preparation;
  return color&&normal&&color->paint_sampling&&color->paint_sampling->matches_current(*paint)&&
      (!corner_colors||(color->current_descriptor()&&color->current_descriptor()->field==object.polygon->colors))&&
      paint->color_projection()&&paint->normal_projection()&&normal->projection&&
      normal_projection_identity(*paint->normal_projection(),*normal->projection);
}

bool affine_source_normal_supported(const SculptObject &object) noexcept {
  const auto &binding=object.material_bindings;
  const bool writable=writable_bake_source_supported(object);
  if(writable)return true;
  if(!binding||!binding->normal_frames||!object.painted)return false;
  return !source_normal_domain_error(object,object.painted->normal_projection()&&binding->normal_preparation,false);
}
SourceBakeNormalResolver source_bake_normals(const SculptObject &object,CanvasTransform relative,
    const SourceBakeNormalOptions &options){
  const auto &binding=object.material_bindings;
  const auto domain=object.painted?object.painted->paint_sampling_domain():nullptr;
  auto stop=[&]{if(options.cancelled&&options.cancelled())throw std::runtime_error("Source normal capture cancelled");};stop();
  PaintSamplingLimits limits;
  limits.max_working_bytes=options.max_working_bytes;
  limits.cancelled=options.cancelled;
  uint64_t writable_bytes=0;
  if(domain){
    if(!writable_bake_source_supported(object))
      throw std::invalid_argument("Bake source lacks its supported independent paint owner");
    const auto attachment=material_paint_attachment(binding.get());
    if(!attachment||!attachment->matches(object.painted->mesh_owner(),domain))
      throw std::invalid_argument("Bake source lost its exact current paint attachment");
    writable_bytes=domain->retained_bytes();
  }
  const auto canonical=binding?binding->canonical_attributes:nullptr;
  const auto event=object.painted?object.painted->baked():nullptr;
  const auto field=event?event->normal_field:nullptr;
  const bool native_attributes=event&&event->normal_interpretation==BakedNormalInterpretation::NativeAttributes;
  const auto validate_writable=[&] {
    source_paint_pairing(object.painted.get(),binding.get(),object.appearance.get(),true);
    domain->validate_current(object.painted->mesh(),limits);
    if(binding->native_paint_frames)
      binding->native_paint_frames->validate_binding(*binding,true,options.cancelled);
  };
  const bool native_baked=domain&&binding->native_paint_sampling&&event&&!native_attributes&&!event->maps.normal.rgba.empty();
  if(native_baked||(domain&&!binding->normal_frames&&binding->native_paint_sampling)){
    const auto &mesh=object.painted->mesh();
    const uint64_t bake_bytes=native_baked?
        uint64_t(object.painted->resolution())*object.painted->resolution()*36+
        mesh.indices.size()*(2*sizeof(Vec3)+sizeof(Tangent)+sizeof(std::array<float,2>)+sizeof(uint32_t)):0;
    const uint64_t capture=object.painted->storage_bytes()+writable_bytes+bake_bytes+
        mesh.positions.size()*(sizeof(Vec3)*3+sizeof(std::array<float,2>))+
        mesh.indices.size()*sizeof(uint32_t);
    const auto work=std::max(capture,normal_replay_bytes(*binding,stop));
    if(!options.max_working_bytes||options.max_working_bytes>512*1024*1024||work>options.max_working_bytes)
      throw std::length_error("Native paint Bake exceeds prospective working budget");
    validate_writable();
    auto posed=object.deformation?std::optional(object.deformation->preview(mesh)):std::nullopt;
    auto normals=std::make_shared<std::vector<Vec3>>(posed?posed->normals:mesh.normals);
    std::vector<Tangent> tangents;
    PaintSourceTexture normal_image;
    if(native_baked){
      auto raster=native_paint_bake_mesh(posed?*posed:mesh,*domain,options.cancelled);
      tangents=tangent_corners(raster,options.cancelled);
      for(size_t c=0;c<tangents.size();++c){
        if((c&1023)==0)stop();
        auto t=options.placement?placed_tangent(*options.placement,tangents[c]):tangents[c];
        const auto rotated=ob::rotate(relative.orientation,{t[0],t[1],t[2]});
        tangents[c]={rotated.x,rotated.y,rotated.z,t[3]};
      }
      auto composite=object.painted->preview(object.material,options.cancelled);
      if(composite->normal.rgba.empty())
        throw std::logic_error("Native writable Bake source lost its composed normal map");
      const auto *image=&composite->normal;
      normal_image.image=std::shared_ptr<const Image>(std::move(composite),image);
      normal_image.wrap_s=normal_image.wrap_t=33071;
      normal_image.min_filter=normal_image.mag_filter=9729;
    }
    for(size_t v=0;v<normals->size();++v){
      if((v&1023)==0)stop();
      auto n=(*normals)[v];
      if(options.placement)n=placed_normal(*options.placement,n);
      const auto rotated=ob::rotate(relative.orientation,{n.x,n.y,n.z});
      (*normals)[v]=normalized(Vec3{rotated.x,rotated.y,rotated.z});
    }
    auto sample=[normals=std::shared_ptr<const std::vector<Vec3>>(std::move(normals)),
        tangents=std::move(tangents),normal_image=std::move(normal_image),domain,
        geometry=object.painted->mesh_owner(),cancelled=options.cancelled](uint32_t triangle,std::array<float,3> bary,bool,
            const SourceBakeNormalResolver::FilteredTextures *){
      if(cancelled&&cancelled())throw std::runtime_error("Native paint Bake sampling cancelled");
      TangentFrame frame;
      for(size_t k=0;k<3;++k){
        const auto corner=size_t(triangle)*3+k;
        const auto n=normals->at(geometry->indices.at(corner));
        frame.normal+=n*bary[k];
        if(normal_image.image){
          const auto t=tangents.at(corner);
          const Vec3 direction{t[0],t[1],t[2]};
          frame.tangent+=direction*bary[k];
          frame.bitangent+=cross(n,direction)*(bary[k]*t[3]);
        }
      }
      auto n=normalized(frame.normal);
      if(normal_image.image){
        const auto uv=bake_paint_coordinates(domain.get(),triangle,bary,{});
        const auto rgb=sample_source_texture(normal_image,uv[0],uv[1],false);
        n=frame.to_world({rgb[0]*2-1,rgb[1]*2-1,rgb[2]*2-1});
      }
      return std::array{n,n};
    };
    auto maps=paired_bake_maps(object,options.cancelled);
    const auto packet=maps->overlay;
    return SourceBakeNormalResolver(std::move(sample),{},packet,std::move(maps),domain);
  }
  if(!binding||!binding->normal_frames){
    if(canonical||field||domain)throw std::invalid_argument("Bake source lost its current frame owner");
    return {};
  }
  const bool prepared=object.painted&&object.painted->normal_projection()&&binding->normal_preparation;
  if(const auto error=source_normal_domain_error(object,prepared,bool(domain)))throw std::invalid_argument(error);
  const auto preparation=prepared?binding->normal_preparation:nullptr;
  const auto retained=prepared?object.painted->normal_projection()->sampling_domain:nullptr;
  const auto canonical_uv=canonical?binding->canonical_uv:nullptr;
  const auto &mesh=object.painted->mesh();const auto &set=*binding->normal_frames;
  const bool framed_bake=framed_source_bake(object)&&!event->maps.normal.rgba.empty();
  const uint64_t corners=mesh.indices.size();
  // Charge both posed role arrays, immutable evaluated corner columns and
  // layout scratch before allocating, including retained morph/frame payload.
  const uint64_t preview_bytes=(native_attributes||prepared||((canonical||domain)&&object.painted->color_projection()))?
      uint64_t(object.painted->resolution())*object.painted->resolution()*4*12:0;
  const uint64_t capture_bytes=preview_bytes+set.storage_bytes()+
      corners*(3*sizeof(uint32_t)+2*(sizeof(Vec3)+sizeof(std::array<float,4>)+sizeof(NormalBakeVertex)))+
      (retained?retained->storage_bytes():0)+(canonical_uv?canonical_uv->storage_bytes():0)+
      (field?field->retained_bytes():0)+writable_bytes+
      (framed_bake?mesh.positions.size()*64+corners*(sizeof(Tangent)*5+sizeof(NormalBakeVertex)+sizeof(uint32_t)):0);
  const uint64_t work=std::max(capture_bytes,(prepared||canonical||domain)?normal_replay_bytes(*binding,stop):0);
  if(!options.max_working_bytes||options.max_working_bytes>512*1024*1024||work>options.max_working_bytes)
    throw std::length_error("Artist source normal capture exceeds prospective working budget");
  normal_frame_current_authority(*binding,mesh,object.deformation.get(),options.cancelled);
  if(!set.pbr||(binding->has_mtoon&&!set.mtoon))throw std::invalid_argument("Artist source lost its explicitly declared PBR/MToon frame roles");
  if(domain)validate_writable();
  if(canonical){
    if(set.pbr->tangents.empty())throw std::invalid_argument("Canonical normal frames require their actual tangents");
    if(!domain)source_paint_pairing(object.painted.get(),binding.get(),nullptr,true);
    if(canonical_uv)canonical_uv->validate_current(mesh,options.cancelled);
    else{
      const auto &original=canonical->before_paint()->mesh();
      if(original.indices.size()!=mesh.indices.size())
        throw std::invalid_argument("Canonical source lost its ordered normal coordinates");
      for(size_t c=0;c<mesh.indices.size();++c){
        if((c&1023)==0)stop();
        if(mesh.uv.at(mesh.indices[c])!=original.uv.at(original.indices[c]))
          throw std::invalid_argument("Canonical source needs the owning normal UV correspondence");
      }
    }
    if(field)field->validate();
  }
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
    if(!normal_projection_identity(*object.painted->normal_projection(),*preparation->projection))
      throw std::invalid_argument("Prepared source lost its actual intrinsic normal authority");
    // The owning accessor resolves actual intrinsic/detail normal events
    // without asking unrelated material images to composite through UV0.
    current_normals=SourceNormalPreparationAccess::resolved(*object.painted,options.cancelled);
    current_normals->validate(object.painted->resolution());
  }
  auto paired_maps=(canonical||domain||framed_bake)?paired_bake_maps(object,options.cancelled):nullptr;
  PaintSourceTexture native_normal;
  if(native_attributes){
    if(!domain||!binding->native_paint_frames||set.pbr->tangents.empty())
      throw std::invalid_argument("Authored Bake donor lost its writable atlas or tangent frame");
    auto composite=object.painted->preview(object.material,options.cancelled);
    if(composite->normal.rgba.empty())
      throw std::logic_error("Authored Bake donor lost its resolved normal image");
    const auto *image=&composite->normal;
    native_normal.image=std::shared_ptr<const Image>(std::move(composite),image);
    native_normal.wrap_s=native_normal.wrap_t=33071;
    native_normal.min_filter=native_normal.mag_filter=9729;
  }
  Source coordinates;coordinates.mesh=object.painted->mesh_owner();coordinates.maps=object.painted->source_maps();
  coordinates.packet=paired_maps?paired_maps->overlay:PaintOverlayFactory::capture(*object.painted,options.cancelled);
  coordinates.authored_mtoon_uv1=options.authored_mtoon_uv1;
  coordinates.independent_source=object.painted->source_sampling_prepared();
  coordinates.cancelled=options.cancelled;
  if(!coordinates.maps)coordinates.maps=std::make_shared<const PaintSourceMaps>();
  coordinates.maps->validate_surface(mesh);
  const auto typed=binding->has_mtoon?std::optional<MToonMaterial>(binding->material):std::optional<MToonMaterial>{};
  if(typed&&(typed->parameters.scroll!=std::array<float,2>{}||typed->parameters.rotation_speed!=0))
    throw std::invalid_argument("Artist normal source requires an explicit frozen UV animation time");
  PaintSourceTexture typed_role;if(!prepared&&typed&&typed->textures[size_t(MToonSlot::Normal)].index>=0){typed_role=binding->roles[size_t(MToonSlot::Normal)];if(!typed_role.image)throw std::invalid_argument("Artist source declared typed normal has no actual image owner");}
  if((coordinates.maps->normal.image&&set.pbr->tangents.empty())||(typed&&typed->parameters.normal_enabled&&typed_role.image&&set.mtoon->tangents.empty()))
    throw std::invalid_argument("Artist normal texture requires its genuine declared tangent attribute role");
  NormalFrameLayout layout;layout.vertices=mesh.indices;layout.corners.resize(size_t(corners));std::iota(layout.corners.begin(),layout.corners.end(),0u);
  const auto posed=normal_frame_preview(set,layout,object.deformation.get(),options.cancelled,
      (typed?3u:1u)|(framed_bake&&set.canonical?4u:0u));
  std::optional<ImportedBakedNormal> source8;
  if(framed_bake) {
    auto geometry=object.deformation?std::make_shared<const Mesh>(object.deformation->preview(mesh)):coordinates.mesh;
    const auto placement=affine_similarity(relative)*(options.placement?options.placement->world():AffineTransform{});
    source8.emplace(ImportedBakedNormal{std::move(geometry),placement,{}});
    if(!event->maps.normal.rgba.empty()){
      const auto &evaluated=*source8->geometry;
      const auto generated=set.canonical?std::vector<Tangent>{}:tangent_corners(evaluated,options.cancelled);
      const auto &tangents=set.canonical?posed.tangents[2]:generated;
      const AffineInteractionFrame frame(placement);
      source8->columns.reserve(corners);
      for(size_t c=0;c<corners;++c){
        if((c&1023)==0)stop();
        const auto n=placed_normal(frame,set.canonical?posed.normals[2][c]:evaluated.normals[mesh.indices[c]]);
        const auto t=placed_tangent(frame,tangents[c]);
        source8->columns.push_back(normal_bake_vertex(n,{t[0],t[1],t[2]},t[3],NormalVertexBitangent::RawCross));
      }
    }
  }
  const std::array<bool,2> has_tangent{!set.pbr->tangents.empty(),typed&&!set.mtoon->tangents.empty()};
  auto vertices=std::make_shared<std::array<std::vector<NormalBakeVertex>,2>>();
  for(size_t role=0;role<(typed?2u:1u);++role){
    auto &out=vertices->at(role);out.reserve(size_t(corners));
    for(size_t c=0;c<corners;++c){if((c&1023)==0)stop();auto N=posed.normals[role].at(c);Vec3 T{1,0,0};float w=1;
      if(!posed.tangents[role].empty()){const auto t=posed.tangents[role][c];T={t[0],t[1],t[2]};w=t[3];}
      if(options.placement){
        N=placed_normal(*options.placement,N);
        if(has_tangent[role]){
          const auto t=placed_tangent(*options.placement,{T.x,T.y,T.z,w});
          T={t[0],t[1],t[2]};w=t[3];
        }
      }
      auto n=ob::rotate(relative.orientation,{N.x,N.y,N.z}),t=ob::rotate(relative.orientation,{T.x,T.y,T.z});
      if(has_tangent[role])out.push_back(normal_bake_vertex({n.x,n.y,n.z},{t.x,t.y,t.z},w,
          (prepared||field||native_attributes)?NormalVertexBitangent::UnitCross:NormalVertexBitangent::RawCross));
      else out.push_back({normalized(Vec3{n.x,n.y,n.z}),{},{},1});
    }
  }
  // W is a per-triangle discrete attribute, never a barycentric float sign.
  for(size_t role=0;role<(typed?2u:1u);++role)for(size_t c=0;c<corners;c+=3){if((c&1023)==0)stop();if(vertices->at(role)[c].w!=vertices->at(role)[c+1].w||vertices->at(role)[c].w!=vertices->at(role)[c+2].w)throw std::invalid_argument("Artist source tangent handedness changes inside a triangle");}
  stop();std::shared_ptr<const std::array<std::vector<NormalBakeVertex>,2>> immutable=std::move(vertices);
  const auto packet=coordinates.packet;
  SourceBakeNormalResolver::TextureUv texture_uv;
  if(!prepared&&!field&&!native_normal.image&&(!source8||!typed)) {
    texture_uv=[coordinates,typed,derivative=bool(source8)](unsigned role,uint32_t triangle,std::array<float,3> bary) {
      if(derivative)
        return source_texture_uv(*coordinates.mesh,*coordinates.maps,coordinates.maps->normal.texcoord,triangle,bary);
      constexpr auto slot=MToonSlot::Normal;
      const auto uv=coordinates.coordinates(triangle,bary,
          role?mtoon_coordinate(typed->textures[size_t(slot)]):coordinates.maps->normal.texcoord,role!=0);
      return role?mtoon_uv_validated(*typed,slot,uv,0,1):uv;
    };
  }
  auto sample=[immutable,coordinates=std::move(coordinates),typed,typed_role,has_tangent,prepared,
      current_normals,retained,field,canonical_uv,native_normal=std::move(native_normal),domain,
      source8=std::move(source8)](uint32_t triangle,std::array<float,3> bary,bool mtoon_back,
          const SourceBakeNormalResolver::FilteredTextures *filtered){
    coordinates.stop();const std::array<double,3> weights{bary[0],bary[1],bary[2]};std::array<Vec3,2> result;
    if(source8){
      const auto original=source8->core_normal(coordinates,immutable->at(0),triangle,bary,
                                             filtered?(*filtered)[0].data():nullptr);
      result[0]=source8->with_detail(coordinates,original,triangle,bary);
      if(!typed)return std::array{result[0],result[0]};
    }
    if(field){
      // Both canonical planes are encoded against the same prepared PBR columns.
      std::array<NormalBakeVertex,3> corners;
      for(size_t k=0;k<3;++k)corners[k]=immutable->at(0).at(size_t(triangle)*3+k);
      const auto basis=normal_bake_basis(corners,weights,NormalFragmentBasis::UnitColumns);
      auto uv=coordinates.coordinates(triangle,bary,0,false);
      if(canonical_uv){
        std::array<double,2> original{};
        for(size_t k=0;k<3;++k){
          const auto vertex=canonical_uv->mesh()->indices[size_t(triangle)*3+k];
          const auto q=canonical_uv->coordinates()[vertex];
          for(size_t c=0;c<2;++c)original[c]+=double(q[c])*bary[k];
        }
        uv={float(original[0]),float(original[1])};
      }
      return field->sample(basis,uv);
    }
    if(native_normal.image){
      std::array<NormalBakeVertex,3> corners;
      for(size_t k=0;k<3;++k)corners[k]=immutable->at(0).at(size_t(triangle)*3+k);
      const auto basis=normal_bake_basis(corners,weights,NormalFragmentBasis::UnitColumns);
      const auto uv=bake_paint_coordinates(domain.get(),triangle,bary,{});
      const auto rgb=sample_source_texture(native_normal,uv[0],uv[1],false);
      const auto n=mtoon_normal_map({rgb[0],rgb[1],rgb[2]},1);
      const auto normal=normalized(basis.t*n.x+basis.b*n.y+basis.n*n.z);
      return std::array{normal,normal};
    }
    for(size_t role=source8?1:0;role<(typed?2u:1u);++role){
      std::array<NormalBakeVertex,3> corners;for(size_t k=0;k<3;++k)corners[k]=immutable->at(role).at(size_t(triangle)*3+k);
      const bool back=mtoon_back&&role==1&&typed->version()==MToonVersion::V1;
      if(!has_tangent[role]){Vec3 N;for(size_t k=0;k<3;++k)N+=corners[k].n*bary[k];result[role]=normalized(N)*(back?-1.f:1.f);continue;}
      const auto mode=role==0?NormalFragmentBasis::UnitColumns:typed->version()==MToonVersion::V0?NormalFragmentBasis::RawColumns:NormalFragmentBasis::UnitCross;
      auto basis=normal_bake_basis(corners,weights,mode);
      if(back){basis.n=basis.n*-1;basis.b=basis.b*-1;}
      result[role]=normalized(basis.n);
      auto view=role==0?coordinates.maps->normal:typed_role;const bool enabled=role==0||typed->parameters.normal_enabled;
      if(prepared){
        view={};view.wrap_s=view.wrap_t=33071;view.min_filter=view.mag_filter=9729;
        view.image=role==0?current_normals->pbr:current_normals->mtoon;
      }
      if(enabled&&view.image){
        const auto value=[&] {
          if(filtered)return (*filtered)[role];
          auto uv=retained?std::array<float,2>{}:coordinates.coordinates(triangle,bary,
              prepared?0:role==0?view.texcoord:mtoon_coordinate(typed->textures[size_t(MToonSlot::Normal)]),!prepared&&role!=0);
          if(retained){
            const auto &values=role&&!retained->mtoon_coordinates.empty()?retained->mtoon_coordinates:retained->coordinates;
            std::array<double,2> point{};
            for(size_t k=0;k<3;++k){
              const auto q=values[retained->indices[size_t(triangle)*3+k]];
              for(size_t c=0;c<2;++c)point[c]+=double(q[c])*bary[k];
            }
            uv={float(point[0]),float(point[1])};
          }
          if(role&&!prepared)uv=mtoon_uv(*typed,MToonSlot::Normal,uv,0);
          view.texcoord=0;
          return sample_source_texture(view,uv[0],uv[1],false);
        }();
        const float scale=role==0?(prepared?1:coordinates.maps->normal_scale):typed->parameters.normal_scale;
        const auto ts=mtoon_normal_map({value[0],value[1],value[2]},scale);
        result[role]=normalized(basis.t*ts.x+basis.b*ts.y+basis.n*ts.z);
      }
    }
    if(source8)result[1]=source8->with_detail(coordinates,result[1],triangle,bary,true,
        mtoon_back&&typed->version()==MToonVersion::V1);
    else if(!typed)result[1]=result[0];
    return result;
  };
  return SourceBakeNormalResolver(std::move(sample),prepared?object.painted->normal_projection():nullptr,
      packet,std::move(paired_maps),domain,std::move(texture_uv));
}
std::shared_ptr<const TextureMaps> raw_bake_source(const SculptObject &object,
    const std::function<bool()> &cancelled,const SourceBakeNormalResolver *captured_normals){
  if(cancelled&&cancelled())throw std::runtime_error("Raw source Bake cancelled");
  const auto &binding=object.material_bindings;
  const auto typed=binding&&binding->has_mtoon?std::optional<MToonMaterial>(binding->material):
      (object.appearance?retained_mtoon(object.appearance.get()):std::optional<MToonMaterial>{});
  require_original_emission_uv1(object,typed);
  const bool framed_bake=framed_source_bake(object)&&!object.painted->baked()->maps.normal.rgba.empty();
  if(framed_bake||(binding&&binding->canonical_attributes)||(object.painted&&(object.painted->paint_sampling_domain()||
      (object.painted->baked()&&object.painted->baked()->normal_field)))){
    const auto maps=captured_normals?captured_normals->paired_maps():
        source_bake_normals(object,{},SourceBakeNormalOptions{256*1024*1024,cancelled}).paired_maps();
    if(!maps)throw std::invalid_argument("Bake source requires its paired material capture");
    if(framed_bake&&(!maps->overlay||maps->baked_base!=object.painted->baked()||
       maps->source_maps!=object.painted->source_maps()||maps->overlay->source_baked_geometry()!=object.painted->mesh_owner()))
      throw std::invalid_argument("Prepared source Bake requires its own material, geometry and normal capture");
    return maps;
  }
  const auto has_frames=[&]{
    return captured_normals?bool(*captured_normals):
        bool(source_bake_normals(object,{},SourceBakeNormalOptions{256*1024*1024,cancelled}));
  };
  if(object.mesh_active&&object.painted&&object.painted->normal_projection()){
    // Current prepared planes are already independently composed and retain
    // their owning coordinate recipe. Never reinterpret them as raw images.
    if(!has_frames())
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
  const bool framed=has_frames();
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
  // Keep the original material frames and the independently baked atlas
  // frame paired with their current geometry and deformation.
  if(framed_source_bake(object))
    normal_frame_current_authority(*object.material_bindings,object.painted->mesh(),object.deformation.get(),cancelled);
  else (void)raw_bake_source(object,cancelled);
  return PaintOverlayFactory::capture_source_baked(*object.painted,cancelled);
}
std::function<std::array<float,3>(uint32_t,std::array<float,3>)>
emission_bake_source(const SculptObject &object,const std::function<bool()> &cancelled,
    const SourceBakeNormalResolver *captured_normals,std::shared_ptr<const EmissionBakeBatch> *batch,
    bool require_absolute){
  if(batch)batch->reset();
  if(cancelled&&cancelled())throw std::runtime_error("Emission Bake cancelled");
  const auto &binding=object.material_bindings;
  std::optional<MToonMaterial> typed;
  if(binding&&binding->has_mtoon)typed=binding->material;
  else if(object.appearance)typed=retained_mtoon(object.appearance.get());
  require_original_emission_uv1(object,typed);
  const bool typed_hdr=typed&&(typed->parameters.emission_strength>1||
      typed->parameters.emission.x>1||typed->parameters.emission.y>1||typed->parameters.emission.z>1);
  const bool hdr=std::any_of(object.material.emissive.begin(),object.material.emissive.end(),[](float x){return x>1;});
  if(!require_absolute&&!hdr&&!typed_hdr&&!(object.painted&&object.painted->has_physical_emission())&&
     !(binding&&binding->physical_emission))return {};
  object.material.validate();if(binding)binding->validate();
  auto source=std::make_shared<Source>();source->background=object.material.emissive;source->cancelled=cancelled;
  if(object.mesh_active&&object.painted){
    source->mesh=object.painted->mesh_owner();source->maps=object.painted->source_maps();
    source->independent_source=object.painted->source_sampling_prepared();
    if(const auto domain=object.painted->paint_sampling_domain()){
      const auto detached=captured_normals?SourceBakeNormalResolver{}:
          source_bake_normals(object,{},SourceBakeNormalOptions{256*1024*1024,cancelled});
      const auto &capture=captured_normals?*captured_normals:detached;
      if(!capture.paired_maps()||capture.paint_domain()!=domain||
         capture.paint_domain().owner_before(domain)||domain.owner_before(capture.paint_domain()))
        throw std::invalid_argument("Emission Bake lost its exact current paint geometry attachment");
      source->packet=capture.paired_maps()->overlay;
      source->paint_domain=domain;
    }else source->packet=PaintOverlayFactory::capture(*object.painted,cancelled);
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
  if(batch){
    auto work=std::make_shared<EmissionBakeBatch>();
    work->overlay=immutable->packet;
    work->background=immutable->background;
    work->legacy_strength=immutable->legacy_strength;
    if(!immutable->physical_background)work->source=immutable->role;
    work->sample=[immutable](uint32_t triangle,std::array<float,3> bary){
      immutable->stop();
      EmissionRasterDevice::BakeRadianceSample sample;
      if(immutable->role.image&&!immutable->physical_background){
        const auto uv=immutable->image_coordinates(triangle,bary);
        std::copy_n(uv.begin(),2,sample.image_uv);
      }
      if(immutable->packet){
        const auto uv=immutable->paint_coordinates(triangle,bary);
        std::copy_n(uv.begin(),2,sample.paint_uv);
      }
      return sample;
    };
    *batch=std::move(work);
  }
  return [immutable](uint32_t triangle,std::array<float,3> bary){return immutable->sample(triangle,bary);};
}
} // namespace dome::detail
