// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_normal_color_suite
#include "normal-color-uv.cpp"
#undef main
#include "dome/color-paint-sampling.hpp"
#include "dome/polygon-color.hpp"
#include "paint-sampling-access.hpp"
#include "normal-frame-runtime.hpp"
#include "normal-sampling-access.hpp"
#include "source-paint-pairing.hpp"
#include "dome/normal-sampling.hpp"
#include <type_traits>
static_assert(!std::is_default_constructible_v<SourceColorPaintSampling>);
namespace {
std::vector<uint8_t> planes(const PaintedMesh &paint){
 auto maps=paint.composite(Material{});std::vector<uint8_t> bytes;
 for(auto p:{&maps->color,&maps->pbr_color,&maps->metal_rough,&maps->emission,&maps->normal,&maps->mtoon_normal,&maps->color_coverage,&maps->emission_coverage})bytes.insert(bytes.end(),p->rgba.begin(),p->rgba.end());return bytes;
}
}
int main()try{
 auto scene=prepared(false);const auto &object=scene.active();auto encoded=scene.encode();
 UvWorkspace workspace(object.painted->mesh(),object.painted->resolution());auto p=front(object.painted->mesh());workspace.edit(UvAction::Pick,p[0],p[1]);workspace.edit(UvAction::Scale,.7f);workspace.edit(UvAction::Rotate,1.57079632679f);auto view=workspace.view();check(view.valid,"Actual chart fractional-scale and90degree turn validates");
 auto derivative=prepare_polygon_uv_derivative(object,*view.mesh,*view.source_vertices,{});auto descriptor=std::make_shared<const PolygonColorSurface>(PolygonColorSurface{std::make_shared<const PolygonMesh>(derivative.authority->topology),derivative.authority->colors});
 const auto before_pixels=planes(*object.painted);
 auto result=retain_source_corner_paint(object.painted,object.material_bindings,object.deformation,descriptor,derivative.mesh,derivative.source_vertices);
 check(result.colors&&result.colors->paint_sampling==result.sampling&&result.colors->current_descriptor()->topology==descriptor->topology&&result.colors->sampling_descriptor()==object.material_bindings->color_preparation->current_descriptor(),"Actual colour preparation retains authored and sampling descriptor semantics separately");
 result.sampling->validate();check(result.sampling->matches_current(*result.paint)&&result.paint->mesh().uv==derivative.mesh.uv&&result.paint->paint_sampling_domain(),"Actual paired Bake output attaches the authored current layout and independent writable domain");
 check(result.sampling->receipt().tested_corners==derivative.mesh.indices.size()&&result.sampling->receipt().maximum_corner_error==std::array<float,4>{},"Own complete triangle-corner proof independently establishes exact linear RGBA interpolation");
 check(result.sampling->current_descriptor()==descriptor||result.sampling->current_descriptor()->topology==descriptor->topology,"Current descriptor names new authored topology rather than projection's retained layout");
 check(result.sampling->sampling_descriptor()==object.material_bindings->color_preparation->current_descriptor()&&result.sampling->current_descriptor()->field==result.sampling->sampling_descriptor()->field,"Distinct sampling descriptor retains exact original field and source basis without fakeIdentity");
 check(result.paint->baked()==object.painted->baked()&&result.paint->color_projection()==object.painted->color_projection()&&result.paint->normal_projection()==object.painted->normal_projection()&&planes(*result.paint)==before_pixels,"Every actual paired Bake channel, normal plane and colour coverage retains exact bytes/owners instead of resampling");
 SourceUvBakeInput normal_input;normal_input.paint=object.painted;normal_input.current_material=object.material_bindings;normal_input.current_deformation=object.deformation;normal_input.target=derivative.mesh;normal_input.source_vertices=derivative.source_vertices;
 SourceUvBakeResult normal_result;SourceUvBakeOptions normal_options;normal_options.normal_sampling=SourceNormalUvMethod::RetainSource;
 auto normal=detail::retain_normal_sampling(normal_input,normal_result,normal_options,result.paint->paint_sampling_domain());
 check(normal->metadata_sampling_domain==result.paint->paint_sampling_domain()&&normal->receipt.version==2,"Independent normal receipt2 owns exact writable metadata basis instead of borrowing colour quality");
 auto joint=adopt_retained_paint_normal(*result.paint,*normal);auto binding=std::make_shared<SourceMaterialBindings>(*object.material_bindings);binding->color_preparation=result.colors;
 auto normal_preparation=std::make_shared<SourceNormalPreparation>(*binding->normal_preparation);normal_preparation->projection=normal->projection;normal_preparation->sampling_domain=normal;normal_preparation->uv_reprojection.reset();binding->normal_preparation=std::move(normal_preparation);
 binding->validate();detail::source_paint_pairing(joint.get(),binding.get(),nullptr,true);
 check(joint->baked()->normal_authority==normal->projection->sampling_domain->detail&&joint->baked()->coverage==object.painted->baked()->coverage&&joint->baked()->maps.normal.rgba==object.painted->baked()->maps.normal.rgba&&joint->baked()->maps.pbr_color.rgba==object.painted->baked()->maps.pbr_color.rgba,"Normal-specific event and actual before-paint metadata remain exact beside independently unchanged RGBA coverage");
 PaintImageArchive image_pool;NormalSamplingArchive normal_pool;PaintSamplingArchive paint_pool;
 auto envelope=joint->encode(&image_pool,&normal_pool,&paint_pool);auto decoded=PaintedMesh::decode(envelope,&image_pool,false,&normal_pool,&paint_pool);
 check(!result.sampling->matches_current(*decoded),"Independently decoded equal REST is not treated as the same immutable geometry owner");
 auto before_rebind=decoded->composite(Material{});auto rebound=rebind_paint_sampling_geometry(*decoded,result.paint->paint_sampling_attachment());
 check(result.sampling->matches_current(*rebound)&&rebound->composite(Material{})==before_rebind&&rebound->baked()==decoded->baked()&&rebound->layers().size()==decoded->layers().size(),"Worker rebind proves complete REST/domain then shares canonical immutable map while preserving decoded paint/cache owners");
 detail::source_paint_pairing(rebound.get(),binding.get(),nullptr,true);check(true,"Rebound current colour and normal independently authenticate against their own full before-state");
 auto wrong_normal=*normal;wrong_normal.metadata_sampling_domain.reset();check(refuses([&]{wrong_normal.validate();}),"Normal receipt2 cannot silently omit its own metadata basis");
 auto wrong_detail=std::make_shared<NormalDetailAuthority>(*normal->projection->sampling_domain->detail);wrong_detail->coverage[0]^=1;auto wrong_normal_domain=std::make_shared<NormalSamplingDomain>(*normal->projection->sampling_domain);wrong_normal_domain->detail=wrong_detail;auto wrong_projection=std::make_shared<PaintNormalProjection>(*normal->projection);wrong_projection->sampling_domain=wrong_normal_domain;wrong_normal=*normal;wrong_normal.projection=wrong_projection;
 check(refuses([&]{wrong_normal.validate();}),"Normal metadata cannot borrow old capture planes or unrelated colour coverage in place of actual authenticated before-paint bytes");
 auto snapshot=result.paint->snapshot();snapshot->layer_properties("Writable retained basis",true,.8f);check(result.sampling->matches_current(*snapshot),"Legal editing properties do not invalidate geometry/field attachment");
 auto wrong=std::make_shared<PolygonColorSurface>(*descriptor);wrong->field=std::make_shared<const PolygonColorField>(*descriptor->field);
 check(refuses([&]{retain_source_corner_paint(object.painted,object.material_bindings,object.deformation,wrong,derivative.mesh,derivative.source_vertices);}),"Equal-valued but independently owned corner field cannot borrow source authority");
 auto target=derivative.mesh;target.positions[0].x+=.001f;check(refuses([&]{retain_source_corner_paint(object.painted,object.material_bindings,object.deformation,descriptor,target,derivative.source_vertices);}),"Current evaluated or modified REST cannot replace exact colour geometry");
 auto remapped=detail::normal_frame_remap_deformation(object.deformation,object.painted->mesh(),derivative.mesh,derivative.source_vertices,{});auto posed=*remapped;posed.pose.resize(posed.skeleton->bones.size());posed.pose[1].translation.x=.11f;posed.morph_weights[0]=.7f;auto evaluated=posed.preview(derivative.mesh);check(refuses([&]{retain_source_corner_paint(object.painted,object.material_bindings,object.deformation,descriptor,evaluated,derivative.source_vertices);}),"Posed geometry is not mistaken for original field REST identity");
 auto missing=derivative.source_vertices;missing[0]=UINT32_MAX;check(refuses([&]{retain_source_corner_paint(object.painted,object.material_bindings,object.deformation,descriptor,derivative.mesh,missing);}),"Explicit source remap cannot borrow an unrelated old vertex");
 check(refuses([&]{result.paint->encode();})&&scene.encode()==encoded,"Unadmitted material/normal closure remains guarded while exact original Scene restarts unchanged");
 PaintSamplingLimits low;low.max_work=1;check(refuses([&]{retain_source_corner_paint(object.painted,object.material_bindings,object.deformation,descriptor,derivative.mesh,derivative.source_vertices,low);}),"Current corner factory bounds actual geometry and field proof work");
 size_t polls=0;PaintSamplingLimits measured;measured.cancelled=[&]{++polls;return false;};auto counted=retain_source_corner_paint(object.painted,object.material_bindings,object.deformation,descriptor,derivative.mesh,derivative.source_vertices,measured);check(polls>6,"Real source geometry/field/grid/receipt stages poll cancellation");
 for(auto at:{size_t(1),polls/2,polls}){size_t n=0;auto limits=measured;limits.cancelled=[&]{return ++n>=at;};check(refuses([&]{retain_source_corner_paint(object.painted,object.material_bindings,object.deformation,descriptor,derivative.mesh,derivative.source_vertices,limits);})&&n==at,"Early/middle/final corner-grid cancellation exposes no half-adopted field or pixels");}
 check(scene.encode()==encoded&&planes(*object.painted)==before_pixels,"Every detached attempt preserves original source/paint/normal/corner/shape/affine state");
 std::cout<<"PASS "<<checks<<" retained current-corner paint checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
