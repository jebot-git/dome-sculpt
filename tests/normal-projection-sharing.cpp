// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/bake.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/source-maps.hpp"
#include "paint-normal-access.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
namespace {
using namespace dome;size_t checks=0;
void check(bool v,const char*why){if(!v)throw std::runtime_error(why);++checks;}
bool same(const auto&a,const auto&b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);}
Mesh quad(){Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
Image neutral(){Image image{64,64,std::vector<uint8_t>(64*64*4)};for(size_t i=0;i<64*64;++i){image.rgba[4*i]=128;image.rgba[4*i+1]=128;image.rgba[4*i+2]=255;image.rgba[4*i+3]=255;}return image;}
std::vector<uint8_t>bytes(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
}
int main(int argc,char**argv)try{
 if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);constexpr size_t pixels=64*64;
 ColorPreparationInput input;input.paint=std::make_shared<PaintedMesh>(quad(),64);const auto result=prepare_source_normals(input);const auto &projection=*result.paint->normal_projection();
 check(result.converted&&same(projection.pbr,projection.mtoon),"Equal completed intrinsic roles physically share one immutable image control block");
 check(result.paint->storage_bytes()==pixels*4,"Only the actual shared allocation is retained against the original paint quota");
 Scene scene;scene.active().mesh_active=true;scene.active().painted=result.paint;scene.active().material_bindings=result.material_bindings;const auto native=scene.encode();auto restored=Scene::decode(native);
 check(restored.encode()==native&&same(restored.active().painted->normal_projection()->pbr,restored.active().painted->normal_projection()->mtoon),"Genuine material/normal-frame native replay preserves both roles and their shared owner");
 scene.save(out/"shared.dsculpt");restored.save(out/"shared-reopened.dsculpt");check(bytes(out/"shared.dsculpt")==bytes(out/"shared-reopened.dsculpt"),"Shared-image native Save/reopen is byte exact");
 auto maps=std::make_shared<PaintSourceMaps>();auto normal=std::make_shared<Image>(neutral());for(size_t i=0;i<pixels;++i)normal->rgba[4*i]=255;maps->normal.image=normal;
 ColorPreparationInput different;different.paint=PaintedMesh::from_maps(quad(),maps,64);
 // This is an authored native quad with known original N/T, not an imported
 // normal texture with missing frame provenance. Retain those actual ordered
 // corner attributes and the original UV/paint owner before preparation.
 auto frame=std::make_shared<NormalFrames>();frame->origin_vertex_count=4;frame->origin_vertices=different.paint->mesh().indices;frame->normals.assign(6,{0,0,1});frame->tangents.assign(6,{1,0,0,1});
 auto frames=std::make_shared<NormalFrameSet>();frames->pbr=frame;
 auto origin=std::make_shared<SourceUvOrigin>();origin->paint=different.paint->snapshot();
 auto binding=std::make_shared<SourceMaterialBindings>();binding->has_mtoon=false;binding->uv_origin=origin;binding->normal_frames=frames;binding->normal_frame=SourceMaterialNormalFrame::OriginalAttributes;binding->validate();different.material_bindings=binding;
 const auto changed=prepare_source_normals(different);const auto &roles=*changed.paint->normal_projection();
 check(!same(roles.pbr,roles.mtoon)&&roles.pbr->rgba!=roles.mtoon->rgba,"Genuinely distinct PBR and absent-MToon intrinsic values keep separate image owners");
 check(changed.paint->storage_bytes()==pixels*8,"Distinct role allocations remain fully charged");
 auto historical=std::make_shared<PaintedMesh>(quad(),64);BakeSource source;source.mesh=quad();BakeOptions options;options.resolution=64;options.padding=0;options.ao_samples=0;auto bake=bake_mesh(quad(),source,options);BakedTextures textures;textures.maps=std::move(bake.maps);textures.maps.normal=std::move(bake.normal);for(auto c:bake.coverage)textures.coverage.push_back(c==1||c==2?255:0);historical->install_bake(std::move(textures));
 auto separate=std::make_shared<PaintNormalProjection>();separate->pbr=std::make_shared<const Image>(neutral());separate->mtoon=std::make_shared<const Image>(neutral());auto shared=std::make_shared<PaintNormalProjection>();shared->pbr=separate->pbr;shared->mtoon=separate->pbr;
 const auto two=SourceNormalPreparationAccess::adopt_canonical(*historical,separate),one=SourceNormalPreparationAccess::adopt_canonical(*historical,shared);
 check(two->storage_bytes()==pixels*21&&one->storage_bytes()==pixels*17,"Equal byte values receive no credit unless the actual image allocation is shared");
 check(!same(two->normal_projection()->pbr,two->normal_projection()->mtoon),"Existing separate immutable owners are never silently collapsed by accounting");
 PaintImageArchive table;check(table.add(shared->pbr)==table.add(shared->mtoon),"Existing image archive records the real shared owner once");
 PaintImageArchive distinct;const auto a=distinct.add(separate->pbr),b=distinct.add(separate->mtoon);check(a!=b,"Legacy equal-but-separate owners keep distinct native image identities");
 const auto wire=encode_normal_projection(*separate,distinct);const auto decoded=decode_normal_projection(wire,distinct);check(!same(decoded->pbr,decoded->mtoon)&&decoded->pbr->rgba==decoded->mtoon->rgba,"Legacy distinct identities replay unchanged despite equal bytes");
 std::cout<<"PASS "<<checks<<" normal projection sharing controls\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
