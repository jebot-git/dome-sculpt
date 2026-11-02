// SPDX-License-Identifier: GPL-3.0-or-later
// Actual vendor runtime/module probe; no API shim or CPU backend is linked.
#include "dome/brush.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
#if defined(__HIPCC__)
#include <hip/hip_runtime_api.h>
#define GPU_COUNT hipGetDeviceCount
#define GPU_TEXT hipGetErrorString
#define GPU_OK hipSuccess
#define GPU_NONE hipErrorNoDevice
#define GPU_DRIVER hipErrorInsufficientDriver
#define GPU_INIT hipErrorInitializationError
inline constexpr const char *backend="ROCM";
#else
#include <cuda_runtime_api.h>
#define GPU_COUNT cudaGetDeviceCount
#define GPU_TEXT cudaGetErrorString
#define GPU_OK cudaSuccess
#define GPU_NONE cudaErrorNoDevice
#define GPU_DRIVER cudaErrorInsufficientDriver
#define GPU_INIT cudaErrorInitializationError
inline constexpr const char *backend="CUDA";
#endif
int main()try{
 using namespace dome;int devices=0;auto status=GPU_COUNT(&devices);
 if(!((status==GPU_OK&&devices==0)||status==GPU_NONE||status==GPU_DRIVER||status==GPU_INIT))throw std::runtime_error("Probe requires an actually missing vendor device");
 if(compute_backend()!=backend)throw std::runtime_error("Probe linked an unexpected backend");
 std::cout<<"REAL_VENDOR_STATUS "<<backend<<' '<<int(status)<<' '<<GPU_TEXT(status)<<" devices="<<devices<<'\n';
 apply_brush({},Brush{});
 for(size_t count:{size_t(1),size_t(257),size_t(2'000'007)}){
  std::vector<Sample> samples(count,{{.01f,.02f,.03f},.04f,-.02f,.37f});auto before=samples;bool refused=false;
  try{apply_brush(samples,Brush{});}catch(const std::runtime_error &e){const std::string_view text=e.what();refused=text==GPU_TEXT(GPU_NONE)||text==GPU_TEXT(GPU_DRIVER)||text==GPU_TEXT(GPU_INIT);std::cout<<"EXPECTED_DEVICE_ERROR "<<count<<' '<<text<<'\n';}
  if(!refused||std::memcmp(samples.data(),before.data(),samples.size()*sizeof(Sample)))throw std::runtime_error("Missing vendor device changed caller or silently used a CPU fallback");
 }
 std::cout<<"PASS actual "<<backend<<" module refuses missing device; empty and3 nonempty spans preserve complete host records\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
