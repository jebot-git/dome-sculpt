// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/material-visibility.hpp"
#include "dome/source-placement.hpp"
#include "sculpt-placed-chunks.hpp"
#include <nlohmann/json.hpp>
#include <atomic>
#include <bit>
#include <iostream>
#include <thread>
using namespace dome;
using namespace dome::detail;
namespace {
size_t checks=0;
void check(bool yes,const char *why){if(!yes)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<'\n';}
template<class F>bool refuses(F fn){try{fn();return false;}catch(const std::exception&){return true;}}
template<class T>bool same(const std::shared_ptr<const T>&a,const std::shared_ptr<const T>&b){return a.get()==b.get()&&!a.owner_before(b)&&!b.owner_before(a);}
bool bits(Vec3 a,Vec3 b){return std::bit_cast<std::array<uint32_t,3>>(a)==std::bit_cast<std::array<uint32_t,3>>(b);}
std::atomic_bool armed=false,waiting=false,released=false;
std::atomic_uint64_t blocked=0;
Volume sphere(float voxel){
  Volume::Voxels values;const float band=voxel*4;const int n=int(std::ceil((.6f+band)/voxel));
  for(int z=-n;z<=n;++z)for(int y=-n;y<=n;++y)for(int x=-n;x<=n;++x){
    const auto distance=length(Vec3{x*voxel,y*voxel,z*voxel})-.6f;
    if(distance<band){if(values.size()>=4'000'000)throw std::length_error("Original complete field sample guard");values.emplace(Int3{x,y,z},std::max(-band,distance));}
  }
  return Volume::from_samples(voxel,std::move(values));
}
Scene fixture(bool production){
  Scene s;const auto v=sphere(production?.0125f:.06f);s.active().volume=std::make_shared<Volume>(v);s.active().material.double_sided=true;
  if(!production){
    CanvasTransform pose;pose.position={1.4f,0,0};const auto second=s.add("Placed sibling",v,pose);s.object(second).material.double_sided=true;
    s.object(second).affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(std::array<double,16>{-1,0,0,0,.17,1.1,0,0,.07,.09,.85,0,0,0,0,1}));
    const auto hidden=s.add("Hidden sibling",v);s.object(hidden).visible=false;s.select(1);
  }
  return s;
}
void ready(Document &d){
  check(d.wait_mesh(std::chrono::seconds(120)),"Actual Document reaches publication");
  if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);
  const auto f=d.frame();check(f.surface->generation==f.state->generation&&f.surface->revision==f.state->revision&&f.surface->view_epoch==f.state->view_epoch,"Actual worker publishes the current generation/revision/view");
}
void exact(const Scene&s,const SculptSceneChunks::Ptr&published){
  size_t corners=0,queries=0;
  for(const auto&object:published->objects()){
    const auto whole=surface_mesh(*s.object(object.object).volume);whole.validate();size_t at=0;
    object.geometry->for_each_run([&](const auto&run){for(size_t c=run.first;c<run.first+run.count;++c){
      if(at>=whole.indices.size())throw std::runtime_error("Extra ordered corner");
      const auto a=run.chunk->mesh.indices.at(c),b=whole.indices.at(at++);
      if(!bits(run.chunk->mesh.positions.at(a),whole.positions.at(b))||!bits(run.chunk->mesh.normals.at(a),whole.normals.at(b)))throw std::runtime_error("Independent whole extraction differs in ordered local corner/normal bits");
      ++corners;
    }});
    check(at==whole.indices.size(),"Every ordered local corner/normal equals independent whole extraction");
    SurfaceQuery oracle(whole);size_t local_queries=0;
    object.placed->for_each([&](const auto&placed){
      check(same(placed->geometry_owner(),std::shared_ptr<const Mesh>(placed->source(),&placed->source()->mesh)),"Placed geometry aliases its actual immutable local source control block");
      if(local_queries>=4)return;
      const auto&m=*placed->geometry_owner();const auto a=m.indices[0],b=m.indices[1],c=m.indices[2];const auto center=(m.positions[a]+m.positions[b]+m.positions[c])/3.f;
      const auto n=normalized(m.normals[a]+m.normals[b]+m.normals[c]);const auto origin=center+n*.02f;
      const auto &world=placed->placement_frame().world();const auto p=world.point({origin.x,origin.y,origin.z});const auto&m4=world.matrix();
      const Vec3 start{float(p[0]),float(p[1]),float(p[2])},direction{float(-m4[0]*n.x-m4[4]*n.y-m4[8]*n.z),float(-m4[1]*n.x-m4[5]*n.y-m4[9]*n.z),float(-m4[2]*n.x-m4[6]*n.y-m4[10]*n.z)};
      const auto query=placed->query()->raycast_result(start,direction,.08f,world,{});
      const auto expected=oracle.raycast_result(start,direction,.08f,world,{});
      if(!query||!expected||std::abs(query->surface.distance-expected->surface.distance)>3e-5||length(query->surface.position-expected->surface.position)>3e-5f||length(query->surface.normal-expected->surface.normal)>3e-4f)throw std::runtime_error("Full affine query differs from independent whole geometry");
      ++local_queries;++queries;
    });
  }
  check(corners&&queries,"Independent exact geometry and full-affine query controls do real work");
}
NativeStrokeSample input(){NativeStrokeSample v;v.origin={.24f,0,1.5f};v.direction={0,0,-1};v.brush.radius=.045f;v.brush.blend=.006f;v.brush.strength=.23f;v.brush.tool=Tool::Clay;return v;}
void stroke(Document&d){check(d.begin_native_stroke()&&d.sample_native_stroke(input()),"Actual warm-up native stroke is accepted");d.end_native_stroke(true);check(d.wait_native_stroke(std::chrono::seconds(120)),"Actual warm-up native worker completes");ready(d);}
void limits_and_owners(){
  auto source=fixture(false);const auto first=SculptSceneChunks::capture(source,{31,1,1,true,true});const auto*old=first.chunks->find(1);const auto kept=old->placed;exact(source,first.chunks);
  check(first.chunks->retained_bytes()<2*(old->geometry->report().retained_bytes+first.chunks->find(2)->geometry->report().retained_bytes),"Scene retains actual geometry once rather than once again per placed directory");
  source.object(1).transform.position={.07f,.03f,-.02f};const auto moved=SculptSceneChunks::capture(source,{31,2,1,true,true},first.chunks);const auto*current=moved.chunks->find(1);
  check(same(current->geometry,old->geometry)&&moved.chunks->find(2)->placed==first.chunks->find(2)->placed,"Authored movement shares its true geometry and preserves the unrelated posed object");
  size_t shared=0;current->placed->for_each([&](const auto&p){const auto previous=kept->find(p->source());if(!same(p->query(),previous->query())||p==previous)throw std::runtime_error("Moved wrapper failed to share its actual previous BVH");++shared;});check(shared>1,"Multiple moved wrappers share actual source/query owners without new geometry");
  check(current->placed->report().working_bytes<2*current->placed->report().retained_bytes,"Same-geometry placement avoids counting the same complete geometry owner twice");exact(source,moved.chunks);
  SculptPlacedChunksLimits limit;size_t low=0,high=limit.working_bytes;
  while(high-low>1){const auto middle=low+(high-low)/2;limit.working_bytes=middle;if(refuses([&]{SculptPlacedChunks::capture(moved.chunks,1,kept,true,limit);}))low=middle;else high=middle;}
  limit.working_bytes=high;const auto admitted=SculptPlacedChunks::capture(moved.chunks,1,kept,true,limit);check(admitted->report().working_bytes<=high,"Standalone full-owner capture admits at its actual tight working limit");
  limit.working_bytes=high-1;check(refuses([&]{SculptPlacedChunks::capture(moved.chunks,1,kept,true,limit);}),"One byte below the actual standalone envelope refuses");
  limit={};limit.working_bytes=old->geometry->report().retained_bytes-1;check(refuses([&]{SculptPlacedChunks::capture(moved.chunks,1,{},true,limit);}),"Standalone factory still reserves its complete actual borrowed geometry");
  limit={};limit.retained_bytes=current->placed->report().retained_bytes-1;check(refuses([&]{SculptPlacedChunks::capture(moved.chunks,1,{},true,limit);}),"Standalone retained guard still includes geometry plus every actual placed owner");
  size_t polls=0;check(refuses([&]{SculptPlacedChunks::capture(moved.chunks,1,kept,true,{},[&]{return ++polls>=5;});})&&polls>=5,"Cancellation reaches actual directory work without mutating retained owners");
  check(first.chunks->find(1)->placed==kept&&first.chunks->find(1)->geometry==old->geometry,"Tight-limit/cancellation failures preserve the original immutable directory");
}
void document(bool production){
  Document d;auto scene=fixture(production);d.replace(scene);d.configure_object_grab(true,false);ready(d);const auto original=d.copy_scene().encode();const auto kept=d.frame().surface;size_t triangles=0;for(const auto&o:kept->sculpt_chunks->objects())triangles+=o.geometry->report().indices/3;
  if(production)check(triangles>=250000,"Unchanged P1 workload really contains at least250000 visible triangles");
  std::cout<<"INVENTORY triangles="<<triangles<<" retained="<<kept->sculpt_chunks->retained_bytes()<<'\n';exact(d.copy_scene(),kept->sculpt_chunks);
  stroke(d);const auto changed=d.copy_scene().encode();check(changed!=original,"Warm-up actually changes native sculpt state");exact(d.copy_scene(),d.frame().surface->sculpt_chunks);
  check(d.undo(),"Actual warm-up Undo is accepted");ready(d);check(d.copy_scene().encode()==original,"Actual warm-up Undo restores exact native state");exact(d.copy_scene(),d.frame().surface->sculpt_chunks);
  check(d.redo(),"Actual warm-up Redo is accepted");ready(d);check(d.copy_scene().encode()==changed,"Actual warm-up Redo restores exact edited state");
  check(d.begin_object_grab(1),"Actual generated object enters held mode");const auto held=d.frame().surface;CanvasTransform delta;delta.position={.04f,.02f,0};delta.scale=.9f;d.move_held_object_world(delta);check(d.frame().surface==held,"Actual held motion retains the immutable geometry/query snapshot");d.end_object_grab(false);ready(d);check(d.copy_scene().encode()==changed,"Held cancellation preserves exact authored state");
  const auto replacement=d.copy_scene();waiting=false;released=false;armed=true;
  struct Release{~Release(){armed=false;released=true;}}release;
  check(d.begin_native_stroke()&&d.sample_native_stroke(input()),"Stale-generation control starts an actual native stroke");d.end_native_stroke(true);check(d.wait_native_stroke(std::chrono::seconds(120)),"Stale-generation native worker completes");
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(120);while(!waiting&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));check(waiting,"Actual completed capture return seam is reached");d.replace(replacement,false);released=true;ready(d);check(d.copy_scene().encode()==changed&&d.frame().surface->generation!=blocked,"Actual retired candidate cannot replace the newer generation");check(kept->sculpt_chunks->find(1)->geometry->report().indices>0,"Original immutable published owner survives all warm/history/stale work");
}
}
// Test-only barrier after the genuine complete factory returns, before the
// Document accepts it. No product hook, artificial snapshot or bypass exists.
#define CAPTURE_SYMBOL "_ZN4dome6detail17SculptSceneChunks7captureERKNS_5SceneENS0_23SculptSceneChunkContextERKSt10shared_ptrIKS1_ENS0_22SculptSceneChunkLimitsESt8functionIFbvEE"
extern "C" SculptScenePreview actual_capture(const Scene&,SculptSceneChunkContext,const SculptSceneChunks::Ptr&,SculptSceneChunkLimits,LocalSurfaceCache::Cancel) asm("__real_" CAPTURE_SYMBOL);
extern "C" SculptScenePreview intercepted_capture(const Scene&,SculptSceneChunkContext,const SculptSceneChunks::Ptr&,SculptSceneChunkLimits,LocalSurfaceCache::Cancel) asm("__wrap_" CAPTURE_SYMBOL);
extern "C" SculptScenePreview intercepted_capture(const Scene&s,SculptSceneChunkContext c,const SculptSceneChunks::Ptr&b,SculptSceneChunkLimits l,LocalSurfaceCache::Cancel x){auto result=actual_capture(s,c,b,l,std::move(x));if(armed.exchange(false)){blocked=result.chunks->context().generation;waiting=true;while(!released)std::this_thread::sleep_for(std::chrono::milliseconds(1));}return result;}
int main(int argc,char**argv)try{const bool production=argc==2&&std::string_view(argv[1])=="--p1-warm-undo";check(argc==1||production,"Usage: sculpt-placed-workspace [--p1-warm-undo]");limits_and_owners();document(production);std::cout<<"PASS "<<checks<<" placed-owner workspace controls\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
