// SPDX-License-Identifier: GPL-3.0-or-later
#include "cut-color-fixture.hpp"
#include "dome/document.hpp"
#include "document-authority.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
using namespace dome;
namespace {
std::atomic_bool enabled{false},hit{false};std::atomic_size_t calls{0},boundary{SIZE_MAX};
thread_local bool tracking=false;
std::atomic<Document*> current_document{nullptr};
CutPublishReport prepare_or_replay(Scene&,const CutPreview&,const BakeSource&,uint32_t,const std::function<bool()>&,const std::function<void(float)>&);
void cache_unchanged();
struct Gate{std::mutex mutex;std::condition_variable changed;bool armed=false,entered=false;void arm(){std::lock_guard lock(mutex);armed=true;entered=false;}void stop(){std::unique_lock lock(mutex);if(!armed)return;entered=true;changed.notify_all();changed.wait(lock,[&]{return !armed;});}void wait(){std::unique_lock lock(mutex);if(!changed.wait_for(lock,std::chrono::seconds(10),[&]{return entered;}))throw std::runtime_error("Actual cut helper gate was not entered");}void release(){std::lock_guard lock(mutex);armed=false;changed.notify_all();}} gate;
struct Release{~Release(){gate.release();}};
void require(bool yes,const char *why){if(!yes)throw std::runtime_error(why);}
}
[[gnu::noinline]] void *operator new(size_t n){if(tracking&&enabled&&++calls>=boundary.load()){hit=true;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](size_t n){return ::operator new(n);}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p,size_t)noexcept{std::free(p);}
extern "C" CutPublishReport cut_real(Scene&,const CutPreview&,const BakeSource&,uint32_t,const std::function<bool()>&,const std::function<void(float)>&) asm("__real__ZN4dome11publish_cutERNS_5SceneERKNS_10CutPreviewERKNS_10BakeSourceEjRKSt8functionIFbvEERKS8_IFvfEE");
extern "C" CutPublishReport cut_wrap(Scene&,const CutPreview&,const BakeSource&,uint32_t,const std::function<bool()>&,const std::function<void(float)>&) asm("__wrap__ZN4dome11publish_cutERNS_5SceneERKNS_10CutPreviewERKNS_10BakeSourceEjRKSt8functionIFbvEERKS8_IFvfEE");
extern "C" CutPublishReport cut_wrap(Scene &s,const CutPreview &p,const BakeSource &source,uint32_t resolution,const std::function<bool()> &cancelled,const std::function<void(float)> &progress){tracking=false;gate.stop();auto result=prepare_or_replay(s,p,source,resolution,cancelled,progress);tracking=enabled.load();return result;}
extern "C" void commit_real(void*,bool) asm("__real__ZN4dome8Document11Transaction6commitEb");
extern "C" void commit_wrap(void*,bool) asm("__wrap__ZN4dome8Document11Transaction6commitEb");
extern "C" void commit_wrap(void *self,bool checkpoint){tracking=false;commit_real(self,checkpoint);}
extern "C" void error_real(Document*,void*,std::exception_ptr) asm("__real__ZN4dome8Document24file_worker_error_lockedERNS0_7FileJobENSt15__exception_ptr13exception_ptrE");
extern "C" void error_wrap(Document*,void*,std::exception_ptr) asm("__wrap__ZN4dome8Document24file_worker_error_lockedERNS0_7FileJobENSt15__exception_ptr13exception_ptrE");
extern "C" void error_wrap(Document *d,void *job,std::exception_ptr error){error_real(d,job,std::move(error));tracking=false;}
namespace dome {struct DocumentTransactionTestAccess {
 static auto tokens(const Document &d){std::lock_guard lock(d.model_mutex_);return std::tuple{d.content_,d.next_content_,d.saved_content_,d.undo_.size(),d.redo_.size(),d.state_.revision,d.state_.generation,d.recovery_content_};}
 static auto source(const Document &d){std::lock_guard lock(d.model_mutex_);return d.native_source_scene_;}
 static auto lease(const Document &d){std::lock_guard lock(d.model_mutex_);return std::tuple{d.cut_source_.generation,d.cut_source_.revision,d.cut_source_.object,d.cut_source_.edit_authority,d.cut_base_,d.cut_bake_source_};}
 static auto work(const Document &d){return d.asset_work_.load();}
 static auto admitted(const Document &d){std::lock_guard lock(d.model_mutex_);return std::tuple{d.cut_source_.generation,d.cut_source_.revision,d.cut_source_.object,d.cut_source_.edit_authority};}
};}
namespace {
struct Prepared {
 std::shared_ptr<const Scene> input,output,output_witness;
 nlohmann::json input_value,output_value;
 BakeSource display;CutPreview preview;uint32_t resolution=0;CutPublishReport report;
 std::tuple<uint64_t,uint64_t,uint64_t,uint64_t> admitted;
} prepared;
bool same_vec(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
void same_mesh(const Mesh&a,const Mesh&b){require(a.indices==b.indices&&a.uv==b.uv&&a.uv_pins==b.uv_pins&&a.uv_seams==b.uv_seams&&a.positions.size()==b.positions.size()&&a.normals.size()==b.normals.size(),"Replay requires exact ordered geometry/UV identities");for(size_t i=0;i<a.positions.size();++i)require(same_vec(a.positions[i],b.positions[i])&&same_vec(a.normals[i],b.normals[i]),"Replay requires exact ordered source positions/normals");}
void same_map(const TextureMaps&a,const TextureMaps&b){
 require(a.source_maps==b.source_maps&&a.baked_base==b.baked_base&&a.color_projection==b.color_projection&&a.normal_projection==b.normal_projection&&a.source_only==b.source_only&&a.transparent==b.transparent&&a.pbr_transparent==b.pbr_transparent,"Replay preserves exact source/normal/color/map owners and interpretation");
 for(auto pair:{std::pair{&a.color,&b.color},std::pair{&a.metal_rough,&b.metal_rough},std::pair{&a.emission,&b.emission},std::pair{&a.normal,&b.normal},std::pair{&a.pbr_color,&b.pbr_color},std::pair{&a.baked_normal,&b.baked_normal},std::pair{&a.baked_coverage,&b.baked_coverage},std::pair{&a.color_coverage,&b.color_coverage},std::pair{&a.emission_coverage,&b.emission_coverage},std::pair{&a.mtoon_normal,&b.mtoon_normal}})require(pair.first->width==pair.second->width&&pair.first->height==pair.second->height&&pair.first->rgba==pair.second->rgba,"Replay requires complete exact composited map bytes");
}
void cache_unchanged(){
 require(prepared.output&&prepared.output_witness&&prepared.input->encode()==prepared.input_value,"Cached original complete source remains immutable");
 detail::require_isolated_scene(*prepared.output_witness,*prepared.output,0);
 require(prepared.output_witness->active().id==prepared.output->active().id,"Cached active object remains immutable");
 for(size_t i=0;i<prepared.output->objects().size();++i){const auto&a=prepared.output_witness->objects()[i];const auto&b=prepared.output->objects()[i];
  require(a.volume!=b.volume&&a.volume->encode()==b.volume->encode(),"Independent COW volume metadata witness remains exact");
  if(a.painted){require(a.painted!=b.painted&&a.painted->active_layer()==b.painted->active_layer()&&a.painted->resolution()==b.painted->resolution()&&a.painted->baked()==b.painted->baked()&&a.painted->source_maps()==b.painted->source_maps()&&a.painted->color_projection()==b.painted->color_projection()&&a.painted->normal_projection()==b.painted->normal_projection()&&a.painted->layers().size()==b.painted->layers().size(),"Cached mutable paint metadata remains separately owned and exact with pinned const image/geometry owners");
   for(size_t layer=0;layer<a.painted->layers().size();++layer){const auto&x=a.painted->layers()[layer];const auto&y=b.painted->layers()[layer];require(x.id==y.id&&x.name==y.name&&x.visible==y.visible&&x.opacity==y.opacity&&x.tiles==y.tiles&&x.pbr_tiles==y.pbr_tiles,"Cached paint layers and every immutable common/PBR tile owner remain exact");}
  }
 }
}
CutPublishReport prepare_or_replay(Scene &s,const CutPreview &p,const BakeSource &source,uint32_t resolution,const std::function<bool()> &cancelled,const std::function<void(float)> &progress){
 auto d=current_document.load();require(d,"Actual native cut Document owner exists");
 if(!prepared.output){
  prepared.input=std::make_shared<const Scene>(s.snapshot());prepared.input_value=s.encode();prepared.display=source;prepared.preview=p;prepared.resolution=resolution;prepared.admitted=DocumentTransactionTestAccess::admitted(*d);
  prepared.report=cut_real(s,p,source,resolution,cancelled,progress);
  prepared.output=std::make_shared<const Scene>(s.snapshot());prepared.output_witness=std::make_shared<const Scene>(prepared.output->snapshot());prepared.output_value=s.encode();
 }else{
  require(s.encode()==prepared.input_value&&DocumentTransactionTestAccess::admitted(*d)==prepared.admitted,"Replay requires complete encoded input and exact admitted generation/revision/object/authority IDs");
  detail::require_isolated_scene(*prepared.input,s,0); // Every object/group is protected.
  for(size_t i=0;i<s.objects().size();++i){const auto&a=*prepared.input->objects()[i].painted;const auto&b=*s.objects()[i].painted;require(a.baked()==b.baked()&&a.source_maps()==b.source_maps()&&a.color_projection()==b.color_projection()&&a.normal_projection()==b.normal_projection()&&a.layers().size()==b.layers().size(),"Replay preserves all immutable paint/source/projection owners");for(size_t l=0;l<a.layers().size();++l)require(a.layers()[l].tiles==b.layers()[l].tiles&&a.layers()[l].pbr_tiles==b.layers()[l].pbr_tiles,"Replay preserves every common/PBR paint tile owner");}
  require(resolution==prepared.resolution&&source.colors==prepared.display.colors&&source.tangents==prepared.display.tangents&&source.triangle_materials==prepared.display.triangle_materials&&source.materials.size()==prepared.display.materials.size(),"Replay requires complete source RGBA/tangent/material routing");same_mesh(source.mesh,prepared.display.mesh);
  for(size_t i=0;i<source.materials.size();++i){const auto&a=source.materials[i];const auto&b=prepared.display.materials[i];require(detail::same_material(a.material,b.material)&&a.normal_map==b.normal_map&&a.normal_strength==b.normal_strength&&a.repeat_uv==b.repeat_uv&&a.maps_are_composited==b.maps_are_composited&&bool(a.maps)==bool(b.maps),"Replay preserves actual complete source material authority");if(a.maps)same_map(*a.maps,*b.maps);}
  require(p.changed==prepared.preview.changed&&p.settings.yaw==prepared.preview.settings.yaw&&p.settings.pitch==prepared.preview.settings.pitch&&p.settings.offset_mm==prepared.preview.settings.offset_mm&&p.settings.cap==prepared.preview.settings.cap&&p.settings.flipped==prepared.preview.settings.flipped&&detail::same_material(p.settings.cap_material,prepared.preview.settings.cap_material)&&same_vec(p.plane.normal,prepared.preview.plane.normal)&&p.plane.offset==prepared.preview.plane.offset,"Replay requires exact actual plane/cap settings");same_mesh(p.cap_mesh,prepared.preview.cap_mesh);require(p.result&&prepared.preview.result&&p.result->source_triangles==prepared.preview.result->source_triangles&&p.result->source_corner_barycentrics==prepared.preview.result->source_corner_barycentrics,"Replay preserves actual clipping witnesses");same_mesh(p.result->mesh,prepared.preview.result->mesh);
  cache_unchanged();s=prepared.output->snapshot();for(size_t i=0;i<s.objects().size();++i)require(s.objects()[i].volume!=prepared.output->objects()[i].volume&&(!s.objects()[i].painted||s.objects()[i].painted!=prepared.output->objects()[i].painted),"Replay recipient cannot mutate cached Volume/Paint metadata");
 }
 return prepared.report;
}
std::unique_ptr<Document> setup(){auto d=std::make_unique<Document>();static const Scene base=[] {auto scene=cut_color_fixture::scene(true,true);cut_color_fixture::pose(scene);return scene;}();d->replace(base.snapshot());current_document=d.get();for(auto name:{"Retained artist A","Retained artist B"}){auto o=d->copy_scene().active();d->object_properties(o.id,name,o.visible,o.locked,o.transform,o.material);}require(d->undo(),"Create pre-existing redo");require(d->wait_mesh(),"Source with retained history packs");d->begin_cut();require(d->wait_cut()&&d->frame().state->error.empty(),"Actual posed source/preview ready");return d;}
std::future<void> accept_save(Document &d,const std::filesystem::path &path){gate.arm();d.end_cut(true);gate.wait();return d.save_async(path);}
}
int main(int argc,char **argv)try{
 require(argc==2||argc==4||argc==5,"Choose publication fixture directory and optional exact fault range/shard");std::filesystem::path directory=argv[1];std::filesystem::create_directories(directory);set_paint_composite_cache_budget(0);
 auto measured=setup();Release cleanup;auto file=directory/"measured.dsculpt";auto saving=accept_save(*measured,file);calls=0;boundary=SIZE_MAX;hit=false;enabled=true;gate.release();saving.get();require(measured->wait_asset()&&measured->frame().state->error.empty(),"Measured publication succeeds");enabled=false;auto count=calls.load();require(count>100,"Real pre-swap/authority/publication allocation boundaries observed");auto expected=measured->copy_scene().encode();cache_unchanged();measured.reset();std::cout<<count<<" actual cut publication boundaries after witnessed immutable preparation replay\n";
 size_t first=1,last=count;
 if(argc==4){first=std::stoull(argv[2]);last=std::min(count,size_t(std::stoull(argv[3])));}
 if(argc==5){require(std::string_view(argv[2])=="--shard","Choose explicit shard mode");size_t shard=std::stoull(argv[3]),total=std::stoull(argv[4]);require(total&&total<=256&&shard<total&&count<=SIZE_MAX/total,"Bounded publication shard index");first=count*shard/total+1;last=count*(shard+1)/total;}
 require(first&&first<=last&&last<=count,"Valid actual publication allocation range");
 for(size_t at=first;at<=last;++at){
  auto d=setup();Release release;auto original=d->copy_scene().encode();auto path=directory/"retained.dsculpt";d->copy_scene().save(path);auto tokens=DocumentTransactionTestAccess::tokens(*d);auto source=DocumentTransactionTestAccess::source(*d);auto mesh=d->frame().surface;auto preview=d->frame().state->cut->preview;auto lease=DocumentTransactionTestAccess::lease(*d);
  auto future=accept_save(*d,path);auto work=DocumentTransactionTestAccess::work(*d);calls=0;boundary=at;hit=false;enabled=true;gate.release();bool failed=false;try{future.get();}catch(const std::bad_alloc&){failed=true;}enabled=false;
  require(d->wait_asset(),"Failed cut/file worker drains");cache_unchanged();require(hit&&failed,"Each real pre-commit allocation fails and dependent Save preserves the original exception");
  require(d->copy_scene().encode()==original&&Scene::load(path).encode()==original,"Failed publication preserves complete source Scene and old Save bytes");
  require(DocumentTransactionTestAccess::tokens(*d)==tokens&&DocumentTransactionTestAccess::source(*d)==source&&d->frame().surface==mesh,"Failed publication retains undo/redo/content/nextContent/revision/generation/cache/mesh authority");
  require(d->frame().state->cut&&d->frame().state->cut->preview==preview&&!d->frame().state->cut->committing&&DocumentTransactionTestAccess::lease(*d)==lease,"Failed cut retains exact original draft/preview/source lease for retry");
  require(work->cut_done&&work->cut_failure&&!work->cut_completed,"Accepted snapshot failure bookkeeping remains exact under persistent allocation denial");
  d->end_cut(true);require(d->wait_asset()&&d->frame().state->error.empty()&&!d->frame().state->cut&&d->copy_scene().encode()==expected,"Actual failed cut retries its exact prepared result");
  require(d->undo()&&d->copy_scene().encode()==original&&d->redo()&&d->copy_scene().encode()==expected,"Retried cut has exactly one undoable source-retaining history transition");cache_unchanged();
  if((at&63)==0)std::cout<<"Reached publication allocation "<<at<<std::endl;
 }
 require(prepared.output->encode()==prepared.output_value,"Final full encoded prepared snapshot remains byte-for-byte exact");
 std::cout<<"PASS publication allocation range "<<first<<".."<<last<<" of "<<count<<" actual cut pre-commit allocations preserve source/history/leases, original exception, old output and retry\n";return 0;
}catch(const std::exception &e){enabled=false;gate.release();std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
