// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_NORMAL_COLOR_RENDER_NO_MAIN
#include "normal-color-uv-render.cpp"
#undef DOME_NORMAL_COLOR_RENDER_NO_MAIN
#include "dome/native-authoring.hpp"
#include "dome/emission-native.hpp"
#include "dome/bake.hpp"
#include <numeric>
#include "native-authoring-compiler.hpp"
#include "paint-overlay-sample.hpp"
#include <fstream>
namespace {
std::vector<uint8_t> bytes(const std::filesystem::path &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
Scene plain(){Scene scene;scene.active().visible=false;const auto id=scene.add("Native physical paint",Volume{});scene.select(id);auto &o=scene.active();o.mesh_active=true;o.painted=std::make_shared<PaintedMesh>(quad(),64);o.material.unlit=false;o.material.color={.7f,.8f,.6f,1};return scene;}
void ink(Scene &scene){auto &o=scene.active();o.material.emissive={8,.016f,.16f};o.painted=o.painted->snapshot();auto &p=*o.painted;p.add_layer("Float partial mask");PaintSettings s;s.front_only=false;s.channel=PaintChannel::Emission;s.emission_units=PaintEmissionUnits::Physical;s.emission={12,.002f,3};Brush b;b.from=b.to={-.02f,.015f,0};b.normal={0,0,1};
 const auto &mesh=p.mesh();for(size_t f=0;f<mesh.indices.size()/3;++f)if(mesh.normals.at(mesh.indices[f*3]).z>.9f){b.from=b.to=(mesh.positions.at(mesh.indices[f*3])+mesh.positions.at(mesh.indices[f*3+1])+mesh.positions.at(mesh.indices[f*3+2]))/3;break;}
 b.radius=.19f;b.strength=.73f;p.begin_stroke();p.stamp(b,s);check(p.end_stroke(),"Actual native/retained physical brush changes float detail");p.layer_properties("Float partial mask",true,.63f);s.channel=PaintChannel::Mask;s.mask=.4f;b.radius=.1f;p.begin_stroke();p.stamp(b,s);check(p.end_stroke(),"Physical float detail uses the actual editable common mask");p.add_layer("Hidden high radiance");s.channel=PaintChannel::Emission;s.emission={10000,10000,10000};p.begin_stroke();p.stamp(b,s);p.end_stroke();p.layer_properties("Hidden high radiance",false,1);}
}
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
Scene prepared64(){
 auto scene=current_color_target_fixture::fixture(false,64);auto source=scene.objects().back().id;
 cut_color_fixture::pose(scene);auto d=*scene.active().deformation;d.pose.clear();d.morph_weights.assign(d.morphs->targets.size(),0);scene.set_deformation(scene.active().id,std::move(d));
 for(auto id:{source,scene.active().id}){auto &object=scene.object(id);object.material.unlit=false;object.material.alpha=Material::AlphaMode::Opaque;object.material.color={.72f,.83f,.61f,.7f};}
 auto &high=scene.object(source);auto m=high.painted->mesh();for(size_t v=0;v<m.positions.size();++v){auto p=m.positions[v];m.normals[v]=normalized(m.normals[v]+Vec3{.19f*p.y,.23f*p.x,.11f*p.z});}for(auto &p:m.positions)p.x=(p.x-.2f)*.5f;high.painted=std::make_shared<PaintedMesh>(m,64);
 Document doc;doc.replace(scene);settled(doc);ObjectBakeSettings settings;settings.prepare_colors=settings.prepare_normals=true;settings.ao_samples=0;settings.allow_partial=true;doc.bake_from_object(source,settings);settled(doc);scene=doc.copy_scene();scene.active().transform.scale=2.5f;return scene;
}

void seed(const std::filesystem::path&p){std::ofstream f(p,std::ios::binary);f<<"Unchanged native physical output";}
bool refusal(auto fn,std::string_view expected){try{fn();}catch(const std::exception&e){std::cout<<"EXPECTED "<<e.what()<<std::endl;return std::string_view(e.what()).find(expected)!=std::string_view::npos;}return false;}
bool cancelled(std::future<void> work){if(work.wait_for(std::chrono::seconds(120))!=std::future_status::ready)throw std::runtime_error("Native physical Cancel observer timed out");return refusal([&]{work.get();},"cancelled");}
NativeAuthoringExport owner(const Scene&s){NativeAuthoringExport out;out.scene=std::make_shared<const Scene>(s.snapshot());out.allow_portable_fallback=true;return out;}
}
#ifndef DOME_NATIVE_PHYSICAL_CONTRACTS_NO_MAIN
int main(int argc,char**argv)try{
 if(argc<3||argc>4)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 auto scene=std::string_view(argv[2])=="--retained"?joint::transfer(prepared64()):
    std::string_view(argv[2])=="--plain"?plain():Scene::load(argv[2]);
 if(std::string_view(argv[2])=="--plain"||std::string_view(argv[2])=="--retained")ink(scene);
 const bool sdf=argc==4&&std::string_view(argv[3])=="--sdf";
 if(sdf){
  scene.active().painted=std::make_shared<PaintedMesh>(quad(),64);scene.active().mesh_active=false;Brush sphere;sphere.radius=.3f;sphere.blend=0;scene.active().volume->begin_stroke();scene.active().volume->stamp(sphere);check(scene.active().volume->end_stroke(),"Native geometry quota uses a genuinely extracted SDF surface");
 }
 const auto exact=scene.encode();auto captured=owner(scene);SourceUvBakeOptions policy;policy.resolution=64;
 check(scene.active().painted->has_physical_emission()||(!scene.active().mesh_active&&scene.active().material.emissive[0]>1),"Contracts use actual native float Paint or constant native SDF radiance, not a relabeled Source4 field");
 check(!scene.active().painted->source_sampling_prepared(),"New native contracts have no Source4 sampling authority");
 set_paint_composite_cache_budget(0);
 if(argc==4&&std::string_view(argv[3])=="--oom"){
  const auto path=out/"oom.glb";seed(path);const auto prior=bytes(path);size_t baseline=0;
  {allocation_probe::Guard guard(SIZE_MAX);export_glb_authoring(captured,out/"baseline.glb",{},policy);baseline=allocation_probe::calls;}
  std::cout<<"NATIVE_ALLOCATION_BASELINE "<<baseline<<std::endl;
  size_t denied=0;bool success=false;
  for(size_t prefix=0;prefix<=baseline+2048;++prefix){bool failed=false;
   {allocation_probe::Guard guard(prefix);try{export_glb_authoring(captured,path,{},policy);}catch(const std::bad_alloc&){failed=true;}}
   if(failed){if(bytes(path)!=prior||scene.encode()!=exact){std::cerr<<"NATIVE_FAILED_PREFIX "<<prefix<<" FILE_CHANGED "<<(bytes(path)!=prior)<<" OWNER_CHANGED "<<(scene.encode()!=exact)<<std::endl;throw std::runtime_error("Native persistent allocation failure changed original file or owner");}++denied;}
   else{check(extract_native_authoring(bytes(path))->encode()==exact,"Allocation sweep eventually produces the complete exact new native project");success=true;break;}
   if(prefix%500==0)std::cout<<"NATIVE_PREFIX "<<prefix<<std::endl;
  }
  check(success&&denied>0,"New native capture/compiler/float/domain/native-payload persistent allocation prefixes rollback and retry");
  std::cout<<"NATIVE_PERSISTENT_CPP_PREFIXES "<<denied<<" baseline="<<baseline<<std::endl;return 0;
 }
 const auto path=out/"direct.glb";export_glb_authoring(captured,path,{},policy);const auto original=bytes(path);
 for(int n=0;n<6;++n){if(sdf&&n>0&&n<4)continue;auto limit=policy;if(n==0)limit.max_working_bytes=4;if(n==1)limit.max_output_bytes=4;if(n==2)limit.max_sample_operations=4;if(n==3)limit.max_raster_samples=4;if(n==4)limit.max_vertices=1;if(n==5)limit.max_triangles=1;
  const auto denied=refusal([&]{export_glb_authoring(captured,path,{},limit);},"budget");std::cout<<"NATIVE_QUOTA "<<n<<" denied="<<denied<<std::endl;
  check(denied&&bytes(path)==original&&scene.encode()==exact,"Native geometry/image/work/raster quota refusal preserves prior output and exact inputs");}
 const auto bounded_mesh=detail::NativeAuthoringCapture::create(captured,{})->originals().front().mesh;
 auto boundary=policy;boundary.max_vertices=bounded_mesh.positions.size();boundary.max_triangles=bounded_mesh.indices.size()/3;
 export_glb_authoring(captured,path,{},boundary);
 check(bytes(path)==original,"Exact native geometry limits preserve successful output bytes");
 for(bool triangles:{false,true}){auto below=boundary;if(triangles)--below.max_triangles;else --below.max_vertices;
  check(refusal([&]{export_glb_authoring(captured,path,{},below);},"budget")&&bytes(path)==original,"One below actual geometry quota refuses without publication");}
 size_t polls=0;GltfExportOptions counted;counted.cancelled=[&]{++polls;return false;};export_glb_authoring(captured,path,counted,policy);
 for(size_t at:{size_t(1),polls/4,polls/2,polls*3/4,polls-4}){size_t calls=0;auto c=counted;c.cancelled=[&]{return ++calls>=at;};check(refusal([&]{export_glb_authoring(captured,path,c,policy);},"cancelled")&&bytes(path)==original&&scene.encode()==exact,"New native capture/compile/encode cancellation leaves exact data and old file unchanged");}
 Document d;d.replace(scene.snapshot());d.shading(false);settled(d);const auto running=out/"running.glb",queued=out/"queued.glb",saved=out/"ordered.dsculpt";seed(running);seed(queued);const auto prior=bytes(running);
 {durable_gate::Guard guard;durable_gate::arm();auto a=d.export_asset(running,AssetExportFormat::EditableGlb256);check(durable_gate::wait(),"Actual native file worker reaches durable temporary output before commit");auto b=d.export_asset(queued,AssetExportFormat::EditableGlb256);auto save=d.save_async(saved);check(d.frame().state->pending_exports==2&&d.frame().state->can_cancel_asset,"Running and queued new native exports expose shared Cancel");d.cancel_background_work();durable_gate::release();check(cancelled(std::move(a))&&cancelled(std::move(b)),"Actual new native running and queued exports both cancel");save.get();check(bytes(running)==prior&&bytes(queued)==prior&&Scene::load(saved).encode()==exact&&d.copy_scene().encode()==exact,"Native Cancel preserves both files, exact live project and following ordered Save");}
 d.export_asset(running,AssetExportFormat::EditableGlb256).get();check(Scene::load(running).encode()==exact&&!d.frame().state->pending_exports,"New native export retry succeeds and retires file status");
 check(scene.encode()==exact,"All new native contracts leave original owning scene unchanged");std::cout<<"PASS "<<checks<<" new native physical contracts\n";return 0;
}catch(const std::exception&e){durable_gate::release();std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}

#endif
