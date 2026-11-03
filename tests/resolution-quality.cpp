// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/source-maps.hpp"
#ifdef DOME_TEST_SYCL
#include <sycl/sycl.hpp>
#endif
#include "sparse-resample-backend.hpp"
#include <nlohmann/json.hpp>
#include <bit>
#include <condition_variable>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
unsigned checks=0;std::atomic_uint calls=0;std::atomic_bool fail=false,gate=false;
std::mutex mutex;std::condition_variable changed;bool entered=false,released=false;
void check(bool b,const char*m){++checks;if(!b)throw std::runtime_error(m);std::cout<<"PASS "<<m<<'\n';}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception&e){std::cout<<"REFUSAL "<<e.what()<<'\n';return true;}}
void arm(){std::lock_guard l(mutex);entered=released=false;gate=true;}
void await(){std::unique_lock l(mutex);if(!changed.wait_for(l,std::chrono::seconds(30),[]{return entered;}))throw std::runtime_error("Actual aggregate GPU quality boundary did not enter");}
void release(){std::lock_guard l(mutex);released=true;changed.notify_all();}
struct Release{~Release(){release();}};
nlohmann::json ordered(const Volume &v){auto j=v.encode();std::sort(j["samples"].begin(),j["samples"].end());for(auto &l:j["sculpt_layers"])std::sort(l["delta"].begin(),l["delta"].end());if(j.contains("sculpt_mask"))std::sort(j["sculpt_mask"]["values"].begin(),j["sculpt_mask"]["values"].end());return j;}
Volume fixture(int size=33){
 Volume v(.02f);auto id=v.add_layer("Hidden independent 日本語");v.layer_properties(id,"Hidden independent 日本語",false,true,.37f);v.fill_mask(.37f);
 auto j=v.encode();auto &delta=j["sculpt_layers"][1]["delta"];auto &mask=j["sculpt_mask"]["values"];
 for(int z=0;z<size;++z)for(int y=0;y<size;++y)for(int x=0;x<size;++x){const int a=x-size/2,b=y-size/2,c=z-size/2;delta.push_back({a,b,c,.001*(.5+std::sin(a*.13)+b*.01-c*.02)});mask.push_back({a,b,c,float(.5+.02*std::sin(a*.13)+b*.001)});}
 return Volume::decode(j);
}
SculptResolutionOptions options(){SculptResolutionOptions o;o.voxel_size=.03f;o.maximum_layer_error=1;o.maximum_mask_error=1;return o;}
template<class Map>double native(const Map&map,Int3 q,float old,float next,double background){
 const double p[]={double(q.x)*old/next,double(q.y)*old/next,double(q.z)*old/next};Int3 lo{int(std::floor(p[0])),int(std::floor(p[1])),int(std::floor(p[2]))};double f[]={p[0]-lo.x,p[1]-lo.y,p[2]-lo.z},sum=background;
 for(int z=0;z<2;++z)for(int y=0;y<2;++y)for(int x=0;x<2;++x){auto i=map.find(lo+Int3{x,y,z});if(i!=map.end())sum+=(double(i->second)-background)*(x?f[0]:1-f[0])*(y?f[1]:1-f[1])*(z?f[2]:1-f[2]);}return sum;
}
Scene scene(const Volume&v){
 Scene s;s.active().volume=std::make_shared<Volume>(v.geometry_snapshot());
 Mesh m;m.positions={{0,0,0},{1,0,0},{1,1,0},{0,1,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};
 auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=std::make_shared<const Image>(Image{2,1,{255,30,20,255,10,200,255,127}});
 auto paint=PaintedMesh::from_maps(m,maps,64);paint->add_layer("Retained paint 日本語");paint->layer_properties("Retained paint 日本語",true,.73f);s.active().painted=paint;
 auto other=s.add("Independent other",Volume(.07f));s.select(1);(void)other;return s;
}
void ready(Document&d){if(!d.wait_mesh())throw std::runtime_error("Current immutable mesh failed");}
}
// Link-only fixture seam around the ACTUAL production aggregate call. Numerical
// work remains real selected-backend work; failures/gates occur after readback.
extern "C" uint64_t quality_real(SparseFieldSampler*,std::span<const sparse_resample::ErrorQuery>,const std::function<bool()>&) asm("__real__ZN4dome18SparseFieldSampler13maximum_errorESt4spanIKNS_15sparse_resample10ErrorQueryELm18446744073709551615EERKSt8functionIFbvEE");
extern "C" uint64_t quality_wrap(SparseFieldSampler*s,std::span<const sparse_resample::ErrorQuery>q,const std::function<bool()>&cancelled) asm("__wrap__ZN4dome18SparseFieldSampler13maximum_errorESt4spanIKNS_15sparse_resample10ErrorQueryELm18446744073709551615EERKSt8functionIFbvEE");
extern "C" uint64_t quality_wrap(SparseFieldSampler*s,std::span<const sparse_resample::ErrorQuery>q,const std::function<bool()>&cancelled){
 ++calls;auto value=quality_real(s,q,cancelled);
 if(gate.exchange(false)){std::unique_lock l(mutex);entered=true;changed.notify_all();if(!changed.wait_for(l,std::chrono::seconds(30),[]{return released;}))throw std::runtime_error("Quality gate timed out");}
 if(fail.load())throw std::runtime_error("Injected post-aggregate quality failure");return value;
}
int main(int argc,char**argv)try{
 auto source=fixture();const auto before=ordered(source);auto owner=source.geometry_owner();auto o=options();
 if(argc>1&&std::string_view(argv[1])=="--missing"){check(refuses([&]{resample_sculpt(source,o);})&&ordered(source)==before&&source.geometry_owner()==owner,"Missing selected backend preserves complete original Volume; no CPU fallback");return 0;}
#ifdef DOME_TEST_SYCL
 sycl::device device{sycl::gpu_selector_v};const auto name=device.get_info<sycl::info::device::name>();const auto vendor=device.get_info<sycl::info::device::vendor>();const auto pci=device.get_info<sycl::ext::intel::info::device::pci_address>();
 std::cout<<"ACTUAL_GPU "<<name<<" vendor="<<vendor<<" pci="<<pci<<'\n';
 if(std::getenv("DOME_REQUIRE_ARC_PCI03"))check(name.find("Arc")!=std::string::npos&&name.find("A770")!=std::string::npos&&vendor.find("Intel")!=std::string::npos&&pci=="0000:03:00.0","Actual selected numerical backend is strict PCI03 Intel ArcA770");
#endif
 const bool gpu=compute_backend()!="CPU";float last=-1;o.progress=[&](float p){if(p<last||p>=1)throw std::runtime_error("Nonmonotone worker quality progress");last=p;};
 auto result=resample_sculpt(source,o);check(result.replacement&&last==.98f,"Actual hidden-layer and sparse-mask coarsening completes with monotone bounded progress");
 check(gpu?calls.load()==2:calls.load()==0,"Measured tier dispatches layer+mask aggregation only on selected GPU");
 double layer=0,mask=0;const auto &target=*result.replacement;
 for(auto [q,v]:source.layers()[1].delta)layer=std::max(layer,std::abs(native(target.layers()[1].delta,q,.02f,.03f,0)-v));
 for(auto [q,v]:source.mask_values())mask=std::max(mask,std::abs(native(target.mask_values(),q,.02f,.03f,source.mask_default())-double(v)));
 check(result.report.maximum_layer_error==float(layer)&&result.report.maximum_mask_error==float(mask),"Actual Volume reports exactly match independent native-double reconstruction maxima");
 check(ordered(source)==before&&source.geometry_owner()==owner&&target.layers()[1].id==source.layers()[1].id&&target.layers()[1].name==source.layers()[1].name&&!target.layers()[1].visible&&target.layers()[1].locked&&target.layers()[1].opacity==.37f&&target.mask_default()==.37f,"Quality preserves original owners and independent hidden layer/lock/strength/mask settings");
 o.progress={};o.maximum_layer_error=float(layer*.5);check(refuses([&]{resample_sculpt(source,o);})&&ordered(source)==before,"Independent raw layer error bound is enforced before replacement");
 o=options();o.maximum_mask_error=float(mask*.5);check(refuses([&]{resample_sculpt(source,o);})&&ordered(source)==before,"Independent float-mask reconstruction bound is enforced before replacement");
 o=options();o.maximum_work=result.report.work-1;check(refuses([&]{resample_sculpt(source,o);})&&ordered(source)==before,"Aggregate packing and query work is charged to actual bounded work budget");
 calls=0;auto small=fixture(4);auto small_result=resample_sculpt(small,options());check(small_result.replacement&&calls==0,"Small coarsening deliberately uses native CPU quality even in GPU build");
 // Refinement stays a deliberate CPU quality tier; GPU forward remains selected.
 calls=0;o=options();o.voxel_size=.01f;auto fine=resample_sculpt(small,o);check(fine.replacement&&calls==0,"Refinement deliberately uses native CPU quality without changing selected forward backend");
 calls=0;Volume::Voxels samples;
 for(int z=-21;z<=21;++z)for(int y=-21;y<=21;++y)for(int x=-21;x<=21;++x){float d=length(Vec3{float(x),float(y),float(z)}*.01f)-.167f;if(d<.04f)samples.emplace(Int3{x,y,z},std::max(d,-.04f));}
 auto closed=Volume::from_samples(.01f,std::move(samples));o={};o.voxel_size=.015f;
 auto closed_result=resample_sculpt(closed,o);
 check(closed.layers()[0].delta.size()>=32768&&closed_result.replacement&&closed_result.report.source.closed_manifold()&&closed_result.report.target.closed_manifold()&&closed_result.report.source_euler==closed_result.report.target_euler&&closed_result.report.minimum_normal_dot>=.5f&&(gpu?calls.load()>0:calls.load()==0),"Eligible visible closed Volume retains certified topology/distance/normals alongside aggregate reconstruction");
 o=options();
 if(!gpu){std::cout<<"PASS "<<checks<<" native CPU quality integration checks\n";return 0;}
 fail=true;o=options();check(refuses([&]{resample_sculpt(source,o);})&&ordered(source)==before,"Real aggregate post-readback failure preserves full detached source and never retries on CPU");fail=false;
 auto path=std::filesystem::temp_directory_path()/("dome-quality-"+std::to_string(getpid())+".dsculpt");
 Document d;d.replace(scene(source));ready(d);const auto original=d.copy_scene().encode();const auto paint_owner=d.copy_scene().active().painted;const auto other_owner=d.copy_scene().object(2).volume->geometry_owner();
 Release cleanup;arm();d.change_sculpt_resolution(o);await();check(d.frame().state->asset_busy&&d.frame().state->can_save_after_asset,"Accepted resolution endpoint permits ordered Save during real quality work");auto saved=d.save_async(path);release();saved.get();check(d.wait_asset(),"Accepted aggregate operation and dependent Save drain");ready(d);
 auto after=ordered(d.copy_volume());check(d.copy_scene().active().painted->mesh_owner()==paint_owner->mesh_owner()&&d.copy_scene().active().painted->source_maps()==paint_owner->source_maps()&&d.copy_scene().active().painted->encode()==paint_owner->encode()&&d.copy_scene().object(2).volume->geometry_owner()==other_owner,"Actual worker retains nested dormant paint geometry/image owners, exact paint/layers and independent other geometry");check(after==ordered(target)&&ordered(*Scene::load(path).active().volume)==after,"Queued Save captures exact accepted quality result and survives native restart");check(d.undo()&&d.copy_scene().encode()==original&&!d.frame().state->can_undo,"One Undo restores complete original scene and no extra history");check(d.redo()&&ordered(d.copy_volume())==after,"Redo restores exact detached quality result");
 d.undo();ready(d);arm();d.change_sculpt_resolution(o);await();d.cancel_asset_work();release();check(d.wait_asset()&&d.copy_scene().encode()==original&&!d.frame().state->can_undo,"Cancellation after actual aggregate readback publishes no scene/history");
 arm();d.change_sculpt_resolution(o);await();d.replace(Volume(.09f));release();check(d.wait_asset()&&d.copy_volume().voxel_size()==.09f&&!d.frame().state->can_undo,"Stale generation after actual aggregate cannot overwrite newer document");
 d.replace(scene(source));ready(d);fail=true;arm();d.change_sculpt_resolution(o);await();auto failed_save=d.save_async(path);release();check(refuses([&]{failed_save.get();})&&d.wait_asset()&&!d.frame().state->error.empty()&&d.copy_scene().encode()==original&&!d.frame().state->can_undo&&ordered(*Scene::load(path).active().volume)==after,"Post-readback worker failure rejects dependent Save, preserving prior file/scene/paint/source/history");fail=false;
 d.change_sculpt_resolution(o);check(d.wait_asset()&&ordered(d.copy_volume())==after,"Same-source retry succeeds after injected detached quality failure");
 auto closing=std::make_unique<Document>();closing->replace(scene(source));ready(*closing);arm();closing->change_sculpt_resolution(o);await();auto final_save=closing->save_async(path);
 {std::jthread teardown([&]{closing.reset();});release();final_save.get();}
 check(ordered(*Scene::load(path).active().volume)==after,"Final shutdown drains accepted aggregate result and dependent Save without a stale-source snapshot");std::filesystem::remove(path);
 std::cout<<"PASS "<<checks<<" actual Volume/worker aggregate checks on "<<compute_backend()<<'\n';return 0;
}catch(const std::exception&e){release();std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
