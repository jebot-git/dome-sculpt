// SPDX-License-Identifier: GPL-3.0-or-later
#include "texture-set-surface.hpp"
#include "texture-set-paint-access.hpp"
#include "dome/polygon-scene.hpp"
#include <bit>
namespace dome::detail {
namespace {
void stop(const TextureSetLimits&l){if(l.cancelled&&l.cancelled())throw std::runtime_error("Texture-set surface preparation cancelled");}
void charge(size_t&total,size_t n,size_t unit,const TextureSetLimits&l){if(total>l.maximum_bytes||(unit&&n>(l.maximum_bytes-total)/unit))throw std::length_error("Texture-set surface staging exceeds its byte budget");total+=n*unit;}
void work(size_t&total,size_t n,const TextureSetLimits&l){if(n>l.maximum_work-total)throw std::length_error("Texture-set surface exceeds its work budget");total+=n;}
void bounded_product(size_t&total,size_t n,size_t unit,const TextureSetLimits&l){if(unit&&n>(l.maximum_work-total)/unit)throw std::length_error("Texture-set topology replay exceeds its work budget");total+=n*unit;}
void plain(const PaintedMesh&p,const CreatorAppearance*appearance,const SourceMaterialBindings*binding,const ObjectBakeRecipe*recipe){
 if(appearance||binding||recipe||p.source_maps()||p.color_projection()||p.normal_projection()||p.paint_sampling_domain()||p.source_sampling_prepared()||
    (p.baked()&&(p.baked()->normal_field||p.baked()->normal_authority)))
  throw std::invalid_argument("Texture-set complex frames/sampling need their complete corner-attribute surface adapter");
}
bool bits(float a,float b){return std::bit_cast<uint32_t>(a)==std::bit_cast<uint32_t>(b);}
bool bits(Vec3 a,Vec3 b){return bits(a.x,b.x)&&bits(a.y,b.y)&&bits(a.z,b.z);}
}
std::shared_ptr<const TextureSetSurfacePlan> prepare_texture_set_surface(const SculptObject&o,const TextureSetLimits&limits){
 limits.validate();stop(limits);
 if(!o.mesh_active||!o.painted||!o.texture_sets||o.painted->in_stroke())throw std::invalid_argument("Texture-set surface needs its actual finished current collection");
 const auto&m=o.painted->mesh();const auto corners=m.indices.size(),faces=corners/3;
 if(corners>limits.maximum_corners||faces>limits.maximum_triangles)throw std::length_error("Texture-set surface geometry exceeds its declared bounds");
 plain(*o.painted,o.appearance.get(),o.material_bindings.get(),o.bake_recipe.get());
 for(const auto&field:o.texture_sets->additional()){
  if(!field.painted)throw std::invalid_argument("Texture-set surface field has no actual paint owner");
  plain(*field.painted,field.appearance.get(),field.material_bindings.get(),field.bake_recipe.get());
 }
 size_t forecast=0;
 if(o.polygon&&o.polygon->active)for(const auto&face:o.polygon->topology.faces()){
  stop(limits);bounded_product(forecast,face.corners.size(),8*face.corners.size()+32,limits);
 }
 // Charge tree comparisons conservatively before the existing exact ID replay.
 // No topology/skin/normal field is replaced by this arithmetic forecast.
 bounded_product(forecast,faces,4*std::bit_width(faces)+16,limits);
 bounded_product(forecast,corners,2*std::bit_width(corners)+8,limits);
 bounded_product(forecast,m.positions.size(),8,limits);work(forecast,corners,limits);
 size_t bytes=2*sizeof(TextureSetSurfacePlan)+128;charge(bytes,corners,2*sizeof(uint32_t),limits);
 charge(bytes,o.texture_sets->additional().size()+1,sizeof(TextureSetView)+sizeof(TextureSetDraw),limits);
 charge(bytes,o.texture_sets->additional().size()+1,514,limits); // Two bounded names per captured field.
 charge(bytes,1,TextureSetPaintAccess::working_bytes(*o.painted,o.painted->resolution()),limits);
 size_t validation_scratch=0;
 // Exact raw-ID uniqueness and face-slot replay use bounded tree nodes. This
 // scratch is released before packet arrays are allocated; budget the peak.
 charge(validation_scratch,faces+corners,6*sizeof(void*)+sizeof(uint64_t),limits);
 charge(validation_scratch,faces,1,limits);
 (void)bytes;(void)validation_scratch;
 // IDs, domains and source/material fields are authenticated before allocating
 // the packet. The fields below retain COW pixels rather than cloning them.
 o.texture_sets->validate(o,true,limits);
 auto plan=std::shared_ptr<TextureSetSurfacePlan>(new TextureSetSurfacePlan);
 plan->object_=o.id;plan->geometry_=o.painted->mesh_owner();plan->deformation_=o.deformation;plan->collection_=o.texture_sets;
 plan->fields_.reserve(o.texture_sets->additional().size()+1);plan->draws_.reserve(o.texture_sets->additional().size()+1);
 plan->indices_.reserve(corners);plan->corners_.reserve(corners);
 auto append=[&](TextureSetId id){
  stop(limits);auto field=texture_set_view(o,id);
  if(!field.domain||!field.domain->matches(plan->geometry_)||field.domain->set()!=id)throw std::invalid_argument("Texture-set surface field lost its exact domain");
  if(!id)field.painted=field.painted->snapshot();
  const auto first=plan->indices_.size();
  for(auto triangle:field.domain->triangles()){
   if((plan->indices_.size()&1023)==0)stop(limits);
   for(uint32_t c=0;c<3;++c){const auto logical=size_t(triangle)*3+c;if(logical>=corners)throw std::invalid_argument("Texture-set surface refers to a foreign logical corner");plan->indices_.push_back(m.indices[logical]);plan->corners_.push_back(uint32_t(logical));}
  }
  plan->draws_.push_back({id,first,plan->indices_.size()-first});plan->fields_.push_back(std::move(field));
 };
 append(0);for(const auto&field:o.texture_sets->additional())append(field.id);
 if(plan->indices_.size()!=corners)throw std::invalid_argument("Texture-set surface omitted actual current triangles");
 stop(limits);
 std::shared_ptr<const TextureSetSurfacePlan>sealed(new const TextureSetSurfacePlan(std::move(*plan)));
 sealed->self_=sealed;return sealed;
}
void TextureSetSurfacePlan::authenticate_owner(const std::shared_ptr<const TextureSetSurfacePlan>&owner)const{
 const auto expected=self_.lock();
 if(!expected||owner!=expected||owner.owner_before(expected)||expected.owner_before(owner))throw std::invalid_argument("Texture-set surface plan borrowed a foreign owner control block");
}
void TextureSetSurfacePlan::authenticate_rest(const Mesh&mesh,
    const std::shared_ptr<const DeformationState>&deformation,const TextureSetLimits&limits)const{
 limits.validate();stop(limits);
 if(!geometry_||!collection_||!collection_->topology()||geometry_!=collection_->topology()->geometry()||geometry_.owner_before(collection_->topology()->geometry())||collection_->topology()->geometry().owner_before(geometry_)||
    deformation_!=deformation||deformation_.owner_before(deformation)||deformation.owner_before(deformation_))
  throw std::invalid_argument("Texture-set surface lost its actual geometry/deformation owners");
 const auto &expected=*geometry_;
 size_t forecast=0;bounded_product(forecast,mesh.positions.size()+mesh.normals.size()+mesh.uv.size(),4,limits);work(forecast,mesh.indices.size(),limits);
 if(mesh.positions.size()!=expected.positions.size()||mesh.normals.size()!=expected.normals.size()||mesh.uv.size()!=expected.uv.size()||mesh.indices!=expected.indices||mesh.uv_pins!=expected.uv_pins||mesh.uv_seams!=expected.uv_seams)
  throw std::invalid_argument("Texture-set surface current REST topology/UV flags differ");
 for(size_t v=0;v<mesh.positions.size();++v){if((v&1023)==0)stop(limits);if(!bits(mesh.positions[v],expected.positions[v]))throw std::invalid_argument("Texture-set surface REST positions differ");}
 for(size_t v=0;v<mesh.normals.size();++v)if(!bits(mesh.normals[v],expected.normals[v]))throw std::invalid_argument("Texture-set surface authored REST normals differ");
 for(size_t v=0;v<mesh.uv.size();++v)if(!bits(mesh.uv[v][0],expected.uv[v][0])||!bits(mesh.uv[v][1],expected.uv[v][1]))throw std::invalid_argument("Texture-set surface authored UV0 differs");
 stop(limits);
}
} // namespace dome::detail
