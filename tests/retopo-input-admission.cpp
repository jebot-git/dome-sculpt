// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_RETOPO_CALLER_ENTRY prior_caller_main
#include "retopo-caller-admission.cpp"
// Observe the actual existing input worker via its real cross-TU update call.
// The thread tag persists so the next frame's admission allocations, before
// its first update, are included. No substitute input or publication is used.
extern "C" void real_update(SpatialInput &,const SpatialSample &)
  asm("__real__ZN4dome12SpatialInput6updateERKNS_13SpatialSampleE");
extern "C" void wrapped_update(SpatialInput &,const SpatialSample &)
  asm("__wrap__ZN4dome12SpatialInput6updateERKNS_13SpatialSampleE");
extern "C" void wrapped_update(SpatialInput &input,const SpatialSample &sample) {
  caller_fault::input_thread=true;real_update(input,sample);
}
namespace rebase_observer {
thread_local bool submit_on_commit=false;
Document *document=nullptr;SpatialFrame frame;bool submitted=false;
}
extern "C" void real_scope_commit(void *)
  asm("__real__ZN4dome8Document16RetopoInputScope6commitEv") __attribute__((weak));
extern "C" void wrapped_scope_commit(void *)
  asm("__wrap__ZN4dome8Document16RetopoInputScope6commitEv");
