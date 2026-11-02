// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/material-bindings.hpp"
#include "dome/shapekey-authoring.hpp"
#include "dome/import-authoring.hpp"
#include "dome/source-maps.hpp"
#include "import-authoring-fixtures.hpp"
#include <iostream>
using namespace dome;
namespace {
unsigned checks=0;const char *notice="Prepare an editable texture atlas before changing geometry keys with original UV2+ normals";
void check(bool value,const char *why){if(!value)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
std::array<Vec3,3> basis(const std::vector<Vec3>&p,const Mesh&m,const std::vector<std::array<float,2>>&uv){
 auto a=m.indices[0],b=m.indices[1],c=m.indices[2];auto e1=p[b]-p[a],e2=p[c]-p[a];auto q0=uv[a],q1=uv[b],q2=uv[c];float u1=q1[0]-q0[0],v1=q1[1]-q0[1],u2=q2[0]-q0[0],v2=q2[1]-q0[1],det=u1*v2-u2*v1;
 auto n=normalized(cross(e1,e2)),t=normalized((e1*v2-e2*v1)/det);auto raw_b=(e2*u1-e1*u2)/det;auto b_axis=cross(n,t)*(dot(cross(n,t),raw_b)<0?-1.f:1.f);return {t,b_axis,n};
}
Scene source(const std::filesystem::path&file,bool atlas=false){ImportAuthoringOptions o;o.paint_resolution=128;o.prepare_texture_atlas=atlas;return prepare_authoring_import(Scene{},file,o).scene;}
void ready(Document&d){if(!d.wait_asset()||!d.wait_mesh()||!d.frame().state->error.empty())throw std::runtime_error("Key admission endpoint: "+d.frame().state->error);}
template<class F>bool refused(F f){try{f();return false;}catch(const std::exception&e){std::cout<<"REFUSAL "<<e.what()<<std::endl;return e.what()==std::string_view(notice);}}
}
int main(int argc,char**argv)try{
 if(argc<2)return 2;const std::filesystem::path root=argv[1];std::filesystem::create_directories(root);import_fixture::Fixture f;auto &p=f.j["meshes"][0]["primitives"][0];p["attributes"].erase("TANGENT");p["targets"][0].erase("TANGENT");f.j["accessors"][p["indices"].get<int>()]["count"]=3;
 while(f.bin.size()%4)f.bin.push_back(0);auto uv=f.values({0,0,0,1,1,1,1,0},2,"VEC2");p["attributes"]["TEXCOORD_2"]=uv;f.j["materials"][0]["normalTexture"]={{"index",0},{"texCoord",2}};f.j["materials"][0].erase("extensions");f.j.erase("extensionsUsed");auto file=root/"generated-uv2.glb";f.save(file);
 auto initial=source(file);const auto id=initial.active().id;const auto &object=initial.active();const auto &rest=object.painted->mesh();const auto frame=object.material_bindings->normal_frames->pbr;
 check(frame&&frame->generated_uv&&*frame->generated_uv==2&&object.painted->source_maps()->normal.texcoord==2,"Actual raw native import retains a generated UV2 tangent frame and its distinct normal role");
 const auto before=initial.encode();ShapeKeyEdit edit;edit.action=ShapeKeyAction::Replace;edit.target=0;std::vector<Vec3> old=rest.positions;for(size_t v=0;v<old.size();++v)old[v]+=object.deformation->morphs->targets[0].delta[v];edit.positions.reserve(old.size());for(auto p:old)edit.positions.push_back({1.8f*p.x+.7f*p.y,.3f*p.x+.9f*p.y,.2f*p.x+.4f*p.y});
 if(argc>2&&std::string_view(argv[2])=="--probe"){
  auto edited=initial;author_shapekey(edited,id,edit);auto now=edited.active().material_bindings->normal_frames->pbr;const auto &previous=frame->targets[0],&actual=now->targets[0];auto a=basis(old,rest,*object.painted->source_maps()->extra_uv_sets.at(2)),b=basis(edit.positions,rest,*object.painted->source_maps()->extra_uv_sets.at(2));float ne=0,te=0;
  for(size_t c=0;c<frame->normals.size();++c){auto rotate=[&](Vec3 v){return b[0]*dot(v,a[0])+b[1]*dot(v,a[1])+b[2]*dot(v,a[2]);};auto n=frame->normals[c]+previous.normals[c];auto base=frame->tangents[c];Vec3 tangent{base[0],base[1],base[2]};auto t=tangent+(previous.tangents.empty()?Vec3{}:previous.tangents[c]);ne=std::max(ne,length(actual.normals[c]-(rotate(n)-frame->normals[c])));te=std::max(te,length(actual.tangents[c]-(rotate(t)-tangent)));}
  std::cout<<"UNSUPPORTED_RAW_KEY_UV2 normal_error="<<ne<<" tangent_error="<<te<<std::endl;check(te>1e-4f,"Independent single-triangle role-UV Jacobian detects UV0 geometric reauthoring on a UV2 normal material");ShapeKeyEdit restore;restore.action=ShapeKeyAction::Restore;author_shapekey(edited,id,restore);check(edited.encode()==before,"Old-source probe retains exact original source through restoration");return 3;
 }
 auto direct=initial;check(refused([&]{author_shapekey(direct,id,edit);})&&direct.encode()==before,"Direct geometry-key admission refuses unsupported original UV2 normal transport atomically");
 auto mirror=edit;mirror.action=ShapeKeyAction::Mirror;check(refused([&]{author_shapekey(direct,id,mirror);})&&direct.encode()==before,"Direct mirrored geometry-key admission retains the complete raw UV2 source on refusal");
 Document d;d.replace(initial);ready(d);auto epoch=d.frame().state->edit_authority,rev=d.frame().state->revision;check(refused([&]{d.author_shape(edit);})&&!d.frame().state->asset_busy&&d.copy_scene().encode()==before&&d.frame().state->edit_authority==epoch&&d.frame().state->revision==rev&&!d.frame().state->can_undo,"Native geometry-key request refuses before work/history/authority publication");check(refused([&]{d.author_shape(mirror);})&&!d.frame().state->asset_busy&&d.copy_scene().encode()==before,"Native mirrored-key refusal publishes no accepted work or history");check(refused([&]{d.configure_shape_sculpt(true);})&&!d.frame().state->shape_sculpt&&!d.frame().state->native_pending,"Native/controller sculpt-mode admission refuses unsupported original UV2 normal basis before accepting a stroke");
 for(auto action:{ShapeKeyAction::Duplicate,ShapeKeyAction::CaptureMix,ShapeKeyAction::Create}){auto scene=initial;ShapeKeyEdit supported;supported.action=action;supported.target=0;supported.name="Exact retained attributes";author_shapekey(scene,id,supported);check(scene.active().material_bindings->normal_frames->pbr->generated_uv==frame->generated_uv,"Non-geometric key action retains exact original UV2 base tangent provenance");ShapeKeyEdit restore;restore.action=ShapeKeyAction::Restore;author_shapekey(scene,id,restore);check(scene.encode()==before,"Non-geometric raw key action restores the complete original source exactly");}
 auto atlas=source(file,true);check(!atlas.active().painted->source_maps()->uses_alternate_uv()&&atlas.active().material_bindings->uv_origin->paint->source_maps()->normal.texcoord==2,"Explicit atlas has current UV0 normal and only retained original UV2 provenance");auto positive=edit;positive.positions.clear();const auto &mesh=atlas.active().painted->mesh();for(size_t v=0;v<mesh.positions.size();++v)positive.positions.push_back(mesh.positions[v]+atlas.active().deformation->morphs->targets[0].delta[v]+Vec3{0,0,.01f});author_shapekey(atlas,atlas.active().id,positive);check(atlas.active().shape_source!=nullptr,"Already prepared active UV0 keys remain authorable with original UV2 only in their before-state");
 std::cout<<"PASS "<<checks<<" source UV2 key admission checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
