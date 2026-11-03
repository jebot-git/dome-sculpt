// SPDX-License-Identifier: GPL-3.0-or-later
#include "sparse-resample-backend.hpp"
#include "sparse-resample-native.hpp"
#include "dome/volume.hpp"
#include <chrono>
#include <cstring>
#include <iostream>
#include <limits>
#ifdef DOME_TEST_SYCL
#include <sycl/sycl.hpp>
#endif
using namespace dome;using namespace dome::sparse_resample;
namespace {
size_t checks=0;void check(bool ok,const char *why){++checks;if(!ok)throw std::runtime_error(why);}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception &e){std::cout<<"REFUSAL "<<e.what()<<'\n';return true;}}
using Clock=std::chrono::steady_clock;double ms(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
template<class Field>double map_sample(const Field &field,Point q,Parameters p){
 const auto source=std::bit_cast<double>(p.source),target=std::bit_cast<double>(p.target),background=std::bit_cast<double>(p.background);
 const double pos[]={double(q.x)*target/source,double(q.y)*target/source,double(q.z)*target/source};Int3 lo{int(std::floor(pos[0])),int(std::floor(pos[1])),int(std::floor(pos[2]))};
 const double f[]={pos[0]-lo.x,pos[1]-lo.y,pos[2]-lo.z};double value=background;
 for(int z=0;z<2;++z)for(int y=0;y<2;++y)for(int x=0;x<2;++x){auto it=field.find(lo+Int3{x,y,z});if(it!=field.end())value+=(double(it->second)-background)*(x?f[0]:1-f[0])*(y?f[1]:1-f[1])*(z?f[2]:1-f[2]);}
 return value;
}
template<class Field>uint64_t reference(const Field &field,std::span<const ErrorQuery> queries,Parameters p){double error=0;for(auto &q:queries)error=std::max(error,std::abs(map_sample(field,q.point,p)-std::bit_cast<double>(q.expected)));return std::bit_cast<uint64_t>(error);}
void numeric(){
 CowMap<Int3,double,IntHash> field;std::vector<Entry> entries;
 for(int z=-3;z<=3;++z)for(int y=-4;y<=4;++y)for(int x=-5;x<=5;++x){const double value=double(x*31+y*7-z*11)/137.;field.emplace(Int3{x,y,z},value);entries.push_back({{x,y,z},value});}
 Grid grid(entries);const auto before=std::vector<Cell>(grid.cells().begin(),grid.cells().end());
 for(auto p:{parameters(.037f,.023f,.37),parameters(.023f,.037f,0),parameters(.04f,.02f,1)}){
  for(size_t count:{size_t(1),size_t(7),size_t(127),size_t(128),size_t(129),size_t(65543)}){
   std::vector<ErrorQuery> q;for(size_t i=0;i<count;++i)q.push_back({{int32_t(i%19)-9,int32_t((i/19)%17)-8,int32_t((i/323)%15)-7},0,std::bit_cast<uint64_t>(double(int(i%13)-6)/31.)});
   q.back().expected=std::bit_cast<uint64_t>(17.125);const auto original=q;
   SparseFieldSampler sampler(grid,p,127);auto actual=sampler.maximum_error(q);check(actual==reference(field,q,p),"Aggregate error differs from independent native paged-map interpolation/subtraction");
   uint64_t integer=0;for(auto &v:q)integer=std::max(integer,absolute_error(grid.cells().data(),grid.mask(),v,p));check(actual==integer,"Aggregate backend differs from shared integer-binary64 point errors");
   check(!std::memcmp(q.data(),original.data(),q.size()*sizeof(ErrorQuery)),"Aggregate query mutated expected values or coordinates");
   check(sampler.maximum_error({})==0,"Empty quality query has a nonzero maximum");
  }
 }
 // Force maxima into distinct workgroups and across the last chunk boundary.
 {const auto p=parameters(.037f,.023f,.37);std::vector<ErrorQuery> q(65543,{{1,-1,2},0,0});SparseFieldSampler sampler(grid,p,65536);
  for(size_t index:{size_t(127),size_t(128),size_t(65535),size_t(65542)}){q[index].expected=std::bit_cast<uint64_t>(31.125);check(sampler.maximum_error(q)==reference(field,q,p),"Two-stage maximum omitted a nonfirst workgroup or final chunk tail");q[index].expected=0;}}
 check(!std::memcmp(before.data(),grid.cells().data(),grid.cells().size_bytes()),"Quality changes its original sparse cells");
 const auto p=parameters(.037f,.023f,.37);SparseFieldSampler sampler(grid,p,127);std::vector<ErrorQuery> q(257,{{1,-1,2},0,std::bit_cast<uint64_t>(.123)});
 auto original=q;auto expect=reference(field,q,p);for(int kind=0;kind<4;++kind){auto bad=q;if(kind==0)bad[0].reserved=1;if(kind==1)bad[0].expected=std::bit_cast<uint64_t>(std::numeric_limits<double>::quiet_NaN());if(kind==2)bad[0].expected=std::bit_cast<uint64_t>(1e7);if(kind==3)bad[0].point.x=INT32_MAX;check(refuses([&]{sampler.maximum_error(bad);}),"Invalid quality admission unexpectedly dispatches");check(sampler.maximum_error(q)==expect,"Bad admission poisoned an otherwise untouched session");}
 size_t calls=0;const size_t validation_calls=(q.size()+255)/256+1;
 check(refuses([&]{sampler.maximum_error(q,[&]{return ++calls>validation_calls+2;});}),"Cancellation after an actual quality chunk returns a partial maximum");
 if(compute_backend()!="CPU")check(refuses([&]{sampler.maximum_error(q);}),"Cancelled GPU quality session was reused");
 SparseFieldSampler retry(grid,p,127);check(retry.maximum_error(q)==expect&&!std::memcmp(q.data(),original.data(),q.size()*sizeof(ErrorQuery)),"Quality retry after cancellation changed input/result");
 for(auto bits:{UINT64_C(0x8000000000000000),UINT64_C(0x7ff0000000000000),UINT64_MAX})check(refuses([&]{checked_error(bits);}),"Invalid device maximum is treated as a valid quality bound");
 // Independent signed-zero/subnormal/max-range subtraction oracles.
 Grid tiny(std::vector<Entry>{{{0,0,0},std::numeric_limits<double>::denorm_min()}});auto exact=parameters(.04f,.04f,0);std::vector<ErrorQuery> tail{{{0,0,0},0,0},{{0,0,0},0,std::bit_cast<uint64_t>(-0.)}};SparseFieldSampler small(tiny,exact,1);check(small.maximum_error(tail)==1,"Minimum binary64 subnormal absolute error was rounded away");
 {Grid unit(std::vector<Entry>{{{0,0,0},1}});SparseFieldSampler sampler(unit,parameters(.04f,.04f,0),1);std::vector<ErrorQuery> q{{{0,0,0},0,std::bit_cast<uint64_t>(std::nextafter(1.,2.))}};check(sampler.maximum_error(q)==std::bit_cast<uint64_t>(std::nextafter(1.,2.)-1.),"Binary64 oneULP error near unity was silently reduced at float precision");}
 std::cout<<"PASS "<<checks<<" exact aggregate sparse-quality checks on "<<compute_backend()<<'\n';
}
template<class Value>void bench_case(int base,bool coarse,const char *kind){
 CowMap<Int3,Value,IntHash> field,original;
 const int n=coarse?base:(base*3/2),m=coarse?(base*3/2):base;const float source=coarse?.03f:.02f,target=coarse?.02f:.03f;const double background=std::is_same_v<Value,float>?.37:0;
 auto function=[&](int x,int y,int z,float spacing){double v=std::sin(x*double(spacing)*3.1)+y*double(spacing)*.2-z*double(spacing)*.7;return Value(std::is_same_v<Value,float>?.5+v*.1:v);};
 for(int z=-n/2;z<n-n/2;++z)for(int y=-n/2;y<n-n/2;++y)for(int x=-n/2;x<n-n/2;++x)field.emplace(Int3{x,y,z},function(x,y,z,source));
 for(int z=-m/2;z<m-m/2;++z)for(int y=-m/2;y<m-m/2;++y)for(int x=-m/2;x<m-m/2;++x)original.emplace(Int3{x,y,z},function(x,y,z,target));
 const auto p=parameters(source,target,background);auto start=Clock::now();double native=0;for(auto [q,value]:original)native=std::max(native,std::abs(map_sample(field,{q.x,q.y,q.z},p)-double(value)));const auto native_ms=ms(start);
 start=Clock::now();std::vector<ErrorQuery> queries;queries.reserve(original.size());for(auto [q,value]:original)queries.push_back({{q.x,q.y,q.z},0,std::bit_cast<uint64_t>(double(value))});const auto query_pack=ms(start);
 start=Clock::now();std::vector<Entry> entries;entries.reserve(field.size());for(auto [q,value]:field)entries.push_back({{q.x,q.y,q.z},double(value)});const auto entry_pack=ms(start);
 start=Clock::now();Grid grid(entries);const auto hash_ms=ms(start);start=Clock::now();double packed=0;for(auto &q:queries)packed=std::max(packed,std::abs(native_sample(grid,p,q.point)-std::bit_cast<double>(q.expected)));const auto packed_ms=ms(start);check(std::bit_cast<uint64_t>(packed)==std::bit_cast<uint64_t>(native),"Packed CPU reference differs from native paged-map reconstruction");start=Clock::now();
 SparseFieldSampler sampler(grid,p);const auto session_ms=ms(start);start=Clock::now();const auto maximum=sampler.maximum_error(queries);const auto query_ms=ms(start);
 check(maximum==std::bit_cast<uint64_t>(native),"Full packed/upload/reduce/readback maximum differs from native map quality");
 std::cout<<"BENCH backend="<<compute_backend()<<" mode="<<(coarse?"coarsen":"refine")<<" kind="<<kind<<" source="<<field.size()<<" expected="<<queries.size()<<" native_map_ms="<<native_ms<<" packed_cpu_query_ms="<<packed_ms<<" packed_cpu_total_ms="<<(query_pack+entry_pack+hash_ms+packed_ms)<<" query_pack_ms="<<query_pack<<" entry_pack_ms="<<entry_pack<<" hash_ms="<<hash_ms<<" session_ms="<<session_ms<<" reduce_read_ms="<<query_ms<<" total_ms="<<(query_pack+entry_pack+hash_ms+session_ms+query_ms)<<" readback_bytes="<<(compute_backend()=="CPU"?0:((queries.size()+65535)/65536*8))<<" maxword="<<maximum<<'\n';
 for(int repeat=0;repeat<2;++repeat){start=Clock::now();double warm_native=0;for(auto [q,v]:original)warm_native=std::max(warm_native,std::abs(map_sample(field,{q.x,q.y,q.z},p)-double(v)));check(std::bit_cast<uint64_t>(warm_native)==maximum,"Repeated native map reconstruction changed error");std::cout<<"WARM_NATIVE mode="<<(coarse?"coarsen":"refine")<<" kind="<<kind<<" source="<<field.size()<<" expected="<<queries.size()<<" iteration="<<repeat<<" ms="<<ms(start)<<'\n';start=Clock::now();auto value=sampler.maximum_error(queries);check(value==maximum,"Repeated reduction lost its exact maximum");std::cout<<"WARM backend="<<compute_backend()<<" mode="<<(coarse?"coarsen":"refine")<<" kind="<<kind<<" source="<<field.size()<<" expected="<<queries.size()<<" iteration="<<repeat<<" reduce_read_ms="<<ms(start)<<'\n';}
}
}
int main(int argc,char **argv)try{
 if(argc>1&&std::string_view(argv[1])=="--missing"){Grid grid(std::vector<Entry>{{{0,0,0},1}});check(refuses([&]{SparseFieldSampler sampler(grid,parameters(.04f,.02f,0));sampler.maximum_error(std::vector<ErrorQuery>{{{0,0,0},0,0}});}),"Missing selected GPU silently returns a CPU quality result");return 0;}
#ifdef DOME_TEST_SYCL
 sycl::queue device{sycl::gpu_selector_v};auto info=device.get_device();auto name=info.get_info<sycl::info::device::name>();std::cout<<"ACTUAL_GPU "<<name<<" nativeFP64="<<info.has(sycl::aspect::fp64)<<'\n';check(name.find("Arc")!=std::string::npos&&name.find("A770")!=std::string::npos,"Quality fixture requires actual requested ArcA770");
#endif

 if(argc>1&&std::string_view(argv[1])=="--trace"){Grid grid(std::vector<Entry>{{{0,0,0},.125}});SparseFieldSampler sampler(grid,parameters(.04f,.02f,0),127);std::vector<ErrorQuery> q(257,{{0,0,0},0,std::bit_cast<uint64_t>(1.)});check(sampler.maximum_error(q)==std::bit_cast<uint64_t>(.875),"Traced aggregate-only three-chunk transaction has wrong maximum");std::cout<<"TRACE_EXPECT grid_bytes=48 query_uploads=3048,3048,72 scalar_readbacks=8,8,8 kernels=6\n";return 0;}
 if(argc>1&&std::string_view(argv[1])=="--bench"){for(int n:{4,24,48})for(bool coarse:{true,false}){bench_case<double>(n,coarse,"layer");bench_case<float>(n,coarse,"mask");}return 0;}
 numeric();return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
