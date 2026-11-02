// SPDX-License-Identifier: GPL-3.0-or-later
// Vendor-API SHIMS: execute the production host transaction, NOT GPU arithmetic.
#include "gpu-brush-transfer.hpp"
#include <array>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
namespace allocation_probe {thread_local bool deny=false;}
void *operator new(size_t n){if(allocation_probe::deny)throw std::bad_alloc();if(void*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}
using namespace dome;
namespace {
unsigned checks=0;
void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);++checks;}
enum class Stage {None,AllocateInput,AllocateOutput,Upload,Launch,Wait,Readback};
struct VendorError:std::exception {Stage stage;int chunk;VendorError(Stage s,int c):stage(s),chunk(c){}const char*what()const noexcept override{return "simulated vendor API error";}};
struct Fault {Stage stage=Stage::None;int chunk=1;};
struct Stats {size_t allocations=0,releases=0,uploaded=0,downloaded=0,max_input=0,max_output=0,launches=0,waits=0,drains=0,deferred=0;};
// The shim models allocations/copies, deferred device work and an error after
// partially writing HOST output. Synthetic scalar output is a sentinel only.
struct ApiShim {
 std::span<Sample> caller;std::span<const Sample> before;Brush expected;Fault fault;Stats stats;bool protocol=true,all_reads=false;int chunk=0;size_t uploaded_samples=0,read_samples=0;
 std::array<void*,2> pointers{};const Sample *pending_input=nullptr;float *pending_output=nullptr;size_t pending_count=0;float *pending_host=nullptr;float deferred_value=0;unsigned char *pending_device=nullptr;unsigned char device_value=0;
 ApiShim(std::span<Sample> c,std::span<const Sample>b,Brush brush,Fault f={}):caller(c),before(b),expected(brush),fault(f){}
 void observe(){if(!all_reads&&std::memcmp(caller.data(),before.data(),caller.size_bytes()))protocol=false;}
 bool fails(Stage s)const{return fault.stage==s&&fault.chunk==chunk;}
 void allocate(void **p,size_t bytes){observe();size_t slot=stats.allocations++;if(slot>=2){protocol=false;throw VendorError(Stage::AllocateInput,0);}*p=std::malloc(bytes);if(!*p)throw std::bad_alloc();pointers[slot]=*p;if(slot==0)stats.max_input=bytes;else stats.max_output=bytes;
  if(fault.stage==(slot?Stage::AllocateOutput:Stage::AllocateInput))throw VendorError(fault.stage,0);
 }
 void release(void *p)noexcept{observe();if(pending_input||pending_host||pending_device)protocol=false;bool found=false;for(auto &q:pointers)if(q==p){q=nullptr;found=true;}if(!found)protocol=false;++stats.releases;std::free(p);}
 void upload(void *target,const void *source,size_t bytes){observe();++chunk;if(target!=pointers[0]||bytes%sizeof(Sample)||bytes>stats.max_input||source!=caller.data()+uploaded_samples)protocol=false;
  std::memcpy(target,source,bytes);stats.uploaded+=bytes;uploaded_samples+=bytes/sizeof(Sample);
  if(fails(Stage::Upload)){pending_device=static_cast<unsigned char*>(target)+bytes-1;device_value=static_cast<const unsigned char*>(source)[bytes-1];throw VendorError(Stage::Upload,chunk);}
 }
 void launch(const Sample *input,float *output,size_t count,const Brush &brush){observe();++stats.launches;
  if(input!=pointers[0]||output!=pointers[1]||count*sizeof(Sample)>stats.max_input||count*sizeof(float)>stats.max_output||pending_input)protocol=false;
  if(std::memcmp(&brush.from,&expected.from,sizeof(Vec3))||std::memcmp(&brush.to,&expected.to,sizeof(Vec3))||std::memcmp(&brush.normal,&expected.normal,sizeof(Vec3))||std::memcmp(&brush.symmetry_origin,&expected.symmetry_origin,sizeof(Vec3))||brush.radius!=expected.radius||brush.strength!=expected.strength||brush.pressure!=expected.pressure||brush.blend!=expected.blend||brush.band!=expected.band||brush.tool!=expected.tool||brush.operation!=expected.operation||brush.symmetry_axes!=expected.symmetry_axes)protocol=false;
  pending_input=input;pending_output=output;pending_count=count;if(fails(Stage::Launch))throw VendorError(Stage::Launch,chunk);
 }
 void finish_kernel()noexcept{if(pending_input){for(size_t i=0;i<pending_count;++i)pending_output[i]=pending_input[i].distance+.125f;pending_input=nullptr;pending_output=nullptr;}}
 void wait(){observe();++stats.waits;if(fails(Stage::Wait))throw VendorError(Stage::Wait,chunk);finish_kernel();}
 void readback(void *target,const void *source,size_t bytes){observe();auto address=reinterpret_cast<uintptr_t>(target),begin=reinterpret_cast<uintptr_t>(caller.data());
  if(source!=pointers[1]||pending_input||bytes%sizeof(float)||bytes>stats.max_output||(address<begin+caller.size_bytes()&&address+bytes>begin))protocol=false;
  if(fails(Stage::Readback)){const size_t prefix=std::min(bytes,size_t(3*sizeof(float)));std::memcpy(target,source,prefix);if(bytes>prefix){pending_host=static_cast<float*>(target)+3;deferred_value=static_cast<const float*>(source)[3];}throw VendorError(Stage::Readback,chunk);}
  std::memcpy(target,source,bytes);stats.downloaded+=bytes;read_samples+=bytes/sizeof(float);all_reads=read_samples==caller.size();
 }
 void drain()noexcept{observe();++stats.drains;finish_kernel();if(pending_host){*pending_host=deferred_value;pending_host=nullptr;++stats.deferred;}if(pending_device){*pending_device=device_value;pending_device=nullptr;++stats.deferred;}}
};
struct CudaApiShim:ApiShim{using ApiShim::ApiShim;static constexpr const char *name="CUDA API SHIM";};
struct HipApiShim:ApiShim{using ApiShim::ApiShim;static constexpr const char *name="HIP API SHIM";};
std::vector<Sample> values(size_t n){std::vector<Sample> out(n);for(size_t i=0;i<n;++i){auto &s=out[i];s.position={std::bit_cast<float>(uint32_t(0x7fc00000u|(i&0xffffu))),-0.f,float(i%31)};s.distance=float(i%127)*.001f;s.average=std::bit_cast<float>(uint32_t(0x7fc10000u|(i&0xffffu)));s.influence=i%2?-.0f:.37f;}return out;}
bool exact(std::span<const Sample>a,std::span<const Sample>b){return a.size()==b.size()&&!std::memcmp(a.data(),b.data(),a.size_bytes());}
template<class Api>void suite(){
 Brush b;b.from={.01f,.02f,.03f};b.to={-.2f,.12f,.34f};b.normal={1,0,0};b.radius=.23f;b.strength=.47f;b.pressure=.81f;b.blend=.03f;b.band=.4f;b.tool=Tool::Flatten;b.operation=BrushOperation::MaskSmooth;b.symmetry_axes=7;b.symmetry_origin={-.1f,.3f,.02f};
 {Api api({}, {},b);gpu_brush::apply({},b,api);check(api.stats.allocations==0&&api.stats.launches==0,"Empty span unexpectedly touches vendor runtime");}
 const size_t limit=gpu_brush::chunk_samples;
 for(size_t count:{size_t(1),limit,limit+7,2*limit+7}){
  auto sample=values(count),before=sample;Api api(sample,before,b);gpu_brush::apply(sample,b,api);auto expected=before;for(auto&s:expected)s.distance+=.125f;
  check(exact(sample,expected)&&api.protocol,"Successful scalar publication changed immutable host fields or brush arguments");check(api.stats.uploaded==count*24&&api.stats.downloaded==count*4,"Transfer is not24-byte upload/4-byte scalar readback");check(api.stats.max_input==std::min(count,limit)*24&&api.stats.max_output==std::min(count,limit)*4&&api.stats.launches==(count+limit-1)/limit,"Device storage or dispatch exceeds exact bounded chunk contract");check(api.stats.releases==2&&api.stats.drains==0,"Successful transaction leaks buffers or performs error recovery");
 }
 const size_t count=2*limit+7;auto baseline=values(count);
 for(auto fault:std::array<Fault,11>{{{Stage::AllocateInput,1},{Stage::AllocateOutput,1},{Stage::Upload,1},{Stage::Launch,1},{Stage::Wait,1},{Stage::Readback,1},{Stage::Upload,2},{Stage::Launch,2},{Stage::Wait,2},{Stage::Readback,2},{Stage::Readback,3}}}){
  auto sample=baseline;Api api(sample,baseline,b,fault);bool refused=false;try{gpu_brush::apply(sample,b,api);}catch(const VendorError&e){refused=e.stage==fault.stage;}
  check(refused&&exact(sample,baseline)&&api.protocol,"Failed/partial later-chunk transfer changed original host bytes or lost exact error");check(api.stats.releases==api.stats.allocations,"Failed allocation/dispatch leaks a partially allocated buffer");
  const bool allocated=fault.stage==Stage::AllocateInput||fault.stage==Stage::AllocateOutput;check(api.stats.drains==size_t(!allocated),"Failed in-flight work was not drained before staging/buffers unwind");if(fault.stage==Stage::Readback)check(api.stats.deferred==1,"Partial host readback did not complete into still-live detached staging during drain");
  Api retry(sample,baseline,b);gpu_brush::apply(sample,b,retry);auto expected=baseline;for(auto&s:expected)s.distance+=.125f;check(exact(sample,expected)&&retry.protocol,"Fresh same-input retry did not retain exact output after failed transaction");
 }
 {auto sample=values(7),before=sample;Api api(sample,before,b);bool refused=false;allocation_probe::deny=true;try{gpu_brush::apply(sample,b,api);}catch(const std::bad_alloc&){refused=true;}allocation_probe::deny=false;check(refused&&exact(sample,before)&&api.stats.allocations==0,"Host result allocation failure reached vendor runtime or mutated caller");}
 std::cout<<"PASS "<<Api::name<<" real production host-control-flow; synthetic sentinel kernel only, no GPU numerical claim\n";
}
}
int main()try{suite<CudaApiShim>();suite<HipApiShim>();std::cout<<"PASS "<<checks<<" vendor-API shim transport/atomicity checks\n";return 0;}catch(const std::exception&e){allocation_probe::deny=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
