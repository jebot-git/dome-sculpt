// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/shapekey-authoring.hpp"
#include "dome/creation.hpp"
#include "dome/material-bindings.hpp"
#include "dome/polygon-scene.hpp"
#include "normal-frame-runtime.hpp"
#include "polygon-conversion.hpp"
#include "cgltf.h"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
int checks=0;void check(bool v,const char*m){++checks;if(!v)throw std::runtime_error(m);std::cout<<"PASS "<<m<<'\n';}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}
bool same_vectors(std::span<const Vec3>a,std::span<const Vec3>b){if(a.size()!=b.size())return false;for(size_t i=0;i<a.size();++i)if(a[i].x!=b[i].x||a[i].y!=b[i].y||a[i].z!=b[i].z)return false;return true;}
Scene fixture(){
 Mesh m;m.positions={{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}};m.indices={0,1,2,0,2,3};m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.normals.assign(4,normalized(Vec3{.2f,.15f,1}));
 Scene s;s.active().mesh_active=true;s.active().painted=std::make_shared<PaintedMesh>(m,64);return s;
}
const NormalFrameTarget &key(const NormalFrames &f,uint64_t id){for(const auto&t:f.targets)if(t.id==id)return t;throw std::runtime_error("Missing key");}
void standalone(const std::filesystem::path &prefix){
 auto s=fixture();auto before=s.encode();ShapeKeyEdit edit;edit.name="Smile";auto result=author_shapekey(s,s.active().id,edit);
 check(result.changed&&result.target==0&&s.active().shape_source&&s.active().deformation->morphs->targets.size()==1,"Create adds a real editable key and complete independent before-state source");
 auto encoded=s.encode();check(encoded.at("version")==16&&encoded.at("polygon_sources").at("version")==4&&encoded.at("polygon_sources").at("authorities").empty(),"Scene16 stores a direct native source without fabricated polygon authority");
 const auto &frames=*s.active().material_bindings->normal_frames;
 check(frames.pbr==frames.mtoon&&key(*frames.pbr,frames.current_targets[0]).normals.empty()&&frames.pbr->normals[0].x==s.active().painted->mesh().normals[0].x,"A neutral key preserves artist base directions and frame aliasing exactly");
 auto id=frames.current_targets[0];auto rest=s.active().painted->mesh();edit.action=ShapeKeyAction::Replace;edit.target=0;edit.positions=rest.positions;edit.positions[0].z=.2f;
 author_shapekey(s,s.active().id,edit);auto edited=s.active().deformation->morphs->targets[0].delta;
 check(s.active().material_bindings->normal_frames->current_targets[0]==id&&!key(*s.active().material_bindings->normal_frames->pbr,id).normals.empty(),"Geometry editing retains stable key identity and authors full normal/tangent channels");
 check(s.active().shape_source->material_bindings==nullptr&&s.active().shape_source->deformation==nullptr&&s.active().shape_source->paint->encode()==fixture().active().painted->encode(),"Editing retains the exact original native mesh and pre-deformation material state");
 auto exact=s.encode();check(!author_shapekey(s,s.active().id,edit).changed&&s.encode()==exact,"Identical replacement is a true no-op with exact source/state");
 edit={};edit.action=ShapeKeyAction::Mirror;edit.target=0;edit.name="Mirror";auto mirror=author_shapekey(s,s.active().id,edit);
 check(mirror.target==1&&std::abs(s.active().deformation->morphs->targets[1].delta[1].z-.2f)<1e-6,"Mirror creates the geometric counterpart with independent key identity");
 edit.action=ShapeKeyAction::Duplicate;edit.name="Copy";auto copy=author_shapekey(s,s.active().id,edit);auto f=s.active().material_bindings->normal_frames;
 check(copy.target==2&&same_vectors(s.active().deformation->morphs->targets[2].delta,edited)&&same_vectors(key(*f->pbr,f->current_targets[2]).normals,key(*f->pbr,id).normals),"Duplicate keeps exact POSITION and artist direction deltas");
 auto d=*s.active().deformation;d.morph_weights={.3f,.2f,.1f};s.set_deformation(s.active().id,d);auto wanted=d.preview(rest);
 edit.action=ShapeKeyAction::CaptureMix;edit.name="Mix";auto mix=author_shapekey(s,s.active().id,edit);d=*s.active().deformation;d.morph_weights.assign(4,0);d.morph_weights[mix.target]=1;auto actual=d.preview(rest);
 bool same=true;for(size_t v=0;v<rest.positions.size();++v)same&=length(actual.positions[v]-wanted.positions[v])<1e-6f;
 check(same,"Capture mix creates a real pre-skin key matching the current blended geometry");
 auto path=prefix;path+=".dsculpt";s.save(path);auto reopened=Scene::load(path);check(reopened.encode()==s.encode()&&reopened.active().shape_source,"Actual save/reopen preserves current keys and the full independent source capsule");
 auto restored=reopened;edit.action=ShapeKeyAction::Restore;author_shapekey(restored,restored.active().id,edit);check(restored.encode()==before,"Restore after reopen recovers the exact complete before-authoring scene");
 auto duplicate=s;duplicate.duplicate(duplicate.active().id);auto round=Scene::decode(duplicate.encode());check(round.objects()[0].shape_source==round.objects()[1].shape_source,"Shared source capsules remain aliased across duplicate and codec roundtrip");
 for(int mode=0;mode<5;++mode){auto bad=s.encode();
  if(mode==0)bad["version"]=13;
  if(mode==1)bad["polygon_sources"]["version"]=1;
  if(mode==2)bad["objects"][0]["shape_source"]=UINT32_MAX;
  if(mode==3)bad["polygon_sources"]["origins"][0]["shape_source"]=0;
  if(mode==4)bad["polygon_sources"]["origins"].push_back(bad["polygon_sources"]["origins"][0]);
  check(refuses([&]{(void)Scene::decode(bad);}),"Wrong-version, invalid, cyclic and orphan direct sources are refused before admission");
 }
 auto unchanged=s.encode();edit={};edit.name="Cancelled";
 check(refuses([&]{author_shapekey(s,s.active().id,edit,[]{return true;});})&&s.encode()==unchanged,"Cancellation preserves exact current keys and all retained sources");
}
void avatar(const std::filesystem::path &prefix){
 Scene s;append_humanoid(s,humanoid_baseline_defaults("vroid-beta-female"),"CC0",{},64);
 auto i=std::find_if(s.objects().begin(),s.objects().end(),[](const auto&o){return o.deformation&&o.deformation->morphs&&!o.deformation->morphs->targets.empty();});
 check(i!=s.objects().end(),"Real bundled CC0 VRoid has authored shape targets");auto id=i->id;s.select(id);auto before=s.encode();auto raw=s.active().appearance;auto original=s.active().material_bindings;auto oldcount=s.active().deformation->morphs->targets.size();
 ShapeKeyEdit e;e.action=ShapeKeyAction::Duplicate;e.target=0;e.name="Authored VRoid copy";author_shapekey(s,id,e);
 check(s.active().deformation->morphs->targets.size()==oldcount+1&&s.active().shape_source->material_bindings==original&&s.active().appearance==raw,"Actual VRoid duplicate preserves original artist frames, raw source and existing target identities");
 check(s.active().material_bindings->normal_frames->targets.back().source_target==std::nullopt,"A new native VRoid key never claims an original source target ID");
 e.action=ShapeKeyAction::Replace;e.target=0;e.positions=s.active().painted->mesh().positions;for(size_t v=0;v<e.positions.size();++v)e.positions[v]+=s.active().deformation->morphs->targets[0].delta[v];e.positions[0].y+=.0001f;
 author_shapekey(s,id,e);auto edited=s.active().material_bindings;
 check(edited->normal_frames->current_targets[0]==original->normal_frames->current_targets[0]&&s.active().shape_source->material_bindings->normal_frames==original->normal_frames,"Real artist key editing preserves its source/expression identity and immutable original attribute arrays");
 auto project=prefix;project+="-vroid.dsculpt";s.save(project);auto reopened=Scene::load(project);e.action=ShapeKeyAction::Restore;author_shapekey(reopened,id,e);check(reopened.encode()==before,"Actual VRoid before-state restores exactly after native project reopen");
}
}
int main(int argc,char**argv)try{
 if(argc==3&&std::string_view(argv[1])=="--ui-fixture"){fixture().save(argv[2]);return 0;}
 if(argc==3&&std::string_view(argv[1])=="--inspect"){std::cout<<Scene::load(argv[2]).encode().dump()<<'\n';return 0;}
 if(argc!=2)return 2;standalone(argv[1]);avatar(argv[1]);std::cout<<"PASS "<<checks<<" shapekey authoring checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
