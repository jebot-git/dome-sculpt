// SPDX-License-Identifier: GPL-3.0-or-later
// This includes the established FBO/material controls, not its command entry.
#define SOURCE_ARTIST_RENDER_NO_MAIN
#include "source-artist-normal-render.cpp"
#include "dome/normal-preparation.hpp"
namespace prepared_pixel_oracle {
// Frozen independent double-precision decoder from native fixture1740.
using D=std::array<double,3>;
void finite_sample(D a){for(auto v:a)if(!std::isfinite(v))throw std::runtime_error("Independent consumer encountered a nonfinite sample");}
D add(D a,D b){for(size_t k=0;k<3;++k)a[k]+=b[k];return a;}
D mul(D a,double s){for(auto &v:a)v*=s;return a;}
D unit(D a){finite_sample(a);double n=0;for(auto v:a)n+=v*v;if(!std::isfinite(n)||n<1e-20)throw std::runtime_error("Independent singular normal");return mul(a,1/std::sqrt(n));}
D vc(Vec3 a){return {a.x,a.y,a.z};}
D crossd(D a,D b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
D sample(const Image &im,std::array<double,2> uv,bool srgb=false){
 if(!std::isfinite(uv[0])||!std::isfinite(uv[1])||!im.width||!im.height||im.rgba.size()!=size_t(im.width)*im.height*4)throw std::runtime_error("Independent consumer has invalid finite image/UV input");
 const double x=uv[0]*im.width-.5,y=uv[1]*im.height-.5;
 if(!std::isfinite(x)||!std::isfinite(y)||std::abs(x)>1e9||std::abs(y)>1e9)throw std::runtime_error("Independent consumer image coordinate exceeds finite bounds");const int ix=int(std::floor(x)),iy=int(std::floor(y));D out{};
 for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){const auto px=std::clamp(ix+dx,0,int(im.width)-1),py=std::clamp(iy+dy,0,int(im.height)-1);const double w=(dx?x-ix:1-(x-ix))*(dy?y-iy:1-(y-iy));for(size_t c=0;c<3;++c){double value=im.rgba[(size_t(py)*im.width+px)*4+c]/255.;if(srgb)value=value<=.04045?value/12.92:std::pow((value+.055)/1.055,2.4);out[c]+=w*value;}}
 finite_sample(out);return out;
}
D direction(const SculptObject &o,size_t triangle,std::array<float,3> bary,size_t role){
 const auto &binding=*o.material_bindings;const auto &frames=role==0?*binding.normal_frames->pbr:*binding.normal_frames->mtoon;
 D n{},t{},b{};std::array<double,2> uv{};const auto &mesh=o.painted->mesh();
 for(size_t k=0;k<3;++k){const auto c=triangle*3+k;const auto v=mesh.indices[c];const auto nt=unit(vc(frames.normals[c]));const auto tt=frames.tangents[c];const auto tangent=unit(D{tt[0],tt[1],tt[2]});n=add(n,mul(nt,bary[k]));t=add(t,mul(tangent,bary[k]));b=add(b,mul(unit(crossd(nt,tangent)),bary[k]*tt[3]));for(size_t q=0;q<2;++q)uv[q]+=mesh.uv[v][q]*bary[k];}
 const bool legacy=binding.material.version()==MToonVersion::V0;
 if(role==0){n=unit(n);t=unit(t);b=unit(b);}else if(!legacy){n=unit(n);t=unit(t);b=mul(unit(crossd(n,t)),frames.tangents[triangle*3][3]);}
 const auto maps=o.painted->preview(o.material);const auto encoded=sample(role==0?maps->normal:maps->mtoon_normal,uv);const double scale=role==0?1:binding.material.parameters.normal_scale;
 D ts{(encoded[0]*2-1)*scale,(encoded[1]*2-1)*scale,encoded[2]*2-1};
 return unit(add(add(mul(t,ts[0]),mul(b,ts[1])),mul(n,ts[2])));
}
double error(D a,Vec3 b){finite_sample(a);finite_sample(vc(b));double e=0;for(size_t k=0;k<3;++k)e=std::max(e,std::abs(a[k]-vc(b)[k]));return e;}

Vec3 vector(D d){finite_sample(d);return {float(d[0]),float(d[1]),float(d[2])};}
}
namespace {
uint64_t prepared_target(Scene&scene,const Mesh&mesh,bool legacy){
 const auto id=scene.add("Prepared render destination",Volume{});auto&o=scene.object(id);o.mesh_active=true;o.painted=std::make_shared<PaintedMesh>(mesh,64);
 auto binding=std::make_shared<SourceMaterialBindings>();binding->material=typed(legacy);const float g=srgb_to_linear(136.f/255);o.material.color={g,g,g,1};o.material.alpha=Material::AlphaMode::Opaque;o.material.metallic=0;o.material.roughness=1;binding->native_reference.color=o.material.color;binding->material.parameters.base_color=o.material.color;binding->material.parameters.shade_color={g*.25f,g*.3f,g*.35f};binding->material.parameters.toony=0;binding->validate();o.material_bindings=std::move(binding);return id;
}
std::shared_ptr<MeshSnapshot> prepared_point(const SculptObject&o,std::array<float,2>uv,Vec3 desired,Vec3 center,Vec3 x,Vec3 y,float span,bool pbr){
 auto result=snapshot(o.material);result->pbr_view=pbr;result->ranges[0].pbr_view=pbr;result->ranges[0].material_bindings=ordinary_binding(o);
 result->mesh.positions={center-x*span-y*span,center+x*span-y*span,center+x*span+y*span,center-x*span+y*span};result->mesh.normals.assign(4,desired);result->mesh.uv.assign(4,uv);
 // Retain the actual material fields and isolate only the normal consumer.
 // Neither expected direction nor ordinary control has prepared-frame authority.
 auto maps=std::make_shared<TextureMaps>(*o.painted->preview(o.material));maps->normal={};maps->mtoon_normal={};maps->normal_projection.reset();maps->baked_normal={};maps->baked_coverage={};maps->baked_base.reset();maps->overlay.reset();maps->current_grid_preview.reset();maps->polygon_surface.reset();
 // Generic color/emission are material correction inputs even for a fully
 // derived preview. Remove only normal sampling from the independent control.
 if(maps->source_maps){auto sources=std::make_shared<PaintSourceMaps>(*maps->source_maps);sources->normal={};
  // This genuine fixture's generic material is explicitly reprojected into
  // current UV0 by SourceUV. Refuse an alternate coordinate domain rather than
  // mislabel a six-vertex source coordinate array as this four-vertex quad.
  if(sources->uv1||!sources->extra_uv_sets.empty()||sources->uses_alternate_uv())throw std::runtime_error("Normal pixel control requires its explicit current UV0 material domain");
  sources->validate_surface(result->mesh);maps->source_maps=std::move(sources);}
 maps->source_only=false;result->ranges[0].textures=std::move(maps);return result;
}
void prepared_view(Scene scene,uint64_t first,uint64_t second,bool pbr,const std::string&label){
 for(const auto&o:scene.objects())scene.object(o.id).visible=o.id==second;scene.select(second);const auto before=scene.encode();const auto&o=scene.object(second);const auto&mesh=o.painted->mesh();constexpr uint32_t tri=0;std::array<Vec3,3> points;for(size_t k=0;k<3;++k)points[k]=mesh.positions.at(mesh.indices.at(k));
 const auto ex=points[1]-points[0],ey=points[2]-points[0];const auto n=normalized(cross(ex,ey)),x=normalized(ex),y=normalized(cross(n,x)),center=(points[0]+points[1]+points[2])/3;const float span=std::max({length(ex),length(ey),length(points[2]-points[1])})*.85f;const auto matrix=face_projection(center,x,y,n,span);const auto camera=center+n*2;
 require_face_depth(matrix,center,n);require_visible_probes(mesh,tri,center,x,y,n,span);
 Document doc;doc.replace(scene);if(pbr)doc.shading(false);else doc.authored_shading();check(doc.wait_mesh()&&doc.frame().state->error.empty(),"Actual second-Bake target publishes through shared Document renderer route");
 Fbo f;SurfaceRenderer renderer;renderer.set_frame_time(0);f.bind();const auto actual=draw_view(renderer,doc.frame().surface,matrix,camera);retain_pixels(label+"-actual",actual);int maximum=0,wrong_sensitive=0,omitted_sensitive=0,wrong_gap_max=0,omitted_gap_max=0;
 auto omitted_object=scene.object(first);omitted_object.painted=omitted_object.painted->snapshot();omitted_object.painted->clear_bake();
 for(const auto xy:std::array<std::array<int,2>,4>{{{48,48},{45,47},{49,45},{50,50}}}){
  const auto position=center+x*((xy[0]+.5f)/96*2-1)*span+y*((xy[1]+.5f)/96*2-1)*span;const auto r=position-points[0];const double d00=dot(ex,ex),d01=dot(ex,ey),d11=dot(ey,ey),d20=dot(r,ex),d21=dot(r,ey),den=d00*d11-d01*d01;const float v=float((d11*d20-d01*d21)/den),w=float((d00*d21-d01*d20)/den);const std::array<float,3>bary{1-v-w,v,w};check(*std::min_element(bary.begin(),bary.end())>.05f,"Prepared render probe lies strictly inside actual projected triangle");
  std::array<float,2>uv{};for(size_t k=0;k<3;++k)for(size_t c=0;c<2;++c)uv[c]+=mesh.uv[mesh.indices[k]][c]*bary[k];const size_t role=pbr?0:1;
  const auto want=prepared_pixel_oracle::direction(scene.object(first),tri,bary,role);const auto observed=prepared_pixel_oracle::direction(o,tri,bary,role);double err=0;for(size_t k=0;k<3;++k)err=std::max(err,std::abs(want[k]-observed[k]));check(err<.035,"Independent actual second-Bake direction retains unchanged native normal bound");
  const auto expected=prepared_pixel_oracle::vector(want);auto control=prepared_point(o,uv,expected,center,x,y,span,pbr);f.bind();const auto correct_pixels=draw_view(renderer,control,matrix,camera);const auto correct=at(correct_pixels,xy[0],xy[1]),got=at(actual,xy[0],xy[1]);const auto prefix=label+"-"+std::to_string(xy[0])+"-"+std::to_string(xy[1]);retain_pixels(prefix+"-correct",correct_pixels);
  auto wrong=std::make_shared<MeshSnapshot>(*control);wrong->mesh.normals.assign(4,{0,0,1});f.bind();const auto wrong_pixels=draw_view(renderer,wrong,matrix,camera);const auto bad=at(wrong_pixels,xy[0],xy[1]);retain_pixels(prefix+"-wrong-worldZ",wrong_pixels);
  auto omitted=std::make_shared<MeshSnapshot>(*control);omitted->mesh.normals.assign(4,prepared_pixel_oracle::vector(prepared_pixel_oracle::direction(omitted_object,tri,bary,role)));f.bind();const auto omitted_pixels=draw_view(renderer,omitted,matrix,camera);const auto absent=at(omitted_pixels,xy[0],xy[1]);retain_pixels(prefix+"-omitted-source",omitted_pixels);
  int e=0,bg=0,og=0;for(size_t k=0;k<3;++k){e=std::max(e,std::abs(got[k]-correct[k]));bg=std::max(bg,std::abs(correct[k]-bad[k]));og=std::max(og,std::abs(correct[k]-absent[k]));}maximum=std::max(maximum,e);wrong_gap_max=std::max(wrong_gap_max,bg);omitted_gap_max=std::max(omitted_gap_max,og);wrong_sensitive+=bg>=7&&!close(got,bad,3)&&!close(correct,bad,3);omitted_sensitive+=og>=7&&!close(got,absent,3)&&!close(correct,absent,3);
  std::cout<<"PREPARED_RENDER_POINT label="<<label<<" xy="<<xy[0]<<','<<xy[1]<<" actual="<<got[0]<<','<<got[1]<<','<<got[2]<<" correct="<<correct[0]<<','<<correct[1]<<','<<correct[2]<<" wrong="<<bad[0]<<','<<bad[1]<<','<<bad[2]<<" omitted="<<absent[0]<<','<<absent[1]<<','<<absent[2]<<" error="<<e<<" wrong_gap="<<bg<<" omitted_gap="<<og<<std::endl;
  check(close(got,correct,3),"Actual prepared-source second-Bake RGB agrees with independent ordinary normal consumer at unchanged bound3");
 }
 std::cout<<"PREPARED_RENDER_CASE label="<<label<<" correct_max="<<maximum<<" wrong_gap_max="<<wrong_gap_max<<" omitted_gap_max="<<omitted_gap_max<<" wrong_disjoint="<<wrong_sensitive<<" omitted_disjoint="<<omitted_sensitive<<std::endl;
 check(wrong_sensitive>0&&omitted_sensitive>0,"Wrong-world and omitted-source controls have disjoint RGB intervals and fail unchanged bound3");check(doc.copy_scene().encode()==before&&scene.encode()==before,"Prepared pixel controls preserve exact source, target, maps and native recipe");check(glGetError()==GL_NO_ERROR,"Actual prepared target and independent controls leave GL error queue empty");
}
void prepared_render(){const auto normal=sensitive_normal();for(bool legacy:{false,true})for(bool posed:{false,true}){
 auto f=source_artist_fixture::fixture(legacy,posed,&normal);tune_sensitive(f.scene,f.source);const auto mesh=f.scene.object(f.source).deformation->preview(f.scene.object(f.source).painted->mesh());ObjectBakeSettings options;options.prepare_normals=true;options.normal_error=.03f;options.front_mm=options.back_mm=300;options.ao_samples=0;options.padding=0;
 const auto first=prepared_target(f.scene,mesh,legacy);f.scene.select(first);check(bake_object(f.scene,f.source,options).projected>0&&f.scene.object(first).painted->normal_projection(),"First real artist Bake produces genuine prepared normal source");const auto owner=f.scene.object(first).painted;const auto binding=f.scene.object(first).material_bindings;const auto source_data=f.scene.encode().at("objects");
 const auto second=prepared_target(f.scene,mesh,legacy);f.scene.select(second);check(bake_object(f.scene,first,options).projected>0&&f.scene.object(second).painted->baked(),"Genuine prepared source is consumed by actual second Bake for pixel proof");check(f.scene.object(first).painted==owner&&f.scene.object(first).material_bindings==binding,"Second pixel Bake retains exact prepared-source immutable owners");auto retained=f.scene.snapshot();retained.erase(second);check(retained.encode().at("objects")==source_data,"Second pixel Bake preserves exact complete original and prepared-source objects");check(Scene::decode(f.scene.encode()).encode()==f.scene.encode(),"Actual prepared-source second-Bake render input independently roundtrips native archive");
 const auto label=std::string("prepared")+(legacy?"MT0":"MT1")+(posed?"posed":"rest");prepared_view(f.scene,first,second,false,label);prepared_view(f.scene,first,second,true,label+"PBR");
 }}
}
#include "source-advanced-normal-render.hpp"
#include "source-advanced-normal-display.hpp"
#include "source-advanced-normal-forecast.hpp"
#include "source-advanced-normal-diagnose.hpp"
#include "source-advanced-normal-resolution-study.hpp"
#include "source-advanced-normal-resolution-study.hpp"
int main(int argc,char**argv)try{if(argc==3&&std::string_view(argv[1])=="--advanced-resolution-study")return advanced_render::resolution_study::run(argv[2]);if(argc==3&&std::string_view(argv[1])=="--advanced-resolution-study")return advanced_render::resolution_study::run(argv[2]);if(argc==3&&std::string_view(argv[1])=="--advanced-display-diagnose")return advanced_render::diagnose::run(argv[2]);if(argc==4&&std::string_view(argv[1])=="--advanced-display-render")return advanced_render::display::execute(argv[2],argv[3]);if(argc==3&&std::string_view(argv[1])=="--advanced-display-forecast")return advanced_render::forecast::run(argv[2],true);if(argc==3&&std::string_view(argv[1])=="--advanced-contrast-forecast")return advanced_render::forecast::run(argv[2]);if(argc==3&&std::string_view(argv[1])=="--advanced-render")return advanced_render::execute(argv[2]);if(argc!=3||std::string_view(argv[1])!="--prepared-render")throw std::runtime_error("Prepared graphics requires explicit coordinated mode and output directory");graphics_output=argv[2];NativeGraphics graphics;std::cout<<"ACTUAL_RENDERER "<<glGetString(GL_RENDERER)<<std::endl;prepared_render();std::cout<<"PASS "<<checks<<" actual prepared first-to-second Bake pixel controls\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
