// SPDX-License-Identifier: GPL-3.0-or-later
#include "shapekey-normal-preparation.hpp"
#include "dome/normal-preparation.hpp"
#include "normal-bake-frames.hpp"
#include "normal-frame-runtime.hpp"
#include "normal-projection-identity.hpp"
#include "normal-sampling-access.hpp"
#include "normal-key-derivation.hpp"
#include "polygon-conversion.hpp"
#include "polygon-color-target.hpp"
#include <set>
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


// Count the destination model before allocating remapped target arrays. The
// historical before-pose and current output-pose can have different exact
// matrix payloads; neither may disappear from the aggregate resource bound.
void domain_key_budget(const DeformationState *base,size_t vertices,size_t targets,
    size_t weights,const std::function<bool()> &cancelled){
  constexpr size_t limit=8'000'000;size_t entries=0;
  auto add=[&](size_t n){if(n>limit-entries)throw std::length_error("Prepared key remap exceeds native deformation quota");entries+=n;};add(weights);
  if(base){add(base->pose.size());for(const auto &pose:base->pose)if(pose.full_local_matrix)add(16);
    if(base->skin)for(size_t v=0;v<base->skin->vertices.size();++v){if((v&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Prepared key remap cancelled");add(base->skin->vertices[v].size());}}
  if(targets&&vertices>(limit-entries)/targets)throw std::length_error("Prepared key target arrays exceed native deformation quota");
}


// Rewinding the paint/model alone is insufficient: the live object's topology
// may already describe a later UV layout. A prepared corner descriptor owns the
// actual earlier topology/field; validate it against the earlier REST paint
// before constructing this detached control state. The live authority is never
// replaced. Its genuine retained origin chain is preserved in the rewind.
std::shared_ptr<const PolygonAuthority> key_before_polygon(const SculptObject &source,
    const std::shared_ptr<const PaintedMesh> &paint,
    const std::shared_ptr<const SourceMaterialBindings> &binding,
    const std::function<bool()> &cancelled){
  const auto current=source.polygon;if(!current||!current->active)return current;
  PolygonLimits limits;limits.cancelled=cancelled;
  if(const auto colours=binding?binding->color_preparation:nullptr;colours&&colours->current_descriptor()){
    const auto &descriptor=*colours->current_descriptor();
    polygon_color_owner(current.get(),source.material_bindings?source.material_bindings->color_preparation.get():nullptr,true);
    polygon_target_colors(paint->mesh(),descriptor,cancelled);
    auto before=std::make_shared<PolygonAuthority>(*current);
    before->topology=*descriptor.topology;before->colors=descriptor.field;
    polygon_color_owner(before.get(),colours.get(),true);
    validate_polygon_derivative(*before,paint.get(),true,limits);
    return before;
  }
  // Without a colour descriptor, geometry equality cannot supply a corner
  // field. Follow only an uncoloured authority attached to the exact original
  // SurfaceMap/domain owner, and reject competing historical certificates.
  auto same_owner=[](const auto &a,const auto &b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);};
  auto own_mesh=[&](const PaintedMesh *candidate){return candidate&&same_owner(candidate->mesh_owner(),paint->mesh_owner())&&same_owner(candidate->paint_sampling_domain(),paint->paint_sampling_domain());};
  std::shared_ptr<const PolygonAuthority> found;
  auto accept=[&](const std::shared_ptr<const PolygonAuthority> &candidate,const PaintedMesh *owner){
    if(!candidate||!candidate->active||candidate->colors||!own_mesh(owner))return;
    validate_polygon_derivative(*candidate,paint.get(),true,limits);
    if(found&&!same_owner(found,candidate))throw std::invalid_argument("Prepared key topology rewind has ambiguous historical authorities");
    found=candidate;
  };
  accept(current,source.painted.get());
  std::set<const PolygonOrigin*> visited;
  std::function<void(const std::shared_ptr<const PolygonOrigin>&,size_t)> find;
  find=[&](const auto &origin,size_t depth){
    if(!origin)return;if(cancelled&&cancelled())throw std::runtime_error("Prepared key topology rewind cancelled");
    if(depth>32||visited.size()>=512)throw std::length_error("Prepared key topology rewind exceeds retained history budget");
    if(!visited.insert(origin.get()).second)return;
    accept(origin->polygon,origin->paint.get());
    if(origin->polygon)find(origin->polygon->origin,depth+1);
    find(origin->shape_source,depth+1);find(origin->rig_source,depth+1);
  };
  find(current->origin,1);find(source.shape_source,1);find(source.rig_source,1);
  if(found)return found;
  throw std::invalid_argument("Prepared key topology rewind requires its genuine before-domain authority");
}

// UV correspondence is an authored identity, not a nearest-position query.
// A seam split may duplicate a source vertex but cannot silently average two
// independently edited directions back into one original key vertex.
std::vector<Vec3> uv_origin_delta(const std::vector<Vec3> &delta,
    const SourceNormalUvLayout &layout, size_t before_vertices,
    const std::vector<Vec3> *retained, const std::function<bool()> &cancelled) {
  if (delta.size() != layout.source_vertices.size() ||
      (retained && retained->size() != before_vertices))
    throw std::invalid_argument("Post-UV key vertex authority is incomplete");
  std::vector<Vec3> out = retained ? *retained : std::vector<Vec3>(before_vertices);
  std::vector<uint8_t> seen(before_vertices);
  for (size_t v = 0; v < delta.size(); ++v) {
    if ((v & 1023) == 0 && cancelled && cancelled())
      throw std::runtime_error("Post-UV key remap cancelled");
    const auto original = layout.source_vertices[v];
    if (original >= out.size())
      throw std::invalid_argument("Post-UV key source vertex is unavailable");
    if (seen[original] && (out[original].x != delta[v].x ||
        out[original].y != delta[v].y || out[original].z != delta[v].z))
      throw std::invalid_argument("This key separates retained UV seam vertices; explicit native corner-domain reauthoring is required");
    out[original] = delta[v];
    seen[original] = 1;
  }
  return out;
}

ShapeKeyResult author_domain_prepared_key(Scene &scene, uint64_t object,
    const ShapeKeyEdit &edit, const std::function<bool()> &cancelled) {
  const auto &source = scene.object(object);
  const auto binding = source.material_bindings;
  const auto &recipe = *binding->normal_preparation;
  const auto sampled=recipe.sampling_domain;
  const auto raster=recipe.uv_reprojection;
  const auto before_paint=sampled?sampled->before_paint:raster->before_paint;
  const auto before_material=sampled?sampled->before_material:raster->before_material;
  const auto before_appearance=sampled?sampled->before_appearance:raster->before_appearance;
  const auto before_deformation=sampled?sampled->before_deformation:raster->before_deformation;
  const auto layout=sampled?sampled->layout:raster->layout;
  const auto &before = before_paint->mesh();
  const auto &current = source.painted->mesh();
  const auto projection = source.painted->normal_projection();
  auto stop = [&] { if (cancelled && cancelled())
      throw std::runtime_error("Post-UV prepared key authoring cancelled"); };
  stop();
  normal_frame_current_authority(*binding, current, source.deformation.get(), cancelled);
  const auto source_deform=source.deformation.get();
  const size_t targets=source_deform&&source_deform->morphs?source_deform->morphs->targets.size():0;
  const size_t weights=source_deform?source_deform->morph_weights.size():0;
  const bool replacing=edit.action==ShapeKeyAction::Replace||edit.action==ShapeKeyAction::Rename;
  domain_key_budget(before_deformation.get(),before.positions.size(),targets,weights,cancelled);
  if(!replacing)domain_key_budget(before_deformation.get(),before.positions.size(),targets+1,targets+1,cancelled);
  domain_key_budget(source_deform,current.positions.size(),targets+(replacing?0:1),replacing?weights:targets+1,cancelled);

  auto before_deform = std::make_shared<DeformationState>(
      before_deformation ? *before_deformation : DeformationState(before));
  before_deform->morphs = MorphSet(before);
  if (source.deformation && source.deformation->morphs) {
    for (size_t slot = 0; slot < source.deformation->morphs->targets.size(); ++slot) {
      stop();
      const auto &target = source.deformation->morphs->targets[slot];
      const std::vector<Vec3> *retained = nullptr;
      if (before_deformation && before_deformation->morphs &&
          slot < before_deformation->morphs->targets.size())
        retained = &before_deformation->morphs->targets[slot].delta;
      before_deform->morphs->targets.push_back({target.name,
          uv_origin_delta(target.delta, *layout, before.positions.size(), retained, cancelled)});
    }
    before_deform->morph_weights = source.deformation->morph_weights;
    before_deform->expressions = source.deformation->expressions;
  } else {
    before_deform->morph_weights.clear();
    before_deform->expressions.reset();
  }
  before_deform->validate(before);

  ShapeKeyEdit mapped = edit;
  if (edit.action == ShapeKeyAction::Replace) {
    std::vector<Vec3> delta(current.positions.size());
    for (size_t v = 0; v < delta.size(); ++v) {
      if ((v & 1023) == 0) stop();
      delta[v] = edit.positions.at(v) - current.positions[v];
    }
    const std::vector<Vec3> *retained = before_deform->morphs &&
        edit.target < before_deform->morphs->targets.size()
        ? &before_deform->morphs->targets[edit.target].delta : nullptr;
    auto original = uv_origin_delta(delta, *layout, before.positions.size(), retained, cancelled);
    mapped.positions = before.positions;
    for (size_t v = 0; v < mapped.positions.size(); ++v) mapped.positions[v] += original[v];
  }

  auto capsule = source.shape_source;
  if (!capsule || !capsule->paint || !capsule->paint->normal_projection() ||
      !capsule->material_bindings || !capsule->material_bindings->normal_frames ||
      !normal_projection_identity(*capsule->paint->normal_projection(), *projection) ||
      !same_base_set(*capsule->material_bindings->normal_frames, *binding->normal_frames))
    capsule = capture_polygon_origin(scene, object);

  Scene detached = scene;
  auto &input = detached.object(object);
  input.painted = before_paint->snapshot();
  input.material_bindings = before_material;
  input.appearance = before_appearance;
  input.deformation = std::move(before_deform);
  input.polygon=key_before_polygon(source,input.painted,input.material_bindings,cancelled);
  auto result = author_prepared_normal_shapekey(detached, object, mapped, cancelled);
  if (!result.changed) return result;
  const auto &edited = detached.object(object);
  if (!same_base_set(*binding->normal_frames, *edited.material_bindings->normal_frames))
    throw std::invalid_argument("Post-UV key authoring changed the fixed intrinsic base axes");

  // Authenticate the newly edited before-model through every real UV replay.
  // Keep current paint/detail/mask bytes and material values on the output;
  // reinstalling a historical full bake would discard edits made after UV Apply.
  auto next_recipe = std::make_shared<SourceNormalPreparation>(
      *edited.material_bindings->normal_preparation);
  next_recipe->projection = projection;
  if(sampled){
    // This is a new owning replay of newly authored attributes, not a pointer
    // substitution into the old proof. In particular, never copy its private
    // runtime seal: the node identity and every before-owner have changed.
    auto next_sampling=std::make_shared<SourceNormalSampling>();
    next_sampling->receipt=sampled->receipt;
    next_sampling->layout=sampled->layout;
    next_sampling->projection=projection;
    next_sampling->before_paint=edited.painted;
    next_sampling->before_material=edited.material_bindings;
    next_sampling->before_appearance=edited.appearance;
    next_sampling->before_deformation=edited.deformation;
    const auto &prior=edited.material_bindings->normal_preparation->sampling_domain;
    next_sampling->sampling_material=prior?prior->sampling_material:edited.material_bindings;
    next_sampling->current_sampling_reference=sampled->current_sampling_reference;
    next_sampling->metadata_sampling_domain=sampled->metadata_sampling_domain;
    // Current intrinsic/resolved images, normal-specific coverage and detail
    // event owners stay exact. Full replay must authenticate all of them from
    // the edited before-model before the independent metadata seal is minted.
    authenticate_normal_sampling_metadata(next_sampling,cancelled);
    next_recipe->sampling_domain=std::move(next_sampling);
    next_recipe->uv_reprojection.reset();
  }else{
    auto next_uv=std::make_shared<SourceNormalUvReprojection>(*raster);
    next_uv->before_paint=edited.painted;
    next_uv->before_material=edited.material_bindings;
    next_uv->before_appearance=edited.appearance;
    next_uv->before_deformation=edited.deformation;
    next_uv->projection=projection;
    next_uv->validate(true,cancelled);
    next_recipe->uv_reprojection=std::move(next_uv);
  }
  next_recipe->validate();
  auto next_binding = std::make_shared<SourceMaterialBindings>(*binding);
  next_binding->normal_frames = next_recipe->target_frames;
  next_binding->normal_preparation = std::move(next_recipe);
  next_binding->uv_origin = edited.material_bindings->uv_origin;
  next_binding->validate();

  auto deform = std::make_shared<DeformationState>(source.deformation
      ? *source.deformation : DeformationState(current));
  deform->morphs = MorphSet(current);
  for (const auto &target : edited.deformation->morphs->targets) {
    stop();
    std::vector<Vec3> delta;
    delta.reserve(current.positions.size());
    for (auto original : layout->source_vertices) delta.push_back(target.delta.at(original));
    deform->morphs->targets.push_back({target.name, std::move(delta)});
  }
  deform->morph_weights = edited.deformation->morph_weights;
  deform->expressions = edited.deformation->expressions;
  deform->validate(current);
  normal_frame_current_authority(*next_binding, current, deform.get(), cancelled);
  Scene next = scene;
  auto &output = next.object(object);
  output.material_bindings = std::move(next_binding);
  output.deformation = std::move(deform);
  output.shape_source = std::move(capsule);
  next.validate();
  stop();
  scene = std::move(next);
  return result;
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
  if(needs_independent_key_domain(source,edit))return author_derived_normal_shapekey(scene,object,edit,cancelled);
  if(recipe.uv_reprojection||recipe.sampling_domain)return author_domain_prepared_key(scene,object,edit,cancelled);
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
  input.polygon=key_before_polygon(source,input.painted,input.material_bindings,cancelled);
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
