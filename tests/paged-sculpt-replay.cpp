// SPDX-License-Identifier: GPL-3.0-or-later
// This exact source also compiles against the frozen pre-page core. Compare
// STATE lines; measurements deliberately exclude sorting/serialization.
#include "dome/document.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <nlohmann/json.hpp>
#ifdef DOME_PAGED_STORAGE
#include "dome/sculpt-storage.hpp"
#endif
namespace {
std::atomic_bool recording{false};
std::atomic_size_t calls{0}, bytes{0};
}
void *operator new(size_t n) {
  if(recording.load(std::memory_order_relaxed)) {
    calls.fetch_add(1,std::memory_order_relaxed);
    bytes.fetch_add(n,std::memory_order_relaxed);
  }
  if(auto p=std::malloc(n?n:1)) return p;
  throw std::bad_alloc();
}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p) noexcept{std::free(p);}
void operator delete[](void*p) noexcept{std::free(p);}
void operator delete(void*p,size_t) noexcept{std::free(p);}
void operator delete[](void*p,size_t) noexcept{std::free(p);}
using namespace dome;
namespace {
uint64_t hash=1469598103934665603ull;
void word(uint64_t n){for(int i=0;i<8;++i){hash^=(n>>(8*i))&255;hash*=1099511628211ull;}}
void number(float n){word(std::bit_cast<uint32_t>(n));}
void number(double n){word(std::bit_cast<uint64_t>(n));}
void table(const auto& map){
  using V=std::remove_cvref_t<decltype((*map.begin()).second)>;
  std::vector<std::pair<Int3,V>> sorted;
  sorted.reserve(map.size());
  for(auto [p,v]:map) sorted.emplace_back(p,v);
  std::sort(sorted.begin(),sorted.end(),[](auto&a,auto&b){return a.first<b.first;});
  word(sorted.size());
  for(auto [p,v]:sorted){word(uint32_t(p.x));word(uint32_t(p.y));word(uint32_t(p.z));number(v);}
}
void state(const Volume&v,const char*label,size_t step){
  hash=1469598103934665603ull;number(v.voxel_size());number(v.band());
  table(v.voxels());word(v.layers().size());word(v.active_layer());
  for(auto&l:v.layers()){
    word(l.id);for(auto c:l.name)word(uint8_t(c));
    word(l.visible);word(l.locked);number(l.opacity);table(l.delta);
  }
  number(v.mask_default());table(v.mask_values());
  std::cout<<"STATE "<<label<<' '<<step<<' '<<v.voxels().size()<<' '<<hash<<'\n';
}
Brush brush(Vec3 p,float radius,Tool tool){
  Brush b;b.from=b.to=p;b.radius=radius;b.blend=0;b.tool=tool;return b;
}
Volume base(){
  Volume v(.025f);v.begin_stroke();v.stamp(brush({},.6f,Tool::Clay));v.end_stroke();
  v.add_layer("Long gesture");return v;
}
Brush stamp(size_t i){
  float a=float(i)*.137f;
  if(i%3==0) return brush({-2.4f+float(i/3)*.075f,-.8f,.15f},.052f,Tool::Clay);
  return brush({.56f*std::cos(a),.56f*std::sin(a),.09f*std::sin(a*.7f)},
               .06f,i%3==1?Tool::Carve:Tool::Clay);
}
using Clock=std::chrono::steady_clock;
double ms(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
void ready(Document&d){
  if(!d.wait_mesh(std::chrono::seconds(60))||!d.frame().state->error.empty())
    throw std::runtime_error("Document mesh failed: "+d.frame().state->error);
}
}
int main() try{
  auto v=base();state(v,"initial",0);
  std::vector<Volume> snapshots;snapshots.reserve(12);
  double elapsed=0;size_t allocations=0,allocated=0;
#ifdef DOME_PAGED_STORAGE
  auto first=sculpt_storage_stats();size_t maxcopy=0,maxpages=0;
#endif
  v.begin_stroke();
  for(size_t i=0;i<192;++i){
#ifdef DOME_PAGED_STORAGE
    auto before=sculpt_storage_stats();
#endif
    calls=0;bytes=0;auto t=Clock::now();recording=true;
    v.stamp(stamp(i));recording=false;
    elapsed+=ms(t);allocations+=calls.load();allocated+=bytes.load();
#ifdef DOME_PAGED_STORAGE
    auto after=sculpt_storage_stats();
    maxcopy=std::max(maxcopy,after.copied_bytes-before.copied_bytes);
    maxpages=std::max(maxpages,after.page_copies-before.page_copies);
#endif
    if((i+1)%16==0)snapshots.push_back(v.geometry_snapshot());
    if((i+1)%32==0){
      state(v,"gesture",i+1);
      std::cout<<"MEASURE gesture "<<i+1<<" ms "<<elapsed<<" allocations "
               <<allocations<<" requested_bytes "<<allocated<<'\n';
    }
  }
  v.end_stroke();state(v,"end",192);
#ifdef DOME_PAGED_STORAGE
  auto last=sculpt_storage_stats();
  std::cout<<"MEASURE pages live "<<last.live_pages<<" dense "<<last.dense_pages
           <<" charged_live_bytes "<<last.live_bytes<<" charged_peak_bytes "<<last.peak_bytes
           <<" copied_bytes "<<last.copied_bytes-first.copied_bytes
           <<" max_stamp_copied_bytes "<<maxcopy<<" max_stamp_page_copies "<<maxpages<<'\n';
#endif
  if(!v.undo())throw std::runtime_error("Missing gesture undo");state(v,"undo",0);
  if(!v.redo())throw std::runtime_error("Missing gesture redo");state(v,"redo",192);
  v.fill_mask(.35f);v.invert_mask();state(v,"mask",192);
  auto loaded=Volume::decode(v.encode());state(loaded,"restart",192);
  for(size_t i=0;i<snapshots.size();++i)state(snapshots[i],"held",(i+1)*16);
  double sum=0;calls=0;bytes=0;auto t=Clock::now();recording=true;
  for(int pass=0;pass<8;++pass)for(auto [p,d]:v.voxels())sum+=d+v.at(p);
  for(int pass=0;pass<200000;++pass)
    sum+=v.sample({float(pass%71)*.015f-.5f,float(pass%67)*.012f-.4f,.02f});
  recording=false;
  std::cout<<"MEASURE readonly ms "<<ms(t)<<" allocations "<<calls<<" requested_bytes "<<bytes<<" checksum "<<sum<<'\n';
  Document doc;doc.replace(base());ready(doc);
  elapsed=0;allocations=allocated=0;
  for(size_t i=0;i<24;++i){
    auto b=stamp(i*3+1);calls=0;bytes=0;auto start=Clock::now();recording=true;
    if(!doc.begin_desktop())throw std::runtime_error("Rejected Document gesture");
    doc.stamp_desktop(b);doc.end_desktop();ready(doc);recording=false;
    elapsed+=ms(start);allocations+=calls.load();allocated+=bytes.load();
  }
  state(doc.copy_volume(),"document",24);
  std::cout<<"MEASURE document ms "<<elapsed<<" allocations "<<allocations<<" requested_bytes "<<allocated<<'\n';
  std::cout<<"PASS sustained gesture, retained snapshots, history, restart and actual Document completed\n";
}catch(const std::exception&e){recording=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
