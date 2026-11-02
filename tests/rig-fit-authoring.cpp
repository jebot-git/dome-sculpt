// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/rig-fit.hpp"
#include "dome/document.hpp"
#include "dome/source-placement.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
unsigned checks=0;
void check(bool v,const char *message){if(!v)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<'\n';}
HumanLandmarks marks(){HumanLandmarks l;l.points={{"hips",{0,1,0}},{"spine",{0,1.2f,0}},{"head",{0,1.7f,0}}};
 for(std::string side:{"left","right"}){float s=side=="left"?1:-1;l.points[side+"UpperLeg"]={s*.15f,1,0};l.points[side+"LowerLeg"]={s*.15f,.55f,0};l.points[side+"Foot"]={s*.15f,.1f,0};l.points[side+"UpperArm"]={s*.25f,1.5f,0};l.points[side+"LowerArm"]={s*.55f,1.4f,0};l.points[side+"Hand"]={s*.8f,1.3f,0};}return l;}
Scene fixture(){auto l=marks();Mesh m;size_t c=0;for(auto &[_,p]:l.points){uint32_t i=uint32_t(m.positions.size());m.positions.insert(m.positions.end(),{p+Vec3{-.02f,-.01f,0},p+Vec3{.02f,-.01f,0},p+Vec3{0,.02f,0}});m.indices.insert(m.indices.end(),{i,i+1,i+2});float u=float(c%4)*.25f+.01f,v=float(c/4)*.25f+.01f;m.uv.insert(m.uv.end(),{{u,v},{u+.2f,v},{u+.1f,v+.2f}});++c;}m.normals.assign(m.positions.size(),{0,0,1});Scene s;s.active().painted=std::make_shared<PaintedMesh>(m,64);s.active().mesh_active=true;
 DeformationState d(m);d.morphs=MorphSet(m);auto key=m.positions;key[0].z+=.02f;d.morphs->set_target(m,"Existing artist key",key);d.morph_weights={.3f};s.set_deformation(1,d);return s;}
Vec3 p(const AffineTransform &m,Vec3 q){auto v=m.point({q.x,q.y,q.z});return {float(v[0]),float(v[1]),float(v[2])};}
}
int main(int argc,char **argv)try{
 if(argc==3&&std::string_view(argv[1])=="--ui-fixture"){auto s=fixture();auto d=*s.active().deformation;d.landmarks=marks();s.set_deformation(1,d);s.save(argv[2]);return 0;}
 if(argc==3&&std::string_view(argv[1])=="--inspect"){auto s=Scene::load(argv[2]);auto d=s.active().deformation;std::cout<<nlohmann::json{{"scene",s.encode()},{"fitted",bool(d&&d->skeleton&&d->humanoid)},{"bound",bool(d&&d->skin)}}.dump();return 0;}
 auto scene=fixture();const auto old=scene.encode();const auto paint=scene.active().painted;const auto mesh=paint->mesh_owner();const auto original=scene.active().deformation;
 auto check_fit=[&](Scene &s,const char *name){const auto expected_paint=s.active().painted;const auto expected_mesh=expected_paint->mesh_owner();auto result=author_humanoid_fit(s,1,marks());check(result.bones==15&&result.bound,name);const auto &d=*s.active().deformation;check(s.active().painted==expected_paint&&s.active().painted->mesh_owner()==expected_mesh,"Fit retains immutable painting REST and pixels");check(d.morphs->encode()==original->morphs->encode()&&d.morph_weights==original->morph_weights,"Fit retains exact artist morph channels and current values");auto segments=d.skeleton->segments();for(const auto &[role,joint]:d.humanoid->joints)if(length(segments[joint][0]-marks().points.at(role))>1e-5f)throw std::runtime_error("Landmark head mismatch");check(true,"Independent declared landmark heads equal fitted REST skeleton");
 const auto skin=d.skeleton->skin_matrices({});for(const auto &m:skin)for(size_t j=0;j<16;++j)if(std::abs(m[j]-(j%5==0?1.f:0.f))>1e-4f)throw std::runtime_error("Rest binding changed mesh");check(true,"Independent rest skin matrices are identity");};
 check_fit(scene,"Unskinned fit creates fifteen mapped bones and actual skin weights");
 auto affine=fixture();std::array<double,16> a{-1.3,0,0,0,.35,.8,0,0,.1,.15,1.2,0,2,-1,.5,1};affine.active().affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(a));check_fit(affine,"Reflected/sheared fitting uses actual WORLD distance and binding metric");
 auto d=*affine.active().deformation;d.pose.reserve(d.skeleton->bones.size());for(auto &b:d.skeleton->bones)d.pose.push_back(b.rest);d.pose[d.humanoid->joints.at("leftLowerArm")].rotation=arcball({0,1,0},normalized({.2f,1,.1f}));affine.set_deformation(1,d);auto posed=d.preview(paint->mesh());check(posed.positions.size()==mesh->positions.size(),"Newly fitted affine rig supports an actual pose without topology loss");
 const auto before=fixture().encode();auto cancelled=fixture();bool stopped=false;try{author_humanoid_fit(cancelled,1,marks(),{},[]{return true;});}catch(...){stopped=true;}check(stopped&&cancelled.encode()==before,"Cancellation preserves complete original Scene atomically");
 auto invalid=fixture();auto crossed=marks();crossed.points["leftHand"].x=-.8f;bool refused=false;try{author_humanoid_fit(invalid,1,crossed);}catch(...){refused=true;}check(refused&&invalid.encode()==before,"Invalid body-side markers refuse without partial replacement");
 auto distant=fixture();auto distant_marks=marks();distant_marks.points["head"].z=1;refused=false;try{author_humanoid_fit(distant,1,distant_marks);}catch(...){refused=true;}check(refused&&distant.encode()==before,"Exact WORLD surface proximity refuses distant markers atomically");
 auto fitted=scene.encode();refused=false;try{author_humanoid_fit(scene,1,marks());}catch(...){refused=true;}check(refused&&scene.encode()==fitted,"Existing rig is not silently replaced by newly fitted hierarchy");
 auto restored=Scene::decode(fitted);check(restored.encode()==fitted,"Fitted skeleton skin mapping landmarks and morphs survive exact archive restart");
 Document doc;auto native=fixture();auto initial=native.encode();doc.replace(native);check(doc.wait_mesh(),"Native fitting waits for its immutable original preview");
 auto source=doc.draft_scene().source;doc.fit_humanoid_rig(marks(),{},source);
 const auto save=std::filesystem::temp_directory_path()/("dome-rig-fit-native-"+std::to_string(getpid())+".dsculpt");doc.save(save);
 check(doc.wait_asset()&&doc.frame().state->error.empty()&&doc.copy_scene().active().deformation->skin,"Actual ordered native worker fits and binds the unskinned target");
 const auto accepted=doc.copy_scene().encode();check(Scene::load(save).encode()==accepted,"Save accepted immediately behind fitting stores the exact fitted endpoint");
 check(doc.undo()&&doc.copy_scene().encode()==initial,"One native Undo restores the complete unrigged source");
 check(doc.redo()&&doc.copy_scene().encode()==accepted,"One native Redo restores exact fitted rig mapping landmarks morphs and skin");
 bool stale=false;try{doc.fit_humanoid_rig(marks(),{},source);}catch(...){stale=true;}
 check(stale&&doc.copy_scene().encode()==accepted&&!doc.frame().state->asset_busy,"Stale source cannot enqueue another fitting or overwrite an existing rig");std::filesystem::remove(save);
 std::cout<<checks<<" humanoid rig-fit authoring checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
