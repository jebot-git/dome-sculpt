// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_CANONICAL_CONVERSION_HELPERS_ONLY
#include "canonical-normal-conversion.cpp"
#include <cstdlib>
#include <new>
namespace conversion_fault {
thread_local bool enabled=false,hit=false;
thread_local size_t calls=0,at=SIZE_MAX;
void visit(){if(enabled&&++calls>=at){hit=true;throw std::bad_alloc();}}
}
[[gnu::noinline]] void *operator new(size_t n){conversion_fault::visit();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
void *operator new(size_t n,std::align_val_t a){conversion_fault::visit();void*p=nullptr;if(posix_memalign(&p,size_t(a),n?n:1))throw std::bad_alloc();return p;}
void *operator new[](size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void*p,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,std::align_val_t)noexcept{std::free(p);}
void operator delete(void*p,size_t,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,size_t,std::align_val_t)noexcept{std::free(p);}
int main()try{
 auto scene=baked_fan(true,true,true);const auto exact=scene.encode();const auto paint=scene.active().painted;const auto deformation=scene.active().deformation;
 ColorPreparationInput input{paint,scene.active().appearance,scene.active().material_bindings,deformation,scene.active().material};
 auto produce=[&]{return detail::convert_native_canonical_normals(input);};
 // Warm immutable composite caches before measuring this conversion's actual
 // persistent C++ allocation prefixes. No C malloc/Mikk sweep is claimed.
 auto warm=produce();conversion_fault::calls=0;conversion_fault::at=SIZE_MAX;conversion_fault::enabled=true;
 auto positive=produce();conversion_fault::enabled=false;const auto count=conversion_fault::calls;
 auto signature=[](const auto &value){NormalFrameArchive a;a.add(value.target_frames);return nlohmann::json{{"frames",a.encode()},{"pbr",value.projection->pbr->rgba},{"mtoon",value.projection->mtoon->rgba},{"centers",value.sampled_centers},{"raster",value.raster_samples},{"error",value.maximum_center_angular_error}};};
 const auto expected=signature(positive);require_cage(count>10&&count<4096,"Actual conversion persistent allocation prefixes are measured after stable native source cache warmup");
 for(size_t at=1;at<=count;++at){conversion_fault::calls=0;conversion_fault::at=at;conversion_fault::hit=false;bool caught=false;conversion_fault::enabled=true;
  try{produce();}catch(const std::bad_alloc&){caught=true;}conversion_fault::enabled=false;
  if(!caught||!conversion_fault::hit||scene.encode()!=exact||scene.active().painted!=paint||scene.active().deformation!=deformation)
   throw std::runtime_error("Persistent conversion allocation failure changed complete original data or missed measured boundary");
  if(signature(produce())!=expected)throw std::runtime_error("Conversion allocation retry differs from exact frame/image/quality result");
 }
 size_t polls=0;NormalPreparationOptions o;o.cancelled=[&]{++polls;return false;};detail::convert_native_canonical_normals(input,o);
 for(size_t cut=1;cut<=polls;++cut){size_t at=0;o.cancelled=[&]{return ++at>=cut;};if(!failure([&]{detail::convert_native_canonical_normals(input,o);},"cancel")||scene.encode()!=exact)throw std::runtime_error("Conversion cancellation poll failed original-data preservation");}
 require_cage(signature(produce())==expected,"Every measured cancellation endpoint retries to exact complete candidate data");
 std::cout<<"PASS conversion_persistent_cpp_prefixes="<<count<<" all_cancel_polls="<<polls<<'\n';return 0;
}catch(const std::exception&e){conversion_fault::enabled=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
