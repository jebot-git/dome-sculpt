// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/deformation.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/material-bindings.hpp"
#include "dome/color-preparation.hpp"
#include "dome/polygon-scene.hpp"
#include <set>
#include <nlohmann/json.hpp>
namespace dome { struct PolygonAuthority; }
namespace dome::detail {
// Scene12 is the first format owning complete polygon source histories. Its
// bounded graph may retain all older frame/color/bind/source receipt features.
inline unsigned minimum_scene_version(const PolygonAuthority *p);
inline unsigned minimum_scene_version(const PolygonOrigin *p);
inline unsigned minimum_scene_version(const DeformationState *d) {
  if (!d || !d->skeleton)
    return 0;
  const auto &s = *d->skeleton;
  return s.inverse_bind_override || s.space_matrix ||
                 std::any_of(s.bones.begin(), s.bones.end(),
                             [](const auto &b) { return bool(b.rest_matrix); })
             ? 11u
             : 0u;
}
inline unsigned minimum_scene_version(const CreatorAppearance *a) {return a && a->source_receipt ? 11u : 0u;}
inline unsigned minimum_scene_version(const SourceMaterialBindings *binding) {
  if (!binding)
    return 0;
  unsigned result=binding->uv_origin
             ? std::max(minimum_scene_version(binding->uv_origin->deformation.get()),minimum_scene_version(binding->uv_origin->appearance.get()))
             : 0u;
  if(const auto &preparation=binding->color_preparation)
    result=std::max({result,10u,minimum_scene_version(preparation->original_deformation.get()),
      minimum_scene_version(preparation->original_appearance.get()),minimum_scene_version(preparation->original_material.get())});
  return result;
}
inline unsigned source_feature_version(const PolygonAuthority *root,const PolygonOrigin *source) {
  unsigned result=0;std::set<const PolygonAuthority*> active_a,done_a;std::set<const PolygonOrigin*> active_o,done_o;
  std::function<void(const PolygonAuthority*,size_t)> authority;std::function<void(const PolygonOrigin*,size_t)> origin;
  authority=[&](auto a,size_t depth){if(!a)return;
    if(depth>32||active_a.contains(a)||done_a.size()+done_o.size()>512)throw std::invalid_argument("Invalid retained object feature-version graph");
    if(done_a.contains(a))return;
    active_a.insert(a);result=std::max(result,12u);
    origin(a->origin.get(),depth);active_a.erase(a);done_a.insert(a);
  };
  origin=[&](auto o,size_t depth){if(!o)return;
    if(depth>32||active_o.contains(o)||done_a.size()+done_o.size()>512)throw std::invalid_argument("Invalid retained object feature-version graph");
    if(done_o.contains(o))return;
    active_o.insert(o);
    result=std::max({result,o->placement?13u:0u,o->shape_source?16u:0u,minimum_scene_version(o->deformation.get()),minimum_scene_version(o->appearance.get()),minimum_scene_version(o->material_bindings.get())});
    authority(o->polygon.get(),depth+1);origin(o->shape_source.get(),depth+1);active_o.erase(o);done_o.insert(o);
  };
  if(root)authority(root,1);else if(source){result=16;origin(source,1);}return result;
}
inline unsigned minimum_scene_version(const PolygonAuthority *p){return source_feature_version(p,nullptr);}
inline unsigned minimum_scene_version(const PolygonOrigin *p){return source_feature_version(nullptr,p);}
inline unsigned minimum_deformation_scene_version(const nlohmann::json &d) {
  if (d.is_null() || d.at("skeleton").is_null())
    return 0;
  const auto &s = d.at("skeleton");
  return s.contains("inverse_bind_override") ||
                 (s.contains("version") && s.at("version") == 2)
             ? 11u
             : 0u;
}
} // namespace dome::detail
