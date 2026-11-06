// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/sculpt-surface-chunks.hpp"
#include <bit>
#include <iostream>
#include <set>
using namespace dome;
using namespace dome::detail;
namespace {
size_t checks=0;
void check(bool value,const char *label) {
  if(!value)throw std::runtime_error(label);
  ++checks;std::cout<<"PASS "<<label<<'\n';
}
bool bits(Vec3 a,Vec3 b) {
  return std::bit_cast<std::array<uint32_t,3>>(a)==std::bit_cast<std::array<uint32_t,3>>(b);
}
bool same(const Mesh &a,const Mesh &b) {
  return a.indices==b.indices&&a.positions.size()==b.positions.size()&&
      a.normals.size()==b.normals.size()&&
      std::equal(a.positions.begin(),a.positions.end(),b.positions.begin(),bits)&&
      std::equal(a.normals.begin(),a.normals.end(),b.normals.begin(),bits);
}
template<class F> bool fails(F f) { try { f();return false; }catch(const std::exception &) {return true;} }
Volume two_shells(float voxel=.04f,Int3 offset={},bool exact_zero=false) {
  Volume::Voxels samples;
  for(int x=-16;x<=16;++x)for(int y=-9;y<=9;++y)for(int z=-9;z<=9;++z) {
    const Vec3 a{float(x)-6.17f,float(y)+.31f,float(z)-.23f};
    const Vec3 b{float(x)+6.39f,float(y)-.41f,float(z)+.37f};
    float distance=std::min(length(a)-4.35f,length(b)-3.93f);
    if(exact_zero)distance=float(std::min(std::max({std::abs(x-6),std::abs(y),std::abs(z)})-4,
                                         std::max({std::abs(x+6),std::abs(y),std::abs(z)})-4));
    samples.emplace(Int3{x,y,z}+offset,std::clamp(distance,-4.f,4.f)*voxel);
  }
  return Volume::from_samples(voxel,std::move(samples));
}
struct Corner { Vec3 position,normal; };
std::vector<Corner> corners(const SculptSurfaceChunks &source) {
  std::map<SculptTriangleOrigin,std::array<Corner,3>> triangles;
  source.for_each_triangle([&](const auto &chunk,size_t t,const auto &p,const auto &n){
    std::array<Corner,3> value;for(size_t i=0;i<3;++i)value[i]={p[i],n[i]};
    if(!triangles.emplace(chunk->triangles.at(t),value).second)
      throw std::runtime_error("Duplicate original chunk triangle provenance");
  });
  std::vector<Corner> result;
  for(const auto &[key,value]:triangles) { (void)key;result.insert(result.end(),value.begin(),value.end()); }
  check(!result.empty(),"Actual chunk triangles retain unique nonempty original cell provenance");
  return result;
}
bool corner_oracle(const std::vector<Corner> &a,const Mesh &b) {
  if(a.size()!=b.indices.size())return false;
  for(size_t i=0;i<a.size();++i)
    if(!bits(a[i].position,b.positions.at(b.indices[i]))||!bits(a[i].normal,b.normals.at(b.indices[i])))return false;
  return true;
}
size_t closed_components(const Mesh &m) {
  // Independent oriented edge incidence and connected components. This does
  // not accept coincident same-position corners as distinct seam vertices.
  std::map<std::array<float,3>,size_t> ids;
  std::vector<size_t> vertices;
  for(auto p:m.positions)vertices.push_back(ids.emplace(std::array<float,3>{p.x,p.y,p.z},ids.size()).first->second);
  using Edge=std::pair<size_t,size_t>;
  std::map<Edge,std::vector<std::pair<size_t,int>>> edges;
  for(size_t t=0;t<m.indices.size()/3;++t)for(size_t k=0;k<3;++k) {
    auto a=vertices.at(m.indices[3*t+k]),b=vertices.at(m.indices[3*t+(k+1)%3]);int sign=1;
    if(a>b){std::swap(a,b);sign=-1;}edges[{a,b}].push_back({t,sign});
  }
  std::vector<std::vector<size_t>> adjacent(m.indices.size()/3);
  for(const auto &[key,faces]:edges) {
    (void)key;
    if(faces.size()!=2||faces[0].second+faces[1].second!=0)throw std::runtime_error("Independent seam/winding oracle failed");
    adjacent[faces[0].first].push_back(faces[1].first);adjacent[faces[1].first].push_back(faces[0].first);
  }
  size_t count=0;std::vector<bool> seen(adjacent.size());
  for(size_t i=0;i<seen.size();++i)if(!seen[i]) {
    ++count;std::vector<size_t> queue{i};seen[i]=true;
    for(size_t h=0;h<queue.size();++h)for(auto next:adjacent[queue[h]])if(!seen[next]) {seen[next]=true;queue.push_back(next);}
  }
  return count;
}
void stamp(Volume &v,Vec3 point) {
  Brush b;b.from=b.to=point;b.radius=.07f;b.tool=Tool::Clay;b.blend=0;
  v.begin_stroke();v.stamp(b);v.end_stroke();
}
} // namespace
int main() try {
  auto field=two_shells();auto source=SculptSurfaceChunks::build(field);
  check(source->representation()==SculptSurfaceChunks::Representation::Chunks,
        "A real seam-crossing two-component field uses the actual chunk path");
  const auto expected=surface_mesh(field);const auto flat=source->materialize();
  check(same(flat,expected),"Chunk flatten preserves independent whole-mesher indices and position/normal bits");
  auto actual=corners(*source);
  check(corner_oracle(actual,expected),"Ordered independent triangle corner/normal/winding oracle agrees");
  check(closed_components(flat)==2,"Two separate connected components remain closed with opposite shared halfedges");
  auto wrong=expected;if(!wrong.normals.empty())wrong.normals.front()=wrong.normals.front()*-1;
  check(!corner_oracle(actual,wrong),"Independent normal oracle rejects a wrong representative normal");
  check(source->update(field)==source,"Authenticated unchanged field reuses the actual complete manifest owner");
  std::map<Int3,SculptSurfaceChunks::Chunk> old;
  source->for_each_chunk([&](const auto &c){old.emplace(c->coordinate,c);});
  stamp(field,{.08f,.04f,0});auto next=source->update(field);
  check(next->representation()==SculptSurfaceChunks::Representation::Chunks,
        "Actual local sculpt edit stays in a represented chunk domain");
  check(same(next->materialize(),surface_mesh(field)),"Local dependency closure preserves the independent whole-mesh result");
  size_t reused=0,changed=0;
  next->for_each_chunk([&](const auto &c){auto i=old.find(c->coordinate);if(i!=old.end()&&i->second==c)++reused;else ++changed;});
  check(reused>0&&changed>0,"Real changed chunks coexist with exact unchanged immutable chunk owners");
  auto replay=old;size_t updates=0;
  check(next->for_each_change(source,[&](Int3 key,const auto &chunk){
    auto i=replay.find(key);
    if(i!=replay.end()&&i->second==chunk)throw std::runtime_error("Unchanged owner appeared in a local upload delta");
    if(chunk)replay[key]=chunk;else replay.erase(key);++updates;
  }),"Actual retained base owner authorizes the sealed local upload delta");
  std::map<Int3,SculptSurfaceChunks::Chunk> current;
  next->for_each_chunk([&](const auto &c){current.emplace(c->coordinate,c);});
  check(replay==current&&updates>0,"Owner-authenticated additions/removals reproduce the complete current manifest");
  auto foreign=SculptSurfaceChunks::build(two_shells());size_t foreign_callbacks=0;
  check(!next->for_each_change(foreign,[&](auto,const auto &){++foreign_callbacks;})&&foreign_callbacks==0,
        "Equal-valued foreign geometry cannot authorize a different base delta");
  check(same(source->materialize(),expected),"Retained old chunk/source owners remain exact after COW edit");
  check(fails([&]{next->update(field,[]{return true;});}),"Actual no-op update still honors cancellation");
  check(same(next->materialize(),surface_mesh(field)),"Cancellation leaves current owning surface usable");
  for(auto offset:{Int3{},Int3{-9,0,7}}) {
    auto zero=two_shells(.04f,offset,true);auto owned=SculptSurfaceChunks::build(zero);
    auto mesh=owned->materialize();
    check(same(mesh,surface_mesh(zero)),"Exact-zero seam crossings preserve current global repair and representative semantics");
    check(closed_components(mesh)==2,"Exact-zero separate components retain actual closed orientation");
  }
  auto tight=SculptChunkLimits{};tight.steps=1;
  check(fails([&]{SculptSurfaceChunks::build(field,tight);}),"Real bounded compiler work refuses without publishing a partial owner");
  check(same(next->materialize(),surface_mesh(field)),"Resource refusal preserves the prior independently verified surface");
  std::cout<<checks<<" sculpt chunk checks passed\n";return 0;
} catch(const std::exception &e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
