// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "emission-source-closure.hpp"
#include "canonical-normal-owner.hpp"
#include "native-paint-frame-source.hpp"
#include "native-paint-sampling.hpp"
#include "normal-key-derivation.hpp"
#include "dome/color-paint-sampling.hpp"
#include "dome/normal-sampling.hpp"
#include "dome/creator-archive.hpp"
#include "dome/source-placement.hpp"
namespace dome::detail {
// An identity ledger of the actual native payloads retained by one context.
// Current fields, detached before-origins and texture-field source/read views
// all count. This is independent of their sometimes very compact wire encoding.
struct EmissionSourcePayloads {
 std::vector<std::shared_ptr<const PaintedMesh>> paints;
 std::vector<std::shared_ptr<const Volume>> volumes;
 std::vector<std::shared_ptr<const Image>> images;
};
inline EmissionSourcePayloads emission_source_payloads(const Scene &scene,
 const std::function<bool()> &cancelled={}) {
 EmissionSourcePayloads result;SourceMaterialArchive materials;PolygonSceneArchive polygons;
 std::set<const PaintedMesh*> paints;std::set<const Volume*> volumes;std::set<const Image*> images;
 std::set<const TextureSetCollection*> sets;std::set<const TextureSetFieldSource*> fields;
 auto stop=[&]{NativeReadScope::check();if(cancelled&&cancelled())throw std::runtime_error("HDR retained-payload collection cancelled");};
 auto image=[&](const auto &value){if(value&&images.insert(value.get()).second)result.images.push_back(value);};
 auto paint=[&](const std::shared_ptr<const PaintedMesh> &value){if(value&&paints.insert(value.get()).second){if(paints.size()>1024)throw std::length_error("HDR retained paint count exceeds bound");result.paints.push_back(value);}};
 auto volume=[&](const std::shared_ptr<const Volume>&value){if(value&&volumes.insert(value.get()).second)result.volumes.push_back(value);};
 auto appearance=[&](const std::shared_ptr<const CreatorAppearance>&value){if(value&&value->decoded_images)for(const auto &i:*value->decoded_images)image(i);};
 auto placement=[&](const auto &groups){for(const auto &g:groups)if(g.source_placement&&g.source_placement->model&&g.source_placement->model->decoded_images)for(const auto &i:*g.source_placement->model->decoded_images)image(i);};
 placement(scene.groups());
 auto binding=[&](const std::shared_ptr<const SourceMaterialBindings>&value){if(value)materials.add(value,false);};
 auto field=[&](const std::shared_ptr<const TextureSetFieldSource>&value){
  stop();if(!value||!fields.insert(value.get()).second)return;value->authenticate_owner(value);
  const auto &v=value->field();paint(v.painted);paint(value->original_paint());paint(value->read_view(v));binding(v.material_bindings);appearance(v.appearance);
  if(value->polygon_owner())polygons.add_origin(value->polygon_owner()->before(),false);
 };
 auto collection=[&](const std::shared_ptr<const TextureSetCollection>&value){
  stop();if(!value||!sets.insert(value.get()).second)return;field(value->primary_source());
  for(const auto &v:value->additional()){paint(v.painted);binding(v.material_bindings);appearance(v.appearance);field(v.source);}
 };
 for(const auto &o:scene.objects()){
  stop();paint(o.painted);volume(o.volume);binding(o.material_bindings);appearance(o.appearance);collection(o.texture_sets);
  if(o.polygon)polygons.add(o.polygon,false);
  if(o.shape_source)polygons.add_origin(o.shape_source,false);
  if(o.rig_source)polygons.add_rig_origin(o.rig_source,false);
 }
 size_t origin=0,material=0,uv=0,p=0;
 for(;;){
  stop();collect_canonical_paint_controls(materials,polygons,false,cancelled);
  while(origin<polygons.origins().size()){
   const auto v=polygons.origins()[origin++];paint(v->paint);volume(v->volume);binding(v->material_bindings);appearance(v->appearance);collection(v->texture_sets);if(v->placement)placement(v->placement->groups);
  }
  while(uv<materials.origins().size()){
   const auto v=materials.origins()[uv++];paint(v->paint);appearance(v->appearance);for(const auto &role:v->roles)image(role.image);
  }
  while(material<materials.bindings().size()){
   const auto b=materials.bindings()[material++];for(const auto &role:b->roles)image(role.image);
   if(b->canonical_attributes)paint(b->canonical_attributes->before_paint());
   if(b->native_paint_frames)paint(b->native_paint_frames->before_paint());
   if(b->key_derivation)paint(b->key_derivation->before_paint());
   if(b->native_paint_sampling)paint(b->native_paint_sampling->before_paint());
   if(const auto &v=b->color_preparation){paint(v->original_paint);appearance(v->original_appearance);if(v->uv_reprojection){paint(v->uv_reprojection->before_paint);appearance(v->uv_reprojection->before_appearance);}if(v->paint_sampling)paint(v->paint_sampling->before_paint());}
   if(const auto &v=b->normal_preparation){paint(v->original_paint);appearance(v->original_appearance);if(v->uv_reprojection){paint(v->uv_reprojection->before_paint);appearance(v->uv_reprojection->before_appearance);}if(v->sampling_domain){paint(v->sampling_domain->before_paint);appearance(v->sampling_domain->before_appearance);}}
  }
  while(p<result.paints.size()){
   stop();const auto v=result.paints[p++];paint(v->source_sampling_original());
   if(const auto &maps=v->source_maps())for(const auto *role:{&maps->color,&maps->metal_rough,&maps->emission,&maps->normal,&maps->occlusion})image(role->image);
   if(const auto &projection=v->color_projection())image(projection->vertex_rgba);
   if(const auto &projection=v->normal_projection()){
    image(projection->pbr);image(projection->mtoon);
    if(const auto &d=projection->sampling_domain){image(d->resolved_pbr);image(d->resolved_mtoon);if(d->detail){image(d->detail->pbr);image(d->detail->mtoon);}}
   }
  }
  collect_canonical_paint_controls(materials,polygons,false,cancelled);
  if(origin==polygons.origins().size()&&material==materials.bindings().size()&&uv==materials.origins().size()&&p==result.paints.size())break;
 }
 stop();return result;
}
} // namespace dome::detail
