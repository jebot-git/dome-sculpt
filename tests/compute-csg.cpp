// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/csg.hpp"
#include <bit>
#include <cstring>
#include <iostream>
#include <random>
#include <thread>
#include <vector>
#ifdef DOME_TEST_SYCL
#include <sycl/sycl.hpp>
#endif
using namespace dome;
namespace {
size_t checks=0;
void check(bool yes,const char *label){if(!yes)throw std::runtime_error(label);++checks;std::cout<<"PASS "<<label<<'\n';}
// Independent double oracle: nearest point on the inner box, or signed
// nearest face distance for an interior point. No production kernel included.
double oracle(Vec3 point,const CsgBox &box) {
  double p[]={point.x,point.y,point.z};
  double lo[]={box.minimum.x+double(box.rounding),box.minimum.y+double(box.rounding),box.minimum.z+double(box.rounding)};
  double hi[]={box.maximum.x-double(box.rounding),box.maximum.y-double(box.rounding),box.maximum.z-double(box.rounding)};
  double squared=0,face=1e100;bool inside=true;
  for(int i=0;i<3;++i){inside&=p[i]>=lo[i]&&p[i]<=hi[i];double nearest=std::clamp(p[i],lo[i],hi[i]);squared+=(p[i]-nearest)*(p[i]-nearest);face=std::min({face,p[i]-lo[i],hi[i]-p[i]});}
  return (inside?-face:std::sqrt(squared))-box.rounding;
}
double expected(const Sample &s,std::span<const CsgBox> boxes,float band) {
  if(s.influence==0)return s.distance;
  double v=s.distance;for(const auto &b:boxes){double d=oracle(s.position,b);v=b.operation==CsgOperation::Union?std::min(v,d):std::max(v,-d);}
  v=std::clamp(v,-double(band),double(band));return double(s.distance)+(v-s.distance)*s.influence;
}
bool exact(std::span<const Sample> a,std::span<const Sample> b){return a.size()==b.size()&&!std::memcmp(a.data(),b.data(),a.size_bytes());}
}
int main(int argc,char **argv)try {
  check(compute_backend()==DOME_EXPECTED_BACKEND,"CSG uses the requested build-time compute backend");
  std::cout<<"Requested backend: "<<compute_backend()<<'\n';
  CsgBox box{{-.1f,-.2f,-.3f},{.1f,.2f,.3f},.03f,CsgOperation::Union};
  apply_csg({},std::span(&box,1),.4f);
  std::vector<Sample> values{{{0,0,0},.2f,-.02f,.37f}},original=values;
  apply_csg(values,{},.4f);check(exact(values,original),"Empty CSG operations preserve all samples without creating a queue");
  if(argc==2&&std::string_view(argv[1])=="--expect-missing-gpu") {
    check(compute_backend()!="CPU","Missing-GPU check requires a selected GPU backend");
    try {apply_csg(values,std::span(&box,1),.4f);}
    catch(const std::exception &e){check(exact(values,original),"Missing CSG GPU retains every host byte");std::cout<<"Explicit device rejection: "<<e.what()<<'\n';return 0;}
    throw std::runtime_error("CSG silently succeeded without an eligible GPU");
  }
#ifdef DOME_TEST_SYCL
  sycl::queue selected{sycl::gpu_selector_v};check(selected.get_device().is_gpu(),"CSG fixture selected actual GPU hardware");
  std::cout<<"Actual eligible GPU: "<<selected.get_device().get_info<sycl::info::device::name>()<<'\n';
#endif
  double maximum_error=0;size_t compared=0,batches=0;
  auto compare=[&](std::vector<Sample> input,std::vector<CsgBox> boxes) {
    auto before=input;apply_csg(input,boxes,.4f);
    for(size_t n=0;n<input.size();++n){double error=std::abs(input[n].distance-expected(before[n],boxes,.4f));maximum_error=std::max(maximum_error,error);if(error>3e-6||!std::isfinite(input[n].distance))throw std::runtime_error("CSG differs from independent signed nearest-boundary oracle");if(std::memcmp(&input[n].position,&before[n].position,sizeof(Vec3))||std::bit_cast<uint32_t>(input[n].average)!=std::bit_cast<uint32_t>(before[n].average)||input[n].influence!=before[n].influence)throw std::runtime_error("CSG changed immutable sample metadata");}
    ++batches;compared+=input.size();
  };
  for(auto operation:{CsgOperation::Union,CsgOperation::Difference})for(float rounding:{0.f,.03f,.1f})for(float influence:{0.f,.37f,1.f}) {
    auto b=box;b.operation=operation;b.rounding=rounding;
    std::vector<Sample> points;
    for(Vec3 p:std::initializer_list<Vec3>{{0,0,0},{.1f,0,0},{.11f,0,0},{.1f,.2f,.3f},{-.1f,-.2f,-.3f},{.15f,.25f,.35f},{-.4f,.3f,-.2f}})
      points.push_back({p,.04f,std::bit_cast<float>(0x7fc01234u),influence});
    compare(std::move(points),{b});
  }
  check(true,"Analytic centres, faces, edges, corners, rounding and protection match the independent oracle");
  std::mt19937 generator(0x43534742u);std::uniform_real_distribution<float> position(-.5f,.5f),distance(-.4f,.4f);
  for(size_t count:{1u,7u,255u,256u,257u,4097u,10000u})for(size_t shape_count:{1u,2u,17u}) {
    std::vector<CsgBox> boxes;for(size_t n=0;n<shape_count;++n){float x=position(generator)*.4f,y=position(generator)*.4f,z=position(generator)*.4f;boxes.push_back({{x-.1f,y-.12f,z-.08f},{x+.1f,y+.12f,z+.08f},n%2?.025f:0.f,n%3?CsgOperation::Union:CsgOperation::Difference});}
    std::vector<Sample> input;for(size_t i=0;i<count;++i)input.push_back({{position(generator),position(generator),position(generator)},distance(generator),.123f,i%3==0?0.f:i%3==1?.37f:1.f});compare(std::move(input),std::move(boxes));
  }
  check(true,"Ordered mixed union/difference batches and workgroup edges retain fields and all metadata");
  {
    std::vector<CsgBox> boxes{box,{{-.05f,-.05f,-.4f},{.05f,.05f,.4f},.02f,CsgOperation::Difference}};
    std::vector<Sample> input(500'007,{{.01f,.02f,.03f},.04f,.12f,.37f});auto before=input;size_t callbacks=0;
    apply_csg(input,boxes,.4f,[&]{++callbacks;return false;});check(std::abs(input.back().distance-expected(before.back(),boxes,.4f))<3e-6,"CSG crosses a bounded arithmetic-work chunk boundary without intermediate host publication");
    std::copy(before.begin(),before.end(),input.begin());size_t until=callbacks-2;bool cancelled=false;try{apply_csg(input,boxes,.4f,[&]{return until--==0;});}catch(const std::runtime_error &){cancelled=true;}
    check(cancelled&&exact(input,before),"Late observed cancellation retains the complete original host transaction");
    apply_csg(input,boxes,.4f);check(std::abs(input.back().distance-expected(before.back(),boxes,.4f))<3e-6,"A cancelled device submission drains safely and can retry on a fresh queue");
  }
  {
    std::vector<Sample> input(257,{{.01f,.02f,.03f},.04f,.12f,.37f});auto before=input;
    size_t callbacks=0;apply_csg(input,std::span(&box,1),.4f,[&]{++callbacks;return false;});
    for(size_t cut=0;cut<callbacks;++cut){std::copy(before.begin(),before.end(),input.begin());size_t at=0;bool stopped=false;try{apply_csg(input,std::span(&box,1),.4f,[&]{return at++==cut;});}catch(const std::runtime_error &){stopped=true;}if(!stopped||!exact(input,before))throw std::runtime_error("CSG callback boundary published a partial result");}
    check(true,"Every observed cancellation boundary is atomic, including queued device copies");
    auto invalid=box;invalid.rounding=.3f;bool rejected=false;try{apply_csg(input,std::span(&invalid,1),.4f);}catch(const std::invalid_argument &){rejected=true;}
    check(rejected&&exact(input,before),"Invalid primitive parameters refuse before mutation");
    std::vector<CsgBox> too_many(2049,box);rejected=false;try{apply_csg(input,too_many,.4f);}catch(const std::length_error &){rejected=true;}
    check(rejected&&exact(input,before),"CSG work and primitive quotas refuse without host mutation");
  }
  std::array<std::exception_ptr,4> errors{};std::vector<std::thread> threads;
  for(size_t i=0;i<4;++i)threads.emplace_back([&,i]{try{auto b=box;b.rounding=.01f*float(i);std::vector<Sample> input(513,{{.02f,.03f,.04f},.04f,.2f,.37f});for(int repeat=0;repeat<8;++repeat){auto before=input;apply_csg(input,std::span(&b,1),.4f);if(std::abs(input.front().distance-expected(before.front(),std::span(&b,1),.4f))>3e-6)throw std::runtime_error("Concurrent CSG queue mismatch");}}catch(...){errors[i]=std::current_exception();}});
  for(auto &t:threads)t.join();for(auto e:errors)if(e)std::rethrow_exception(e);
  check(true,"Concurrent authoring threads retain independent CSG operations, storage and queue state");
  std::cout<<checks<<" CSG compute checks; "<<batches<<" oracle batches / "<<compared<<" samples; maxerror="<<maximum_error<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
