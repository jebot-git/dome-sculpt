// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include "dome/shapekey-authoring.hpp"
#include "normal-key-derivation.hpp"
#include <iostream>
#include <cstdlib>
#include <new>
#include <charconv>
#include <fstream>
#include <thread>
#include "humanoid-sha256.hpp"
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

using namespace dome;
namespace {
void sweep(const Scene &source,const ShapeKeyEdit &edit,const char *label,bool measure){
 const auto exact=source.encode();size_t baseline=0;Scene expected;
 for(size_t pass=0;pass<2;++pass){auto candidate=source;size_t count=0;{allocation_probe::Guard guard(SIZE_MAX);author_shapekey(candidate,candidate.active().id,edit);count=allocation_probe::calls;}baseline=std::max(baseline,count);if(!pass)expected=std::move(candidate);else if(candidate.encode()!=expected.encode())throw std::runtime_error("Repeated actual key factory differs before fault injection");}
 const auto positive=expected.encode();std::cout<<"KEY_CPP_BASELINE "<<label<<" "<<baseline<<std::endl;if(measure)return;
 size_t denied=0,success=0;
 for(size_t cut=0;cut<=baseline;++cut){auto candidate=source;bool failed=false;size_t calls=0;{allocation_probe::Guard guard(cut);try{author_shapekey(candidate,candidate.active().id,edit);}catch(const std::bad_alloc&){failed=true;}calls=allocation_probe::calls;}
  if(failed){if(calls<=cut||candidate.encode()!=exact||source.encode()!=exact)throw std::runtime_error(std::string("Actual key persistent allocation denial changed complete caller: ")+label+" prefix "+std::to_string(cut));++denied;}
  else{if(candidate.encode()!=positive||source.encode()!=exact)throw std::runtime_error(std::string("Actual key allocation success differs from exact complete reference: ")+label+" prefix "+std::to_string(cut));++success;}
  if(cut==0||cut==baseline/2||cut==baseline){auto retry=source;author_shapekey(retry,retry.active().id,edit);if(retry.encode()!=positive)throw std::runtime_error("Persistent allocation context retry differs");}
  if(cut%250==0)std::cout<<"KEY_CPP_PREFIX "<<label<<" "<<cut<<std::endl;
 }
 if(!denied||!success)throw std::runtime_error("Persistent factory allocation cohort lacks denial or success endpoint");std::cout<<"PASS KEY_CPP_PREFIXES "<<label<<" denied="<<denied<<" successful="<<success<<" baseline="<<baseline<<std::endl;
}
}
namespace {
size_t allocation_prefix(std::string_view value){size_t n=0;const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),n);if(value.empty()||error!=std::errc{}||end!=value.data()+value.size())throw std::invalid_argument("Invalid absolute key allocation prefix");return n;}
struct ColdKeyAttempt{size_t calls=0;bool failed=false;std::exception_ptr error;};
ColdKeyAttempt cold_key_attempt(Scene &candidate,const ShapeKeyEdit &edit,size_t cut){
 ColdKeyAttempt result;std::thread caller([&]{{allocation_probe::Guard guard(cut);try{author_shapekey(candidate,candidate.active().id,edit);}catch(const std::bad_alloc&){result.failed=true;}catch(...){result.error=std::current_exception();}result.calls=allocation_probe::calls;}});caller.join();if(result.error)std::rethrow_exception(result.error);return result;
}
int partitioned_key(int argc,char**argv){
 const bool measure=std::string_view(argv[3])=="--oom-measure";if((measure&&argc!=5)||(!measure&&argc!=8))return 2;const std::string operation=argv[measure?4:7];if(operation!="Create"&&operation!="Replace")throw std::invalid_argument("Unknown actual key operation");const std::filesystem::path out=argv[2];if(std::filesystem::exists(out))throw std::invalid_argument("Key partition output exists; preserve original evidence");std::filesystem::create_directories(out);
 auto source=Scene::load(argv[1]);ShapeKeyEdit edit;edit.name="Persistent current37 new key";if(operation=="Replace"){author_shapekey(source,source.active().id,edit);edit.action=ShapeKeyAction::Replace;edit.target=source.active().deformation->morphs->targets.size()-1;edit.positions=source.active().painted->mesh().positions;edit.positions.at(0).z+=.007f;edit.positions.at(3).z-=.006f;}
 const auto exact=source.encode();nlohmann::json positive;std::vector<size_t> counts;for(size_t pass=0;pass<3;++pass){auto candidate=source;const auto measured=cold_key_attempt(candidate,edit,SIZE_MAX);if(measured.failed)throw std::runtime_error("Uninjected fresh caller failed");counts.push_back(measured.calls);if(!pass)positive=candidate.encode();else if(candidate.encode()!=positive)throw std::runtime_error("Repeated actual key result differs");if(source.encode()!=exact)throw std::runtime_error("Actual key measurement changed complete source");}
 const size_t baseline=counts.front();const bool stable=std::all_of(counts.begin(),counts.end(),[&](auto n){return n==baseline;});std::ifstream fixture_file(argv[1],std::ios::binary);const std::vector<uint8_t> fixture{std::istreambuf_iterator<char>(fixture_file),{}};if(!fixture_file.eof()&&fixture_file.fail())throw std::runtime_error("Cannot hash input fixture");
 nlohmann::json report{{"format",1},{"mode",measure?"measure":"range"},{"operation",operation},{"caller","fresh-sequential-thread"},{"baseline",baseline},{"baseline_counts",counts},{"stable_baseline",stable},{"fixture_sha256",creator_detail::sha256(fixture)},{"scene_sha256",creator_detail::sha256(nlohmann::json::to_cbor(exact))},{"baseline_result_sha256",creator_detail::sha256(nlohmann::json::to_cbor(positive))},{"cache_budget",paint_composite_cache_stats().limit_bytes}};
#ifdef __SANITIZE_ADDRESS__
 report["profile"]="asan-ubsan";
#else
 report["profile"]="release";
#endif
 if(!stable){std::ofstream diagnostic(out/"unstable-calibration.json");diagnostic<<report.dump(2)<<'\n';std::cerr<<"KEY_UNSTABLE_COUNTS "<<nlohmann::json(counts)<<std::endl;throw std::runtime_error("Actual key allocation baseline differs across three same-input calls");}
 if(measure){report["terminal_exit"]=0;}else{
  const size_t begin=allocation_prefix(argv[4]),end=allocation_prefix(argv[5]);if(allocation_prefix(argv[6])!=baseline||begin>=end||end>baseline+1)throw std::invalid_argument("Key partition differs from measured absolute interval");size_t denied=0,successful=0;bool endpoint=false;std::vector<size_t> retries;
  for(size_t cut=begin;cut<end;++cut){auto candidate=source;const auto attempt=cold_key_attempt(candidate,edit,cut);const bool failed=attempt.failed;const size_t calls=attempt.calls;
   if(failed){if(calls<=cut||candidate.encode()!=exact||source.encode()!=exact)throw std::runtime_error("Actual key persistent denial changed complete caller: "+operation+" prefix "+std::to_string(cut));++denied;}
   else{if(candidate.encode()!=positive||source.encode()!=exact)throw std::runtime_error("Actual key allocation success differs from exact complete reference: "+operation+" prefix "+std::to_string(cut));++successful;if(cut==baseline)endpoint=true;}
   if(cut==0||cut==baseline/2||cut==baseline){auto retry=source;if(cold_key_attempt(retry,edit,SIZE_MAX).failed)throw std::runtime_error("Uninjected fresh caller retry failed");if(retry.encode()!=positive)throw std::runtime_error("Persistent allocation context retry differs");retries.push_back(cut);}
   if(cut%250==0)std::cout<<"KEY_CPP_PREFIX "<<operation<<" "<<cut<<std::endl;
  }
  if(denied+successful!=end-begin||source.encode()!=exact)throw std::runtime_error("Key partition is incomplete or changed source");report.update({{"begin",begin},{"end",end},{"completed",denied+successful},{"denied",denied},{"successful",successful},{"translated",0},{"endpoint_successful",endpoint},{"retried_prefixes",retries},{"first_completed",begin},{"last_completed",end-1},{"terminal_exit",0}});
 }
 std::ofstream output(out/(measure?"measurement.json":"range.json"));output.exceptions(std::ios::badbit|std::ios::failbit);output<<report.dump(2)<<'\n';std::cout<<"KEY_PARTITION "<<operation<<" baseline="<<baseline<<" mode="<<(measure?"measure":"range")<<" terminal0\n";return 0;
}
}

int main(int argc,char **argv)try{
 if(argc>=4&&(std::string_view(argv[3])=="--oom-measure"||std::string_view(argv[3])=="--oom-range"))return partitioned_key(argc,argv);
 if(argc<2||argc>3)return 2;const bool measure=argc==3&&std::string_view(argv[2])=="--measure";auto source=Scene::load(argv[1]);const auto exact=source.encode();ShapeKeyEdit create;create.name="Persistent current37 new key";sweep(source,create,"Create",measure);author_shapekey(source,source.active().id,create);ShapeKeyEdit replace;replace.action=ShapeKeyAction::Replace;replace.target=source.active().deformation->morphs->targets.size()-1;replace.positions=source.active().painted->mesh().positions;replace.positions.at(0).z+=.007f;replace.positions.at(3).z-=.006f;sweep(source,replace,"Replace",measure);std::cout<<"PASS actual post37 persistent key factory controls\n";return 0;
}catch(const std::exception &e){allocation_probe::active=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
