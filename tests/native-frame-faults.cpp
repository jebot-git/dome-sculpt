// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_RIG_AFTER_AUTHORING_HELPERS_ONLY
#include "rig-after-authoring.cpp"
#include "dome/shapekey-authoring.hpp"
#include <cstdlib>
#include <new>
namespace {
thread_local bool cpp_fault=false,c_fault=false,hit=false;thread_local size_t calls=0,at=SIZE_MAX;
bool deny(){++calls;if(calls>=at){hit=true;return true;}return false;}
}
extern "C" void *__real_malloc(size_t);
extern "C" void *__wrap_malloc(size_t n){if(c_fault&&deny())return nullptr;return __real_malloc(n);}
namespace {
void*allocate(size_t n,size_t alignment=0){if(cpp_fault&&deny())throw std::bad_alloc();void*p=nullptr;if(alignment){if(posix_memalign(&p,alignment,n?n:1))p=nullptr;}else p=__real_malloc(n?n:1);if(!p)throw std::bad_alloc();return p;}
}
void*operator new(size_t n){return allocate(n);}void*operator new[](size_t n){return allocate(n);}void*operator new(size_t n,std::align_val_t a){return allocate(n,size_t(a));}void*operator new[](size_t n,std::align_val_t a){return allocate(n,size_t(a));}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}void operator delete(void*p,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,std::align_val_t)noexcept{std::free(p);}void operator delete(void*p,size_t,std::align_val_t)noexcept{std::free(p);}
int main(int argc,char**argv)try{
 if(argc!=2&&argc!=4&&argc!=6)return 2;const int selected_mode=argc>=4?std::stoi(argv[2]):-1;const std::string allocator=argc>=4?argv[3]:"all";if(selected_mode>3||selected_mode< -1||(allocator!="all"&&allocator!="cpp"&&allocator!="c"))return 2;const size_t first=argc==6?std::stoull(argv[4]):1,last=argc==6?std::stoull(argv[5]):SIZE_MAX;if(!first||last<first)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);auto input=with_reference(native_case(false));author_skin(input.scene,input.target,input.options);const auto plain=input.scene;ShapeKeyEdit create;create.name="Genuine fault-tested native frame";auto created=plain;author_shapekey(created,input.target,create);ShapeKeyEdit replace;replace.action=ShapeKeyAction::Replace;replace.target=0;replace.positions=created.active().painted->mesh().positions;replace.positions[0].z+=.01f;auto replaced=created;author_shapekey(replaced,input.target,replace);created.save(out/"created.dsculpt");replaced.save(out/"replaced.dsculpt");const auto wire=replaced.encode();demand(wire.at("version")==36&&wire.at("source_materials").at("version")==18,"Real public Create/Replace supplies complete native36 input and paired codec graph");
 for(unsigned mode=0;mode<4;++mode){if(selected_mode>=0&&mode!=unsigned(selected_mode))continue;const auto &seed=mode==0?plain:mode==1?created:replaced;const auto before=seed.encode();const auto expected=mode==0?created.encode():mode==1?replaced.encode():wire;
  auto operate=[&](Scene &current){if(mode==0){author_shapekey(current,input.target,create);return nlohmann::json{};}if(mode==1){author_shapekey(current,input.target,replace);return nlohmann::json{};}if(mode==2)return current.encode();current=Scene::decode(wire);return nlohmann::json{};};
  auto signature=[&](const Scene &current,const nlohmann::json &encoded){return mode==2?encoded:current.encode();};auto warm=seed;auto value=operate(warm);demand(signature(warm,value)==expected,"Real frame factory/codec warm result exactly matches authoritative complete endpoint");
  for(bool native_c:{false,true}){if((allocator=="cpp"&&native_c)||(allocator=="c"&&!native_c))continue;
   auto measured=seed;calls=0;at=SIZE_MAX;hit=false;cpp_fault=!native_c;c_fault=native_c;auto result=operate(measured);cpp_fault=c_fault=false;const auto count=calls;demand((native_c||count>0)&&count<32768&&signature(measured,result)==expected,"Measure bounded actual factory/codec allocator boundaries without test preparation noise");if(count&&first>count)throw std::invalid_argument("Requested fault range does not reach an actual measured prefix");size_t refused=0,exact_optional=0;
   for(size_t prefix=first;prefix<=std::min(count,last);++prefix){auto current=seed;calls=0;at=prefix;hit=false;bool caught=false;cpp_fault=!native_c;c_fault=native_c;nlohmann::json encoded;try{encoded=operate(current);}catch(const std::bad_alloc&){caught=true;}catch(const std::exception&){if(!native_c){cpp_fault=c_fault=false;throw;}caught=true;}cpp_fault=c_fault=false;
    if(!hit)throw std::runtime_error("Measured allocator prefix was not reached");if(caught){if(current.encode()!=before)throw std::runtime_error("Actual public frame/codec failure partially changed caller Scene");++refused;}else{if(!native_c||signature(current,encoded)!=expected)throw std::runtime_error("Allocator failure changed output or silently altered C++ factory behavior");++exact_optional;}
    auto retry=seed;auto retry_encoded=operate(retry);if(signature(retry,retry_encoded)!=expected||seed.encode()!=before||wire!=replaced.encode())throw std::runtime_error("Frame/codec failure changed immutable source/wire or exact retry");
   }
   std::cout<<"FRAME_FAULTS mode="<<mode<<" allocator="<<(native_c?"C-malloc":"C++")<<" measured="<<count<<" first="<<first<<" last="<<std::min(count,last)<<" refused="<<refused<<" exact_optional="<<exact_optional<<std::endl;
  }
 }
 demand(Scene::load(out/"created.dsculpt").encode()==created.encode()&&Scene::load(out/"replaced.dsculpt").encode()==wire,"All actual factory/codec denials retain both complete persisted native endpoints");std::cout<<"PASS native_frame_fault_checks="<<rig_checks<<std::endl;return 0;
}catch(const std::exception&e){cpp_fault=c_fault=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
