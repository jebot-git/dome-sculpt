// SPDX-License-Identifier: GPL-3.0-or-later
#include "normal-domain-fixtures.hpp"
#include <iostream>
#include <limits>
using namespace dome;
namespace probe {
bool count=false,inject=false,cheap_finished=false;size_t geometry=0,metadata=0;
const SourceNormalPreparation *target=nullptr;std::shared_ptr<const SourceMaterialBindings> candidate;
}
extern "C" void __real__ZNK4dome20NormalSamplingDomain8validateEbRKSt8functionIFbvEE(const NormalSamplingDomain*,bool,const std::function<bool()>&);
extern "C" void __wrap__ZNK4dome20NormalSamplingDomain8validateEbRKSt8functionIFbvEE(const NormalSamplingDomain *p,bool geometry,const std::function<bool()> &cancelled){if(probe::count){if(geometry)++probe::geometry;else ++probe::metadata;}__real__ZNK4dome20NormalSamplingDomain8validateEbRKSt8functionIFbvEE(p,geometry,cancelled);}
extern "C" void __real__ZNK4dome23SourceNormalPreparation8validateEb(const SourceNormalPreparation*,bool);
extern "C" void __wrap__ZNK4dome23SourceNormalPreparation8validateEb(const SourceNormalPreparation *p,bool deep){
 if(probe::inject&&p==probe::target&&deep){probe::inject=false;probe::candidate->validate(false);probe::cheap_finished=true;probe::candidate->validate(true);}
 __real__ZNK4dome23SourceNormalPreparation8validateEb(p,deep);
}
namespace {
unsigned checks=0;void check(bool value,const char *why){if(!value)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception &e){std::cout<<"REFUSAL "<<e.what()<<std::endl;return true;}}
}
int main()try{
 auto scene=normal_domain_fixture::model();auto holder=scene.active().material_bindings;const auto original=scene.encode();
 probe::geometry=probe::metadata=0;probe::count=true;holder->validate(false);probe::count=false;
 std::cout<<"CHEAP geometry="<<probe::geometry<<" metadata="<<probe::metadata<<std::endl;
 check(probe::geometry==0&&probe::metadata>0,"Cheap publication keeps structural normal-domain checks but performs no full geometry scan");
 probe::geometry=probe::metadata=0;probe::count=true;holder->validate(true);probe::count=false;
 check(probe::geometry>0,"Deep worker/decoder validation still checks actual domain coordinates and corners");
 auto domain=std::make_shared<NormalSamplingDomain>(*holder->normal_preparation->projection->sampling_domain);domain->coordinates[0][0]=std::numeric_limits<float>::quiet_NaN();
 auto projection=std::make_shared<PaintNormalProjection>(*holder->normal_preparation->projection);projection->sampling_domain=domain;
 auto sampling=std::make_shared<SourceNormalSampling>(*holder->normal_preparation->sampling_domain);sampling->projection=projection;
 auto recipe=std::make_shared<SourceNormalPreparation>(*holder->normal_preparation);recipe->sampling_domain=sampling;recipe->projection=projection;
 recipe->validate(false);check(true,"Structural-only validation does not pretend to authenticate unknown coordinate values");
 check(refuses([&]{recipe->validate(true);}),"The actual same forged coordinate is rejected by mandatory deep admission");
 auto bad_image=std::make_shared<Image>(*projection->pbr);++bad_image->width;auto dimensions=std::make_shared<PaintNormalProjection>(*projection);dimensions->pbr=bad_image;
 check(refuses([&]{dimensions->validate(scene.active().painted->resolution(),false);}),"Cheap path still rejects contradictory image dimensions before any coordinate work");
 auto initial=holder;while(initial->normal_preparation->sampling_domain)initial=initial->normal_preparation->sampling_domain->before_material;
 auto frame=std::make_shared<NormalFrames>(*initial->normal_frames->pbr);std::swap(frame->origin_vertices[0],frame->origin_vertices[1]);
 auto frames=std::make_shared<NormalFrameSet>(*initial->normal_frames);frames->pbr=frames->mtoon=frame;
 auto bad_recipe=std::make_shared<SourceNormalPreparation>(*initial->normal_preparation);bad_recipe->target_frames=frames;
 auto bad_binding=std::make_shared<SourceMaterialBindings>(*initial);bad_binding->normal_preparation=bad_recipe;bad_binding->normal_frames=frames;
 bad_binding->validate(false);check(refuses([&]{bad_binding->validate(true);}),"Independent fixture has a genuine deep-only ordered-origin mismatch");
 probe::target=holder->normal_preparation.get();probe::candidate=bad_binding;probe::cheap_finished=false;probe::inject=true;
 const bool upgraded=refuses([&]{holder->validate(true);});probe::inject=false;probe::candidate.reset();
 check(upgraded&&probe::cheap_finished,"Within one actual root validation, a completed cheap proof is upgraded and cannot hide the deep-only forgery");
 holder->validate(true);check(scene.encode()==original,"Exceptional proof upgrade clears the ledger and preserves the exact source for retry");
 std::cout<<"PASS "<<checks<<" cheap/deep normal geometry and shared proof-upgrade checks\n";return 0;
}catch(const std::exception &e){probe::count=probe::inject=false;probe::candidate.reset();std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
