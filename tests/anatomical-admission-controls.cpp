// SPDX-License-Identifier: GPL-3.0-or-later
#include "native-anatomical-weights.hpp"
#include "dome/skin-authoring.hpp"
#include "frame-position-domain.hpp"
#include <iostream>
using namespace dome;using namespace dome::detail;
int main()try{
 size_t checks=0;auto check=[&](bool b,const char*s){if(!b)throw std::runtime_error(s);++checks;std::cout<<"PASS "<<s<<std::endl;};
 static_assert(int(SkinAuthoringMode::Automatic)==0&&int(SkinAuthoringMode::Transfer)==1&&int(SkinAuthoringMode::Anatomical)==4);
 SkinAuthoringOptions options;options.reference=1;options.validate();options.mode=SkinAuthoringMode::Transfer;options.validate();check(true,"Both original method IDs/defaults remain valid");
 for(int id:{2,3}){auto o=options;o.mode=SkinAuthoringMode(id);bool bad=false;try{o.validate();}catch(const std::invalid_argument&){bad=true;}check(bad,"Reserved prior experimental method ID stays refused");}
 options.mode=SkinAuthoringMode::Anatomical;options.limits={1,.9f};options.validate();check(options.anatomical.maximum_work==500000000&&options.anatomical.maximum_workspace==256u*1024u*1024u&&options.anatomical.maximum_total==512u*1024u*1024u,"Explicit anatomical bounds remain separate from inactive legacy controls");
 auto bad=[&](auto f){try{f();return false;}catch(const std::invalid_argument&){return true;}};
 check(bad([&]{auto o=options;o.anatomical.maximum_work=500000001;o.validate();}),"Anatomical work cannot exceed unchanged500M");
 check(bad([&]{auto o=options;o.anatomical.maximum_workspace=256u*1024u*1024u+1;o.validate();}),"Anatomical workspace cannot exceed256MiB");
 check(bad([&]{auto o=options;o.anatomical.maximum_total=512u*1024u*1024u+1;o.validate();}),"Anatomical owning total cannot exceed512MiB");
 native_weight_retention_impl::Counter c(1024*1024,2);c.add(1);c.add(1);const auto used=c.used;bool bounded=false;try{c.add(1);}catch(const std::length_error&){bounded=true;}check(bounded&&c.work==2&&c.used==used,"Ownership preflight obeys actual caller work limit before charging beyond it");
 Scene scene;bool scan=false;try{auto r=native_weight_retention(scene,1024*1024,1);(void)r;}catch(const std::length_error&e){scan=std::string(e.what())=="Native ownership scan work";}check(scan,"Real Scene ownership scan forwards a lowered work cap");
 Mesh m;m.positions={{0,0,0},{1,0,0},{0,1,0}};m.normals.assign(3,{0,0,1});m.uv={{{0,0}},{{1,0}},{{0,1}}};m.indices={0,1,2};auto paint=std::make_shared<PaintedMesh>(m,64);
 auto frame=std::make_shared<NormalFrames>();frame->origin_vertex_count=3;frame->origin_vertices=m.indices;frame->normals=m.normals;auto frames=std::make_shared<NormalFrameSet>();frames->pbr=frame;frames->validate();
 auto origin=std::make_shared<SourceUvOrigin>();origin->paint=paint->snapshot();SourceMaterialBindings binding;binding.has_mtoon=false;binding.normal_frames=frames;binding.uv_origin=origin;
 auto domain=derive_frame_position_domain(paint->mesh_owner(),binding,{});domain->validate();auto derived=std::make_shared<NormalFrameSet>(*frames);derived->position_domain=domain;derived->validate();
 check(bad([&]{native_weight_retention_impl::Counter c(1024*1024);c.frame_set(derived);}),"Genuine separately owned frame POSITION domain refuses before unrepresented resource retention");
 {native_weight_retention_impl::Counter c(1024*1024);c.frame_set(frames);check(c.count>0,"Ordinary represented native frame closure remains admitted");}
 auto owner=std::make_shared<int>(4);auto null=std::make_shared<NormalFrameSet>(*frames);null->position_domain=std::shared_ptr<const FramePositionDomain>(owner,nullptr);
 check(bad([&]{native_weight_retention_impl::Counter c(1024*1024);c.frame_set(null);}),"Owned-null independent geometry alias cannot disappear from the preflight");
 auto unbound=std::make_shared<SourceMaterialBindings>();unbound->normal_preparation=std::shared_ptr<const SourceNormalPreparation>(owner,nullptr);
 check(bad([&]{native_weight_retention_impl::Counter c(1024*1024);c.material(unbound);}),"Owned-null unsupported preparation cannot disappear from resource admission");
 std::cout<<"Anatomical admission controls "<<checks<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
