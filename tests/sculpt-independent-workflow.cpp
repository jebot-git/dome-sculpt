// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/recovery.hpp"
#include "sculpt-scene-chunks.hpp"
#include "sculpt-independent-fixture.hpp"
#include <nlohmann/json.hpp>
#include <bit>
#include <fstream>
#include <iostream>
#include <set>
#include <thread>
#include <cstdint>
#include <vector>
using namespace dome;using namespace dome::detail;
namespace {
size_t checks=0;
void check(bool value,const char *why){if(!value)throw std::runtime_error(why);++checks;}
std::vector<std::uint8_t> native_bytes(const Scene &scene){
 // Retain the complete native encoding, including every field and float bit,
 // instead of keeping several expanded JSON trees alive beside real workers.
 return nlohmann::json::to_cbor(scene.encode());
}
void ready(Document &d){check(d.wait_asset(std::chrono::seconds(120)),"Actual file/asset worker finishes");check(d.wait_mesh(std::chrono::seconds(120)),"Actual mesh worker publishes");check(d.frame().state->error.empty(),"Actual publication has no asynchronous error");}
void recovery_ready(Document &d){
 const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(120);
 while(d.frame().state->recovery_busy&&std::chrono::steady_clock::now()<until)
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
 const auto state=d.frame().state;
 check(!state->recovery_busy,"Actual recovery catalogue worker finishes");
 check(state->recovery_error.empty(),"Recovery discovery has no asynchronous error");
 check(state->error.empty(),"Catalogue publication has no document error");
}
bool bits(Vec3 a,Vec3 b){return std::bit_cast<uint32_t>(a.x)==std::bit_cast<uint32_t>(b.x)&&std::bit_cast<uint32_t>(a.y)==std::bit_cast<uint32_t>(b.y)&&std::bit_cast<uint32_t>(a.z)==std::bit_cast<uint32_t>(b.z);}
void geometry(Document &d){
 const auto scene=d.copy_scene();const auto frame=d.frame();check(scene.objects().size()==4&&frame.surface->sculpt_chunks&&frame.surface->sculpt_chunks->objects().size()==4,"Four real independent sculpt objects publish through Document");
 size_t triangles=0;
 for(const auto &object:scene.objects()){
  check(object.volume->voxel_size()==.0125f&&object.volume->voxels().size()>500000&&object.volume->at({0,0,0})==-object.volume->band(),"Production source retains full-size negative interior and original voxel density");
  check(object.volume->layers().size()==2&&!object.volume->mask_values().empty(),"Every object retains genuine editable layers and sparse masks");
  const auto *view=frame.surface->sculpt_chunks->find(object.id);check(view&&view->geometry->source_owner()->geometry_owner()==object.volume->geometry_owner(),"Derived geometry retains its own exact source field owner");
  const auto expected=surface_mesh(*object.volume);const auto actual=view->geometry->materialize();check(actual.indices.size()==expected.indices.size(),"Independent whole mesher has the same complete topology size");
  for(size_t c=0;c<actual.indices.size();++c){const auto a=actual.indices[c],b=expected.indices[c];if(!bits(actual.positions[a],expected.positions[b])||!bits(actual.normals[a],expected.normals[b]))throw std::runtime_error("Independent full-size whole-mesher corner/normal bit mismatch");}
  ++checks;triangles+=actual.indices.size()/3;
 }
 check(triangles>1'000'000,"Independent production workload keeps over one million real triangles");
 check(frame.surface->sculpt_chunks->retained_bytes()<=512*1024*1024,"Actual independent scene retains the unchanged512MiB envelope");
}
NativeStrokeSample input(){NativeStrokeSample s;s.origin={.15f,.09f,1.5f};s.direction={0,0,-1};s.brush.radius=.045f;s.brush.blend=.006f;s.brush.strength=.23f;s.brush.tool=Tool::Clay;return s;}
void gesture(Document &d,bool accept){check(d.begin_native_stroke(),"Actual native gesture begins");check(d.sample_native_stroke(input()),"Actual native controller sample is accepted");d.end_native_stroke(accept);check(d.wait_native_stroke(std::chrono::seconds(120)),"Native worker terminates");ready(d);}
std::string bytes(const std::filesystem::path &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
}
int main(int argc,char **argv)try{
 check(argc==2||argc==3,"Usage: sculpt-independent-workflow output [accepted-native-project]");const std::filesystem::path output=argv[1];check(std::filesystem::create_directory(output),"Preserve previous outputs in their own namespace");
 Scene source=argc==3?Scene::load(argv[2]):sculpt_independent_fixture::scene();const auto original=native_bytes(source);
 Document d;d.replace(source.snapshot());ready(d);geometry(d);check(native_bytes(d.copy_scene())==original,"Cold publication preserves every original authored source bit");
 const auto previous=d.frame().surface;gesture(d,false);check(native_bytes(d.copy_scene())==original,"Cancelled real native stroke preserves exact complete source");
 gesture(d,true);const auto accepted=native_bytes(d.copy_scene());check(accepted!=original,"Accepted native stroke makes real geometry changes");check(d.undo(),"One native gesture has one Undo");ready(d);check(native_bytes(d.copy_scene())==original,"Undo restores all independent objects exactly");check(d.redo(),"Native gesture Redo is available");ready(d);check(native_bytes(d.copy_scene())==accepted,"Redo restores all independent objects exactly");
 const auto before_mask=d.frame().surface->sculpt_chunks->find(1)->geometry;d.sculpt_mask_invert();ready(d);check(native_bytes(d.copy_scene())!=accepted,"Actual background mask inversion changes protection");check(d.frame().surface->sculpt_chunks->find(1)->geometry==before_mask,"Mask editing preserves exact geometry ownership");check(d.undo(),"Mask inversion Undo is available");ready(d);check(native_bytes(d.copy_scene())==accepted,"Mask Undo restores authored mask and geometry bytes");
 const auto layer=d.copy_scene().active().volume->active_layer();const auto name=d.frame().state->sculpt_layer_name;d.sculpt_layer_properties(1,layer,name,true,false,.7f);ready(d);check(native_bytes(d.copy_scene())!=accepted,"Layer opacity performs real evaluated geometry work");check(d.undo(),"Layer opacity Undo is available");ready(d);check(native_bytes(d.copy_scene())==accepted,"Layer Undo restores exact complete source");geometry(d);
 const auto native=output/"accepted.dsculpt";auto saved=d.save_async(native);saved.get();ready(d);check(native_bytes(Scene::load(native))==accepted,"Actual ordered native Save retains complete independent source");const auto saved_bytes=bytes(native);
 {Document reopened;reopened.open(native);ready(reopened);check(native_bytes(reopened.copy_scene())==accepted,"Fresh native Document.open preserves all source data");geometry(reopened);}
 RecoverySource recovery_source{new_recovery_id(),1,native};const auto root=output/"recovery";
 {RecoveryService service(root);check(service.set_source(recovery_source),"Recovery source is accepted");check(service.enqueue({recovery_source,1,1,d.copy_scene()})==RecoveryEnqueue::Accepted,"Actual detached four-object scene enters recovery worker");check(service.flush(std::chrono::seconds(120))&&service.status().error.empty(),"Durable recovery checkpoint commits");service.shutdown();}
 {const auto found=RecoveryService::discover(root);check(found.errors.empty()&&found.candidates.size()==1,"Fresh recovery discovery finds exactly the checked checkpoint");check(native_bytes(RecoveryService::load(found.candidates[0]))==accepted,"Recovery file retains original authored bits");}
 {Document recovered;recovered.enable_recovery(root);recovered.refresh_recovery();recovery_ready(recovered);check(!recovered.frame().state->recovery_candidates.empty(),"Actual Document discovers recovery on its file worker");recovered.restore_recovery(0);ready(recovered);check(native_bytes(recovered.copy_scene())==accepted,"Actual Document recovery restores exact four-object source");geometry(recovered);}
 check(bytes(native)==saved_bytes,"Recovery never overwrites the user's accepted project");
 {auto scene=d.copy_scene();auto cold=SculptSceneChunks::capture(scene,{987,1,1,true,true});const auto exact=cold.chunks->retained_bytes();SculptSceneChunkLimits limits;limits.retained_bytes=exact;auto kept=SculptSceneChunks::capture(scene,{988,1,1,true,true},{},limits);check(kept.chunks->retained_bytes()==exact,"Cold independent capture fits its exact unchanged retained footprint");--limits.retained_bytes;bool refused=false;try{SculptSceneChunks::capture(scene,{989,1,1,true,true},{},limits);}catch(const std::length_error&){refused=true;}check(refused&&native_bytes(d.copy_scene())==accepted,"One-byte-too-small retained quota refuses without source mutation");size_t polls=0;bool cancelled=false;try{SculptSceneChunks::capture(scene,{990,1,1,true,true},{},{},[&]{return++polls>=32;});}catch(const std::runtime_error&){cancelled=true;}check(cancelled&&native_bytes(d.copy_scene())==accepted,"Cold correspondence cancellation preserves complete source");}
 check(previous->sculpt_chunks->objects().size()==4&&previous->sculpt_chunks->find(1)->geometry->report().indices>0,"Previously acquired immutable scene remains valid through editing, Save and recovery");
 std::cout<<"PASS independent production sculpt workflow checks="<<checks<<" objects=4 voxel=.0125 layers/masks/history/orderedSave/coldOpen/recovery/exactCorners\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
