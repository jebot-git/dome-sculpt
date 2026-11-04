// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_BAKE_CAGE_HELPERS_ONLY
#include "bake-cage.cpp"
#include "dome/native-authoring.hpp"
#include "../third_party/cgltf/cgltf.h"
#include <fstream>
namespace {
Mesh mirrored_fan(){
 Mesh m;m.positions={{0,0,0},{.1f,0,0},{0,.1f,0},{-.1f,0,0},{0,-.1f,0}};
 m.normals.assign(5,{0,0,1});m.uv={{{.5f,.5f}},{{.9f,.5f}},{{.5f,.9f}},{{.5f,.1f}},{{.1f,.5f}}};m.indices={0,1,2,0,3,4};m.uv_pins={1,0,0,0,0};return m;
}
Scene baked_fan(bool skin,bool morph,bool curved=false){
 Scene s;const auto target=s.active().id;auto mesh=mirrored_fan();s.active().name="Mirrored tangent islands";s.active().mesh_active=true;s.active().painted=std::make_shared<PaintedMesh>(mesh,64);
 if(curved){
  mesh.positions[1].z=.015f;mesh.positions[2].z=-.01f;mesh.positions[3].z=.01f;mesh.positions[4].z=-.015f;
  mesh.normals={normalized(Vec3{.1f,.1f,1}),normalized(Vec3{-.6f,.2f,1}),normalized(Vec3{.3f,.7f,1}),normalized(Vec3{.6f,-.4f,1}),normalized(Vec3{-.3f,-.8f,1})};
  s.active().painted=std::make_shared<PaintedMesh>(mesh,64);
 }
 if(skin||morph){DeformationState d(mesh);
  if(skin){d.skeleton=Skeleton{{{"root",-1,{},.2f},{"tip",0,{},.1f}}};d.skeleton->bones[1].rest.translation={0,.025f,0};d.skin=SkinWeights{};for(size_t i=0;i<mesh.positions.size();++i)d.skin->vertices.push_back(i%2?VertexWeights{{1,1}}:VertexWeights{{0,.7f},{1,.3f}});}
  if(morph){d.morphs=MorphSet(mesh);auto positions=mesh.positions;constexpr float angle=.4f;for(auto &p:positions)p={std::cos(angle)*p.x-std::sin(angle)*p.y,std::sin(angle)*p.x+std::cos(angle)*p.y,p.z};d.morphs->set_target(mesh,"In-plane rotation",positions);d.morph_weights={0};}
  s.set_deformation(target,d);
 }
 auto high=mesh;for(auto &p:high.positions)p.z=.01f;high.normals.assign(5,{.3f,.4f,std::sqrt(.75f)});
 if(curved){high.positions={{-.3f,-.3f,.02f},{.3f,-.3f,.02f},{.3f,.3f,.02f},{-.3f,.3f,.02f}};high.normals.assign(4,{.3f,.4f,std::sqrt(.75f)});high.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};high.indices={0,1,2,0,2,3};high.uv_pins.clear();}
 s.add("Unchanged source",Volume{});s.active().mesh_active=true;s.active().visible=false;s.active().locked=true;s.active().painted=std::make_shared<PaintedMesh>(high,64);s.select(target);
 ObjectBakeSettings settings;settings.ao_samples=0;settings.padding=0;auto report=bake_object(s,2,settings);require_cage(report.projected>100,"Actual native baker covers both disjoint mirrored-UV triangles");return s;
}
const cgltf_accessor *attribute(const cgltf_attribute *values,size_t count,cgltf_attribute_type type){for(size_t i=0;i<count;++i)if(values[i].type==type&&values[i].index==0)return values[i].data;return nullptr;}
std::array<float,4> read(const cgltf_accessor *a,size_t v){std::array<float,4> value{};if(!a||!cgltf_accessor_read_float(a,v,value.data(),value.size()))throw std::runtime_error("Independent consumer cannot read attribute");return value;}
bool close(Vec3 a,Vec3 b,float tolerance=2e-6f){return length(a-b)<=tolerance;}
Vec3 vector(std::array<float,4> a){return {a[0],a[1],a[2]};}
}
int main(int argc,char **argv)try{
 const auto out=std::filesystem::path(argc>1?argv[1]:"baked-normal-export");std::filesystem::create_directories(out);
 for(int variant=0;variant<8;++variant){auto scene=baked_fan(variant&1,variant&2,variant&4);const auto before=scene.encode();const auto source=scene.object(2).painted;const auto paint=scene.active().painted;const auto deformation=scene.active().deformation;
  NativeAuthoringExport owner;owner.scene=std::make_shared<const Scene>(scene.snapshot());owner.allow_portable_fallback=true;owner.scope=NativeAuthoringExport::Scope::WholeScene;
  const auto file=out/("fan-"+std::to_string(variant)+".glb");export_glb_authoring(owner,file);
  require_cage(scene.encode()==before&&scene.object(2).painted==source&&scene.active().painted==paint&&scene.active().deformation==deformation,"Actual tangent export preserves complete native source, current data and immutable owners");
  std::ifstream input(file,std::ios::binary);std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input),{}};auto recovered=extract_native_authoring(bytes);require_cage(recovered&&recovered->encode()==before,"Native recipe, mirrored topology, pins, skin and morph data recover exactly from the real GLB");
  cgltf_options options{};cgltf_data *raw=nullptr;
  if(cgltf_parse_file(&options,file.c_str(),&raw)!=cgltf_result_success)throw std::runtime_error("Independent cgltf parse failed");
  std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data(raw,cgltf_free);
  if(cgltf_load_buffers(&options,raw,file.c_str())!=cgltf_result_success||cgltf_validate(raw)!=cgltf_result_success)throw std::runtime_error("Independent cgltf validation failed");
  require_cage(raw->meshes_count==1&&raw->meshes[0].primitives_count==1,"Portable view contains precisely the visible native target");const auto &primitive=raw->meshes[0].primitives[0];
  require_cage(primitive.material&&primitive.material->normal_texture.texture&&primitive.material->normal_texture.texcoord==0&&primitive.material->normal_texture.scale==1,
    "Real canonical normal texture remains bound to UV0 at unit strength alongside explicit tangents");
  const auto p=attribute(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_position),n=attribute(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_normal),t=attribute(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_tangent);
  require_cage(p&&n&&t&&p->count==6&&t->count==6&&primitive.indices->count==6,"Shared vertex is split into two genuine tangent handedness domains");
  if(variant&4)continue; // Analytic planar oracle above remains separate from the curved image consumer.
  std::array<bool,2> origin_signs{};
  for(size_t c=0;c<6;++c){auto v=cgltf_accessor_read_index(primitive.indices,c);auto P=read(p,v),N=read(n,v),T=read(t,v);const size_t original=scene.active().painted->mesh().indices[c];
   const Vec3 expected=c<3?Vec3{1,0,0}:Vec3{0,1,0};const float sign=c<3?1.f:-1.f;
   require_cage(close(vector(P),scene.active().painted->mesh().positions[original])&&close(vector(N),{0,0,1})&&close(vector(T),expected)&&T[3]==sign,"Independent analytic UV derivatives recover exact exported POSITION/NORMAL/TANGENT and handedness");
   if(original==0)origin_signs[c<3?0:1]=T[3]==sign;
   if(variant&1){const auto joints=attribute(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_joints),weights=attribute(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_weights);auto J=read(joints,v),W=read(weights,v);const auto &expected_weights=deformation->skin->vertices[original];
    bool same=true;for(size_t k=0;k<expected_weights.size();++k)same&=J[k]==expected_weights[k].joint&&std::abs(W[k]-expected_weights[k].weight)<1e-6f;for(size_t k=expected_weights.size();k<4;++k)same&=W[k]==0;
    require_cage(same,"Each duplicated tangent vertex retains every original joint and weight without averaging");}
   if(variant&2){require_cage(primitive.targets_count==1,"Native morph slot remains independently selectable");const auto &target=primitive.targets[0];const auto position=attribute(target.attributes,target.attributes_count,cgltf_attribute_type_position),tangent=attribute(target.attributes,target.attributes_count,cgltf_attribute_type_tangent);auto D=read(position,v),DT=read(tangent,v);const Vec3 turned{std::cos(.4f)*expected.x-std::sin(.4f)*expected.y,std::sin(.4f)*expected.x+std::cos(.4f)*expected.y,0};
    require_cage(close(vector(D),deformation->morphs->targets[0].delta[original])&&close(vector(DT),turned-expected),"Analytic rotated endpoint preserves full POSITION and generated TANGENT morph deltas across both islands");}
  }
  require_cage(origin_signs[0]&&origin_signs[1],"Both copies of the original shared vertex retain opposite tangent signs");
 }
 std::cout<<"PASS "<<cage_checks<<" independent baked tangent export checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
