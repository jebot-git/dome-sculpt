#pragma once
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
