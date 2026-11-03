// SPDX-License-Identifier: GPL-3.0-or-later
// Diagnostic-only replay of the real broad workflow endpoint that exceeded
// its original 120-second observation. No production or broad-test limit edits.
#define DOME_UNION_WORKFLOW_NO_MAIN
#define __wrap__ZN4dome15author_shapekeyERNS_5SceneEmRKNS_12ShapeKeyEditERKSt8functionIFbvEE original_workflow_key_wrapper
#include "source-union-workflow.cpp"
#undef __wrap__ZN4dome15author_shapekeyERNS_5SceneEmRKNS_12ShapeKeyEditERKSt8functionIFbvEE
#undef DOME_UNION_WORKFLOW_NO_MAIN
#include <ctime>
namespace observed {
using namespace dome;
const auto start=std::chrono::steady_clock::now();
std::mutex log_mutex;
std::atomic_bool active=false;
thread_local unsigned key_depth=0,validate_depth=0,snapshot_depth=0;
double wall(){return std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();}
double cpu(){timespec t{};clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t);return double(t.tv_sec)+double(t.tv_nsec)*1e-9;}
void event(std::string_view name,std::string_view detail={}){
 std::lock_guard lock(log_mutex);std::cerr<<"OBS "<<wall()<<" cpu="<<cpu()<<" thread="<<std::this_thread::get_id()<<" "<<name<<" "<<detail<<std::endl;
}
struct Phase {std::string name;double w=wall(),c=cpu();explicit Phase(std::string n):name(std::move(n)){event(name+".begin");}~Phase(){event(name+".end","wall="+std::to_string(wall()-w)+" cpu="+std::to_string(cpu()-c));}};
struct Depth {unsigned &value;explicit Depth(unsigned &v):value(v){++value;}~Depth(){--value;}};
}
extern "C" ShapeKeyResult __wrap__ZN4dome15author_shapekeyERNS_5SceneEmRKNS_12ShapeKeyEditERKSt8functionIFbvEE(Scene &scene,uint64_t object,const ShapeKeyEdit &edit,const std::function<bool()> &cancelled){
 observed::Depth depth(observed::key_depth);std::optional<observed::Phase> phase;if(observed::active&&observed::key_depth==1)phase.emplace("actual.key.factory");
 return original_workflow_key_wrapper(scene,object,edit,cancelled);
}
extern "C" void __real__ZNK4dome5Scene4saveERKNSt10filesystem7__cxx114pathE(const Scene*,const std::filesystem::path&);
extern "C" void __wrap__ZNK4dome5Scene4saveERKNSt10filesystem7__cxx114pathE(const Scene *scene,const std::filesystem::path &path){
 observed::Phase phase("actual.scene.save");__real__ZNK4dome5Scene4saveERKNSt10filesystem7__cxx114pathE(scene,path);
}
extern "C" void __real__ZNK4dome5Scene8validateEb(const Scene*,bool);
extern "C" void __wrap__ZNK4dome5Scene8validateEb(const Scene *scene,bool deep){
 observed::Depth depth(observed::validate_depth);std::optional<observed::Phase> phase;if(observed::active&&observed::validate_depth==1)phase.emplace(deep?"actual.scene.validate.deep":"actual.scene.validate.shallow");__real__ZNK4dome5Scene8validateEb(scene,deep);
}
extern "C" Scene __real__ZNK4dome5Scene8snapshotEv(const Scene*);
extern "C" Scene __wrap__ZNK4dome5Scene8snapshotEv(const Scene *scene){
 observed::Depth depth(observed::snapshot_depth);std::optional<observed::Phase> phase;if(observed::active&&observed::snapshot_depth==1)phase.emplace("actual.scene.snapshot");return __real__ZNK4dome5Scene8snapshotEv(scene);
}
int main(int argc,char **argv)try{
 using namespace dome;using namespace union_workflow;if(argc!=4)return 2;directory=argv[3];std::filesystem::create_directories(directory);const auto home=directory/"home";std::filesystem::create_directories(home);setenv("HOME",home.c_str(),1);setenv("XDG_CONFIG_HOME",home.c_str(),1);setenv("XDG_STATE_HOME",home.c_str(),1);
 Scene source,reference;{observed::Phase phase("input.rig25.load");source=Scene::load(argv[1]);}{observed::Phase phase("input.already.completed.reference.load");reference=Scene::load(argv[2]);}
 const auto target=source.active().id;const auto &object=source.active();check(object.painted&&object.painted->paint_sampling_domain()&&object.material_bindings&&object.material_bindings->normal_preparation&&object.rig_source,"Focused input is the actual retained-paint/normal/key/native-REST broad workflow endpoint");
 const auto &keys=object.deformation->morphs->targets;auto found=std::find_if(keys.begin(),keys.end(),[](const auto &t){return t.name=="Union raised surface";});check(found!=keys.end(),"Exact original broad target identity exists");ShapeKeyEdit edit;edit.action=ShapeKeyAction::Replace;edit.target=size_t(found-keys.begin());edit.positions=object.painted->mesh().positions;
 for(size_t v=0;v<edit.positions.size();++v){edit.positions[v]+=found->delta[v];if(edit.positions[v].z>0)edit.positions[v].z+=.002f;}
 const auto &expected=reference.object(target).deformation->morphs->targets.at(edit.target);for(size_t v=0;v<edit.positions.size();++v)if(const auto actual=edit.positions[v]-object.painted->mesh().positions[v];actual.x!=expected.delta[v].x||actual.y!=expected.delta[v].y||actual.z!=expected.delta[v].z)throw std::runtime_error("Focused replay differs from exact already-completed original post-REST requested target");check(true,"Requested Replace reproduces every exact original completed target delta");
 Document doc;{observed::Phase phase("native.replace.input");doc.replace(source);} {observed::Phase phase("native.initial.mesh.endpoint");endpoint(doc);}
 auto before=doc.copy_scene();nlohmann::json encoded;{observed::Phase phase("before.scene.encode");encoded=before.encode();}
 observed::active=true;const auto accepted_at=observed::wall();{observed::Phase phase("native.author.admission");doc.author_shape(edit);}std::future<void> future;const auto output=directory/"focused-key-after-rest.dsculpt";double saved_at=0;{observed::Phase phase("native.save.admission");future=doc.save_async(output);saved_at=observed::wall();}
 bool crossed=false;for(unsigned second=0;;++second){
  if(future.wait_for(std::chrono::seconds(1))==std::future_status::ready)break;
  const auto elapsed=observed::wall()-saved_at;
  if(elapsed>=120&&!crossed){crossed=true;observed::event("ORIGINAL_120_SECOND_OBSERVER_EXCEEDED","diagnostic continues; original failure retained");}
  if(second%5==0){const auto state=doc.frame().state;observed::event("native.heartbeat","save_wait="+std::to_string(elapsed)+" asset_busy="+std::to_string(state->asset_busy)+" revision="+std::to_string(state->revision)+" message="+state->message+" error="+state->error);}
  if(elapsed>600)throw std::runtime_error("Focused diagnostic 600-second observer elapsed; original120 gate remains failed");
 }
 future.get();observed::event("native.save.future.ready","since_author_admission="+std::to_string(observed::wall()-accepted_at)+" save_wait="+std::to_string(observed::wall()-saved_at)+" crossed120="+std::to_string(crossed));observed::active=false;
 {observed::Phase phase("native.final.mesh.endpoint");endpoint(doc);}const auto after=doc.copy_scene();{observed::Phase phase("original.key_preserved.oracle");key_preserved(before,after,target,true);} {observed::Phase phase("actual.saved.restart.oracle");check(Scene::load(output).encode()==after.encode(),"Actual focused ordered Save/restart retains the complete published Scene");}
 {observed::Phase phase("original.history.oracle");history(doc,encoded);} {observed::Phase phase("completed.original.reference.oracle");check(after.encode()==reference.encode(),"Focused native worker reproduces the entire exact original completed Scene, not only a changed target");}
 std::cout<<"PASS "<<checks<<" unchanged owner/history/reference checks; original120_crossed="<<crossed<<std::endl;return 0;
}catch(const std::exception &e){std::cerr<<"FOCUSED_FAILURE "<<e.what()<<std::endl;return 1;}
