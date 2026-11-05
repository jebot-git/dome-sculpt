// SPDX-License-Identifier: GPL-3.0-or-later
#include "humanoid-heat-sources.hpp"
#include <iostream>
using namespace dome;using namespace dome::detail;
namespace{size_t checks=0;void check(bool value,const char*why){if(!value)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}}
int main()try{
 Skeleton rig;Bone foot;foot.name="arbitrary0";foot.rest.translation={-1,0,0};foot.length=.08f;rig.bones.push_back(foot);Bone toes;toes.name="arbitrary1";toes.parent=0;toes.rest.translation={0,0,-.1f};toes.length=.08f;rig.bones.push_back(toes);Bone eye;eye.name="arbitrary2";eye.rest.translation={1,1,0};eye.length=.08f;rig.bones.push_back(eye);Bone secondary;secondary.name="arbitrary3";secondary.rest.translation={5,0,0};secondary.length=.2f;rig.bones.push_back(secondary);
 HumanoidMapping roles;roles.joints={{"leftFoot",0},{"leftToes",1},{"leftEye",2}};std::function<bool()>cancel;weight_surface::Budget work{0,50'000'000,cancel};auto profile=humanoid_heat_sources(rig,roles,work);
 check(profile.size()==4&&profile[0].tail==profile[1].head,"Foot continuation uses actual mapped toe joint head");
 check(profile[1].tail[0]==-1&&profile[1].tail[1]==0&&std::abs(profile[1].tail[2]+.18)<1e-7,"Terminal toe continuation uses actual parent chain rather than display up-axis");
 check(profile[2].head==profile[2].tail,"Typed ocular joint supplies a genuine rotary point source");
 auto source=rig.segments()[3];check(profile[3].tail==std::array<double,3>{source[1].x,source[1].y,source[1].z},"Unmapped secondary control preserves its complete authored finite source");
 auto renamed=rig;for(auto&b:renamed.bones)b.name="renamed"+std::to_string(&b-renamed.bones.data());auto rename=humanoid_heat_sources(renamed,roles,work);bool exact=true;for(size_t i=0;i<profile.size();++i)exact&=profile[i].head==rename[i].head&&profile[i].tail==rename[i].tail&&profile[i].bone==rename[i].bone;check(exact,"Display names never choose handle geometry");
 for(const auto&coefficients:{std::array<double,16>{0,2,0,0,-2,0,0,0,0,0,2,0,4,-8,16,1},std::array<double,16>{-2,0,0,0,.5,1.5,0,0,0,.25,.75,0,16,-8,4,1}}){const AffineTransform affine(coefficients);auto changed=rig;changed.space_matrix=affine.float_matrix();auto transformed=humanoid_heat_sources(changed,roles,work);double error=0;for(size_t i=0;i<profile.size();++i)for(bool tail:{false,true}){auto a=tail?profile[i].tail:profile[i].head;auto q=affine.point(a);auto actual=tail?transformed[i].tail:transformed[i].head;for(size_t k=0;k<3;++k)error=std::max(error,std::abs(q[k]-actual[k]));}check(error<3e-6,"Role handle construction commutes with actual rotation/scale or full affine reflection/shear");}
 auto nondeform=rig;nondeform.bones.back().deform=false;auto selected=humanoid_heat_sources(nondeform,roles,work);check(selected.size()==3&&std::none_of(selected.begin(),selected.end(),[](auto&s){return s.bone==3;}),"Nondeform joints remain geometry but never emit source fields");
 auto wrong=roles;wrong.joints["rightEye"]=2;bool refused=false;try{humanoid_heat_sources(rig,wrong,work);}catch(const std::runtime_error&e){refused=std::string(e.what()).find("joint assigned multiple roles")!=std::string::npos;}check(refused,"Duplicate or foreign humanoid role association refuses");
 auto broken=rig;broken.bones[1].parent=-1;refused=false;try{humanoid_heat_sources(broken,roles,work);}catch(const std::runtime_error&e){refused=std::string(e.what()).find("incorrect humanoid ancestor")!=std::string::npos;}check(refused,"Invented continuation without actual mapped ancestry refuses");
 std::cout<<"PASS humanoid_heat_source_checks="<<checks<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
