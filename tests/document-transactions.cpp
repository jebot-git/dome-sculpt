// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/source-maps.hpp"
#include "dome/import-authoring.hpp"
#include <nlohmann/json.hpp>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#include <tuple>
#include <unistd.h>
using namespace dome;
namespace {
std::atomic_bool enabled{false},hit{false},phase_tracking{true},postcommit_mode{false};std::atomic<size_t> calls{0},failure{0};
thread_local bool permitted=false,inside=false,whole=false,post_tracking=false;
void require(bool x,const char *m){if(!x)throw std::runtime_error(m);}
}
void *operator new(size_t n){
 if(enabled&&(inside||whole||post_tracking)){auto at=++calls;if(at==failure.load()){failure=0;hit=true;throw std::bad_alloc();}}
 if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();
}
void operator delete(void *p)noexcept{std::free(p);}void operator delete(void *p,size_t)noexcept{std::free(p);}
extern "C" void ctor_real(void*,Document&) asm("__real__ZN4dome8Document11TransactionC1ERS0_");
extern "C" void ctor_wrap(void *self,Document &d) asm("__wrap__ZN4dome8Document11TransactionC1ERS0_");
extern "C" void ctor_wrap(void *self,Document &d){if(enabled&&permitted&&phase_tracking)inside=true;try{ctor_real(self,d);}catch(...){inside=false;throw;}}
extern "C" void commit_real(void*,bool) asm("__real__ZN4dome8Document11Transaction6commitEb");
extern "C" void commit_wrap(void *self,bool checkpoint) asm("__wrap__ZN4dome8Document11Transaction6commitEb");
extern "C" void commit_wrap(void *self,bool checkpoint){inside=false;if(!phase_tracking)whole=false;post_tracking=postcommit_mode;commit_real(self,checkpoint);post_tracking=false;}
extern "C" void dtor_real(void*) asm("__real__ZN4dome8Document11TransactionD1Ev");
extern "C" void dtor_wrap(void*) asm("__wrap__ZN4dome8Document11TransactionD1Ev");
extern "C" void dtor_wrap(void *self){inside=false;dtor_real(self);}
extern "C" void uv_real(Document*,void*) asm("__real__ZN4dome8Document19process_uv_transferERNS0_7FileJobE");
extern "C" void uv_wrap(Document*,void*) asm("__wrap__ZN4dome8Document19process_uv_transferERNS0_7FileJobE");
extern "C" void uv_wrap(Document *d,void *j){permitted=true;try{uv_real(d,j);}catch(...){permitted=false;throw;}permitted=false;}
extern "C" void save_real(const Scene*,const std::filesystem::path&) asm("__real__ZNK4dome5Scene4saveERKNSt10filesystem7__cxx114pathE");
extern "C" void save_wrap(const Scene*,const std::filesystem::path&) asm("__wrap__ZNK4dome5Scene4saveERKNSt10filesystem7__cxx114pathE");
extern "C" void save_wrap(const Scene *s,const std::filesystem::path &p){save_real(s,p);permitted=true;}
extern "C" Scene load_real(const std::filesystem::path&,const std::function<bool()>&) asm("__real__ZN4dome5Scene4loadERKNSt10filesystem7__cxx114pathERKSt8functionIFbvEE");
extern "C" Scene load_wrap(const std::filesystem::path&,const std::function<bool()>&) asm("__wrap__ZN4dome5Scene4loadERKNSt10filesystem7__cxx114pathERKSt8functionIFbvEE");
extern "C" Scene load_wrap(const std::filesystem::path &p,const std::function<bool()> &c){auto s=load_real(p,c);permitted=true;return s;}
extern "C" Scene recovery_load_real(const RecoveryCandidate&,RecoveryOptions) asm("__real__ZN4dome15RecoveryService4loadERKNS_17RecoveryCandidateENS_15RecoveryOptionsE");
extern "C" Scene recovery_load_wrap(const RecoveryCandidate&,RecoveryOptions) asm("__wrap__ZN4dome15RecoveryService4loadERKNS_17RecoveryCandidateENS_15RecoveryOptionsE");
extern "C" Scene recovery_load_wrap(const RecoveryCandidate &c,RecoveryOptions o){auto s=recovery_load_real(c,std::move(o));permitted=true;return s;}
// Observe only the real detached helper's successful return. Preparation itself
// is outside this publication-allocation scope; production has no fixture hook.
extern "C" ImportedAuthoring import_real(const Scene&,const std::filesystem::path&,ImportAuthoringOptions) asm("__real__ZN4dome24prepare_authoring_importERKNS_5SceneERKNSt10filesystem7__cxx114pathENS_22ImportAuthoringOptionsE");
extern "C" ImportedAuthoring import_wrap(const Scene&,const std::filesystem::path&,ImportAuthoringOptions) asm("__wrap__ZN4dome24prepare_authoring_importERKNS_5SceneERKNSt10filesystem7__cxx114pathENS_22ImportAuthoringOptionsE");
extern "C" ImportedAuthoring import_wrap(const Scene &s,const std::filesystem::path &p,ImportAuthoringOptions o){auto result=import_real(s,p,std::move(o));permitted=true;return result;}
extern "C" bool procedural_real(Scene&,uint64_t,const ProceduralSculptOptions&,ProceduralSculptReport*) asm("__real__ZN4dome24procedural_sculpt_objectERNS_5SceneEmRKNS_23ProceduralSculptOptionsEPNS_22ProceduralSculptReportE");
extern "C" bool procedural_wrap(Scene&,uint64_t,const ProceduralSculptOptions&,ProceduralSculptReport*) asm("__wrap__ZN4dome24procedural_sculpt_objectERNS_5SceneEmRKNS_23ProceduralSculptOptionsEPNS_22ProceduralSculptReportE");
extern "C" bool procedural_wrap(Scene &s,uint64_t id,const ProceduralSculptOptions &o,ProceduralSculptReport *r){auto result=procedural_real(s,id,o,r);permitted=true;return result;}
namespace dome {struct DocumentTransactionTestAccess {
 static auto tokens(const Document &d){std::lock_guard lock(d.model_mutex_);return std::tuple{d.content_,d.next_content_,d.saved_content_,d.undo_.size(),d.redo_.size(),d.state_.revision,d.state_.generation,d.recovery_content_};}
 static auto source(const Document &d){std::lock_guard lock(d.model_mutex_);return d.native_source_scene_;}
 static size_t recovery_candidate(const Document &d,const std::string &id){std::lock_guard lock(d.model_mutex_);for(size_t i=0;i<d.recovery_catalog_.size();++i)if(d.recovery_catalog_[i].source.id==id)return i;throw std::runtime_error("Fixture recovery candidate missing");}
 static bool wait_recovery(Document &d){std::unique_lock lock(d.file_mutex_);return d.file_changed_.wait_for(lock,std::chrono::seconds(30),[&]{return !d.published_.load()->recovery_busy;});}
 static bool flush_recovery(Document &d){return d.recovery_->flush();}
 static bool control(const Document &d){std::lock_guard lock(d.model_mutex_);return bool(d.control_before_);}
 static void begin_control(Document &d){std::lock_guard lock(d.model_mutex_);d.control_event_locked({VrControlEventType::Begin,VrParameter::BaseColor});d.control_event_locked({VrControlEventType::Absolute,VrParameter::BaseColor,0,.2f});d.publish_locked();}
 static void finish_control(Document &d){std::lock_guard lock(d.model_mutex_);d.finish_control_locked(true);}
};}
namespace {
Scene fixture(){
 Mesh m;m.positions={{-.2f,-.2f,0},{.2f,-.2f,0},{-.2f,.2f,0},{.2f,.2f,0}};m.normals.assign(4,{0,0,1});m.indices={0,1,2,1,3,2};m.uv={{.1f,.1f},{.9f,.1f},{.1f,.9f},{.9f,.9f}};
 auto image=std::make_shared<Image>();image->width=image->height=2;image->rgba={255,0,0,255,0,255,0,255,0,0,255,255,255,255,0,255};
 auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=image;
 Scene s;s.active().mesh_active=true;s.active().painted=PaintedMesh::from_maps(m,maps,64);
 PaintSettings settings;settings.color={.1f,.2f,.7f,.6f};Brush b;b.from=b.to={0,0,0};b.radius=.2f;b.normal={0,0,1};s.active().painted->begin_stroke();s.active().painted->stamp(b,settings);s.active().painted->end_stroke();return s;
}
std::filesystem::path recovery_path(){return std::filesystem::temp_directory_path()/("dome-transaction-recovery-"+std::to_string(getpid()));}
std::filesystem::path fixture_path();
std::filesystem::path import_path(){auto p=fixture_path();p.replace_extension(".input.glb");return p;}
Volume detail_sphere(){Volume v(.04f);v.begin_stroke();Brush b;b.radius=.237f;b.from=b.to={.013f,.017f,-.011f};b.blend=0;v.stamp(b);v.end_stroke();return v.geometry_snapshot();}
std::string restore_id;
Volume coarse_sculpt(){Volume::Voxels samples;for(int z=-1;z<=1;++z)for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x)samples.emplace(Int3{x,y,z},length(Vec3{.2f*x,.2f*y,.2f*z})-.23f);return Volume::from_samples(.2f,std::move(samples));}
std::unique_ptr<Document> setup(const std::string &mode){
 auto d=std::make_unique<Document>();auto initial=fixture();if(mode=="procedural")initial.add("Procedural target",detail_sphere());d->replace(initial);require(d->wait_mesh(),"Baseline mesh ready");
 auto properties=[&](const char *name){auto o=d->copy_scene().active();d->object_properties(o.id,name,o.visible,o.locked,o.transform,o.material);};
 properties("Retained history A");properties("Retained history B");require(d->undo(),"Create retained redo");require(d->wait_mesh(),"History baseline ready");
 if(mode=="control"){DocumentTransactionTestAccess::begin_control(*d);require(d->wait_mesh(),"Accepted color preview is fully ready before its allocation baseline");}
 if(mode=="uv"||mode=="uv-queue"){d->begin_uv();require(d->wait_uv(),"Original-source UV draft ready");d->edit_uv(UvAction::Pin);require(d->wait_uv(),"Pinned UV draft validates");require(!d->frame().state->uv->mesh->uv_pins.empty(),"UV draft has actual metadata change");}
 if(mode=="sculpt-undo"){
  auto s=d->copy_scene();auto v=coarse_sculpt();s.active().volume=std::make_shared<Volume>(v.geometry_snapshot());s.active().mesh_active=false;d->replace(s);require(d->wait_mesh(),"Sculpt source ready");
 }
 if(mode=="paint-undo"||mode=="sculpt-undo"){
  for(int i=0;i<2;++i){require(d->begin_desktop(),"Paint history fixture starts");Brush b;b.from=b.to={.04f*i,0,0};b.radius=.12f;b.normal={0,0,1};if(mode=="sculpt-undo"){b.from=b.to=i?Vec3{.22f,0,0}:Vec3{0,0,.22f};b.tool=Tool::Carve;}d->stamp_desktop(b);d->end_desktop();}
  require(d->undo(),"Retained paint redo");require(d->wait_mesh(),"Paint history baseline ready");
 }
 if(mode=="prepare-queue"){
  auto s=d->copy_scene();s.add("Prepared target",coarse_sculpt(),{{.6f,0,0},{},1});d->replace(s);
  properties("Retained history A");properties("Retained history B");require(d->undo()&&d->wait_mesh(),"Prepare queue source and retained redo ready");
 }
 if(mode=="restore"||mode=="restore-queue"){
  restore_id=new_recovery_id();auto alt=fixture();alt.active().name="External recovered source";
  {RecoveryService service(recovery_path());RecoverySource source{restore_id,1,{}};service.set_source(source);require(service.enqueue({source,1,1,alt.snapshot()})==RecoveryEnqueue::Accepted&&service.flush(),"Independent durable restore source ready");}
  d->enable_recovery(recovery_path());require(DocumentTransactionTestAccess::flush_recovery(*d),"Document recovery baseline ready");
  d->refresh_recovery();require(DocumentTransactionTestAccess::wait_recovery(*d),"Discover independent restore candidate");d->save(fixture_path());
 }
 if(mode=="refresh-queue"){d->enable_recovery(recovery_path());require(DocumentTransactionTestAccess::flush_recovery(*d),"Refresh source ready");}
 if(mode=="recovery"||mode=="postcommit"){d->enable_recovery(recovery_path());require(DocumentTransactionTestAccess::flush_recovery(*d),"Recovery baseline drains");}
 if(mode=="native-save-queue"){require(d->begin_native_stroke(),"Held native gesture before queued Save fault");}
 return d;
}
std::filesystem::path fixture_path(){return std::filesystem::temp_directory_path()/("dome-transaction-"+std::to_string(getpid())+".dsculpt");}
void execute(Document &d,const std::string &mode){
 if(mode.ends_with("-queue")){
  whole=true;try{execute(d,mode.substr(0,mode.size()-6));}catch(...){whole=false;throw;}whole=false;return;
 }
 if(mode=="prepare"){d.prepare_mesh(64);require(d.wait_asset(),"Prepare worker drains");}
 else if(mode=="refresh"){d.refresh_recovery();require(DocumentTransactionTestAccess::wait_recovery(d),"Recovery refresh drains");}
 else if(mode=="restore"){d.restore_recovery(DocumentTransactionTestAccess::recovery_candidate(d,restore_id));require(d.wait_asset(),"Recovery restore drains");}
 else if(mode=="native-begin"){require(d.begin_native_stroke(),"Begin native admission");}
 else if(mode=="export"){auto path=fixture_path();path.replace_extension(".glb");d.export_asset(path,AssetExportFormat::Glb).get();}
 else if(mode=="save"||mode=="native-save"){d.save(fixture_path());}
 else if(mode=="open"){d.open(fixture_path());}
 else if(mode=="asset"){d.edit_scene("Asset allocation fixture",[](Scene &s,const auto &){s.active().name="Accepted asset";permitted=true;});require(d.wait_asset(),"Asset worker drains");}
 else if(mode=="import"){d.import_asset(import_path(),64);require(d.wait_asset(),"Actual import worker drains");}
 else if(mode=="procedural"){ProceduralSculptOptions o;o.rows=o.columns=1;o.density=1;o.depth=.12f;d.apply_procedural_sculpt(o);require(d.wait_asset(),"Actual procedural worker drains");}
 else if(mode=="uv"){d.end_uv(true);require(d.wait_asset(),"UV transfer drains");}
 else {
  permitted=true;whole=mode!="control"&&mode!="recovery"&&mode!="postcommit";
  try{
   if(mode=="structural")d.duplicate_object();
   else if(mode=="scalar"||mode=="recovery"||mode=="postcommit"){auto o=d.copy_scene().active();d.object_properties(o.id,"Accepted scalar",o.visible,o.locked,o.transform,o.material);}
   else if(mode=="control")DocumentTransactionTestAccess::finish_control(d);
   else if(mode=="undo"||mode=="paint-undo"||mode=="sculpt-undo")require(d.undo(),"Undo fixture changes authority");
   else throw std::runtime_error("Unknown fixture mode");
  }catch(...){whole=permitted=inside=false;throw;}
  whole=permitted=inside=false;
 }
}
}
int main(int argc,char **argv)try{
 require(argc==2,"Choose endpoint fixture mode");std::string mode=argv[1];
 if(mode=="structural"){
  auto locked=fixture();locked.active().locked=true;auto id=locked.duplicate(locked.active().id);
  require(!locked.object(id).locked&&locked.object(1).locked,"Staged duplicate preserves unlocked-copy convention and original lock");
  auto invalid=fixture();invalid.active().transform.scale=0;bool refused=false;try{invalid.duplicate(1);}catch(const std::invalid_argument&){refused=true;}
  require(refused&&invalid.objects().size()==1&&invalid.active().id==1,"Invalid duplicate transform refuses before object or identity publication");
 }
 if(mode=="open"||mode=="open-queue"){auto f=fixture();f.active().name="Loaded replacement";f.save(fixture_path());permitted=false;}
 if(mode=="import")export_glb(fixture().surfaces(),import_path());
 postcommit_mode=mode=="postcommit";phase_tracking=!mode.ends_with("-queue")&&!postcommit_mode;
 auto measured=setup(mode);calls=0;failure=0;permitted=inside=whole=false;enabled=true;execute(*measured,mode);enabled=false;
 size_t count=calls.load();std::cerr<<"Measured "<<mode<<": "<<count<<" allocation boundaries\n";require(count>10,"Real allocation boundaries were observed");require(measured->frame().state->error.empty(),"Measured endpoint succeeds");auto expected=measured->copy_scene().encode();
 if(mode=="import")require(measured->copy_scene().objects().size()==2&&bool(measured->copy_scene().active().appearance),"Actual imported source is retained in the publication fixture");
 if(mode=="procedural")require(measured->copy_scene().active().volume->layers().size()==2,"Actual procedural layer is retained in the publication fixture");
 measured.reset();
 for(size_t boundary=1;boundary<=count;++boundary){
  auto d=setup(mode);auto original=d->copy_scene().encode();auto tokens=DocumentTransactionTestAccess::tokens(*d);auto source=DocumentTransactionTestAccess::source(*d);auto mesh=d->frame().surface;auto publication=d->frame().state;auto recovery=d->recovery_status();auto draft=d->frame().state->uv?d->frame().state->uv->mesh:nullptr;
  calls=0;failure=boundary;hit=false;permitted=inside=whole=false;enabled=true;
  try{execute(*d,mode);}catch(const std::bad_alloc&){}enabled=false;failure=0;whole=permitted=inside=false;
  require(hit,"Every measured allocation boundary was injected");
  if(mode=="postcommit"){
   require(d->copy_scene().encode()==expected,"Post-commit persistence allocation cannot discard an accepted Scene");auto after=DocumentTransactionTestAccess::tokens(*d);
   require(std::get<0>(after)==std::get<1>(tokens)&&std::get<1>(after)==std::get<1>(tokens)+1&&std::get<3>(after)==std::get<3>(tokens)+1&&std::get<4>(after)==0,"Post-commit persistence failure retains exactly one successful history/content transition");
   require(d->undo()&&d->copy_scene().encode()==original,"Accepted edit remains exactly undoable after recovery allocation failure");continue;
  }
  require(d->copy_scene().encode()==original,"Failed endpoint preserves exact Scene, original source maps and painting");
  require(DocumentTransactionTestAccess::tokens(*d)==tokens,"Failed endpoint preserves history/redo/content/nextContent/revision/generation/recovery identities");
  require(DocumentTransactionTestAccess::source(*d)==source&&d->frame().surface==mesh,"Failed endpoint never exposes candidate source cache or mesh");
  if(mode!="asset"&&mode!="import"&&mode!="procedural"&&mode!="uv"&&mode!="save"&&mode!="open"&&mode!="export"&&mode!="restore"&&!mode.ends_with("-queue"))require(d->frame().state==publication,"Failed synchronous endpoint retains exact published editor state");
  if(mode=="prepare-queue"){
   require(!d->frame().state->asset_busy,"Rejected prepare retains no busy lease");d->prepare_mesh(64);require(d->wait_asset()&&d->frame().state->error.empty(),"Rejected prepare can retry");
   require(d->copy_scene().encode()["objects"][0]==original["objects"][0],"Preparation retry preserves the independent original source and painting");
  }else if(mode=="refresh-queue"){
   require(!d->frame().state->recovery_busy,"Rejected recovery refresh retains no busy lease");d->refresh_recovery();require(DocumentTransactionTestAccess::wait_recovery(*d),"Rejected refresh can retry");
  }else if(mode=="restore"||mode=="restore-queue"){
   require(!d->frame().state->asset_busy&&!d->frame().state->recovery_busy,"Failed restore releases busy ownership");
   d->restore_recovery(DocumentTransactionTestAccess::recovery_candidate(*d,restore_id));require(d->wait_asset()&&d->frame().state->error.empty()&&d->copy_scene().active().name=="External recovered source","Actual failed restore retries the exact independent durable source");
  }else if(mode=="native-begin-queue"){
   require(!d->frame().state->native_pending&&d->wait_native_stroke(),"Rejected native Begin retains no gesture lease or queued job");
   require(d->begin_native_stroke(),"Rejected native Begin can retry");d->end_native_stroke(false);require(d->wait_native_stroke(),"Native Begin retry cancels exactly");
  }else if(mode=="native-save-queue"){
   require(d->frame().state->native_pending&&d->frame().state->native_accepting,"Rejected native Save keeps the held gesture unaccepted");
   d->end_native_stroke(false);require(d->wait_native_stroke(),"Rejected Save retains cancellable native gesture");d->save(fixture_path());
  }else if(mode=="export-queue"){auto path=fixture_path();path.replace_extension(".glb");d->export_asset(path,AssetExportFormat::Glb).get();}
  else if(mode=="recovery"){
   require(DocumentTransactionTestAccess::flush_recovery(*d),"Failed endpoint recovery drain");auto after=d->recovery_status();
   require(after.accepted==recovery.accepted&&after.coalesced==recovery.coalesced&&after.committed==recovery.committed,"Rejected candidates never enter the durable recovery queue");
   auto o=d->copy_scene().active();d->object_properties(o.id,"Accepted recovered endpoint",o.visible,o.locked,o.transform,o.material);
   require(DocumentTransactionTestAccess::flush_recovery(*d)&&d->recovery_status().latest,"Successful retry reaches durable recovery");
   auto restored=RecoveryService::load(*d->recovery_status().latest);require(restored.encode()==d->copy_scene().encode(),"Recovery stores the exact successful endpoint");
  }else if(mode=="save"||mode=="open"||mode=="save-queue"||mode=="open-queue"){
   require(!d->frame().state->asset_busy,"Failed file publication releases busy lease");
   if(mode=="save"||mode=="save-queue"){require(Scene::load(fixture_path()).encode()==original,"Failed Save status never writes an uncommitted scene");d->save(fixture_path());}
   else d->open(fixture_path());
  }else if(mode=="uv"||mode=="uv-queue"){
   require(d->frame().state->uv&&d->frame().state->uv->mesh==draft&&DocumentTransactionTestAccess::control(*d),"Failed UV publication retains the exact draft and source lease");
   d->end_uv(true);require(d->wait_asset()&&d->frame().state->error.empty()&&!d->frame().state->uv,"Actual failed UV Apply can retry without reopening or losing the draft");
  }else if(mode=="control"){
   require(DocumentTransactionTestAccess::control(*d),"Failed scalar acceptance retains its exact active control");DocumentTransactionTestAccess::finish_control(*d);
  }else if(mode=="import"||mode=="procedural"){
   require(!d->frame().state->asset_busy&&!d->asset_status().cancellable,"Failed actual workflow publication releases its busy source lease");
   execute(*d,mode);require(d->frame().state->error.empty()&&d->copy_scene().encode()==expected,"Actual failed workflow retries to its exact prepared source and paint state");
   auto retained=d->copy_scene().encode()["objects"][0],prior=original["objects"][0];
   // Scene9 adds absent-equivalent optional null fields to every object when
   // the imported source first introduces retained material bindings. Compare
   // the entire payload, normalizing only these two codec-prescribed nulls.
   for(auto *value:{&retained,&prior})for(const char *key:{"material_bindings","retopology"})if(value->contains(key)&&(*value)[key].is_null())value->erase(key);
   if(retained!=prior)std::cerr<<nlohmann::json::diff(prior,retained).dump(2)<<'\n';
   require(retained==prior,"Workflow retry retains the independent original source maps and painting");
   require(d->undo()&&d->copy_scene().encode()==original,"Retried workflow is exactly one undoable transition");
   require(d->redo()&&d->copy_scene().encode()==expected,"Retried workflow redo restores its exact source/layer output");
  }else if(mode=="asset"||mode=="asset-queue"){
   require(!d->frame().state->asset_busy&&!d->asset_status().cancellable,"Failed asset releases busy lease and remains retryable");
   d->edit_scene("Retry asset",[](Scene &s,const auto &){s.active().name="Accepted asset";});require(d->wait_asset()&&d->frame().state->error.empty(),"Actual failed asset can retry");
  }else require(d->redo(),"Earlier redo remains available after allocation failure");
 }
 std::filesystem::remove(fixture_path());std::filesystem::remove(import_path());std::filesystem::remove_all(recovery_path());auto exported=fixture_path();exported.replace_extension(".glb");std::filesystem::remove(exported);
 std::cout<<"PASS "<<mode<<": every one of "<<count<<" real endpoint allocation boundaries; "<<(mode=="postcommit"?"accepted Scene/history retained through persistence failure":"exact scene/history/cache/lease rollback and retry")<<"\n";return 0;
}catch(const std::exception &e){enabled=false;whole=permitted=inside=false;std::cerr<<e.what()<<'\n';return 1;}
