// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_mtoon_fixture
#include "mtoon-render.cpp"
#undef main
#include "dome/document.hpp"
#include "dome/native-authoring.hpp"
#include "dome/source-paint-bake.hpp"
#include "source-paint-fixtures.hpp"
#include "source-color-consumer.hpp"
#include "prepared-normal-consumer.hpp"
#include "source-paint-bake-fixtures.hpp"
#include "import-skin-fixtures.hpp"
#include "native-authoring-capture.hpp"
#include "native-authoring-compiler.hpp"
#include "emission-paint-access.hpp"
#include "gltf-document.hpp"
#include <fstream>
#include <mutex>
#include <condition_variable>
#include <sys/stat.h>
namespace allocation_probe {
thread_local bool active=false;thread_local size_t calls=0,limit=SIZE_MAX;
void hit(){if(active&&calls++>=limit)throw std::bad_alloc();}
struct Guard{explicit Guard(size_t at){calls=0;limit=at;active=true;}~Guard(){active=false;}};
}
void *operator new(size_t n){allocation_probe::hit();if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void *p)noexcept{std::free(p);}
void operator delete[](void *p)noexcept{std::free(p);}
void operator delete(void *p,size_t)noexcept{std::free(p);}
void operator delete[](void *p,size_t)noexcept{std::free(p);}
void *operator new(size_t n,std::align_val_t a){allocation_probe::hit();void *p=nullptr;if(const auto error=posix_memalign(&p,std::max(size_t(a),sizeof(void*)),n?n:1)){std::fprintf(stderr,"ALIGNED_NEW_FAILURE bytes=%zu alignment=%zu error=%d\n",n,size_t(a),error);throw std::bad_alloc();}return p;}
void *operator new[](size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void *p,std::align_val_t)noexcept{std::free(p);}
void operator delete[](void *p,std::align_val_t)noexcept{std::free(p);}
void operator delete(void *p,size_t,std::align_val_t)noexcept{std::free(p);}
void operator delete[](void *p,size_t,std::align_val_t)noexcept{std::free(p);}
void *operator new(size_t n,const std::nothrow_t&)noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
void *operator new[](size_t n,const std::nothrow_t&)noexcept{try{return ::operator new[](n);}catch(...){return nullptr;}}
void *operator new(size_t n,std::align_val_t a,const std::nothrow_t&)noexcept{try{return ::operator new(n,a);}catch(...){return nullptr;}}
void *operator new[](size_t n,std::align_val_t a,const std::nothrow_t&)noexcept{try{return ::operator new[](n,a);}catch(...){return nullptr;}}
void operator delete(void*p,const std::nothrow_t&)noexcept{std::free(p);}
void operator delete[](void*p,const std::nothrow_t&)noexcept{std::free(p);}
void operator delete(void*p,std::align_val_t,const std::nothrow_t&)noexcept{std::free(p);}
void operator delete[](void*p,std::align_val_t,const std::nothrow_t&)noexcept{std::free(p);}
namespace durable_gate {
std::mutex mutex;std::condition_variable changed;bool armed=false,entered=false,released=false;
void arm(){std::lock_guard lock(mutex);armed=true;entered=released=false;}
bool wait(){std::unique_lock lock(mutex);return changed.wait_for(lock,std::chrono::seconds(60),[]{return entered;});}
void release(){std::lock_guard lock(mutex);released=true;changed.notify_all();}
struct Guard{~Guard(){release();}};
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd){const auto result=__real_fsync(fd);struct stat s{};if(!result&&!fstat(fd,&s)&&S_ISREG(s.st_mode)){std::unique_lock lock(durable_gate::mutex);if(durable_gate::armed){durable_gate::armed=false;durable_gate::entered=true;durable_gate::changed.notify_all();durable_gate::changed.wait(lock,[]{return durable_gate::released;});}}return result;}
namespace {
std::vector<uint8_t> bytes(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void seed(const std::filesystem::path&p){std::ofstream f(p,std::ios::binary);f<<"Unchanged physical output";}
bool refusal(auto fn,std::string_view expected){try{fn();}catch(const std::exception&e){std::cout<<"EXPECTED "<<e.what()<<std::endl;return std::string_view(e.what()).find(expected)!=std::string_view::npos;}return false;}
bool cancelled(std::future<void> work){if(work.wait_for(std::chrono::seconds(120))!=std::future_status::ready)throw std::runtime_error("Physical export cancellation observer timed out");return refusal([&]{work.get();},"cancelled");}
Scene fixture(const std::filesystem::path&out,bool skin=false){
 auto f=skin?import_fixture::skinned(true):source_paint_fixture::input(0);auto &m=f.j["materials"][0];m["extensions"].erase("KHR_materials_unlit");m["emissiveFactor"]={.025f,.002f,.02f};m["extensions"]["KHR_materials_emissive_strength"]={{"emissiveStrength",8}};f.j["extensionsUsed"].push_back("KHR_materials_emissive_strength");
 if(skin){f.j["nodes"][1]["weights"]={.65f};f.j["nodes"][2]["weights"]={.35f};f.j["nodes"][3]["rotation"]={0,0,std::sin(.15f),std::cos(.15f)};}
 const auto path=out/(skin?"skin-input.glb":"input.glb");f.save(path);Scene initial;initial.active().visible=false;ImportAuthoringOptions options;options.paint_resolution=64;options.prepare_texture_atlas=true;auto imported=prepare_authoring_import(initial,path,options);
 auto &o=imported.scene.active();o.material.emissive={8,.016f,.16f};o.painted=o.painted->snapshot();auto &paint=*o.painted;
 PaintSettings s;s.channel=PaintChannel::Emission;s.front_only=false;s.emission_units=PaintEmissionUnits::Physical;s.emission={12,.005f,3};Brush b;b.from=b.to={0,0,0};b.normal={0,0,1};b.radius=.19f;b.strength=.73f;paint.begin_stroke();paint.stamp(b,s);check(paint.end_stroke(),"Contracts fixture owns an actual physical source paint stroke");
 const auto active=o.id;const auto hidden=imported.scene.duplicate(active);imported.scene.object(hidden).visible=false;imported.scene.select(active);imported.scene.validate();return imported.scene;
}
NativeAuthoringExport owner(const Scene&s){NativeAuthoringExport out;out.scene=std::make_shared<const Scene>(s.snapshot());out.allow_portable_fallback=true;return out;}
}
int main(int argc,char**argv)try{
 if(argc<2||argc>3)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 auto scene=fixture(out);const auto exact=scene.encode();auto captured=owner(scene);SourceUvBakeOptions policy;policy.resolution=64;
 if(argc==3&&std::string_view(argv[2])=="--oom"){
  const auto path=out/"oom.glb";seed(path);const auto prior=bytes(path);size_t baseline=0;
  {allocation_probe::Guard guard(SIZE_MAX);export_glb_authoring(captured,out/"baseline.glb",{},policy);baseline=allocation_probe::calls;}
  size_t denied=0;bool success=false;
  for(size_t prefix=0;prefix<=baseline+2048;++prefix){bool failed=false;
   {allocation_probe::Guard guard(prefix);try{export_glb_authoring(captured,path,{},policy);}catch(const std::bad_alloc&){failed=true;}}
   if(failed){if(bytes(path)!=prior||scene.encode()!=exact){std::cerr<<"PHYSICAL_FAILED_PREFIX "<<prefix<<" FILE_CHANGED "<<(bytes(path)!=prior)<<" OWNER_CHANGED "<<(scene.encode()!=exact)<<std::endl;throw std::runtime_error("Physical persistent allocation failure changed original file or owner");}++denied;}
   else{check(extract_native_authoring(bytes(path))->encode()==exact,"Physical allocation sweep eventually produces a complete exact native project");success=true;break;}
  }
  check(success&&denied>0,"Physical capture/compiler/float/factor/native-payload persistent allocation prefixes rollback and retry");
  std::cout<<"PHYSICAL_PERSISTENT_CPP_PREFIXES "<<denied<<" baseline="<<baseline<<std::endl;return 0;
 }
 NativeGraphics graphics;Fbo fbo;fbo.bind();SurfaceRenderer native;native.set_frame_time(0);color_consumer::Renderer reader;prepared_normal_consumer::Renderer roles;
 const auto path=out/"direct.glb";export_glb_authoring(captured,path,{},policy);const auto original=bytes(path);
 for(int n=0;n<6;++n){auto limit=policy;if(n==0)limit.max_working_bytes=4;if(n==1)limit.max_output_bytes=4;if(n==2)limit.max_sample_operations=4;if(n==3)limit.max_raster_samples=4;if(n==4)limit.max_vertices=1;if(n==5)limit.max_triangles=1;
  check(refusal([&]{export_glb_authoring(captured,path,{},limit);},"budget")&&bytes(path)==original&&scene.encode()==exact,"Physical geometry/image/work/raster quota refusal preserves prior output and exact native inputs");}
 size_t polls=0;GltfExportOptions counted;counted.cancelled=[&]{++polls;return false;};export_glb_authoring(captured,path,counted,policy);
 for(size_t at:{size_t(1),polls/4,polls/2,polls*3/4,polls-4}){size_t calls=0;auto c=counted;c.cancelled=[&]{return ++calls>=at;};check(refusal([&]{export_glb_authoring(captured,path,c,policy);},"cancelled")&&bytes(path)==original&&scene.encode()==exact,"Physical capture/compile/encode cancellation leaves exact native data and old file unchanged");}
 Document d;d.replace(scene.snapshot());d.shading(false);settle(d);const auto running=out/"running.glb",queued=out/"queued.glb",saved=out/"ordered.dsculpt";seed(running);seed(queued);const auto prior=bytes(running);
 {durable_gate::Guard guard;durable_gate::arm();auto a=d.export_asset(running,AssetExportFormat::EditableGlb256);check(durable_gate::wait(),"Actual physical file worker reaches durable temporary output before commit");auto b=d.export_asset(queued,AssetExportFormat::EditableGlb256);auto save=d.save_async(saved);check(d.frame().state->pending_exports==2&&d.frame().state->can_cancel_asset,"Real running and queued physical exports expose shared Cancel");d.cancel_background_work();durable_gate::release();check(cancelled(std::move(a))&&cancelled(std::move(b)),"Actual physical running and queued exports both cancel");save.get();check(bytes(running)==prior&&bytes(queued)==prior&&Scene::load(saved).encode()==exact&&d.copy_scene().encode()==exact,"Physical Cancel preserves both files, exact live project and following ordered native Save");}
 d.export_asset(running,AssetExportFormat::EditableGlb256).get();check(Scene::load(running).encode()==exact&&!d.frame().state->pending_exports,"Real physical export retry succeeds and retires file status");
 // A true near-one float replacement must retain visible HDR residuals.
 {auto near=fixture(out);auto &o=near.active();o.material.emissive={10000,0,0};o.painted=detail::EmissionExportAccess::without_emission(*o.painted);o.painted->add_layer("Near full float coverage");PaintSettings s;s.channel=PaintChannel::Emission;s.emission_units=PaintEmissionUnits::Physical;s.emission={0,0,0};s.front_only=false;Brush b;b.from=b.to={0,0,0};b.normal={0,0,1};b.radius=5;b.strength=1;o.painted->begin_stroke();for(int pass=0;pass<4;++pass)o.painted->stamp(b,s);check(o.painted->end_stroke(),"Actual physical black creates explicit replacement coverage");o.painted->layer_properties("Near full float coverage",true,.999999f);const auto before=near.encode();const auto file=out/"near-one.glb";export_glb_authoring(owner(near),file,{},policy);color_consumer::Model parsed(file);const auto &factor=parsed.json.at("materials").at(0).at("emissiveFactor");check(factor[0].get<double>()>.005&&factor[0].get<double>()<.02,"Near-one float coverage preserves HDR residual instead of rounding it to byte-one");check(extract_native_authoring(bytes(file))->encode()==before,"Near-one layer opacity and float values recover exactly");}
 // Actual deduplicated intermediate resources must keep unrelated material data.
 {auto mixed=scene.snapshot();const auto first=mixed.active().id;const auto second=mixed.duplicate(first);mixed.object(second).visible=true;mixed.object(second).material.emissive={2,.016f,.16f};const auto third=mixed.duplicate(first);auto &ordinary=mixed.object(third);ordinary.visible=true;ordinary.painted=detail::EmissionExportAccess::without_emission(*ordinary.painted);ordinary.material.emissive={};mixed.select(first);auto cap=detail::NativeAuthoringCapture::create(owner(mixed),{});auto compiled=detail::compile_native_authoring(*cap,policy,256*1024*1024,{});auto document=detail::make_gltf(compiled.surfaces);check(compiled.emission.size()==2&&document.objects.size()==3,"Mixed factory prepares two real physical fields and one ordinary source object");auto shared=document.objects[0];const auto unchanged=document.json.at("materials").at(shared.material);for(auto &id:document.objects){id.mesh=shared.mesh;id.material=shared.material;document.json["nodes"][id.node]["mesh"]=shared.mesh;}detail::finish_native_authoring_emission(document,compiled,256*1024*1024,{});check(document.objects[0].mesh!=document.objects[1].mesh&&document.objects[0].material!=document.objects[1].material&&document.objects[2].mesh==shared.mesh&&document.objects[2].material==shared.material,"Physical finisher owns distinct actual object/node/material remaps after real resource deduplication");check(document.json.at("materials").at(shared.material)==unchanged,"Unrelated aliased material remains byte-identical");for(const auto &id:document.objects)check(document.json.at("nodes").at(id.node).at("mesh")==id.mesh&&document.json.at("meshes").at(id.mesh).at("primitives").at(0).at("material")==id.material,"Finisher node and material references agree with actual document remaps");}
 {auto rig=fixture(out,true);const auto before=rig.encode();Document document;document.replace(rig.snapshot());document.shading(false);settle(document);auto image=draw(native,document.frame().surface);const auto file=out/"skin.glb";document.export_asset(file,AssetExportFormat::EditableGlb256).get();color_consumer::Model parsed(file);fbo.bind();reader.render(parsed.data,nullptr,{},true);auto q=fidelity(image,read());std::cout<<"PHYSICAL_SKIN visible="<<q.visible<<" mean="<<q.mean<<" interior="<<q.interior_maximum<<std::endl;check(q.visible>20&&q.mean<=2&&q.interior_maximum<=2,"Actual authored skin pose and nonzero artist morph retain finite Core-PBR quality");Document restore;restore.open_async(file).get();restore.shading(false);settle(restore);check(restore.copy_scene().encode()==before&&delta(image,draw(native,restore.frame().surface))==0,"Physical native recovery restores exact source skin/pose/artist targets and pixels");}
 check(scene.encode()==exact,"All contracts leave the original physical owner unchanged");std::cout<<"PASS "<<checks<<" physical export contracts\n";return 0;
}catch(const std::exception&e){durable_gate::release();std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
