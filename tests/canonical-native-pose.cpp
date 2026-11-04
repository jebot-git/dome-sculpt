// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_CANONICAL_CONVERSION_FAULT_HELPERS_ONLY
#include "canonical-normal-conversion-faults.cpp"
#include "canonical-native-pose.hpp"
#include "dome/source-uv-bake.hpp"
#include <type_traits>
using detail::CanonicalNativePose;
using detail::CanonicalNativePoseOptions;
static_assert(!std::is_default_constructible_v<CanonicalNativePose>);
static_assert(!std::is_copy_constructible_v<CanonicalNativePose>);
namespace {
unsigned count=0;
void pose_check(bool yes,const char *label){if(!yes)throw std::runtime_error(label);++count;std::cout<<"PASS "<<label<<'\n';}
bool pose_refuses(auto call){try{call();return false;}catch(const std::exception&){return true;}}
Scene set_pose(Scene s,int index,float weight){auto &o=s.active();auto d=*o.deformation;d.morph_weights.assign(d.morphs->targets.size(),weight);d.pose.clear();if(index){for(const auto &b:d.skeleton->bones)d.pose.push_back(b.rest);d.pose[0].scale=index==1?Vec3{1.1f,.85f,.55f}:Vec3{.8f,1.05f,1.6f};const float a=index==1?.2f:-.25f;d.pose[1].rotation={std::cos(a/2),0,std::sin(a/2),0};}s.set_deformation(o.id,d);return s;}
Scene split_uv(Scene s){auto &o=s.active();SourceUvBakeInput in;in.paint=o.painted;in.current_material=o.material_bindings;in.current_deformation=o.deformation;
 for(auto v:o.painted->mesh().indices){const auto &m=o.painted->mesh();in.target.positions.push_back(m.positions[v]);in.target.normals.push_back(m.normals[v]);auto uv=m.uv[v];in.target.uv.push_back({.1f+.8f*(1-uv[1]),.1f+.8f*uv[0]});in.target.indices.push_back(uint32_t(in.target.indices.size()));in.source_vertices.push_back(v);}
 auto a=adopt_source_uv_bake(bake_source_uv(in));o.painted=std::move(a.paint);o.material_bindings=std::move(a.material_bindings);o.deformation=std::move(a.deformation);return s;}
auto pose_signature(const auto &p){nlohmann::json j={{"p",nlohmann::json::array()},{"n",nlohmann::json::array()},{"uv",p->mesh().uv},{"indices",p->mesh().indices},{"T",p->tangents()},{"root",p->root_vertices()},{"bytes",p->working_bytes()}};for(auto v:p->mesh().positions)j["p"].push_back({v.x,v.y,v.z});for(auto v:p->mesh().normals)j["n"].push_back({v.x,v.y,v.z});return j;}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;auto scene=Scene::load(argv[1]);const auto exact=scene.encode();const auto root=scene.active().material_bindings->canonical_attributes;const auto &rest=root->before_paint()->mesh();
 const auto uv=split_uv(scene);double maximum=0;size_t corners=0;
 for(int pose=0;pose<3;++pose)for(float weight:{0.f,.35f,1.f}){
  auto original=set_pose(scene,pose,weight);const auto wanted=original.active().deformation->preview(rest);const auto wanted_t=tangent_corners(wanted);
  for(bool split:{false,true}){auto current=set_pose(split?uv:scene,pose,weight);const auto before=current.encode();auto produced=CanonicalNativePose::create(current.active());
   pose_check(produced->valid_owner(produced)&&produced->origin()==root,"Actual native pose factory retains genuine root ownership");
   double error=0;
   for(size_t c=0;c<produced->mesh().indices.size();++c){const auto v=produced->mesh().indices[c],source=rest.indices[c];
    error=std::max({error,double(length(produced->mesh().positions[v]-wanted.positions[source])),double(length(produced->mesh().normals[v]-wanted.normals[source]))});
    for(size_t k=0;k<4;++k)error=std::max(error,double(std::abs(produced->tangents()[c][k]-wanted_t[c][k])));++corners;
   }
   maximum=std::max(maximum,error);std::cout<<"ORIGINAL_NATIVE_POSE pose="<<pose<<" weight="<<weight<<" split="<<split<<" max="<<error<<std::endl;
   pose_check(error<=1e-6&&current.encode()==before,"Every original posed POSITION/NORMAL/Mikk corner is preserved through editable UV splitting");
  }
 }
 pose_check(maximum<=1e-6&&corners==108,"All actual curved skin/morph pose states retain original direction bases");
 auto current=set_pose(uv,2,.35f);auto produced=CanonicalNativePose::create(current.active());const auto positive=pose_signature(produced);
 auto mutable_d=std::make_shared<DeformationState>(*current.active().deformation);auto alias=current.active();alias.deformation=mutable_d;auto snapshot=CanonicalNativePose::create(alias);const auto before=pose_signature(snapshot);mutable_d->morph_weights[0]=.9f;mutable_d->pose[0].scale.x=2;
 pose_check(pose_signature(snapshot)==before,"Evaluated pose output retains no mutable deformation aliases");
 auto bad=current.active();auto b=std::make_shared<SourceMaterialBindings>(*bad.material_bindings);b->canonical_attributes=Scene::decode(exact).active().material_bindings->canonical_attributes;bad.material_bindings=b;
 pose_check(pose_refuses([&]{CanonicalNativePose::create(bad);}),"Same-value foreign canonical root cannot impersonate current UV/frame authority");
 bad=current.active();auto broken=std::make_shared<DeformationState>(*bad.deformation);broken->morphs->targets[0].delta[0].x+=.01f;bad.deformation=broken;
 pose_check(pose_refuses([&]{CanonicalNativePose::create(bad);}),"Changed morph POSITION channels cannot borrow old frame identities");
 CanonicalNativePoseOptions small;small.max_working_bytes=produced->working_bytes()-1;
 pose_check(pose_refuses([&]{CanonicalNativePose::create(current.active(),small);}),"Prospective full pose/smoothing/skin/Mikk memory forecast refuses before publication");
 small={};small.max_work=1;pose_check(pose_refuses([&]{CanonicalNativePose::create(current.active(),small);}),"Original source pose work has an explicit finite budget");
 auto make=[&]{return CanonicalNativePose::create(current.active());};make();conversion_fault::calls=0;conversion_fault::at=SIZE_MAX;conversion_fault::enabled=true;auto measured=make();conversion_fault::enabled=false;const auto allocations=conversion_fault::calls;
 pose_check(allocations>0&&allocations<20000,"Measure real source-pose factory C++ allocation prefixes");
 for(size_t cut=1;cut<=allocations;++cut){conversion_fault::calls=0;conversion_fault::at=cut;conversion_fault::hit=false;bool caught=false;conversion_fault::enabled=true;try{make();}catch(const std::bad_alloc&){caught=true;}conversion_fault::enabled=false;
  if(!caught||!conversion_fault::hit||pose_signature(make())!=positive||scene.encode()!=exact)throw std::runtime_error("Original pose allocation failure lost immutable source or exact retry");}
 size_t polls=0;CanonicalNativePoseOptions o;o.cancelled=[&]{++polls;return false;};CanonicalNativePose::create(current.active(),o);
 for(size_t cut=1;cut<=polls;++cut){size_t at=0;o.cancelled=[&]{return ++at>=cut;};if(!pose_refuses([&]{CanonicalNativePose::create(current.active(),o);})||scene.encode()!=exact)throw std::runtime_error("Original source pose cancellation changed native input");}
 pose_check(pose_signature(make())==positive&&scene.encode()==exact,"Every measured allocation/cancellation endpoint preserves exact original scene and retry");
 std::cout<<"PASS "<<count<<" authentic native pose checks; corners="<<corners<<" maximum="<<maximum<<" persistent_cpp_prefixes="<<allocations<<" all_cancel_polls="<<polls<<'\n';return 0;
}catch(const std::exception&e){conversion_fault::enabled=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
