// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native-weight-paint-access.hpp"
#include "native-weight-state.hpp"
#include "geometry-weight-authoring.hpp"
#include "dome/creator-archive.hpp"
#include "dome/material-bindings.hpp"
#include "dome/import-source.hpp"
#include "dome/source-placement.hpp"
#include "dome/bake-recipe.hpp"
#include <array>
namespace dome::detail {
struct NativeWeightVolumeAccess {
 static bool same_current(const Volume&a,const Volume&b)noexcept{
   using namespace native_weight_state;
   if(a.editing_||b.editing_||!scalar(a.voxel_,b.voxel_)||!scalar(a.band_,b.band_)||a.active_!=b.active_||a.next_layer_!=b.next_layer_||a.revision_!=b.revision_||!owner(a.cells_.owner(),b.cells_.owner())||!scalar(a.mask_.background,b.mask_.background)||!owner(a.mask_.values.owner(),b.mask_.values.owner())||a.layers_.size()!=b.layers_.size())return false;
   const auto&x=a.settings_;const auto&y=b.settings_;
   if(x.symmetry_axes!=y.symmetry_axes||!vector(x.symmetry_origin,y.symmetry_origin)||x.mask_edit!=y.mask_edit||x.mask_visible!=y.mask_visible||x.mask_tool!=y.mask_tool||!scalar(x.mask_strength,y.mask_strength))return false;
   for(size_t i=0;i<a.layers_.size();++i){const auto&u=a.layers_[i];const auto&v=b.layers_[i];if(u.id!=v.id||u.name!=v.name||u.visible!=v.visible||u.locked!=v.locked||!scalar(u.opacity,v.opacity)||!owner(u.delta.owner(),v.delta.owner()))return false;}
   return true;
 }
 static bool detached(const Volume&v){return !v.editing_&&v.undo_.empty()&&v.redo_.empty()&&v.before_.empty()&&!v.mask_start_&&!v.cells_start_&&!v.delta_start_;}
};
struct NativeWeightMaterialAccess {
 static auto json(const MToonMaterial&m){return std::array<const nlohmann::json*,2>{&m.source_,&m.initial_known_};}
};
// Prospective immutable REQUEST ownership, not allocator RSS, file decoding,
// saving, UI Undo history, or global render caches. Buffers use real capacities;
// identical object/control pairs are charged once; distinct aliases are
// conservatively visited separately. Overheads overestimate node allocations.
// Unsupported closure shapes refuse; they are never omitted from the sum.
struct NativeWeightRetention {
  size_t bytes=0, work=0, owners=0;
};
namespace native_weight_retention_impl {
struct Counter {
  struct Entry {std::shared_ptr<const void> owner;const void*address=nullptr;const void*type=nullptr;bool active=false;};
  template<class T>static const void*tag(){static const char t{};return &t;}
  std::array<Entry,4096> seen{};
  size_t used=sizeof(seen)+65536, maximum, maximum_work, work=0, count=0, active=0;
  explicit Counter(size_t max,size_t max_work=500'000'000):maximum(max),maximum_work(max_work) { if(!maximum_work||maximum_work>500'000'000)throw std::invalid_argument("Native ownership work bounds"); if(used>maximum)throw std::length_error("Native total admission cannot hold its ownership ledger"); }
  void step(size_t n=1){if(n>maximum_work-work)throw std::length_error("Native ownership scan work");work+=n;}
  void add(size_t n,size_t unit=1){step();if(unit&&n>(maximum-used)/unit)throw std::length_error("Native owning input exceeds total request budget before staging");used+=n*unit;}
  template<class T> void vector(const std::vector<T>&v){add(v.capacity(),sizeof(T));if(v.capacity())add(v.capacity()*sizeof(T)>=128u*1024u?4096:64);}
  void text(const std::string&s){add(s.capacity()+1);add(s.capacity()>=128u*1024u?4096:64);}
  template<class T> bool enter(const std::shared_ptr<T>&p){
    if(!p){std::shared_ptr<const void>empty;if(p.owner_before(empty)||empty.owner_before(p))throw std::invalid_argument("Native source has an owned null alias");return false;}
    using Value=std::remove_cv_t<T>;
    if(active==64)throw std::length_error("Native owning source depth cap");
    for(size_t i=0;i<count;++i){step();if(seen[i].address==p.get()&&seen[i].type==tag<Value>()&&!p.owner_before(seen[i].owner)&&!seen[i].owner.owner_before(p)){
      if(seen[i].active)throw std::invalid_argument("Native owning source graph has an active cycle");return false;}}
    if(count==seen.size())throw std::length_error("Native owning source node cap");
    add(64);seen[count++]={p,p.get(),tag<Value>(),true};++active;return true;
  }
  template<class T> void leave(const std::shared_ptr<T>&p){for(size_t i=count;i;--i)if(seen[i-1].address==p.get()&&seen[i-1].type==tag<std::remove_cv_t<T>>()&&!p.owner_before(seen[i-1].owner)&&!seen[i-1].owner.owner_before(p)){seen[i-1].active=false;--active;return;}throw std::logic_error("Native owning ledger leave");}
  void mesh(const Mesh&m){vector(m.positions);vector(m.normals);vector(m.uv);vector(m.indices);vector(m.uv_pins);vector(m.uv_seams);}
  template<class T> void shared_vector(const std::shared_ptr<const std::vector<T>>&p){if(enter(p)){add(sizeof(*p));vector(*p);leave(p);}}
  void image(const std::shared_ptr<const Image>&p){if(enter(p)){add(sizeof(*p));vector(p->rgba);leave(p);}}
  void maps(const std::shared_ptr<const PaintSourceMaps>&p){if(enter(p)){add(sizeof(*p));for(auto&t:{&p->color,&p->normal,&p->metal_rough,&p->emission,&p->occlusion})image(t->image);shared_vector(p->uv1);for(auto&[id,uv]:p->extra_uv_sets){(void)id;add(128);shared_vector(uv);}leave(p);}}
  void receipt(const std::shared_ptr<const ImportSourceReceipt>&p){if(enter(p)){add(sizeof(*p));text(p->adapter);text(p->root.name);shared_vector(p->root.bytes);vector(p->resources);for(auto&r:p->resources){text(r.name);shared_vector(r.bytes);}leave(p);}}
  void asset(const std::shared_ptr<const ImportedAsset>&p){if(!enter(p))return;
    if(p->nodes.size()>4096||p->primitives.size()>256||p->images.size()>256||p->materials.size()>4096)throw std::length_error("Native retained source dimensions");
    add(sizeof(*p));text(p->extras_json);vector(p->extensions_used);for(auto&s:p->extensions_used)text(s);
    vector(p->nodes);for(auto&n:p->nodes){text(n.name);text(n.extras_json);vector(n.children);vector(n.weights);}
    vector(p->skins);for(auto&s:p->skins){text(s.name);text(s.extras_json);vector(s.joints);vector(s.inverse_bind_matrices);}
    vector(p->meshes);for(auto&m:p->meshes){text(m.name);text(m.extras_json);vector(m.primitives);vector(m.weights);vector(m.target_names);for(auto&s:m.target_names)text(s);}
    vector(p->scenes);for(auto&s:p->scenes){text(s.name);text(s.extras_json);vector(s.roots);}
    vector(p->omitted_animations);for(auto&a:p->omitted_animations)text(a.name);
    vector(p->primitives);for(auto&v:p->primitives){text(v.name);text(v.extras_json);mesh(v.mesh);vector(v.influences);for(auto&i:v.influences){vector(i.joints);vector(i.weights);}vector(v.morph_targets);for(auto&t:v.morph_targets){vector(t.positions);vector(t.normals);vector(t.tangents);}vector(v.colors);vector(v.tangents);vector(v.uv1);for(auto&[set,uv]:v.extra_uv_sets){(void)set;add(128);vector(uv);}}
    vector(p->materials);for(auto&m:p->materials)text(m.name);
    vector(p->images);for(auto&i:p->images){text(i.name);text(i.mime_type);text(i.source_uri);vector(i.bytes);}
    vector(p->textures);vector(p->warnings);for(auto&s:p->warnings)text(s);leave(p);
  }
  void string(const std::shared_ptr<const std::string>&p){if(enter(p)){add(sizeof(*p));text(*p);leave(p);}}
  void images(const std::shared_ptr<const std::vector<std::shared_ptr<const Image>>>&p){if(enter(p)){add(sizeof(*p));vector(*p);for(auto&i:*p)image(i);leave(p);}}
  void appearance(const std::shared_ptr<const CreatorAppearance>&p){if(enter(p)){add(sizeof(*p));shared_vector(p->original_bytes);receipt(p->source_receipt);asset(p->asset);images(p->decoded_images);string(p->vrm_json);vector(p->source_vertices);vector(p->source_joint_nodes);vector(p->source_morph_targets);text(p->material_json);text(p->mtoon_json);leave(p);}}
  void model(const std::shared_ptr<const CreatorSourceModel>&p){if(enter(p)){add(sizeof(*p));shared_vector(p->original_bytes);receipt(p->source_receipt);asset(p->asset);images(p->decoded_images);string(p->vrm_json);leave(p);}}
  void deformation(const std::shared_ptr<const DeformationState>&p){if(!enter(p))return;
    if(p->pose.size()>256||p->morph_weights.size()>128||(p->skeleton&&p->skeleton->bones.size()>256)||(p->skin&&p->skin->vertices.size()>2'000'000)||(p->morphs&&p->morphs->targets.size()>128))throw std::length_error("Native retained deformation dimensions");add(sizeof(*p));vector(p->pose);vector(p->morph_weights);
    if(p->skeleton){vector(p->skeleton->bones);for(auto&b:p->skeleton->bones)text(b.name);if(p->skeleton->inverse_bind_override)vector(*p->skeleton->inverse_bind_override);}
    if(p->skin){vector(p->skin->vertices);for(auto&r:p->skin->vertices)vector(r);}
    if(p->morphs){vector(p->morphs->targets);for(auto&t:p->morphs->targets){text(t.name);vector(t.delta);}}
    if(p->humanoid)for(auto&[n,j]:p->humanoid->joints){(void)j;add(128);text(n);}
    if(p->landmarks)for(auto&[n,v]:p->landmarks->points){(void)v;add(128);text(n);}
    if(p->expressions)for(auto&[n,v]:p->expressions->visemes){add(128);text(n);vector(v);for(auto&b:v)text(b.shape);}leave(p);
  }
  void frames(const std::shared_ptr<const NormalFrames>&p){if(enter(p)){add(sizeof(*p));vector(p->origin_vertices);vector(p->source_vertices);vector(p->source_corners);vector(p->normals);vector(p->tangents);vector(p->targets);for(auto&t:p->targets){vector(t.normals);vector(t.tangents);}leave(p);}}
  template<class T> void unsupported(const std::shared_ptr<T>&p,const char*message){std::shared_ptr<const void>empty;if(p||p.owner_before(empty)||empty.owner_before(p))throw std::invalid_argument(message);}
  void frame_set(const std::shared_ptr<const NormalFrameSet>&p){if(enter(p)){unsupported(p->position_domain,"Native anatomical input has not yet admitted independent frame POSITION domains");add(sizeof(*p));frames(p->pbr);frames(p->mtoon);frames(p->canonical);vector(p->targets);for(auto&t:p->targets)vector(t.positions);vector(p->current_targets);leave(p);}}
  void json(const nlohmann::json&j,size_t depth=0){
    if(depth>64)throw std::length_error("Native material JSON depth");add(sizeof(j));
    if(j.is_object()){for(auto&i:j.items()){add(128);text(i.key());json(i.value(),depth+1);}}
    else if(j.is_array()){auto*a=j.get_ptr<const nlohmann::json::array_t*>();vector(*a);for(auto&v:*a)json(v,depth+1);}
    else if(j.is_string())text(j.get_ref<const std::string&>());
    else if(j.is_binary())add(j.get_binary().capacity()+64);
  }
  void material(const std::shared_ptr<const SourceMaterialBindings>&p){if(enter(p)){
    unsupported(p->color_preparation,"Native anatomical budget has not yet admitted prepared colour closures");unsupported(p->normal_preparation,"Native anatomical budget has not yet admitted prepared normal closures");unsupported(p->key_derivation,"Native anatomical budget has not yet admitted key derivation closures");
    add(sizeof(*p));for(auto*j:NativeWeightMaterialAccess::json(p->material))json(*j);vector(p->material.diagnostics);for(auto&s:p->material.diagnostics)text(s);for(auto&t:p->roles)image(t.image);frame_set(p->normal_frames);uv_origin(p->uv_origin);leave(p);
  }}
  void uv_origin(const std::shared_ptr<const SourceUvOrigin>&p){if(enter(p)){
    add(sizeof(*p));paint(p->paint);appearance(p->appearance);deformation(p->deformation);
    for(auto&t:p->roles)image(t.image);
    if(p->material){for(auto*j:NativeWeightMaterialAccess::json(*p->material))json(*j);vector(p->material->diagnostics);for(auto&s:p->material->diagnostics)text(s);}
    text(p->creator_kind);text(p->creator_parameters);leave(p);
  }}
  void paint(const std::shared_ptr<const PaintedMesh>&p,bool prospective=false){if(!enter(p))return;
    // Prospective current wrappers will have history/cache detached. Historical
    // paint owners must already have the same no-history immutable form.
    const auto&v=*p;
    const auto surface=NativeWeightPaintAccess::surface(v,prospective);
    if(enter(surface.owner)){add(surface.bytes);leave(surface.owner);}
    add(sizeof(PaintedMesh)+256);add(v.layers().capacity(),sizeof(PaintLayer));
    for(auto&l:v.layers()){text(l.name);vector(l.tiles);vector(l.pbr_tiles);vector(l.emission_tiles);
      for(auto&t:l.tiles)if(enter(t)){add(sizeof(*t));leave(t);}
      for(auto&t:l.pbr_tiles)if(enter(t)){add(sizeof(*t));leave(t);}
      for(auto&t:l.emission_tiles)if(enter(t)){add(sizeof(*t));leave(t);}}
    maps(v.source_maps());leave(p);
  }
  void volume(const std::shared_ptr<const Volume>&p,bool prospective=false){if(!enter(p))return;if(p->in_stroke()||(!prospective&&!NativeWeightVolumeAccess::detached(*p)))throw std::invalid_argument("Native retained volume must omit mutable history");add(sizeof(*p));
    auto cells=p->geometry_owner();if(enter(cells)){add(cells->ray_retained_bytes());leave(cells);}auto mask=p->mask_values().owner();if(enter(mask)){add(mask->ray_retained_bytes());leave(mask);}
    add(p->layers().capacity(),sizeof(Volume::Layer));for(auto&l:p->layers()){text(l.name);auto delta=l.delta.owner();if(enter(delta)){add(delta->ray_retained_bytes());leave(delta);}}
    // GeometryHistory holds at most64 events with WEAK storage links. Charge
    // its full construction ceiling, including weak control retention. No
    // unbounded sculpt Undo/Redo is carried by geometry_snapshot().
    add(64,sizeof(Volume::GeometryChange)+256);add(4096);
    leave(p);
  }
  void group(const SceneGroup&g){step(64);if(g.humanoid)g.humanoid->validate();text(g.name);if(enter(g.humanoid)){add(sizeof(*g.humanoid));text(g.humanoid->baseline);leave(g.humanoid);}if(enter(g.source_placement)){add(sizeof(*g.source_placement));model(g.source_placement->model);leave(g.source_placement);}if(enter(g.affine_edit)){add(sizeof(*g.affine_edit));leave(g.affine_edit);}}
  void placement(const std::shared_ptr<const SourcePlacementCapsule>&p){if(enter(p)){add(sizeof(*p));vector(p->groups);for(auto&g:p->groups)group(g);if(enter(p->object_edit)){add(sizeof(*p->object_edit));leave(p->object_edit);}leave(p);}}
  void bake_recipe(const std::shared_ptr<const ObjectBakeRecipe>&p){if(enter(p)){add(sizeof(*p));text(p->source.name);if(p->cage)text(p->cage->name);leave(p);}}
  void origin(const std::shared_ptr<const PolygonOrigin>&p){if(enter(p)){
    if(p->retopology||p->polygon)throw std::invalid_argument("Native anatomical total budget has not yet admitted retained editable topology");
    add(sizeof(*p));text(p->name);text(p->material.name);volume(p->volume);paint(p->paint);deformation(p->deformation);appearance(p->appearance);material(p->material_bindings);placement(p->placement);origin(p->shape_source);origin(p->rig_source);bake_recipe(p->bake_recipe);leave(p);
  }}
};
}
inline NativeWeightRetention native_weight_retention(const Scene&scene,size_t maximum,size_t maximum_work=500'000'000) {
  using namespace native_weight_retention_impl;
  Counter c(maximum,maximum_work);c.add(sizeof(Scene));c.add(scene.objects().capacity(),sizeof(SculptObject));c.add(scene.groups().capacity(),sizeof(SceneGroup));
  for(auto&g:scene.groups())c.group(g);
  for(auto&o:scene.objects()){
    if(o.retopology||o.polygon)throw std::invalid_argument("Native anatomical total budget has not yet admitted editable topology");
    c.text(o.name);c.text(o.material.name);c.volume(o.volume,true);c.paint(o.painted,true);c.deformation(o.deformation);c.appearance(o.appearance);c.material(o.material_bindings);c.origin(o.shape_source);c.origin(o.rig_source);c.bake_recipe(o.bake_recipe);if(c.enter(o.affine_edit)){c.add(sizeof(*o.affine_edit));c.leave(o.affine_edit);}
  }
  return {c.used,c.work,c.count};
}
}
