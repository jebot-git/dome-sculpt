// SPDX-License-Identifier: GPL-3.0-or-later
#include "emission-export.hpp"
#include "baked-emission-write-scope.hpp"
#include "emission-paint-access.hpp"
#include "emission-raster-device.hpp"
#include "emission-linear.hpp"
#include "baked-emission-access.hpp"
#include "emission-fallback-metrics.hpp"
#include "emission-authority.hpp"
#include "emission-bake-source.hpp"
#include "paint-overlay-packet.hpp"
#include "source-paint-export.hpp"
#include "native-paint-sampling.hpp"
#include "texture-set-export.hpp"
#include "texture-set-paint-access.hpp"
#include "native-paint-export-policy.hpp"
#include "native-paint-frame-export-policy.hpp"
#include "canonical-paint-export-policy.hpp"
#include "dome/paint-sampling.hpp"
#include "dome/material-bindings.hpp"
#include "dome/source-maps.hpp"
#include "dome/material-visibility.hpp"
#include "dome/humanoid-creator.hpp"
#include <nlohmann/json.hpp>
#include <bit>
#include <mutex>
#include <set>
#include <tuple>
namespace dome::detail {namespace {
void stop(const std::function<bool()> &cancelled){if(cancelled&&cancelled())throw std::runtime_error("HDR emission export cancelled");}
bool object_has_hdr(const SculptObject &object){
 return hdr_emission(object.material.emissive)||
        (object.painted&&object.painted->has_physical_emission())||
        (object.material_bindings&&physical_emission(*object.material_bindings));
}
float linear(float v){return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);}
float srgb(float v){return v<=.0031308f?v*12.92f:1.055f*std::pow(v,1/2.4f)-.055f;}
bool exact(const auto&a,const auto&b){return a.size()==b.size()&&(a.empty()||std::memcmp(a.data(),b.data(),a.size()*sizeof(a[0]))==0);}
bool same_mesh(const Mesh&a,const Mesh&b){return exact(a.positions,b.positions)&&exact(a.normals,b.normals)&&exact(a.uv,b.uv)&&a.indices==b.indices;}
// This offline texture export evaluates an explicit current UV0 view. Original
// animation/samplers and independent coefficient planes remain native owners;
// the standard fallback is not claimed to represent their future evaluations.
EmissionExportRaster raster(const SculptObject &object,const std::function<bool()> &cancelled,
                            bool authored_mtoon,bool native_domain,bool streaming){
 if(!object.mesh_active||!object.painted)
  throw std::invalid_argument("HDR export currently requires an owning editable mesh");
 stop(cancelled);
 const auto &paint=*object.painted;
 const auto packet=streaming?nullptr:PaintOverlayFactory::capture(paint,cancelled);
 const auto padding=packet?std::span<const int32_t>(*packet->padding()):
                           EmissionExportAccess::writable_padding(paint);
 const auto r=paint.resolution();
 const auto count=padding.size();
 // The scene already owns the retained Bake, layers and raster. In the
 // scanline path they are borrowed in place, so only new staging is charged.
 size_t storage=packet?packet->storage_bytes():sizeof(EmissionExportRaster);
 const auto &maps=paint.source_maps();const auto &binding=object.material_bindings;std::optional<MToonMaterial> toon;
 if(authored_mtoon){
  if(binding&&binding->has_mtoon){
   if(streaming)storage+=binding->material.copy_working_bytes(16u*1024*1024,cancelled);
   toon=binding->material;
  }else if(object.appearance){
   toon=retained_mtoon(object.appearance.get());
   if(streaming&&toon)storage+=toon->copy_working_bytes(16u*1024*1024,cancelled);
  }
 }
 PaintSourceTexture typed_emission;float legacy_strength=1;EmissionRgb typed_background{};
 if(toon){toon->validate();const auto &parameters=toon->parameters;legacy_strength=parameters.emission_strength;if(parameters.scroll!=std::array<float,2>{}||parameters.rotation_speed!=0)throw std::invalid_argument("Typed HDR export requires explicit frozen UV animation, not an animated compiled emission texture");const auto &slot=toon->textures[size_t(MToonSlot::Emission)];if(mtoon_coordinate(slot)!=0&&slot.index>=0)throw std::invalid_argument("Typed HDR emission needs its actual alternate-coordinate export raster");if(toon->version()==MToonVersion::V0){const auto &main=toon->textures[0];if(main.offset!=std::array<float,2>{}||main.scale!=std::array<float,2>{1,1}||main.rotation!=0)throw std::invalid_argument("VRM0 shared ST requires genuine all-role UV preparation before compiled HDR emission");}SourceNativeMaterial reference;if(binding)reference=binding->native_reference;else reference=native_source(*object.appearance);const std::array<float,3> source{parameters.emission.x,parameters.emission.y,parameters.emission.z};for(size_t k=0;k<3;++k)typed_background[k]=mtoon_factor(source[k],reference.emission[k],object.material.emissive[k])*legacy_strength;
  if(slot.index>=0){if(binding)typed_emission=binding->roles[size_t(MToonSlot::Emission)];else {const auto &appearance=*object.appearance;const auto &texture=appearance.asset->textures.at(size_t(slot.index));if(texture.image<0||!appearance.decoded_images||size_t(texture.image)>=appearance.decoded_images->size())throw std::invalid_argument("Typed HDR emission has no owned original image");typed_emission.image=appearance.decoded_images->at(size_t(texture.image));typed_emission.wrap_s=texture.wrap_s;typed_emission.wrap_t=texture.wrap_t;typed_emission.min_filter=texture.min_filter;typed_emission.mag_filter=texture.mag_filter;}if(!typed_emission.image)throw std::invalid_argument("Typed HDR emission role has no image owner");}
 }if(paint.paint_sampling_domain()&&!native_domain)throw std::invalid_argument("Retained HDR paint export requires its exact current/source UV correspondence adapter");if(maps&&maps->emission.image&&(maps->emission.texcoord!=0))throw std::invalid_argument("HDR emission source requires its exact alternate-coordinate export raster");
 const auto baked=paint.baked();
 const bool baked_float=baked&&baked->emission_field;
 EmissionRgb original=toon?typed_background:object.material.emissive;if(binding&&binding->physical_emission)original=*binding->physical_emission;
 const bool source_overridden=binding&&binding->physical_emission;
 const bool typed_sampler=toon&&typed_emission.image&&!source_overridden;
 if(compute_backend()!="CPU"){
  struct State {
   std::mutex mutex;
   std::unique_ptr<EmissionRasterDevice> device;
  };
  auto state=std::make_shared<State>();
  PaintSourceTexture source;
  std::optional<MToonTexture> transform;
  if(typed_sampler){
    source=typed_emission;
    // The admitted VRM 0.0 shared transform is identity. At time zero
    // both legacy animation conjugation and the VRM 1.0 pivot are exact
    // identities on the native power-of-two texel centers.
    if(toon->version()==MToonVersion::V1){
      const auto &slot=toon->textures[size_t(MToonSlot::Emission)];
      if(slot.offset!=std::array<float,2>{}||slot.scale!=std::array<float,2>{1,1}||slot.rotation!=0)
        transform=slot;
    }
  }else if(!toon&&!source_overridden&&maps)source=maps->emission;
  // Metadata-only qualification does not allocate an upload. Scanline
  // readers retain the same immutable, device-resident grid across passes.
  auto read=[state,owner=paint.snapshot(),padding,source=std::move(source),transform,original,legacy_strength,r,cancelled]
            (uint32_t y0,std::span<EmissionExrPixel> rows){
    const size_t first=size_t(y0)*r;
    if(first>padding.size()||rows.size()>padding.size()-first)
      throw std::logic_error("HDR field read exceeds its owning raster");
    stop(cancelled);
    if(rows.empty())return;
    std::lock_guard lock(state->mutex);
    if(!state->device)
      state->device=create_emission_raster_device(*owner,padding,&source,transform?&*transform:nullptr,
                                                  original,legacy_strength,cancelled);
    state->device->read(first,rows,cancelled);
  };
  return {r,baked_float?2u:1u,storage+emission_raster_staging_bytes(paint),std::move(read)};
 }
 auto read=[padding,baked,&paint,cancelled,maps,binding,
            toon=std::move(toon),typed_emission=std::move(typed_emission),legacy_strength,original,baked_float,r,count]
           (uint32_t y0,std::span<EmissionExrPixel> rows){
 const size_t begin=size_t(y0)*r;
 if(begin>count||rows.size()>count-begin)throw std::logic_error("HDR field read exceeds its owning raster");
 for(size_t i=0;i<rows.size();++i){const size_t index=begin+i;if((index&1023)==0)stop(cancelled);auto p=padding[index];const auto source=p<0?index:size_t(p);const auto x=uint32_t(source%r),y=uint32_t(source/r);EmissionOverlay overlay;BakedEmissionSample resolved_base;
  if(baked_float)resolved_base=baked_emission_texel(*baked,source);
  else if(baked){const float c=float(baked->coverage.at(source))/255;overlay.coverage=c;for(size_t k=0;k<3;++k)overlay.legacy[k]=linear(float(baked->maps.emission.rgba.at(source*4+k))/255)*c;}
  for(const auto &layer:paint.layers())if(layer.visible&&layer.opacity>0){const auto &common=paint.pixel(layer.id,x,y);auto value=paint.emission_pixel(layer.id,x,y);const auto opacity=layer.opacity*float(common.data[13])/255*value.coverage;if(opacity>0){if(baked_float){BakedEmissionSample above;above.coverage=opacity;for(size_t k=0;k<3;++k){above.legacy[k]=value.legacy[k]*opacity;above.absolute[k]=value.physical[k]*opacity;}resolved_base=BakedEmissionField::over(resolved_base,above);}else overlay=emission_over(overlay,value,opacity);}}
  auto background=original;if(toon&&typed_emission.image&&!(binding&&binding->physical_emission)){auto uv=mtoon_uv(*toon,MToonSlot::Emission,{(float(x)+.5f)/r,(float(y)+.5f)/r},0);auto sample=sample_source_texture(typed_emission,uv[0],uv[1],true);for(size_t k=0;k<3;++k)background[k]*=sample[k];}else if(!toon&&maps&&maps->emission.image&&!(binding&&binding->physical_emission)){auto sample=sample_source_texture(maps->emission,(float(x)+.5f)/r,(float(y)+.5f)/r,true);for(size_t k=0;k<3;++k)background[k]*=sample[k];}
  const auto resolved=baked_float?resolved_base.resolve(background,legacy_strength):emission_radiance(overlay,background,legacy_strength);auto &destination=rows[i];for(size_t k=0;k<3;++k){destination[k]=resolved[k];destination[3+k]=baked_float?resolved_base.legacy[k]:overlay.legacy[k];destination[6+k]=baked_float?resolved_base.absolute[k]:overlay.physical[k];}destination[9]=baked_float?resolved_base.coverage:overlay.coverage;
 }
 };
 return {r,baked_float?2u:1u,storage,std::move(read)};
}
EmissionInterchangeImage materialize(const EmissionExportRaster &raster){
 const auto count=size_t(raster.resolution)*raster.resolution;
 if(raster.working_bytes>256*1024*1024||count*sizeof(EmissionExrPixel)>256*1024*1024-raster.working_bytes)
  throw std::length_error("HDR field extraction exceeds working budget");
 EmissionInterchangeImage image;
 image.width=image.height=raster.resolution;
 image.version=raster.version;
 image.pixels.resize(count);
 raster.read(0,image.pixels);
 image.validate();
 return image;
}

}
class EmissionExportProof final {
 struct Entry {uint64_t id;Material material;std::shared_ptr<const TextureMaps> textures;std::shared_ptr<const SourceMaterialBindings> binding;};
 std::shared_ptr<const Scene> scene;std::vector<Entry> entries;
 std::vector<std::pair<uint64_t,std::shared_ptr<const TextureSetSurfacePlan>>> texture_sets;
 EmissionExportProof()=default;
 friend EmissionExportPrepared prepare_emission_export(std::span<const ObjectSurface>,const NativeEmissionExport &,const std::function<bool()> &,bool);
 friend void validate_native_emission_surfaces(const EmissionExportPrepared &,const std::function<bool()> &);
 friend bool matches_texture_set_emission_surface(const EmissionExportPrepared &,const ObjectSurface &);
 friend bool matches_native_emission_surface(const EmissionExportPrepared &,const ObjectSurface &,const std::function<bool()> &);
 friend void finish_emission_export(GltfDocument &,EmissionExportPrepared &,const NativeEmissionExport &,size_t,const std::function<bool()> &);
};
bool matches_native_emission_surface(const EmissionExportPrepared &prepared,const ObjectSurface &surface,const std::function<bool()> &cancelled){
 stop(cancelled);const auto proof=prepared.proof;if(!proof||!proof->scene)return false;
 const auto entry=std::find_if(proof->entries.begin(),proof->entries.end(),[&](const auto &e){return e.id==surface.id;});if(entry==proof->entries.end())return false;
 auto same=[](const auto &a,const auto &b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);};const auto &owner=proof->scene->object(surface.id);if(!owner.painted||!owner.material_bindings||(!owner.material_bindings->native_paint_sampling&&!owner.material_bindings->canonical_paint_sampling))throw std::invalid_argument("Normalized native HDR lost its actual current Paint owner");const auto &a=surface.material,&b=entry->material;
 if(!same(surface.textures,entry->textures)||!same(surface.material_bindings,entry->binding)||!same(surface.material_bindings,owner.material_bindings)||a.name!=b.name||a.color!=b.color||a.emissive!=b.emissive||a.metallic!=b.metallic||a.roughness!=b.roughness||a.toon!=b.toon||a.double_sided!=b.double_sided||a.alpha!=b.alpha||a.alpha_cutoff!=b.alpha_cutoff||a.unlit!=b.unlit||surface.appearance||!same(surface.deformation,owner.deformation)||!same(surface.rig_source,owner.rig_source)||surface.polygon_colors||!same_mesh(surface.mesh,owner.painted->mesh())||surface.world_matrix().float_matrix()!=proof->scene->world_matrix(owner.id).float_matrix())throw std::invalid_argument("Normalized native HDR view differs from its exact prepared owner");
 const auto native=owner.material_bindings->native_paint_sampling;const auto canonical=owner.material_bindings->canonical_paint_sampling;
 if(bool(native)==bool(canonical))throw std::invalid_argument("Normalized HDR view needs one actual independent writable owner");
 PaintSamplingLimits limits;limits.cancelled=cancelled;
 if(canonical){canonical->validate_binding(*owner.material_bindings,true);canonical->validate_current(*owner.painted,limits);}else native->validate_current(*owner.painted,limits);
 const auto attachment=canonical?canonical->attachment():native->attachment();
 if(!same(surface.paint_sampling_attachment,attachment)||surface.paint_corner_coordinates!=attachment->domain()->corner_coordinates(surface.mesh,limits))throw std::invalid_argument("Normalized native HDR view changed its owning paint corners");return true;
}
bool matches_texture_set_emission_surface(const EmissionExportPrepared &prepared,const ObjectSurface &surface){
 const auto &proof=prepared.proof;
 if(!proof||!surface.texture_set_surface)return false;
 const auto found=std::find_if(proof->texture_sets.begin(),proof->texture_sets.end(),
     [&](const auto &entry){return entry.first==surface.id;});
 if(found==proof->texture_sets.end())return false;
 const auto &plan=surface.texture_set_surface;
 if(plan!=found->second||plan.owner_before(found->second)||found->second.owner_before(plan))
  throw std::invalid_argument("Texture-set HDR compilation borrowed another owning surface plan");
 const auto &object=proof->scene->object(surface.id);
 if(surface.world_matrix().float_matrix()!=proof->scene->world_matrix(object.id).float_matrix())
  throw std::invalid_argument("Texture-set HDR surface changed its owning world placement");
 return true;
}
void validate_native_emission_surfaces(const EmissionExportPrepared &prepared,const std::function<bool()> &cancelled){
 if(!prepared.proof)return;
 for(const auto &[object,plan]:prepared.proof->texture_sets){
  stop(cancelled);
  const auto member=std::find_if(prepared.surfaces.begin(),prepared.surfaces.end(),
      [&](const auto &surface){return surface.id==object;});
  if(member==prepared.surfaces.end()||!matches_texture_set_emission_surface(prepared,*member))
   throw std::invalid_argument("Texture-set HDR lost its exact compiled subject");
 }

 for(const auto &entry:prepared.proof->entries){stop(cancelled);const auto matches=[&](const auto &object){return object.id==entry.id;};
  if(std::count_if(prepared.surfaces.begin(),prepared.surfaces.end(),matches)!=1)throw std::invalid_argument("Native HDR prepared field lost its unique emitted subject");
  const auto surface=std::find_if(prepared.surfaces.begin(),prepared.surfaces.end(),matches);
  if(!matches_native_emission_surface(prepared,*surface,cancelled))throw std::invalid_argument("Native HDR field has no genuine prepared emission authority");
 }
}
static EmissionExportRaster canonical_paint_emission_raster(const SculptObject &object,const std::function<bool()> &cancelled,bool streaming){
 const auto binding=object.material_bindings;const auto owner=binding?binding->canonical_paint_sampling:nullptr;
 if(!object.mesh_active||!object.painted||!owner||!owner->valid_owner(owner)||!owner->matches_current(*object.painted)||
    binding->has_mtoon||binding->native_paint_sampling||binding->native_paint_frames||binding->normal_preparation||object.appearance||object.polygon)
  throw std::invalid_argument("Canonical HDR field needs its genuine independent current normal/paint owners");
 PaintSamplingLimits limits;limits.cancelled=cancelled;binding->validate();owner->validate_current(*object.painted,limits);
 canonical_paint_export_working_bytes(object,512*1024*1024,cancelled);
 // Raster coefficients are the actual retained writable grid. This does not
 // reinterpret the separate normal domain or resample either field into UV0.
 return raster(object,cancelled,false,true,streaming);
}
static EmissionExportRaster native_paint_emission_raster(const SculptObject &object,const std::function<bool()> &cancelled,bool streaming){
 const auto binding=object.material_bindings;const auto owner=binding?binding->native_paint_sampling:nullptr;
 const auto frame=binding?binding->native_paint_frames:nullptr;
 if(!object.mesh_active||!object.painted||!owner||!owner->valid_owner(owner)||binding->has_mtoon||binding->canonical_attributes||binding->canonical_uv||(!frame&&(binding->normal_frames||binding->key_derivation))||binding->normal_preparation||binding->color_preparation||object.appearance||object.polygon)
  throw std::invalid_argument("Native HDR field requires its own supported native paint owner");
 // A physical field is intrinsic to the same writable REST grid. Skinning
 // changes the emitted geometry, not these radiance/coverage coefficients.
 PaintSamplingLimits limits;limits.cancelled=cancelled;binding->validate();owner->validate_current(*object.painted,limits);
 if(frame){
  // A real captured current key frame supplies the normal/morph authority;
  // its older writable paint grid continues to own the radiance field. The
  // downstream normal/paint coordinate compiler refines both exact domains.
  frame->validate_binding(*binding,true,cancelled);
  native_paint_frame_export_working_bytes(object,512*1024*1024,cancelled);
 }else native_paint_export_working_bytes(object.painted->mesh(),object.deformation.get());
 return raster(object,cancelled,false,true,streaming);
}
static EmissionExportRaster owning_raster(const SculptObject &object,
    const std::function<bool()> &cancelled, bool authored_mtoon,bool streaming) {
 if(object.material_bindings&&object.material_bindings->canonical_paint_sampling)
  return canonical_paint_emission_raster(object,cancelled,streaming);
 if(object.material_bindings&&object.material_bindings->native_paint_sampling)
  return native_paint_emission_raster(object,cancelled,streaming);
 return raster(object,cancelled,authored_mtoon,false,streaming);
}
EmissionExportRaster export_emission_raster(const SculptObject &object,
    const std::function<bool()> &cancelled, bool authored_mtoon) {
 return owning_raster(object,cancelled,authored_mtoon,true);
}
EmissionInterchangeImage canonical_paint_emission_field(const SculptObject &object,
    const std::function<bool()> &cancelled) {
 return materialize(canonical_paint_emission_raster(object,cancelled,false));
}
EmissionInterchangeImage native_paint_emission_field(const SculptObject &object,
    const std::function<bool()> &cancelled) {
 return materialize(native_paint_emission_raster(object,cancelled,false));
}
EmissionInterchangeImage baked_emission_field(const SculptObject &object,const std::function<bool()> &cancelled,bool authored_mtoon){
 if(!object.mesh_active||!object.painted||!object.painted->baked()||!object.painted->baked()->emission_field)
  throw std::invalid_argument("Resolved FLOAT replay needs its actual current baked field owner");
 return materialize(owning_raster(object,cancelled,authored_mtoon,false));
}
std::shared_ptr<const TextureMaps> EmissionExportAccess::other_channels(const PaintedMesh &paint,Material material,const std::function<bool()> &cancelled){auto detached=without_emission(paint);material.emissive={};return detached->composite(material,cancelled);}
bool requires_native_emission_package(const Scene &scene,const std::function<bool()> &cancelled){
 stop(cancelled);SceneEmissionFeatureProbe feature(scene);const auto value=scene.encode();
 if(feature.float_emitted())return true;
 size_t visited=0;
 auto physical=[&](auto &&self,const nlohmann::json &node)->bool{if((++visited&4095)==0)stop(cancelled);if(node.is_object()){const auto direct=node.find("physical_emission");if(direct!=node.end()&&(direct->is_boolean()?direct->get<bool>():!direct->is_null()))return true;if(node.contains("physical_emission_tiles")&&!node.at("physical_emission_tiles").empty())return true;if(node.contains("emissive")){const auto &v=node.at("emissive");if(v.is_array()&&v.size()==3)for(const auto &n:v)if(n.is_number()&&n.get<double>()>1)return true;}if(node.contains("native_reference")){const auto &v=node.at("native_reference");if(v.is_object()&&v.contains("emission"))for(const auto &n:v.at("emission"))if(n.get<double>()>1)return true;}for(const auto &child:node.items())if(self(self,child.value()))return true;}else if(node.is_array())for(const auto &child:node)if(self(self,child))return true;return false;};const bool result=physical(physical,value);stop(cancelled);return result;
}
static void validate_texture_set_surface(const ObjectSurface &surface,const SculptObject &owner,
    const Scene &scene,const std::function<bool()> &cancelled){
 const auto &plan=surface.texture_set_surface;
 const auto same=[](const auto &a,const auto &b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);};
 if(!plan||!owner.painted||!owner.visible||plan->object()!=owner.id||
    !same(plan->collection(),owner.texture_sets)||!same(plan->geometry(),owner.painted->mesh_owner())||
    !same(plan->deformation(),owner.deformation)||!same(surface.deformation,owner.deformation)||
    !same(surface.rig_source,owner.rig_source)||
    surface.world_matrix().float_matrix()!=scene.world_matrix(owner.id).float_matrix())
  throw std::invalid_argument("Texture-set HDR surface differs from its immutable native source");
 plan->authenticate_owner(plan);
 const auto material_key=[](const Material &m){
  return std::tie(m.name,m.color,m.metallic,m.roughness,m.emissive,m.toon,m.double_sided,
      m.alpha,m.alpha_cutoff,m.unlit);
 };
 for(const auto &field:plan->fields()){
  stop(cancelled);
  const auto native=texture_set_view(owner,field.id);
  if(field.name!=native.name||material_key(field.material)!=material_key(native.material)||
     !same(field.domain,native.domain)||!same(field.source,native.source)||
     !same(field.appearance,native.appearance)||!same(field.material_bindings,native.material_bindings)||
     !same(field.bake_recipe,native.bake_recipe)||
     !TextureSetPaintAccess::same_current(*field.painted,*native.painted))
   throw std::invalid_argument("Texture-set HDR member differs from the current native scene");
 }
}
EmissionExportPrepared prepare_emission_export(std::span<const ObjectSurface> objects,const NativeEmissionExport &options,const std::function<bool()> &cancelled,bool authored_mtoon){
 stop(cancelled);if(!options.scene||!options.allow_measured_core_fallback)throw std::invalid_argument("HDR export needs its immutable Scene and explicit measured fallback permission");options.scene->validate();
 size_t field_pixels=0;
 bool texture_set_hdr=false;
 const auto charge_pixels=[&](uint32_t resolution){
  const auto required=size_t(resolution)*resolution;
  if(required>emission_stream_pixels-field_pixels)
   throw std::length_error("Aggregate HDR sample count exceeds streaming budget");
  field_pixels+=required;
 };
 for(const auto &surface:objects){
  stop(cancelled);
  const auto &owner=options.scene->object(surface.id);
  if(owner.texture_sets){
   validate_texture_set_surface(surface,owner,*options.scene,cancelled);
   for(const auto &field:surface.texture_set_surface->fields())
    if(!field.domain->triangles().empty()&&texture_set_physical_emission(field)){
     charge_pixels(field.painted->resolution());
     texture_set_hdr=true;
    }
   continue;
  }
  if(!object_has_hdr(owner))continue;
  if(!owner.mesh_active||!owner.painted)throw std::invalid_argument("HDR export requires its editable mesh owner");
  charge_pixels(owner.painted->resolution());
 }
 std::shared_ptr<EmissionExportProof> proof;
 EmissionExportPrepared result;result.authored_mtoon=authored_mtoon;
 result.streaming=texture_set_hdr||field_pixels>emission_dense_bytes/sizeof(EmissionExrPixel);
 result.surfaces.assign(objects.begin(),objects.end());std::set<uint64_t> identities;
 for(size_t i=0;i<objects.size();++i){stop(cancelled);const auto &surface=objects[i];const auto &owner=options.scene->object(surface.id);if(owner.texture_sets){
  if(!identities.insert(surface.id).second)throw std::invalid_argument("Repeated HDR export object");
  result.ids.push_back(surface.id);
  if(!proof){proof=std::shared_ptr<EmissionExportProof>(new EmissionExportProof);proof->scene=options.scene;}
  proof->texture_sets.emplace_back(surface.id,surface.texture_set_surface);
  for(const auto &field:surface.texture_set_surface->fields())
   if(!field.domain->triangles().empty()&&texture_set_physical_emission(field)){
    EmissionExportImage entry;
    entry.object=owner.id;
    entry.texture_set=field.id;
    entry.image.width=entry.image.height=field.painted->resolution();
    entry.image.version=2;
    result.images.push_back(std::move(entry));
   }
  continue;
 }
 const auto source_mesh=owner.mesh_active?Mesh{}:surface_mesh(*owner.volume,cancelled);
 const auto &expected=owner.mesh_active?owner.painted->mesh():source_mesh;
 if(!identities.insert(surface.id).second||!same_mesh(surface.mesh,expected)||
    surface.material.name!=owner.material.name||surface.material.toon!=owner.material.toon||
    surface.material.color!=owner.material.color||surface.material.emissive!=owner.material.emissive||
    surface.material.metallic!=owner.material.metallic||surface.material.roughness!=owner.material.roughness||
    surface.material.unlit!=owner.material.unlit||surface.material.alpha!=owner.material.alpha||
    surface.material.alpha_cutoff!=owner.material.alpha_cutoff||surface.material.double_sided!=owner.material.double_sided||
    surface.deformation!=owner.deformation||surface.appearance!=owner.appearance||
    surface.material_bindings!=owner.material_bindings||
    surface.world_matrix().float_matrix()!=options.scene->world_matrix(owner.id).float_matrix())
  throw std::invalid_argument("HDR export surface does not match its immutable native source");
 result.ids.push_back(surface.id);
 auto &safe=result.surfaces[i];
 if(!object_has_hdr(owner)){
  if(!authored_mtoon&&owner.mesh_active&&owner.painted->source_sampling_prepared()){
   // The immutable Scene owns the contributions even when two surfaces share source maps.
   safe.textures=source_baked_preview(owner,cancelled);
   if(!safe.textures)safe.textures=owner.painted->preview(owner.material,cancelled);
   source_paint_export(*owner.painted,safe,cancelled,true);
  }
  continue;
 }
 const bool native_domain=owner.material_bindings&&bool(owner.material_bindings->native_paint_sampling);const auto canonical=owner.material_bindings?owner.material_bindings->canonical_paint_sampling:nullptr;
 if(native_domain||canonical){if(surface.material_bindings.owner_before(owner.material_bindings)||owner.material_bindings.owner_before(surface.material_bindings))throw std::invalid_argument("Native HDR borrowed a foreign material control block");if(surface.deformation.owner_before(owner.deformation)||owner.deformation.owner_before(surface.deformation)||surface.rig_source!=owner.rig_source||surface.rig_source.owner_before(owner.rig_source)||owner.rig_source.owner_before(surface.rig_source))throw std::invalid_argument("Native HDR borrowed a foreign current model control block");const auto attachment=owner.painted->paint_sampling_attachment();if(attachment!=surface.paint_sampling_attachment||attachment.owner_before(surface.paint_sampling_attachment)||surface.paint_sampling_attachment.owner_before(attachment)||surface.paint_corner_coordinates!=attachment->domain()->corner_coordinates(owner.painted->mesh()))throw std::invalid_argument("Native HDR surface borrowed another paint coordinate owner");}
 EmissionInterchangeImage image;
 if(result.streaming){
  auto source=export_emission_raster(owner,cancelled,authored_mtoon);
  image.width=image.height=source.resolution;
  image.version=source.version;
 }else image=materialize(owning_raster(owner,cancelled,authored_mtoon,false));
 safe.material.emissive={};safe.textures=EmissionExportAccess::other_channels(*owner.painted,safe.material,cancelled);if(safe.material_bindings&&!native_domain&&!canonical){auto binding=std::make_shared<SourceMaterialBindings>(*safe.material_bindings);binding->physical_emission.reset();if(!binding->native_paint_sampling&&!binding->canonical_paint_sampling)binding->native_reference.emission={};safe.material_bindings=std::move(binding);}EmissionExportImage entry;entry.authored_mtoon=authored_mtoon;entry.object=surface.id;entry.native_paint_domain=native_domain;entry.canonical_paint_domain=canonical;entry.image=std::move(image);if(native_domain||canonical){if(!proof){proof=std::shared_ptr<EmissionExportProof>(new EmissionExportProof);proof->scene=options.scene;}proof->entries.push_back({safe.id,safe.material,safe.textures,safe.material_bindings});}result.images.push_back(std::move(entry));}
 if(proof)result.proof=std::move(proof);return result;
}
void finish_emission_export(GltfDocument &document,EmissionExportPrepared &prepared,const NativeEmissionExport &options,size_t max_bytes,const std::function<bool()> &cancelled){
 using Json=nlohmann::json;if(prepared.proof&&(prepared.proof->scene!=options.scene||prepared.proof->scene.owner_before(options.scene)||options.scene.owner_before(prepared.proof->scene)))throw std::invalid_argument("Native HDR finisher borrowed another owning Scene");auto &j=document.json;auto &binary=document.binary;
 const bool native_domains=std::any_of(prepared.images.begin(),prepared.images.end(),[](const auto &entry){return entry.native_paint_domain||bool(entry.canonical_paint_domain);});
 for(auto &entry:prepared.images){stop(cancelled);const auto found=std::find(prepared.ids.begin(),prepared.ids.end(),entry.object);if(found==prepared.ids.end())throw std::logic_error("Missing HDR export object");const auto i=size_t(found-prepared.ids.begin());auto &identity=document.objects.at(i);entry.node=identity.node;
  if(entry.texture_set){
   const auto &primitives=j.at("meshes").at(identity.mesh).at("primitives");
   const auto member=std::find_if(primitives.begin(),primitives.end(),[&](const auto &primitive){
    return primitive.at("extras").at("dome_sculpt_texture_set").at("id")==*entry.texture_set;
   });
   if(member==primitives.end())throw std::logic_error("Texture-set HDR lost its emitted material");
   entry.material=member->at("material").get<int>();
   const auto &material=j.at("materials").at(entry.material);
   const auto &receipt=material.at("extras").at("domeSculptPortableEmission");
   entry.texcoord=material.at("emissiveTexture").value("texCoord",0u);
   const auto extensions=material.find("extensions");
   entry.authored_mtoon=extensions!=material.end()&&extensions->contains("VRMC_materials_mtoon");
   if(extensions!=material.end()&&extensions->contains("KHR_materials_emissive_strength"))
    entry.fallback_strength=extensions->at("KHR_materials_emissive_strength").at("emissiveStrength").get<float>();
   entry.fallback_factor=material.at("emissiveFactor").get<std::array<float,3>>();
   entry.maximum_absolute_error=receipt.at("maximumAbsoluteError").get<double>();
   entry.maximum_relative_error=receipt.at("maximumRelativeError").get<double>();
   entry.erased_nonzero_samples=receipt.at("erasedNonzeroSamples").get<size_t>();
   continue;
  }
  if(native_domains){
   if(identity.mesh<0||identity.node<0||j.at("nodes").at(identity.node).at("mesh")!=identity.mesh)throw std::logic_error("Native HDR lost its actual emitted object mapping");
   size_t users=0;for(const auto &node:j.at("nodes"))if(node.contains("mesh")&&node.at("mesh")==identity.mesh)++users;
   if(users>1){if(j.at("meshes").size()>=65536)throw std::length_error("Native HDR mesh table exceeds policy");auto copy=j.at("meshes").at(identity.mesh);identity.mesh=int(j.at("meshes").size());j["meshes"].push_back(std::move(copy));j["nodes"][identity.node]["mesh"]=identity.mesh;}
   auto &primitives=j["meshes"][identity.mesh]["primitives"];if(primitives.size()!=1)throw std::invalid_argument("Native HDR requires its unique emitted primitive");
   const auto old=primitives[0].at("material");users=0;for(const auto &mesh:j.at("meshes"))for(const auto &primitive:mesh.at("primitives"))if(primitive.contains("material")&&primitive.at("material")==old)++users;
   if(users>1){if(j.at("materials").size()>=65536)throw std::length_error("Native HDR material table exceeds policy");auto copy=j.at("materials").at(old.get<size_t>());identity.material=int(j.at("materials").size());j["materials"].push_back(std::move(copy));primitives[0]["material"]=identity.material;}
  }
  entry.material=j.at("meshes").at(identity.mesh).at("primitives").at(0).at("material").get<int>();auto &material=j.at("materials").at(entry.material);
  if(entry.native_paint_domain||entry.canonical_paint_domain){const auto &info=material.at("emissiveTexture");entry.texcoord=info.value("texCoord",0u);if(entry.texcoord>7||info.contains("extensions"))throw std::invalid_argument("Native HDR needs its genuine untransformed emitted paint coordinate");}
  const size_t total=size_t(entry.image.width)*entry.image.height;
  std::optional<EmissionExportRaster> source;
  std::vector<EmissionExrPixel> rows;
  if(prepared.streaming){
   source=export_emission_raster(options.scene->object(entry.object),cancelled,entry.authored_mtoon);
   if(entry.image.width!=source->resolution||entry.image.height!=source->resolution||entry.image.version!=source->version)
    throw std::invalid_argument("HDR raster changed after owning preparation");
   constexpr size_t fallback_bytes=256u*1024*1024;
   const auto scratch=size_t(source->resolution)*16*sizeof(EmissionExrPixel);
   const auto png_forecast=total*4+total/8+1024*1024;
   if(source->working_bytes>fallback_bytes||scratch+total*4+png_forecast>fallback_bytes-source->working_bytes)
    throw std::length_error("HDR portable fallback exceeds working budget");
   rows.resize(size_t(source->resolution)*16);
  }
  const auto visit=[&](const auto &pixels){
   if(!source){pixels(size_t(0),std::span<const EmissionExrPixel>(entry.image.pixels));return;}
   for(uint32_t y=0;y<entry.image.height;y+=16){
    stop(cancelled);
    const auto count=size_t(entry.image.width)*std::min(16u,entry.image.height-y);
    auto samples=std::span(rows).first(count);
    source->read(y,samples);
    pixels(size_t(y)*entry.image.width,std::span<const EmissionExrPixel>(samples));
   }
  };
  std::array<float,3> peak{};
  visit([&](size_t,std::span<const EmissionExrPixel> pixels){
   for(const auto &pixel:pixels)for(size_t k=0;k<3;++k)peak[k]=std::max(peak[k],pixel[k]);
  });
  const float normalizer=std::max({1.f,peak[0],peak[1],peak[2]});const bool vrm0=prepared.authored_mtoon&&j.contains("extensions")&&j.at("extensions").contains("VRM");const bool unlit=material.contains("extensions")&&material.at("extensions").contains("KHR_materials_unlit");const float strength=unlit?1.f:normalizer;const float typed_strength=vrm0?normalizer:strength;entry.fallback_strength=strength;for(size_t k=0;k<3;++k)entry.fallback_factor[k]=peak[k]/normalizer;Image image;image.width=entry.image.width;image.height=entry.image.height;image.rgba.resize(total*4);
  ResolvedFallbackMetrics measured;
  visit([&](size_t offset,std::span<const EmissionExrPixel> pixels){
   for(size_t i=0;i<pixels.size();++i){const auto p=offset+i;if((p&1023)==0)stop(cancelled);for(size_t k=0;k<3;++k){
    const float actual=pixels[i][k];const auto q=uint8_t(std::clamp(std::lround(srgb(peak[k]>0?actual/peak[k]:0)*255),0l,255l));image.rgba[p*4+k]=q;
    if(entry.image.version==2){
      measured.sample(actual,resolved_fallback_radiance(q,entry.fallback_factor[k],strength,unlit));
    }else{
      // Exact original F32 calculation/wire for legacy interpretation1.
      const float restored=legacy_fallback_radiance(q,entry.fallback_factor[k],strength,unlit);
      const float error=std::abs(actual-restored);
      entry.maximum_absolute_error=std::max(entry.maximum_absolute_error,double(error));
      if(actual>0){entry.maximum_relative_error=std::max(entry.maximum_relative_error,double(error/actual));if(restored==0)++entry.erased_nonzero_samples;}
    }
  }image.rgba[p*4+3]=255;}
  });
  if(entry.image.version==2){entry.maximum_absolute_error=measured.maximum_absolute_error;entry.maximum_relative_error=measured.maximum_relative_error;entry.erased_nonzero_samples=measured.erased_nonzero_samples;}

  const auto png=image.png();size_t padding=(4-binary.size()%4)%4;if(binary.size()>max_bytes||padding>max_bytes-binary.size()||png.size()>max_bytes-binary.size()-padding)throw std::length_error("HDR fallback exceeds output budget");binary.insert(binary.end(),padding,0);const auto offset=binary.size();binary.insert(binary.end(),png.begin(),png.end());const auto view=j["bufferViews"].size();j["bufferViews"].push_back({{"buffer",0},{"byteOffset",offset},{"byteLength",png.size()}});const auto im=j["images"].size();j["images"].push_back({{"bufferView",view},{"mimeType","image/png"}});const auto sampler=j["samplers"].size();j["samplers"].push_back({{"wrapS",33071},{"wrapT",33071},{"minFilter",9729},{"magFilter",9729}});const auto texture=j["textures"].size();j["textures"].push_back({{"source",im},{"sampler",sampler}});Json info={{"index",texture}};if((entry.native_paint_domain||entry.canonical_paint_domain)&&entry.texcoord)info["texCoord"]=entry.texcoord;material["emissiveTexture"]=std::move(info);material["emissiveFactor"]=entry.fallback_factor;if(!unlit&&strength>1){material["extensions"]["KHR_materials_emissive_strength"]={{"emissiveStrength",strength}};auto &used=j["extensionsUsed"];if(used.is_null())used=Json::array();if(std::find(used.begin(),used.end(),"KHR_materials_emissive_strength")==used.end())used.push_back("KHR_materials_emissive_strength");}else if(material.contains("extensions"))material["extensions"].erase("KHR_materials_emissive_strength");
  if(vrm0){auto &properties=j.at("extensions").at("VRM").at("materialProperties").at(size_t(entry.material));auto previous=properties["vectorProperties"].value("_EmissionColor",Json::array({0,0,0,1}));const auto alpha=previous.is_array()&&previous.size()==4?previous[3]:Json(1);properties["vectorProperties"]["_EmissionColor"]={entry.fallback_factor[0]*typed_strength,entry.fallback_factor[1]*typed_strength,entry.fallback_factor[2]*typed_strength,alpha};properties["textureProperties"]["_EmissionMap"]=texture;}
 }
 attach_native_emission(document,options,prepared.ids,prepared.images,max_bytes,cancelled,prepared.streaming);
 const uint64_t complete=28+((uint64_t(document.json.dump().size())+3)&~uint64_t(3))+((uint64_t(document.binary.size())+3)&~uint64_t(3));if(complete>max_bytes)throw std::length_error("Complete HDR GLB/VRM container exceeds output budget");
}
}
namespace dome {
std::vector<ObjectSurface> emission_export_surfaces(const Scene &scene,const std::function<bool()> &cancelled){
 auto detached=scene.snapshot();
 for(const auto &original:scene.objects()){
  if(cancelled&&cancelled())throw std::runtime_error("HDR export surface extraction cancelled");
  if(!original.visible||original.texture_sets)continue;
  if(!detail::object_has_hdr(original))continue;
  auto &object=detached.object(original.id);
  if(object.painted&&object.painted->has_physical_emission())
   object.painted=detail::EmissionExportAccess::without_emission(*object.painted);
  object.material.emissive={};
  if(object.material_bindings){
   auto binding=std::make_shared<SourceMaterialBindings>(*object.material_bindings);
   binding->physical_emission.reset();
   if(!binding->native_paint_sampling&&!binding->canonical_paint_sampling)binding->native_reference.emission={};
   object.material_bindings=std::move(binding);
  }
 }
 auto surfaces=detached.preview_surface_bundles(cancelled,{});
 for(auto &surface:surfaces){
  const auto &original=scene.object(surface.id);
  if(original.texture_sets)continue;
  surface.material=original.material;
  surface.material_bindings=original.material_bindings;
  surface.retopo_mtoon_export=detail::RetopoMToonExport::capture(original,cancelled);
  if(original.painted&&detail::object_has_hdr(original)){
   auto maps=std::make_shared<TextureMaps>(*surface.textures);
   maps->overlay=detail::PaintOverlayFactory::capture(*original.painted,cancelled);
   surface.textures=std::move(maps);
  }
 }
 return surfaces;
}
}
