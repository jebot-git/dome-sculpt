// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/bake-authoring.hpp"
#include "bake-mask.hpp"
#include "dome/bake-recipe.hpp"
#include "dome/source-maps.hpp"
#include "dome/color-preparation.hpp"
#include "current-colors.hpp"
#include "cut-color.hpp"
#include "bake-canonical-frame.hpp"
#include "bake-cage.hpp"
#include "bake-ray-query.hpp"
#include "polygon-color-preparation.hpp"
#include "dome/source-placement.hpp"
#include "dome/normal-preparation.hpp"
#include "normal-bake-encoding.hpp"
#include "baked-normal-field.hpp"
#include "canonical-normal-uv.hpp"
#include "canonical-paint-sampling.hpp"
#include "native-paint-sampling.hpp"
#include "native-paint-bake.hpp"
#include "emission-bake-source.hpp"
#include "source4-baked.hpp"
#include "source-bake-normal.hpp"
#include "texture-set-bake.hpp"
#include "texture-set-field-source.hpp"
#include "polygon-surface-paint.hpp"
#include "polygon-surface-preview.hpp"
#include "paint-overlay-packet.hpp"
#include "paint-composite-backend.hpp"
#include "emission-authority.hpp"
#include "emission-paint-access.hpp"
#include "affine-surface.hpp"
#include "source-skin-world.hpp"
#include "deformation-active.hpp"
#include <set>
namespace dome {
namespace {
// The source and target share one allowance for transient posed frames.
// Paint, UVs, and original artist columns stay on their immutable owners.
struct RetainedBakePose {
  const std::function<bool()> &cancelled;
  size_t bytes=0,work=0;
  size_t atlas_budget=TextureSetLimits{}.maximum_bytes;

