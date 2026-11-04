// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_NATIVE_MTOON_PHYSICAL_NO_MAIN
#include "native-mtoon-physical.cpp"
#undef DOME_NATIVE_MTOON_PHYSICAL_NO_MAIN
namespace allocation_probe {
thread_local bool active=false;thread_local size_t calls=0,limit=SIZE_MAX;
void hit(){if(active&&calls++>=limit)throw std::bad_alloc();}
struct Guard{explicit Guard(size_t at){calls=0;limit=at;active=true;}~Guard(){active=false;}};
}
void *operator new(size_t n){allocation_probe::hit();if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void *p)noexcept{std::free(p);}
void operator delete[](void *p)noexcept{std::free(p);}
void operator delete(void *p,size_t)noexcept{std::free(p);}
void operator delete[](void *p,size_t)noexcept{std::free(p);}
void *operator new(size_t n,std::align_val_t a){allocation_probe::hit();void *p=nullptr;if(const auto error=posix_memalign(&p,std::max(size_t(a),sizeof(void*)),n?n:1)){std::fprintf(stderr,"ALIGNED_NEW_FAILURE bytes=%zu alignment=%zu error=%d\n",n,size_t(a),error);throw std::bad_alloc();}return p;}
void *operator new[](size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void *p,std::align_val_t)noexcept{std::free(p);}
void operator delete[](void *p,std::align_val_t)noexcept{std::free(p);}
void operator delete(void *p,size_t,std::align_val_t)noexcept{std::free(p);}
void operator delete[](void *p,size_t,std::align_val_t)noexcept{std::free(p);}
void *operator new(size_t n,const std::nothrow_t&)noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
void *operator new[](size_t n,const std::nothrow_t&)noexcept{try{return ::operator new[](n);}catch(...){return nullptr;}}
void *operator new(size_t n,std::align_val_t a,const std::nothrow_t&)noexcept{try{return ::operator new(n,a);}catch(...){return nullptr;}}
void *operator new[](size_t n,std::align_val_t a,const std::nothrow_t&)noexcept{try{return ::operator new[](n,a);}catch(...){return nullptr;}}
void operator delete(void*p,const std::nothrow_t&)noexcept{std::free(p);}
void operator delete[](void*p,const std::nothrow_t&)noexcept{std::free(p);}
void operator delete(void*p,std::align_val_t,const std::nothrow_t&)noexcept{std::free(p);}
void operator delete[](void*p,std::align_val_t,const std::nothrow_t&)noexcept{std::free(p);}

#include "emission-paint-access.hpp"
#include "paint-overlay-packet.hpp"
namespace {
bool same_owner(const auto&a,const auto&b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);}
bool same_maps(const TextureMaps&a,const TextureMaps&b){
 const std::array<const Image*,10> left{&a.color,&a.metal_rough,&a.emission,&a.color_coverage,&a.normal,&a.baked_normal,&a.baked_coverage,&a.emission_coverage,&a.pbr_color,&a.mtoon_normal},right{&b.color,&b.metal_rough,&b.emission,&b.color_coverage,&b.normal,&b.baked_normal,&b.baked_coverage,&b.emission_coverage,&b.pbr_color,&b.mtoon_normal};
 for(size_t i=0;i<left.size();++i)if(left[i]->width!=right[i]->width||left[i]->height!=right[i]->height||left[i]->rgba!=right[i]->rgba)return false;
 return a.transparent==b.transparent&&a.pbr_transparent==b.pbr_transparent&&a.source_only==b.source_only&&same_owner(a.source_maps,b.source_maps)&&same_owner(a.baked_base,b.baked_base)&&same_owner(a.color_projection,b.color_projection)&&same_owner(a.normal_projection,b.normal_projection)&&a.overlay&&b.overlay&&same_owner(a.overlay->native_geometry(),b.overlay->native_geometry())&&same_owner(a.overlay->native_domain(),b.overlay->native_domain())&&detail::overlay_uses_float_emission(*a.overlay)==detail::overlay_uses_float_emission(*b.overlay);
}
}
int main()try{
 set_paint_composite_cache_budget(0);
 for(bool visible:{false,true}){
  auto scene=native_legacy_emission(false);hidden_physical(scene);
  if(visible)scene.active().painted->layer_properties("Hidden physical",true,.15f);
  const auto original=scene.active().painted;const auto mesh=original->mesh_owner();const auto exact=scene.encode();
  auto initial=detail::EmissionExportAccess::preview(*original,scene.active().material,{});TextureMaps expected=*initial;initial.reset();
  size_t baseline=0;{allocation_probe::Guard guard(SIZE_MAX);auto maps=detail::EmissionExportAccess::preview(*original,scene.active().material,{});baseline=allocation_probe::calls;}
  std::cout<<"HIDDEN_PREVIEW_BASELINE visible="<<visible<<" calls="<<baseline<<std::endl;
  size_t denied=0;bool succeeded=false;
  for(size_t prefix=0;prefix<=baseline+128;++prefix){bool failed=false;std::shared_ptr<const TextureMaps> result;
   {allocation_probe::Guard guard(prefix);try{result=detail::EmissionExportAccess::preview(*original,scene.active().material,{});}catch(const std::bad_alloc&){failed=true;}}
   if(scene.active().painted!=original||!same_owner(original->mesh_owner(),mesh)||scene.encode()!=exact)throw std::runtime_error("Preview denial changed complete native input/owners");
   if(!failed){if(!same_maps(expected,*result))throw std::runtime_error("Successful preview changed exact maps or native owner");succeeded=true;break;}
   ++denied;auto retry=detail::EmissionExportAccess::preview(*original,scene.active().material,{});if(!same_maps(expected,*retry))throw std::runtime_error("Preview retry changed exact maps or native owner");
   if(prefix%100==0)std::cout<<"HIDDEN_PREVIEW_PREFIX visible="<<visible<<" prefix="<<prefix<<std::endl;
  }
  check(succeeded&&denied>0,"Every actual native preview allocation-denial prefix preserves complete input and retries exact maps");
  std::cout<<"HIDDEN_PREVIEW_DENIED visible="<<visible<<" prefixes="<<denied<<" baseline="<<baseline<<std::endl;
  size_t polls=0;{auto maps=detail::EmissionExportAccess::preview(*original,scene.active().material,[&]{++polls;return false;});}
  for(size_t at:{size_t(1),polls/4,polls/2,polls*3/4,polls}){size_t calls=0;bool refused=false;try{auto maps=detail::EmissionExportAccess::preview(*original,scene.active().material,[&]{return ++calls>=at;});}catch(const std::exception&e){refused=std::string_view(e.what()).find("cancelled")!=std::string_view::npos;}
   check(refused&&scene.encode()==exact,"Actual native preview cancellation leaves exact complete scene");auto retry=detail::EmissionExportAccess::preview(*original,scene.active().material,{});check(same_maps(expected,*retry),"Cancelled native preview retries exact maps/owners");}
 }
 std::cout<<"PASS "<<checks<<" hidden preview cleanup controls\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
