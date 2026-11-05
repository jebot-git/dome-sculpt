// SPDX-License-Identifier: GPL-3.0-or-later
#include "canonical-paint-native.hpp"
#include "normal-key-derivation.hpp"
namespace dome::detail {
bool canonical_paint_retention_supported(const SculptObject &o) noexcept {
 const auto b=o.material_bindings;const auto p=o.painted;const auto c=b?b->canonical_attributes:nullptr;
 if(!o.mesh_active||!p||p->in_stroke()||!o.volume||o.volume->in_stroke()||o.appearance||o.polygon||!b||!c||!c->valid_owner(c)||
    b->has_mtoon||b->native_paint_sampling||b->native_paint_frames||b->color_preparation||b->normal_preparation||!b->normal_frames||
    p->source_maps()||p->source_sampling_prepared()||p->color_projection()||p->normal_projection()||p->surface_diagnostics().policy!=PaintUvPolicy::Strict)return false;
 if(b->key_derivation){if(!b->key_derivation->valid_owner(b->key_derivation)||b->key_derivation->frames()!=b->normal_frames)return false;}
 else if(b->normal_frames!=c->frames())return false;
 const auto retained=b->canonical_paint_sampling;
 return !retained?!p->paint_sampling_domain():retained->valid_owner(retained)&&retained->matches_current(*p)&&retained->normal_uv()==b->canonical_uv;
}
bool canonical_paint_source_restorable(const SculptObject &o) noexcept {
 return o.visible&&!o.locked&&canonical_paint_retention_supported(o)&&bool(o.material_bindings->canonical_paint_sampling);
}
void restore_canonical_paint(Scene &scene,uint64_t id,const std::function<bool()> &cancelled){
 auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Canonical paint source Restore cancelled");};stop();const auto &current=scene.object(id);
 if(!canonical_paint_source_restorable(current))throw std::invalid_argument("Canonical UV source Restore requires its actual supported current owners");
 auto owner=current.material_bindings->canonical_paint_sampling;owner->validate_binding(*current.material_bindings);PaintSamplingLimits limits;limits.cancelled=cancelled;owner->validate_current(*current.painted,limits);
 size_t depth=0;while(owner->before()->material_bindings->canonical_paint_sampling){stop();if(++depth>128)throw std::length_error("Canonical source Restore exceeds retained history bound");owner=owner->before()->material_bindings->canonical_paint_sampling;}
 const auto &source=*owner->before();auto next=scene;auto &target=next.object(id);
 target.volume=std::make_shared<Volume>(source.volume->geometry_snapshot());target.painted=source.paint->snapshot();target.material=source.material;target.mesh_active=source.mesh_active;
 target.material_bindings=source.material_bindings;target.deformation=source.deformation;target.appearance=source.appearance;target.polygon=source.polygon;
 target.retopology=source.retopology;target.shape_source=source.shape_source;target.rig_source=source.rig_source;target.bake_recipe=source.bake_recipe;
 // Match other explicit source Restore operations: current identity, placement,
 // group and user name remain current; the captured placement stays provenance.
 next.validate();stop();scene=std::move(next);
}
}
