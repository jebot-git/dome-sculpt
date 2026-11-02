// SPDX-License-Identifier: GPL-3.0-or-later
#include "cut-color-fixture.hpp"
#include "dome/document.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <sys/stat.h>
using namespace dome;
namespace {
thread_local bool tracking=false;
bool hit=false;size_t calls=0,boundary=SIZE_MAX;
void require(bool yes,const char *why){if(!yes)throw std::runtime_error(why);}
struct Gate {
 std::mutex mutex;std::condition_variable changed;bool armed=false,entered=false;
 void arm(){std::lock_guard lock(mutex);armed=true;entered=false;}
 void stop(){std::unique_lock lock(mutex);if(!armed)return;entered=true;changed.notify_all();changed.wait(lock,[&]{return !armed;});}
 void wait(){std::unique_lock lock(mutex);if(!changed.wait_for(lock,std::chrono::seconds(30),[&]{return entered;}))throw std::runtime_error("Earlier actual file fsync gate not entered");}
 void release(){std::lock_guard lock(mutex);armed=false;changed.notify_all();}
} gate;
struct Release{~Release(){tracking=false;gate.release();}};
}
[[gnu::noinline]] void *operator new(size_t n){if(tracking&&++calls>=boundary){hit=true;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](size_t n){return ::operator new(n);}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p,size_t)noexcept{std::free(p);}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd){struct stat status{};if(!fstat(fd,&status)&&S_ISREG(status.st_mode))gate.stop();return __real_fsync(fd);}
namespace dome {struct DocumentTransactionTestAccess {
 static auto tokens(const Document &d){std::lock_guard lock(d.model_mutex_);return std::tuple{d.content_,d.next_content_,d.saved_content_,d.undo_.size(),d.redo_.size(),d.state_.revision,d.state_.generation,d.recovery_content_,d.state_.edit_authority,d.state_.held_object,d.state_.selected_objects};}
 static auto leases(const Document &d){std::lock_guard lock(d.model_mutex_);return std::tuple{d.cut_source_.generation,d.cut_source_.revision,d.cut_source_.object,d.cut_source_.edit_authority,d.cut_base_,d.cut_bake_source_,d.native_source_scene_,d.asset_work_.load(),d.state_.cut,d.state_.asset_busy};}
 static auto input(const Document &d){std::lock_guard lock(d.model_mutex_);auto p=d.input_.menu_anchor();return std::tuple{d.input_.menu_open(),d.input_.menu_page(),d.input_.menu_index(),d.input_.command_device(),p.x,p.y,p.z};}
 static size_t queued(const Document &d){std::lock_guard lock(d.file_mutex_);return d.files_.size();}
};}
namespace {
struct Controller {
 Document &d;SpatialFrame f;
 explicit Controller(Document &doc):d(doc){f.focused=f.head_valid=true;f.head.position={0,1.6f,0};f.hands[1].tracked.valid=true;send();}
 void send(float x=0,float y=0,bool press=false){++f.sequence;f.time+=.02;f.hands[1].x=x;f.hands[1].y=y;f.hands[1].joystick=press;d.submit(f);require(d.wait_input(),"Actual controller input drains");}
 void open(){send(0,0,true);send();}
 void apply(){float a=7*6.283185307f/12;send(std::cos(a),std::sin(a));send(std::cos(a),std::sin(a),true);send();}
};
std::unique_ptr<Document> setup(){
 auto d=std::make_unique<Document>();static const auto original=[] {auto scene=cut_color_fixture::scene(true,true);cut_color_fixture::pose(scene);return scene;}();d->replace(original.snapshot());
 for(auto name:{"Admission retained A","Admission retained B"}){auto o=d->copy_scene().active();d->object_properties(o.id,name,o.visible,o.locked,o.transform,o.material);}
 require(d->undo()&&d->wait_mesh(),"Actual previous history/redo and source finish");d->configure_object_grab(true,true);d->hold_object(d->frame().state->active_object);return d;
}
void prepare(Document &d,Controller &hand){d.begin_cut();require(d.wait_cut()&&d.frame().state->cut&&d.frame().state->cut->preview&&d.frame().state->cut->preview->changed&&!d.frame().state->cut->busy,"Actual source/preview ready before direct Apply allocation scope");hand.send();hand.open();require(d.frame().state->menu&&d.frame().state->menu_page==17,"Actual menu/input lease is open before direct Apply");}
}
int main(int argc,char **argv)try{
 require(argc==2||argc==4,"Choose admission directory and optional first/last fault indices");std::filesystem::path directory=argv[1];std::filesystem::create_directories(directory);
 size_t count=0;
 {
  auto d=setup();Release release;gate.arm();auto prior=d->save_async(directory/"measure-before.dsculpt");gate.wait();Controller hand(*d);prepare(*d,hand);
  calls=0;boundary=SIZE_MAX;hit=false;tracking=true;d->end_cut(true);tracking=false;count=calls;
  require(count>10&&count<4096&&d->frame().state->cut->committing&&d->frame().state->asset_busy&&DocumentTransactionTestAccess::queued(*d)==1,"Measured direct Apply has exactly one admitted job and no fallible post-admission publication");
  auto saving=d->save_async(directory/"measure-after.dsculpt");gate.release();prior.get();saving.get();require(d->wait_asset()&&!d->frame().state->cut&&d->frame().state->error.empty(),"Measured accepted cut and dependent Save drain normally");
 }
 std::cout<<count<<" actual direct end_cut(true) admission allocations\n"<<std::flush;
 size_t first=1,last=count;if(argc==4){first=std::stoull(argv[2]);last=std::min(count,size_t(std::stoull(argv[3])));}require(first&&first<=last&&last<=count,"Measured direct admission fault range valid");
 for(size_t at=first;at<=last;++at){
  auto d=setup();Release release;auto path=directory/"retained.dsculpt";auto encoded=d->copy_scene().encode();d->copy_scene().save(path);gate.arm();auto prior=d->save_async(directory/"before.dsculpt");gate.wait();Controller hand(*d);prepare(*d,hand);
  auto state=d->frame().state;auto mesh=d->frame().surface;auto tokens=DocumentTransactionTestAccess::tokens(*d);auto leases=DocumentTransactionTestAccess::leases(*d);auto input=DocumentTransactionTestAccess::input(*d);
  calls=0;boundary=at;hit=false;bool failed=false;tracking=true;try{d->end_cut(true);}catch(const std::bad_alloc&){failed=true;}tracking=false;
  require(hit&&failed,"Every real direct Apply prefix allocation is actually denied");
  require(d->frame().state==state&&d->frame().surface==mesh&&d->frame().state->cut&&!d->frame().state->cut->committing&&!d->frame().state->asset_busy,"Failed direct Apply preserves exact ready draft/publication/mesh and cannot stick committing");
  require(d->copy_scene().encode()==encoded&&DocumentTransactionTestAccess::tokens(*d)==tokens&&DocumentTransactionTestAccess::leases(*d)==leases&&DocumentTransactionTestAccess::input(*d)==input&&DocumentTransactionTestAccess::queued(*d)==0,"Admission refusal preserves scene/history/redo/content/held/source/input leases and no queued work");
  bool no_phantom=false;try{auto unexpected=d->save_async(path);(void)unexpected;}catch(const std::logic_error&){no_phantom=true;}
  require(no_phantom&&DocumentTransactionTestAccess::queued(*d)==0&&Scene::load(path).encode()==encoded,"Unaccepted draft refuses Save rather than capturing a phantom pending result or changing old file bytes");
  // Reuse the real observed menu lease after its failed publication. This
  // catches lost button/device latches, not merely matching displayed labels.
  hand.apply();require(d->frame().state->cut&&d->frame().state->cut->committing&&d->frame().state->asset_busy&&DocumentTransactionTestAccess::queued(*d)==1,"Same actual controller input lease retries precisely one accepted cut");
  auto saving=d->save_async(path);gate.release();prior.get();saving.get();require(d->wait_asset()&&d->wait_mesh()&&!d->frame().state->cut&&d->frame().state->error.empty(),"Retry and actual ordered Save finish without a stuck draft");
  auto after=d->copy_scene();auto shell=after.active().id;require(Scene::load(path).encode()==after.encode()&&d->frame().state->held_object==shell&&d->object_editable(shell),"Successful retry Save owns exactly the accepted held shell endpoint");
  require(d->undo()&&d->copy_scene().encode()==encoded&&d->frame().state->held_object==d->frame().state->active_object,"Retry is exactly one source-retaining Undo with the original held endpoint");
  if((at&15)==0)std::cout<<"Reached direct Apply admission allocation "<<at<<std::endl;
 }
 std::cout<<"PASS actual direct Apply admission failures "<<first<<".."<<last<<" of "<<count<<" preserve exact ready draft, authority, input, no phantom Save and successful one-Undo retry\n";return 0;
}catch(const std::exception &e){tracking=false;gate.release();std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
