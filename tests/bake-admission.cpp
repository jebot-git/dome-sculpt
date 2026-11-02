// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-color-fixtures.hpp"
#define main original_bake_policy_fixture
#include "bake-projection-policy.cpp"
#undef main
#include <cstdlib>
#include <new>
#include <sys/stat.h>
namespace {
thread_local bool tracking=false;
bool hit=false;size_t calls=0,boundary=SIZE_MAX;
void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
struct Gate {
 std::mutex mutex;std::condition_variable changed;bool armed=false,entered=false;
 void arm(){std::lock_guard lock(mutex);armed=true;entered=false;}
 void stop(){std::unique_lock lock(mutex);if(!armed)return;entered=true;changed.notify_all();changed.wait(lock,[&]{return !armed;});}
 void wait(){std::unique_lock lock(mutex);if(!changed.wait_for(lock,std::chrono::seconds(30),[&]{return entered;}))throw std::runtime_error("Actual preceding Save did not reach fsync gate");}
 void release(){std::lock_guard lock(mutex);armed=false;changed.notify_all();}
} gate;
struct Release{~Release(){tracking=false;gate.release();}};
auto settings_tuple(const ObjectBakeSettings &s){return std::tuple{s.front_mm,s.back_mm,s.ao_distance_mm,s.ao_samples,s.padding,s.allow_partial,s.allow_backfaces,s.prepare_colors,s.color_error,s.prepare_normals,s.normal_error};}
Scene fixture(){
 auto scene=source_color_fixture::model(true,false);const auto target=scene.active().id;const auto id=scene.add("Hidden original source",Volume{});
 auto source=source_color_fixture::model(false,false).active();source.id=id;source.name="Hidden original source";source.visible=false;source.locked=true;source.transform.position.z=.005f;scene.object(id)=std::move(source);scene.select(target);scene.validate();return scene;
}
std::unique_ptr<Document> setup(){
 static const auto original=fixture();auto d=std::make_unique<Document>();d->replace(original.snapshot());
 for(auto name:{"Retained target A","Retained target B"}){auto object=d->copy_scene().active();d->object_properties(object.id,name,object.visible,object.locked,object.transform,object.material);}
 require(d->undo()&&d->wait_mesh(),"Actual prior history and redo finish before admission");return d;
}
void prepare(Driver &hand){
 hand.open();hand.confirm(false);hand.sector(9);hand.point(520,210);hand.confirm(true);
 require(hand.doc.frame().state->controls.parameter==VrParameter::None&&hand.doc.frame().state->menu&&hand.doc.frame().state->menu_page==12&&hand.doc.frame().state->bake_settings.prepare_colors,"Actual same-controller Bake menu/configuration is ready before direct caller faults");
}
ObjectBakeSettings candidate(Document &d){auto s=d.frame().state->bake_settings;s.front_mm=73;s.back_mm=69;s.ao_samples=0;s.padding=3;return s;}
}
[[gnu::noinline]] void *operator new(size_t n){if(tracking&&++calls>=boundary){hit=true;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](size_t n){return ::operator new(n);}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p,size_t)noexcept{std::free(p);}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd){struct stat s{};if(!fstat(fd,&s)&&S_ISREG(s.st_mode))gate.stop();return __real_fsync(fd);}
namespace dome {struct DocumentTransactionTestAccess {
 static auto tokens(const Document &d){std::lock_guard lock(d.model_mutex_);return std::tuple{d.content_,d.next_content_,d.saved_content_,d.undo_.size(),d.redo_.size(),d.state_.revision,d.state_.generation,d.recovery_content_,d.state_.edit_authority,d.state_.held_object,d.state_.selected_objects};}
 static auto leases(const Document &d){std::lock_guard lock(d.model_mutex_);return std::tuple{d.native_source_scene_,d.asset_work_.load(),d.state_.asset_busy,d.control_before_.has_value(),d.control_source_.generation,d.control_source_.revision,d.control_source_.object,d.control_source_.edit_authority};}
 static auto controls(const Document &d){std::lock_guard lock(d.model_mutex_);return std::pair{d.state_.bake_source,settings_tuple(d.state_.bake_settings)};}
 static auto input(const Document &d){std::lock_guard lock(d.model_mutex_);auto p=d.input_.menu_anchor();return std::tuple{d.input_.menu_open(),d.input_.menu_page(),d.input_.menu_index(),d.input_.command_device(),p.x,p.y,p.z};}
 static size_t queued(const Document &d){std::lock_guard lock(d.file_mutex_);return d.files_.size();}
};}
int main(int argc,char **argv)try{
 require(argc==2||argc==4,"Choose admission output directory and optional first/last failure range");std::filesystem::path directory=argv[1];std::filesystem::create_directories(directory);size_t count=0;
 {
  auto d=setup();Release release;gate.arm();auto prior=d->save_async(directory/"measure-before.dsculpt");gate.wait();Driver hand(*d);prepare(hand);auto settings=candidate(*d);auto source=d->copy_scene().objects().back().id;
  calls=0;boundary=SIZE_MAX;hit=false;tracking=true;d->bake_from_object(source,settings);tracking=false;count=calls;
  require(count>10&&count<4096&&DocumentTransactionTestAccess::queued(*d)==1&&d->frame().state->asset_busy&&settings_tuple(d->frame().state->bake_settings)==settings_tuple(settings)&&DocumentTransactionTestAccess::controls(*d).second==settings_tuple(settings),"Measured caller has one accepted job and one matching published/internal Bake configuration");
  gate.release();prior.get();require(d->wait_asset()&&d->frame().state->error.empty(),"Measured accepted real Bake finishes normally");
 }
 std::cout<<count<<" actual bake_from_object caller admission allocations\n"<<std::flush;
 size_t first=1,last=count;if(argc==4){first=std::stoull(argv[2]);last=std::min(count,size_t(std::stoull(argv[3])));}require(first&&first<=last&&last<=count,"Measured admission fault range valid");
 for(size_t at=first;at<=last;++at){
  auto d=setup();Release release;auto old_file=directory/"old-source.dsculpt";auto before=d->copy_scene().encode();d->copy_scene().save(old_file);
  gate.arm();auto prior=d->save_async(directory/"before.dsculpt");gate.wait();Driver hand(*d);prepare(hand);auto settings=candidate(*d);auto source=d->copy_scene().objects().back().id;
  auto state=d->frame().state;auto mesh=d->frame().surface;auto tokens=DocumentTransactionTestAccess::tokens(*d);auto leases=DocumentTransactionTestAccess::leases(*d);auto controls=DocumentTransactionTestAccess::controls(*d);auto input=DocumentTransactionTestAccess::input(*d);
  calls=0;boundary=at;hit=false;bool failed=false;tracking=true;try{d->bake_from_object(source,settings);}catch(const std::bad_alloc&){failed=true;}tracking=false;
  require(hit&&failed,"Every measured real caller prefix allocation is actually denied");
  if(d->frame().state!=state||d->frame().surface!=mesh||d->frame().state->asset_busy)std::cerr<<"Boundary "<<at<<" changed publication/busy; queued="<<DocumentTransactionTestAccess::queued(*d)<<std::endl;
  require(d->frame().state==state&&d->frame().surface==mesh&&!d->frame().state->asset_busy,"Failed Bake caller preserves exact ready publication/mesh and cannot leave accepted phantom work");
  require(d->copy_scene().encode()==before&&DocumentTransactionTestAccess::tokens(*d)==tokens&&DocumentTransactionTestAccess::leases(*d)==leases&&DocumentTransactionTestAccess::controls(*d)==controls&&DocumentTransactionTestAccess::input(*d)==input&&DocumentTransactionTestAccess::queued(*d)==0,"Caller refusal preserves exact scene/source/paint/history/redo/content/configuration/input leases and queue");
  require(Scene::load(old_file).encode()==before,"Failed caller leaves pre-existing project bytes and complete source/paint state unchanged");
  hand.sector(3);require(d->frame().state->asset_busy&&DocumentTransactionTestAccess::queued(*d)==1,"The exact same observed controller menu lease retries precisely one real Bake");
  auto pending_save=d->save_async(old_file);
  require(pending_save.wait_for(std::chrono::seconds(0))!=std::future_status::ready&&DocumentTransactionTestAccess::queued(*d)==2&&Scene::load(old_file).encode()==before,"Save queues behind the actual retried Bake while preserving the existing file and its exact source endpoint");
  gate.release();prior.get();pending_save.get();require(d->wait_asset()&&d->wait_mesh()&&d->frame().state->error.empty(),"Actual same-controller retry and its dependent Save complete preparation and normal/color Bake");
  auto after=d->copy_scene().encode();require(Scene::load(old_file).encode()==after,"The pending Save writes the real accepted Bake result rather than its earlier source");auto path=directory/"accepted.dsculpt";d->save(path);require(Scene::load(path).encode()==after&&d->undo()&&d->copy_scene().encode()==before,"Successful retry saves its accepted result and adds exactly one source-retaining Undo");
  if((at&15)==0)std::cout<<"Reached real Bake caller failure "<<at<<std::endl;
 }
 std::cout<<"PASS actual Bake caller admission failures "<<first<<".."<<last<<" of "<<count<<" preserve exact configuration/scene/source/paint/history/leases/queue, no phantom Save and same-controller retry\n";return 0;
}catch(const std::exception &e){tracking=false;gate.release();std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
