// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-artist-normal-fixture.hpp"
#include "emission-bake-source.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/normal-sampling.hpp"
#include "dome/source-uv-bake.hpp"
using namespace source_artist_fixture;
namespace {
// Independent double-precision consumer: no production basis, sampler or
// source-normal resolver is used to derive the expected directions.
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
std::shared_ptr<const SourceMaterialBindings> blank_typed(bool legacy){
 auto b=std::make_shared<SourceMaterialBindings>();b->material=legacy?MToonMaterial::from_vrm0({{"shader","VRM/MToon"}}):MToonMaterial::from_vrm1({{"extensions",{{"VRMC_materials_mtoon",{{"specVersion","1.0"}}}}}});b->validate();return b;
}
uint64_t target(Scene &scene,const Mesh &mesh,bool legacy){
 const auto id=scene.add("Independent prepared destination",Volume{});auto &o=scene.object(id);o.mesh_active=true;o.painted=std::make_shared<PaintedMesh>(mesh,64);o.material_bindings=blank_typed(legacy);return id;
}

void emission_guards(const Scene &baseline,uint64_t source,const ObjectBakeSettings &options,const std::filesystem::path &folder){
 for(bool hdr:{false,true})for(bool genuine:{false,true}){
  auto scene=baseline.snapshot();auto &o=scene.object(source);
  std::shared_ptr<PaintedMesh> input;
  auto image=std::make_shared<const Image>(Image{2,2,{255,0,0,255,0,0,255,255,255,0,0,255,0,0,255,255}});
  if(genuine){auto maps=std::make_shared<PaintSourceMaps>();maps->emission.image=image;maps->emission.texcoord=1;auto logical=o.painted->mesh().uv;for(auto &q:logical)q[0]=1-q[0];maps->uv1=std::make_shared<const std::vector<std::array<float,2>>>(std::move(logical));input=capture_source_sampling(o.painted->mesh(),maps,64);}
  else input=std::make_shared<PaintedMesh>(o.painted->mesh(),64);
  NormalPreparationOptions preparation;preparation.max_angular_error=.03;
  auto prepared=prepare_source_normals({input,{ },blank_typed(false),{},o.material},preparation);
  o.painted=std::move(prepared.paint);auto binding=std::make_shared<SourceMaterialBindings>(*prepared.material_bindings);
  o.material.emissive=hdr?std::array<float,3>{2,1,4}:std::array<float,3>{.5f,.25f,1};binding->native_reference.emission=o.material.emissive;
  binding->material.parameters.emission={.5f,.25f,1};binding->material.parameters.emission_strength=hdr?4:1;
  auto &slot=binding->material.textures[size_t(MToonSlot::Emission)];slot.index=0;slot.texcoord=1;slot.encoding=MToonEncoding::Srgb;
  auto &role=binding->roles[size_t(MToonSlot::Emission)];role.image=image;role.min_filter=role.mag_filter=9729;
  o.material_bindings=binding;scene.validate();const auto before=scene.encode();
  check(Scene::decode(before).encode()==before&&o.painted->normal_projection()&&!o.painted->source_sampling_prepared(),"Emission guard fixture owns genuine ordinary prepared normals and roundtrips its editable missing/present logical UV1 state");
  if(genuine){
   const std::array<float,3> bary{.2f,.3f,.5f};std::array<double,2> uv{};
   const auto &mesh=o.painted->mesh();for(size_t k=0;k<3;++k)for(size_t c=0;c<2;++c)uv[c]+=mesh.uv[mesh.indices[k]][c]*bary[k];
   if(!std::isfinite(uv[0])||!std::isfinite(uv[1]))throw std::runtime_error("Independent emission coordinates are nonfinite");
   const double weight=std::clamp(2*(1-uv[0])-.5,0.,1.);const double wrong_weight=std::clamp(2*uv[0]-.5,0.,1.);
   const D wanted{(hdr?2:.5)*(1-weight),0,(hdr?4:1)*weight};
   const D wrong{(hdr?2:.5)*(1-wrong_weight),0,(hdr?4:1)*wrong_weight};
   finite_sample(wanted);finite_sample(wrong);
   double gap=0;for(size_t c=0;c<3;++c)gap=std::max(gap,std::abs(wanted[c]-wrong[c]));
   check(gap>.1,"Independent emission UV1 positive differs visibly from current writable UV0");
   const auto callback=detail::emission_bake_source(o);
   if(hdr){check(bool(callback),"Prepared HDR genuine UV1 retains its actual FLOAT callback");const auto value=callback(0,bary);finite_sample({value[0],value[1],value[2]});double e=0;for(size_t c=0;c<3;++c)e=std::max(e,std::abs(wanted[c]-value[c]));check(e<3e-6,"Independent HDR emitter oracle proves original genuine UV1 rather than writable UV0 sampling");}
   const auto raw=detail::raw_bake_source(o);check(bool(raw),"Prepared normal source retains genuine authored emission UV1 in raw capture");
   if(!hdr){check(raw->source_only&&raw->normal_projection&&raw->overlay&&raw->source_maps&&raw->source_maps->emission.texcoord==1&&raw->source_maps->uv1,"Prepared LDR source retains actual independent raw material and normal owners");std::array<double,2> logical{};for(size_t k=0;k<3;++k)for(size_t c=0;c<2;++c)logical[c]+=raw->source_maps->uv1->at(mesh.indices[k])[c]*bary[k];auto value=sample(*raw->source_maps->emission.image,logical,true);for(size_t c=0;c<3;++c)value[c]*=o.material.emissive[c];finite_sample(value);double e=0;for(size_t c=0;c<3;++c)e=std::max(e,std::abs(wanted[c]-value[c]));check(e<3e-6,"Independent LDR raw-role decoder proves genuine UV1 transport before actual Bake");}
   check(bake_object(scene,source,options).projected>0,"Prepared normal source with genuine emission UV1 remains usable for actual Bake");
   const auto &baked=*scene.active().painted->baked();D actual{};
   if(hdr){check(bool(baked.emission_field),"Prepared HDR repeated Bake retains actual FLOAT radiance field");const auto radiance=baked.emission_field->sample(float(uv[0]),float(uv[1])).resolve({},1);actual={radiance[0],radiance[1],radiance[2]};}
   else actual=sample(baked.maps.emission,uv,true);
   finite_sample(actual);finite_sample(wanted);double e=0;for(size_t c=0;c<3;++c)e=std::max(e,std::abs(wanted[c]-actual[c]));
   check(e<(hdr?2e-5:.012),"Independent actual-Bake emission consumer preserves genuine original UV1 radiance for both LDR and HDR");continue;
  }
  check(refuses([&]{detail::emission_bake_source(o);}),"Prepared missing logical emission UV1 refuses emitter before HDR/LDR selection");
  check(refuses([&]{detail::raw_bake_source(o);}),"Prepared missing logical emission UV1 refuses raw capture before its normal-preview early return");
  check(refuses([&]{bake_object(scene,source,options);})&&scene.encode()==before,"Actual prepared-source Bake refuses missing logical emission UV1 without source/target mutation");
  const auto path=folder/(hdr?"prepared-hdr-prior.dsculpt":"prepared-ldr-prior.dsculpt");const std::string previous="Keep exact prior save bytes\n";{std::ofstream f(path,std::ios::binary);f<<previous;}
  Document doc;doc.replace(scene.snapshot());check(doc.wait_mesh(),"Prepared missing-UV1 Scene publishes before Bake");const auto initial=doc.frame().state;bool saved=true;
  try{doc.bake_from_object(source,options);auto result=doc.save_async(path);result.get();}catch(const std::exception&){saved=false;}
  std::ifstream file(path,std::ios::binary);const std::string bytes{std::istreambuf_iterator<char>(file),{}};
  check(doc.wait_asset(std::chrono::seconds(120))&&doc.wait_mesh()&&!saved&&bytes==previous&&doc.copy_scene().encode()==before&&!doc.frame().state->can_undo&&!doc.frame().state->can_redo&&doc.frame().state->project_path==initial->project_path,"Prepared missing-UV1 worker and dependent Save preserve exact data, prior bytes, path and history in both LDR and HDR");
 }
}

void advanced_recipe_guards(const Scene &baseline,uint64_t source,const ObjectBakeSettings &options){
 for(const auto method:{SourceNormalUvMethod::Rasterize,SourceNormalUvMethod::RetainSource}){
  auto scene=baseline.snapshot();const auto &o=scene.object(source);SourceUvBakeInput input;input.paint=o.painted;input.current_material=o.material_bindings;input.current_deformation=o.deformation;input.appearance=o.appearance;input.target=o.painted->mesh();input.source_vertices.resize(input.target.positions.size());std::iota(input.source_vertices.begin(),input.source_vertices.end(),0);
  for(auto &uv:input.target.uv)for(auto &v:uv)v=.5f+(v-.5f)*.95f;
  SourceUvBakeOptions settings;settings.normal_sampling=method;settings.max_normal_sample_error=.03;
  auto adopted=adopt_source_uv_bake(bake_source_uv(input,settings));auto &current=scene.object(source);current.painted=adopted.paint;current.material_bindings=adopted.material_bindings;current.deformation=adopted.deformation;current.appearance=adopted.appearance;scene.validate();const auto before=scene.encode();
  check(Scene::decode(before).encode()==before&&current.material_bindings->normal_preparation&&(current.material_bindings->normal_preparation->uv_reprojection||current.material_bindings->normal_preparation->sampling_domain),"Advanced negative owns a genuine native UV-reprojected or retained-domain normal recipe");
  check(refuses([&]{detail::source_bake_normals(current,{});})&&refuses([&]{detail::raw_bake_source(current);})&&refuses([&]{bake_object(scene,source,options);})&&scene.encode()==before,"Advanced normal/material UV recipes remain explicitly refused before sampling with exact Scene/source/target preservation");
 }
}

void run_case(bool legacy,bool posed,const std::filesystem::path &folder){
 auto f=fixture(legacy,posed);auto mesh=f.scene.object(f.source).deformation->preview(f.scene.object(f.source).painted->mesh());
 const auto first=target(f.scene,mesh,legacy);ObjectBakeSettings options;options.prepare_normals=true;options.normal_error=.03f;options.front_mm=options.back_mm=300;options.ao_samples=0;options.padding=0;
 const auto initial=bake_object(f.scene,f.source,options);check(initial.projected&&f.scene.object(first).painted->normal_projection(),"First genuine artist Bake produces a prepared source with explicit current normal planes");
 const auto before=f.scene.encode();const auto prepared=f.scene.object(first).painted;const auto binding=f.scene.object(first).material_bindings;
 const auto resolver=detail::source_bake_normals(f.scene.object(first),{});check(bool(resolver),"Prepared source exposes its actual owned frame/coordinate resolver");
 {BakeSource raw;raw.mesh=mesh;raw.materials[0].maps=detail::raw_bake_source(f.scene.object(first));BakeOptions naked;naked.resolution=64;naked.ao_samples=0;naked.padding=0;check(refuses([&]{bake_mesh(mesh,raw,naked);})&&f.scene.encode()==before,"Public legacy Bake cannot consume prepared raw material without the authenticated separate normal resolver");}
 double maximum=0,wrong=0;for(size_t tri=0;tri<mesh.indices.size()/3;++tri)for(auto bary:std::array<std::array<float,3>,4>{{{.2f,.3f,.5f},{.7f,.1f,.2f},{.1f,.7f,.2f},{.33333334f,.33333334f,.33333334f}}}){const auto actual=resolver(uint32_t(tri),bary);for(size_t role=0;role<2;++role){const auto expected=direction(f.scene.object(first),tri,bary,role);maximum=std::max(maximum,error(expected,actual[role]));wrong=std::max(wrong,std::abs(expected[0]));}}
 check(maximum<3e-6&&wrong>.02,"Independent double-precision prepared-image consumer verifies both material normal roles and rejects a neutral-normal substitute");
 detail::SourceBakeNormalOptions limited;limited.max_working_bytes=1;check(refuses([&]{detail::source_bake_normals(f.scene.object(first),{},limited);})&&f.scene.encode()==before,"Prepared-source normal memory refusal preserves exact source and immutable maps");
 size_t polls=0;limited={};limited.cancelled=[&]{++polls;return false;};(void)detail::source_bake_normals(f.scene.object(first),{},limited);
 bool cancelled=true;for(size_t point=1;point<=polls;++point){size_t at=0;limited.cancelled=[&]{return ++at>=point;};cancelled&=refuses([&]{detail::source_bake_normals(f.scene.object(first),{},limited);})&&f.scene.encode()==before;}
 check(cancelled&&polls>1,"Every prepared-source capture cancellation boundary preserves source, recipe and history");
 const auto second=target(f.scene,mesh,legacy);f.scene.select(second);const auto original=f.scene.encode();const auto native_before=f.scene.snapshot();
 if(!legacy&&posed){emission_guards(native_before,first,options,folder);advanced_recipe_guards(native_before,first,options);}
 const auto report=bake_object(f.scene,first,options);check(report.projected&&f.scene.object(second).painted->baked(),"A real prepared Bake result is reusable as the source of a second Bake");
 check(f.scene.object(first).painted==prepared&&f.scene.object(first).material_bindings==binding,"Second Bake retains the exact immutable prepared source owners");
 double baked_error=0,neutral_error=0,omitted_error=0,wrong_role_error=0;size_t interior=0;
 for(size_t y=0;y<64;++y)for(size_t x=0;x<64;++x){
  const std::array<double,2> q{(x+.5)/64.,(y+.5)/64.};
  for(size_t tri=0;tri<mesh.indices.size()/3;++tri){
   const auto a=mesh.uv[mesh.indices[tri*3]],b=mesh.uv[mesh.indices[tri*3+1]],c=mesh.uv[mesh.indices[tri*3+2]];
   const double d=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1]);
   const double u=((b[1]-c[1])*(q[0]-c[0])+(c[0]-b[0])*(q[1]-c[1]))/d;
   const double v=((c[1]-a[1])*(q[0]-c[0])+(a[0]-c[0])*(q[1]-c[1]))/d;
   if(std::min({u,v,1-u-v})<.06)continue;
   const std::array<float,3> bary{float(u),float(v),float(1-u-v)};
   const auto &source_object=f.scene.object(first);D geometric{};
   for(size_t k=0;k<3;++k)geometric=add(geometric,mul(vc(source_object.painted->mesh().normals[source_object.painted->mesh().indices[tri*3+k]]),bary[k]));
   geometric=unit(geometric);const std::array<D,2> wanted{direction(source_object,tri,bary,0),direction(source_object,tri,bary,1)};
   for(size_t role=0;role<2;++role){const auto actual=direction(f.scene.object(second),tri,bary,role);for(size_t k=0;k<3;++k){baked_error=std::max(baked_error,std::abs(wanted[role][k]-actual[k]));neutral_error=std::max(neutral_error,std::abs(wanted[role][k]-(k==2?1.:0.)));omitted_error=std::max(omitted_error,std::abs(wanted[role][k]-geometric[k]));wrong_role_error=std::max(wrong_role_error,std::abs(wanted[role][k]-wanted[1-role][k]));}}
   ++interior;break;
  }
 }
 check(neutral_error>.07&&omitted_error>.07&&wrong_role_error>.07,"Same actual interior texels independently reject neutral, omitted and swapped material-role source directions with margin above the unchanged 0.035 Bake tolerance");
 check(interior>500&&baked_error<.035,"Independent decoding of actual second-Bake texels preserves both source directions within declared quantization tolerance");

 const auto encoded=f.scene.encode();check(Scene::decode(encoded).encode()==encoded,"Repeated prepared-source Bake roundtrips through the native archive exactly");
 Document doc;doc.replace(native_before);check(doc.wait_mesh(),"Prepared-source Document reaches its genuine initial geometry");doc.bake_from_object(first,options);const auto out=folder/(std::to_string(legacy)+std::to_string(posed)+".dsculpt");auto saved=doc.save_async(out);saved.get();check(doc.wait_asset(std::chrono::seconds(120))&&doc.wait_mesh()&&doc.frame().state->error.empty()&&doc.copy_scene().encode()==encoded&&Scene::load(out).encode()==encoded,"Native prepared-source Bake worker and immediately ordered Save publish exact second-Bake data");
 check(doc.undo()&&doc.wait_mesh()&&doc.copy_scene().encode()==original,"Second prepared-source Bake Undo restores the complete prior Scene");check(doc.redo()&&doc.wait_mesh()&&doc.copy_scene().encode()==encoded,"Second prepared-source Bake Redo restores exact current role planes");
 std::cout<<"PREPARED legacy="<<legacy<<" posed="<<posed<<" max_direction_error="<<maximum<<" baked_error="<<baked_error<<" neutral_control_error="<<neutral_error<<" omitted_control_error="<<omitted_error<<" wrong_role_control_error="<<wrong_role_error<<" decoded_texels="<<interior<<" projected="<<report.projected<<" polls="<<polls<<'\n';
}
}
int main(int argc,char **argv)try{if(argc!=2)return 2;std::filesystem::create_directories(argv[1]);for(bool legacy:{false,true})for(bool posed:{false,true})run_case(legacy,posed,argv[1]);std::cout<<"PASS "<<checks<<" prepared source controls\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
