// SPDX-License-Identifier: GPL-3.0-or-later
#define main prior_affine_main
#include "retopo-affine.cpp"
#undef main
#include <atomic>
#include <cstdlib>
#include <new>
namespace caller_fault {
thread_local bool enabled = false, hit = false;
thread_local size_t calls = 0, boundary = 0;
thread_local bool deny_publication = false, worker_publication = false, input_thread = false;
std::atomic<bool> input_enabled{false}, input_hit{false};
std::atomic<size_t> input_calls{0}, input_boundary{0};
std::atomic<bool> deny_after_face{false}, worker_hit{false};
std::atomic<int> deny_after_action{-1}, gate_action{-1};
std::atomic<bool> observe_actions{false};
std::atomic<size_t> observed_ends{0}, observed_undos{0};
std::mutex gate_mutex;std::condition_variable gate_changed;
bool gate_entered=false,gate_released=false;
void release_gate(){std::lock_guard lock(gate_mutex);gate_released=true;gate_changed.notify_all();}
struct GateRelease {~GateRelease(){release_gate();}};

void *allocate(size_t n, size_t alignment = 0) {
  if (enabled && ++calls >= boundary && boundary) { hit = true; throw std::bad_alloc(); }
  if (input_thread && input_enabled.load()) {
    const auto n = input_calls.fetch_add(1) + 1;
    if (input_boundary.load() && n >= input_boundary.load()) {input_hit = true;throw std::bad_alloc();}
  }
  void *p = nullptr;
  if (alignment) { if (posix_memalign(&p, alignment, n ? n : 1)) p = nullptr; }
  else p = std::malloc(n ? n : 1);
  if (!p) throw std::bad_alloc(); return p;
}
}
void *operator new(size_t n) { return caller_fault::allocate(n); }
void *operator new[](size_t n) { return caller_fault::allocate(n); }
void *operator new(size_t n, std::align_val_t a) { return caller_fault::allocate(n, size_t(a)); }
void *operator new[](size_t n, std::align_val_t a) { return caller_fault::allocate(n, size_t(a)); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
void operator delete[](void *p, size_t) noexcept { std::free(p); }
void operator delete(void *p, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void *p, std::align_val_t) noexcept { std::free(p); }
void operator delete(void *p, size_t, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void *p, size_t, std::align_val_t) noexcept { std::free(p); }
extern "C" void real_publish(Document *, bool, bool) asm("__real__ZN4dome8Document14publish_lockedEbb");
extern "C" void wrapped_publish(Document *, bool, bool) asm("__wrap__ZN4dome8Document14publish_lockedEbb");
extern "C" void wrapped_publish(Document *d, bool force, bool checkpoint) {
  if (caller_fault::deny_publication) { caller_fault::deny_publication = false; throw std::bad_alloc(); }
  if (caller_fault::worker_publication) {
    caller_fault::worker_publication = false; caller_fault::worker_hit = true; throw std::bad_alloc();
  }
  real_publish(d, force, checkpoint);
}
extern "C" void real_edit(RetopoWorkspace &, RetopoAction, Vec3, float, const std::function<bool()> &)
  asm("__real__ZN4dome15RetopoWorkspace4editENS_12RetopoActionENS_4Vec3EfRKSt8functionIFbvEE");
extern "C" void wrapped_edit(RetopoWorkspace &, RetopoAction, Vec3, float, const std::function<bool()> &)
  asm("__wrap__ZN4dome15RetopoWorkspace4editENS_12RetopoActionENS_4Vec3EfRKSt8functionIFbvEE");
extern "C" void wrapped_edit(RetopoWorkspace &w, RetopoAction a, Vec3 p, float mm, const std::function<bool()> &c) {
  if(caller_fault::gate_action.load()==int(a)&&caller_fault::gate_action.exchange(-1)==int(a)) {
    std::unique_lock lock(caller_fault::gate_mutex);caller_fault::gate_entered=true;caller_fault::gate_changed.notify_all();
    if(!caller_fault::gate_changed.wait_for(lock,std::chrono::seconds(120),[]{return caller_fault::gate_released;}))
      throw std::runtime_error("Actual retopo edit gate timed out");
  }
  real_edit(w,a,p,mm,c);
  if(caller_fault::observe_actions) {
    if(a==RetopoAction::EndGesture)++caller_fault::observed_ends;
    if(a==RetopoAction::Undo)++caller_fault::observed_undos;
  }
  if(caller_fault::deny_after_action.load()==int(a)&&caller_fault::deny_after_action.exchange(-1)==int(a))
    caller_fault::worker_publication=true;
  if (a == RetopoAction::Face && caller_fault::deny_after_face.exchange(false))
    caller_fault::worker_publication = true;
}
namespace {
constexpr Matrix matrix{-1.3,.1,0,0,.35,.8,.05,0,.1,.15,1.2,0,.15,-.1,.2,1};
void points(Document &d, const Scene &s) {
  d.replace(s.snapshot()); d.begin_retopology(); ready(d);
  for (auto p : std::array<Vec3,3>{{{-.2f,-.15f,-.055f},{.2f,-.15f,.025f},{0,.15f,.015f}}})
    place(d,point(matrix,evaluated(p,true)));
}
Brush held_brush() {
  Brush b;b.radius=.01f;b.from=b.to=point(matrix,evaluated({.22f,.17f,.061f},true));return b;
}
void hold(Document &d) {
  check(d.begin_desktop(),"Real physical input owns a held fourth point");
  d.stamp_desktop(held_brush());ready(d);
}
void barrier(Document &d) { d.edit_retopology(RetopoAction::PlaceMode);ready(d); }
void retry(Document &d, const Scene &scene, const std::filesystem::path &path) {
  const auto before=d.frame().state->retopology->topology->encode();
  check(d.frame().state->retopology->topology->vertices().size()==3,"Retry starts with exact three-vertex draft");
  d.edit_retopology(RetopoAction::Face);ready(d);
  const auto face=d.frame().state->retopology->topology->encode();
  check(d.frame().state->retopology->topology->faces().size()==1,"Healthy retry commits one real Face");
  d.edit_retopology(RetopoAction::Undo);ready(d);
  check(d.frame().state->retopology->topology->encode()==before,"One local Undo returns exact pre-Face history");
  d.edit_retopology(RetopoAction::Redo);ready(d);
  check(d.frame().state->retopology->topology->encode()==face,"Local Redo restores exact accepted Face");
  d.end_retopology(true);auto saved=d.save_async(path);saved.get();
  check(d.wait_asset()&&!d.frame().state->retopology&&d.frame().state->error.empty(),"Real Apply and immediate ordered Save finish");
  const auto accepted=d.copy_scene().encode();
  check(Scene::load(path).encode()==accepted,"Immediate Save/reopen preserves complete exact source and patch");
  check(d.undo()&&d.copy_scene().encode()==scene.encode(),"One global Undo preserves original affine paint/pose/meta Scene");
  check(d.redo()&&d.copy_scene().encode()==accepted,"Global Redo restores complete accepted Scene");
}
size_t calibration(const Scene &scene, bool held, RetopoAction action) {
  Document d;points(d,scene);if(held)hold(d);
  caller_fault::calls=0;caller_fault::boundary=0;caller_fault::enabled=true;
  d.edit_retopology(action);
  caller_fault::enabled=false;const auto count=caller_fault::calls;ready(d);return count;
}
void refusal(const Scene &scene, bool held, RetopoAction action, size_t boundary, bool publish,
             const std::filesystem::path &path) {
  Document d;points(d,scene);
  const auto base=d.frame().state->retopology->topology->encode();
  if(held)hold(d);
  const auto previous=d.frame().state;const auto original=scene.encode();
  caller_fault::calls=0;caller_fault::boundary=boundary;caller_fault::hit=false;
  caller_fault::deny_publication=publish;caller_fault::enabled=!publish;
  bool failed=false;
  try { d.edit_retopology(action); } catch(const std::bad_alloc &) { failed=true; }
  caller_fault::enabled=caller_fault::deny_publication=false;
  check(failed&&(publish||caller_fault::hit),"Actual persistent caller boundary refuses publication");
  check(d.frame().state==previous&&d.copy_scene().encode()==original,"Caller refusal retains exact immutable view and full source Scene");
  if(held) {
    // Prove physical ownership is retained, then explicitly cancel it. A healthy
    // FIFO barrier alone would itself finish/cancel the original held gesture.
    d.stamp_desktop(held_brush());ready(d);d.cancel_desktop();
  }
  barrier(d);
  check(d.frame().state->retopology->topology->encode()==base,"Accepted FIFO barrier proves no hidden command or held cancellation survives refusal");
  retry(d,scene,path);
}
void worker_batch(const Scene &scene,const std::filesystem::path &path) {
  Document d;points(d,scene);const auto base=d.frame().state->retopology->topology->encode();
  hold(d);const auto held=d.frame().state->retopology->topology->encode();
  caller_fault::worker_hit=false;caller_fault::deny_after_face=true;
  d.edit_retopology(RetopoAction::Face);
  check(d.wait_retopology(),"Atomic held Cancel+Face reaches real late worker publication");
  check(caller_fault::worker_hit&&!d.frame().state->error.empty(),"Real successful Face edit is refused by actual worker publication");
  check(d.frame().state->retopology->topology->encode()==held&&d.copy_scene().encode()==scene.encode(),"Late worker failure cannot commit hidden CancelGesture or partial Face");
  barrier(d); // Healthy PlaceMode finalizes the preserved held fourth point.
  check(d.frame().state->retopology->topology->encode()==held,"Healthy barrier retains held geometry after failed atomic batch");
  d.edit_retopology(RetopoAction::Undo);ready(d);
  check(d.frame().state->retopology->topology->encode()==base,"Undo proves preserved held history was finalized exactly once");
  d.edit_retopology(RetopoAction::Redo);ready(d);
  check(d.frame().state->retopology->topology->encode()==held,"Redo proves no hidden pre-publication cancellation changed history");
  d.edit_retopology(RetopoAction::Undo);ready(d);retry(d,scene,path);
}
}
#ifndef DOME_RETOPO_CALLER_ENTRY
#define DOME_RETOPO_CALLER_ENTRY main
#endif
int DOME_RETOPO_CALLER_ENTRY(int argc,char **argv)try {
  if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
  const auto scene=source(matrix,true);
  // All public discrete dispatch branches share this one transaction. Include
  // state-changing mode/selection/history actions, not only Face allocation.
  constexpr RetopoAction actions[]{RetopoAction::Face,RetopoAction::MoveMode,RetopoAction::Clear,RetopoAction::Undo};
  size_t denials=0;
  for(bool held:{false,true})for(auto action:actions) {
    size_t count=0;
    for(size_t repeat=0;repeat<3;++repeat) {
      const auto current=calibration(scene,held,action);
      if(repeat)check(current==count,"Actual caller allocation count is stable across fresh documents");
      count=current;
    }
    std::cout<<"CALLER held="<<held<<" action="<<int(action)<<" allocations="<<count<<std::endl;
    check(count>0,"Caller sweep covers real allocations");
    for(size_t at=1;at<=count;++at) {refusal(scene,held,action,at,false,out/"retry.dsculpt");++denials;}
    refusal(scene,held,action,0,true,out/"retry.dsculpt");
  }
  worker_batch(scene,out/"worker-retry.dsculpt");
  std::cout<<"PASS "<<checks<<" caller admission checks; "<<denials<<" persistent caller boundaries\n";return 0;
}catch(const std::exception &e){caller_fault::enabled=caller_fault::deny_publication=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
