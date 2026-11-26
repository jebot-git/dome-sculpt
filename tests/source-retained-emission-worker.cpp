// SPDX-License-Identifier: GPL-3.0-or-later
#include "bake-retained-emission.hpp"
#include "emission-bake-source.hpp"
#include "dome/deformation.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
namespace {
using namespace dome;
size_t checks=0;
void check(bool value,const char *why){if(!value)throw std::runtime_error(why);++checks;}
void refuses(auto action,std::string_view message){
 try{action();}catch(const std::exception &e){check(std::string_view(e.what()).find(message)!=std::string_view::npos,"Refusal must identify the original failed invariant");return;}
 throw std::runtime_error("Expected retained-emission refusal");
}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;
 const auto scene=Scene::load(argv[1]);const auto original=scene.encode();const auto &target=scene.active();
 uint64_t source=0;for(const auto &o:scene.objects())if(o.id!=target.id&&o.painted&&o.painted->has_physical_emission()){check(!source,"One genuine physical source");source=o.id;}
 check(source&&target.painted->resolution()==2048&&scene.object(source).painted->resolution()==64,"Original independent source64/target2048 input");
 const auto mesh=target.painted->mesh_owner();const auto domain=detail::BakedEmissionDomain::capture(scene,source,mesh,2048,scene.world_transform(target.id));
 const auto &object=scene.object(source);BakeSource high;high.mesh=object.deformation?object.deformation->preview(object.painted->mesh()):object.painted->mesh();
 const auto transform=ob::compose(ob::inverse(scene.world_transform(target.id)),scene.world_transform(source));
 for(auto &p:high.mesh.positions){const auto q=ob::transform(transform,{p.x,p.y,p.z});p={q.x,q.y,q.z};}
 high.materials[0].material=object.material;high.materials[0].resolved_emission=detail::emission_bake_source(object);
 BakeOptions options;options.resolution=2048;options.ao_samples=0;options.padding=0;
 auto bake=[&](const Mesh &low,const BakeSource &input,const BakeOptions &policy,const auto &owner,const auto &geometry){return detail::bake_mesh_retained_emission(low,input,policy,{},owner,geometry);};
 refuses([&]{bake_mesh(*mesh,high,options);},"FLOAT Bake retained channels exceed unchanged paint budget");
 const auto result=bake(*mesh,high,options,domain,mesh);const size_t pixels=size_t(2048)*2048;
 check(result.projected>500&&result.coverage.size()==pixels,"Full2048 geometric projection and coverage remain active");
 check(!result.emission_field&&result.maps.emission.rgba.empty()&&!result.maps.emission.width&&!result.maps.emission.height,"No discarded target FLOAT field or duplicate RGB8 emission");
 for(const auto *image:{&result.maps.color,&result.maps.metal_rough,&result.normal,&result.ambient_occlusion})
   check(image->width==2048&&image->height==2048&&image->rgba.size()==pixels*4,"Every ordinary Bake channel keeps full2048 dimensions");
 check(domain->field()->resolution()==64&&domain->target_resolution()==2048&&domain->storage_bytes()<32*1024*1024,"Actual independently owned source64 coefficients survive inside unchanged retained quota");
 const auto foreign=std::make_shared<const Mesh>(*mesh);
 refuses([&]{bake(*mesh,high,options,domain,foreign);},"borrowed another current geometry/grid owner");
 auto changed=*mesh;changed.uv[changed.indices[0]][0]+=.0001f;
 refuses([&]{bake(changed,high,options,domain,mesh);},"changed certified target geometry or UVs");
 changed=*mesh;changed.positions[changed.indices[0]].x+=.0001f;
 refuses([&]{bake(changed,high,options,domain,mesh);},"changed certified target geometry or UVs");
 auto source_changed=high;source_changed.mesh.positions[source_changed.mesh.indices[0]].x+=.0001f;
 refuses([&]{bake(*mesh,source_changed,options,domain,mesh);},"changed certified target geometry or UVs");
 source_changed=high;source_changed.materials.emplace_back();
 refuses([&]{bake(*mesh,source_changed,options,domain,mesh);},"one genuine certified source");
 source_changed=high;source_changed.materials[0].resolved_emission={};
 refuses([&]{bake(*mesh,source_changed,options,domain,mesh);},"one genuine certified source");
 auto policy=options;policy.cage_positions=mesh->positions;
 refuses([&]{bake(*mesh,high,policy,domain,mesh);},"changed its exact source corners or ray columns");
 changed=*mesh;for(auto &n:changed.normals)n=n*-1;
 refuses([&]{bake(changed,high,options,domain,mesh);},"changed its exact source corners or ray columns");
 refuses([&]{bake(*mesh,high,options,std::shared_ptr<const detail::BakedEmissionDomain>{},mesh);},"one genuine certified source");
 policy=options;policy.resolution=1024;
 refuses([&]{bake(*mesh,high,policy,domain,mesh);},"borrowed another current geometry/grid owner");
 policy=options;policy.cancelled=[] {return true;};
 refuses([&]{bake(*mesh,high,policy,domain,mesh);},"Mesh baking cancelled");
 check(scene.encode()==original,"Positive and all refused workers preserve the complete original project");
 std::cout<<"PASS "<<checks<<" retained emission worker controls\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
