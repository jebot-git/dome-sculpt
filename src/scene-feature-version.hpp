// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/deformation.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/material-bindings.hpp"
#include "dome/color-preparation.hpp"
#include "dome/color-paint-sampling.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/polygon-scene.hpp"
#include "dome/source-maps.hpp"
#include "dome/source-placement.hpp"
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
inline unsigned minimum_scene_version(const ImportedAsset *a) {
  return a && std::any_of(a->primitives.begin(),a->primitives.end(),[](const auto &p){return !p.extra_uv_sets.empty();})?26u:0u;
}
inline unsigned minimum_scene_version(const CreatorAppearance *a) {return !a?0u:std::max(minimum_scene_version(a->asset.get()),a->material==0?17u:a->source_receipt?11u:0u);}
inline unsigned minimum_scene_version(const CreatorSourceModel *a) {return a?minimum_scene_version(a->asset.get()):0u;}
inline unsigned minimum_scene_version(const SourcePlacementCapsule *p) {
  unsigned version=p?13u:0u;
  if(p)for(const auto &g:p->groups)if(g.source_placement)version=std::max(version,minimum_scene_version(g.source_placement->model.get()));
  return version;
}
inline unsigned minimum_appearance_scene_version(const nlohmann::json &a){
  if(a.is_null())return 0;
  const auto &material=a.at("material");
  if(!material.is_number_integer()||(material.is_number_unsigned()?material.get<uint64_t>()>INT32_MAX:material.get<int64_t>()<0||material.get<int64_t>()>INT32_MAX))throw std::runtime_error("Invalid retained appearance material identity");
  return material.get<int64_t>()==0?17u:0u;
}
inline unsigned minimum_scene_version(const PaintedMesh *paint){return paint&&paint->source_maps()&&!paint->source_maps()->extra_uv_sets.empty()?26u:paint&&paint->paint_sampling_domain()?24u:paint&&paint->normal_projection()&&paint->normal_projection()->version==2?21u:paint&&paint->source_maps()&&paint->source_maps()->uv1?18u:0u;}

inline unsigned minimum_scene_version(const SourceMaterialBindings *binding) {
  if (!binding)
    return 0;
  unsigned result=binding->uv_origin
             ? std::max({minimum_scene_version(binding->uv_origin->paint.get()),minimum_scene_version(binding->uv_origin->deformation.get()),minimum_scene_version(binding->uv_origin->appearance.get())})
             : 0u;
  if(binding->normal_frames)for(const auto &frame:{binding->normal_frames->pbr,binding->normal_frames->mtoon,binding->normal_frames->canonical})if(frame&&frame->generated_uv&&*frame->generated_uv>=2)result=std::max(result,26u);
  if(const auto &preparation=binding->color_preparation)
    result=std::max({result,preparation->polygon_colors?20u:10u,minimum_scene_version(preparation->original_paint.get()),minimum_scene_version(preparation->original_deformation.get()),
      minimum_scene_version(preparation->original_appearance.get()),minimum_scene_version(preparation->original_material.get()),
      preparation->uv_reprojection?23u:0u,
      preparation->uv_reprojection?minimum_scene_version(preparation->uv_reprojection->before_material.get()):0u,
      preparation->uv_reprojection?minimum_scene_version(preparation->uv_reprojection->before_paint.get()):0u,
      preparation->uv_reprojection?minimum_scene_version(preparation->uv_reprojection->before_deformation.get()):0u,
      preparation->uv_reprojection?minimum_scene_version(preparation->uv_reprojection->before_appearance.get()):0u});
  if(binding->color_preparation&&binding->color_preparation->paint_sampling){
    const auto &u=*binding->color_preparation->paint_sampling;
    result=std::max({result,24u,minimum_scene_version(u.before_material().get()),minimum_scene_version(u.before_paint().get()),minimum_scene_version(u.before_deformation().get()),minimum_scene_version(u.before_appearance().get())});
  }
  if(const auto &preparation=binding->normal_preparation)
    result=std::max({result,preparation->uv_reprojection?19u:15u,minimum_scene_version(preparation->original_paint.get()),minimum_scene_version(preparation->original_deformation.get()),
      minimum_scene_version(preparation->original_appearance.get()),minimum_scene_version(preparation->original_material.get()),
      preparation->uv_reprojection?minimum_scene_version(preparation->uv_reprojection->before_material.get()):0u,
      preparation->uv_reprojection?minimum_scene_version(preparation->uv_reprojection->before_paint.get()):0u,
      preparation->uv_reprojection?minimum_scene_version(preparation->uv_reprojection->before_deformation.get()):0u,
      preparation->uv_reprojection?minimum_scene_version(preparation->uv_reprojection->before_appearance.get()):0u});
  if(binding->normal_preparation&&binding->normal_preparation->sampling_domain){
    const auto &u=*binding->normal_preparation->sampling_domain;
    result=std::max({result,u.metadata_sampling_domain?24u:21u,minimum_scene_version(u.before_material.get()),minimum_scene_version(u.sampling_material.get()),
      minimum_scene_version(u.before_paint.get()),minimum_scene_version(u.before_deformation.get()),minimum_scene_version(u.before_appearance.get())});
  }
  return result;
}
inline unsigned source_feature_version(const PolygonAuthority *root,const PolygonOrigin *source) {
  unsigned result=0;std::set<const PolygonAuthority*> active_a,done_a;std::set<const PolygonOrigin*> active_o,done_o;
  std::function<void(const PolygonAuthority*,size_t)> authority;std::function<void(const PolygonOrigin*,size_t)> origin;
  authority=[&](auto a,size_t depth){if(!a)return;
    if(depth>32||active_a.contains(a)||done_a.size()+done_o.size()>512)throw std::invalid_argument("Invalid retained object feature-version graph");
    if(done_a.contains(a))return;
    active_a.insert(a);result=std::max(result,a->colors?14u:12u);
    origin(a->origin.get(),depth);active_a.erase(a);done_a.insert(a);
  };
  origin=[&](auto o,size_t depth){if(!o)return;
    if(depth>32||active_o.contains(o)||done_a.size()+done_o.size()>512)throw std::invalid_argument("Invalid retained object feature-version graph");
    if(done_o.contains(o))return;
    active_o.insert(o);
    result=std::max({result,minimum_scene_version(o->placement.get()),o->shape_source?16u:0u,minimum_scene_version(o->paint.get()),minimum_scene_version(o->deformation.get()),minimum_scene_version(o->appearance.get()),minimum_scene_version(o->material_bindings.get())});
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
