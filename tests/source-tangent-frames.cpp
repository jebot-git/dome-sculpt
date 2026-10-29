// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/source-tangent-frames.hpp"
#include "dome/gltf-export.hpp"
#include "dome/humanoid-creator.hpp"
#include "cgltf.h"
#include <iostream>
#include <unistd.h>
using namespace dome;
using namespace dome::detail;
namespace {
void check(bool b,const char *s) { if(!b)throw std::runtime_error(s);std::cout<<"PASS "<<s<<'\n'; }
template<class F> bool rejects(F f) {try{f();return false;}catch(const std::exception&){return true;}}
Vec3 xyz(Tangent t) {return {t[0],t[1],t[2]};}
Tangent analytic(const Mesh &m,std::span<const std::array<float,2>> uv) {
  auto a=m.indices[0],b=m.indices[1],c=m.indices[2];
  auto p=m.positions[b]-m.positions[a],q=m.positions[c]-m.positions[a];
  float ux=uv[b][0]-uv[a][0],uy=uv[c][0]-uv[a][0],vx=uv[b][1]-uv[a][1],vy=uv[c][1]-uv[a][1];
  float d=ux*vy-uy*vx;auto du=(p*vy-q*vx)/d,dv=(q*ux-p*uy)/d,n=normalized(m.normals[a]);
  auto t=normalized(du-n*dot(n,du));return {t.x,t.y,t.z,dot(cross(n,t),dv)<0?-1.f:1.f};
}
Vec3 world(Vec3 tangent,Vec3 normal,float hand,Vec3 sample,Vec3 scale) {
  auto n=normalized(Vec3{normal.x/scale.x,normal.y/scale.y,normal.z/scale.z});
  auto t=normalized(Vec3{tangent.x*scale.x,tangent.y*scale.y,tangent.z*scale.z});
  auto b=normalized(cross(n,t))*(hand*(scale.x*scale.y*scale.z<0?-1.f:1.f));
  return normalized(t*sample.x+b*sample.y+n*sample.z);
}
}
int main() try {
  ImportedPrimitive p;p.mesh.positions={{0,0,0},{1,0,0},{0,1,0}};p.mesh.normals.assign(3,{0,0,1});p.mesh.indices={0,1,2};
  constexpr float c=.7071067811865475f;p.mesh.uv={{{0,0}},{{c,c}},{{-c,c}}};
  ImportedMorphTarget target;target.positions={{0,0,0},{0,0,.5f},{0,0,0}};
  auto n=normalized(Vec3{-.5f,0,1});target.normals.assign(3,n-Vec3{0,0,1});
  // Deliberately unrelated: missing-base rules require ignoring this channel.
  target.tangents.assign(3,{999,888,777});p.morph_targets={target};
  auto frames=source_tangent_frames(p,p.mesh.uv);
  auto a=analytic(p.mesh,p.mesh.uv);auto posed=p.mesh;posed.positions[1].z=.5f;posed.normals.assign(3,n);auto b=analytic(posed,p.mesh.uv);
  check(frames.generated && frames.base.size()==3 && length(xyz(frames.base[0])-xyz(a))<1e-5f && frames.base[0][3]==a[3],
        "Original-frame base Mikk agrees with independent rotated-UV triangle derivative");
  check(frames.targets.size()==1 && frames.targets[0].size()==3 &&
          length(xyz(frames.base[0])+frames.targets[0][0]-xyz(b))<1e-5f,
        "Generated base produces target tangent deltas from target positions and normals, ignoring supplied deltas");
  std::array<float,1> local_weight{.35f};auto local=source_local_frame(frames,0,local_weight);
  check(length(local.tangent-(xyz(a)+(xyz(b)-xyz(a))*.35f))<1e-5f &&
        length(local.normal-(Vec3{0,0,1}+(n-Vec3{0,0,1})*.35f))<1e-6f && local.handedness==a[3],
        "Local frame evaluation preserves simultaneous authored normal and generated tangent morphs before skinning");
  auto authored=p;authored.tangents.assign(3,{-1,0,0,-1});
  auto exact=source_tangent_frames(authored,{});
  check(!exact.generated && exact.base[0]==authored.tangents[0] && length(exact.targets[0][0]-target.tangents[0])==0,
        "Authored base and tangent morphs retain their exact values independently of sampling UVs");
  authored.morph_targets[0].tangents.clear();
  check(source_tangent_frames(authored,{}).targets[0].empty(),
        "Authored base with absent tangent morph keeps the base tangent intact");
  auto mirrored=p;for(auto &uv:mirrored.mesh.uv)uv[0]=-uv[0];
  check(source_tangent_frames(mirrored,mirrored.mesh.uv).base[0][3]==-frames.base[0][3],
        "Original mirrored UV handedness survives generated tangent transport");
  auto without_normals=p;without_normals.morph_targets[0].normals.clear();
  auto kept_normals=source_tangent_frames(without_normals,without_normals.mesh.uv);
  auto base_normal_pose=posed;base_normal_pose.normals=p.mesh.normals;
  check(length(xyz(kept_normals.base[0])+kept_normals.targets[0][0]-xyz(analytic(base_normal_pose,p.mesh.uv)))<1e-5f,
        "Absent normal morph keeps the specified base normals when generating target tangents");
  Vec3 sample=normalized(Vec3{.6f,.2f,.77f});
  auto old_rest=world(xyz(a),{0,0,1},a[3],sample,{1,1,1});
  auto atlas=p.mesh;atlas.uv={{{0,0}},{{1,0}},{{0,1}}};auto canonical=analytic(atlas,atlas.uv);
  auto rest_b=cross(Vec3{0,0,1},xyz(canonical))*canonical[3];
  Vec3 converted{dot(old_rest,xyz(canonical)),dot(old_rest,rest_b),old_rest.z};
  check(length(world(xyz(a),{0,0,1},a[3],sample,{2,.7f,1.4f})-
               world(xyz(canonical),{0,0,1},canonical[3],converted,{2,.7f,1.4f}))>.03f,
        "Independent anisotropic pose fixture detects invalid static rest-frame normal conversion");
  // Materialize the generated original frame into preserved source accessors,
  // then independently parse actual exported GLB through cgltf.
  auto source=std::make_shared<ImportedAsset>();source->mode=ImportMode::PreserveGltf;
  p.tangents=frames.base;p.morph_targets[0].tangents=frames.targets[0];source->primitives={p};
  auto appearance=std::make_shared<CreatorAppearance>();appearance->asset=source;appearance->source_vertices={0,1,2};appearance->source_morph_targets={0};appearance->source_basis_quaternion=Quat{};
  ObjectSurface object;object.mesh=p.mesh;object.appearance=appearance;object.name="Original tangent frame";
  auto d=std::make_shared<DeformationState>(object.mesh);d->morphs=MorphSet(object.mesh);d->morphs->set_target(object.mesh,"Pose",posed.positions);object.deformation=d;
  auto path=std::filesystem::temp_directory_path()/("dome-source-frame-"+std::to_string(getpid())+".glb");export_glb({object},path);
  cgltf_options options{};cgltf_data *raw=nullptr;
  if(cgltf_parse_file(&options,path.c_str(),&raw)!=cgltf_result_success)throw std::runtime_error("cgltf parse failed");
  std::unique_ptr<cgltf_data,decltype(&cgltf_free)> parsed(raw,cgltf_free);
  check(cgltf_load_buffers(&options,raw,path.c_str())==cgltf_result_success && cgltf_validate(raw)==cgltf_result_success,
        "Independent cgltf validates emitted original base and target tangent accessors");
  auto &primitive=raw->meshes[0].primitives[0];auto tangent=cgltf_find_accessor(&primitive,cgltf_attribute_type_tangent,0);cgltf_accessor *delta=nullptr;
  for(size_t i=0;i<primitive.targets[0].attributes_count;++i)if(primitive.targets[0].attributes[i].type==cgltf_attribute_type_tangent)delta=primitive.targets[0].attributes[i].data;
  float t[4]{},dt[3]{};if(!tangent || !delta || !cgltf_accessor_read_float(tangent,0,t,4) || !cgltf_accessor_read_float(delta,0,dt,3))throw std::runtime_error("cgltf tangent accessor missing");
  for(float weight:{0.f,.35f,1.f})for(Vec3 scale:{Vec3{2,.7f,1.4f},Vec3{-2,.7f,1.4f}}) {
    auto exported=world(Vec3{t[0],t[1],t[2]}+Vec3{dt[0],dt[1],dt[2]}*weight,Vec3{0,0,1}+(n-Vec3{0,0,1})*weight,t[3],sample,scale);
    auto expected=world(xyz(a)+(xyz(b)-xyz(a))*weight,Vec3{0,0,1}+(n-Vec3{0,0,1})*weight,a[3],sample,scale);
    if(length(exported-expected)>1e-5f)throw std::runtime_error("Analytic parsed morph/anisotropic/reflection world normal differs");
  }
  check(true,"Parsed GLB preserves analytic original-frame world normals under nonzero morphs, anisotropic transforms and reflection");
  check(rejects([&]{source_tangent_frames(p,{},[]{return true;});}) &&
        rejects([&]{source_tangent_frames(p,{}, {},2);}),
        "Original tangent preparation enforces cancellation and bounded corner work");
  std::filesystem::remove(path);return 0;
} catch(const std::exception &e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
