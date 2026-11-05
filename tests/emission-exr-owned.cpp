// SPDX-License-Identifier: GPL-3.0-or-later
#include "emission-exr-owned.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>
namespace {
thread_local int64_t deny=-1;
thread_local bool observe=false,hit=false;
thread_local size_t calls=0;
void allocating(){if(observe)++calls;if(deny==0){hit=true;throw std::bad_alloc();}if(deny>0)--deny;}
}
void *operator new(size_t n){allocating();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void *p)noexcept{std::free(p);}void operator delete[](void *p)noexcept{std::free(p);}
void operator delete(void *p,size_t)noexcept{std::free(p);}void operator delete[](void *p,size_t)noexcept{std::free(p);}
using namespace dome;
namespace {
size_t checks=0;
void check(bool ok,const char *why){++checks;if(!ok)throw std::runtime_error(why);}
template<class F>void refuses(F f,const char *why){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,why);}
struct Memory {
 size_t calls=0,live=0,peak=0;int64_t deny=-1;bool hit=false;
 static void *allocate(void *state,size_t n)noexcept{auto &s=*static_cast<Memory*>(state);++s.calls;if(s.deny==0){s.hit=true;return nullptr;}if(s.deny>0)--s.deny;auto p=std::malloc(n);if(p){++s.live;s.peak=std::max(s.peak,s.live);}return p;}
 static void release(void *state,void *p)noexcept{auto &s=*static_cast<Memory*>(state);--s.live;std::free(p);}
 detail::EmissionExrMemory provider(){return {this,allocate,release};}
};
EmissionInterchangeImage image(uint32_t w,uint32_t h){EmissionInterchangeImage f;f.width=w;f.height=h;f.pixels.resize(size_t(w)*h);for(size_t i=0;i<f.pixels.size();++i){auto &p=f.pixels[i];p[9]=float(i%7)/6;p[3]=p[9]*.07f;p[4]=p[9]*.005f;p[5]=p[9]*.8f;p[6]=float(i%128)/128*50*p[9];p[7]=.0002f*p[9];p[8]=100*p[9];for(size_t k=0;k<3;++k)p[k]=.02f*(1-p[9])+p[3+k]+p[6+k];}return f;}
template<class F>void raw_sweep(F f,const char *name){Memory baseline;f(baseline.provider());check(!baseline.live,"Successful Core operation releases every context allocation");size_t refused=0;for(size_t i=0;i<baseline.calls;++i){Memory m;m.deny=int64_t(i);bool rejected=false;try{f(m.provider());}catch(const std::exception&){rejected=true;}check(m.hit,"Every counted raw malloc-null prefix reached");check(!m.live,"Raw malloc-null releases all context/pipeline blocks");check(rejected,"Raw allocation failure refuses rather than publishes partial output");++refused;}Memory retry;f(retry.provider());check(!retry.live,"Retry after raw failure releases all blocks");std::cout<<name<<" malloc-null prefixes="<<refused<<" peak-live="<<baseline.peak<<std::endl;}
template<class F>void cpp_sweep(F f,const char *name){calls=0;observe=true;f();observe=false;const auto count=calls;size_t refused=0;for(size_t i=0;i<count;++i){deny=int64_t(i);hit=false;bool rejected=false;try{f();}catch(...){rejected=true;}deny=-1;check(hit,"Every counted C++ prefix reached");check(rejected,"C++ allocation failure refuses complete operation");++refused;}f();std::cout<<name<<" C++ prefixes="<<refused<<std::endl;}
template<class F>void cancellations(F f,const char *name){size_t count=0;EmissionInterchangeLimits l;l.cancelled=[&]{++count;return false;};f(l);for(size_t i=0;i<count;++i){size_t p=0;auto cancel=l;cancel.cancelled=[&]{return p++>=i;};bool failed=false;try{f(cancel);}catch(const std::exception &e){failed=std::string_view(e.what()).find("cancelled")!=std::string_view::npos;}check(failed,"Cancellation reaches every callback and returns actual cancellation");}std::cout<<name<<" cancellation points="<<count<<std::endl;}
}
int main()try {
 auto f=image(64,35);const auto initial=f.pixels;auto bytes=detail::encode_emission_exr_owned(f);
 auto legacy_hash=[](const auto &b){uint64_t h=14695981039346656037ull;for(auto v:b)h=(h^v)*1099511628211ull;return h;};
 // Independently captured from the immutable pre-fix high-level writer; the
 // development proof also compares complete bytes against that old archive.
 check(bytes.size()==18014&&legacy_hash(bytes)==16333072579688455284ull,"Legacy encoder byte fingerprint remains exact");check(detail::decode_emission_exr_owned(bytes).pixels==initial,"Owned decoder restores exact ten-plane FLOAT32 fields");
 struct Golden{uint32_t w,h;size_t size;uint64_t hash;};for(auto g:{Golden{1,1,775,700591849390724062ull},{7,16,1957,18035003280282636195ull},{19,17,4423,15456539845958369503ull},{129,67,36004,11183535747692206137ull}}){auto v=image(g.w,g.h);auto b=detail::encode_emission_exr_owned(v);check(b.size()==g.size&&legacy_hash(b)==g.hash,"Legacy byte fingerprints exact across chunk/tiny extents");check(detail::decode_emission_exr_owned(b).pixels==v.pixels,"Multiple chunk extent exact decode");}
 raw_sweep([&](auto p){auto b=detail::encode_emission_exr_owned(f,{},p);check(b==bytes,"Raw-provider writer bytes exact");},"encode");
 raw_sweep([&](auto p){auto out=detail::decode_emission_exr_owned(bytes,{},p);check(out.pixels==initial,"Raw-provider reader floats exact");},"decode");
 raw_sweep([&](auto p){check(detail::emission_exr_owned_decoded_bytes(bytes,{},p)==initial.size()*40,"Raw-provider header reports exact storage");},"header");
 cpp_sweep([&]{check(detail::encode_emission_exr_owned(f)==bytes,"C++ writer exact retry");},"encode");cpp_sweep([&]{check(detail::decode_emission_exr_owned(bytes).pixels==initial,"C++ reader exact retry");},"decode");
 cancellations([&](auto l){detail::encode_emission_exr_owned(f,l);},"encode");cancellations([&](auto l){detail::decode_emission_exr_owned(bytes,l);},"decode");
 EmissionInterchangeLimits exact;exact.max_encoded_bytes=bytes.size();check(detail::encode_emission_exr_owned(f,exact)==bytes,"Actual encoded exact cap accepted below forecast");--exact.max_encoded_bytes;refuses([&]{detail::encode_emission_exr_owned(f,exact);},"One below real encoded size refuses");
 exact={};exact.max_encoded_bytes=bytes.size();check(detail::decode_emission_exr_owned(bytes,exact).pixels==initial,"Exact encoded reader cap accepted");--exact.max_encoded_bytes;refuses([&]{detail::decode_emission_exr_owned(bytes,exact);},"Reader encoded cap one below refuses");
 exact={};exact.max_working_bytes=initial.size()*40+bytes.size();refuses([&]{detail::decode_emission_exr_owned(bytes,exact);},"Decoder charges context scratch in addition to input and pixels");
 exact={};exact.max_pixels=f.pixels.size()-1;refuses([&]{detail::decode_emission_exr_owned(bytes,exact);},"Header dimensions rejected before pixel allocation");
 for(size_t n:{size_t(0),size_t(1),size_t(8),size_t(127),bytes.size()/2,bytes.size()-1})refuses([&]{detail::decode_emission_exr_owned(std::span(bytes).first(n));},"Truncated EXR refuses");
 for(auto [key,value]:{std::pair{"linear-cd/m2","linear-xx/xx"},{"row0=v0","row1=v0"}}){auto bad=bytes;auto begin=std::search(bad.begin(),bad.end(),key,key+std::strlen(key));check(begin!=bad.end(),"Actual metadata marker found");std::memcpy(&*begin,value,std::strlen(key));refuses([&]{detail::decode_emission_exr_owned(bad);},"Foreign units/coordinates cannot borrow ten FLOAT planes");}
 check(f.pixels==initial,"All failure/cancellation paths preserve actual source floats");
 // Nested callbacks must restore the outer call's context allocator, including a
 // nested exceptional operation before the outer pipeline has finished.
 bool nested=false;EmissionInterchangeLimits nesting;nesting.cancelled=[&]{if(!nested){nested=true;Memory m;m.deny=0;refuses([&]{detail::decode_emission_exr_owned(bytes,{},m.provider());},"Nested injected read refuses");check(!m.live,"Nested failed context releases memory");}return false;};check(detail::encode_emission_exr_owned(f,nesting)==bytes,"Nested exception restores outer allocator scope");
 std::cout<<"PASS owning EXR "<<checks<<" checks; legacy bytes="<<bytes.size()<<std::endl;return 0;
}catch(const std::exception &e){deny=-1;observe=false;std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
