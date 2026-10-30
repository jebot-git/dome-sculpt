// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/volume.hpp"
#include "dome/local-surface.hpp"
#include <cstdlib>
#include <iostream>
namespace {thread_local bool no_alloc=false;}
void *operator new(size_t n){if(no_alloc)throw std::bad_alloc();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
using namespace dome;
int main()try{
  unsigned checks=0;auto check=[&](bool yes,const char*s){if(!yes)throw std::runtime_error(s);++checks;std::cout<<"PASS "<<s<<'\n';};
  auto initial=sculpt_storage_stats().live_bytes;
  std::weak_ptr<const CowMap<Int3,float,IntHash>::Storage> weak;
  {
    CowMap<Int3,float,IntHash> values;
    for(int z=0;z<16;++z)for(int y=0;y<16;++y)for(int x=0;x<16;++x)values.emplace({x,y,z},float(x+16*y+256*z));
    values.emplace({1000000,-1000000,7},-7);values.emplace({-1,-16,-17},-17);
    check(values.size()==4098&&sculpt_storage_stats().dense_pages==1,"Dense promotion and far sparse pages preserve exact counts without a dense gap");
    auto before=values.read();auto same_page=values.owner()->page_owner({1000000,-1000000,7});auto changed_page=values.owner()->page_owner({0,0,0});auto old=sculpt_storage_stats();
    values[{0,0,0}]=.5f;auto stats=sculpt_storage_stats();
    check(before.at({0,0,0})==0&&values.at({0,0,0})==.5f&&values.owner()->page_owner({0,0,0})!=changed_page&&values.owner()->page_owner({1000000,-1000000,7})==same_page,"One-page mutation detaches only touched immutable data");
    check(stats.root_copies-old.root_copies==1&&stats.shard_copies-old.shard_copies==1&&stats.page_copies-old.page_copies==1&&stats.copied_bytes-old.copied_bytes<24000,"Local copy counts and payload bytes are independent of unrelated pages");
    double sum=0;size_t count=0;no_alloc=true;
    auto view=values.read();for(auto [p,v]:view){sum+=v;++count;if(view.at(p)!=v)throw std::runtime_error("read mismatch");}
    for(int i=0;i<100000;++i)sum+=view.at({i%16,(i/16)%16,(i/256)%16});
    no_alloc=false;
    check(count==4098&&sum>0,"Read-only count/find/sampling/iteration performs no allocations or hidden flattening");
    auto legacy=values.read().materialize();check(legacy.size()==values.size()&&values.read()==legacy,"Explicit legacy-map export preserves every value");
    auto retained=values.owner();auto cap=sculpt_storage_budget();set_sculpt_storage_budget(sculpt_storage_stats().live_bytes+256);bool rejected=false;
    try{values[{1,1,1}]=99;}catch(const std::length_error&){rejected=true;}set_sculpt_storage_budget(cap);
    check(rejected&&values.owner()==retained&&values.at({1,1,1})==273,"Memory exhaustion counts retained owners and refuses before root publication");
    before=values.read();for(int z=0;z<16;++z)for(int y=0;y<16;++y)for(int x=0;x<16;++x)if(x+16*y+256*z>=31)values.erase({x,y,z});
    check(values.size()==33&&before.size()==4098&&values.owner()->retained_bytes()<100000,"Dense erasure demotes a page and keeps acquired views unchanged");
    no_alloc=true;values.clear();no_alloc=false;check(values.empty()&&before.size()==4098,"Clear is allocation-free even with retained snapshots");
    weak=before.owner();
  }
  check(weak.expired()&&sculpt_storage_stats().live_pages==0&&sculpt_storage_stats().live_bytes>initial,"Weak ancestry retains control-block accounting but releases all pages");
  weak.reset();check(sculpt_storage_stats().live_bytes==initial,"All root/page/directory allocations return to their original budget");
  {
    CowMap<Int3,float,IntHash> many;
    for(int i=0;i<4096;++i)many.emplace({i*32,-i*48,(i%17)*32},float(i));
    auto original=many.read();
    std::vector<CowMap<Int3,float,IntHash>::Owner> held;held.reserve(128);
    size_t maximum_bytes=0,maximum_pages=0;
    auto untouched=many.owner()->page_owner({4095*32,-4095*48,(4095%17)*32});
    for(int i=0;i<128;++i){
      held.push_back(many.owner());auto before=sculpt_storage_stats();
      many[{(i%7)*32,-(i%7)*48,((i%7)%17)*32}]=float(-i-1);
      auto after=sculpt_storage_stats();
      maximum_bytes=std::max(maximum_bytes,after.copied_bytes-before.copied_bytes);
      maximum_pages=std::max(maximum_pages,after.page_copies-before.page_copies);
    }
    check(original.size()==4096&&original.at({0,0,0})==0&&held.front()->at({0,0,0})==0&&many.at({0,0,0})<0,"Repeated small edits across 4096 sparse pages preserve every retained historical root");
    check(maximum_pages==1&&maximum_bytes<65536&&many.owner()->page_owner({4095*32,-4095*48,(4095%17)*32})==untouched,"Many-page sustained edits copy one page and bounded touched directory payload, retaining distant pages");
    no_alloc=true;size_t entries=0;for(auto [p,value]:many.read()){if(!many.contains(p)||many.at(p)!=value)throw std::runtime_error("many-page read mismatch");++entries;}no_alloc=false;
    check(entries==4096,"4096-page sampler and iterator path allocates no directory or flattened representation");
    std::cout<<"MEASURE 4096-page maximum logical stamp-copy bytes "<<maximum_bytes<<" page copies "<<maximum_pages<<'\n';
  }
  check(sculpt_storage_stats().live_bytes==initial,"Many-page retained history returns all storage budget when released");
  std::cout<<checks<<" paged storage checks passed\n";
}catch(const std::exception&e){no_alloc=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
