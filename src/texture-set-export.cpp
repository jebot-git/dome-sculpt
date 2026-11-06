// SPDX-License-Identifier: GPL-3.0-or-later
#include "texture-set-export.hpp"
#include "texture-set-paint-access.hpp"
#include "normal-frame-runtime.hpp"
#include "emission-authority.hpp"
#include <bit>
#include <map>
namespace dome::detail {
namespace {
void stop(const TextureSetLimits&l){if(l.cancelled&&l.cancelled())throw std::runtime_error("Texture-set export cancelled");}
void charge(size_t&v,size_t n,size_t u,size_t cap){if(v>cap||(u&&n>(cap-v)/u))throw std::length_error("Texture-set export exceeds its working budget");v+=n*u;}
template<class T>bool owner(const std::shared_ptr<const T>&a,const std::shared_ptr<const T>&b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);}
bool material(const Material&a,const Material&b){return a.name==b.name&&a.color==b.color&&a.metallic==b.metallic&&a.roughness==b.roughness&&a.emissive==b.emissive&&a.toon==b.toon&&a.double_sided==b.double_sided&&a.alpha==b.alpha&&a.alpha_cutoff==b.alpha_cutoff&&a.unlit==b.unlit;}
}
size_t texture_set_export_working_bound(const ObjectSurface&input,const TextureSetLimits&limits){
 limits.validate();stop(limits);const auto&plan=input.texture_set_surface;
 if(!plan)throw std::invalid_argument("Texture-set export requires its owning surface plan");plan->authenticate_owner(plan);
 if(input.id!=plan->object()||!owner(input.deformation,plan->deformation())||plan->fields().empty()||!material(input.material,plan->fields()[0].material)||
    !input.mesh.positions.empty()||!input.mesh.normals.empty()||!input.mesh.indices.empty()||!input.mesh.uv.empty()||!input.mesh.uv_pins.empty()||!input.mesh.uv_seams.empty()||input.textures||input.appearance||input.material_bindings||input.source_dormant||input.polygon_colors||input.paint_sampling_attachment||!input.paint_corner_coordinates.empty()||input.normal_preview_proof)
  throw std::invalid_argument("Texture-set export bundle differs from its genuine geometry/material/pose owner");
 const auto&rest=*plan->geometry();plan->authenticate_rest(rest,input.deformation,limits);rest.validate();
 size_t bytes=sizeof(TextureSetExport)+plan->preparation_bytes(),work=plan->preparation_work();const auto count=plan->indices().size();
 charge(bytes,count,2*sizeof(Vec3)+sizeof(std::array<float,2>)+sizeof(Tangent)+3*sizeof(uint32_t)+1,limits.maximum_bytes);
 charge(bytes,count,10*sizeof(void*)+6*sizeof(uint32_t),limits.maximum_bytes);
 charge(work,count,64,limits.maximum_work);size_t scratch=0;
 for(size_t f=0;f<plan->fields().size();++f){
  stop(limits);const auto&field=plan->fields()[f];const auto&draw=plan->draws()[f];
  if(!draw.count)continue;
  if(hdr_emission(field.material.emissive)||field.painted->has_physical_emission())throw std::invalid_argument("Texture-set HDR emission needs its per-material precision-qualified export compiler");
  const bool canonical=field.painted->baked()&&!field.painted->baked()->maps.normal.rgba.empty();(void)canonical;
  const auto r=field.painted->resolution();charge(bytes,size_t(r)*r,64,limits.maximum_bytes);
  scratch=std::max(scratch,TextureSetPaintAccess::working_bytes(*field.painted,r));
  charge(work,size_t(r)*r,16+8*field.painted->layers().size(),limits.maximum_work);
 }
 charge(bytes,scratch,1,limits.maximum_bytes);
 if(input.deformation){const auto entries=input.deformation->resource_entries();charge(bytes,entries,32,limits.maximum_bytes);charge(work,entries,16,limits.maximum_work);}
 stop(limits);return bytes;
}
TextureSetExport compile_texture_set_export(const ObjectSurface&input,const TextureSetLimits&limits){
 TextureSetExport out;out.working_bound=texture_set_export_working_bound(input,limits);
 const auto&plan=input.texture_set_surface;const auto&rest=*plan->geometry();const auto count=plan->indices().size();
 bool normal=false;for(size_t f=0;f<plan->fields().size();++f)if(plan->draws()[f].count){const auto&p=plan->fields()[f].painted;normal|=p->baked()&&!p->baked()->maps.normal.rgba.empty();}
 // Tangent basis is evaluated in REST, never the current pose. Include the
 // actual vertex and role-specific frame bits in every necessary split.
 const auto frames=normal?tangent_corners(rest,limits.cancelled):std::vector<Tangent>{};
 using Signature=std::array<uint32_t,5>;std::map<Signature,uint32_t>vertices;std::vector<uint32_t>rest_vertices;
 auto&mesh=out.surface.mesh;mesh.indices.reserve(count);rest_vertices.reserve(std::min(count,rest.positions.size()));
 for(size_t f=0;f<plan->fields().size();++f){
  stop(limits);const auto&field=plan->fields()[f];const auto&draw=plan->draws()[f];if(!draw.count)continue;
  const bool canonical=field.painted->baked()&&!field.painted->baked()->maps.normal.rgba.empty();
  TextureSetExportField output;output.set=field.id;output.first=mesh.indices.size();output.count=draw.count;output.material=field.material;output.textures=field.painted->composite(field.material,limits.cancelled);
  for(size_t c=0;c<draw.count;++c){
   if((c&1023)==0)stop(limits);const auto logical=plan->logical_corners()[draw.first+c];const auto vertex=rest.indices.at(logical);
   const Tangent tangent=canonical?frames.at(logical):Tangent{1,0,0,1};const Signature key{vertex,std::bit_cast<uint32_t>(tangent[0]),std::bit_cast<uint32_t>(tangent[1]),std::bit_cast<uint32_t>(tangent[2]),std::bit_cast<uint32_t>(tangent[3])};
   auto found=vertices.find(key);uint32_t id;
   if(found!=vertices.end())id=found->second;
   else{
    if(mesh.positions.size()>=2'000'000)throw std::length_error("Texture-set exported vertex splits exceed the native geometry limit");
    id=uint32_t(mesh.positions.size());vertices.emplace(key,id);rest_vertices.push_back(vertex);mesh.positions.push_back(rest.positions.at(vertex));mesh.normals.push_back(rest.normals.at(vertex));if(!rest.uv.empty())mesh.uv.push_back(rest.uv.at(vertex));if(!rest.uv_pins.empty())mesh.uv_pins.push_back(rest.uv_pins.at(vertex));out.tangents.push_back(tangent);
   }
   mesh.indices.push_back(id);if(!rest.uv_seams.empty())mesh.uv_seams.push_back(rest.uv_seams.at(logical));
  }
  out.fields.push_back(std::move(output));
 }
 if(out.fields.empty()||mesh.indices.size()!=count)throw std::invalid_argument("Texture-set export omitted a nonempty material range");
 mesh.validate();out.surface.id=input.id;out.surface.name=input.name;out.surface.transform=input.transform;out.surface.groups=input.groups;out.surface.affine_edit=input.affine_edit;out.surface.rig_source=input.rig_source;
 out.surface.material=out.fields[0].material;out.surface.textures=out.fields[0].textures;
 out.surface.deformation=normal_frame_remap_deformation(input.deformation,rest,mesh,rest_vertices,limits.cancelled);
 if(!normal)out.tangents.clear();stop(limits);return out;
}
} // namespace dome::detail
