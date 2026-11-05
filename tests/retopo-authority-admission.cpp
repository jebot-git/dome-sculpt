// SPDX-License-Identifier: GPL-3.0-or-later
#define main prior_affine_main
#include "retopo-affine.cpp"
#undef main
#include <atomic>
#include <cstdlib>
namespace failure {
thread_local bool input_thread=false;
std::atomic<bool> enabled{false},hit{false},arm_after_publish{false};
std::atomic<size_t> calls{0},boundary{0},authority_publications{0};
void *allocate(size_t n,size_t alignment=0) {
  if(input_thread&&enabled) {
    const auto at=calls.fetch_add(1)+1;
    if(boundary&&at>=boundary){hit=true;throw std::bad_alloc();}
  }
  void *p=nullptr;
  if(alignment){if(posix_memalign(&p,alignment,n?n:1))p=nullptr;}else p=std::malloc(n?n:1);
  if(!p)throw std::bad_alloc();return p;
}
}
void *operator new(size_t n){return failure::allocate(n);}
void *operator new[](size_t n){return failure::allocate(n);}
void *operator new(size_t n,std::align_val_t a){return failure::allocate(n,size_t(a));}
void *operator new[](size_t n,std::align_val_t a){return failure::allocate(n,size_t(a));}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
void operator delete(void*p,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,std::align_val_t)noexcept{std::free(p);}
void operator delete(void*p,size_t,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,size_t,std::align_val_t)noexcept{std::free(p);}
extern "C" void real_update(SpatialInput&,const SpatialSample&)asm("__real__ZN4dome12SpatialInput6updateERKNS_13SpatialSampleE");
extern "C" void wrapped_update(SpatialInput&,const SpatialSample&)asm("__wrap__ZN4dome12SpatialInput6updateERKNS_13SpatialSampleE");
extern "C" void wrapped_update(SpatialInput&i,const SpatialSample&s){failure::input_thread=true;real_update(i,s);}
extern "C" void real_publish(Document*,bool,bool)asm("__real__ZN4dome8Document14publish_lockedEbb");
extern "C" void wrapped_publish(Document*,bool,bool)asm("__wrap__ZN4dome8Document14publish_lockedEbb");
extern "C" void wrapped_publish(Document*d,bool force,bool checkpoint){
  real_publish(d,force,checkpoint);
  if(failure::input_thread&&failure::arm_after_publish.exchange(false)) {
    ++failure::authority_publications;failure::calls=0;failure::boundary=1;failure::enabled=true;
  }
}
namespace {
constexpr Matrix matrix{-1.3,.1,0,0,.35,.8,.05,0,.1,.15,1.2,0,.15,-.1,.2,1};
struct Hand {
  Document &d;SpatialFrame f;
  explicit Hand(Document&x):d(x){check(d.wait_mesh(),"Actual current source display mesh ready");f.focused=f.head_valid=true;f.head.position={0,1.6f,0};f.hands[1].tracked.valid=true;send();}
  void send(){++f.sequence;f.time+=.02;d.submit(f);check(d.wait_input(),"Actual tracked input worker drains");}
};
void prepare(Document&d,const Scene&s,bool grab){
  d.replace(s.snapshot());if(grab)d.configure_object_grab(true,false);else d.hold_object(s.active().id);
  d.begin_retopology();ready(d);
  for(auto p:std::array<Vec3,3>{{{-.2f,-.15f,-.055f},{.2f,-.15f,.025f},{0,.15f,.015f}}})place(d,point(matrix,evaluated(p,true)));
}
size_t focus_count(const Scene&s){
  Document d;prepare(d,s,false);Hand h(d);h.f.focused=false;
  failure::calls=0;failure::boundary=0;failure::enabled=true;h.send();failure::enabled=false;
  const auto count=failure::calls.load();check(!d.frame().state->retopology,"Healthy authority loss retires actual workspace");return count;
}
}
int main(int argc,char**argv)try{
  if(argc!=4)return 2;const bool fixed=std::string_view(argv[2])=="fixed";check(fixed,"Fixed authority proof mode");const std::string mode=argv[1];const bool grab=mode=="grab";check(grab||mode=="focus","Known authority counter mode");
  const auto s=source(matrix,true);size_t boundary=0;
  if(!grab){boundary=focus_count(s);check(boundary==focus_count(s),"Actual unfocused frame allocation count stable");std::cout<<"ACTUAL focus last boundary="<<boundary<<std::endl;}
  Document d;prepare(d,s,grab);Hand h(d);const auto original=d.copy_scene().encode();
  failure::hit=false;failure::authority_publications=0;failure::calls=0;
  if(grab){h.f.hands[1].tracked.squeeze=.8f;failure::arm_after_publish=true;}
  else{h.f.focused=false;failure::boundary=boundary;failure::enabled=true;}
  h.send();failure::enabled=failure::arm_after_publish=false;
  const auto failed=failure::hit.load();const auto publications=failure::authority_publications.load();
  if(grab)check(!failed&&publications==1,"Accepted authority frame has no later fallible allocation/publication");
  else check(failed,"Real unfocused publication allocation still refuses at actual measured boundary");
  h.f.focused=true;h.f.hands[1].tracked.squeeze=0;h.f.hands[1].tracked.timeMs=777777;
  const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);size_t neutral_frames=0;
  do {h.send();++neutral_frames;}
  while(d.frame().state->hands[1].tracked.timeMs!=777777&&std::chrono::steady_clock::now()<until);
  check(d.frame().state->hands[1].tracked.timeMs==777777,"A real neutral release frame executes after diagnostic retirement");
  std::cout<<"ACTUAL processed release marker="<<d.frame().state->hands[1].tracked.timeMs<<" neutralFrames="<<neutral_frames<<std::endl;
  const auto view=d.frame().state;
  std::cout<<"OBSERVED authorityPub="<<publications<<" denied="<<failed<<" retiredViewRevived="<<bool(view->retopology)<<" grabbing="<<view->object_grabbing<<" held="<<view->held_object<<std::endl;
  check(d.copy_scene().encode()==original,"Authority refusal retains complete original affine source/paint/pose/meta");
  check(!view->retopology,"An irreversibly retired workspace cannot be revived by input-scope rollback");
  d.configure_object_grab(false,false);d.release_held_object(false);
  d.begin_retopology();ready(d);check(d.frame().state->retopology->topology->vertices().empty(),"Fresh Begin builds a genuine new workspace after authority retirement");
  for(auto q:std::array<Vec3,3>{{{-.2f,-.15f,-.055f},{.2f,-.15f,.025f},{0,.15f,.015f}}})place(d,point(matrix,evaluated(q,true)));
  d.edit_retopology(RetopoAction::Face);ready(d);d.end_retopology(true);auto saved=d.save_async(argv[3]);saved.get();
  check(d.wait_asset()&&!d.frame().state->retopology&&d.frame().state->error.empty(),"Real Place/Face/Apply plus immediate Save retry completes");
  const auto accepted=d.copy_scene().encode();check(Scene::load(argv[3]).encode()==accepted,"Authority retry reopens the exact full source and patch");
  check(d.undo()&&d.copy_scene().encode()==original,"One Undo of retry restores every original source owner");check(d.redo()&&d.copy_scene().encode()==accepted,"Redo restores exact authority recovery result");
  std::cout<<"PASS "<<checks<<" actual authority retirement/retry checks\n";return 0;
}catch(const std::exception&e){failure::enabled=failure::arm_after_publish=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
