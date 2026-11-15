// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-artist-normal-fixture.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/normal-sampling.hpp"
using namespace source_artist_fixture;
namespace {
// Independent double-precision consumer: no production basis, sampler or
// source-normal resolver is used to derive the expected directions.
using D=std::array<double,3>;
D add(D a,D b){for(size_t k=0;k<3;++k)a[k]+=b[k];return a;}
D mul(D a,double s){for(auto &v:a)v*=s;return a;}
D unit(D a){double n=0;for(auto v:a)n+=v*v;if(n<1e-20)throw std::runtime_error("Independent singular normal");return mul(a,1/std::sqrt(n));}
D vc(Vec3 a){return {a.x,a.y,a.z};}
D crossd(D a,D b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
D sample(const Image &im,std::array<double,2> uv){
 const double x=uv[0]*im.width-.5,y=uv[1]*im.height-.5;const int ix=int(std::floor(x)),iy=int(std::floor(y));D out{};
 for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){const auto px=std::clamp(ix+dx,0,int(im.width)-1),py=std::clamp(iy+dy,0,int(im.height)-1);const double w=(dx?x-ix:1-(x-ix))*(dy?y-iy:1-(y-iy));for(size_t c=0;c<3;++c)out[c]+=w*im.rgba[(size_t(py)*im.width+px)*4+c]/255.;}
 return out;
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
double error(D a,Vec3 b){double e=0;for(size_t k=0;k<3;++k)e=std::max(e,std::abs(a[k]-vc(b)[k]));return e;}
std::shared_ptr<const SourceMaterialBindings> blank_typed(bool legacy){
 auto b=std::make_shared<SourceMaterialBindings>();b->material=legacy?MToonMaterial::from_vrm0({{"shader","VRM/MToon"}}):MToonMaterial::from_vrm1({{"extensions",{{"VRMC_materials_mtoon",{{"specVersion","1.0"}}}}}});b->validate();return b;
}
uint64_t target(Scene &scene,const Mesh &mesh,bool legacy){
 const auto id=scene.add("Independent prepared destination",Volume{});auto &o=scene.object(id);o.mesh_active=true;o.painted=std::make_shared<PaintedMesh>(mesh,64);o.material_bindings=blank_typed(legacy);return id;
}
void run_case(bool legacy,bool posed,const std::filesystem::path &folder){
 auto f=fixture(legacy,posed);auto mesh=f.scene.object(f.source).deformation->preview(f.scene.object(f.source).painted->mesh());
 const auto first=target(f.scene,mesh,legacy);ObjectBakeSettings options;options.prepare_normals=true;options.normal_error=.03f;options.front_mm=options.back_mm=300;options.ao_samples=0;options.padding=0;
 const auto initial=bake_object(f.scene,f.source,options);check(initial.projected&&f.scene.object(first).painted->normal_projection(),"First genuine artist Bake produces a prepared source with explicit current normal planes");
 const auto before=f.scene.encode();const auto prepared=f.scene.object(first).painted;const auto binding=f.scene.object(first).material_bindings;
 const auto resolver=detail::source_bake_normals(f.scene.object(first),{});check(bool(resolver),"Prepared source exposes its actual owned frame/coordinate resolver");
 double maximum=0,wrong=0;for(size_t tri=0;tri<mesh.indices.size()/3;++tri)for(auto bary:std::array<std::array<float,3>,4>{{{.2f,.3f,.5f},{.7f,.1f,.2f},{.1f,.7f,.2f},{.33333334f,.33333334f,.33333334f}}}){const auto actual=resolver(uint32_t(tri),bary);for(size_t role=0;role<2;++role){const auto expected=direction(f.scene.object(first),tri,bary,role);maximum=std::max(maximum,error(expected,actual[role]));wrong=std::max(wrong,std::abs(expected[0]));}}
 check(maximum<3e-6&&wrong>.02,"Independent double-precision prepared-image consumer verifies both material normal roles and rejects a neutral-normal substitute");
 detail::SourceBakeNormalOptions limited;limited.max_working_bytes=1;check(refuses([&]{detail::source_bake_normals(f.scene.object(first),{},limited);})&&f.scene.encode()==before,"Prepared-source normal memory refusal preserves exact source and immutable maps");
 size_t polls=0;limited={};limited.cancelled=[&]{++polls;return false;};(void)detail::source_bake_normals(f.scene.object(first),{},limited);
 bool cancelled=true;for(size_t point=1;point<=polls;++point){size_t at=0;limited.cancelled=[&]{return ++at>=point;};cancelled&=refuses([&]{detail::source_bake_normals(f.scene.object(first),{},limited);})&&f.scene.encode()==before;}
 check(cancelled&&polls>1,"Every prepared-source capture cancellation boundary preserves source, recipe and history");
 const auto second=target(f.scene,mesh,legacy);f.scene.select(second);const auto original=f.scene.encode();const auto native_before=f.scene.snapshot();
 const auto report=bake_object(f.scene,first,options);check(report.projected&&f.scene.object(second).painted->baked(),"A real prepared Bake result is reusable as the source of a second Bake");
 check(f.scene.object(first).painted==prepared&&f.scene.object(first).material_bindings==binding,"Second Bake retains the exact immutable prepared source owners");
 double baked_error=0;size_t interior=0;
 for(size_t y=0;y<64;++y)for(size_t x=0;x<64;++x){
  const std::array<double,2> q{(x+.5)/64.,(y+.5)/64.};
  for(size_t tri=0;tri<mesh.indices.size()/3;++tri){
   const auto a=mesh.uv[mesh.indices[tri*3]],b=mesh.uv[mesh.indices[tri*3+1]],c=mesh.uv[mesh.indices[tri*3+2]];
   const double d=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1]);
   const double u=((b[1]-c[1])*(q[0]-c[0])+(c[0]-b[0])*(q[1]-c[1]))/d;
   const double v=((c[1]-a[1])*(q[0]-c[0])+(a[0]-c[0])*(q[1]-c[1]))/d;
   if(std::min({u,v,1-u-v})<.06)continue;
   const std::array<float,3> bary{float(u),float(v),float(1-u-v)};
   for(size_t role=0;role<2;++role){const auto wanted=direction(f.scene.object(first),tri,bary,role),actual=direction(f.scene.object(second),tri,bary,role);for(size_t k=0;k<3;++k)baked_error=std::max(baked_error,std::abs(wanted[k]-actual[k]));}
   ++interior;break;
  }
 }
 check(interior>500&&baked_error<.035,"Independent decoding of actual second-Bake texels preserves both source directions within declared quantization tolerance");

