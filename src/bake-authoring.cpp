// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/bake-authoring.hpp"
#include "dome/bake-recipe.hpp"
#include "dome/source-maps.hpp"
#include "dome/color-preparation.hpp"
#include "current-colors.hpp"
#include "cut-color.hpp"
#include "bake-canonical-frame.hpp"
#include "bake-cage.hpp"
#include "polygon-color-preparation.hpp"
#include "dome/source-placement.hpp"
#include "dome/normal-preparation.hpp"
#include "normal-bake-encoding.hpp"
#include "baked-normal-field.hpp"
#include "canonical-normal-uv.hpp"
#include "emission-bake-source.hpp"
#include "source4-baked.hpp"
#include "source-bake-normal.hpp"
#include "texture-set-bake.hpp"
#include "texture-set-field-source.hpp"
#include "polygon-surface-paint.hpp"
#include "polygon-surface-preview.hpp"
#include "paint-overlay-sample.hpp"
#include "emission-authority.hpp"
#include "emission-paint-access.hpp"
#include "affine-surface.hpp"
namespace dome {
namespace {
// Reuse the sealed before/current streams once the hit belongs to its field.
// Replaying the complete polygon graph per texel would dominate projection.
Vec3 retained_source_normal(const detail::PolygonSurfacePreview &preview, size_t triangle,
    std::array<double,3> bary, std::optional<std::array<float,2>> normal_uv,
    const std::optional<detail::AffineInteractionFrame> &placement) {
 const auto &normals=preview.normals();
 const auto &frame=preview.geometric_frames().at(triangle);
 const auto normal_direction=[&](Vec3 n) {
  return placement?detail::placed_normal(*placement,n):detail::normal_bake_math::unit(n);
 };
 const auto geometry=[&](size_t offset) {
  const auto &n=frame[offset],&t=frame[offset+1];
  Vec3 normal{n[0],n[1],n[2]},tangent{t[0],t[1],t[2]};
  float hand=n[3];
  if(placement){
   normal=normal_direction(normal);
   const auto placed=detail::placed_tangent(*placement,{t[0],t[1],t[2],hand});
   const Vec3 raw{placed[0],placed[1],placed[2]};
   tangent=detail::normal_bake_math::unit(raw-normal*dot(raw,normal));
   hand=placed[3];
  }
  return detail::PolygonGeometryFrame{{tangent,cross(normal,tangent)*hand,normal},hand};
 };
 const auto before=geometry(0),current=geometry(2);
 const auto first=triangle*3;
 if(preview.native_bake())return preview.sample_native_bake(triangle,bary,
      placement?placement->world():AffineTransform{});
 if(preview.model()==detail::PolygonSurfaceNormalModel::NativeNOnly) {
  Vec3 normal;
  for(size_t k=0;k<3;++k)
   normal+=normal_direction(normals.at(first+k))*float(bary[k]);
  return detail::transport_polygon_direction(normal,before,current);
 }
 if(!normal_uv)throw std::invalid_argument("Retained Bake source lost its normal coordinates");
 std::array<detail::NormalBakeVertex,3> vertices;
 for(size_t k=0;k<3;++k) {
  auto t=preview.tangents().at(first+k);
  auto n=normals.at(first+k);
  if(placement){
   n=normal_direction(n);
   t=detail::placed_tangent(*placement,t);
  }
  vertices[k]=detail::normal_bake_vertex(n,{t[0],t[1],t[2]},t[3],
      detail::NormalVertexBitangent::UnitCross);
 }
 const auto basis=detail::normal_bake_basis(vertices,bary,detail::NormalFragmentBasis::UnitColumns);
 PaintSourceTexture texture;
 texture.image=preview.baked()->normal_field->pbr();
 texture.wrap_s=texture.wrap_t=33071;texture.min_filter=texture.mag_filter=9729;
 const auto value=sample_source_texture(texture,(*normal_uv)[0],(*normal_uv)[1],false);
 Vec3 decoded{value[0]*2-1,value[1]*2-1,value[2]*2-1};
 if(std::max({std::abs(decoded.x),std::abs(decoded.y),std::abs(decoded.z)})<=1e-20f)decoded={0,0,1};
 return detail::transport_polygon_normal({basis,vertices.front().w,1},decoded,before,current);
}

std::vector<detail::RetainedSourceBakeField> capture_texture_set_bake_source(
    BakeSource &high, SculptObject source, CanvasTransform normal_to_target,
    const std::optional<detail::AffineInteractionFrame> &placement,
    const std::function<bool()> &cancelled) {
  const auto primary=texture_set_view(source,0);
  const auto collection=std::move(source.texture_sets);
  high.materials.clear();
  high.materials.reserve(collection->additional().size()+1);
  high.triangle_materials.resize(high.mesh.indices.size()/3);
  std::vector<detail::RetainedSourceBakeField> fields;
  fields.reserve(collection->additional().size()+1);
  TextureSetLimits limits;limits.cancelled=cancelled;
  const auto capture=[&](const TextureSetView &field) {
    if (cancelled && cancelled())
      throw std::runtime_error("Texture-set Bake source capture cancelled");
    if(field.domain->triangles().empty())return;
    const auto index=uint32_t(high.materials.size());
    for (const auto triangle : field.domain->triangles())
      high.triangle_materials[triangle]=index;
    auto &material=high.materials.emplace_back();
    material.material=field.material;
    auto &resolved=fields.emplace_back();
    if(!field.source) {
      source.painted=field.painted->snapshot();
      source.material=field.material;
      source.material_bindings=field.material_bindings;
      source.appearance=field.appearance;
      material.resolved_emission=detail::emission_bake_source(source,cancelled);
      material.maps=material.resolved_emission
        ? detail::EmissionExportAccess::preview(*field.painted,field.material,cancelled)
        : field.painted->preview(field.material,cancelled);
      return;
    }
    const auto paint=detail::texture_set_source_read_view(field,limits);
    const auto preview=field.source->source_preview();
    material.maps=detail::PolygonSurfacePaintCodec::preview(*paint,field.material_bindings,
        field.material,cancelled,preview);
    const auto packet=material.maps->overlay;
    resolved.physical_emission=detail::overlay_uses_float_emission(*packet)||
        detail::hdr_emission(field.material.emissive)||detail::physical_emission(*field.material_bindings);
    const auto background=field.material_bindings->physical_emission.value_or(field.material.emissive);
    resolved.sample=[owner=field.source,preview,limits,normal_to_target,placement,background,
        emission=resolved.physical_emission?packet:nullptr](uint32_t triangle,std::array<float,3> weights) {
      const std::array<double,3> bary{weights[0],weights[1],weights[2]};
      const auto point=owner->sample(triangle,bary,limits);
      const auto local=retained_source_normal(*preview,triangle,bary,point.normal_uv,placement);
      const auto normal=ob::rotate(normal_to_target.orientation,{local.x,local.y,local.z});
      detail::RetainedSourceBakeHit result{point.paint_uv,{normal.x,normal.y,normal.z},{}};
      if(emission) {
        const auto values=detail::sample_overlay_values(*emission,point.paint_uv[0],point.paint_uv[1],false);
        result.emission=BakedEmissionSample{{values[2][0],values[2][1],values[2][2]},
            {values[4][0],values[4][1],values[4][2]},values[3][3]}.resolve(background,1);
      }
      return result;
    };
  };
  capture(primary);
  for (const auto &field : collection->additional())
    capture({field.id,field.name,field.painted,field.material,field.appearance,
             field.material_bindings,field.bake_recipe,field.domain,field.source});
  return fields;
}

std::vector<Tangent> place_bake_target(Mesh &mesh,BakeOptions &options,
    const detail::AffineInteractionFrame &placement,const AffineTransform &metric_from_world) {
  if(mesh.positions.size()>2'000'000||mesh.indices.size()/3>2'000'000)
    throw std::length_error("Low bake mesh exceeds surface limits");
  const auto stop=[&](size_t i) {
    if((i&1023)==0&&options.cancelled&&options.cancelled())
      throw std::runtime_error("Bake target placement cancelled");
  };
  // The viewport places native tangents and inverse-transpose normals
  // independently. Generate the corner tangents before moving any positions.
  std::vector<Tangent> corners;
  if(options.low_tangents.empty())corners=tangent_corners(mesh,options.cancelled);
  else {
    corners.reserve(mesh.indices.size());
    for(size_t c=0;c<mesh.indices.size();++c){
      stop(c);
      corners.push_back(options.low_tangents.at(mesh.indices[c]));
    }
    options.low_tangents.clear();
  }
  for(size_t c=0;c<corners.size();++c){
    stop(c);
    corners[c]=detail::placed_tangent(placement,corners[c]);
  }
  const auto metric_from_target=metric_from_world*placement.world();
  for(size_t v=0;v<mesh.positions.size();++v){
    stop(v);
    const auto p=metric_from_target.point(detail::affine_vector(mesh.positions[v]));
    mesh.positions[v]=detail::affine_position(p);
    mesh.normals[v]=detail::placed_normal(placement,mesh.normals[v]);
  }
  return corners;
}
} // namespace
ObjectBakeReport bake_object(Scene &scene, uint64_t source_id,
                              ObjectBakeSettings settings,
                              const std::function<bool()> &cancelled,
                              const std::function<void(float)> &progress) {
  auto check = [&] {
    if (cancelled && cancelled()) throw std::runtime_error("Object baking cancelled");
  };
  check();
  if(progress)progress(0);
  scene.validate();
  auto target = scene.active();
  auto source = scene.object(source_id);
  if(target.texture_sets && !detail::bake_target_supported(target))
    throw std::logic_error("The selected target field needs its source-retaining Bake adapter");
  if(source.texture_sets && !detail::bake_source_supported(source))
    throw std::logic_error("The source texture collection needs its retained material Bake adapter");
  std::optional<TextureSetView> recipient;
  if(target.texture_sets){
    if(settings.prepare_colors || settings.prepare_normals)
      throw std::invalid_argument("Prepare source colors or portable normal targets before splitting this mesh into texture sets");
    recipient=active_texture_set(target);
    target.painted=recipient->painted->snapshot();
    recipient->painted=target.painted;
    target.material=recipient->material;
    target.appearance=recipient->appearance;
    target.material_bindings=recipient->material_bindings;
  }
  if(target.material_bindings&&target.material_bindings->native_paint_sampling)throw std::logic_error("Retained native paint target requires its sampling-aware Bake adapter");
  if(source.material_bindings&&source.material_bindings->native_paint_sampling)throw std::logic_error("Retained native paint source requires its sampling-aware Bake adapter");
  if(target.material_bindings&&target.material_bindings->normal_preparation&&target.material_bindings->normal_preparation->canonical_conversion){
    NormalPreparationOptions normal_options;normal_options.cancelled=cancelled;normal_options.max_angular_error=settings.normal_error;
    auto layered=convert_canonical_normals({target.painted,target.appearance,target.material_bindings,target.deformation,target.material,{}},normal_options);
    target.painted=std::move(layered.paint);target.material_bindings=std::move(layered.material_bindings);
  }
  const bool current_source=source.polygon&&source.polygon->active&&source.polygon->colors;
  const bool current_target=target.polygon&&target.polygon->active&&target.polygon->colors;
  if(!target.texture_sets && !detail::corner_bake_target_supported(target))
    throw std::logic_error("This corner-color target has shading roles that target baking cannot yet preserve");

  // This source route uses the same authenticated ordinary shading domain as
  // plane cutting. Other original/prepared roles still require their adapters.
  if(current_source&&!detail::cut_color_supported(source))
    throw std::logic_error("This corner-color source has shading roles that source baking cannot yet preserve");
  if (source.id == target.id)
    throw std::invalid_argument("Choose a different source object for baking");
  if (!target.visible || target.locked || !target.mesh_active || !target.painted)
    throw std::logic_error("Show, unlock and prepare the target painting mesh before baking");
  if(settings.corresponding_topology){
    if(settings.cage_object)
      throw std::invalid_argument("Matching source triangles requires a bake without a cage");
    if(!source.mesh_active||!source.painted)
      throw std::invalid_argument("Matching source triangles requires an editable source mesh");
    const auto &source_mesh=source.painted->mesh();
    const auto &target_mesh=target.painted->mesh();
    if(source_mesh.positions.size()!=target_mesh.positions.size()||
       source_mesh.indices.size()!=target_mesh.indices.size())
      throw std::invalid_argument("Matching source triangles requires the same vertex count and ordered indices");
    for(size_t corner=0;corner<target_mesh.indices.size();++corner){
      if((corner&1023)==0)check();
      if(source_mesh.indices[corner]!=target_mesh.indices[corner])
        throw std::invalid_argument("Matching source triangles requires the same vertex count and ordered indices");
    }
  }
  if(!source.texture_sets&&source.mesh_active&&source.painted&&
      source.painted->baked()&&source.painted->baked()->normal_field)
    throw std::invalid_argument("Select a source without prepared normal attributes for this bake; source-frame transport is required and both objects are retained");
  if(!std::isfinite(settings.color_error)||settings.color_error<0||settings.color_error>1)
    throw std::invalid_argument("Invalid vertex-color preparation tolerance");
  if(!std::isfinite(settings.normal_error)||settings.normal_error<0||settings.normal_error>.25f)
    throw std::invalid_argument("Invalid normal preparation angular tolerance");
  for (auto mm : {settings.front_mm, settings.back_mm, settings.ao_distance_mm})
    if (!std::isfinite(mm) || mm < 0 || mm > 100'000)
      throw std::invalid_argument("Invalid world-millimetre bake distance");
  if (target.deformation) {
    auto preview = target.deformation->preview(target.painted->mesh());
    const auto &rest = target.painted->mesh();
    for (size_t i = 0; i < rest.positions.size(); ++i)
      if (length(preview.positions[i]-rest.positions[i]) > 1e-6f ||
          length(preview.normals[i]-rest.normals[i]) > 1e-5f)
        throw std::logic_error("Return the target to its rest pose and zero morphs before baking");
  }
  std::optional<PolygonColorSurface> current;
  if(current_source){
    current.emplace();current->topology=std::shared_ptr<const PolygonMesh>(source.polygon,&source.polygon->topology);
    current->field=source.polygon->colors;
  }
  // Authenticate ordered REST geometry before evaluation. Skin/morph changes
  // positions/normals later, never the source field's corner identity or RGBA.
  auto source_maps=source.texture_sets?std::shared_ptr<const TextureMaps>{}:detail::raw_bake_source(source,cancelled);
  if(!source_maps&&source.mesh_active&&!source.texture_sets)source_maps=source.painted->preview(source.material,cancelled);
  auto source_colors=source.mesh_active&&!source.texture_sets?detail::current_vertex_colors(source.painted->mesh(),current?&*current:nullptr,
      source.appearance.get(),source.material_bindings.get(),source_maps.get(),cancelled):std::vector<PolygonRgba>{};
  std::shared_ptr<const PolygonColorSurface> target_current;
  if(current_target){
    auto descriptor=std::make_shared<PolygonColorSurface>();
    descriptor->topology=std::make_shared<const PolygonMesh>(target.polygon->topology);
    descriptor->field=target.polygon->colors;target_current=std::move(descriptor);
  }
  auto target_maps=current_target?target.painted->preview(target.material,cancelled):std::shared_ptr<const TextureMaps>{};
  auto target_colors=current_target?detail::current_vertex_colors(target.painted->mesh(),target_current.get(),
      target.appearance.get(),target.material_bindings.get(),target_maps.get(),cancelled)
      :detail::source_vertex_colors(target.appearance.get(),target.painted->mesh().positions.size(),cancelled);
  auto nonidentity=[](const auto &colors){return std::any_of(colors.begin(),colors.end(),[](auto c){return c!=std::array<float,4>{1,1,1,1};});};
  const bool source_prepared=source.mesh_active&&!source.texture_sets&&source.painted->color_projection();
  const bool target_prepared=bool(target.painted->color_projection());
  const bool separate=target_prepared||settings.prepare_colors;
  if(!separate&&(source_prepared||nonidentity(source_colors)||nonidentity(target_colors)))
    throw std::logic_error("Enable PBR color preparation to bake colored source or target data without losing or multiplying its vertex colors");
  bool converted=false;
  if(settings.prepare_colors&&!target_prepared){
    auto placement=capture_source_placement(scene,target.id);
    const auto world=placement?CanvasTransform{}:scene.world_transform(target.id);
    detail::strict_polygon_color_target(target,world,cancelled,std::move(placement),current_target);
    ColorPreparationOptions options;options.resolve_identity=true;options.max_projection_error=settings.color_error;options.cancelled=cancelled;
    auto prepared=prepare_source_colors({target.painted,target.appearance,target.material_bindings,target.deformation,target.material,target_current},options);
    target.painted=std::move(prepared.paint);target.material_bindings=std::move(prepared.material_bindings);converted=prepared.converted;
  }
  bool normal_converted=false;
  if(settings.prepare_normals&&!target.painted->normal_projection()&&!(target.material_bindings&&target.material_bindings->canonical_attributes)){
    auto placement=capture_source_placement(scene,target.id);
    const auto world=placement?CanvasTransform{}:scene.world_transform(target.id);
    detail::strict_polygon_color_target(target,world,cancelled,std::move(placement),current_target);
    NormalPreparationOptions normal_options;normal_options.max_angular_error=settings.normal_error;normal_options.cancelled=cancelled;
    auto prepared=prepare_source_normals({target.painted,target.appearance,target.material_bindings,target.deformation,target.material},normal_options);
    target.painted=std::move(prepared.paint);target.material_bindings=std::move(prepared.material_bindings);normal_converted=prepared.converted;
  }
  if(progress)progress(.05f);
  BakeSource high;
  if (source.mesh_active) {
    high.mesh = source.deformation ? source.deformation->preview(source.painted->mesh())
                                   : source.painted->mesh();
    high.materials[0].maps = std::move(source_maps);
    if(!source_prepared)high.colors=std::move(source_colors);
  } else
    high.mesh = surface_mesh(*source.volume, cancelled);
  const auto target_placement=scene.has_affine_placement(target.id)
    ? std::optional(detail::placement_frame(scene,target.id)) : std::nullopt;
  const auto target_world=target_placement?CanvasTransform{}:scene.world_transform(target.id);
  const auto target_inverse=ob::inverse(target_world);
  AffineTransform metric_from_world;
  if(target_placement){
    auto matrix=metric_from_world.matrix();
    const auto &world=target_placement->world().matrix();
    for(size_t i=12;i<15;++i)matrix[i]=-world[i];
    metric_from_world=AffineTransform(matrix);
  }
  const auto placement=source.texture_sets&&scene.has_affine_placement(source.id)
    ? std::optional(detail::placement_frame(scene,source.id)) : std::nullopt;
  const auto relative=placement?target_inverse:
      ob::compose(target_inverse,scene.world_transform(source.id));
  std::vector<detail::RetainedSourceBakeField> source_fields;
  if(source.texture_sets)source_fields=capture_texture_set_bake_source(high,source,relative,placement,cancelled);
  else{
    high.materials[0].material=source.material;
    high.materials[0].resolved_emission=detail::emission_bake_source(source,cancelled);
  }
  detail::SourceBakeNormalOptions source_normal_options;source_normal_options.cancelled=cancelled;
  const auto source_normals=source.texture_sets?detail::SourceBakeNormalResolver{}:
      detail::source_bake_normals(source,relative,source_normal_options);
  std::vector<Tangent> placed_corners;
  if(placement&&std::any_of(high.materials.begin(),high.materials.end(),[](const auto &field){
       return field.maps&&!field.maps->polygon_surface&&!field.maps->normal.rgba.empty();
      })){
    placed_corners=tangent_corners(high.mesh,cancelled);
    for(size_t i=0;i<placed_corners.size();++i){
      if((i&1023)==0)check();
      auto t=detail::placed_tangent(*placement,placed_corners[i]);
      const auto rotated=ob::rotate(relative.orientation,{t[0],t[1],t[2]});
      placed_corners[i]={rotated.x,rotated.y,rotated.z,t[3]};
    }
  }
  const detail::SourceBakePlacement source_placement{placed_corners,
      placement&&placement->orientation()==AffineOrientation::Reversing};
  if(placement||target_placement){
    // Keep original triangle and corner identities even under reflection.
    // The Bake hit keeps its source barycentrics and tests effective winding.
    const auto source_to_bake=placement?affine_similarity(target_inverse)*placement->world():affine_similarity(relative);
    const auto transform=target_placement?metric_from_world*source_to_bake:source_to_bake;
    for(size_t i=0;i<high.mesh.positions.size();++i){
      if((i&1023)==0)check();
      high.mesh.positions[i]=detail::affine_position(transform.point(detail::affine_vector(high.mesh.positions[i])));
      const auto n=placement?detail::placed_normal(*placement,high.mesh.normals[i]):high.mesh.normals[i];
      const auto rotated=ob::rotate(relative.orientation,{n.x,n.y,n.z});
      high.mesh.normals[i]={rotated.x,rotated.y,rotated.z};
    }
  }else{
    for (auto &p : high.mesh.positions) {
      auto world = ob::transform(relative, {p.x,p.y,p.z});
      p = {world.x,world.y,world.z};
    }
    for (auto &n : high.mesh.normals) {
      auto world = ob::rotate(relative.orientation, {n.x,n.y,n.z});
      n = {world.x,world.y,world.z};
    }
  }
  BakeOptions options;
  options.resolution = target.painted->resolution();
  options.front_distance = float(double(settings.front_mm) * .001 / double(target_world.scale));
  options.back_distance = float(double(settings.back_mm) * .001 / double(target_world.scale));
  options.ao_distance = float(double(settings.ao_distance_mm) * .001 / double(target_world.scale));
  options.ray_bias = .0001f / target_world.scale;
  options.ao_samples = settings.ao_samples;
  options.padding = settings.padding;
  options.fail_on_miss = !settings.allow_partial;options.allow_backfaces=settings.allow_backfaces;
  options.separate_pbr_color=separate;
  options.cancelled = cancelled;
  if(progress)options.progress=[&](float value){progress(.1f+.8f*value);};
  const auto canonical_attributes=target.material_bindings?target.material_bindings->canonical_attributes:nullptr;
  const bool prepared_normals=bool(target.painted->normal_projection())||bool(canonical_attributes);
  const bool canonical_captured=!(recipient&&recipient->source)&&!prepared_normals&&detail::capture_new_bake_frame(target,cancelled);
  std::optional<Mesh> low;
  const auto retained_recipient=recipient&&recipient->source?recipient->source->source_preview():nullptr;
  if(retained_recipient){
    const auto &mesh=target.painted->mesh();
    if(mesh.indices.size()>2'000'000)
      throw std::length_error("Retained Bake target exceeds the corner workspace");
    low.emplace();
    low->positions.reserve(mesh.indices.size());
    low->normals.reserve(mesh.indices.size());
    low->indices.reserve(mesh.indices.size());
    for(size_t c=0;c<mesh.indices.size();++c){
      if((c&1023)==0)check();
      const auto vertex=mesh.indices[c];
      low->indices.push_back(uint32_t(c));
      low->positions.push_back(mesh.positions.at(vertex));
      low->normals.push_back(mesh.normals.at(vertex));
    }
    low->uv=retained_recipient->paint_coordinates();
  }else if(prepared_normals){
    const auto &frame=*target.material_bindings->normal_frames->pbr;const auto &mesh=target.painted->mesh();
    low.emplace();low->indices.resize(mesh.indices.size());
    for(size_t c=0;c<mesh.indices.size();++c){if((c&1023)==0)check();const auto v=mesh.indices[c];low->indices[c]=uint32_t(c);low->positions.push_back(mesh.positions[v]);low->normals.push_back(frame.normals[c]);low->uv.push_back(mesh.uv[v]);options.low_tangents.push_back(frame.tangents[c]);}
  }else low=detail::retained_bake_basis(target,options);
  if(settings.cage_object)
    options.cage_positions=detail::object_bake_cage(scene,target.id,settings.cage_object,
        target.painted->mesh(),low?*low:target.painted->mesh(),cancelled,
        target_placement?&metric_from_world:nullptr);
  std::vector<Tangent> target_metric_corners;
  if(target_placement){
    if(!low)low=target.painted->mesh();
    target_metric_corners=place_bake_target(*low,options,*target_placement,metric_from_world);
  }
  if(prepared_normals){
    const auto frames=target.material_bindings->normal_frames;
    const auto placed_low=target_placement?&*low:nullptr;
    const std::span<const Tangent> metric_corners=target_metric_corners;
    const auto mode=canonical_attributes?PreparedMToonNormalEvaluation::Absent:target.material_bindings->normal_preparation->receipt.mtoon;
    options.encode_target_normal=[frames,placed_low,metric_corners,mode,limit=double(settings.normal_error)](uint32_t triangle,std::array<float,3> bary,Vec3 desired){
      std::array<detail::NormalBakeVertex,3> pbr,toon;const auto &f=*frames->pbr;
      for(size_t k=0;k<3;++k){const size_t c=size_t(triangle)*3+k;
        const auto t=placed_low?metric_corners[c]:f.tangents[c];
        const auto n=placed_low?placed_low->normals[placed_low->indices[c]]:f.normals[c];
        const Vec3 T{t[0],t[1],t[2]};
        pbr[k]=detail::normal_bake_vertex(n,T,t[3],detail::NormalVertexBitangent::UnitCross);
        toon[k]=detail::normal_bake_vertex(n,T,t[3],detail::NormalVertexBitangent::RawCross);
      }
      const std::array<double,3> weights{bary[0],bary[1],bary[2]};
      const auto a=detail::normal_bake_basis(pbr,weights,detail::NormalFragmentBasis::UnitColumns);
      const auto b=mode==PreparedMToonNormalEvaluation::Absent?a:detail::normal_bake_basis(toon,weights,mode==PreparedMToonNormalEvaluation::LegacyRawInterpolated?detail::NormalFragmentBasis::RawColumns:detail::NormalFragmentBasis::UnitCross);
      auto x=detail::encode_normal_bake(a,desired),y=mode==PreparedMToonNormalEvaluation::Absent?x:detail::encode_normal_bake(b,desired);
      if(!x||!y||x->quantized_angle_radians>limit||y->quantized_angle_radians>limit)
        throw std::invalid_argument("Projected normal exceeds the explicit target basis conditioning/quantization contract");
      return std::array<std::array<uint8_t,4>,2>{x->rgba,y->rgba};
    };
  }
  const auto &low_mesh=low?*low:target.painted->mesh();
  const auto retained_uv=target.material_bindings?target.material_bindings->canonical_uv:nullptr;
  const bool native_recipient=retained_recipient&&
      retained_recipient->model()==detail::PolygonSurfaceNormalModel::NativeNOnly;
  const bool split_recipient=retained_recipient&&!native_recipient&&
      low_mesh.uv!=retained_recipient->normal_coordinates();
  const auto target_triangles=recipient?recipient->domain->triangles():std::span<const uint32_t>{};
  const auto encode_retained=[&](BakeOptions &options){
    const auto world=target_placement?target_placement->world():AffineTransform{};
    retained_recipient->validate_bake_world(target_triangles,world,cancelled);
    options.encode_target_normal=[preview=retained_recipient,world,limit=double(settings.normal_error)](
        uint32_t triangle,std::array<float,3> bary,Vec3 desired){
      const auto [before,current]=preview->triangle_frames(triangle,world);
      const auto direction=detail::transport_polygon_direction(desired,current,before);
      const auto basis=preview->before_role_basis(triangle,{bary[0],bary[1],bary[2]},world);
      const auto encoded=detail::encode_normal_bake(basis,direction);
      if(!encoded||encoded->quantized_angle_radians>limit)
        throw std::invalid_argument("Projected normal exceeds the retained target basis conditioning/quantization contract");
      return std::array{encoded->rgba,encoded->rgba};
    };
  };
  if(retained_recipient&&!split_recipient)encode_retained(options);
  if((retained_uv||split_recipient)&&progress)options.progress=[&](float value){progress(.1f+.4f*value);};
  auto result = detail::bake_mesh_source_normals(low_mesh,high,options,source_normals,
      target_triangles,source_fields,source_placement,target_metric_corners,
      settings.corresponding_topology);
  std::optional<BakeResult> retained_normal_write;
  if(split_recipient){
    auto normal_options=options;
    if(result.rays>=options.max_rays||result.raster_samples>=options.max_raster_samples)
      throw std::length_error("Retained texture-set normal bake lacks aggregate projection/raster budget");
    normal_options.max_rays-=result.rays;
    normal_options.max_raster_samples-=result.raster_samples;
    if(progress)normal_options.progress=[&](float value){progress(.5f+.4f*value);};
    encode_retained(normal_options);
    Mesh normal_low=low_mesh;
    normal_low.uv=retained_recipient->normal_coordinates();
    retained_normal_write=detail::bake_mesh_source_normals(normal_low,high,normal_options,
        source_normals,target_triangles,source_fields,source_placement,target_metric_corners,
        settings.corresponding_topology);
    if(!retained_normal_write->projected)
      throw std::runtime_error("No retained normal texels project onto the selected source");
  }
  if(retained_uv){
    retained_uv->validate_current(target.painted->mesh(),cancelled);
    if(!low||!canonical_attributes||low->positions.size()!=target.painted->mesh().indices.size())
      throw std::invalid_argument("Retained normal rebake lost its exact split corner basis");
    // Material channels belong to the current editable atlas. The independent
    // normal field still belongs to its original root coordinates. Project
    // that role in the original domain instead of replacing it with current-UV
    // texels. These are CPU geometry queries, never framebuffer readbacks.
    Mesh normal_low=*low;
    for(size_t c=0;c<normal_low.uv.size();++c){if((c&1023)==0)check();
      normal_low.uv[c]=retained_uv->coordinates().at(target.painted->mesh().indices[c]);}
    auto normal_options=options;
    if(result.rays>=options.max_rays||result.raster_samples>=options.max_raster_samples)
      throw std::length_error("Retained normal rebake lacks aggregate projection/raster budget");
    normal_options.max_rays-=result.rays;normal_options.max_raster_samples-=result.raster_samples;
    if(progress)normal_options.progress=[&](float value){progress(.5f+.4f*value);};
    retained_normal_write=detail::bake_mesh_source_normals(normal_low,high,normal_options,
        source_normals,{},source_fields,source_placement,target_metric_corners,
        settings.corresponding_topology);
  }
  check();
  if(!result.projected)throw std::runtime_error("No target texels project onto the selected source");
  ObjectBakeReport report{result.projected,result.padded,result.rays+(retained_normal_write?retained_normal_write->rays:0),result.missed};
  report.color_prepared=converted;
  report.canonical_captured=canonical_captured;
  report.normal_prepared=normal_converted;
  if(prepared_normals)report.normal_initial_error=canonical_attributes?canonical_attributes->receipt().observed_initial_error:target.material_bindings->normal_preparation->receipt.observed_initial_error;
  if(target.material_bindings&&target.material_bindings->color_preparation)
    report.color_error_bound=target.material_bindings->color_preparation->projection->maximum_error;
  if(auto prior=target.painted->baked())
    for(size_t i=0;i<result.coverage.size();++i) {
      if((i&1023)==0)check();
      if(result.coverage[i]==3 && prior->coverage[i])++report.preserved_misses;
    }
  BakedTextures textures;
  if(prepared_normals||(retained_recipient&&!native_recipient))
    textures.normal_interpretation=BakedNormalInterpretation::PreparedAttributes;
  if(separate)textures.color_interpretation=BakedColorInterpretation::ResolvedPbr;
  textures.maps = std::move(result.maps);
  textures.emission_field=std::move(result.emission_field);
  textures.maps.normal = std::move(result.normal);
  textures.coverage.reserve(result.coverage.size());
  for (auto c : result.coverage) textures.coverage.push_back(c==1 || c==2 ? 255 : 0);
  auto painting = target.painted->snapshot();
  if(canonical_attributes){
    detail::BakedNormalFieldOptions field_options;field_options.cancelled=cancelled;
    const auto old=target.painted->baked()?target.painted->baked()->normal_field:nullptr;
    auto before=old?old:detail::BakedNormalField::from_intrinsic(canonical_attributes,field_options);
    if(retained_normal_write){
      BakedTextures normal_write;normal_write.normal_interpretation=BakedNormalInterpretation::PreparedAttributes;
      normal_write.color_interpretation=textures.color_interpretation;
      normal_write.maps=std::move(retained_normal_write->maps);normal_write.maps.normal=std::move(retained_normal_write->normal);
      normal_write.coverage.reserve(retained_normal_write->coverage.size());
      for(auto c:retained_normal_write->coverage)normal_write.coverage.push_back(c==1||c==2?255:0);
      textures.normal_field=before->replace(normal_write,field_options);
    }else textures.normal_field=before->replace(textures,field_options);
    textures.maps.normal={};textures.maps.mtoon_normal={};
  }
  std::shared_ptr<const Image> native_normal;
  if(native_recipient){
    auto normal=std::make_shared<Image>(std::move(textures.maps.normal));
    const auto &old=retained_recipient->native_bake();
    constexpr std::array<uint8_t,4> neutral{128,128,255,0};
    for(size_t i=0;i<textures.coverage.size();++i){
      if((i&1023)==0)check();
      if(!textures.coverage[i]){
        const auto *pixel=old?old->rgba.data()+i*4:neutral.data();
        std::copy_n(pixel,4,normal->rgba.data()+i*4);
      }
    }
    native_normal=std::move(normal);
    textures.maps.normal={};
    textures.maps.mtoon_normal={};
  }else if(retained_recipient){
    const auto &prior=target.painted->baked()->normal_field;
    detail::BakedNormalFieldOptions field_options;field_options.cancelled=cancelled;
    if(retained_normal_write){
      BakedTextures normal_write;
      normal_write.normal_interpretation=BakedNormalInterpretation::PreparedAttributes;
      normal_write.maps=std::move(retained_normal_write->maps);
      normal_write.maps.normal=std::move(retained_normal_write->normal);
      normal_write.emission_field=std::move(retained_normal_write->emission_field);
      normal_write.coverage.reserve(retained_normal_write->coverage.size());
      for(auto c:retained_normal_write->coverage)normal_write.coverage.push_back(c==1||c==2?255:0);
      textures.normal_field=prior->replace(normal_write,field_options);
    }else textures.normal_field=prior->replace(textures,field_options);
    textures.maps.normal={};textures.maps.mtoon_normal={};
  }
  if(painting->source_sampling_prepared()){
    SourcePaintOptions source_options;source_options.cancelled=cancelled;
    painting=detail::install_source_bake(*painting,std::move(textures),source_options);
  }else painting->install_bake(std::move(textures));
  auto next = scene;
  auto recipe=capture_bake_recipe(scene,source_id,settings);
  if(recipient){
    TextureSetLimits limits;limits.cancelled=cancelled;
    detail::stage_texture_set_bake(next.active(),target.texture_sets,*recipient,std::move(painting),std::move(recipe),limits,std::move(native_normal));
  }else{
    next.active().painted=std::move(painting);
    next.active().material_bindings=target.material_bindings;
    next.active().polygon=target.polygon;
    next.active().bake_recipe=std::move(recipe);
  }
  next.validate();
  check();
  // A user progress callback can throw or request cancellation. No fallible
  // callback/validation may escape after the detached scene becomes authority.
  if(progress)progress(1);
  check();
  static_assert(std::is_nothrow_move_assignable_v<Scene>);
  scene = std::move(next);
  return report;
}
} // namespace dome
