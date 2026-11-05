// SPDX-License-Identifier: GPL-3.0-or-later
#define main preserved_legacy_visibility_diagnostic
#include "automatic-visibility-audit.cpp"
#undef main
#include "dome/document.hpp"
#include "dome/source-placement.hpp"
namespace {
size_t native_checks=0;void expect(bool value,const char*why){if(!value)throw std::runtime_error(why);++native_checks;std::cout<<"PASS "<<why<<std::endl;}
template<class F>bool rejected(F&&f){try{f();return false;}catch(const std::exception&){return true;}}
Vec3 at(const AffineTransform&m,Vec3 p){auto q=m.point({p.x,p.y,p.z});return {float(q[0]),float(q[1]),float(q[2])};}
struct NativeInput {Scene scene;Mesh reference;uint64_t target=0;};
NativeInput native_fixture(bool reflected){
 Mesh m;box(m,0);box(m,.003f);unwrap(m,128,2);m.validate();Scene scene;scene.active().name="Genuine unweighted native reference";scene.active().painted=std::make_shared<PaintedMesh>(m,128);scene.active().mesh_active=true;
 DeformationState d(m);Skeleton rig;for(float x:{0.f,.003f}){Bone b;b.name=rig.bones.empty()?"Native first control":"Native second control";b.rest.translation={x,.02f,0};b.length=.16f;rig.bones.push_back(b);}rig.inverse_bind_override=rig.inverse_bind();d.skeleton=rig;for(const auto&b:rig.bones)d.pose.push_back(b.rest);d.pose[0].translation.z+=.1f;d.pose[1].translation.z-=.1f;scene.set_deformation(1,d);
 auto g=scene.add_group("Immutable native reference placement");scene.active().group=g;
 auto frame=AffineTransform{}.matrix();if(reflected){frame[0]=1.5;frame[4]=.25;frame[5]=.75;frame[9]=.125;frame[10]=1.25;frame[12]=.2;frame[13]=-.1;}scene.group(g).affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(frame));
 auto target=scene.add("Unskinned native painted target",Volume{});scene.object(target).mesh_active=true;
 auto target_frame=AffineTransform{}.matrix();if(reflected){target_frame[0]=-2;target_frame[4]=.5;target_frame[5]=.5;target_frame[10]=.75;target_frame[12]=-.2;target_frame[14]=.1;}scene.object(target).affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(target_frame));
 auto relative=scene.world_matrix(target).inverse()*scene.world_matrix(1);auto t=m;
 for(size_t i=0;i<t.positions.size();++i){t.positions[i]=at(relative,m.positions[i]);auto q=relative.normal({m.normals[i].x,m.normals[i].y,m.normals[i].z});t.normals[i]=normalized(Vec3{float(q[0]),float(q[1]),float(q[2])});}
 if(relative.orientation()==AffineOrientation::Reversing)for(size_t c=0;c<t.indices.size();c+=3){std::swap(t.indices[c+1],t.indices[c+2]);if(!t.uv_seams.empty())std::swap(t.uv_seams[c],t.uv_seams[c+2]);}
 scene.object(target).painted=std::make_shared<PaintedMesh>(t,128);DeformationState shapes(t);shapes.morphs=MorphSet(t);auto edited=t.positions;for(auto&p:edited)p.z+=.0001f;shapes.morphs->set_target(t,"Actual authored native key",edited);shapes.morph_weights={.25f};scene.set_deformation(target,shapes);scene.select(target);scene.validate();return {std::move(scene),std::move(m),target};
}
}
int main(int argc,char**argv)try{
 if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);expect(int(SkinAuthoringMode::Automatic)==0&&int(SkinAuthoringMode::Transfer)==1&&int(SkinAuthoringMode::Anatomical)==4&&int(SkinAuthoringMode::SurfaceHeat)==5,"Additive surface heat preserves all explicit legacy method identities");
 nlohmann::json results=nlohmann::json::array();
 for(bool reflected:{false,true}){
  auto fixture=native_fixture(reflected);auto&scene=fixture.scene;const auto before=scene.encode();const auto target=fixture.target;const auto old=scene.object(target);const auto paint=old.painted;const auto mesh_owner=paint->mesh_owner();auto original_morph=old.deformation->morphs->encode();SkinAuthoringOptions options;options.reference=1;expect(options.mode==SkinAuthoringMode::SurfaceHeat,"Actual new request defaults to additive surface heat");
  expect(author_skin(scene,target,options),"Actual public default binds genuinely unskinned native geometry without appearance or source labels");const auto&object=scene.object(target);const auto&d=*object.deformation;expect(d.skin&&d.skeleton&&d.skeleton->inverse_bind_override&&object.rig_source,"Native new bind retains genuine exact rig/IBM and complete first source");expect(object.painted==paint&&object.painted->mesh_owner()==mesh_owner&&d.morphs->encode()==original_morph&&d.morph_weights==std::vector<float>{.25f},"Native paint/UV owner and active authored morph remain unchanged");
  auto retained=scene;retained.object(target).deformation=old.deformation;retained.object(target).rig_source=old.rig_source;expect(retained.encode()==before,"Only actual target skin/rig and first capsule change; reference/placement/groups stay exact");double wrong=0,error=0,mean=0;auto preview=d.preview(object.painted->mesh());const auto world=scene.world_matrix(target),reference_world=scene.world_matrix(1);
  for(size_t v=0;v<preview.positions.size();++v){const auto joint=fixture.reference.positions[v].x<.0015f?0u:1u;wrong=std::max(wrong,influence(d.skin->vertices[v],1-joint));const auto p=object.painted->mesh().positions[v]+d.morphs->targets[0].delta[v]*.25f;auto expected=world.point({p.x,p.y,p.z});const auto delta=reference_world.direction({0,0,joint==0?.1:-.1});for(size_t k=0;k<3;++k)expected[k]+=delta[k];const auto actual=world.point({preview.positions[v].x,preview.positions[v].y,preview.positions[v].z});const double e=std::hypot(actual[0]-expected[0],actual[1]-expected[1],actual[2]-expected[2]);error=std::max(error,e);mean+=e;}
  mean/=preview.positions.size();expect(wrong==0,"Actual native thin disconnected limbs with1mm gap have zero foreign weights");expect(error<3e-5&&mean<1e-5,"Native full affine/reflection/active-morph pose matches independent world/IBM rigid-region oracle");
  auto encoded=scene.encode();const auto file=out/(reflected?"affine.dsculpt":"native.dsculpt");scene.save(file);expect(Scene::load(file).encode()==encoded,"Actual native surface-heat Save/reopen is exact");auto lock=options;lock.reference=target;lock.locks.assign(d.skeleton->bones.size(),1);auto owner=object.deformation;expect(!author_skin(scene,target,lock)&&scene.object(target).deformation==owner&&scene.encode()==encoded,"Existing actual hard-locked field is an exact native factory no-op");
  auto reject=fixture.scene;reject.object(target).deformation=old.deformation;reject.object(target).rig_source=old.rig_source;const auto original=reject.encode();auto limited=options;limited.anatomical.maximum_work=1;expect(rejected([&]{author_skin(reject,target,limited);})&&reject.encode()==original,"Native owning work refusal preserves exact source/current graph");limited=options;limited.anatomical.maximum_total=320u*1024u*1024u;limited.anatomical.maximum_workspace=65536;expect(rejected([&]{author_skin(reject,target,limited);})&&reject.encode()==original,"Insufficient actual native workspace refuses without partial adoption");
  size_t callbacks=0;expect(rejected([&]{author_skin(reject,target,options,[&]{return ++callbacks==64;});})&&callbacks==64&&reject.encode()==original,"Actual generic native preparation cancels deterministically without publication");
  results.push_back({{"reflected",reflected},{"foreign_weight",wrong},{"max_pose_m",error},{"mean_pose_m",mean}});
 }
 std::ofstream(out/"report.json")<<nlohmann::json{{"checks",native_checks},{"work_limit",500'000'000},{"workspace_limit",256u*1024u*1024u},{"owning_limit",512u*1024u*1024u},{"records",results}}.dump(2);std::cout<<"PASS native_surface_heat_checks="<<native_checks<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
