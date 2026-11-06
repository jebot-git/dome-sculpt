// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/sculpt-chunk-index.hpp"
#include "sculpt-surface-tile.hpp"
#include "dome/mesh.hpp"
#include <bit>
#include <iostream>
#include <map>
using namespace dome;
using namespace dome::detail;
namespace {
size_t checks=0;
void check(bool value,const char *why){if(!value)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<'\n';}
template<class F>bool fails(F f){try{f();return false;}catch(const std::exception&){return true;}}
struct Traits{static size_t bytes(int){return 0;}static SculptChunkBounds bounds(int value){SculptChunkBounds b;b.include(Vec3{float(value),0,0});return b;}};
using Index=PersistentSculptIndex<int,int,Traits>;
struct Work {
  size_t bytes=0,steps=0,limit=SIZE_MAX,maximum_steps=SIZE_MAX;
  void tick(){if(steps>=maximum_steps)throw std::runtime_error("Cancelled index operation");++steps;}
  void charge(size_t n){if(bytes>limit||n>limit-bytes)throw std::length_error("Native index budget exceeded");bytes+=n;}
};
void index_regressions(){
  std::vector<std::pair<int,int>> values;for(int i=0;i<4095;++i)values.emplace_back(i,i*7);
  Work initial;Index tree;tree.assign_sorted(values.begin(),values.end(),initial);
  const auto expected=values.size()*(sizeof(Index::Node)+128);
  check(initial.bytes==expected,"Cold sorted construction charges exactly one final owning node per key");
  check(Index::n(tree.root)==values.size()&&Index::h(tree.root)==12,"Sorted build has the exact count and balanced height");
  Work traverse;std::vector<std::pair<int,int>> actual;tree.visit([&](int k,int v){actual.emplace_back(k,v);},traverse);
  check(actual==values,"Actual sorted-tree traversal preserves every key and value in original order");
  Work repeated;Index old_method;for(auto [k,v]:values)old_method.put(k,v,repeated);
  check(repeated.bytes>initial.bytes*4,"Equivalent repeated persistent insertion creates real discarded path-node requests");
  check(tree.root->payload_bytes==old_method.root->payload_bytes&&tree.root->bounds.minimum.x==0&&tree.root->bounds.maximum.x==float(values.back().second),"Cold aggregate payload and bounds match the existing persistent-index contract");
  const auto retained=tree.root;
  auto invalid=values;std::swap(invalid[2],invalid[3]);Work bad_order;
  check(fails([&]{tree.assign_sorted(invalid.begin(),invalid.end(),bad_order);})&&tree.root==retained,"Out-of-order input refuses without replacing the prior immutable root");
  invalid=values;invalid[3]=invalid[2];Work duplicate;
  check(fails([&]{tree.assign_sorted(invalid.begin(),invalid.end(),duplicate);})&&tree.root==retained,"Duplicate keys refuse without silently replacing a representative");
  Work tight;tight.limit=expected-1;
  check(fails([&]{tree.assign_sorted(values.begin(),values.end(),tight);})&&tree.root==retained,"A real one-byte owning-budget denial preserves the old complete tree");
  Work cancelled;cancelled.maximum_steps=values.size()+64;
  check(fails([&]{tree.assign_sorted(values.begin(),values.end(),cancelled);})&&tree.root==retained,"Cancellation during actual bottom-up node construction preserves the old root");
  Work edit;tree.put(11,-1,edit);tree.erase(14,edit);
  check(tree.find(11)&&*tree.find(11)==-1&&!tree.find(14)&&Index::n(tree.root)==values.size()-1,"Ordinary persistent local edits work on the directly built balanced tree");
  Index previous;previous.root=retained;
  check(previous.find(11)&&*previous.find(11)==77&&previous.find(14),"Retained initial ownership remains exact after later persistent edits");
}
Volume sphere(float voxel,bool interior_noise=false){
  const float band=voxel*4;const int n=int(std::ceil((.6f+band)/voxel));Volume::Voxels values;
  for(int z=-n;z<=n;++z)for(int y=-n;y<=n;++y)for(int x=-n;x<=n;++x){
    const float r=length(Vec3{x*voxel,y*voxel,z*voxel}),d=r-.6f;
    if(d<band){if(values.size()>=4'000'000)throw std::length_error("Complete sphere sample limit exceeded");float value=std::max(-band,d);if(interior_noise&&r<.15f&&((x+y+z)&1))value+=voxel*.2f;values.emplace(Int3{x,y,z},value);}
  }
  return Volume::from_samples(voxel,std::move(values));
}
bool bits(Vec3 a,Vec3 b){return std::bit_cast<std::array<uint32_t,3>>(a)==std::bit_cast<std::array<uint32_t,3>>(b);}
void exact_order(const Volume &field,const SculptSurfaceChunks::Ptr &owned){
  auto reference=surface_mesh(field);reference.validate();size_t at=0;
  owned->for_each_run([&](const auto &run){for(size_t i=run.first;i<run.first+run.count;++i){
    if(at>=reference.indices.size())throw std::runtime_error("Published ordered stream exceeds whole reference");
    const auto a=run.chunk->mesh.indices.at(i),b=reference.indices.at(at++);
    if(!bits(run.chunk->mesh.positions.at(a),reference.positions.at(b))||!bits(run.chunk->mesh.normals.at(a),reference.normals.at(b)))throw std::runtime_error("Direct cold index changed ordered representative/winding/normal bits");
  }});
  check(at==reference.indices.size(),"Every actual ordered corner/normal equals an independent whole-surface extraction");
}
void kernel_regressions(bool production){
  auto field=sphere(production?.0125f:.04f);const SculptChunkLimits limits;
  auto first=SculptSurfaceChunks::build(field,limits);
  check(first->representation()==SculptSurfaceChunks::Representation::Chunks,"Actual complete sphere cold build retains genuine chunks");
  check(first->report().working_bytes<=limits.working_bytes&&first->report().retained_bytes<=limits.retained_bytes,"Cold producer stays inside unchanged conservative1GiB/512MiB envelopes");
  if(production)check(first->report().indices/3>=250000,"Declared P1 kernel contains at least250000 real triangles");
  std::cout<<"INVENTORY voxel="<<field.voxel_size()<<" samples="<<field.stored_samples()<<" vertices="<<first->report().vertices<<" triangles="<<first->report().indices/3<<" retained="<<first->report().retained_bytes<<" working_peak="<<first->report().working_bytes<<'\n';
  exact_order(field,first);
  const auto prior=first->materialize();
  Brush brush;brush.from=brush.to={0,0,.58f};brush.radius=.07f;brush.blend=0;brush.tool=Tool::Carve;
  field.begin_stroke();field.stamp(brush);check(field.end_stroke(),"Real local geometry stroke changes authentic source ancestry");
  auto changed=first->update(field);exact_order(field,changed);
  auto denied=limits;denied.working_bytes=first->report().retained_bytes;
  check(fails([&]{first->update(field,denied);}),"Prior immutable ownership plus new live work still requires prospective capacity");
  check(fails([&]{first->update(field,[]{return true;});}),"Actual changed producer still honors cancellation");
  auto retained=first->materialize();check(retained.indices==prior.indices&&std::equal(retained.positions.begin(),retained.positions.end(),prior.positions.begin(),bits)&&std::equal(retained.normals.begin(),retained.normals.end(),prior.normals.begin(),bits),"Budget/cancellation controls preserve the prior exact immutable geometry");
  // A real interior smoothing edit changes authenticated SDF values across
  // multiple empty inner tiles. The surface remains far outside that noise;
  // this exposes serial extraction scratch without manufacturing geometry.
  auto noisy=sphere(.04f,true);auto before=SculptSurfaceChunks::build(noisy);exact_order(noisy,before);
  Brush smooth;smooth.from=smooth.to={};smooth.radius=.35f;smooth.tool=Tool::Smooth;smooth.strength=.6f;
  noisy.begin_stroke();noisy.stamp(smooth);check(noisy.end_stroke(),"Interior smoothing performs a real authenticated multi-tile source edit");
  auto after=before->update(noisy);exact_order(noisy,after);
  check(after->report().extracted_tiles>1,"Scratch regression actually extracts multiple serial tiles");
  auto tight=limits;tight.working_bytes=after->report().working_bytes;
  auto exact_bound=before->update(noisy,tight);exact_order(noisy,exact_bound);
  check(exact_bound->report().working_bytes<=tight.working_bytes,"Same real serial extraction admits at its conservative measured peak bound");
  const auto old_tile_minimum=before->report().retained_bytes+after->report().extracted_tiles*sculpt_surface_tile_working_bytes;
  check(after->report().working_bytes<old_tile_minimum,"Real serial-tile peak is below even the old cumulative scratch lower bound");
  std::cout<<"LOCAL extracted_tiles="<<after->report().extracted_tiles<<" working_peak="<<after->report().working_bytes<<" old_scratch_minimum="<<old_tile_minimum<<'\n';
}
}
int main(int argc,char **argv)try{
  const bool production=argc==2&&std::string_view(argv[1])=="--p1-reference";check(argc==1||production,"Usage: sculpt-chunk-workspace [--p1-reference]");
  index_regressions();kernel_regressions(production);std::cout<<"PASS total "<<checks<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
