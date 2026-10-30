// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/brush.hpp"
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <thread>
#include <vector>
#ifdef DOME_TEST_SYCL
#include <sycl/sycl.hpp>
#endif
namespace dome { void dome_reference_apply_brush(std::span<Sample>,const Brush &); }
using namespace dome;
namespace {
void check(bool value,const char *message) { if(!value)throw std::runtime_error(message); }
bool identical(const std::vector<Sample> &a,const std::vector<Sample> &b) {
  return a.size()==b.size() && !std::memcmp(a.data(),b.data(),a.size()*sizeof(Sample));
}
}
int main(int argc,char **argv) try {
  check(compute_backend()==DOME_EXPECTED_BACKEND,"Executable does not use the requested build-time backend");
  std::cout<<"Requested backend: "<<compute_backend()<<'\n';
  apply_brush({},Brush{});
  if(argc==2 && std::string_view(argv[1])=="--expect-missing-gpu") {
#ifdef DOME_TEST_SYCL
    std::vector<Sample> values{{{0,0,0},.04f,-.02f}};auto before=values;
    try { apply_brush(values,Brush{}); }
    catch(const sycl::exception &error) {
      check(identical(values,before),"Unavailable GPU partially changed host samples");
      std::cout<<"PASS missing GPU fails explicitly and retains host samples: "<<error.what()<<'\n';return 0;
    }
    throw std::runtime_error("GPU backend silently succeeded without an eligible GPU");
#else
    throw std::runtime_error("Missing-GPU check requires the SYCL test build");
#endif
  }
#ifdef DOME_TEST_SYCL
  sycl::queue selected{sycl::gpu_selector_v};
  auto device=selected.get_device();check(device.is_gpu(),"SYCL selected a CPU fallback");
  std::cout<<"Actual eligible GPU: "<<device.get_info<sycl::info::device::name>()
           <<" / "<<device.get_info<sycl::info::device::vendor>()<<'\n';
#endif
  Brush brush;brush.from=brush.to={};brush.radius=.1f;brush.band=.2f;
  brush.pressure=brush.strength=1;brush.blend=0;brush.normal={0,1,0};
  std::array<float,4> exact{-.1f,.1f,-.02f,0};
  for(size_t tool=0;tool<exact.size();++tool) {
    brush.tool=Tool(tool);std::vector<Sample> values{{{0,0,0},.04f,-.02f}};
    apply_brush(values,brush);
    check(std::abs(values[0].distance-exact[tool])<2e-6f,"Analytic centered brush endpoint differs");
  }
#ifdef DOME_TEST_CPU
  std::cout<<"PASS 4 independent analytic CPU endpoints and empty dispatch\n";
  return 0;
#else
  std::mt19937 generator(0x5343554c);
  std::uniform_real_distribution<float> position(-.3f,.3f),distance(-.2f,.2f);
  std::array<size_t,8> counts{1,7,255,256,257,1023,4097,10000};
  float maximum_error=0;size_t batches=0,samples=0;
  auto start=std::chrono::steady_clock::now();
  for(size_t tool=0;tool<4;++tool)for(float pressure:{0.f,.2f,.7f,1.f})for(auto count:counts) {
    std::vector<Sample> values;values.reserve(count);
    for(size_t i=0;i<count;++i)values.push_back({{position(generator),position(generator),position(generator)},distance(generator),distance(generator)});
    auto original=values,reference=values;
    brush.tool=Tool(tool);brush.pressure=pressure;brush.strength=.71f;brush.blend=.035f;
    brush.from={-.13f,.02f,-.01f};brush.to={.09f,-.01f,.03f};
    dome_reference_apply_brush(reference,brush);apply_brush(values,brush);
    for(size_t i=0;i<count;++i) {
      maximum_error=std::max(maximum_error,std::abs(values[i].distance-reference[i].distance));
      check(std::isfinite(values[i].distance) && std::abs(values[i].distance-reference[i].distance)<5e-6f,
            "Requested backend differs from the separately compiled CPU reference");
      check(!std::memcmp(&values[i].position,&original[i].position,sizeof(Vec3)) && values[i].average==original[i].average,
            "GPU operation changed non-distance sample fields");
    }
    if(pressure==0)check(identical(values,original),"Zero pressure modifies an unchanged brush batch");
    ++batches;samples+=count;
  }
  {
    // Cross the bounded device-storage chunk boundary without exposing an
    // intermediate host result. Values also exercise exact preserved metadata.
    std::vector<Sample> values(2'000'007,{{.01f,.02f,.03f},.04f,-.02f});
    auto expected=values;brush.tool=Tool::Smooth;brush.pressure=.7f;
    dome_reference_apply_brush(expected,brush);apply_brush(values,brush);
    for(size_t i=0;i<values.size();++i)
      check(std::abs(values[i].distance-expected[i].distance)<5e-6f &&
            values[i].position.x==.01f && values[i].position.y==.02f && values[i].average==-.02f,
            "A multi-chunk GPU operation changed its reference result or immutable inputs");
    std::cout<<"PASS bounded device storage spans a2000007-sample transaction\n";
  }
  std::array<std::exception_ptr,4> errors{};
  std::vector<std::thread> threads;
  for(size_t worker=0;worker<errors.size();++worker)threads.emplace_back([&,worker] {
    try {
      std::vector<Sample> values(257,{{.01f,.02f,.03f},.04f,-.02f});auto expected=values;
      auto edit=brush;edit.tool=Tool(worker);edit.pressure=.3f+worker*.1f;
      for(int repeat=0;repeat<8;++repeat) {
        dome_reference_apply_brush(expected,edit);apply_brush(values,edit);
        for(size_t i=0;i<values.size();++i)
          check(std::abs(values[i].distance-expected[i].distance)<5e-6f,
                "Concurrent authoring threads shared GPU brush storage");
      }
    }catch(...){errors[worker]=std::current_exception();}
  });
  for(auto &thread:threads)thread.join();
  for(auto error:errors)if(error)std::rethrow_exception(error);
  std::cout<<"PASS independent concurrent authoring queues and repeated buffer reuse\n";
  auto milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  std::cout<<"PASS 4 independent analytic endpoints, "<<batches<<" CPU-reference batches / "<<samples
           <<" samples; max error="<<maximum_error<<" elapsed_ms="<<milliseconds<<'\n';
  return 0;
#endif
} catch(const std::exception &error) {std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