  std::shared_ptr<const detail::PolygonSurfacePreview> field(
      const TextureSetView &field,const Mesh *geometry) {
    const auto &original=field.source->source_preview();
    if(!geometry)return original;
    const auto &frames=original->geometric_frames();
    const auto &domain=*field.domain;
    const auto members=domain.triangles();
    const TextureSetLimits limits;
    if(frames.size()>(limits.maximum_bytes-bytes)/sizeof(frames.front())||
       frames.size()>(limits.maximum_work-work)/8||
       members.size()>(limits.maximum_work-work-frames.size()*8)/96)
      throw std::length_error("Retained Bake pose exceeds the frame workspace");
    auto posed=original->with_geometry(field.painted->mesh_owner(),*geometry,members,cancelled);
    if(posed->posed_frame_owner())bytes+=frames.size()*sizeof(frames.front());
    work+=frames.size()*8+members.size()*96;
    return posed;
  }
};

// Reuse the sealed before/current streams once the hit belongs to its field.
// Replaying the complete polygon graph per texel would dominate projection.
Vec3 retained_source_normal(const detail::PolygonSurfacePreview &preview, size_t triangle,
    std::array<double,3> bary, const std::array<float,4> &sample,
    const std::optional<detail::AffineInteractionFrame> &placement) {
 if(preview.native_bake())return preview.transport_baked_normal(triangle,bary,
      placement?placement->world():AffineTransform{},sample);
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
 if(preview.model()==detail::PolygonSurfaceNormalModel::NativeNOnly) {
  Vec3 normal;
  for(size_t k=0;k<3;++k)
   normal+=normal_direction(normals.at(first+k))*float(bary[k]);
  return detail::transport_polygon_direction(normal,before,current);
 }
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
 Vec3 decoded{sample[0]*2-1,sample[1]*2-1,sample[2]*2-1};
 if(std::max({std::abs(decoded.x),std::abs(decoded.y),std::abs(decoded.z)})<=1e-20f)decoded={0,0,1};
 return detail::transport_polygon_normal({basis,vertices.front().w,1},decoded,before,current);
}

std::vector<detail::RetainedSourceBakeField> capture_texture_set_bake_source(
    BakeSource &high, SculptObject source, CanvasTransform normal_to_target,
    const std::optional<detail::AffineInteractionFrame> &placement,
    RetainedBakePose &pose) {
  const auto &cancelled=pose.cancelled;
  const auto *deformed=detail::evaluates_geometry(source.deformation.get())?&high.mesh:nullptr;
  const auto primary=texture_set_view(source,0);
  const auto collection=std::move(source.texture_sets);
  high.materials.clear();
  high.materials.reserve(collection->additional().size()+1);
  high.triangle_materials.resize(high.mesh.indices.size()/3);
  std::vector<detail::RetainedSourceBakeField> fields;
  fields.reserve(collection->additional().size()+1);
  TextureSetLimits limits;limits.cancelled=cancelled;
  // The Bake may revisit donors across raster batches. Keep their atlases
  // ready within one texture-set workspace allowance for the whole donor.
  auto &atlas_budget=pose.atlas_budget;
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
      material.resolved_emission=detail::emission_bake_source(source,cancelled,nullptr,&material.emission_batch);
      material.maps=material.resolved_emission
        ? detail::EmissionExportAccess::preview(*field.painted,field.material,cancelled)
        : field.painted->preview(field.material,cancelled);
      return;
    }
    const auto preview=pose.field(field,deformed);
    if(deformed)preview->validate_bake_world(field.domain->triangles(),
        placement?placement->world():AffineTransform{},cancelled);
    auto captured=detail::PolygonSurfacePaintCodec::preview_for_bake(field,cancelled,atlas_budget,preview);
    material.maps=std::move(captured.maps);
    resolved.atlas=std::move(captured.atlas);
    if(resolved.atlas)atlas_budget-=resolved.atlas->bytes();
    const auto packet=material.maps->overlay;
    if(detail::overlay_uses_float_emission(*packet)||detail::hdr_emission(field.material.emissive)||
       detail::physical_emission(*field.material_bindings))
      resolved.emission_background=field.material_bindings->physical_emission.value_or(field.material.emissive);
    const bool native_normal=bool(preview->native_bake());
    const bool prepared_normal=preview->model()!=detail::PolygonSurfaceNormalModel::NativeNOnly;
    if(native_normal||prepared_normal){
      auto &normal=resolved.normal_image;
      normal.image=native_normal?preview->native_bake():preview->baked()->normal_field->pbr();
      normal.wrap_s=normal.wrap_t=33071;
      normal.min_filter=normal.mag_filter=9729;
    }
    resolved.sample=[owner=field.source,limits,native_normal,prepared_normal](uint32_t triangle,std::array<float,3> weights) {
      const std::array<double,3> bary{weights[0],weights[1],weights[2]};
      const auto point=owner->sample(triangle,bary,limits);
      if(prepared_normal&&!point.normal_uv)
        throw std::invalid_argument("Retained Bake source lost its normal coordinates");
      return detail::RetainedSourceBakeHit{point.paint_uv,native_normal?point.paint_uv:
          point.normal_uv.value_or(std::array<float,2>{})};
    };
    resolved.transport_normal=[preview,normal_to_target,placement](uint32_t triangle,std::array<float,3> weights,
                                                                std::array<float,4> sample) {
      const std::array<double,3> bary{weights[0],weights[1],weights[2]};
      const auto local=retained_source_normal(*preview,triangle,bary,sample,placement);
      const auto normal=ob::rotate(normal_to_target.orientation,{local.x,local.y,local.z});
      return Vec3{normal.x,normal.y,normal.z};
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
bool has_vertex_colors(const std::vector<PolygonRgba> &colors){
  return std::ranges::any_of(colors,[](auto rgba){return rgba!=PolygonRgba{1,1,1,1};});
}
struct CapturedBakeDonor {
  BakeSource source;
  detail::SourceBakeNormalResolver normals;
  std::vector<detail::RetainedSourceBakeField> fields;
  std::vector<Tangent> placed_corners;
  bool reverse_winding=false,requires_colors=false,has_mtoon_normals=false;
};
CapturedBakeDonor capture_bake_donor(const Scene &scene,SculptObject source,
    CanvasTransform target_inverse,bool target_affine,const AffineTransform &metric_from_world,
    AffineVector bake_origin,bool precise_frame,RetainedBakePose &pose,bool force_absolute_emission){
  const auto &cancelled=pose.cancelled;
  const auto check=[&]{if(cancelled&&cancelled())throw std::runtime_error("Bake source capture cancelled");};
  check();
  const bool source_affine=scene.has_affine_placement(source.id);
  if((source.texture_sets||source_affine)&&!detail::bake_source_supported(source,source_affine))
    throw std::logic_error("A Bake source needs an eligible material and normal frame adapter");
  if(!source.texture_sets&&source.mesh_active&&source.painted&&
     source.painted->baked()&&source.painted->baked()->normal_field&&
     (!source.material_bindings||!source.material_bindings->canonical_attributes))
    throw std::invalid_argument("Independent normal-field source requires its native canonical attributes");
  const bool current_source=source.polygon&&source.polygon->active&&source.polygon->colors;
  if(current_source&&!detail::cut_color_supported(source)&&!detail::writable_bake_source_supported(source))
    throw std::logic_error("This corner-color source needs an eligible shading adapter");
  std::optional<PolygonColorSurface> current;
  if(current_source){
    current.emplace();current->topology=std::shared_ptr<const PolygonMesh>(source.polygon,&source.polygon->topology);
    current->field=source.polygon->colors;
  }
  const auto placement=source_affine
    ? std::optional(detail::placement_frame(scene,source.id)) : std::nullopt;
  const auto relative=placement?target_inverse:
      ob::compose(target_inverse,scene.world_transform(source.id));
  detail::SourceBakeNormalOptions source_normal_options;
  source_normal_options.cancelled=cancelled;
  source_normal_options.placement=placement?&*placement:nullptr;
  auto source_normals=source.texture_sets?detail::SourceBakeNormalResolver{}:
      detail::source_bake_normals(source,relative,source_normal_options);
  // Authenticate ordered REST geometry before evaluation. Skin/morph changes
  // positions/normals later, never the source field's corner identity or RGBA.
  auto source_maps=source.texture_sets?std::shared_ptr<const TextureMaps>{}:
      detail::raw_bake_source(source,cancelled,&source_normals);
  if(!source_maps&&source.mesh_active&&!source.texture_sets)source_maps=source.painted->preview(source.material,cancelled);
  auto source_colors=source.mesh_active&&!source.texture_sets?detail::current_vertex_colors(source.painted->mesh(),current?&*current:nullptr,
      source.appearance.get(),source.material_bindings.get(),source_maps.get(),cancelled):std::vector<PolygonRgba>{};
  BakeSource high;
  const bool source_prepared=source.mesh_active&&!source.texture_sets&&source.painted->color_projection();
  const bool requires_colors=source_prepared||has_vertex_colors(source_colors);
  if (source.mesh_active) {
    high.mesh = source.deformation ? source.deformation->preview(source.painted->mesh())
                                   : source.painted->mesh();
    high.materials[0].maps = std::move(source_maps);
    if(source_normals.paired_maps())
      high.materials[0].maps_are_composited=!source_normals.paired_maps()->source_only;
    if(!source_prepared)high.colors=std::move(source_colors);
  } else
    high.mesh = surface_mesh(*source.volume, cancelled);
  std::vector<detail::RetainedSourceBakeField> source_fields;
  if(source.texture_sets)source_fields=capture_texture_set_bake_source(high,source,relative,placement,pose);
  else{
    high.materials[0].material=source.material;
    auto &material=high.materials[0];
    material.resolved_emission=detail::emission_bake_source(source,cancelled,&source_normals,
        &material.emission_batch,force_absolute_emission);
  }
  std::vector<Tangent> placed_corners;
  if(placement&&!source_normals&&std::any_of(high.materials.begin(),high.materials.end(),[](const auto &field){
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
  if(placement||target_affine||precise_frame){
    // Keep original triangle and corner identities even under reflection.
    // The Bake hit keeps its source barycentrics and tests effective winding.
    auto source_to_bake=placement?affine_similarity(target_inverse)*placement->world():affine_similarity(relative);
    if(!target_affine){
      // Keep the float ray workspace near the donor. A large translation can
      // collapse tiny but distinct isosurface triangles after rounding.
      auto matrix=source_to_bake.matrix();
      for(size_t axis=0;axis<3;++axis){
        matrix[12+axis]-=bake_origin[axis];
      }
      source_to_bake=AffineTransform(matrix);
    }
    const auto transform=target_affine?metric_from_world*source_to_bake:source_to_bake;
    std::optional<std::vector<Vec3>> source_positions;
    if(placement&&source.mesh_active&&source.appearance&&source.deformation&&source.deformation->skin){
      // The retained mesh node and original joint graph can have different
      // spaces. Use the same authenticated evaluation as the viewport, and
      // center the result before converting the skin positions to float.
      ObjectSurface surface;
      surface.id=source.id;surface.transform=source.transform;surface.mesh=source.painted->mesh();
      surface.deformation=source.deformation;surface.groups=scene.group_path(source.group);
      surface.appearance=source.appearance;surface.affine_edit=source.affine_edit;surface.rig_source=source.rig_source;
      auto matrix=affine_similarity(target_inverse).matrix();
      for(size_t axis=0;axis<3;++axis)matrix[12+axis]-=bake_origin[axis];
      const auto output_from_world=target_affine?metric_from_world:AffineTransform(matrix);
      source_positions=detail::source_skin_world_positions(surface,cancelled,256u*1024u*1024u,&output_from_world);
      if(source_positions)high.mesh.positions=std::move(*source_positions);
    }
    for(size_t i=0;i<high.mesh.positions.size();++i){
      if((i&1023)==0)check();
      if(!source_positions)
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
  const bool reverse=placement&&placement->orientation()==AffineOrientation::Reversing;
  const bool mtoon=source_normals&&source.material_bindings&&source.material_bindings->has_mtoon;
  return {std::move(high),std::move(source_normals),std::move(source_fields),std::move(placed_corners),
          reverse,requires_colors,mtoon};
}

struct BakeDonorSize {
  size_t vertices=0,triangles=0,materials=0;
  void add(const BakeSource &donor){
    if(donor.mesh.positions.size()>2'000'000-vertices||
       donor.mesh.indices.size()/3>2'000'000-triangles||
       donor.materials.size()>4096-materials)
      throw std::length_error("Combined Bake sources exceed the geometry or material budget");
    vertices+=donor.mesh.positions.size();
    triangles+=donor.mesh.indices.size()/3;
    materials+=donor.materials.size();
  }
};

BakeSource combine_bake_donors(std::span<CapturedBakeDonor> donors,const BakeDonorSize &size,
    std::vector<detail::SourceBakeDomain> &domains,std::vector<detail::RetainedSourceBakeField> &fields,
    const auto &check){
  BakeSource combined;
  combined.materials.clear();
  combined.mesh.positions.reserve(size.vertices);
  combined.mesh.normals.reserve(size.vertices);
  combined.mesh.indices.reserve(size.triangles*3);
  combined.triangle_materials.reserve(size.triangles);
  combined.materials.reserve(size.materials);
  fields.reserve(size.materials);
  domains.reserve(donors.size());
  for(auto &donor:donors){
    check();
    const auto base_vertex=uint32_t(combined.mesh.positions.size());
    const auto base_material=uint32_t(combined.materials.size());
    const auto &mesh=donor.source.mesh;
    combined.mesh.positions.insert(combined.mesh.positions.end(),mesh.positions.begin(),mesh.positions.end());
    combined.mesh.normals.insert(combined.mesh.normals.end(),mesh.normals.begin(),mesh.normals.end());
    for(size_t k=0;k<mesh.indices.size();++k){
      if((k&4095)==0)check();
      combined.mesh.indices.push_back(base_vertex+mesh.indices[k]);
    }
    for(size_t t=0;t<mesh.indices.size()/3;++t){
      if((t&4095)==0)check();
      combined.triangle_materials.push_back(base_material+
          (donor.source.triangle_materials.empty()?0:donor.source.triangle_materials[t]));
    }
    combined.materials.insert(combined.materials.end(),donor.source.materials.begin(),donor.source.materials.end());
    if(donor.fields.empty())fields.resize(fields.size()+donor.source.materials.size());
    else fields.insert(fields.end(),donor.fields.begin(),donor.fields.end());
    domains.push_back({&donor.source,&donor.normals,donor.placed_corners,donor.reverse_winding});
  }
  return combined;
}
} // namespace
ObjectBakeReport bake_objects(Scene &scene, std::span<const uint64_t> source_ids,
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
  if(source_ids.empty()||source_ids.size()>256)
    throw std::invalid_argument("Choose one or more Bake sources");
  std::set<uint64_t> eligible;
  for(const auto id:source_ids){
    if(!id||id==target.id||!eligible.insert(id).second)
      throw std::invalid_argument("Bake sources must be distinct from each other and the target");
  }
  if(settings.corresponding_topology&&source_ids.size()!=1)
    throw std::invalid_argument("Matching source triangles requires exactly one Bake source");
  const auto source_id=source_ids.front();
  const auto &source=scene.object(source_id);
  if(target.texture_sets && !detail::bake_target_supported(target))
    throw std::logic_error("The selected target field needs its source-retaining Bake adapter");
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
  const auto native_writable=target.material_bindings?target.material_bindings->native_paint_sampling:nullptr;
  if(settings.mask_layer){
    if(!target.painted||!detail::has_bake_mask_layer(*target.painted,settings.mask_layer))
      throw std::invalid_argument("The saved Bake mask layer is unavailable; select another layer or Whole field");
    if(settings.prepare_colors||settings.prepare_normals)
      throw std::invalid_argument("Prepare the whole surface before limiting Bake to a paint mask");
    if(target.material_bindings&&target.material_bindings->normal_preparation&&
       target.material_bindings->normal_preparation->canonical_conversion)
      throw std::invalid_argument("Convert the target's whole normal basis before using a Bake mask");
  }
  if(native_writable&&(settings.prepare_colors||settings.prepare_normals))
    throw std::invalid_argument("Native retained paint Bake uses the existing writable tangent basis");
  if(target.material_bindings&&target.material_bindings->normal_preparation&&target.material_bindings->normal_preparation->canonical_conversion){
    NormalPreparationOptions normal_options;normal_options.cancelled=cancelled;normal_options.max_angular_error=settings.normal_error;
    auto layered=convert_canonical_normals({target.painted,target.appearance,target.material_bindings,target.deformation,target.material,{}},normal_options);
    target.painted=std::move(layered.paint);target.material_bindings=std::move(layered.material_bindings);
  }
  const bool current_target=target.polygon&&target.polygon->active&&target.polygon->colors;
  if(!target.texture_sets && !detail::bake_target_supported(target))
    throw std::logic_error("This corner-color target has shading roles that target baking cannot yet preserve");

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
  if(!std::isfinite(settings.color_error)||settings.color_error<0||settings.color_error>1)
    throw std::invalid_argument("Invalid vertex-color preparation tolerance");
  if(!std::isfinite(settings.normal_error)||settings.normal_error<0||settings.normal_error>.25f)
    throw std::invalid_argument("Invalid normal preparation angular tolerance");
  for (auto mm : {settings.front_mm, settings.back_mm, settings.ao_distance_mm})
    if (!std::isfinite(mm) || mm < 0 || mm > 100'000)
      throw std::invalid_argument("Invalid world-millimetre bake distance");
  const bool retained_target=recipient&&bool(recipient->source);
  std::optional<Mesh> posed_target;
  if(retained_target){
    if(detail::evaluates_geometry(target.deformation.get()))
      posed_target=target.deformation->preview(target.painted->mesh());
  }else if (target.deformation) {
    auto preview = target.deformation->preview(target.painted->mesh());
    const auto &rest = target.painted->mesh();
    for (size_t i = 0; i < rest.positions.size(); ++i)
      if (length(preview.positions[i]-rest.positions[i]) > 1e-6f ||
          length(preview.normals[i]-rest.normals[i]) > 1e-5f)
        throw std::logic_error("Return the target to its rest pose and zero morphs before baking");
  }
  const auto target_placement=scene.has_affine_placement(target.id)
    ? std::optional(detail::placement_frame(scene,target.id)) : std::nullopt;
  const auto target_world=target_placement?CanvasTransform{}:scene.world_transform(target.id);
  const auto target_inverse=ob::inverse(target_world);
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
  const bool target_prepared=bool(target.painted->color_projection());
  const bool separate=target_prepared||settings.prepare_colors;
  if(!separate&&has_vertex_colors(target_colors))
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
  AffineTransform metric_from_world;
  if(target_placement){
    auto matrix=metric_from_world.matrix();
    const auto &world=target_placement->world().matrix();
    for(size_t i=12;i<15;++i)matrix[i]=-world[i];
    metric_from_world=AffineTransform(matrix);
  }
  const bool multiple=source_ids.size()>1;
  AffineVector bake_origin{};
  if(!multiple&&!target_placement&&scene.has_affine_placement(source_id)){
    // Retain the single-donor origin convention, including its exact
    // archive output. Combined donors share the target's local origin.
    const auto placement=detail::placement_frame(scene,source_id);
    const auto transform=affine_similarity(target_inverse)*placement.world();
    for(size_t axis=0;axis<3;++axis)bake_origin[axis]=transform.matrix()[12+axis];
  }
  RetainedBakePose retained_pose{cancelled};
  const auto retained_recipient=retained_target
    ?retained_pose.field(*recipient,posed_target?&*posed_target:nullptr):nullptr;
  const auto &recipient_binding=recipient?recipient->material_bindings:target.material_bindings;
  const bool absolute_emission=(!recipient||!recipient->source)&&recipient_binding&&
      recipient_binding->has_mtoon&&recipient_binding->material.parameters.emission_strength!=1;
  std::vector<CapturedBakeDonor> captured;
  captured.reserve(source_ids.size());
  BakeDonorSize donor_size;
  for(const auto object:source_ids){
    auto donor=capture_bake_donor(scene,scene.object(object),target_inverse,bool(target_placement),
        metric_from_world,bake_origin,multiple,retained_pose,absolute_emission&&!recipient);
    if(!separate&&donor.requires_colors)
      throw std::logic_error("Enable PBR color preparation to retain the source's vertex colors");
    if(multiple)donor_size.add(donor.source);
    captured.push_back(std::move(donor));
  }
  std::vector<detail::SourceBakeDomain> domains;
  std::vector<detail::RetainedSourceBakeField> combined_fields;
  std::optional<BakeSource> combined;
  if(multiple)combined.emplace(combine_bake_donors(captured,donor_size,domains,combined_fields,check));
  const auto &high=combined?*combined:captured.front().source;
  const auto &source_fields=multiple?combined_fields:captured.front().fields;
  const detail::SourceBakeNormalResolver no_normal;
  const auto &source_normals=multiple?no_normal:captured.front().normals;
  const detail::SourceBakePlacement source_placement{
      multiple?std::span<const Tangent>{}:std::span<const Tangent>(captured.front().placed_corners),
      !multiple&&captured.front().reverse_winding,bake_origin,domains};
  const bool have_mtoon_source=std::ranges::any_of(captured,&CapturedBakeDonor::has_mtoon_normals);
  BakeOptions options;
  options.resolution = target.painted->resolution();
  options.front_distance = float(double(settings.front_mm) * .001 / double(target_world.scale));
  options.back_distance = float(double(settings.back_mm) * .001 / double(target_world.scale));
  options.ao_distance = float(double(settings.ao_distance_mm) * .001 / double(target_world.scale));
  options.ray_bias = .0001f / target_world.scale;
  options.ao_samples = settings.ao_samples;
  options.padding = settings.padding;
  options.writable=detail::bake_layer_writability(*target.painted,settings.mask_layer,check);
  options.supersample = settings.supersample;
  if(options.supersample)options.max_texels=16'777'216; // Four samples across the largest 2048² paint atlas.
  options.fail_on_miss = !settings.allow_partial;options.allow_backfaces=settings.allow_backfaces;
  options.separate_pbr_color=separate;
  options.cancelled = cancelled;
  if(progress)options.progress=[&](float value){progress(.1f+.8f*value);};
  const auto canonical_attributes=target.material_bindings?target.material_bindings->canonical_attributes:nullptr;
  const bool prepared_normals=bool(target.painted->normal_projection())||bool(canonical_attributes);
  const bool canonical_captured=!native_writable&&!(recipient&&recipient->source)&&!prepared_normals&&detail::capture_new_bake_frame(target,cancelled);
  const auto old_bake=target.painted->baked();
  const bool native_attributes=native_writable&&target.material_bindings->native_paint_frames&&
      (!old_bake||old_bake->normal_interpretation==BakedNormalInterpretation::NativeAttributes);
  std::optional<Mesh> low;
  if(native_writable){
    PaintSamplingLimits limits;
    limits.cancelled=cancelled;
    native_writable->validate_current(*target.painted,limits);
    low=detail::native_paint_bake_mesh(target.painted->mesh(),*native_writable->domain(),cancelled);
    if(native_attributes){
      detail::normal_frame_current_authority(*target.material_bindings,target.painted->mesh(),
          target.deformation.get(),cancelled);
      const auto &frame=*target.material_bindings->normal_frames->pbr;
      if(frame.normals.size()!=low->positions.size()||frame.tangents.size()!=low->positions.size())
        throw std::invalid_argument("Authored native Bake needs a complete ordered tangent frame");
      low->normals=frame.normals;
      options.low_tangents=frame.tangents;
    }
  }else if(retained_recipient){
    const auto &mesh=posed_target?*posed_target:target.painted->mesh();
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
    const auto &frame=*target.material_bindings->normal_frames->pbr;
    const auto &mesh=target.painted->mesh();
    low.emplace();
    if(const auto &writable=target.material_bindings->canonical_paint_sampling){
      PaintSamplingLimits limits;
      limits.cancelled=cancelled;
      writable->validate_current(*target.painted,limits);
      low->uv=writable->attachment()->domain()->corner_coordinates(mesh,limits);
    }else{
      low->uv.reserve(mesh.indices.size());
      for(auto vertex:mesh.indices){
        if((low->uv.size()&1023)==0)check();
        low->uv.push_back(mesh.uv[vertex]);
      }
    }
    low->positions.reserve(mesh.indices.size());
    low->normals.reserve(mesh.indices.size());
    low->indices.reserve(mesh.indices.size());
    options.low_tangents.reserve(mesh.indices.size());
    for(size_t c=0;c<mesh.indices.size();++c){
      if((c&1023)==0)check();
      low->indices.push_back(uint32_t(c));
      low->positions.push_back(mesh.positions[mesh.indices[c]]);
      low->normals.push_back(frame.normals[c]);
      options.low_tangents.push_back(frame.tangents[c]);
    }
  }else low=detail::retained_bake_basis(target,options);
  if(settings.cage_object)
    options.cage_positions=detail::object_bake_cage(scene,target.id,settings.cage_object,
        target.painted->mesh(),low?*low:target.painted->mesh(),cancelled,
        target_placement?&metric_from_world:nullptr,posed_target?&*posed_target:nullptr);
  std::vector<Tangent> target_metric_corners;
  if(target_placement){
    if(!low)low=target.painted->mesh();
    target_metric_corners=place_bake_target(*low,options,*target_placement,metric_from_world);
  }else if(bake_origin!=AffineVector{}){
    if(!low)low=target.painted->mesh();
    const auto recenter=[&](std::span<Vec3> positions){
      for(size_t i=0;i<positions.size();++i){
        if((i&1023)==0)check();
        auto p=detail::affine_vector(positions[i]);
        for(size_t axis=0;axis<3;++axis)p[axis]-=bake_origin[axis];
        positions[i]=detail::affine_position(p);
      }
    };
    recenter(low->positions);
    recenter(options.cage_positions);
  }
  if(native_attributes){
    const auto frame=target.material_bindings->normal_frames->pbr;
    const auto *placed_low=target_placement?&*low:nullptr;
    const std::span<const Tangent> placed_tangents=target_metric_corners;
    options.encode_target_normal=[frame,placed_low,placed_tangents,limit=double(settings.normal_error)](
          uint32_t triangle,std::array<float,3> bary,Vec3 desired){
      std::array<detail::NormalBakeVertex,3> corners;
      for(size_t k=0;k<3;++k){
        const size_t corner=size_t(triangle)*3+k;
        const auto t=placed_low?placed_tangents[corner]:frame->tangents[corner];
        const auto n=placed_low?placed_low->normals[placed_low->indices[corner]]:frame->normals[corner];
        corners[k]=detail::normal_bake_vertex(n,{t[0],t[1],t[2]},t[3],detail::NormalVertexBitangent::UnitCross);
      }
      const auto basis=detail::normal_bake_basis(corners,{bary[0],bary[1],bary[2]},detail::NormalFragmentBasis::UnitColumns);
      const auto encoded=detail::encode_normal_bake(basis,desired);
      if(!encoded||encoded->quantized_angle_radians>limit)
        throw std::invalid_argument("Projected normal exceeds the authored native basis conditioning or quantization limit");
      return std::array{encoded->rgba,encoded->rgba};
    };
  }else if(prepared_normals){
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
  if(options.supersample){
    // The laid-out normal and retained UV bases may have split raster
    // vertices. Their corner order still names the original logical mesh.
    options.chart_corners=target.painted->mesh().indices;
    options.chart_seams=target.painted->mesh().uv_seams;
  }
  const auto retained_uv=target.material_bindings?target.material_bindings->canonical_uv:nullptr;
  const bool native_recipient=retained_recipient&&
      retained_recipient->model()==detail::PolygonSurfaceNormalModel::NativeNOnly;
  const auto old_native_normal=native_recipient?retained_recipient->native_bake():nullptr;
  const auto normal_resolution=old_native_normal?old_native_normal->width:
      retained_recipient&&!native_recipient?target.painted->baked()->normal_field->resolution():
      options.resolution;
  const bool split_recipient=retained_recipient&&
      (normal_resolution!=options.resolution||
       (!native_recipient&&low_mesh.uv!=retained_recipient->normal_coordinates()));
  // A pass visits each writable texel at most once. Mapped triangles need
  // no projection ray. The extra normal-coordinate pass does not write AO.
  const bool separate_normal_pass=retained_uv||split_recipient;
  const uint64_t projection_rays=!settings.corresponding_topology;
  // Keep a nonzero valid allowance if a matching normal-only second pass
  // follows a primary pass that uses the complete projected AO budget.
  const uint64_t sufficient_rays=(options.supersample?4u:1u)*(uint64_t(options.resolution)*options.resolution*
      (uint64_t(options.ao_samples)+projection_rays)+
      (separate_normal_pass?uint64_t(normal_resolution)*normal_resolution*projection_rays:0))+1;
  options.max_rays=std::max(options.max_rays,std::min(sufficient_rays,detail::bake_ray_limit));
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
  if(separate_normal_pass&&progress)options.progress=[&](float value){progress(.1f+.4f*value);};
  const bool paired_canonical=!recipient&&!prepared_normals&&target.appearance&&
      target.material_bindings&&target.material_bindings->has_mtoon&&target.painted->source_sampling_prepared()&&
      (have_mtoon_source||
       (old_bake&&!old_bake->maps.mtoon_normal.rgba.empty()));
  auto result = native_attributes?detail::bake_mesh_native_attributes(low_mesh,high,options,
      source_normals,source_fields,source_placement,target_metric_corners,settings.corresponding_topology):
      detail::bake_mesh_source_normals(low_mesh,high,options,source_normals,
      target_triangles,source_fields,source_placement,target_metric_corners,
      settings.corresponding_topology,absolute_emission,
      paired_canonical?std::optional(settings.normal_error):std::nullopt);
  std::optional<BakeResult> retained_normal_write;
  const auto normal_pass_options=[&](const Mesh &normal_low,uint32_t resolution,
                                     std::span<const uint32_t> faces){
    auto pass=options;
    pass.resolution=resolution;
    pass.ao_samples=0;
    if(result.rays>=pass.max_rays||result.raster_samples>=pass.max_raster_samples)
      throw std::length_error("Retained normal bake lacks aggregate projection/raster budget");
    pass.max_rays-=result.rays;
    pass.max_raster_samples-=result.raster_samples;
    uint64_t mask_work=0;
    pass.writable=detail::map_bake_writability(low_mesh,normal_low,options.resolution,options.writable,pass,faces,mask_work);
    if(mask_work>=pass.max_raster_samples)
      throw std::length_error("Retained normal bake lacks projection work after mapping its mask");
    pass.max_raster_samples-=mask_work;
    if(progress)pass.progress=[&](float value){progress(.5f+.4f*value);};
    return pass;
  };
  if(split_recipient){
    Mesh normal_low=low_mesh;
    if(!native_recipient)normal_low.uv=retained_recipient->normal_coordinates();
    auto normal_options=normal_pass_options(normal_low,normal_resolution,target_triangles);
    encode_retained(normal_options);
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
    // The first pass uses the editable paint atlas. Project the independent
    // normal field through its own root coordinates.
    Mesh normal_low=*low;
    for(size_t c=0;c<normal_low.uv.size();++c){if((c&1023)==0)check();
      normal_low.uv[c]=retained_uv->coordinates().at(target.painted->mesh().indices[c]);}
    auto normal_options=normal_pass_options(normal_low,options.resolution,{});
    retained_normal_write=detail::bake_mesh_source_normals(normal_low,high,normal_options,
        source_normals,{},source_fields,source_placement,target_metric_corners,
        settings.corresponding_topology);
  }
  check();
  if(!result.projected)throw std::runtime_error("No target texels project onto the selected source");
  ObjectBakeReport report{result.projected,result.padded,result.rays+(retained_normal_write?retained_normal_write->rays:0),result.missed};
  report.protected_texels=std::ranges::count(result.coverage,uint8_t(4));
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
  if(native_attributes)textures.normal_interpretation=BakedNormalInterpretation::NativeAttributes;
  else if(prepared_normals||(retained_recipient&&!native_recipient))
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
      normal_write.emission_field=std::move(retained_normal_write->emission_field);
      normal_write.coverage.reserve(retained_normal_write->coverage.size());
      for(auto c:retained_normal_write->coverage)normal_write.coverage.push_back(c==1||c==2?255:0);
      textures.normal_field=before->replace(normal_write,field_options);
    }else textures.normal_field=before->replace(textures,field_options);
    textures.maps.normal={};textures.maps.mtoon_normal={};
  }
  std::shared_ptr<const Image> native_normal;
  if(native_recipient){
    auto normal=std::make_shared<Image>(retained_normal_write?
        std::move(retained_normal_write->normal):std::move(textures.maps.normal));
    const auto &coverage=retained_normal_write?retained_normal_write->coverage:result.coverage;
    constexpr std::array<uint8_t,4> neutral{128,128,255,0};
    for(size_t i=0;i<coverage.size();++i){
      if((i&1023)==0)check();
      if(coverage[i]!=1&&coverage[i]!=2){
        const auto *pixel=old_native_normal?old_native_normal->rgba.data()+i*4:neutral.data();
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
  auto recipe=multiple?capture_bake_recipe(scene,source_ids,settings):capture_bake_recipe(scene,source_id,settings);
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
ObjectBakeReport bake_object(Scene &scene,uint64_t source,ObjectBakeSettings settings,
    const std::function<bool()> &cancelled,const std::function<void(float)> &progress){
  return bake_objects(scene,std::span(&source,1),settings,cancelled,progress);
}
} // namespace dome
