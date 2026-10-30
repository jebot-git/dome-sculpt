// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/source-tangent-frames.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
namespace {
thread_local bool measuring=false,hit=false;
thread_local size_t allocations=0,fail_at=SIZE_MAX;
}
void *operator new(size_t bytes) {
  if(measuring && ++allocations>=fail_at){hit=true;throw std::bad_alloc();}
  if(void *p=std::malloc(bytes?bytes:1))return p;
  throw std::bad_alloc();
}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void *p) noexcept{std::free(p);}
void operator delete[](void *p) noexcept{std::free(p);}
void operator delete(void *p,size_t) noexcept{std::free(p);}
void operator delete[](void *p,size_t) noexcept{std::free(p);}
using namespace dome;
using namespace dome::detail;
namespace {
template<class T> bool same(const std::vector<T> &a,const std::vector<T> &b) {
  static_assert(std::is_trivially_copyable_v<T>);
  return a.size()==b.size() && (a.empty() || !std::memcmp(a.data(),b.data(),a.size()*sizeof(T)));
}
bool source_exact(const ImportedPrimitive &a,const ImportedPrimitive &b) {
  bool equal=a.normals_authored==b.normals_authored && a.name==b.name &&
      same(a.mesh.positions,b.mesh.positions) && same(a.mesh.normals,b.mesh.normals) &&
      same(a.mesh.uv,b.mesh.uv) && same(a.mesh.indices,b.mesh.indices) &&
      same(a.mesh.uv_pins,b.mesh.uv_pins) && same(a.mesh.uv_seams,b.mesh.uv_seams) &&
      same(a.tangents,b.tangents) && same(a.colors,b.colors) && same(a.uv1,b.uv1) &&
      a.morph_targets.size()==b.morph_targets.size();
  for(size_t n=0;n<a.morph_targets.size() && equal;++n)
    equal=same(a.morph_targets[n].positions,b.morph_targets[n].positions) &&
      same(a.morph_targets[n].normals,b.morph_targets[n].normals) &&
      same(a.morph_targets[n].tangents,b.morph_targets[n].tangents);
  return equal;
}
bool frames_exact(const SourceTangentFrames &a,const SourceTangentFrames &b) {
  bool equal=a.generated==b.generated && a.origin_vertex_count==b.origin_vertex_count &&
      a.origin_vertices==b.origin_vertices && same(a.base,b.base) && same(a.normals,b.normals) &&
      a.targets.size()==b.targets.size() && a.normal_targets.size()==b.normal_targets.size();
  for(size_t n=0;n<a.targets.size() && equal;++n)
    equal=same(a.targets[n],b.targets[n]) && same(a.normal_targets[n],b.normal_targets[n]);
  return equal;
}
void require(bool condition,const char *message){if(!condition)throw std::runtime_error(message);}
}
int main() try {
  ImportedPrimitive source;source.name="Žluťoučký flat source";
  source.mesh.positions={{0,0,0},{1,0,0},{0,1,0},{0,0,1}};
  source.mesh.normals.assign(4,normalized(Vec3{1,1,1}));source.mesh.indices={0,1,2,0,3,1};
  source.mesh.uv={{{0,0}},{{1,0}},{{0,1}},{{0,-1}}};source.mesh.uv_pins={1,0,0,1};source.mesh.uv_seams={1,0,1,0,1,0};
  source.tangents.assign(4,{0,0,1,-1});source.colors.assign(4,{.2f,.4f,.8f,.6f});source.uv1=source.mesh.uv;
  ImportedMorphTarget target;target.positions={{0,0,0},{0,0,0},{0,0,.5f},{0,.5f,0}};
  target.normals.assign(4,{99,98,97});target.tangents.assign(4,{123,456,789});source.morph_targets={target};
  const auto before=source;const auto *positions=source.mesh.positions.data(),*normals=source.mesh.normals.data();
  measuring=true;auto reference=source_tangent_frames(source,source.mesh.uv);measuring=false;
  const auto count=allocations;require(count>20,"Fault fixture did not exercise derived frame allocations");
  for(size_t site=1;site<=count;++site) {
    allocations=0;fail_at=site;hit=false;bool refused=false;measuring=true;
    try {auto output=source_tangent_frames(source,source.mesh.uv);}
    catch(const std::exception &){refused=true;}
    measuring=false;
    require(hit && refused,"A measured flat-frame failure site was not reached and refused");
    require(source_exact(source,before) && positions==source.mesh.positions.data() && normals==source.mesh.normals.data(),
        "Flat-frame allocation failure changed source values or owners");
    require(frames_exact(source_tangent_frames(source,source.mesh.uv),reference),"Flat-frame retry differs after allocation failure");
  }
  size_t visits=0;source_tangent_frames(source,source.mesh.uv,[&]{++visits;return false;});
  for(size_t cutoff=1;cutoff<=visits;++cutoff) {
    size_t visited=0;bool refused=false;
    try {source_tangent_frames(source,source.mesh.uv,[&]{return ++visited>=cutoff;});}
    catch(const std::runtime_error &){refused=true;}
    require(refused && visited>=cutoff && source_exact(source,before),"Flat-frame cancellation missed its source-exact boundary");
    require(frames_exact(source_tangent_frames(source,source.mesh.uv),reference),"Flat-frame retry differs after cancellation");
  }
  std::cout<<"PASS "<<count<<" actual persistent C++ allocation failures and "<<visits
      <<" cancellation boundaries preserve exact source owners and retry frames\n";
}catch(const std::exception &e){measuring=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