 const auto encoded=f.scene.encode();check(Scene::decode(encoded).encode()==encoded,"Repeated prepared-source Bake roundtrips through the native archive exactly");
 Document doc;doc.replace(native_before);check(doc.wait_mesh(),"Prepared-source Document reaches its genuine initial geometry");doc.bake_from_object(first,options);const auto out=folder/(std::to_string(legacy)+std::to_string(posed)+".dsculpt");auto saved=doc.save_async(out);saved.get();check(doc.wait_asset(std::chrono::seconds(120))&&doc.wait_mesh()&&doc.frame().state->error.empty()&&doc.copy_scene().encode()==encoded&&Scene::load(out).encode()==encoded,"Native prepared-source Bake worker and immediately ordered Save publish exact second-Bake data");
 check(doc.undo()&&doc.wait_mesh()&&doc.copy_scene().encode()==original,"Second prepared-source Bake Undo restores the complete prior Scene");check(doc.redo()&&doc.wait_mesh()&&doc.copy_scene().encode()==encoded,"Second prepared-source Bake Redo restores exact current role planes");
 std::cout<<"PREPARED legacy="<<legacy<<" posed="<<posed<<" max_direction_error="<<maximum<<" baked_error="<<baked_error<<" decoded_texels="<<interior<<" projected="<<report.projected<<" polls="<<polls<<'\n';
}
}
int main(int argc,char **argv)try{if(argc!=2)return 2;std::filesystem::create_directories(argv[1]);for(bool legacy:{false,true})for(bool posed:{false,true})run_case(legacy,posed,argv[1]);std::cout<<"PASS "<<checks<<" prepared source controls\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
