// SPDX-License-Identifier: GPL-3.0-or-later
// Reuse the qualified genuine painted/posed affine fixture and atlas observer.
// Its separate entry remains callable by the small composed native proof.
#define main affine_native_main
#include "retopo-affine.cpp"
#undef main
#include <fstream>
#include <cstdlib>
#include <new>
namespace handoff_fault {
thread_local bool armed=false,hit=false;
thread_local size_t calls=0,fail=0;
void allocation(){if(armed&&++calls>=fail&&fail){hit=true;throw std::bad_alloc();}}
}
void *operator new(size_t n){handoff_fault::allocation();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void *p)noexcept{std::free(p);}
void operator delete[](void *p)noexcept{std::free(p);}
void operator delete(void *p,size_t)noexcept{std::free(p);}
void operator delete[](void *p,size_t)noexcept{std::free(p);}
void *operator new(size_t n,std::align_val_t a){handoff_fault::allocation();void *p=nullptr;if(posix_memalign(&p,size_t(a),n?n:1))throw std::bad_alloc();return p;}
void *operator new[](size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void *p,std::align_val_t)noexcept{std::free(p);}
void operator delete[](void *p,std::align_val_t)noexcept{std::free(p);}
void operator delete(void *p,size_t,std::align_val_t)noexcept{std::free(p);}
void operator delete[](void *p,size_t,std::align_val_t)noexcept{std::free(p);}
namespace input_gate {
std::mutex mutex;std::condition_variable changed;
bool armed=false,entered=false,released=false,throw_after=false;
void arm(bool error=false){std::lock_guard lock(mutex);armed=true;entered=released=false;throw_after=error;}
void release(){std::lock_guard lock(mutex);released=true;armed=false;changed.notify_all();}
bool wait(){std::unique_lock lock(mutex);return changed.wait_for(lock,std::chrono::seconds(120),[]{return entered;});}
struct Release{~Release(){release();}};
}
extern "C" void real_edit(RetopoWorkspace &,RetopoAction,Vec3,float,const std::function<bool()> &)
  asm("__real__ZN4dome15RetopoWorkspace4editENS_12RetopoActionENS_4Vec3EfRKSt8functionIFbvEE");
extern "C" void wrapped_edit(RetopoWorkspace &,RetopoAction,Vec3,float,const std::function<bool()> &)
  asm("__wrap__ZN4dome15RetopoWorkspace4editENS_12RetopoActionENS_4Vec3EfRKSt8functionIFbvEE");
