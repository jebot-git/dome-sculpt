// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_shape_kernel_suite
#include "shapekey-authoring.cpp"
#undef main
#include "dome/document.hpp"
#include <atomic>
#include <thread>
namespace dome {
struct DocumentTransactionTestAccess {
 static bool processing_empty(Document &d){std::lock_guard lock(d.input_mutex_);return d.processing_&&d.inputs_.empty();}
 static size_t queued(Document &d){std::lock_guard lock(d.input_mutex_);return d.inputs_.size();}
 static bool overflow_pending(Document &d){std::lock_guard lock(d.input_mutex_);return d.reset_input_;}
};
}
namespace publication {
std::atomic_bool armed=false,entered=false,released=false,expect_overflow=false,retained_overflow=false;
Document *owner=nullptr;
void release(){released=true;released.notify_all();}
struct Release {~Release(){release();}};
template<class F>void wait(F predicate){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(15);while(!predicate()){if(std::chrono::steady_clock::now()>end)throw std::runtime_error("Input publication gate timed out");std::this_thread::yield();}}
}
extern "C" void __real__ZN4dome8Document11Transaction6commitEb(void*,bool);
extern "C" void __wrap__ZN4dome8Document11Transaction6commitEb(void *transaction,bool checkpoint){
 bool gated=false;
 if(publication::armed&&publication::owner){auto state=publication::owner->frame().state;
  if(!state->asset_busy&&state->message.starts_with("Author shapekey complete")&&publication::armed.exchange(false)){
   gated=true;publication::entered=true;publication::entered.notify_all();while(!publication::released)publication::released.wait(false);
  }
 }
 __real__ZN4dome8Document11Transaction6commitEb(transaction,checkpoint);
 if(gated&&publication::expect_overflow)publication::retained_overflow=DocumentTransactionTestAccess::overflow_pending(*publication::owner);
}
namespace {
void ready(Document &d){if(!d.wait_asset()||!d.wait_mesh())throw std::runtime_error("Shapekey work did not settle");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
struct Hand {
 Document &doc;SpatialFrame frame;
 explicit Hand(Document &d):doc(d){frame.focused=frame.head_valid=true;frame.head.position={0,1.6f,0};frame.hands[1].tracked.valid=true;send();}
 void send(float x=0,float y=0,bool click=false){++frame.sequence;frame.time+=.05;auto &hand=frame.hands[1];hand.x=x;hand.y=y;hand.joystick=click;doc.submit(frame);if(!doc.wait_input())throw std::runtime_error("Input worker timed out");}
 void sector(int i){send();float angle=i*3.14159265358979323846f/6;send(std::cos(angle),std::sin(angle));send(std::cos(angle),std::sin(angle),true);send();}
 void open(){send(0,0,true);send();sector(8);sector(9);sector(7);}
};
}
int main()try{
 {
  Volume volume;SpatialInput input(volume);input.retain_page(24,2,0);
  SpatialSample sample;sample.id=2;sample.sequence=1;sample.time=1;sample.focused=sample.tracked=true;sample.stick_y=1;
  input.update(sample);
  check(input.menu_index()==3&&input.menu_open(),"First released controller sample gives immediate radial hover feedback after a retained-menu rearm");
  check(!input.take_command()&&input.take_controls().empty(),"Released hover feedback never bypasses command or parameter press guards");
 }
 for(bool overflow:{false,true}){
 publication::armed=false;publication::entered=false;publication::released=false;
 publication::expect_overflow=overflow;publication::retained_overflow=false;
 Document document;publication::Release release;document.replace(fixture());ready(document);
 document.author_shape({});ready(document);Hand controller(document);controller.open();
 check(document.frame().state->menu_page==24,"Controller reaches the actual retained shapekey tools page");
 auto epoch=document.frame().state->input_epoch;publication::owner=&document;publication::armed=true;
 ShapeKeyEdit duplicate;duplicate.action=ShapeKeyAction::Duplicate;document.author_shape(duplicate);
 publication::wait([]{return publication::entered.load();});
 check(document.frame().state->input_epoch>epoch&&!document.frame().state->asset_busy,"New immutable authoring endpoint is already visible before rebind queue cleanup");
 // The real input worker takes A and then waits on the publishing model lock.
 // B is submitted through the real public API with the NEW published epoch.
 auto a=controller.frame;++a.sequence;a.time+=.05;a.hands[1].x=a.hands[1].y=0;a.hands[1].joystick=false;document.submit(a);
 publication::wait([&]{return DocumentTransactionTestAccess::processing_empty(document);});
 auto b=a;b.hands[1].x=0;b.hands[1].y=1;
 for(int i=0;i<(overflow?65:1);++i){++b.sequence;b.time+=.001;document.submit(b);}
 check(DocumentTransactionTestAccess::queued(document)==1,"Next real hover is queued behind a real in-flight released sample");
 if(overflow)check(DocumentTransactionTestAccess::overflow_pending(document),"Real bounded-queue overflow requests a cancellation reset before the new endpoint resumes");
 publication::release();ready(document);check(document.wait_input(),"Both input submissions settle after publication");
 if(overflow)check(publication::retained_overflow,"Rebind cleanup preserves the pending cancellation reset for retained current-epoch overflow input");
 else check(document.frame().state->menu_page==24&&document.frame().state->menu_index==3,"Rebind cleanup retains a new-epoch controller hover instead of dropping its actual selection");
 check(document.copy_scene().active().deformation->morphs->targets.size()==2,"Retaining current input never duplicates the accepted shapekey operation");
 }
 std::cout<<"PASS input publication checks "<<checks<<'\n';return 0;
}catch(const std::exception &error){publication::release();std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
