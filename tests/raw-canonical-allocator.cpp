// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-paint-fixtures.hpp"
#include "raw-source-canonical.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <new>
namespace probe {
thread_local bool active=false,new_failure=false,malloc_failure=false;
thread_local size_t allocations=0,news=0,live=0,new_limit=SIZE_MAX,malloc_limit=SIZE_MAX;
thread_local std::array<void*,65536> owners{};
struct Scope {
  Scope(size_t n=SIZE_MAX,size_t m=SIZE_MAX){allocations=news=live=0;new_limit=n;malloc_limit=m;new_failure=malloc_failure=false;active=true;}
  ~Scope(){active=false;}
};
void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
}
extern "C" void *__real_malloc(size_t);
extern "C" void __real_free(void*);
extern "C" void *__wrap_malloc(size_t n){
  if(probe::active&&++probe::allocations>=probe::malloc_limit){probe::malloc_failure=true;return nullptr;}
  auto *p=__real_malloc(n);
  if(probe::active&&p){if(probe::live==probe::owners.size()){__real_free(p);return nullptr;}probe::owners[probe::live++]=p;}
  return p;
}
extern "C" void __wrap_free(void *p){
  if(probe::active&&p)for(size_t n=0;n<probe::live;++n)if(probe::owners[n]==p){probe::owners[n]=probe::owners[--probe::live];break;}
  __real_free(p);
}
void *operator new(size_t n){
  if(probe::active&&++probe::news>=probe::new_limit){probe::new_failure=true;throw std::bad_alloc();}
  if(auto *p=std::malloc(n?n:1))return p;throw std::bad_alloc();
}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}
void operator delete[](void*p,size_t)noexcept{std::free(p);}
void *operator new(size_t n,const std::nothrow_t&)noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
void *operator new[](size_t n,const std::nothrow_t&)noexcept{try{return ::operator new[](n);}catch(...){return nullptr;}}
void operator delete(void*p,const std::nothrow_t&)noexcept{std::free(p);}
void operator delete[](void*p,const std::nothrow_t&)noexcept{std::free(p);}
int main(int argc,char **argv)try{
  using namespace dome;using probe::require;
  if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
  auto input=source_paint_fixture::input(0);const auto path=out/"source.glb";input.save(path);
  Scene initial;initial.active().visible=false;ImportAuthoringOptions options;options.paint_resolution=64;
  auto scene=prepare_authoring_import(initial,path,options).scene;const auto before=scene.encode();
  const auto appearance=scene.active().appearance;const std::function<bool()> never=[] {return false;};
  size_t mallocs=0,news=0;
  {probe::Scope scope;{detail::RawSourceCanonical parsed(*appearance,never);}mallocs=probe::allocations;news=probe::news;}
  require(!probe::live&&mallocs&&news&&mallocs<65536&&news<65536,"Bounded real parser allocation/free baseline");
  for(int mode=0;mode<2;++mode){const auto count=mode?mallocs:news;
    for(size_t cut=1;cut<=count;++cut){bool refused=false;
      {probe::Scope scope(mode?SIZE_MAX:cut,mode?cut:SIZE_MAX);
        try{detail::RawSourceCanonical parsed(*appearance,never);}catch(const std::bad_alloc&){refused=true;}}
      require(refused&&(mode?probe::malloc_failure:probe::new_failure)&&!probe::live,
          "Persistent parser new/malloc-null failure unwinds every partial allocation");
      require(scene.encode()==before,"Parser denial preserves the full actual source/native Scene");
      {probe::Scope scope;{detail::RawSourceCanonical parsed(*appearance,never);}}
      require(!probe::live,"Parser retry normally frees the complete successful cgltf model");
    }
    std::cout<<"PASS parser "<<(mode?"malloc-null":"C++new")<<" persistent boundaries="<<count<<'\n';
  }
  size_t polls=0;const std::function<bool()> counted=[&]{++polls;return false;};
  {detail::RawSourceCanonical parsed(*appearance,counted);}const auto count=polls;require(count>1,"Actual parser cancellation polls measured");
  for(size_t cut=1;cut<=count;++cut){polls=0;const std::function<bool()> cancelled=[&]{return ++polls==cut;};bool refused=false;
    {probe::Scope scope;try{detail::RawSourceCanonical parsed(*appearance,cancelled);}catch(const std::runtime_error&){refused=true;}}
    require(refused&&polls==cut&&!probe::live&&scene.encode()==before,"Every parser cancellation frees partial allocations, preserves source and permits clean retry");
    {detail::RawSourceCanonical parsed(*appearance,never);}
  }
  polls=0;const std::function<bool()> throwing=[&]()->bool{if(++polls==count/2)throw std::runtime_error("Actual parser callback failure");return false;};bool threw=false;
  {probe::Scope scope;try{detail::RawSourceCanonical parsed(*appearance,throwing);}catch(const std::runtime_error&e){threw=std::string_view(e.what())=="Actual parser callback failure";}}
  require(threw&&!probe::live&&scene.encode()==before,"Callback exceptions cross the owned C++ parser and release every partial allocation");
  {detail::RawSourceCanonical parsed(*appearance,never);}
  std::cout<<"PASS parser cancellation boundaries="<<count<<"; source exact\n";return 0;
}catch(const std::exception&e){probe::active=false;std::cerr<<e.what()<<'\n';return 1;}