extern "C" void wrapped_edit(RetopoWorkspace &workspace,RetopoAction action,Vec3 p,float distance,const std::function<bool()> &cancelled){
  bool error=false;
  if(action==RetopoAction::EndGesture){
    std::unique_lock lock(input_gate::mutex);
    if(input_gate::armed){input_gate::entered=true;input_gate::changed.notify_all();
      if(!input_gate::changed.wait_for(lock,std::chrono::seconds(120),[]{return input_gate::released;}))throw std::runtime_error("Retopo independent-input test gate timeout");
      error=input_gate::throw_after;
    }
  }
  real_edit(workspace,action,p,distance,cancelled);
  if(error)throw std::runtime_error("Test-injected independent retopology finalization failure");
}
namespace {
constexpr Matrix handoff_matrix{-1.3,.1,0,0,.35,.8,.05,0,.1,.15,1.2,0,.15,-.1,.2,1};
std::string bytes(const std::filesystem::path &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void prior(const std::filesystem::path &p){std::ofstream(p,std::ios::binary)<<"previous exact native file";}
std::string failure(std::future<void> &f){try{f.get();return {};}catch(const std::exception &e){return e.what();}}
void draft(Document &d,const Scene &s,bool face=true){
  d.replace(s.snapshot());check(d.wait_mesh(),"Current native source is ready before retopology");d.begin_retopology();ready(d);
  if(!face)return;
  for(auto p:std::array<Vec3,3>{{{-.2f,-.15f,-.055f},{.2f,-.15f,.025f},{0,.15f,.015f}}})place(d,point(handoff_matrix,evaluated(p,true)));
  d.edit_retopology(RetopoAction::Face);ready(d);
}
void close_cancelled(Document &d){input_gate::release();check(d.wait_asset(),"Cancelled accepted work drains");if(d.frame().state->retopology)d.end_retopology(false);}
void runtime_cases(const std::filesystem::path &out,const Scene &s){
 const auto exact=s.encode();
 {
  Document d;draft(d,s);input_gate::Release release;input_gate::arm();const auto path=out/"immediate.dsculpt";prior(path);
  d.end_retopology(true);auto saved=d.save_async(path); // No input/atlas wait here.
  check(input_gate::wait(),"Independent input worker is held BEFORE real finalization/unwrap");
  check(saved.wait_for(std::chrono::milliseconds(0))==std::future_status::timeout&&bytes(path)=="previous exact native file","Immediate Save waits for its accepted operation without touching prior file");
  auto model=std::async(std::launch::async,[&]{return d.copy_scene().encode();});
  const auto status=model.wait_for(std::chrono::seconds(5));if(status!=std::future_status::ready)input_gate::release();
  check(status==std::future_status::ready&&model.get()==exact,"Accepted file worker waits outside model lock and original authority stays exact");
  input_gate::release();saved.get();check(d.wait_asset()&&Scene::load(path).encode()==d.copy_scene().encode(),"Immediate Apply-to-Save captures the exact newly published native result");
  const auto accepted=d.copy_scene().encode();check(d.undo()&&d.copy_scene().encode()==exact&&d.redo()&&d.copy_scene().encode()==accepted,"Early accepted Save preserves one global Undo/Redo");
  d.begin_retopology();ready(d);d.end_retopology(true);auto unchanged=d.save_async(out/"noop.dsculpt");unchanged.get();check(Scene::load(out/"noop.dsculpt").encode()==accepted&&d.copy_scene().encode()==accepted,"Unchanged reopened acceptance saves exact Scene without a duplicate patch");
  check(d.undo()&&d.copy_scene().encode()==exact,"Unchanged acceptance creates no extra Undo entry");
 }
 {
  Document d;draft(d,s);input_gate::Release release;input_gate::arm();const auto path=out/"cancel.dsculpt";prior(path);
  d.end_retopology(true);auto saved=d.save_async(path);check(input_gate::wait(),"Cancel case reaches real independent-input gate");
  d.cancel_background_work();check(saved.wait_for(std::chrono::seconds(5))==std::future_status::ready,"Cancel wakes the file CV while independent input is still held");
  check(failure(saved).find("cancel")!=std::string::npos&&bytes(path)=="previous exact native file"&&d.copy_scene().encode()==exact,"Early Cancel retires dependent Save and preserves source/prior bytes");close_cancelled(d);
 }
 for(bool injected:{false,true}){
  Document d;draft(d,s,injected);input_gate::Release release;input_gate::arm(injected);const auto path=out/(injected?"injected-error.dsculpt":"invalid-face.dsculpt");prior(path);
  d.end_retopology(true);auto saved=d.save_async(path);check(input_gate::wait(),"Failure case owns accepted Save before input finalization");input_gate::release();
  const auto error=failure(saved);check(error.find(injected?"Test-injected independent":"Make at least one face")!=std::string::npos,"Dependent Save retains the actual independent-worker exception");
  check(d.wait_asset()&&d.copy_scene().encode()==exact&&bytes(path)=="previous exact native file","Worker failure keeps exact source and prior file");
  if(d.frame().state->retopology)d.end_retopology(false);
 }
 {
  Document d;draft(d,s);input_gate::Release release;input_gate::arm();const auto path=out/"stale.dsculpt";prior(path);d.end_retopology(true);auto saved=d.save_async(path);check(input_gate::wait(),"Stale case holds independent input");
  Scene replacement;replacement.active().name="New authority";const auto newer=replacement.encode();d.replace(replacement.snapshot());
  check(saved.wait_for(std::chrono::seconds(5))==std::future_status::ready&&!failure(saved).empty(),"Document replacement wakes and refuses stale accepted Save");input_gate::release();
  check(d.copy_scene().encode()==newer&&bytes(path)=="previous exact native file","Stale input/file workers cannot publish into replacement generation");
 }
 {
  auto d=std::make_unique<Document>();draft(*d,s);input_gate::Release release;input_gate::arm();const auto path=out/"shutdown.dsculpt";
  d->end_retopology(true);auto saved=d->save_async(path);check(input_gate::wait(),"Shutdown case holds accepted input with ordered Save");
  auto closed=std::async(std::launch::async,[doc=std::move(d)]()mutable{doc.reset();});
  check(closed.wait_for(std::chrono::milliseconds(20))==std::future_status::timeout,"Shutdown drains accepted Save instead of dropping it");input_gate::release();
  check(closed.wait_for(std::chrono::seconds(120))==std::future_status::ready,"Shutdown releases every independent worker after acceptance");closed.get();saved.get();
  const auto loaded=Scene::load(path);check(loaded.objects().size()==2&&loaded.active().retopology&&!loaded.object(s.active().id).visible,"Shutdown Save retains complete actual source and authored patch");
 }
 {
  Document d;draft(d,s);input_gate::Release release;input_gate::arm();d.end_retopology(true);check(input_gate::wait(),"File-capacity case holds accepted independent input");
  std::vector<std::future<void>> saves;for(size_t i=0;i<8;++i)saves.push_back(d.save_async(out/("queued-"+std::to_string(i)+".dsculpt")));
  const auto overflow=out/"overflow.dsculpt";prior(overflow);check(refuses([&]{(void)d.save_async(overflow);})&&bytes(overflow)=="previous exact native file","Real eight-entry file queue refuses overflow before any output mutation");
  input_gate::release();for(auto &f:saves)f.get();const auto accepted=d.copy_scene().encode();for(size_t i=0;i<8;++i)check(Scene::load(out/("queued-"+std::to_string(i)+".dsculpt")).encode()==accepted,"Every admitted queued Save uses the same exact accepted endpoint");
 }
}
void admission_faults(const std::filesystem::path &out,const Scene &s){
 const auto exact=s.encode();size_t count=0;
 {
  Document d;draft(d,s);input_gate::Release release;input_gate::arm();handoff_fault::calls=handoff_fault::fail=0;handoff_fault::armed=true;
  try{d.end_retopology(true);}catch(...){handoff_fault::armed=false;throw;}handoff_fault::armed=false;count=handoff_fault::calls;
  check(count>0&&count<=4096,"Real caller-side input/file admission allocation boundary measured");auto saved=d.save_async(out/"calibration.dsculpt");input_gate::release();saved.get();
 }
 for(size_t at=1;at<=count;++at){
  Document d;draft(d,s);const auto view=d.frame().state->retopology;const auto revision=d.frame().state->revision;
  input_gate::Release release;input_gate::arm();handoff_fault::calls=0;handoff_fault::fail=at;handoff_fault::hit=false;bool failed=false;handoff_fault::armed=true;
  try{d.end_retopology(true);}catch(const std::bad_alloc&){failed=true;}catch(...){handoff_fault::armed=false;throw;}handoff_fault::armed=false;input_gate::release();
  check(handoff_fault::hit&&failed,"Every measured persistent input/file admission allocation denial propagates");
  check(d.copy_scene().encode()==exact&&d.frame().state->retopology==view&&d.frame().state->revision==revision&&!d.frame().state->asset_busy&&!view->committing,"Rejected admission preserves exact source/draft/queue/publication authority");
  d.end_retopology(true);auto saved=d.save_async(out/"retry.dsculpt");saved.get();check(d.wait_asset()&&Scene::load(out/"retry.dsculpt").encode()==d.copy_scene().encode(),"Every persistent-denial caller retries through actual immediate accepted Save");
 }
 std::cout<<"PERSISTENT_ADMISSION_PREFIXES "<<count<<std::endl;
}
}
int retopo_save_handoff_checks(int argc,char **argv)try{
 if(argc!=2)return 2;checks=0;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);auto s=source(handoff_matrix,true);
 runtime_cases(out,s);admission_faults(out,s);std::cout<<"PASS "<<checks<<" immediate retopo handoff checks\n";return 0;
}catch(const std::exception &e){handoff_fault::armed=false;input_gate::release();atlas_gate::release();std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
#ifndef DOME_RETOPO_HANDOFF_EMBEDDED
int main(int argc,char **argv){return retopo_save_handoff_checks(argc,argv);}
#endif
