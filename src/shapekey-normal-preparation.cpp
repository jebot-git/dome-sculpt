// SPDX-License-Identifier: GPL-3.0-or-later
#include "shapekey-normal-preparation.hpp"
#include "dome/normal-preparation.hpp"
#include "normal-bake-frames.hpp"
#include "normal-frame-runtime.hpp"
#include "normal-projection-identity.hpp"
#include "polygon-conversion.hpp"
namespace dome::detail {
namespace {
bool same_vectors(const std::vector<Vec3> &a,const std::vector<Vec3> &b){
  if(a.size()!=b.size())return false;
  for(size_t n=0;n<a.size();++n)if(a[n].x!=b[n].x||a[n].y!=b[n].y||a[n].z!=b[n].z)return false;
  return true;
}
bool same_base(const std::shared_ptr<const NormalFrames> &a,const std::shared_ptr<const NormalFrames> &b){
  if(!a||!b)return bool(a)==bool(b);
  return a->transport==b->transport&&a->generated_tangents==b->generated_tangents&&a->generated_uv==b->generated_uv&&
      a->origin_vertex_count==b->origin_vertex_count&&a->source_vertex_count==b->source_vertex_count&&a->source_corner_count==b->source_corner_count&&
      a->origin_vertices==b->origin_vertices&&a->source_vertices==b->source_vertices&&a->source_corners==b->source_corners&&
      same_vectors(a->normals,b->normals)&&a->tangents==b->tangents;
}
bool same_image(const Image &a,const Image &b){return a.width==b.width&&a.height==b.height&&a.rgba==b.rgba;}
bool same_base_set(const NormalFrameSet &a,const NormalFrameSet &b){
  return a.skin_policy==b.skin_policy&&a.canonical_evaluation==b.canonical_evaluation&&
      same_base(a.pbr,b.pbr)&&same_base(a.mtoon,b.mtoon)&&same_base(a.canonical,b.canonical);
}
}
ShapeKeyResult author_prepared_normal_shapekey(Scene &scene,uint64_t object,const ShapeKeyEdit &edit,
    const std::function<bool()> &cancelled){
  auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Prepared normal shapekey authoring cancelled");};stop();
  const auto &source=scene.object(object);const auto binding=source.material_bindings;
  if(!binding||!binding->normal_preparation||!source.painted||!source.painted->normal_projection()||source.painted->in_stroke())
    throw std::invalid_argument("Prepared shapekey authoring requires its finished owning normal model");
  const auto &recipe=*binding->normal_preparation;binding->validate();
  recipe.validate_current(source.painted->mesh(),source.appearance.get(),cancelled);
  const auto projection=source.painted->normal_projection();
  if(!normal_projection_identity(*projection,*recipe.projection)||!normal_projection_identity(*projection,*recipe.initial_projection))
    throw std::invalid_argument("Prepared shapekey authoring requires authenticated intrinsic sampling authority");
  const auto &current=source.painted->mesh(),&original=recipe.original_paint->mesh();
  normal_frame_current_authority(*binding,current,source.deformation.get(),cancelled);
  const NormalBakeFrameOptions bounds{128*1024*1024,128*1024*1024,cancelled};
  auto before_deform=std::make_shared<DeformationState>(recipe.original_deformation?*recipe.original_deformation:DeformationState(original));
  before_deform->morphs=MorphSet(original);
  if(source.deformation&&source.deformation->morphs){
    for(const auto &target:source.deformation->morphs->targets){stop();before_deform->morphs->targets.push_back({target.name,
        normal_bake_target_origin_positions(current,original,target.delta,bounds)});}
    before_deform->morph_weights=source.deformation->morph_weights;
    before_deform->expressions=source.deformation->expressions;
  }else{before_deform->morph_weights.clear();before_deform->expressions.reset();}
  before_deform->validate(original);
  // These are authenticated CURRENT native attributes, including generated T.
  // The full old recipe/raw N-only input is retained by the real source capsule.
  // Re-generating T from raw N-only input here would change CaptureMix semantics.
  auto before_binding=recipe.original_material?std::make_shared<SourceMaterialBindings>(*recipe.original_material):std::make_shared<SourceMaterialBindings>();
  if(!recipe.original_material){before_binding->has_mtoon=false;before_binding->native_reference=binding->native_reference;}
  before_binding->normal_preparation.reset();before_binding->normal_frames=binding->normal_frames;
  before_binding->normal_frame=SourceMaterialNormalFrame::OriginalAttributes;before_binding->uv_origin=binding->uv_origin;
  before_binding->validate();normal_frame_current_authority(*before_binding,original,before_deform.get(),cancelled);
  auto capsule=source.shape_source;
  if(!capsule||!capsule->paint||!capsule->paint->normal_projection()||!capsule->material_bindings||!capsule->material_bindings->normal_preparation||!capsule->material_bindings->normal_frames||
      !normal_projection_identity(*capsule->paint->normal_projection(),*projection)||!same_base_set(*capsule->material_bindings->normal_frames,*binding->normal_frames))
    capsule=capture_polygon_origin(scene,object);
  Scene detached=scene;auto &input=detached.object(object);
  input.painted=recipe.original_paint->snapshot();input.appearance=recipe.original_appearance;
  input.deformation=std::move(before_deform);input.material_bindings=std::move(before_binding);input.shape_source=capsule;
  ShapeKeyEdit mapped=edit;
  if(edit.action==ShapeKeyAction::Replace){
    std::vector<Vec3> delta(current.positions.size());
    for(size_t n=0;n<delta.size();++n){if((n&1023)==0)stop();delta[n]=edit.positions[n]-current.positions[n];}
    auto origin_delta=normal_bake_target_origin_positions(current,original,delta,bounds);mapped.positions=original.positions;
    for(size_t n=0;n<mapped.positions.size();++n)mapped.positions[n]+=origin_delta[n];
  }
  auto result=author_shapekey(detached,object,mapped,cancelled);if(!result.changed)return result;
  const auto &edited=detached.object(object);NormalPreparationOptions options;
  options.max_angular_error=recipe.receipt.accepted_angular_error;options.cancelled=cancelled;
  auto prepared=prepare_source_normals({edited.painted,edited.appearance,edited.material_bindings,edited.deformation,edited.material},options);
  const auto &replacement=*prepared.material_bindings->normal_preparation;const auto &old_frames=*binding->normal_frames,&new_frames=*replacement.target_frames;
  if(!same_base_set(old_frames,new_frames))
    throw std::invalid_argument("Prepared key authoring changed the intrinsic base attribute contract");
  const bool replaces=edit.action==ShapeKeyAction::Replace||edit.action==ShapeKeyAction::Rename;
  if(new_frames.current_targets.size()!=old_frames.current_targets.size()+(replaces?0:1)||
      !std::equal(old_frames.current_targets.begin(),old_frames.current_targets.end(),new_frames.current_targets.begin()))
    throw std::invalid_argument("Prepared key authoring changed existing stable target slots");
  for(const auto &old:old_frames.targets){
    auto found=std::find_if(new_frames.targets.begin(),new_frames.targets.end(),[&](const auto &t){return t.id==old.id;});
    if(found==new_frames.targets.end()||found->origin_slot!=old.origin_slot||found->source_target!=old.source_target||
        (!(edit.action==ShapeKeyAction::Replace&&old.id==old_frames.current_targets.at(edit.target))&&!same_vectors(found->positions,old.positions)))
      throw std::invalid_argument("Prepared key authoring changed unrelated original target authority");
  }
  const auto &quality=replacement.receipt,&old_quality=recipe.receipt;
  if(quality.version!=old_quality.version||quality.resolution!=old_quality.resolution||quality.pbr!=old_quality.pbr||quality.mtoon!=old_quality.mtoon||
      quality.accepted_angular_error!=old_quality.accepted_angular_error||quality.observed_initial_error!=old_quality.observed_initial_error||quality.sampled_texels!=old_quality.sampled_texels||
      !same_image(*replacement.initial_projection->pbr,*projection->pbr)||!same_image(*replacement.initial_projection->mtoon,*projection->mtoon))
    throw std::invalid_argument("Prepared key authoring changed intrinsic normal sampling or its quality receipt");
  // Reuse the old plane owners only after an independent full preparation has
  // reproduced their bytes. Deep replay below authenticates the edited model.
  auto next_recipe=std::make_shared<SourceNormalPreparation>(replacement);next_recipe->projection=next_recipe->initial_projection=projection;next_recipe->validate();
  auto next_binding=std::make_shared<SourceMaterialBindings>(*binding);next_binding->normal_frames=next_recipe->target_frames;
  next_binding->normal_preparation=std::move(next_recipe);next_binding->uv_origin=prepared.material_bindings->uv_origin;next_binding->validate();
  auto deform=std::make_shared<DeformationState>(source.deformation?*source.deformation:DeformationState(current));
  deform->morphs=MorphSet(current);
  for(const auto &target:edited.deformation->morphs->targets){stop();deform->morphs->targets.push_back({target.name,
      normal_bake_target_origin_positions(original,current,target.delta,bounds)});}
  deform->morph_weights=edited.deformation->morph_weights;deform->expressions=edited.deformation->expressions;
  deform->validate(current);normal_frame_current_authority(*next_binding,current,deform.get(),cancelled);
  Scene next=scene;auto &output=next.object(object);output.material_bindings=std::move(next_binding);output.deformation=std::move(deform);output.shape_source=std::move(capsule);
  next.validate();stop();scene=std::move(next);return result;
}
}