extern "C" void wrapped_scope_commit(void *scope) {
  if(rebase_observer::submit_on_commit) {
    rebase_observer::submit_on_commit=false;
    rebase_observer::document->submit(rebase_observer::frame);
    rebase_observer::submitted=true;
  }
  if(!real_scope_commit)std::abort();
  real_scope_commit(scope);
}
namespace {
enum class Route { Open,Begin,Sample,End,Cancel,Undo,HeldUndo,Redo,Close,Distance,Rebase,SpatialBegin };
constexpr const char *names[]{"open","begin","sample","end","cancel","undo","held-undo","redo","close","distance","rebase","spatial-begin"};
void setup(Document &d,const Scene &scene,Route route) {
  if(route==Route::Open){d.replace(scene.snapshot());check(d.wait_mesh(),"Opening fixture mesh reaches current source");return;}
  points(d,scene);
  if(route==Route::Sample){check(d.begin_desktop(),"Sample fixture owns actual empty gesture");ready(d);}
  if(route==Route::End||route==Route::Cancel||route==Route::HeldUndo)hold(d);
  if(route==Route::Redo){check(d.undo(),"Redo fixture actually removes a point");ready(d);}
}
void invoke(Document &d,Route route) {
  switch(route) {
  case Route::Open:d.begin_retopology();break;
  case Route::Begin:check(d.begin_desktop(),"Actual desktop Begin accepts");break;
  case Route::Sample:d.stamp_desktop(held_brush());break;
  case Route::End:d.end_desktop();break;
  case Route::Cancel:d.cancel_desktop();break;
  case Route::Undo:case Route::HeldUndo:check(d.undo(),"Actual public Undo accepts");break;
  case Route::Redo:check(d.redo(),"Actual public Redo accepts");break;
  case Route::Close:d.end_retopology(false);break;
  case Route::Distance:d.retopology_distance(75);break;
  case Route::Rebase:d.rebase_space(CanvasTransform{{.1f,0,0},{},1});break;
  case Route::SpatialBegin:d.begin_spatial();break;
  }
}
size_t route_count(const Scene &scene,Route route) {
  Document d;setup(d,scene,route);
  caller_fault::calls=caller_fault::boundary=0;caller_fault::enabled=true;
  invoke(d,route);caller_fault::enabled=false;const auto count=caller_fault::calls;
  check(d.wait_retopology(),"Measured caller route reaches its real endpoint");return count;
}
void denied_route(const Scene &scene,Route route,size_t boundary) {
  Document d;setup(d,scene,route);const auto previous=d.frame().state;
  const auto before=previous->retopology?previous->retopology->topology->encode():std::string{};
  const auto original=scene.encode();
  caller_fault::calls=0;caller_fault::boundary=boundary;caller_fault::hit=false;caller_fault::enabled=true;
  bool failed=false;try{invoke(d,route);}catch(const std::bad_alloc&){failed=true;}
  caller_fault::enabled=false;
  check(failed&&caller_fault::hit,"Measured persistent route allocation actually refuses");
  check(d.frame().state==previous&&d.copy_scene().encode()==original,"Refused route retains exact immutable view/source/paint/pose/meta");
  if(route==Route::Open) {
    d.begin_retopology();ready(d);check(d.frame().state->retopology->topology->vertices().empty(),"Refused Open cannot leave a hidden initial workspace/session");
  }else if(route==Route::Begin) {
    check(d.begin_desktop(),"Refused Begin cannot retain hidden physical ownership");d.cancel_desktop();barrier(d);
  }else if(route==Route::Sample) {
    // End is a healthy FIFO witness, not Cancel which could erase a hidden
    // rejected point and accidentally make an incorrect implementation pass.
    d.end_desktop();barrier(d);
  }else if(route==Route::End||route==Route::Cancel||route==Route::HeldUndo) {
    d.stamp_desktop(held_brush());ready(d);d.end_desktop();barrier(d);
  }else if(route==Route::Close) {
    check(d.begin_desktop(),"Refused workspace Cancel retains its real draft input owner");
    check(bool(d.frame().state->retopology),"Refused Close cannot reinterpret next input as source painting");
    d.cancel_desktop();barrier(d);
  }else barrier(d);
  const auto current=d.frame().state;
  if(route!=Route::Open)check(current->retopology->topology->encode()==before,"Healthy accepted barrier proves no hidden route or history operation survived refusal");
  if(route==Route::Distance)check(current->retopology->snap_mm==previous->retopology->snap_mm,"Refused projection setting cannot affect subsequent real input");
  if(route==Route::Rebase||route==Route::SpatialBegin)check(current->input_epoch==previous->input_epoch&&current->canvas.position.x==previous->canvas.position.x&&current->canvas.position.y==previous->canvas.position.y&&current->canvas.position.z==previous->canvas.position.z,"Refused rebase retains genuine controller epoch and canvas");
  check(d.copy_scene().encode()==original,"Route recovery preserves complete original source identity");
  d.end_retopology(false);check(!d.frame().state->retopology&&d.copy_scene().encode()==original,"Healthy explicit Cancel closes the recovered workspace exactly");
}
struct InputHand {
  Document &d;SpatialFrame f;
  explicit InputHand(Document &doc):d(doc) {
    check(d.wait_mesh(),"Tracked input uses the actual current immutable source mesh");
    f.focused=f.head_valid=true;f.head.position={0,1.6f,0};f.hands[1].tracked.valid=true;send();
  }
  void send(float pressure=0,float x=0,float y=0,bool click=false) {
    ++f.sequence;f.time+=.02;auto &h=f.hands[1];h.tracked.trigger=pressure;h.x=x;h.y=y;h.joystick=click;
    d.submit(f);check(d.wait_input(),"Actual shared controller worker drains");
  }
  void axis(int sector) {const float a=sector*6.283185307f/12;send(0,std::cos(a),std::sin(a));}
  void click(int sector) {const float a=sector*6.283185307f/12;send(0,std::cos(a),std::sin(a),true);}
  void sector(int n) {axis(n);click(n);send();}
  void menu() {send(0,0,0,true);send();}
  void page() {
    menu();
    // Existing retopology resumes its contextual page directly. The root
    // navigation applies only before a workspace exists, as in the older
    // opening fixture; do not dispatch Undo/settings against page16 here.
    if(d.frame().state->menu_page!=16){sector(8);sector(9);sector(11);}
    check(d.frame().state->menu&&d.frame().state->menu_page==16,"Real shared menu reaches Retopology page");
  }
  void position(Vec3 p) {f.hands[1].tracked.grip.position=ob::transform(d.frame().state->canvas,{p.x,p.y,p.z});}
};
void input_face(const Scene &scene,size_t at,bool measure,size_t &count) {
  Document d;points(d,scene);check(d.wait_mesh(),"Controller source has current immutable display mesh");
  const auto before=d.frame().state->retopology->topology->encode();InputHand hand(d);hand.page();hand.axis(2);
  caller_fault::input_calls=0;caller_fault::input_boundary=at;caller_fault::input_hit=false;caller_fault::input_enabled=true;
  hand.click(2);caller_fault::input_enabled=false;count=caller_fault::input_calls.load();
  if(!measure)check(caller_fault::input_hit,"Actual shared controller allocation boundary is denied persistently");
  hand.send();barrier(d);
  if(!measure)check(d.frame().state->retopology->topology->encode()==before,"Shared command refusal leaves no hidden Face after healthy input/report/FIFO recovery");
  else check(d.frame().state->retopology->topology->faces().size()==1,"Measured real controller command creates one Face");
  check(d.copy_scene().encode()==scene.encode(),"Shared input preserves full affine paint/pose/source identity");
}
void terminal_cases(const Scene &scene,const std::filesystem::path &out) {
  for(bool apply:{false,true}) {
    Document d;points(d,scene);d.edit_retopology(RetopoAction::Face);ready(d);
    InputHand hand(d);hand.page();const int sector=apply?10:11;hand.axis(sector);
    hand.f.hands[1].undo=hand.f.hands[1].redo=true;hand.click(sector);
    if(apply) {
      auto save=d.save_async(out/"controller-apply.dsculpt");save.get();
      check(d.wait_asset()&&!d.frame().state->retopology&&d.copy_scene().objects().size()==2,"Terminal actual Apply plus simultaneous history retains its ordered patch");
      check(Scene::load(out/"controller-apply.dsculpt").encode()==d.copy_scene().encode(),"Controller Apply and immediate Save recover exact native payload");
    }else check(!d.frame().state->retopology&&d.copy_scene().encode()==scene.encode(),"Terminal actual Cancel consumes simultaneous history without touching source namespace");
    const auto exact=d.copy_scene().encode();hand.send();
    check(d.copy_scene().encode()==exact,"Held history cannot leak into new namespace after terminal command");
    hand.f.hands[1].undo=hand.f.hands[1].redo=false;hand.send();
    if(apply) {
      hand.f.hands[1].undo=true;hand.send();check(d.copy_scene().encode()==scene.encode(),"Fresh real history release/press can undo accepted patch");
      hand.send();check(d.copy_scene().encode()==scene.encode(),"Holding accepted Undo cannot repeat against another history entry");
      hand.f.hands[1].undo=false;hand.send();hand.f.hands[1].redo=true;hand.send();
      check(d.copy_scene().encode()==exact,"Fresh release/Redo restores exact accepted source/patch");
    }
  }
}
void history_level_focus_rebase(const Scene &scene) {
  Document d;points(d,scene);InputHand hand(d);
  const auto original=d.copy_scene().encode();
  auto count=[&]{ready(d);return d.frame().state->retopology->topology->vertices().size();};
  auto processed=[&]{
    static uint64_t marker=900000;const auto next=++marker;
    hand.f.hands[1].tracked.timeMs=next;
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    do{hand.send();}while(d.frame().state->hands[1].tracked.timeMs!=next&&std::chrono::steady_clock::now()<until);
    check(d.frame().state->hands[1].tracked.timeMs==next,"History-level probe observes a genuinely processed frame");
  };
  check(count()==3,"History focus/rebase probe starts with three independent accepted points");
  hand.f.hands[1].undo=true;processed();check(count()==2,"First history level press removes exactly one real point");
  processed();check(count()==2,"Held history level does not repeat");
  hand.f.focused=false;hand.send();check(count()==2,"Actual focus loss preserves draft history");
  hand.f.focused=true;processed();check(count()==2,"Focus regain with held button is not a new history edge");
  d.rebase_space(CanvasTransform{{.1f,0,0},{},1});processed();
  check(count()==2,"Real spatial epoch rebase retains consumed history levels");
  d.begin_spatial();processed();check(count()==2,"Spatial startup cannot manufacture a held history press");
  hand.f.hands[1].undo=false;processed();hand.f.hands[1].undo=true;processed();
  check(count()==1,"A genuine release then second press performs the next Undo");
  hand.f.hands[1].undo=false;processed();hand.f.hands[1].redo=true;processed();
  check(count()==2,"Fresh Redo level restores exactly one point");
  processed();check(count()==2,"Held Redo cannot repeat");
  hand.f.focused=false;hand.send();hand.f.focused=true;processed();
  check(count()==2,"Focus regain preserves the independently consumed Redo latch");
  hand.f.hands[1].redo=false;processed();hand.f.hands[1].redo=true;processed();
  check(count()==3,"Second genuine Redo press restores the exact three-point draft");
  check(d.copy_scene().encode()==original,"History focus/rebase never mutates original affine paint/skin/morph/source");
  d.end_retopology(false);
}
void hold_controller(InputHand &hand) {
  hand.position(point(matrix,evaluated({.22f,.17f,.061f},true)));hand.send(.7f);ready(hand.d);
  check(hand.d.frame().state->retopology->topology->vertices().size()==4,"Actual tracked pressure holds a real fourth point");
}
void spatial_cancel_routes(const Scene &scene) {
  for(bool finish:{false,true}) {
    size_t count=0;
    for(size_t repeat=0;repeat<3;++repeat) {
      Document d;points(d,scene);InputHand hand(d);hold_controller(hand);
      caller_fault::calls=caller_fault::boundary=0;caller_fault::enabled=true;
      if(finish)d.finish_spatial();else d.cancel_spatial_menu();
      caller_fault::enabled=false;const auto n=caller_fault::calls;
      if(repeat)check(n==count,"Spatial cancellation caller count is stable");count=n;
      ready(d);check(d.frame().state->retopology->topology->vertices().size()==3,"Healthy actual shutdown/menu cancellation discards exactly the held point");
    }
    std::cout<<"SPATIAL "<<(finish?"finish":"menu-cancel")<<" allocations="<<count<<std::endl;
    for(size_t at=1;at<=count;++at) {
      Document d;points(d,scene);InputHand hand(d);hold_controller(hand);
      const auto previous=d.frame().state;const auto held=previous->retopology->topology->encode();
      caller_fault::calls=0;caller_fault::boundary=at;caller_fault::hit=false;caller_fault::enabled=true;
      bool failed=false;try{if(finish)d.finish_spatial();else d.cancel_spatial_menu();}catch(const std::bad_alloc&){failed=true;}
      caller_fault::enabled=false;
      check(failed&&caller_fault::hit,"Every actual spatial cancellation caller boundary refuses persistently");
      check(d.frame().state==previous&&d.copy_scene().encode()==scene.encode(),"Refused cancellation preserves exact source and immutable held input view");
      // This healthy setting publication retains the actual pending/busy flag.
      // It cannot erase an already-enqueued Cancel as a second Cancel could.
      d.retopology_distance(50);ready(d);
      check(d.frame().state->retopology->topology->encode()==held,"Refused shutdown/menu cancellation cannot leave a hidden CancelGesture");
      if(finish)d.begin_spatial();else hand.send();
      ready(d);
      if(finish)check(d.frame().state->retopology->topology->vertices().size()==3,"Real re-entry after stopped producer cancels its preserved held input once");
      else check(d.frame().state->retopology->topology->encode()==held,"Healthy physical release finalizes the exact preserved gesture");
      d.end_retopology(false);check(d.copy_scene().encode()==scene.encode(),"Spatial cancellation recovery leaves every native source owner exact");
    }
  }
}
void frame_batch(const Scene &scene,const std::filesystem::path &path) {
  Document d;points(d,scene);const auto base=d.frame().state->retopology->topology->encode();InputHand hand(d);hold_controller(hand);
  const auto held=d.frame().state->retopology->topology->encode();
  caller_fault::observed_ends=caller_fault::observed_undos=0;caller_fault::observe_actions=true;
  caller_fault::worker_hit=false;caller_fault::deny_after_action=int(RetopoAction::Undo);
  hand.f.hands[1].undo=true;hand.send();
  check(d.wait_retopology(),"Real release+history frame reaches worker publication");caller_fault::observe_actions=false;
  check(caller_fault::worker_hit&&caller_fault::observed_ends==1&&caller_fault::observed_undos==1,"Actual same frame edits EndGesture then Undo before late publication failure");
  check(!d.frame().state->error.empty()&&d.frame().state->retopology->topology->encode()==held,"Failed frame cannot partially publish a gesture/history batch");
  std::cout<<"ACTUAL frame EndGesture="<<caller_fault::observed_ends<<" Undo="<<caller_fault::observed_undos<<" workerHit="<<caller_fault::worker_hit<<std::endl;
  // A partial End commit gives Undo a Redo entry. An intact held gesture is
  // cancelled by Undo and has no such entry: geometry alone cannot prove this.
  d.edit_retopology(RetopoAction::Undo);ready(d);
  check(d.frame().state->retopology->topology->encode()==base&&!d.frame().state->retopology->can_redo,"Exact unfinished gesture history proves no hidden partial EndGesture committed");
  retry(d,scene,path);
}
void gate_sample() {
  std::lock_guard lock(caller_fault::gate_mutex);caller_fault::gate_entered=caller_fault::gate_released=false;
  caller_fault::gate_action=int(RetopoAction::Sample);
}
void wait_sample() {
  std::unique_lock lock(caller_fault::gate_mutex);
  check(caller_fault::gate_changed.wait_for(lock,std::chrono::seconds(120),[]{return caller_fault::gate_entered;}),"Real worker sample is held outside the model lock");
}
Brush drag_brush(float x) {
  Brush b;b.radius=.01f;b.from=b.to=point(matrix,evaluated({x,.15f,.2f*x+.015f},true));return b;
}
void queue_budget(const Scene &scene) {
  Document d;points(d,scene);const auto before=d.frame().state->retopology->topology->encode();
  d.edit_retopology(RetopoAction::MoveMode);ready(d);check(d.begin_desktop(),"Real Move gesture owns bounded queued samples");
  d.stamp_desktop(drag_brush(0));ready(d);const auto snapshot=d.frame().state->retopology->topology;
  caller_fault::GateRelease release;gate_sample();d.stamp_desktop(drag_brush(.01f));wait_sample();
  for(size_t n=0;n<254;++n)d.stamp_desktop(drag_brush(.01f+.0001f*float(n)));
  const auto previous=d.frame().state;
  bool full=false;try{d.stamp_desktop(drag_brush(.04f));}catch(const std::length_error&){full=true;}
  check(full&&d.frame().state==previous,"254 actual queued operations refuse prospectively with exact caller view");
  d.end_desktop(); // Reserved finalizer room cannot be consumed by ordinary input.
  bool undo_full=false;try{d.edit_retopology(RetopoAction::Undo);}catch(const std::length_error&){undo_full=true;}
  check(undo_full,"An accepted finalizer does not reopen ordinary queue capacity");
  caller_fault::release_gate();ready(d);
  const auto accepted=d.frame().state->retopology->topology->encode();
  check(accepted!=before&&snapshot->encode()==before&&d.copy_scene().encode()==scene.encode(),"All accepted samples retain immutable earlier draft/source snapshots");
  d.edit_retopology(RetopoAction::Undo);ready(d);check(d.frame().state->retopology->topology->encode()==before,"Whole bounded Move gesture remains exactly one local Undo");
  d.edit_retopology(RetopoAction::Redo);ready(d);check(d.frame().state->retopology->topology->encode()==accepted,"Redo restores the final immutable bounded gesture");d.end_retopology(false);
}
void stale_and_epoch(const Scene &scene) {
  {
    Document d;points(d,scene);InputHand hand(d);
    rebase_observer::document=&d;rebase_observer::frame=hand.f;
    ++rebase_observer::frame.sequence;rebase_observer::frame.time+=.02;
    rebase_observer::frame.hands[1].tracked.grip.position={.11f,.22f,.33f};
    rebase_observer::submitted=false;rebase_observer::submit_on_commit=true;
    const auto epoch=d.frame().state->input_epoch;d.rebase_space(CanvasTransform{{.1f,0,0},{},1});
    check(rebase_observer::submitted&&d.wait_input(),"Actual producer queues the new published epoch before retirement commits");
    const auto state=d.frame().state;const auto p=state->hands[1].tracked.grip.position;
    check(state->input_epoch==epoch+1&&p.x==.11f&&p.y==.22f&&p.z==.33f,"Deferred retirement preserves genuine concurrently queued current-epoch input");
    d.end_retopology(false);rebase_observer::document=nullptr;
  }
  {
    Document d;points(d,scene);d.edit_retopology(RetopoAction::MoveMode);ready(d);check(d.begin_desktop(),"Stale-source fixture owns actual Move input");
    d.stamp_desktop(drag_brush(0));ready(d);
    caller_fault::GateRelease release;gate_sample();d.stamp_desktop(drag_brush(.02f));wait_sample();
    auto replacement=scene.snapshot();replacement.active().name="Independent replacement source";
    try{d.replace(replacement.snapshot());}
    catch(const std::exception &e){throw std::runtime_error(std::string("Real stale source replacement failed: ")+e.what());}
    caller_fault::release_gate();
    d.begin_retopology();ready(d);
    check(d.copy_scene().encode()==replacement.encode()&&d.frame().state->retopology->topology->vertices().empty(),"Stale accepted worker candidate cannot cross real source/session replacement");
    d.end_retopology(false);
  }
}

}
int main(int argc,char **argv)try {
  const auto scene=source(matrix,true);
  if(argc==3&&std::string_view(argv[1])=="--frame-counter") {frame_batch(scene,argv[2]);return 0;}
  if(argc==3&&std::string_view(argv[1])=="--counter") {
    const auto found=std::find(std::begin(names),std::end(names),std::string_view(argv[2]));
    if(found==std::end(names))return 2;const auto route=Route(found-std::begin(names));
    const auto count=route_count(scene,route);std::cout<<"ACTUAL caller last boundary "<<count<<std::endl;
    denied_route(scene,route,count);std::cout<<"PASS route counter invariant\n";return 0;
  }
  if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
  size_t denials=0;
  for(size_t index=0;index<std::size(names);++index) {
    const auto route=Route(index);size_t count=0;
    for(size_t repeat=0;repeat<3;++repeat){const auto n=route_count(scene,route);if(repeat)check(n==count,"Full route caller allocation count is stable");count=n;}
    check(count>0,"Full input route has real measured allocations");
    std::cout<<"ROUTE "<<names[index]<<" allocations="<<count<<std::endl;
    for(size_t at=1;at<=count;++at){denied_route(scene,route,at);++denials;}
  }
  size_t count=0;input_face(scene,0,true,count);const auto input_count=count;
  for(size_t repeat=0;repeat<2;++repeat){input_face(scene,0,true,count);check(count==input_count,"Whole controller frame allocation count is stable");}
  std::cout<<"INPUT Face allocations="<<input_count<<std::endl;
  for(size_t at=1;at<=input_count;++at){input_face(scene,at,false,count);++denials;}
  terminal_cases(scene,out);
  history_level_focus_rebase(scene);
  std::cout<<"TAIL spatial cancellation"<<std::endl;spatial_cancel_routes(scene);
  std::cout<<"TAIL atomic frame"<<std::endl;frame_batch(scene,out/"frame-retry.dsculpt");
  std::cout<<"TAIL immutable queue budget"<<std::endl;queue_budget(scene);
  std::cout<<"TAIL stale and epoch"<<std::endl;stale_and_epoch(scene);
  // Keep the original public discrete/worker-batch/ordered Save oracle in the
  // same whole consumer, now against this complete route implementation.
  char *prior_args[]{argv[0],argv[1]};check(prior_caller_main(2,prior_args)==0,"Prior public caller admission controls remain exact");
  std::cout<<"PASS "<<checks<<" full input admission checks; "<<denials<<" route persistent boundaries\n";return 0;
}catch(const std::exception &e){caller_fault::enabled=caller_fault::deny_publication=false;caller_fault::input_enabled=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
