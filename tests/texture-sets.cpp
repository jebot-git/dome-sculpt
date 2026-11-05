// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/texture-sets.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>
#include <type_traits>
using namespace dome;
namespace {
size_t checks=0;
void check(bool v,const char*message){if(!v)throw std::runtime_error(message);++checks;}
template<class F>void refuses(F&&f,const char*message){bool caught=false;try{f();}catch(const std::exception&e){caught=true;std::cout<<"EXPECTED "<<e.what()<<'\n';}check(caught,message);}
Mesh mesh(){Mesh m;m.positions={{-1,-1,0},{0,-1,0},{0,1,0},{-1,1,0},{0,-1,0},{1,-1,0},{1,1,0},{0,1,0}};m.normals.assign(8,{0,0,1});m.uv={{{.05f,.05f}},{{.45f,.05f}},{{.45f,.95f}},{{.05f,.95f}},{{.55f,.05f}},{{.95f,.05f}},{{.95f,.95f}},{{.55f,.95f}}};m.indices={0,1,2,0,2,3,4,5,6,4,6,7};m.validate();return m;}
Scene fixture(){Scene s;s.active().mesh_active=true;s.active().painted=std::make_shared<PaintedMesh>(mesh(),64);s.active().material.color={.1f,.2f,.3f,1};return s;}
}
int main(int argc,char**argv)try{
 if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 static_assert(std::is_same_v<decltype(TextureSetRecord{}.painted),std::shared_ptr<const PaintedMesh>>);
 auto s=fixture();const auto legacy=s.encode();const auto object=s.active().id;const auto original_mesh=s.active().painted->mesh_owner();
 const std::array<uint64_t,2>right{3,4};const auto id=create_texture_set(s,object,right,"Right material");
 check(id==1&&s.active().texture_sets->active()==id,"Persistent explicit set ID and active selection");
 check(s.active().painted->mesh_owner()==original_mesh&&s.active().texture_sets->at(id).painted->mesh_owner()==original_mesh,"Set fork shares one immutable actual authoring mesh owner");
 check(s.active().texture_sets->triangle_sets()[0]==0&&s.active().texture_sets->triangle_sets()[2]==id,"Actual face IDs assign distinct material slots");
 const auto first=s.encode();check(first.at("version")==40&&first.at("objects")[0].at("painted").is_null(),"Conditional Scene40 has no duplicate primary geometry payload");
 check(first.at("texture_set_geometry").at("topologies").size()==1,"All sets share one geometry archive record");
 const auto&sets=first.at("objects")[0].at("texture_sets");check(sets.at("primary").at("version")==7&&sets.at("additional")[0].at("painted").at("version")==7,"Every set requires its own Paint7 membership context");
 check(!sets.at("primary").at("payload").contains("mesh")&&!sets.at("additional")[0].at("painted").at("payload").contains("mesh"),"Set paint payloads never copy the full current geometry");
 refuses([&]{PaintedMesh::decode(sets.at("primary"));},"Standalone Paint reader cannot invent set membership");
 auto history=s.snapshot();const auto frozen=history.encode();
 check(edit_texture_set(s,object,id,[](PaintedMesh&p,Material&m){p.add_layer("Independent physical");PaintSettings settings;settings.front_only=false;settings.channel=PaintChannel::Emission;settings.emission_units=PaintEmissionUnits::Physical;settings.emission={12,.01f,4};Brush b;b.from=b.to={.5f,0,0};b.normal={0,0,1};b.radius=.7f;b.strength=.8f;p.begin_stroke();p.stamp(b,settings);const auto changed=p.end_stroke();m.roughness=.73f;return changed;}),"Actual independent set F32 stroke commits a detached field");
 check(history.encode()==frozen,"Const collection/history aliases retain exact paint/material bytes");
 check(s.active().texture_sets->at(id).painted->has_physical_emission()&&!s.active().painted->has_physical_emission(),"Physical field cannot leak to unselected set");
 check(s.active().texture_sets->at(id).painted->layers().size()==2&&s.active().painted->layers().size()==1,"Writable layers are independent per set");
 const auto changed=s.encode();auto reopened=Scene::decode(changed);check(reopened.encode()==changed,"All F32/legacy layers, materials, IDs and memberships survive native codec roundtrip");
 check(reopened.active().texture_sets->at(id).painted->mesh_owner()==reopened.active().painted->mesh_owner(),"Decoder reconstructs one shared actual geometry owner");
 reopened.save(out/"sets.dsculpt");check(Scene::load(out/"sets.dsculpt").encode()==changed,"Actual atomic file Save/reopen preserves the owning Scene40 graph");
 check(select_texture_set(s,object,0)&&s.active().painted->mesh_owner()==original_mesh,"Switching active set preserves authoritative geometry");
 const auto exact=s.encode();auto detached=s.snapshot();detached.active().texture_sets.reset();refuses([&]{detached.validate();},"Membership-bound paint cannot lose its actual current collection");
 refuses([&]{edit_texture_set(s,object,id,[](PaintedMesh&p,Material&){p.add_layer("Never published");throw std::runtime_error("Injected edit failure");return true;});},"Edit failure rolls back before immutable publication");check(s.encode()==exact,"Edit failure preserves complete Scene, active set and high-water IDs");
 refuses([&]{edit_texture_set(s,object,id,[](PaintedMesh&p,Material&){p.begin_stroke();return true;});},"Unfinished stroke cannot escape owning draft");check(s.encode()==exact,"Unfinished draft leaves history and fields exact");
 auto bad=changed;bad["version"]=36;refuses([&]{Scene::decode(bad);},"Old Scene cannot reinterpret new membership");
 bad=changed;bad["objects"][0]["texture_sets"]["active"]=7;refuses([&]{Scene::decode(bad);},"Foreign active material slot refuses");
 bad=changed;bad["objects"][0]["texture_sets"]["additional"][0]["painted"]["set"]=0;refuses([&]{Scene::decode(bad);},"Copied foreign member paint cannot borrow another slot");
 bad=changed;bad["texture_set_geometry"]["topologies"].push_back(bad["texture_set_geometry"]["topologies"][0]);refuses([&]{Scene::decode(bad);},"Unreachable geometry record refuses");
 TextureSetLimits cancelled;cancelled.cancelled=[] {return true;};refuses([&]{assign_texture_set_faces(s,object,id,std::array<uint64_t,1>{1},cancelled);},"Actual cancellation leaves membership unmodified");check(s.encode()==exact,"Cancelled assignment retains complete native state");
 check(Scene::decode(legacy).encode()==legacy&&legacy.at("version")!=40,"Unused set feature leaves historical codec conditional");
 std::cout<<"PASS "<<checks<<" texture-set model/codec controls\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
