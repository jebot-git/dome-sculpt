// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/shapekey-authoring.hpp"
#include "native-paint-frame-source.hpp"
#include <nlohmann/json.hpp>
#include <atomic>
#include <iostream>
using namespace dome;
namespace {std::atomic_size_t replay_calls=0;size_t checks=0;void check(bool b,const char*label){if(!b)throw std::runtime_error(label);++checks;std::cout<<"PASS "<<label<<std::endl;}bool refused(auto f){try{f();return false;}catch(const std::exception&e){std::cout<<"expected "<<e.what()<<std::endl;return true;}}}
extern "C" detail::NormalBakeFrameCandidate real_capture(const Mesh&,const DeformationState*,const SourceMaterialBindings*,const detail::NormalBakeFrameOptions&) asm("__real__ZN4dome6detail27normal_bake_frame_candidateERKNS_4MeshEPKNS_16DeformationStateEPKNS_22SourceMaterialBindingsERKNS0_22NormalBakeFrameOptionsE");
extern "C" detail::NormalBakeFrameCandidate wrap_capture(const Mesh&,const DeformationState*,const SourceMaterialBindings*,const detail::NormalBakeFrameOptions&) asm("__wrap__ZN4dome6detail27normal_bake_frame_candidateERKNS_4MeshEPKNS_16DeformationStateEPKNS_22SourceMaterialBindingsERKNS0_22NormalBakeFrameOptionsE");
extern "C" detail::NormalBakeFrameCandidate wrap_capture(const Mesh&m,const DeformationState*d,const SourceMaterialBindings*b,const detail::NormalBakeFrameOptions&o){++replay_calls;return real_capture(m,d,b,o);}
int main(int argc,char**argv)try{if(argc!=3)return 2;auto s=Scene::load(argv[1]);const auto before=s.encode();const auto &b=s.active().material_bindings;check(b&&b->native_paint_frames&&b->native_paint_sampling->history_depth()>1,"Actual post-key UV result owns both frame capture and new paint history");
 replay_calls=0;b->validate(false);std::cout<<"FALSE_REPLAY="<<replay_calls.load()<<std::endl;check(replay_calls==0,"Published false material validation performs no normal capture replay");
 replay_calls=0;b->validate(true);check(replay_calls>0,"Worker/decode true validation still independently replays normal capture");
 auto forged=*b;forged.native_paint_sampling=std::shared_ptr<const detail::NativePaintSampling>(b->native_paint_sampling.get(),[](const detail::NativePaintSampling*){});check(refused([&]{forged.validate(false);})&&refused([&]{forged.validate(true);}),"Both metadata and deep paths reject same-address foreign native paint control block");
 auto bad=s;auto d=std::make_shared<DeformationState>(*s.active().deformation);d->morphs->targets[0].delta[0].z+=.001f;bad.active().deformation=d;bad.validate(false);check(refused([&]{bad.validate(true);}),"Forged current morph POSITION is worker-deep-only and cannot inherit the old sealed field proof");check(s.encode()==before,"Failed independent current geometry proof leaves original complete scene exact");
 Document doc;doc.replace(s);check(doc.wait_mesh()&&doc.wait_asset(),"Actual post-key metadata publication fixture is ready");auto actor=doc.copy_scene().active();replay_calls=0;doc.object_properties(actor.id,actor.name+" metadata rename",actor.visible,actor.locked,actor.transform,actor.material);check(doc.wait_mesh(),"Actual renamed mesh publication completes");std::cout<<"PUBLICATION_REPLAY="<<replay_calls.load()<<std::endl;check(replay_calls==0,"Actual native scene publication does not replay immutable normal capture");
 check(doc.undo()&&doc.copy_scene().encode()==before,"Metadata-only publication retains one exact native history endpoint");std::filesystem::create_directories(argv[2]);doc.save_async(std::filesystem::path(argv[2])/"restored.dsculpt").get();check(Scene::load(std::filesystem::path(argv[2])/"restored.dsculpt").encode()==before,"Current metadata fast path retains complete native Save/decode proof");
 std::cout<<"PASS native_key_uv_metadata_checks="<<checks<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
