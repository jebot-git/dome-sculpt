// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/color-preparation.hpp"
#include "dome/document.hpp"
#include "paint-composite-backend.hpp"
#include "current-paint-preview.hpp"
#include "dome/paint-sampling.hpp"
#include "paint-sampling-access.hpp"
#include "source-color-fixtures.hpp"
#include <numeric>
#include <nlohmann/json.hpp>
#include <atomic>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
thread_local bool reference_mode=false;std::atomic_uint calls=0,prepared_calls=0;std::atomic_int fault=0;size_t checks=0;
void check(bool b,const char*m){++checks;if(!b)throw std::runtime_error(m);std::cout<<"PASS "<<m<<'\n';}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception&e){std::cout<<"REFUSAL "<<e.what()<<'\n';return true;}}
bool equal(const TextureMaps&a,const TextureMaps&b){return a.color.rgba==b.color.rgba&&a.metal_rough.rgba==b.metal_rough.rgba&&a.emission.rgba==b.emission.rgba&&a.color_coverage.rgba==b.color_coverage.rgba&&a.color_coverage.width==b.color_coverage.width&&a.emission_coverage.rgba==b.emission_coverage.rgba&&a.emission_coverage.width==b.emission_coverage.width&&a.transparent==b.transparent&&a.pbr_transparent==b.pbr_transparent&&a.normal.rgba==b.normal.rgba&&a.pbr_color.rgba==b.pbr_color.rgba&&a.baked_normal.rgba==b.baked_normal.rgba&&a.baked_coverage.rgba==b.baked_coverage.rgba;}
std::shared_ptr<PaintedMesh> clone(const nlohmann::json &j){return PaintedMesh::decode(j);}
BakedTextures bake_fixture(uint32_t side){
 BakedTextures bake;
 const size_t count=size_t(side)*side;
 bake.coverage.resize(count);
 for(auto *plane:{&bake.maps.color,&bake.maps.metal_rough,&bake.maps.emission,&bake.maps.normal})
  *plane={side,side,std::vector<uint8_t>(4*count)};
 for(size_t i=0;i<count;++i){
  bake.coverage[i]=i%7==0?255:i%7==1?0:106;
  const std::array<uint8_t,4> rgba{uint8_t(i%251),52,131,uint8_t(i%256)};
  const std::array<uint8_t,4> orm{uint8_t(i%199),47,16,255};
  const std::array<uint8_t,4> emission{91,uint8_t(i%151),27,255};
  const std::array<uint8_t,4> normal{uint8_t(i%256),uint8_t(i/17%256),uint8_t(i/113%256),uint8_t(i%37)};
  std::copy(rgba.begin(),rgba.end(),bake.maps.color.rgba.begin()+4*i);
  std::copy(orm.begin(),orm.end(),bake.maps.metal_rough.rgba.begin()+4*i);
  std::copy(emission.begin(),emission.end(),bake.maps.emission.rgba.begin()+4*i);
  std::copy(normal.begin(),normal.end(),bake.maps.normal.rgba.begin()+4*i);
 }
 return bake;
}
Scene source(){
 Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};auto p=std::make_shared<PaintedMesh>(m,512);
 p->begin_stroke();Brush b;b.from=b.to={.3f,.3f,0};b.radius=.1f;b.pressure=1;b.strength=1;b.normal={0,0,1};PaintSettings settings;settings.color={0,1,0,1};p->stamp(b,settings);if(!p->end_stroke())throw std::runtime_error("Seed paint must actually mutate the surface");
 Scene s;s.active().painted=p;s.active().mesh_active=true;DeformationState def(m);def.skeleton=Skeleton{{{"Root",-1,{},1}}};def.skin=SkinWeights{};def.skin->vertices.assign(4,{{0,1}});def.pose.resize(1);def.pose[0].translation={.2f,.1f,0};def.morphs=MorphSet(m);auto moved=m.positions;for(auto &v:moved)v.x+=.1f;def.morphs->set_target(m,"Moved",moved);def.morph_weights={1};s.set_deformation(1,def);return s;
}
void ready(Document&d){if(!d.wait_mesh())throw std::runtime_error("Mesh did not become ready");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
}
std::string_view real_backend() asm("__real__ZN4dome15compute_backendEv");
std::string_view wrapped_backend() asm("__wrap__ZN4dome15compute_backendEv");
std::string_view wrapped_backend(){if(reference_mode)return "CPU";auto name=real_backend();return name=="CPU"?std::string_view{"TEST-HOST-PAINT"}:name;}
paint_compute::Result real_composite(const paint_compute::Input&,const std::function<bool()>&) asm("__real__ZN4dome13paint_compute9compositeERKNS0_5InputERKSt8functionIFbvEE");
paint_compute::Result wrapped_composite(const paint_compute::Input&,const std::function<bool()>&) asm("__wrap__ZN4dome13paint_compute9compositeERKNS0_5InputERKSt8functionIFbvEE");
paint_compute::Result wrapped_composite(const paint_compute::Input&in,const std::function<bool()>&cancelled){
 ++calls;auto result=real_composite(in,cancelled);const auto f=fault.exchange(0);
 if(!in.projected_color.empty())++prepared_calls;
 if(f==1)throw std::runtime_error("Injected completed-composite failure");
 if(f==2)result.pixels.back().reserved=1;
 if(f==3&&!result.normals.empty())result.normals.pop_back();
 if(f==4&&!result.pbr_colors.empty())result.pbr_colors.pop_back();
 return result;
}
int main()try{
 if(real_backend()=="CPU")std::cout<<"TEST-ONLY HOST-CONTROL-FLOW; native sampler, no GPU/SDK numerical claim\n";
 set_paint_composite_cache_budget(0);auto scene=source();const auto raw=scene.active().painted->encode();Material a,b;b.color={.12f,.37f,.8f,.997f};b.emissive={.07f,.01f,.09f};b.metallic=.71f;b.roughness=.22f;
 reference_mode=true;auto expected=clone(raw)->composite(b);reference_mode=false;
 // A detached valid grid-domain stage can have no source maps/bake/projection
 // yet. Its independent owner alone must keep it off the legacy GPU packet.
 auto domain_source=clone(raw);auto target=domain_source->mesh();for(auto &uv:target.uv){uv[0]=uv[0]*.7f+.1f;uv[1]=uv[1]*.7f+.1f;}
 std::vector<uint32_t> corners(target.indices.size());std::iota(corners.begin(),corners.end(),0u);
 auto domain=retain_paint_sampling(domain_source->mesh(),{},target,corners);
 auto retained=retain_paint_grid(*domain_source,target,domain);const auto domain_start=calls.load();
 check(retained->paint_sampling_domain()&&retained->paint_sampling_attachment()&&retained->valid_paint_sampling_attachment(),"Combined valid independent paint domain owns its exact current geometry before source/material adoption");
 auto domain_maps=retained->composite(b);check(calls==domain_start&&equal(*domain_maps,*expected),"Domain-only paint uses the exact retained CPU grid and never the plain selected-backend packet");
 check(retained->preview(b)==domain_maps&&calls==domain_start,"Domain-only preview reuses its own immutable CPU maps without a backend dispatch");
 auto p=clone(raw);auto base=p->composite(a);const auto pixels=base->color.rgba;auto current=p->composite(b);check(equal(*current,*expected),"Complete producer maps exactly match legacy native CPU for the same immutable paint input");auto n=calls.load();check(p->composite(b)==current&&calls==n,"Unchanged material/token reuses exact immutable maps without another backend call");
 for(int f:{1,2}){
  p=clone(raw);base=p->composite(a);n=calls.load();fault=f;check(refuses([&]{p->composite(b);})&&calls==n+1,"Backend/late image-validation failure does not retry on CPU");n=calls.load();check(p->composite(a)==base&&calls==n&&base->color.rgba==pixels&&p->encode()==raw,"Failure leaves prior cache, held frame bytes and complete source paint exact");check(equal(*p->composite(b),*expected),"Same-source retry succeeds after failed detached map publication");
 }
 size_t polls=0;p=clone(raw);p->composite(b,[&]{++polls;return false;});
 for(size_t boundary:{size_t(1),polls/3,polls-1,polls}){
  p=clone(raw);base=p->composite(a);size_t at=0;check(refuses([&]{p->composite(b,[&]{return ++at>=boundary;});}),"Cancellation before/during/after staging returns no map");n=calls.load();check(p->composite(a)==base&&calls==n&&p->encode()==raw,"Cancellation retains old cache and immutable source exactly");check(equal(*p->composite(b),*expected),"Cancelled composite can retry the exact same source");
 }
 for(bool painted:{false,true}){
  auto baked=painted?clone(raw):std::make_shared<PaintedMesh>(scene.active().painted->mesh(),512);
  baked->install_bake(bake_fixture(baked->resolution()));
  const auto archive=baked->encode();
  reference_mode=true;
  const auto expected_bake=clone(archive)->composite(b);
  reference_mode=false;
  const auto before_bake=calls.load();
  const auto result=baked->composite(b);
  check(calls==before_bake+1&&equal(*expected_bake,*result)&&result->baked_base==baked->baked()&&
        !result->normal.rgba.empty()&&result->color_coverage.width==512&&result->emission_coverage.width==512,
        "Native Bake, normal data and replacement coverage are exact when the selected backend composites the material");
  for(int failure:{1,3}){
   const auto before_fault=calls.load();
   fault=failure;
   check(refuses([&]{baked->composite(a);})&&calls==before_fault+1&&baked->composite(b)==result&&baked->encode()==archive,
         "Baked dispatch failure or truncated normal output keeps the exact source and prior maps without CPU substitution");
  }
  check(baked->composite(a)->baked_base==baked->baked(),"Retrying the baked composite retains its immutable Bake owner");
 }
 {
  auto colored=source_color_fixture::model(false,true,false,512,false);
  auto &object=colored.active();
  Brush brush;brush.from=brush.to={.23f,.24f,0};brush.radius=.23f;brush.normal={0,0,1};
  PaintSettings paint;paint.color={.93f,.57f,.13f,1};paint.flow=.71f;paint.front_only=false;
  object.painted->begin_stroke();object.painted->stamp(brush,paint);
  check(object.painted->end_stroke(),"An actual source COLOR_0 mesh receives a common paint layer");
  const auto original=colored.encode();
  const auto start=prepared_calls.load();
  auto prepared=prepare_source_colors({object.painted,object.appearance,object.material_bindings,object.deformation,object.material});
  check(prepared.converted&&prepared_calls>start&&colored.encode()==original,
        "The native producer prepares actual imported vertex color on the selected backend and retains the source");
  auto unbaked=prepared.paint->composite(object.material);
  check(!unbaked->transparent&&unbaked->pbr_transparent&&unbaked->pbr_color.rgba!=unbaked->color.rgba,
        "Prepared PBR alpha remains distinct from the opaque common color image");
  auto bake=bake_fixture(512);
  bake.color_interpretation=BakedColorInterpretation::ResolvedPbr;
  bake.maps.pbr_color=bake.maps.color;
  for(size_t i=0;i<bake.maps.pbr_color.rgba.size();i+=4)std::swap(bake.maps.pbr_color.rgba[i],bake.maps.pbr_color.rgba[i+2]);
  prepared.paint->install_bake(std::move(bake));
  object.painted=prepared.paint;object.material_bindings=prepared.material_bindings;
  const auto archive=colored.encode();
  auto restarted=Scene::decode(archive);
  reference_mode=true;
  const auto expected_prepared=restarted.active().painted->composite(b);
  reference_mode=false;
  const auto dispatch=prepared_calls.load();
  const auto current_prepared=object.painted->composite(b);
  check(prepared_calls==dispatch+1&&equal(*expected_prepared,*current_prepared)&&restarted.encode()==archive,
        "Prepared color, resolved Bake, alpha and the archive match the independent native reference byte for byte");
  fault=4;
  check(refuses([&]{object.painted->composite(a);})&&object.painted->composite(b)==current_prepared&&colored.encode()==archive,
        "Truncated prepared color publishes no maps and leaves the baked source and previous cache exact");
  check(object.painted->composite(a)->color_projection==prepared.paint->color_projection(),
        "Prepared color retry keeps the original projection owner");
 }
 Document d;d.replace(scene);ready(d);const auto original=d.copy_scene().encode();auto held=d.frame().surface->ranges.front().textures;auto held_pixels=held->color.rgba;auto rest=d.copy_scene().active().painted->mesh();const auto rig=d.copy_scene().active().deformation->encode(rest);
 const auto native_start=calls.load();PaintSettings settings;settings.color={1,0,0,1};d.paint_settings(settings);check(d.begin_native_stroke(),"Real posed native paint gesture admitted");NativeStrokeSample sample;sample.origin={.3f,.1f,1};sample.direction={0,0,-1};sample.brush.radius=.2f;sample.brush.pressure=1;sample.brush.strength=1;d.sample_native_stroke(sample);d.end_native_stroke();
 auto path=std::filesystem::temp_directory_path()/("gpu-paint-composite-"+std::to_string(getpid())+".dsculpt");auto saved=d.save_async(path);saved.get();check(d.wait_native_stroke(),"Accepted native gesture and ordered Save drain");ready(d);
 auto after=d.copy_scene();check(bool(d.frame().surface->ranges.front().textures->current_grid_preview),"Native worker publishes a genuine render-only current-grid packet instead of downloading a derived composite");check(after.active().painted->pixel(1,256,256).data[9]>200&&after.active().deformation->encode(rest)==rig&&held->color.rgba==held_pixels,"Native posed paint changes original UV pixel while preserving rig/rest and held frame");
 reference_mode=true;auto native_expected=clone(after.active().painted->encode())->composite(after.active().material);reference_mode=false;check(equal(*after.active().painted->preview(after.active().material),*native_expected),"Existing explicit immutable CPU preview retains exact legacy14-channel image contract");d.frame().surface->ranges.front().textures->current_grid_preview->authenticate_range(d.frame().surface->mesh,d.frame().surface->ranges.front(),true);check(Scene::load(path).encode()==after.encode(),"Immediate native Save/restart captures accepted paint rather than a derived render job");
 check(d.undo()&&d.copy_scene().encode()==original&&!d.frame().state->can_undo,"Exactly one Undo restores complete source scene");ready(d);check(d.redo()&&d.copy_scene().encode()==after.encode(),"Redo restores exact posed paint result");ready(d);
 check(d.begin_native_stroke(),"Cancelled native gesture admitted");sample.origin.x+=.05f;d.sample_native_stroke(sample);d.end_native_stroke(false);check(d.wait_native_stroke()&&d.copy_scene().encode()==after.encode(),"Native cancel preserves authoritative paint and history");ready(d);
 check(d.begin_native_stroke(),"Stale-generation native gesture admitted");d.sample_native_stroke(sample);d.replace(Volume(.09f));check(d.wait_native_stroke()&&d.copy_volume().voxel_size()==.09f&&!d.frame().state->mesh_active,"New generation rejects late paint/derived maps");std::filesystem::remove(path);
 check(calls>0,"Actual producer/worker entered selected aggregate compositor");std::cout<<"PASS "<<checks<<" paint producer/cache/native checks on "<<real_backend()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
