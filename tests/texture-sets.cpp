// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/texture-sets.hpp"
#include "dome/polygon-scene.hpp"
#include "texture-set-surface.hpp"
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
void plan_controls(){
 auto s=fixture();const auto id=create_texture_set(s,s.active().id,std::array<uint64_t,1>{3},"One actual triangle");
 const auto exact=s.encode();const auto plan=detail::prepare_texture_set_surface(s.active());
 check(plan->geometry()==s.active().painted->mesh_owner()&&plan->indices().size()==12&&plan->logical_corners().size()==12,"Partition packet retains one immutable mesh and exactly one complete corner stream");
 check(plan->draws().size()==2&&plan->draws()[0].count==9&&plan->draws()[1].count==3,"Unselected and selected set triangles are both retained exactly once");
 for(size_t c=0;c<plan->indices().size();++c)check(plan->indices()[c]==plan->geometry()->indices.at(plan->logical_corners()[c]),"Every packed corner has its actual logical REST identity");
 check(plan->logical_corners()[6]==9&&plan->logical_corners()[9]==6,"Noncontiguous source faces have explicit current corner remaps");
 plan->authenticate_rest(s.active().painted->mesh(),{});check(true,"Actual REST replay accepts the owning source");
 auto changed=s.active().painted->mesh();changed.positions[0].x+=.1f;refuses([&]{plan->authenticate_rest(changed,{});},"Posed or foreign geometry cannot replace REST authentication");
 changed=s.active().painted->mesh();changed.normals[0].x=-0.f;refuses([&]{plan->authenticate_rest(changed,{});},"Authored normal bit changes cannot borrow the source plan");
 refuses([&]{plan->authenticate_rest(s.active().painted->mesh(),std::make_shared<const DeformationState>());},"A foreign deformation owner cannot borrow the source plan");
 const auto layer_count=plan->fields()[1].painted->layers().size();
 edit_texture_set(s,s.active().id,id,[](PaintedMesh&p,Material&){p.add_layer("Later edit");return true;});
 check(plan->fields()[1].painted->layers().size()==layer_count&&plan->collection()!=s.active().texture_sets,"Plan fields remain immutable across later actual set edits");
 TextureSetLimits small;small.maximum_bytes=1;refuses([&]{detail::prepare_texture_set_surface(s.active(),small);},"Geometry packet allocation refuses its byte forecast before staging");
 small=TextureSetLimits{};small.maximum_work=1;refuses([&]{detail::prepare_texture_set_surface(s.active(),small);},"Exact geometry packet work is bounded before replay");
 small=TextureSetLimits{};small.cancelled=[] {return true;};refuses([&]{detail::prepare_texture_set_surface(s.active(),small);},"Actual packet cancellation refuses before publication");
 check(exact!=s.encode(),"Independent field edit is actual rather than a no-op");
}
void polygon_controls(){
 auto authority=std::make_shared<PolygonAuthority>();authority->topology=PolygonMesh::from_triangles(mesh());
 Scene s;s.active().polygon=authority;s.active().mesh_active=true;s.active().painted=std::make_shared<PaintedMesh>(authority->topology.triangulate().mesh,64);
 const auto face=authority->topology.faces().at(2).id;const auto id=create_texture_set(s,s.active().id,std::span<const uint64_t>(&face,1),"Actual polygon face");
 check(id==1&&s.active().texture_sets->topology()->polygon_ids(),"Actual polygon membership uses authentic topology IDs");
 const auto encoded=s.encode();check(Scene::decode(encoded).encode()==encoded,"Genuine polygon face/corner set roundtrip");
 auto bad=encoded;bad["texture_set_geometry"]["topologies"][0]["polygon_ids"]=false;
 refuses([&]{Scene::decode(bad);},"Forged raw identity marker must not reinterpret genuine polygon IDs");
 bad=encoded;auto &faces=bad["texture_set_geometry"]["topologies"][0]["faces"].get_binary();faces[0]^=128;
 refuses([&]{Scene::decode(bad);},"Foreign polygon face identity refuses");
 bad=encoded;auto &corners=bad["texture_set_geometry"]["topologies"][0]["corners"].get_binary();corners[0]^=128;
 refuses([&]{Scene::decode(bad);},"Foreign polygon corner identity refuses");
 auto raw=fixture();create_texture_set(raw,raw.active().id,std::array<uint64_t,1>{1},"Triangle source");bad=raw.encode();bad["texture_set_geometry"]["topologies"][0]["polygon_ids"]=true;
 refuses([&]{Scene::decode(bad);},"Invented polygon marker cannot borrow a missing authority");
}
}
int main(int argc,char**argv)try{
 if(argc==3&&std::string_view(argv[2])=="--plan"){plan_controls();std::cout<<"PASS "<<checks<<" texture-set partition plan controls\n";return 0;}
 if(argc==3&&std::string_view(argv[2])=="--polygon"){polygon_controls();std::cout<<"PASS "<<checks<<" texture-set polygon controls\n";return 0;}
 if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 static_assert(std::is_same_v<decltype(TextureSetRecord{}.painted),std::shared_ptr<const PaintedMesh>>);
 auto s=fixture();const auto legacy=s.encode();const auto object=s.active().id;const auto original_mesh=s.active().painted->mesh_owner();
 const std::array<uint64_t,2>right{3,4};const auto id=create_texture_set(s,object,right,"Right material");
 check(id==1&&s.active().texture_sets->active()==id,"Persistent explicit set ID and active selection");
 check(s.active().painted->mesh_owner()==original_mesh&&s.active().texture_sets->at(id).painted->mesh_owner()==original_mesh,"Set fork shares one immutable actual authoring mesh owner");
 check(s.active().texture_sets->triangle_sets()[0]==0&&s.active().texture_sets->triangle_sets()[2]==id,"Actual face IDs assign distinct material slots");
 const auto first=s.encode();check(first.at("version")==40&&first.at("objects")[0].at("painted").is_null(),"Conditional Scene40 has no duplicate primary geometry payload");
 check(first.at("texture_set_geometry").at("meshes").size()==1&&first.at("texture_set_geometry").at("topologies").size()==1,"All sets share one geometry archive record");
 auto duplicate=s.snapshot();const auto copy=duplicate.duplicate(object);const auto two=duplicate.encode();
 check(two.at("texture_set_geometry").at("meshes").size()==1,"Distinct mesh instances with the same immutable geometry owner do not duplicate vertex arrays");
 check(Scene::decode(two).encode()==two,"Independent object-local set IDs survive shared-geometry roundtrip");
 check(edit_texture_set(duplicate,copy,id,[](PaintedMesh&p,Material&){p.add_layer("Other instance only");return true;}),"Duplicated instance edits detach its immutable set collection");
 check(duplicate.object(object).texture_sets->at(id).painted->layers().size()==1&&duplicate.object(copy).texture_sets->at(id).painted->layers().size()==2,"Shared geometry never implies shared mutable set layers");
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
