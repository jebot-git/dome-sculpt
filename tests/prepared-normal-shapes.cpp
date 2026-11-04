// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/polygon-scene.hpp"
#include "dome/shapekey-authoring.hpp"
#include "normal-projection-identity.hpp"
#include "prepared-normal-shapes-fixtures.hpp"
#include <atomic>
#include <iostream>
#include <thread>
using namespace dome;
namespace gate {
std::atomic<bool> armed=false,entered=false,released=false;
void arm(){entered=false;released=false;armed=true;}
void release(){released=true;released.notify_all();}
void wait(){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(20);while(!entered&&std::chrono::steady_clock::now()<end)std::this_thread::yield();if(!entered)throw std::runtime_error("Prepared shape worker barrier timeout");}
}
extern "C" ShapeKeyResult __real__ZN4dome15author_shapekeyERNS_5SceneEmRKNS_12ShapeKeyEditERKSt8functionIFbvEE(Scene&,uint64_t,const ShapeKeyEdit&,const std::function<bool()>&);
extern "C" ShapeKeyResult __wrap__ZN4dome15author_shapekeyERNS_5SceneEmRKNS_12ShapeKeyEditERKSt8functionIFbvEE(Scene &s,uint64_t id,const ShapeKeyEdit &e,const std::function<bool()> &c){
 if(gate::armed.exchange(false)){gate::entered=true;while(!gate::released)gate::released.wait(false);}
 return __real__ZN4dome15author_shapekeyERNS_5SceneEmRKNS_12ShapeKeyEditERKSt8functionIFbvEE(s,id,e,c);
}
namespace {
unsigned checks=0;void check(bool b,const char *m){if(!b)throw std::runtime_error(m);++checks;std::cout<<"PASS "<<m<<'\n';}
bool refuses(auto f){try{f();return false;}catch(const std::exception&){return true;}}
bool same_vectors(const std::vector<Vec3> &a,const std::vector<Vec3> &b){if(a.size()!=b.size())return false;for(size_t n=0;n<a.size();++n)if(a[n].x!=b[n].x||a[n].y!=b[n].y||a[n].z!=b[n].z)return false;return true;}
const NormalFrameTarget &target(const NormalFrames &f,uint64_t id){for(const auto &t:f.targets)if(t.id==id)return t;throw std::runtime_error("Missing prepared target ID");}
void ready(Document &d){check(d.wait_asset()&&d.wait_mesh(),"Prepared key worker and immutable render snapshot finish");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}

nlohmann::json rig(const DeformationState &d,const Mesh &m){auto j=d.encode(m);j.erase("morphs");j.erase("morph_weights");j.erase("expressions");return j;}
struct Hand{
 Document &doc;SpatialFrame f;
 explicit Hand(Document &d):doc(d){f.focused=f.head_valid=true;f.head.position={0,1.6f,0};f.hands[1].tracked.valid=true;send();}
 void send(float x=0,float y=0,bool press=false){++f.sequence;f.time+=.05;f.hands[1].x=x;f.hands[1].y=y;f.hands[1].joystick=press;doc.submit(f);if(!doc.wait_input())throw std::runtime_error("Prepared shape controller timeout");}
 void sector(int i){const float a=i*3.14159265358979323846f/6;send(std::cos(a),std::sin(a));check(doc.frame().state->menu_index==i,"Controller deflection observes intended shape sector");send(std::cos(a),std::sin(a),true);send();}
 void open(){send(0,0,true);send();sector(8);sector(9);sector(7);}
};
}
int main(int argc,char **argv)try{
 if(argc==3&&std::string_view(argv[1])=="--ui-fixture"){prepared_shape_fixture::model(true).save(argv[2]);return 0;}
 if(argc==3&&std::string_view(argv[1])=="--inspect"){std::cout<<Scene::load(argv[2]).encode().dump()<<'\n';return 0;}
 if(argc!=2)return 2;const std::filesystem::path prefix=argv[1];std::filesystem::create_directories(prefix.parent_path());auto scene=prepared_shape_fixture::model();const auto original=scene.encode();const auto paint=scene.active().painted;const auto pixels=paint->encode();const auto material=original.at("objects").at(0).at("material");
 const auto first_recipe=scene.active().material_bindings->normal_preparation;const auto old_raw_source=scene.active().shape_source;const auto initial_count=scene.active().deformation->morphs->targets.size();const auto old_rig=rig(*scene.active().deformation,paint->mesh());
 Document doc;doc.replace(scene);ready(doc);ShapeKeyEdit edit;edit.name="Prepared new";doc.author_shape(edit);ready(doc);auto authored=doc.copy_scene();
 check(authored.active().painted->mesh_owner()==paint->mesh_owner()&&authored.active().painted->baked()==paint->baked()&&authored.active().painted->encode()==pixels&&authored.encode().at("objects").at(0).at("material")==material,"Create retains exact paint/detail/layers/masks and current material values");
 check(authored.active().shape_source->paint->encode()==pixels&&authored.active().shape_source->material_bindings->normal_preparation==first_recipe&&authored.active().shape_source->shape_source==old_raw_source,"Actual source capsule keeps complete old prepared model and older raw source chain");
 check(rig(*authored.active().deformation,paint->mesh())==old_rig&&authored.active().deformation->morph_weights[0]==.35f,"Current edited skin and anisotropic pose survive reauthoring without historical rig replacement");
 check(authored.active().material_bindings->normal_preparation!=first_recipe&&detail::normal_projection_identity(*paint->normal_projection(),*authored.active().material_bindings->normal_preparation->initial_projection),"New recipe deep-replays edited explicit attributes while retaining proven immutable intrinsic owners");
 const auto first_capsule=authored.active().shape_source;const auto stable=authored.active().material_bindings->normal_frames->current_targets.at(initial_count);
 check(doc.undo()&&doc.copy_scene().encode()==original&&doc.redo()&&doc.copy_scene().encode()==authored.encode(),"One Undo/Redo retains exact normal model, source lineage and authored endpoint");ready(doc);
 edit.action=ShapeKeyAction::Replace;edit.target=initial_count;edit.positions=paint->mesh().positions;edit.positions[2].z=.07f;doc.author_shape(edit);ready(doc);auto replaced=doc.copy_scene();
 check(replaced.active().material_bindings->normal_frames->current_targets[initial_count]==stable&&replaced.active().shape_source==first_capsule&&replaced.active().painted->mesh_owner()==paint->mesh_owner()&&replaced.active().painted->encode()==pixels,"Replacement preserves stable ID and first prepared lineage without adding persistent source per stroke");
 auto before_noop=doc.copy_scene().encode();doc.author_shape(edit);ready(doc);check(doc.copy_scene().encode()==before_noop,"Identical prepared key replacement remains a true no-op");
 check(doc.undo()&&doc.copy_scene().encode()==authored.encode()&&doc.redo()&&doc.copy_scene().encode()==before_noop,"No-op authoring adds no hidden native history entry");ready(doc);
 edit.action=ShapeKeyAction::Duplicate;edit.name="Prepared duplicate";doc.author_shape(edit);ready(doc);
 edit.action=ShapeKeyAction::Mirror;edit.mirror_frames=ShapeKeyMirrorFrames::VertexPositions;edit.name="Prepared mirror";doc.author_shape(edit);ready(doc);
 edit.action=ShapeKeyAction::CaptureMix;edit.name="Prepared captured mix";doc.author_shape(edit);ready(doc);
 edit.action=ShapeKeyAction::Rename;edit.target=initial_count;edit.name="Prepared renamed";doc.author_shape(edit);ready(doc);
 check(doc.copy_scene().active().deformation->morphs->targets.size()==initial_count+4&&doc.copy_scene().active().deformation->morphs->targets[initial_count].name==edit.name,"Native Duplicate/Mirror/CaptureMix/Rename all reauthor owning prepared recipes");
 const auto accepted=doc.copy_scene().encode();gate::arm();edit={};edit.name="Cancelled prepared";doc.author_shape(edit);gate::wait();doc.cancel_background_work();gate::release();ready(doc);check(doc.copy_scene().encode()==accepted,"Actual queued prepared authoring cancellation publishes no partial frames or history");
 {Document stale;stale.replace(doc.copy_scene());ready(stale);gate::arm();edit.name="Stale prepared";stale.author_shape(edit);gate::wait();stale.replace(scene);gate::release();ready(stale);check(stale.copy_scene().encode()==original,"Document replacement rejects stale prepared authoring without admitting old source or selection state");}
 Hand hand(doc);hand.open();check(doc.frame().state->menu_page==24,"Actual joystick navigation reaches the accepted shape workflow on prepared targets");hand.sector(2);ready(doc);check(doc.copy_scene().active().deformation->morphs->targets.size()==initial_count+5,"Controller Duplicate reaches the same prepared-normal adapter");doc.cancel_spatial_menu();
 auto path=prefix;path+=".dsculpt";doc.save(path);Document reopened;reopened.open(path);ready(reopened);check(reopened.copy_scene().encode()==doc.copy_scene().encode(),"Actual Save/new Document reopen retains authored recipes, sources and current rig");
 auto glb=prefix;glb+=".glb";reopened.export_asset(glb).get();check(std::filesystem::file_size(glb)>0,"Actual background GLB export accepts newly authenticated prepared direction targets");
 auto repeated=reopened.copy_scene();const auto repeat_projection=repeated.active().painted->normal_projection();ObjectBakeSettings settings;settings.front_mm=500;settings.back_mm=50;settings.ao_samples=1;settings.padding=2;auto posed_before=repeated.encode();check(refuses([&]{bake_object(repeated,2,settings);})&&repeated.encode()==posed_before,"Posed repeat-bake refusal preserves the exact authored model before explicit rest reset");auto rest_state=*repeated.active().deformation;rest_state.pose.clear();rest_state.morph_weights.clear();repeated.set_deformation(1,rest_state);bake_object(repeated,2,settings);check(repeated.active().painted->normal_projection()==repeat_projection,"Repeat bake after key edits retains intrinsic model and new target authority");repeated.active().painted=repeated.active().painted->snapshot();repeated.active().painted->clear_bake();repeated.validate();check(repeated.active().material_bindings->normal_preparation&&repeated.active().painted->normal_projection(),"Clear bake keeps edited explicit normal model and its retained original recipe");
 auto restored=doc.copy_scene();edit={};edit.action=ShapeKeyAction::Restore;author_shapekey(restored,1,edit);check(restored.encode()==original,"Restore recovers exact first prepared state including older raw source after restart-capable authoring");
 auto cancelled=doc.copy_scene();auto bytes=cancelled.encode();check(refuses([&]{author_shapekey(cancelled,1,{},[]{return true;});})&&cancelled.encode()==bytes,"Detached prepared cancellation preserves full source graph exactly");
 auto n_only=prepared_shape_fixture::model(true);const auto n_before=n_only.encode();const auto n_recipe=n_only.active().material_bindings->normal_preparation;
 check(n_recipe->original_material->normal_frames->pbr->tangents.empty()&&!n_recipe->target_frames->pbr->tangents.empty(),"Genuine N-only source retains absent artist tangents while preparation explicitly generates current T");
 const auto &input=*n_recipe->target_frames->pbr;auto expected=target(input,n_recipe->target_frames->current_targets[0]);for(auto &v:expected.normals)v=v*.35f;for(auto &v:expected.tangents)v=v*.35f;
 ShapeKeyEdit capture;capture.action=ShapeKeyAction::CaptureMix;capture.name="Current prepared directions";author_shapekey(n_only,1,capture);
 const auto nf=n_only.active().material_bindings->normal_frames;const auto &actual=target(*nf->pbr,nf->current_targets.back());
 check(same_vectors(actual.normals,expected.normals)&&same_vectors(actual.tangents,expected.tangents),"N-only CaptureMix preserves exact current linear N/T directions instead of regenerating a different tangent endpoint");
 check(n_only.active().shape_source->material_bindings->normal_preparation==n_recipe&&n_recipe->original_material->normal_frames->pbr->tangents.empty()&&nf->pbr->generated_tangents,"Source capsule preserves old raw N-only recipe and generated-T provenance without an imported-T claim");
 capture.action=ShapeKeyAction::Restore;author_shapekey(n_only,1,capture);check(n_only.encode()==n_before,"N-only prepared source restores its complete original directions, pixels and pose");
 std::cout<<"PASS "<<checks<<" prepared normal shape checks\n";return 0;
}catch(const std::exception &e){gate::release();std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
