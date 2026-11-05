// SPDX-License-Identifier: GPL-3.0-or-later
#include "canonical-paint-prospect.hpp"
#include "dome/material-bindings.hpp"
#include "dome/color-paint-sampling.hpp"
#include "dome/normal-sampling.hpp"
#include "normal-key-derivation.hpp"
#include "canonical-normal-owner.hpp"
#include "polygon-scene-archive.hpp"
#include "dome/bake-recipe.hpp"
#include "canonical-paint-history.hpp"
#include <set>
namespace dome::detail {
CanonicalPaintArchiveUsage preflight_canonical_paint_models(const Scene &scene,uint64_t selected,
 size_t future,const std::function<bool()> &cancelled){
 constexpr size_t maximum=8'000'000;CanonicalPaintArchiveUsage usage;
 auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Canonical paint model preflight cancelled");};
 auto add=[&](size_t n){if(n>maximum-usage.deformation_entries)throw std::length_error("Canonical paint prospective decoded model quota exceeded before allocation");usage.deformation_entries+=n;};
 auto deform=[&](const std::shared_ptr<const DeformationState> &p){
  stop();if(!p)return;++usage.deformation_slots;
  add(p->pose.size());add(p->morph_weights.size());
  for(const auto &pose:p->pose)if(pose.full_local_matrix)add(16);
  if(p->skin)for(size_t v=0;v<p->skin->vertices.size();++v){if((v&1023)==0)stop();add(p->skin->vertices[v].size());}
  if(p->morphs)for(const auto &target:p->morphs->targets)add(target.delta.size());
 };
 stop();const auto &source=scene.object(selected);SourceMaterialArchive materials;PolygonSceneArchive origins;
 for(const auto &o:scene.objects()){stop();if(o.id==selected){if(o.deformation)++usage.deformation_slots;add(future);}else deform(o.deformation);if(o.material_bindings)materials.add(o.material_bindings,false);if(o.polygon)origins.add(o.polygon,false);if(o.shape_source)origins.add_origin(o.shape_source,false);if(o.rig_source)origins.add_rig_origin(o.rig_source,false);if(o.bake_recipe)o.bake_recipe->validate();}
 // One new immutable before-origin is a real additional inline codec slot.
 // Its model is not merged with an equal/current/UV-origin model by value.
 deform(source.deformation);
 // Recipes own bounded settings/ID references only. Their before-models are
 // held by the genuine source/rig/polygon origins, collected exactly once by
 // the polygon pool. Do not invent another model slot for a recipe reference.
 collect_canonical_paint_controls(materials,origins,false,cancelled);
 for(const auto &p:origins.origins()){stop();deform(p->deformation);if(p->bake_recipe)p->bake_recipe->validate();}
 for(const auto &p:materials.origins()){stop();deform(p->deformation);}
 std::set<const SourceColorPreparation*> colors;std::set<const SourceNormalPreparation*> normals;
 std::set<const SourceKeyDerivation*> keys;
 std::set<const CanonicalNormalConversion*> layered_canonical;
 // Material add follows all supported before-binding edges with cycle/depth
 // guards. Distinct codec tables retain distinct inline models even if their
 // runtime immutable pointers currently alias.
 for(const auto &p:materials.bindings()){
  stop();if(p->color_preparation&&colors.insert(p->color_preparation.get()).second){const auto &c=*p->color_preparation;deform(c.original_deformation);if(c.uv_reprojection)deform(c.uv_reprojection->before_deformation);if(c.paint_sampling)deform(c.paint_sampling->before_deformation());}
  // Legacy canonical receipts reference their actual normal-preparation
  // anchor; they do not serialize a second inline deformation there.
  if(p->normal_preparation&&normals.insert(p->normal_preparation.get()).second){const auto &n=*p->normal_preparation;deform(n.original_deformation);if(n.uv_reprojection)deform(n.uv_reprojection->before_deformation);if(n.sampling_domain)deform(n.sampling_domain->before_deformation);}
  if(p->key_derivation&&keys.insert(p->key_derivation.get()).second)deform(p->key_derivation->before_deformation());
  if(p->canonical_attributes&&layered_canonical.insert(p->canonical_attributes.get()).second)deform(p->canonical_attributes->before_deformation());
 }
 stop();return usage;
}
size_t canonical_paint_model_entries(const DeformationState *input,const Mesh &source,
 const Mesh &target,std::span<const uint32_t> map,const std::function<bool()> &cancelled){
 auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Canonical paint model dimensions cancelled");};
 stop();if(map.size()!=target.positions.size()||map.size()>2'000'000||target.normals.size()!=map.size())
  throw std::invalid_argument("Canonical paint model remap dimensions differ");
 size_t count=0;constexpr size_t limit=8'000'000;
 auto add=[&](size_t n){if(n>limit-count)throw std::length_error("Canonical paint target model exceeds native quota before allocation");count+=n;};
 if(input){add(input->pose.size());add(input->morph_weights.size());for(const auto &p:input->pose)if(p.full_local_matrix)add(16);
  if(input->morphs){if(map.size()&&input->morphs->targets.size()>(limit-count)/map.size())throw std::length_error("Canonical paint target morph dimensions exceed native quota before allocation");add(map.size()*input->morphs->targets.size());}}
 for(size_t v=0;v<map.size();++v){if((v&1023)==0)stop();const auto old=map[v];
  if(old>=source.positions.size()||old>=source.normals.size())throw std::invalid_argument("Canonical paint remap is outside its genuine before geometry");
  const auto p=target.positions[v],q=source.positions[old],n=target.normals[v],m=source.normals[old];
  if(p.x!=q.x||p.y!=q.y||p.z!=q.z||n.x!=m.x||n.y!=m.y||n.z!=m.z)throw std::invalid_argument("Canonical paint remap changed authored POSITION/NORMAL");
  if(input&&input->skin){if(old>=input->skin->vertices.size())throw std::invalid_argument("Canonical paint skin correspondence is unavailable");add(input->skin->vertices[old].size());}}
 stop();return count;
}
}
