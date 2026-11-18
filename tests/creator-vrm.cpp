// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/creation.hpp"
#include "dome/vrm.hpp"
#include "dome/document.hpp"
#include "dome/rig-edit.hpp"
#include "dome/source-maps.hpp"
#include "dome/material-bindings.hpp"
#include "humanoid-sha256.hpp"
#include "cgltf.h"
#include <png.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <set>
using namespace dome;
namespace {
size_t checks=0;
void check(bool x,const char *why){if(!x)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<'\n';}
#include "creator-export-colour.hpp"
Vec3 flip(Vec3 p){return {-p.x,p.y,-p.z};}
Vec3 unity(Vec3 p){return {p.x,p.y,-p.z};}
Vec3 xyz(const nlohmann::json &j){return {j.value("x",0.f),j.value("y",0.f),j.value("z",0.f)};}
Vec3 position(const ImportedAsset&a,size_t i){const auto &m=a.nodes.at(i).world;return {m[12],m[13],m[14]};}
float smooth(float a,float b,float x){float t=std::clamp((x-a)/(b-a),0.f,1.f);return t*t*(3-2*t);}
// Independent test-only implementation of the documented creator equation.
// It consumes original asset-node/primitive data, not export adapter results,
// its interval bounds, refitted metadata or the exported rest joint positions.
struct ConsumerShape {
 const HumanoidControls &c;Vec3 hips,neck,head,left_eye,right_eye;float floor=1000,scale=1;
 ConsumerShape(const HumanoidControls &controls,const ImportedAsset &asset,const nlohmann::json &vrm):c(controls){
  auto anchor=[&](const char*role){for(const auto &b:vrm.at("humanoid").at("humanBones"))if(b.at("bone")==role)return flip(position(asset,b.at("node")));throw std::runtime_error("Source humanoid role missing");};
  hips=anchor("hips");neck=anchor("neck");head=anchor("head");left_eye=anchor("leftEye");right_eye=anchor("rightEye");float top=-1000;
  for(const auto &p:asset.primitives)for(auto v:p.mesh.indices){auto q=raw(p.mesh.positions[v]);floor=std::min(floor,q.y);top=std::max(top,q.y);}scale=c.height/(top-floor);
 }
  Vec3 raw(Vec3 original) const {
    Vec3 p = flip(original);
    float torso = smooth(hips.y - .05f, hips.y + .08f, p.y) *
                  (1 - smooth(neck.y - .08f, neck.y, p.y));
    float upper = smooth(hips.y + .1f, neck.y - .05f, p.y) *
                  (1 - smooth(neck.y, neck.y + .04f, p.y));
    float waist = std::exp(-std::pow((p.y - (hips.y + .12f)) / .12f, 2.f));
    p.x *= 1 + c.build * .08f * torso + c.shoulder_width * .1f * upper +
           c.waist_width * .12f * waist;
    p.z *= 1 + c.build * .10f * torso;
    if (p.y < hips.y)
      p.y = hips.y + (p.y - hips.y) * (1 + c.leg_length * .12f);
    float shoulder = std::max(.08f, std::abs(left_eye.x) * 3.5f);
    if (std::abs(p.x) > shoulder)
      p.x += (p.x < 0 ? -1.f : 1.f) * (std::abs(p.x) - shoulder) *
             c.arm_length * .12f;
    float h = smooth(neck.y - .015f, neck.y + .04f, p.y) *
              (1 - smooth(.12f, .22f, std::abs(p.x)));
    p.x *= 1 + h * (c.head_width * .10f + c.anime * .08f);
    p.y += h * (p.y - neck.y) * (c.head_height * .08f + c.anime * .06f);
    p.z += h * (p.z - head.z) * c.anime * .04f;
    for (auto eye : {left_eye, right_eye}) {
      Vec3 d = p - eye;
      float rx = std::max(.025f, std::abs(left_eye.x - right_eye.x) * .75f),
            ry = .045f;
      float w = std::exp(-(d.x * d.x / (rx * rx) + d.y * d.y / (ry * ry))) *
                smooth(0, .1f, p.z - head.z);
      p.x += (d.x * c.eye_size * .18f +
              (eye.x < 0 ? -1.f : 1.f) * c.eye_spacing * .008f) *
             w;
      p.y += d.y * c.eye_size * .22f * w;
    }
    float nose = std::exp(-std::pow(p.x / .025f, 2.f) -
                          std::pow((p.y - (left_eye.y - .035f)) / .035f, 2.f));
    p.z += (p.z - head.z) * c.nose_size * .15f * nose;
    float mouth = std::exp(-std::pow(p.x / .055f, 2.f) -
                           std::pow((p.y - (left_eye.y - .075f)) / .025f, 2.f));
    p.x *= 1 + c.mouth_width * .12f * mouth;
    float jaw = std::exp(-std::pow((p.y - (head.y - .01f)) / .045f, 2.f));
    p.x *= 1 + c.jaw_width * .08f * jaw;
    return p;
  }

