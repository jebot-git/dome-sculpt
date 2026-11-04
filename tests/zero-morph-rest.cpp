// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/scene-file.hpp"
#include <cstring>
#include <iostream>
using namespace dome;
namespace {
size_t checks=0;
void check(bool value,const char *why){if(!value)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
bool exact_mesh(const Mesh &a,const Mesh &b){return a.positions.size()==b.positions.size()&&a.normals.size()==b.normals.size()&&a.indices==b.indices&&a.uv==b.uv&&a.uv_pins==b.uv_pins&&a.uv_seams==b.uv_seams&&std::memcmp(a.positions.data(),b.positions.data(),a.positions.size()*sizeof(Vec3))==0&&std::memcmp(a.normals.data(),b.normals.data(),a.normals.size()*sizeof(Vec3))==0;}
Mesh mesh(float radius,float z){Mesh m;m.positions={{-radius,-radius,z},{radius,-radius,z},{radius,radius,z},{-radius,radius,z}};m.normals.assign(4,{0,0,1});m.indices={0,1,2,0,2,3};m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.uv_pins={1,0,0,1};m.uv_seams={1,0,1,0,1,0};return m;}
Scene fixture(){Scene scene;auto low=mesh(.1f,0);low.normals={normalized(Vec3{.1f,.1f,1}),normalized(Vec3{-.6f,.2f,1}),normalized(Vec3{.3f,.7f,1}),normalized(Vec3{.6f,-.4f,1})};scene.active().mesh_active=true;scene.active().painted=std::make_shared<PaintedMesh>(low,64);DeformationState d(low);d.morphs=MorphSet(low);auto edited=low.positions;edited[0].z=.02f;d.morphs->set_target(low,"Lift",edited);edited=low.positions;edited[2].z=-.02f;d.morphs->set_target(low,"Lower",edited);d.morph_weights={0.f,-0.f};scene.set_deformation(1,d);scene.add("Read-only high source",Volume{});scene.active().mesh_active=true;scene.active().visible=false;scene.active().locked=true;scene.active().painted=std::make_shared<PaintedMesh>(mesh(.3f,.02f),64);scene.select(1);return scene;}
void ready(Document &d){if(!d.wait_asset()||!d.wait_mesh()||!d.frame().state->error.empty())throw std::runtime_error("Native worker failed: "+d.frame().state->error);}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);auto initial=fixture();const auto original=initial.encode();const auto &rest=initial.active().painted->mesh();
 check(exact_mesh(initial.active().deformation->preview(rest),rest),"Explicit zero and signed-zero morph weights retain the complete artist-authored rest mesh");
 auto empty=*initial.active().deformation;empty.morph_weights.clear();check(exact_mesh(empty.preview(rest),initial.active().deformation->preview(rest)),"Implicit and explicit zero morph state have identical native shading");
 Document doc;doc.replace(initial.snapshot());ready(doc);check(doc.copy_scene().encode()==original,"Native publication preserves exact morph weights, authored normals and source data");
 ObjectBakeSettings settings;settings.ao_samples=0;settings.padding=0;
 doc.bake_from_object(2,settings);ready(doc);auto baked=doc.copy_scene();const auto accepted=baked.encode();
 check(bool(baked.active().painted->baked()),"Actual native Bake accepts an unchanged authored rest surface with zero morph weights");
 check(exact_mesh(baked.active().painted->mesh(),rest)&&accepted.at("objects").at(1)==original.at("objects").at(1)&&accepted.at("objects").at(0).at("deformation")==original.at("objects").at(0).at("deformation"),"Actual Bake retains the complete rest mesh, morphs and read-only source");
 check(doc.undo()&&doc.copy_scene().encode()==original&&doc.redo()&&doc.copy_scene().encode()==accepted,"Native Bake remains one exact Undo and Redo transaction");ready(doc);
 doc.save(out/"zero-morph-rest.dsculpt");auto loaded=Scene::load(out/"zero-morph-rest.dsculpt");check(loaded.encode()==accepted&&exact_mesh(loaded.active().deformation->preview(loaded.active().painted->mesh()),rest),"Native Save and restart retain authored rest normals and signed-zero morph state");
 doc.export_asset(out/"zero-morph-rest.glb",AssetExportFormat::EditableGlb256).get();auto recovered=load_scene_file(out/"zero-morph-rest.glb").scene;check(recovered.encode()==accepted&&exact_mesh(recovered.active().deformation->preview(recovered.active().painted->mesh()),rest),"Actual editable GLB worker and native recovery retain exact authored rest shading");
 auto posed=initial;auto changed=*posed.active().deformation;changed.morph_weights={1,0};posed.set_deformation(1,changed);const auto before=posed.encode();bool refused=false;try{bake_object(posed,2,settings);}catch(const std::logic_error &){refused=true;}check(refused&&posed.encode()==before,"A genuinely active morph still refuses rest-only Bake without changing any scene data");
 std::cout<<"PASS "<<checks<<" native zero-morph rest checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
