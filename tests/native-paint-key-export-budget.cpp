// SPDX-License-Identifier: GPL-3.0-or-later
#include "native-paint-frame-export-policy.hpp"
#include "native-authoring-capture.hpp"
#include "native-authoring-compiler.hpp"
#include "dome/rig-edit.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using namespace dome;
namespace {
size_t checks=0;
void check(bool pass,const char*label){if(!pass)throw std::runtime_error(label);++checks;std::cout<<"PASS "<<label<<std::endl;}
bool refuses(const auto&call){try{call();return false;}catch(const std::exception&e){std::cout<<"EXPECTED "<<e.what()<<std::endl;return true;}}
}
int main(int argc,char**argv)try{
 if(argc<2||argc>3||(argc==3&&std::string_view(argv[2])!="--policy-only"))return 2;const bool policy_only=argc==3;const auto source=Scene::load(argv[1]);
 for(bool posed:{false,true})for(float weight:{0.f,.37f,1.f}){
  auto scene=source;RigEdit pose;pose.linked_pose=false;pose.morph=MorphWeightEdit{0,weight};if(posed)pose.joint=JointAdjustment{1,{0,0,25},{}};edit_rig(scene,scene.active().id,pose);const auto exact=scene.encode();const auto&o=scene.active();
  check(refuses([&]{(void)detail::native_paint_export_working_bytes(o.painted->mesh(),o.deformation.get());}),"Bare native morph budget remains guarded without an actual frame owner");
  size_t polls=0;const auto bytes=detail::native_paint_frame_export_working_bytes(o,512*1024*1024,[&]{++polls;return false;});check(bytes>detail::native_paint_export_working_bytes(o.painted->mesh())&&polls>2,"Genuine actual key/skin/current-frame layout reserves model and direction storage");
  check(detail::native_paint_frame_export_working_bytes(o,bytes)==bytes,"Exact native frame working boundary succeeds deterministically");
  check(refuses([&]{(void)detail::native_paint_frame_export_working_bytes(o,bytes-1);}),"One-byte-short native key budget refuses before output payload allocation");
  for(size_t at:{size_t(1),polls}){size_t calls=0;check(refuses([&]{(void)detail::native_paint_frame_export_working_bytes(o,512*1024*1024,[&]{return ++calls>=at;});})&&calls>=at,"Actual early/late frame-budget cancellation is reachable");}
  auto foreign=o;auto binding=std::make_shared<SourceMaterialBindings>(*o.material_bindings);binding->native_paint_frames=std::shared_ptr<const detail::NativePaintFrameSource>(binding->native_paint_frames.get(),[](const detail::NativePaintFrameSource*){});foreign.material_bindings=binding;check(refuses([&]{(void)detail::native_paint_frame_export_working_bytes(foreign,512*1024*1024);}),"Same-address foreign frame control block cannot authorize budget eligibility");
  foreign=o;binding=std::make_shared<SourceMaterialBindings>(*o.material_bindings);binding->native_paint_frames.reset();foreign.material_bindings=binding;check(refuses([&]{(void)detail::native_paint_frame_export_working_bytes(foreign,512*1024*1024);}),"A missing native frame cannot acquire eligibility from morph dimensions");
  foreign=o;auto changed=std::make_shared<DeformationState>(*o.deformation);changed->morphs->targets[0].delta[0].z+=.02f;foreign.deformation=changed;check(refuses([&]{(void)detail::native_paint_frame_export_working_bytes(foreign,512*1024*1024);}),"Changed actual current morph field cannot borrow retained frame directions");
  if(!policy_only){
   NativeAuthoringExport owner;owner.scene=std::make_shared<const Scene>(scene.snapshot());owner.allow_portable_fallback=true;owner.scope=NativeAuthoringExport::Scope::WholeScene;SourceUvBakeOptions policy;policy.resolution=256;auto capture=detail::NativeAuthoringCapture::create(owner,{},policy);auto view=detail::compile_native_authoring(*capture,policy,256*1024*1024,{});check(view.surfaces.size()==1,"Actual whole native authoring capture/compiler admits the genuine current key layout");
   policy.max_working_bytes=1;check(refuses([&]{(void)detail::NativeAuthoringCapture::create(owner,{},policy);}),"Actual native key capture keeps the caller working limit before allocation");
  }
  check(scene.encode()==exact,"Complete native source, paint, frame and rig state remain exact after budget probes");
 }
 std::cout<<"PASS "<<checks<<(policy_only?" frame-policy component controls (whole compiler not requested)":" actual integrated native key budget/capture/compiler controls")<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
