// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/bake-authoring.hpp"
#include "dome/source-maps.hpp"
#include "dome/color-preparation.hpp"
#include "source-colors.hpp"
#include "bake-canonical-frame.hpp"
#include "polygon-color-preparation.hpp"
#include "dome/normal-preparation.hpp"
#include "normal-bake-encoding.hpp"
namespace dome {
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
  if (source.id == target.id)
    throw std::invalid_argument("Choose a different source object for baking");
  if (!target.visible || target.locked || !target.mesh_active || !target.painted)
    throw std::logic_error("Show, unlock and prepare the target painting mesh before baking");
  if(source.mesh_active&&source.painted&&source.painted->normal_projection())
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
  auto source_colors=source.mesh_active?detail::source_vertex_colors(source.appearance.get(),source.painted->mesh().positions.size(),cancelled):std::vector<std::array<float,4>>{};
  auto target_colors=detail::source_vertex_colors(target.appearance.get(),target.painted->mesh().positions.size(),cancelled);
  auto nonidentity=[](const auto &colors){return std::any_of(colors.begin(),colors.end(),[](auto c){return c!=std::array<float,4>{1,1,1,1};});};
  const bool source_prepared=source.mesh_active&&source.painted->color_projection();
  const bool target_prepared=bool(target.painted->color_projection());
  const bool separate=target_prepared||settings.prepare_colors;
  if(!separate&&(source_prepared||nonidentity(source_colors)||nonidentity(target_colors)))
    throw std::logic_error("Enable PBR color preparation to bake colored source or target data without losing or multiplying its vertex colors");
  bool converted=false;
  if(settings.prepare_colors&&!target_prepared){
    detail::strict_polygon_color_target(target,scene.world_transform(target.id),cancelled);
    ColorPreparationOptions options;options.resolve_identity=true;options.max_projection_error=settings.color_error;options.cancelled=cancelled;
    auto prepared=prepare_source_colors({target.painted,target.appearance,target.material_bindings,target.deformation,target.material},options);
    target.painted=std::move(prepared.paint);target.material_bindings=std::move(prepared.material_bindings);converted=prepared.converted;
  }
  bool normal_converted=false;
  if(settings.prepare_normals&&!target.painted->normal_projection()){
    detail::strict_polygon_color_target(target,scene.world_transform(target.id),cancelled);
    NormalPreparationOptions normal_options;normal_options.max_angular_error=settings.normal_error;normal_options.cancelled=cancelled;
    auto prepared=prepare_source_normals({target.painted,target.appearance,target.material_bindings,target.deformation,target.material},normal_options);
    target.painted=std::move(prepared.paint);target.material_bindings=std::move(prepared.material_bindings);normal_converted=prepared.converted;
  }
  if(progress)progress(.05f);
  BakeSource high;
  if (source.mesh_active) {
    high.mesh = source.deformation ? source.deformation->preview(source.painted->mesh())
                                   : source.painted->mesh();
    high.materials[0].maps = source.painted->preview(source.material);
    if(!source_prepared)high.colors=std::move(source_colors);
  } else
    high.mesh = surface_mesh(*source.volume, cancelled);
  high.materials[0].material = source.material;
  auto target_world = scene.world_transform(target.id);
  auto relative = ob::compose(ob::inverse(target_world), scene.world_transform(source.id));
  for (auto &p : high.mesh.positions) {
    auto world = ob::transform(relative, {p.x,p.y,p.z});
    p = {world.x,world.y,world.z};
  }
  for (auto &n : high.mesh.normals) {
    auto world = ob::rotate(relative.orientation, {n.x,n.y,n.z});
    n = {world.x,world.y,world.z};
  }
  BakeOptions options;
  options.resolution = target.painted->resolution();
  options.front_distance = settings.front_mm * .001f / target_world.scale;
  options.back_distance = settings.back_mm * .001f / target_world.scale;
  options.ao_distance = settings.ao_distance_mm * .001f / target_world.scale;
  options.ray_bias = .0001f / target_world.scale;
  options.ao_samples = settings.ao_samples;
  options.padding = settings.padding;
  options.fail_on_miss = !settings.allow_partial;options.allow_backfaces=settings.allow_backfaces;
  options.separate_pbr_color=separate;
  options.cancelled = cancelled;
  if(progress)options.progress=[&](float value){progress(.1f+.8f*value);};
  const bool prepared_normals=bool(target.painted->normal_projection());
  const bool canonical_captured=!prepared_normals&&detail::capture_new_bake_frame(target,cancelled);
  std::optional<Mesh> low;
  if(prepared_normals){
    const auto &frame=*target.material_bindings->normal_frames->pbr;const auto &mesh=target.painted->mesh();
    low.emplace();low->indices.resize(mesh.indices.size());
    for(size_t c=0;c<mesh.indices.size();++c){if((c&1023)==0)check();const auto v=mesh.indices[c];low->indices[c]=uint32_t(c);low->positions.push_back(mesh.positions[v]);low->normals.push_back(frame.normals[c]);low->uv.push_back(mesh.uv[v]);options.low_tangents.push_back(frame.tangents[c]);}
    const auto frames=target.material_bindings->normal_frames;
    const auto mode=target.material_bindings->normal_preparation->receipt.mtoon;
    options.encode_target_normal=[frames,mode,limit=double(settings.normal_error)](uint32_t triangle,std::array<float,3> bary,Vec3 desired){
      std::array<detail::NormalBakeVertex,3> pbr,toon;const auto &f=*frames->pbr;
      for(size_t k=0;k<3;++k){const size_t c=size_t(triangle)*3+k;const auto t=f.tangents[c];const Vec3 T{t[0],t[1],t[2]};
        pbr[k]=detail::normal_bake_vertex(f.normals[c],T,t[3],detail::NormalVertexBitangent::UnitCross);
        toon[k]=detail::normal_bake_vertex(f.normals[c],T,t[3],detail::NormalVertexBitangent::RawCross);
      }
      const std::array<double,3> weights{bary[0],bary[1],bary[2]};
      const auto a=detail::normal_bake_basis(pbr,weights,detail::NormalFragmentBasis::UnitColumns);
      const auto b=mode==PreparedMToonNormalEvaluation::Absent?a:detail::normal_bake_basis(toon,weights,mode==PreparedMToonNormalEvaluation::LegacyRawInterpolated?detail::NormalFragmentBasis::RawColumns:detail::NormalFragmentBasis::UnitCross);
      auto x=detail::encode_normal_bake(a,desired),y=mode==PreparedMToonNormalEvaluation::Absent?x:detail::encode_normal_bake(b,desired);
      if(!x||!y||x->quantized_angle_radians>limit||y->quantized_angle_radians>limit)
        throw std::invalid_argument("Projected normal exceeds the explicit target basis conditioning/quantization contract");
      return std::array<std::array<uint8_t,4>,2>{x->rgba,y->rgba};
    };
  }else low=detail::retained_bake_basis(target,options);
  auto result = bake_mesh(low?*low:target.painted->mesh(), high, options);
  check();
  if(!result.projected)throw std::runtime_error("No target texels project onto the selected source");
  ObjectBakeReport report{result.projected,result.padded,result.rays,result.missed};
  report.color_prepared=converted;
  report.canonical_captured=canonical_captured;
  report.normal_prepared=normal_converted;
  if(prepared_normals)report.normal_initial_error=target.material_bindings->normal_preparation->receipt.observed_initial_error;
  if(target.material_bindings&&target.material_bindings->color_preparation)
    report.color_error_bound=target.material_bindings->color_preparation->projection->maximum_error;
  if(auto prior=target.painted->baked())
    for(size_t i=0;i<result.coverage.size();++i) {
      if((i&1023)==0)check();
      if(result.coverage[i]==3 && prior->coverage[i])++report.preserved_misses;
    }
  BakedTextures textures;
  if(prepared_normals)textures.normal_interpretation=BakedNormalInterpretation::PreparedAttributes;
  if(separate)textures.color_interpretation=BakedColorInterpretation::ResolvedPbr;
  textures.maps = std::move(result.maps);
  textures.maps.normal = std::move(result.normal);
  textures.coverage.reserve(result.coverage.size());
  for (auto c : result.coverage) textures.coverage.push_back(c==1 || c==2 ? 255 : 0);
  auto painting = target.painted->snapshot();
  painting->install_bake(std::move(textures));
  auto next = scene;
  next.active().painted = std::move(painting);
  next.active().material_bindings=target.material_bindings;
  next.active().polygon=target.polygon;
  next.validate();
  check();
  scene = std::move(next);
  if(progress)progress(1);
  return report;
}
} // namespace dome
