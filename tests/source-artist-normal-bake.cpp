// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-bake-normal.hpp"
#include "dome/bake-authoring.hpp"
#include "dome/document.hpp"
#include "dome/deformation.hpp"
#include "dome/material-bindings.hpp"
#include "dome/source-paint.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <numeric>
using namespace dome;
namespace {
int checks=0;void check(bool yes,const char*why){if(!yes)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<'\n';}
bool refuses(auto f){try{f();return false;}catch(const std::exception&e){std::cout<<"REFUSAL "<<e.what()<<'\n';return true;}}
const std::array<Vec3,6> artistN{{{.2f,-.3f,1},{-.4f,.2f,1},{.25f,.5f,1},{.2f,-.3f,1},{.25f,.5f,1},{-.3f,-.2f,1}}};
const std::array<Vec3,6> artistT{{{.8f,.4f,.2f},{.5f,.7f,-.1f},{.9f,-.3f,.3f},{.8f,.4f,.2f},{.9f,-.3f,.3f},{.7f,.2f,.4f}}};
Image normal_image(){return {2,2,{190,60,220,255,80,170,240,255,160,210,180,255,110,90,250,255}};}
Vec3 texel(float u,float v){const float x=u*2-.5f,y=v*2-.5f;const int ix=int(std::floor(x)),iy=int(std::floor(y));const float fx=x-ix,fy=y-iy;Vec3 out;const auto im=normal_image();for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){const int px=((ix+dx)%2+2)%2,py=((iy+dy)%2+2)%2;const auto at=(py*2+px)*4;const float w=(dx?fx:1-fx)*(dy?fy:1-fy);out+=Vec3{im.rgba[at]/255.f,im.rgba[at+1]/255.f,im.rgba[at+2]/255.f}*w;}return out;}
struct Fixture{Scene scene;uint64_t source=0;std::vector<std::array<float,2>> uv;};
Fixture fixture(bool legacy,bool posed){
 Mesh raw;raw.positions={{-.6f,-.6f,0},{.6f,-.6f,0},{.6f,.6f,0},{-.6f,-.6f,0},{.6f,.6f,0},{-.6f,.6f,0}};raw.uv={{{0,0}},{{1,0}},{{1,1}},{{0,0}},{{1,1}},{{0,1}}};raw.indices={0,1,2,3,4,5};raw.normals.assign(artistN.begin(),artistN.end());for(auto&n:raw.normals)n=normalized(n);
 auto maps=std::make_shared<PaintSourceMaps>();maps->normal.image=std::make_shared<const Image>(normal_image());maps->normal.texcoord=1;maps->normal.min_filter=maps->normal.mag_filter=9729;maps->normal_scale=.8f;maps->normal.uv_transform={.9f,.25f,-.15f,1.1f,.1f,-.2f};auto uv=std::make_shared<std::vector<std::array<float,2>>>();for(auto q:raw.uv)uv->push_back({.12f+.76f*q[1],1-.69f*q[0]-.17f*q[1]});maps->uv1=uv;
 auto original=capture_source_sampling(raw,maps,64);auto atlas=raw;for(auto&q:atlas.uv){q[0]=.125f+.75f*q[0];q[1]=.0625f+.875f*q[1];}const std::array<uint32_t,6> ids{0,1,2,3,4,5};auto paint=prepare_source_paint(original,atlas,ids);
 auto rest=std::make_shared<DeformationState>(raw);rest->morphs=MorphSet(raw);auto shape=raw.positions;shape[1].z=.04f;shape[4].z=-.025f;rest->morphs->set_target(raw,"Artist source morph",shape);
 auto frames=std::make_shared<NormalFrames>();frames->origin_vertex_count=6;frames->origin_vertices=raw.indices;frames->normals.assign(artistN.begin(),artistN.end());for(auto&n:frames->normals)n=normalized(n);for(auto t:artistT){t=normalized(t);frames->tangents.push_back({t.x,t.y,t.z,1});}frames->targets.push_back({1,std::vector<Vec3>(6,Vec3{.1f,-.15f,.05f}),std::vector<Vec3>(6,Vec3{-.08f,.12f,.06f})});
 auto set=std::make_shared<NormalFrameSet>();set->pbr=set->mtoon=frames;set->targets={{1,0,std::nullopt,{}}};set->current_targets={1};set->next_target=2;
 auto origin=std::make_shared<SourceUvOrigin>();origin->paint=original;origin->deformation=rest;
 auto binding=std::make_shared<SourceMaterialBindings>();binding->normal_frames=set;binding->normal_frame=SourceMaterialNormalFrame::OriginalAttributes;binding->uv_origin=origin;
 binding->material=legacy?MToonMaterial::from_vrm0({{"shader","VRM/MToon"},{"floatProperties",{{"_BumpScale",1.3f}}}}):MToonMaterial::from_vrm1({{"extensions",{{"VRMC_materials_mtoon",{{"specVersion","1.0"}}}}}});binding->material.parameters.normal_enabled=true;binding->material.parameters.normal_scale=1.3f;auto&slot=binding->material.textures[size_t(MToonSlot::Normal)];slot.index=0;slot.texcoord=1;slot.encoding=MToonEncoding::Normal;slot.offset={.1f,-.15f};slot.scale={.7f,1.2f};binding->roles[size_t(MToonSlot::Normal)].image=maps->normal.image;binding->roles[size_t(MToonSlot::Normal)].min_filter=binding->roles[size_t(MToonSlot::Normal)].mag_filter=9729;
 Fixture f;f.uv=*uv;auto&o=f.scene.active();f.source=o.id;o.mesh_active=true;o.painted=paint;o.material_bindings=binding;o.name="Authentic independent artist source";auto current=*rest;if(posed){current.morph_weights={.35f};current.skeleton=Skeleton{{{"root",-1,{},1}}};current.skin=SkinWeights{};current.skin->vertices.assign(6,{{0,1}});current.pose={{{},{},{1.25f,.7f,1.4f}}};}f.scene.set_deformation(o.id,current);
 f.scene.validate();return f;
}
Vec3 expected(const Fixture&f,bool legacy,bool posed,size_t triangle,std::array<float,3>bary,size_t role){
 Vec3 N,T,B;for(size_t k=0;k<3;++k){const auto c=triangle*3+k;auto n=normalized(artistN[c]),t=normalized(artistT[c]);if(posed){n+=Vec3{.1f,-.15f,.05f}*.35f;t+=Vec3{-.08f,.12f,.06f}*.35f;n={n.x/1.25f,n.y/.7f,n.z/1.4f};t={t.x*1.25f,t.y*.7f,t.z*1.4f};}n=normalized(n);t=normalized(t);N+=n*bary[k];T+=t*bary[k];B+=cross(n,t)*bary[k];}
 Vec3 n=N,t=T,b=B;if(role==0){n=normalized(n);t=normalized(t);b=normalized(b);}else if(!legacy){n=normalized(n);t=normalized(t);b=normalized(cross(n,t));}
 std::array<float,2> uv{};for(size_t k=0;k<3;++k)for(size_t j=0;j<2;++j)uv[j]+=f.uv[triangle*3+k][j]*bary[k];const auto p=uv;if(role==0)uv={.9f*p[0]+.25f*p[1]+.1f,-.15f*p[0]+1.1f*p[1]-.2f};else{if(legacy){/* MToon0 role uses the authored Main transform, here identity. */}else uv={p[0]*.7f+.1f,p[1]*1.2f-.15f};}
 const auto q=texel(uv[0],uv[1]);const float strength=role==0?.8f:1.3f;const auto ts=normalized(Vec3{(q.x*2-1)*strength,(q.y*2-1)*strength,q.z*2-1});return normalized(t*ts.x+b*ts.y+n*ts.z);
}
void run(bool legacy,bool posed,const std::filesystem::path&out){auto f=fixture(legacy,posed);const auto before=f.scene.encode();const auto&source=f.scene.object(f.source);auto resolve=detail::source_bake_normals(source,{});check(bool(resolve),"Artist source captures actual declared PBR/typed frames");float role_gap=0;
 for(size_t triangle=0;triangle<2;++triangle)for(auto bary:std::array<std::array<float,3>,3>{{{.2f,.3f,.5f},{.6f,.25f,.15f},{.05f,.7f,.25f}}}){const auto actual=resolve(uint32_t(triangle),bary);for(size_t role=0;role<2;++role)check(length(actual[role]-expected(f,legacy,posed,triangle,bary,role))<2e-6f,"Actual role normal agrees with independent artist/morph/anisotropic-skin/bilinear-UV oracle");role_gap=std::max(role_gap,length(actual[0]-actual[1]));}
 check(role_gap>.1f,"Declared PBR and MToon normal roles produce measurably distinct directions");check(f.scene.encode()==before,"Normal capture leaves all artist/rest/skin/morph/UV metadata exact");
 detail::SourceBakeNormalOptions limited;limited.max_working_bytes=1;check(refuses([&]{detail::source_bake_normals(source,{},limited);}),"Prospective frame-capture quota refuses before publication");size_t polls=0;limited={};limited.cancelled=[&]{++polls;return false;};(void)detail::source_bake_normals(source,{},limited);const auto count=polls;bool atomic=true;for(size_t boundary=1;boundary<=count;++boundary){polls=0;limited.cancelled=[&]{return ++polls>=boundary;};atomic=atomic&&refuses([&]{detail::source_bake_normals(source,{},limited);})&&f.scene.encode()==before;}check(atomic&&count>1,"Every actual frame-capture cancellation preserves immutable native source");
 const auto source_before=f.scene.encode().at("objects").at(0);auto low=source.deformation->preview(source.painted->mesh());const auto target=f.scene.add("Prepared independent target",Volume{});auto&t=f.scene.object(target);t.mesh_active=true;t.painted=std::make_shared<PaintedMesh>(low,64);f.scene.select(target);ObjectBakeSettings settings;settings.ao_samples=0;settings.padding=0;settings.front_mm=settings.back_mm=300;const auto baseline=f.scene.encode();check(refuses([&]{bake_object(f.scene,f.source,settings);})&&f.scene.encode()==baseline,"One-plane target atomically refuses distinct PBR/MToon source normals");settings.prepare_normals=true;settings.normal_error=.03f;
 const auto report=bake_object(f.scene,f.source,settings);check(report.projected&&report.normal_prepared&&f.scene.active().painted->baked()&&!f.scene.active().painted->baked()->maps.mtoon_normal.rgba.empty(),"Actual source-aware Bake retains separate normal planes in genuine prepared target");check(f.scene.encode().at("objects").at(0)==source_before,"Actual Bake retains the complete artist source object");
 const auto encoded=f.scene.encode();check(Scene::decode(encoded).encode()==encoded,"Actual dual-role artist Bake independently reopens its native owners");f.scene.save(out);check(Scene::load(out).encode()==encoded,"Actual dual-role artist Bake saves and reopens exactly");std::cout<<"ARTIST_CASE legacy="<<legacy<<" posed="<<posed<<" role_gap="<<role_gap<<" projected="<<report.projected<<" polls="<<count<<'\n';
}
}
int main(int argc,char**argv)try{if(argc!=2)return 2;std::filesystem::create_directories(argv[1]);for(bool legacy:{false,true})for(bool posed:{false,true})run(legacy,posed,std::filesystem::path(argv[1])/(std::to_string(legacy)+std::to_string(posed)+".dsculpt"));std::cout<<"PASS "<<checks<<" artist source CPU controls\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