 Vec3 operator()(Vec3 p)const{p=raw(p);return {p.x*scale,(p.y-floor)*scale,p.z*scale};}
};
Vec3 transform(const float *m,Vec3 p){return {m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14]};}
Vec3 consume(cgltf_node &node,size_t vertex){
 const auto &p=node.mesh->primitives[0];float q[4]{};
 if(!cgltf_accessor_read_float(cgltf_find_accessor(&p,cgltf_attribute_type_position,0),vertex,q,3))throw std::runtime_error("Consumer POSITION missing");Vec3 pos{q[0],q[1],q[2]};
 for(size_t t=0;t<p.targets_count;++t){float w=t<node.weights_count?node.weights[t]:t<node.mesh->weights_count?node.mesh->weights[t]:0;for(size_t a=0;a<p.targets[t].attributes_count;++a)if(p.targets[t].attributes[a].type==cgltf_attribute_type_position){if(!cgltf_accessor_read_float(p.targets[t].attributes[a].data,vertex,q,3))throw std::runtime_error("Consumer morph missing");pos+=Vec3{q[0],q[1],q[2]}*w;}}
 if(!node.skin)throw std::runtime_error("Creator part lost skin");Vec3 result{};float total=0;
 for(int set=0;;++set){auto *j=cgltf_find_accessor(&p,cgltf_attribute_type_joints,set),*w=cgltf_find_accessor(&p,cgltf_attribute_type_weights,set);if(!j&&!w)break;float js[4],ws[4];if(!j||!w||!cgltf_accessor_read_float(j,vertex,js,4)||!cgltf_accessor_read_float(w,vertex,ws,4))throw std::runtime_error("Consumer skin streams missing");for(size_t k=0;k<4;++k)if(ws[k]){if(js[k]<0||js[k]>=node.skin->joints_count)throw std::runtime_error("Consumer joint outside skin");float world[16],bind[16];cgltf_node_transform_world(node.skin->joints[size_t(js[k])],world);if(!cgltf_accessor_read_float(node.skin->inverse_bind_matrices,size_t(js[k]),bind,16))throw std::runtime_error("Consumer IBM missing");result+=transform(world,transform(bind,pos))*ws[k];total+=ws[k];}}
 if(std::abs(total-1)>1e-5f)throw std::runtime_error("Consumer weights not normalized");return result;
}
std::string scene_hash(const Scene &s){auto bytes=s.encode().dump();return creator_detail::sha256(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size()));}
void near(Vec3 a,Vec3 b){if(length(a-b)>2e-5f)throw std::runtime_error("Independent metadata point disagrees with actual shape");}
void verify(const Scene &scene,const CreatedHumanoid &model,const std::filesystem::path &file){
 const auto before=scene_hash(scene);auto surfaces=scene.surfaces(true);auto report=export_vrm(surfaces,file);check(report.meshes==20,"Styled body and generated groom export together as twenty real meshes");
 cgltf_options options{};cgltf_data *raw=nullptr;check(cgltf_parse_file(&options,file.c_str(),&raw)==cgltf_result_success,"Independent cgltf parses creator VRM");std::unique_ptr<cgltf_data,decltype(&cgltf_free)>data(raw,cgltf_free);
 check(cgltf_load_buffers(&options,raw,file.c_str())==cgltf_result_success&&cgltf_validate(raw)==cgltf_result_success,"Independent cgltf validates actual creator graph, accessors, skin and morph streams");
 auto document=nlohmann::json::parse(raw->json,raw->json+raw->json_size);const auto &vrm=document.at("extensions").at("VRM");
 size_t parts=0,corners=0;float maximum=0;
 for(size_t n=0;n<raw->nodes_count;++n){auto &node=raw->nodes[n];if(!node.mesh)continue;size_t index=size_t(node.mesh-raw->meshes);const auto &surface=surfaces.at(index);const auto mesh=surface.preview_mesh();const auto world=surface.world_matrix();const auto &p=node.mesh->primitives[0];check(p.indices&&p.indices->count==mesh.indices.size(),"Original and generated ordered triangle corners survive VRM export");
  for(size_t c=0;c<mesh.indices.size();++c){auto v=mesh.positions[mesh.indices[c]];auto expected=world.point({v.x,v.y,v.z});auto actual=consume(node,cgltf_accessor_read_index(p.indices,c));float error=length(actual-flip({float(expected[0]),float(expected[1]),float(expected[2])}));maximum=std::max(maximum,error);if(error>3e-5f)throw std::runtime_error("Consumer posed own-IBM corner differs from native creator");++corners;}
  consumer_uv(*raw,p.material,surface.mesh);++parts;
 }
 check(parts==20&&corners>10000,"All body and groom corners independently deform with current rig, own inverse binds and facial targets");
 const auto &appearance=*model.parts.front().appearance;const auto &asset=*appearance.asset;const auto original=nlohmann::json::parse(*appearance.vrm_json);ConsumerShape warp(model.controls,asset,original);
 std::map<uint32_t,size_t> nodes;for(size_t i=0;i<appearance.source_joint_nodes.size();++i){const auto source=appearance.source_joint_nodes[i];const auto *skin=raw->skins;nodes[source]=size_t(skin->joints[i]-raw->nodes);}
 auto emitted_world=[&](size_t source,Vec3 p){float matrix[16];cgltf_node_transform_world(raw->nodes+nodes.at(uint32_t(source)),matrix);return transform(matrix,p);};
 auto native_world=[&](Vec3 p){const auto w=surfaces.front().world_matrix().point({p.x,p.y,p.z});return flip({float(w[0]),float(w[1]),float(w[2])});};
 const auto &first=original.at("firstPerson"),&nowfirst=vrm.at("firstPerson");auto fp=first.at("firstPersonBone").get<size_t>();check(nowfirst.at("firstPersonBone")==nodes.at(uint32_t(fp)),"First-person refers to the exact actual emitted humanoid bone");
 near(emitted_world(fp,unity(xyz(nowfirst.at("firstPersonBoneOffset")))) ,native_world(warp(position(asset,fp)+unity(xyz(first.at("firstPersonBoneOffset"))))));check(true,"First-person original Unity bone-local point crosses original rest, actual nonlinear warp and current exported rest");
 size_t spheres=0,samples=0;float smallest_margin=1e6f;
 const auto &oldgroups=original.at("secondaryAnimation").at("colliderGroups"),&newgroups=vrm.at("secondaryAnimation").at("colliderGroups");
 for(size_t g=0;g<oldgroups.size();++g){const auto &old=oldgroups[g],&next=newgroups[g];auto source=old.at("node").get<size_t>();check(next.at("node")==nodes.at(uint32_t(source)),"Collider attachment retains genuine source joint identity");
  for(size_t i=0;i<old.at("colliders").size();++i){const auto &a=old.at("colliders")[i],&b=next.at("colliders")[i];auto center=position(asset,source)+unity(xyz(a.at("offset")));auto expected=native_world(warp(center));auto actual=emitted_world(source,unity(xyz(b.at("offset"))));near(actual,expected);float r=a.at("radius"),emitted=b.at("radius");check(emitted>0&&emitted<r*warp.scale*8+1e-4f,"Conservative collider radius remains finite and bounded");
   // Independent surface and interior witnesses test containment, not the
   // interval implementation or its own claimed error estimates.
   for(size_t k=0;k<4096;++k){float y=1-2*(k+.5f)/4096,angle=float(k)*2.39996323f,rr=std::sqrt(1-y*y);auto d=Vec3{rr*std::cos(angle),y,rr*std::sin(angle)};for(float interior:{.25f,.65f,1.f}){auto p=native_world(warp(center+d*(r*interior)));float scale=scene.groups().front().transform.scale;float margin=emitted*scale-length(p-actual);smallest_margin=std::min(smallest_margin,margin);if(margin < -2e-5f)throw std::runtime_error("Shaped source sphere escapes emitted conservative collider");++samples;}}
   ++spheres;
  }
 }
 check(spheres>0&&samples>10000,"Independent dense surface/interior samples lie inside every actual conservative shaped collider");
 const auto &oldbones=original.at("secondaryAnimation").at("boneGroups"),&newbones=vrm.at("secondaryAnimation").at("boneGroups");
 for(size_t g=0;g<oldbones.size();++g){auto before=oldbones[g],after=newbones[g];check(after.at("hitRadius").get<float>()>=0,"Refitted spring hit radius is finite and nonnegative");for(size_t r=0;r<before.at("bones").size();++r){const auto source=before.at("bones")[r].get<uint32_t>();check(after.at("bones")[r]==nodes.at(source),"Spring root remains the genuine emitted joint");}
  before.erase("bones");after.erase("bones");before.erase("center");after.erase("center");before.erase("hitRadius");after.erase("hitRadius");check(before==after,"Spring authored force, drag, gravity, collider membership and tuning survive metadata adaptation");
 }
 auto source_hair=std::find_if(model.parts.begin(),model.parts.end(),[](const auto &p){return p.appearance&&p.name.find("HAIR")!=std::string::npos;});check(source_hair!=model.parts.end(),"Authentic original hair is present independently of generated groom");consumer_vrm_colours(*raw,*source_hair,*model.controls.hair_recolor);
 bool groom=false;for(size_t m=0;m<document.at("meshes").size();++m){const auto &mesh=document.at("meshes")[m];if(!mesh.contains("extras")||!mesh.at("extras").contains("DomeSculptGeneratedPart"))continue;groom=true;const auto material=mesh.at("primitives")[0].at("material").get<size_t>();const auto &typed=vrm.at("materialProperties")[material];check(typed.at("name").get<std::string>().find("Dome generated groom")==0,"Generated groom has explicit new MToon material identity");const auto texture=typed.at("textureProperties").at("_EmissionMap").get<size_t>();const auto &v=typed.at("vectorProperties").at("_EmissionColor");std::array<float,4>factor{v[0].get<float>(),v[1].get<float>(),v[2].get<float>(),1};consumer_pixels(consumer_png(raw->textures+texture),*model.parts.back().source_maps->color.image,factor,{1,1,1},true,"generated groom unlit MToon emission");check(typed.at("vectorProperties").at("_Color")[0]==0&&typed.at("vectorProperties").at("_ShadeColor")[0]==0,"Generated unlit groom cannot acquire doubled lit base or shade energy");}
 check(groom,"Generated groom carries explicit authored genesis, without invented original appearance or source material");check(scene_hash(scene)==before,"Export preserves exact immutable native source, geometry, images, rig, morph and paint state");std::cout<<"CONSUMER corners="<<corners<<" maximum="<<maximum<<" collider_samples="<<samples<<" minimum_margin="<<smallest_margin<<'\n';
}
void refuses(const Scene &s,const std::filesystem::path &p,const char *why){bool rejected=false;try{export_vrm(s.surfaces(true),p);}catch(const std::exception &e){std::cout<<"EXPECTED_REFUSAL "<<e.what()<<'\n';rejected=true;}check(rejected,why);}
}
int main(int argc,char **argv){try{if(argc!=2)throw std::runtime_error("usage: creator-vrm OUTPUT");std::cout.setf(std::ios::unitbuf);std::filesystem::path root=argv[1];std::filesystem::create_directories(root);
 for(const char *baseline:{"vroid-beta-female","vroid-beta-male"}) {
  auto c=humanoid_preset(HumanoidPreset::Anime,baseline);c.height=1.85f;c.build=.45f;c.shoulder_width=-.25f;c.waist_width=.3f;c.leg_length=.35f;c.arm_length=-.2f;c.head_height=.2f;c.eye_spacing=.25f;c.nose_size=-.35f;c.mouth_width=.3f;c.jaw_width=-.25f;c.hair_length=.35f;c.hair_volume=.25f;c.fringe=.2f;c.hair_recolor=std::array<float,3>{.8f,.2f,.1f};
  auto model=create_humanoid(c);Scene scene;auto group=append_humanoid(scene,c,"Styled creator",{},64);auto file=root/(std::string(baseline)+"-styled.vrm");verify(scene,model,file);
  auto native=root/(std::string(baseline)+".dsculpt");scene.save(native);const auto hash=scene_hash(scene);{auto opened=Scene::load(native);check(scene_hash(opened)==hash,"Shaped creator and generated genesis survive exact native Save/reopen");verify(opened,model,root/(std::string(baseline)+"-reopened.vrm"));}
  {auto edited=scene;for(const auto &o:scene.objects()){auto d=std::make_shared<DeformationState>(*o.deformation);d->skeleton->bones[d->humanoid->joints.at("head")].rest.translation.x+=.001f;edited.set_deformation(o.id,*d);}refuses(edited,root/"invalid-rest.vrm","Arbitrary rest changes cannot borrow creator metadata authority");}
  {auto edited=scene;auto recipe=std::make_shared<HumanoidControls>(c);recipe->build=-.8f;edited.group(group).humanoid=recipe;refuses(edited,root/"stale-recipe.vrm","Stale creator recipe cannot certify a different rest/geometry");}
  {auto edited=scene;for(const auto &o:scene.objects())if(!o.appearance){auto b=std::make_shared<SourceMaterialBindings>(*o.material_bindings);auto origin=std::make_shared<SourceUvOrigin>(*b->uv_origin);origin->creator_kind="vroid-shape";b->uv_origin=origin;edited.object(o.id).material_bindings=b;}refuses(edited,root/"fake-source.vrm","Generated groom cannot masquerade as imported source identity");}
  {const auto &o=scene.active();RigEdit edit;edit.joint=JointAdjustment{o.deformation->humanoid->joints.at("head"),{0,11,0},{.001f,0,0}};edit_rig(scene,o.id,edit);}
  for(const auto &o:scene.objects())if(o.deformation->morphs){auto d=*o.deformation;d.morph_weights.assign(d.morphs->targets.size(),0);d.morph_weights[0]=.3f;scene.set_deformation(o.id,std::move(d));}
  scene.group(group).transform.position={.2f,.1f,-.3f};scene.group(group).transform.scale=1.2f;verify(scene,model,root/(std::string(baseline)+"-posed.vrm"));
 }
 std::cout<<"CREATOR VRM CHECKS "<<checks<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
