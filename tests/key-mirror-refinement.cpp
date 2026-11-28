// SPDX-License-Identifier: GPL-3.0-or-later
#include "mirror-host-allocation-fault.hpp"
namespace allocation=mirror_failure;
#include "key-mirror-refinement.hpp"
#include "cgltf.h"
#include "dome/bake-recipe.hpp"
#include "dome/document.hpp"
#include "dome/material-bindings.hpp"
#include "dome/rig-edit.hpp"
#include "dome/rig-rest.hpp"
#include "dome/skin-authoring.hpp"
#include "dome/tangents.hpp"
#include <nlohmann/json.hpp>
#include <png.h>
#include <cmath>
#include <iostream>
#include <set>
using namespace dome;
using namespace dome::detail;
namespace {
size_t checks = 0;
void check(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
  ++checks;
  std::cout << "PASS " << m << std::endl;
}
bool same_positions(const std::vector<Vec3> &a, const std::vector<Vec3> &b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (a[i].x != b[i].x || a[i].y != b[i].y || a[i].z != b[i].z)
      return false;
  return true;
}
bool refuses(auto f, std::string_view diagnostic) {
  try {
    f();
    return false;
  } catch (const std::exception &e) {
    std::cout << "REFUSAL " << e.what() << std::endl;
    return std::string_view(e.what()).find(diagnostic) != std::string_view::npos;
  }
}
std::shared_ptr<const Mesh> square() {
  Mesh m;
  m.positions = {{-.5f, -.5f, 0}, {.5f, -.5f, 0}, {.5f, .5f, 0}, {-.5f, .5f, 0}};
  m.normals.assign(4, {0, 0, 1});
  m.uv = {{{0, 0}}, {{1, 0}}, {{1, 1}}, {{0, 1}}};
  m.indices = {0, 1, 2, 0, 2, 3};
  return std::make_shared<const Mesh>(std::move(m));
}
Vec3 reflect(Vec3 p) { return {-p.x, p.y, p.z}; }
Vec3 at(const Mesh &m, const MirrorSurfacePoint &s) {
  Vec3 p;
  for (size_t k = 0; k < 3; ++k)
    p += m.positions[m.indices[s.triangle * 3 + k]] * float(s.barycentric[k]);
  return p;
}
bool same(const KeyMirrorRefinement &a, const KeyMirrorRefinement &b) {
  if (a.source != b.source || a.opposite != b.opposite || a.corners.size() != b.corners.size() ||
      a.maximum_rest_error != b.maximum_rest_error || a.work_units != b.work_units ||
      a.plane_offset != b.plane_offset)
    return false;
  for (size_t c = 0; c < a.corners.size(); ++c) {
    const auto &x = a.corners[c];
    const auto &y = b.corners[c];
    if (x.position.x != y.position.x || x.position.y != y.position.y ||
        x.position.z != y.position.z || x.current.triangle != y.current.triangle ||
        x.current.barycentric != y.current.barycentric ||
        x.reflected.triangle != y.reflected.triangle ||
        x.reflected.barycentric != y.reflected.barycentric)
      return false;
  }
  return true;
}
// Independent physical piecewise-linear field, not a runtime correspondence.
double field(double x, double y) {
  const double u = x + .5, v = y + .5;
  return v <= u ? .04 * (u - v) - .06 * v : -.06 * u + .09 * (v - u);
}
double slide(double x,double y){
  const double u=x+.5,v=y+.5;
  return v<=u?.125*(u-v):-.0625*(v-u);
}
double sampled(const MirrorSurfacePoint &p) {
  constexpr double z[] = {0, .04, -.06, 0, -.06, .09};
  double sum = 0;
  for (size_t k = 0; k < 3; ++k)
    sum += z[p.triangle * 3 + k] * p.barycentric[k];
  return sum;
}

Scene native_fixture(bool split_edge=false,bool planar_key=false){
  Scene scene;
  const auto id=scene.active().id;
  auto mesh=*square();
  if(split_edge){
    const auto original=mesh;
    mesh={};
    for(auto v:original.indices){
      mesh.indices.push_back(uint32_t(mesh.positions.size()));
      mesh.positions.push_back(original.positions[v]);
      mesh.normals.push_back(original.normals[v]);
      mesh.uv.push_back(original.uv[v]);
    }
  }
  mesh.uv_pins.resize(mesh.positions.size());
  mesh.uv_pins[0]=1;
  mesh.uv_seams={1,0,0,0,0,0};
  auto &object=scene.active();
  object.mesh_active=true;
  object.painted=std::make_shared<PaintedMesh>(mesh,64);
  Brush brush;brush.from=brush.to={-.12f,-.03f,0};brush.normal={0,0,1};brush.radius=.24f;
  PaintSettings ink;ink.color={.12f,.6f,.35f,.8f};
  object.painted->begin_stroke();object.painted->stamp(brush,ink);object.painted->end_stroke();
  object.painted->add_layer("Hidden original");
  brush.from=brush.to={.22f,.07f,0};ink.color={.8f,.15f,.1f,1};
  object.painted->begin_stroke();object.painted->stamp(brush,ink);object.painted->end_stroke();
  object.painted->layer_properties("Hidden original",false,.4f);
  ShapeKeyEdit edit;edit.name=planar_key?"Asymmetric slide":"Asymmetric fold";
  author_shapekey(scene,id,edit);
  edit.action=ShapeKeyAction::Replace;edit.positions=mesh.positions;
  for(auto &p:edit.positions){
    if(planar_key)p.x+=float(slide(p.x,p.y));
    else p.z=float(field(p.x,p.y));
  }
  author_shapekey(scene,id,edit);
  auto deformation=*scene.active().deformation;deformation.morph_weights={.25f};
  scene.set_deformation(id,std::move(deformation));
  return scene;
}

Scene baked_fixture(bool masked=false){
  auto scene=native_fixture(false,true);
  const auto target=scene.active().id;
  ObjectBakeSettings settings;
  settings.front_mm=320;settings.back_mm=160;settings.ao_distance_mm=85;
  settings.ao_samples=4;settings.padding=2;
  if(masked){
    const auto &paint=scene.active().painted;
    paint->add_layer("Bake protection");
    settings.mask_layer=paint->active_layer();
    PaintSettings protection;protection.channel=PaintChannel::Mask;protection.mask=0;
    Brush brush;brush.from=brush.to={.18f,-.17f,0};brush.normal={0,0,1};brush.radius=.28f;
    paint->begin_stroke();paint->stamp(brush,protection);paint->end_stroke();
  }
  const auto donor=scene.add("Sloped material donor",Volume{});
  auto surface=*square();
  for(auto &p:surface.positions){p.x*=1.3f;p.y*=1.3f;p.z=.12f+.21f*p.x-.12f*p.y;}
  surface.normals.assign(surface.positions.size(),normalized(Vec3{-.21f,.12f,1}));
  auto &high=scene.object(donor);
  high.mesh_active=true;high.painted=std::make_shared<PaintedMesh>(std::move(surface),64);
  high.material.color={.27f,.43f,.66f,1};high.material.metallic=.31f;high.material.roughness=.52f;
  high.material.emissive=masked?std::array<float,3>{2.5f,.125f,.75f}:std::array<float,3>{.3f,.1f,.2f};
  scene.select(target);
  RigEdit preview;preview.morph=MorphWeightEdit{0,0};
  edit_rig(scene,target,preview);
  const auto report=bake_object(scene,donor,settings);
  preview.morph->weight=.25f;
  edit_rig(scene,target,preview);
  scene.object(donor).visible=false;
  const auto bake=scene.active().painted->baked();
  if(!report.canonical_captured)throw std::runtime_error("Fixture Bake could not capture the original tangent frame");
  if(!bake||bake->maps.normal.rgba.empty()||!report.projected||report.missed||
     (masked&&(!report.protected_texels||!bake->emission_field)))
    throw std::runtime_error("Fixture needs an actual canonical normal Bake and its material coverage");
  return scene;
}

const nlohmann::json &paint_fields(const nlohmann::json &encoded){
  return encoded.contains("payload")?encoded.at("payload"):encoded;
}

void check_bake(const Scene &before,const Scene &after){
  const auto &a=before.active(), &b=after.active();
  const auto prior=a.painted->encode(),current=b.painted->encode();
  const auto in=a.painted->baked(),out=b.painted->baked();
  check(in&&out&&paint_fields(prior).at("baked")==paint_fields(current).at("baked")&&
        out->coverage==in->coverage&&bool(out->emission_field)==bool(in->emission_field),
        "Canonical normals, material channels and native Bake coverage survive exactly");
  check(a.bake_recipe&&b.bake_recipe&&a.bake_recipe->encode()==b.bake_recipe->encode(),
        "The named donor, mask and saved Bake settings stay attached to the refined mesh");
  const auto original=a.painted->preview(a.material),refined=b.painted->preview(b.material);
  check(original->color.rgba==refined->color.rgba&&original->metal_rough.rgba==refined->metal_rough.rgba&&
        original->normal.rgba==refined->normal.rgba&&original->emission.rgba==refined->emission.rgba&&
        original->baked_normal.rgba==refined->baked_normal.rgba&&
        original->baked_coverage.rgba==refined->baked_coverage.rgba,
        "The composed Bake and editable paint keep identical visible pixels across material roles");
  if(out->emission_field){
    bool equal=true;
    for(size_t i=0;i<out->coverage.size();++i){
      const auto a=in->emission_field->texel(i),b=out->emission_field->texel(i);
      equal&=a.legacy==b.legacy&&a.absolute==b.absolute&&a.coverage==b.coverage;
    }
    check(equal,"Masked high-dynamic-range Bake retains independent FLOAT radiance and coverage");
  }
  const auto canonical=b.material_bindings->normal_frames->canonical;
  check(canonical&&canonical->transport==NormalFrameTransport::CanonicalMikk&&
        canonical->tangents.size()==b.painted->mesh().indices.size()&&
        std::ranges::all_of(canonical->tangents,[](const auto &t){return t==Tangent{1,0,0,1};}),
        "Refined corners retain the captured bake tangent directions and handedness");
}

Scene rigged_fixture(bool split_edge=false){
  auto scene=native_fixture(split_edge);
  const auto id=scene.active().id;
  auto rig=*scene.active().deformation;
  rig.skeleton=Skeleton{{{"hips",-1,{},.5f},{"spine",0,{},.4f},{"detail",1,{},.2f}}};
  rig.skeleton->bones[1].rest.translation={.16f,.28f,0};
  rig.skeleton->bones[2].rest.translation={.12f,.21f,0};
  rig.humanoid=HumanoidMapping{{{"hips",0},{"spine",1}}};
  const auto &roles=human_roles();
  for(const auto &role:roles){
    if(rig.humanoid->joints.contains(role.name))continue;
    const auto parent=rig.humanoid->joints.at(role.parent);
    rig.humanoid->joints.emplace(role.name,uint32_t(rig.skeleton->bones.size()));
    rig.skeleton->bones.push_back({role.name,int(parent),{},.08f,false});
    rig.skeleton->bones.back().rest.translation={0,.08f,0};
  }
  rig.skeleton->inverse_bind_override=rig.skeleton->inverse_bind();
  rig.landmarks=HumanLandmarks{{{"hips",{0,0,0}},{"spine",{.16f,.28f,0}}}};
  rig.expressions=AvatarExpressions{{{"aa",{{"Asymmetric fold",.8f}}}}};
  scene.set_deformation(id,std::move(rig));
  SkinAuthoringOptions bind;bind.reference=id;bind.seed_distance=2;
  if(!author_skin(scene,id,bind)||!scene.active().rig_source)
    throw std::runtime_error("Fixture needs a native binding transaction and its first source");
  rig=*scene.active().deformation;
  // One tiny authored influence and one unsorted row detect normalization or
  // reordering of exact old corners. The thirds vary across both diagonals.
  rig.skin->vertices={{{1,.125f},{0,.875f}},{{0,.25f},{1,.75f}},
                      {{0,.625f},{1,.375f}},{{0,.5f},{1,.49999976f},{2,0x1p-22f}}};
  if(split_edge)rig.skin->vertices={{{0,1}},{{0,1}},{{0,1}},{{1,1}},{{1,1}},{{1,1}}};
  for(const auto &bone:rig.skeleton->bones)rig.pose.push_back(bone.rest);
  rig.pose[0].translation={.02f,.018f,.038f};
  rig.pose[1].full_local_matrix=RigMatrix{1,.02f,.02f,0,.1f,.98f,0,0,.015f,0,1,0,.16f,.27f,.025f,1};
  scene.set_deformation(id,std::move(rig));
  return scene;
}

std::array<double,3> expected_weights(Vec3 p){
  const double u=p.x+.5, v=p.y+.5;
  if(v<=u)return {.875*(1-u)+.25*(u-v)+.625*v,.125*(1-u)+.75*(u-v)+.375*v,0.};
  return {.875*(1-v)+.625*u+.5*(v-u),.125*(1-v)+.375*u+.49999976*(v-u),0x1p-22*(v-u)};
}

void check_skin(const Scene &before,const Scene &after){
  const auto &a=before.active(),&b=after.active();
  const auto &original=a.painted->mesh(),&mesh=b.painted->mesh();
  auto prior=a.deformation->encode(original),now=b.deformation->encode(mesh);
  for(auto key:{"source","skin","morphs","morph_weights"}){prior.erase(key);now.erase(key);}
  check(prior==now&&bool(a.rig_source)&&bool(b.rig_source),
        "Rig hierarchy inverse binds affine pose humanoid landmarks and expressions are preserved");
  size_t at_original=0,new_tiny=0;
  for(size_t v=0;v<mesh.positions.size();++v){
    const auto p=mesh.positions[v];
    const auto &row=b.deformation->skin->vertices[v];
    const auto weights=expected_weights(p);
    std::array<float,3> actual{};
    for(const auto &entry:row)actual.at(entry.joint)=entry.weight;
    if(std::abs(actual[0]-weights[0])>8e-8||std::abs(actual[1]-weights[1])>8e-8||
       std::abs(actual[2]-weights[2])>1e-13)
      throw std::runtime_error("New skin row disagrees with the original triangle weight field");
    for(size_t old=0;old<original.positions.size();++old)if(length(p-original.positions[old])<1e-8f){
      if(a.deformation->skin->vertices[old]!=row)
        throw std::runtime_error("An original corner skin row changed during refinement");
      ++at_original;
    }
    if(std::abs(p.x)<.49f&&std::abs(p.y)<.49f&&actual[2]>0)++new_tiny;
  }
  check(at_original>=4&&new_tiny>0,"Original corner rows stay exact and new rows retain the unpruned joint field");
  auto restore_after=after,restore_before=before;
  restore_rig_source(restore_after,b.id,false);restore_rig_source(restore_before,a.id,false);
  check(restore_after.encode()==restore_before.encode(),"Original native rig Restore survives the topology transaction");
}

Vec3 gltf_point(const float *m,Vec3 p){
  return {m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],
          m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14]};
}

void check_skin_glb(const std::filesystem::path &file,const Scene &scene){
  cgltf_options options{};
  cgltf_data *raw=nullptr;
  check(cgltf_parse_file(&options,file.c_str(),&raw)==cgltf_result_success,"Independent cgltf reader opens the refined skinned GLB");
  const std::unique_ptr<cgltf_data,decltype(&cgltf_free)> model(raw,cgltf_free);
  check(cgltf_load_buffers(&options,raw,file.c_str())==cgltf_result_success&&cgltf_validate(raw)==cgltf_result_success,
        "Independent cgltf validation accepts the refined mesh skin and morph accessors");
  const auto &object=scene.active();
  const auto &rest=object.painted->mesh();
  const auto posed=object.deformation->preview(rest);
  size_t vertices=0;
  float maximum=0;
  const cgltf_skin *skin=nullptr;
  nlohmann::json humanoid;
  for(size_t n=0;n<raw->nodes_count;++n){
    const auto &node=raw->nodes[n];
    if(node.extras.data){
      const auto extras=nlohmann::json::parse(node.extras.data);
      if(extras.contains("domeSculptHumanoid"))humanoid=extras["domeSculptHumanoid"]["humanBones"];
    }
    if(!node.mesh)continue;
    if(!node.skin||node.skin->joints_count!=object.deformation->skeleton->bones.size()||
       !node.skin->inverse_bind_matrices||node.mesh->primitives_count!=1)
      throw std::runtime_error("Exported refined mesh lost its native rig");
    skin=node.skin;
    const auto &extras=node.mesh->extras;
    const auto text=std::string_view(raw->json,raw->json_size).substr(extras.start_offset,extras.end_offset-extras.start_offset);
    if(nlohmann::json::parse(text).at("domeSculptExpressions")!=object.deformation->expressions->encode())
      throw std::runtime_error("Exported refined mesh changed its expression binding");
    const auto &primitive=node.mesh->primitives[0];
    const auto *positions=cgltf_find_accessor(&primitive,cgltf_attribute_type_position,0);
    const auto *joints=cgltf_find_accessor(&primitive,cgltf_attribute_type_joints,0);
    const auto *weights=cgltf_find_accessor(&primitive,cgltf_attribute_type_weights,0);
    if(!positions||!joints||!weights||primitive.targets_count!=2||!primitive.indices||
       primitive.indices->count!=rest.indices.size())
      throw std::runtime_error("Exported refined mesh lost its topology skin or morph attributes");
    for(size_t v=0;v<positions->count;++v){
      float p[3],w[4];unsigned j[4];
      if(!cgltf_accessor_read_float(positions,v,p,3)||!cgltf_accessor_read_uint(joints,v,j,4)||
         !cgltf_accessor_read_float(weights,v,w,4))throw std::runtime_error("Cannot read refined vertex accessors");
      const Vec3 rest_point{p[0],p[1],p[2]};
      Vec3 local=rest_point,actual{};
      const auto expected_skin=expected_weights(rest_point);
      std::array<float,3> sampled_skin{};
      for(size_t t=0;t<primitive.targets_count;++t){
        const auto &target=primitive.targets[t];
        const auto end=target.attributes+target.attributes_count;
        const auto attribute=std::find_if(target.attributes,end,[](const cgltf_attribute &a){
          return a.type==cgltf_attribute_type_position;
        });
        if(attribute==end||!cgltf_accessor_read_float(attribute->data,v,p,3))
          throw std::runtime_error("Cannot read refined morph accessor");
        const auto expected_delta=field(t==0?rest_point.x:-rest_point.x,rest_point.y);
        if(std::abs(p[2]-expected_delta)>1e-7||p[0]!=0||p[1]!=0)
          throw std::runtime_error("Exported refined morph differs from its analytic field");
        const float value=node.weights_count>t?node.weights[t]:node.mesh->weights_count>t?node.mesh->weights[t]:0;
        local+=Vec3{p[0],p[1],p[2]}*value;
      }
      for(size_t i=0;i<4;++i)if(w[i]>0){
        float inverse[16],world[16];
        if(j[i]>=node.skin->joints_count||!cgltf_accessor_read_float(node.skin->inverse_bind_matrices,j[i],inverse,16))
          throw std::runtime_error("Cannot read refined inverse bind");
        sampled_skin.at(j[i])=w[i];
        cgltf_node_transform_world(node.skin->joints[j[i]],world);
        actual+=gltf_point(world,gltf_point(inverse,local))*w[i];
      }
      if(std::abs(sampled_skin[0]-expected_skin[0])>8e-8||std::abs(sampled_skin[1]-expected_skin[1])>8e-8||
         std::abs(sampled_skin[2]-expected_skin[2])>1e-13)
        throw std::runtime_error("Exported refined skin differs from the analytic weight field");
      auto matching=std::ranges::find_if(rest.positions,[&](Vec3 q){return length(q-rest_point)<1e-6f;});
      if(matching==rest.positions.end())throw std::runtime_error("Exported vertex has no matching refined rest point");
      const auto &posed_point=posed.positions[matching-rest.positions.begin()];
      const auto expected=scene.world_matrix(object.id).point({posed_point.x,posed_point.y,posed_point.z});
      maximum=std::max(maximum,length(actual-Vec3{float(expected[0]),float(expected[1]),float(expected[2])}));
      ++vertices;
    }
  }
  bool same_roles=skin&&humanoid.size()==object.deformation->humanoid->joints.size();
  for(const auto &[role,joint]:object.deformation->humanoid->joints)
    same_roles=same_roles&&humanoid.at(role).at("node")==skin->joints[joint]-raw->nodes;
  check(same_roles,"Independent GLB verifies humanoid joint identities and expression bindings");
  std::cout<<"GLB_SKINNED_REFINEMENT vertices="<<vertices<<" max_pose_error="<<maximum<<std::endl;
  check(vertices>4&&maximum<2e-6f,"Independent GLB skin and morph evaluation matches the refined native joint pose");
}

void check_smoothing(const Scene &scene){
  const auto &object=scene.active();
  const auto &rig=*object.deformation;
  const auto &mesh=object.painted->mesh();
  const auto posed=rig.preview(mesh);
  const auto at=std::ranges::find_if(mesh.positions,[](Vec3 p){return length(p)<1e-8f;});
  if(at==mesh.positions.end())throw std::runtime_error("Missing shared refined center");
  const auto center=uint32_t(at-mesh.positions.begin());
  std::set<uint32_t> ring;
  for(size_t c=0;c<mesh.indices.size();c+=3){
    const auto triangle=std::span(mesh.indices).subspan(c,3);
    if(std::ranges::find(triangle,center)!=triangle.end())ring.insert(triangle.begin(),triangle.end());
  }
  ring.erase(center);
  double average=0;
  for(const auto v:ring)average+=expected_weights(mesh.positions[v])[1];
  const WeightPaintSurface surface{object.id,&mesh,&*rig.skin,posed.positions};
  WeightBrush brush;brush.from=brush.to=posed.positions[center];brush.radius=.001f;
  brush.mode=WeightBrushMode::Smooth;brush.strength=1;brush.joint=1;
  const auto result=edit_weights(std::span(&surface,1),rig.skeleton->bones.size(),brush);
  const auto &row=result.surfaces.at(0).weights.vertices[center];
  const auto weight=std::ranges::find(row,1u,&Influence::joint);
  check(ring.size()>=4&&result.changed==1&&weight!=row.end()&&std::abs(weight->weight-average/ring.size())<1e-7,
        "A posed Smooth weight stroke uses neighbors across the refined triangle boundaries");
}

void check_authored(const Scene &before,const Scene &after){
  const auto &a=before.active(),&b=after.active();
  const auto &m=b.painted->mesh();
  const auto &keys=b.deformation->morphs->targets;
  check(m.indices.size()>a.painted->mesh().indices.size()&&keys.size()==2&&
        b.deformation->morph_weights==std::vector<float>({.25f,0}),
        "Native Mirror explicitly refines the topology and carries both key weights");
  const bool planar=keys[0].name=="Asymmetric slide";
  bool interpolated=false;
  for(size_t v=0;v<m.positions.size();++v){
    const auto p=m.positions[v];
    const auto evaluate=planar?slide:field;
    const auto original=float(evaluate(p.x,p.y)),mirrored=float(evaluate(-p.x,p.y));
    const auto expected=planar?Vec3{original,0,0}:Vec3{0,0,original};
    const auto reflection=planar?Vec3{-mirrored,0,0}:Vec3{0,0,mirrored};
    if(length(keys[0].delta[v]-expected)>1e-7||length(keys[1].delta[v]-reflection)>1e-7)
      throw std::runtime_error("Authored native key disagrees with the independent continuous-field oracle");
    interpolated|=std::abs(p.x)<.49f&&std::abs(p.y)<.49f;
  }
  check(interpolated,"Prior and mirrored keys preserve their physical fields at new interior vertices");
  const auto middle=[](Vec3 p){return std::abs(p.x)<1e-8f&&std::abs(p.y)<1e-8f;};
  const auto vertex=std::ranges::find_if(m.positions,middle);
  check(vertex!=m.positions.end()&&std::ranges::count_if(m.positions,middle)==1&&
        std::ranges::count(m.indices,uint32_t(vertex-m.positions.begin()))>=4,
        "A continuous original and reflected field shares the crossing vertex between triangles");
  check(paint_fields(a.painted->encode()).at("layers")==paint_fields(b.painted->encode()).at("layers")&&
        a.painted->preview(a.material)->color.rgba==b.painted->preview(b.material)->color.rgba,
        "Every hidden and visible paint layer and the composited UV pixels survive refinement");
  check(std::any_of(m.uv_pins.begin(),m.uv_pins.end(),[](auto pin){return pin;})&&
        std::any_of(m.uv_seams.begin(),m.uv_seams.end(),[](auto seam){return seam;}),
        "Explicit original UV pins and seams carry into the refined surface");
  auto restored=after;
  ShapeKeyEdit restore;restore.action=ShapeKeyAction::Restore;
  author_shapekey(restored,b.id,restore);
  check(restored.encode()==before.encode(),"Restore recovers the exact pre-refinement mesh, frame data and layers");
  if(a.deformation->skin)check_skin(before,after);
  if(a.painted->baked())check_bake(before,after);
}

void check_baked_glb(const std::filesystem::path &file,const SculptObject &object){
  cgltf_options options{};
  cgltf_data *raw=nullptr;
  check(cgltf_parse_file(&options,file.c_str(),&raw)==cgltf_result_success,"Independent reader opens the GLB of the baked refined mesh");
  const std::unique_ptr<cgltf_data,decltype(&cgltf_free)> model(raw,cgltf_free);
  if(cgltf_load_buffers(&options,raw,file.c_str())!=cgltf_result_success||cgltf_validate(raw)!=cgltf_result_success||
     raw->meshes_count!=1||raw->meshes[0].primitives_count!=1)
    throw std::runtime_error("Invalid refined baked GLB mesh or accessors");
  const auto &primitive=raw->meshes[0].primitives[0];
  const auto *material=primitive.material;
  const auto *tangent=cgltf_find_accessor(&primitive,cgltf_attribute_type_tangent,0);
  check(primitive.targets_count==2&&tangent&&material&&material->normal_texture.texture&&
        material->normal_texture.texture->image&&
        material->occlusion_texture.texture==material->pbr_metallic_roughness.metallic_roughness_texture.texture,
        "GLB retains both key targets, explicit baked tangents and the shared AO and material map");
  const auto *view=material->normal_texture.texture->image->buffer_view;
  if(!view||!view->buffer||!view->buffer->data)throw std::runtime_error("Baked GLB is missing its embedded normal map");
  png_image image{};image.version=PNG_IMAGE_VERSION;
  if(!png_image_begin_read_from_memory(&image,static_cast<const uint8_t*>(view->buffer->data)+view->offset,view->size))
    throw std::runtime_error("Independent PNG reader cannot read refined normal texture");
  image.format=PNG_FORMAT_RGBA;
  std::vector<uint8_t> bytes(PNG_IMAGE_SIZE(image));
  const bool read=png_image_finish_read(&image,nullptr,bytes.data(),0,nullptr);
  png_image_free(&image);
  check(read&&bytes==object.painted->composite(object.material)->baked_normal.rgba,
        "Independent PNG reader recovers the exact retained GLB normal texture");
}

void baked_workflow(const std::filesystem::path &out){
  std::filesystem::create_directories(out);
  ShapeKeyEdit edit;edit.action=ShapeKeyAction::Mirror;edit.mirror_frames=ShapeKeyMirrorFrames::RefinedSurface;
  edit.name="Reflected slide";
  for(const bool masked:{false,true}){
    const auto source=baked_fixture(masked);
    const auto id=source.active().id;
    auto direct=source;
    author_shapekey(direct,id,edit);
    check(source.active().painted->baked()==direct.active().painted->baked()&&
          source.active().bake_recipe==direct.active().bake_recipe,
          "Mirror shares the immutable native Bake map and recipe owners");
    check_authored(source,direct);
    Document document;document.replace(source);
    document.author_shape(edit);
    const auto file=out/(masked?"refined-masked.dsculpt":"refined-baked.dsculpt");
    document.save_async(file).get();
    check(document.wait_asset()&&document.wait_mesh()&&document.frame().state->error.empty()&&
          Scene::load(file).encode()==direct.encode()&&document.undo()&&document.copy_scene().encode()==source.encode()&&
          document.redo()&&document.copy_scene().encode()==direct.encode(),
          "Ordered Save and one Undo or Redo preserve the complete Bake and its refined topology");
    auto reopened=Scene::load(file);
    document.replace(reopened);
    const auto recipe=reopened.active().bake_recipe;
    check(document.wait_mesh()&&document.frame().state->bake_recipe&&
          document.frame().state->bake_recipe->encode()==recipe->encode()&&
          document.frame().state->bake_settings.mask_layer==recipe->settings.mask_layer&&
          document.frame().surface->ranges.front().canonical_frame,
          "Cold Open restores the saved Bake settings and publishes the canonical preview frame");
    if(!masked){
      const auto glb=out/"refined-baked.glb";
      document.export_asset(glb).get();
      check_baked_glb(glb,reopened.active());
    }
    RigEdit rest;rest.morph=MorphWeightEdit{0,0};
    edit_rig(reopened,id,rest);
    reopened.object(recipe->source.object).material.color={.6f,.15f,.26f,1};
    document.replace(reopened);
    document.bake_from_object(recipe->source.object,recipe->settings);
    const bool completed=document.wait_asset()&&document.wait_mesh()&&document.frame().state->error.empty();
    const auto rebaked=document.copy_scene();
    const auto previous=reopened.active().painted->baked(),updated=rebaked.active().painted->baked();
    check(completed&&previous&&updated&&rebaked.active().bake_recipe&&
          rebaked.active().painted->mesh().indices==reopened.active().painted->mesh().indices&&
          rebaked.active().bake_recipe->encode()==recipe->encode()&&
          previous->maps.color.rgba!=updated->maps.color.rgba&&previous->coverage==updated->coverage,
          "The saved projection recipe can bake again on the refined topology");
    source.save(out/(masked?"source-masked.dsculpt":"source-baked.dsculpt"));
  }
  auto legacy=baked_fixture();
  auto binding=std::make_shared<SourceMaterialBindings>(*legacy.active().material_bindings);
  auto frames=std::make_shared<NormalFrameSet>(*binding->normal_frames);
  frames->canonical.reset();binding->normal_frames=std::move(frames);
  legacy.active().material_bindings=std::move(binding);
  const auto before=legacy.encode();
  check(refuses([&]{author_shapekey(legacy,legacy.active().id,edit);},"retained constant tangent frame")&&
        legacy.encode()==before,"Legacy baked normals without captured frames remain intact on refusal");
}

void workflow(const std::filesystem::path &out,bool rigged){
  std::filesystem::create_directories(out);
  const auto source=rigged?rigged_fixture():native_fixture();
  auto direct=source;
  ShapeKeyEdit edit;edit.action=ShapeKeyAction::Mirror;edit.mirror_frames=ShapeKeyMirrorFrames::RefinedSurface;
  edit.name="Reflected fold";
  const auto id=source.active().id;
  if(rigged){
    auto overfull=source;
    auto rig=*overfull.active().deformation;
    rig.skeleton=Skeleton{};rig.skeleton->bones.push_back({"root",-1,{},1});
    for(size_t joint=1;joint<64;++joint)rig.skeleton->bones.push_back({"detail "+std::to_string(joint),0,{},1});
    rig.pose.clear();rig.humanoid.reset();
    for(size_t v=0;v<4;++v){
      auto &row=rig.skin->vertices[v];row.clear();
      for(uint32_t j=0;j<32;++j)row.push_back({j+uint32_t(v%2)*32,1.f/32});
    }
    overfull.set_deformation(id,std::move(rig));
    const auto old=overfull.encode();
    check(refuses([&]{author_shapekey(overfull,id,edit);},"more than 32 influences")&&overfull.encode()==old,
          "Influence overflow refuses without dropping weights or publishing partial topology");
    auto split=rigged_fixture(true);
    author_shapekey(split,id,edit);
    const auto &mesh=split.active().painted->mesh();
    const auto &weights=split.active().deformation->skin->vertices;
    std::array<bool,2> seam{};
    for(size_t v=0;v<mesh.positions.size();++v){
      const auto p=mesh.positions[v];
      if(std::abs(p.x)>1e-8f||std::abs(p.y)>1e-8f)continue;
      if(weights[v].size()!=1||weights[v][0].weight!=1)
        throw std::runtime_error("An independently weighted seam blended its opposite side");
      seam.at(weights[v][0].joint)=true;
    }
    check(seam[0]&&seam[1],"Coincident but independently weighted original seam sides remain distinct");
  }
  size_t polls=0;
  check(refuses([&]{author_shapekey(direct,id,edit,[&]{return ++polls>15;});},"cancelled")&&
        direct.encode()==source.encode(),"Cancelled refinement publishes no part of the detached topology");
  author_shapekey(direct,id,edit);
  check_authored(source,direct);
  Document document;document.replace(source);
  document.author_shape(edit);
  const auto file=out/"refined-mirror.dsculpt";
  document.save_async(file).get();
  check(document.wait_asset()&&document.wait_mesh()&&document.frame().state->error.empty(),
        "The ordered native worker publishes its refined mesh and Save");
  check(Scene::load(file).encode()==direct.encode()&&document.undo()&&document.copy_scene().encode()==source.encode()&&
        document.redo()&&document.copy_scene().encode()==direct.encode(),
        "Refined Mirror saves and reopens exactly and is one complete Undo/Redo step");
  auto reopened=Scene::load(file);
  ShapeKeyEdit replace;replace.action=ShapeKeyAction::Replace;replace.target=1;
  replace.positions=reopened.active().painted->mesh().positions;
  const auto &mirrored=reopened.active().deformation->morphs->targets[1].delta;
  for(size_t v=0;v<replace.positions.size();++v)replace.positions[v]+=mirrored[v];
  replace.positions[0].z+=.005f;
  check(author_shapekey(reopened,id,replace).changed&&Scene::decode(reopened.encode()).encode()==reopened.encode(),
        "The saved refined key remains editable and its later endpoint reopens exactly");
  if(rigged){
    const auto glb=out/"refined-skin.glb";
    auto export_scene=Scene::load(file);
    check_smoothing(export_scene);
    RigEdit activate;activate.morph=MorphWeightEdit{1,.45f};
    edit_rig(export_scene,id,activate);
    document.replace(export_scene);
    document.export_asset(glb).get();
    check_skin_glb(glb,export_scene);
  }
  source.save(out/"source.dsculpt");
}
} // namespace
int main(int argc,char **argv) try {
  const auto source = square();
  if(argc==3&&std::string_view(argv[1])=="--ui-fixture"){native_fixture().save(argv[2]);return 0;}
  if(argc==3&&std::string_view(argv[1])=="--ui-rig-fixture"){rigged_fixture().save(argv[2]);return 0;}
  if(argc==3&&std::string_view(argv[1])=="--ui-baked-fixture"){baked_fixture().save(argv[2]);return 0;}
  if(argc==3&&std::string_view(argv[1])=="--baked-authoring"){baked_workflow(argv[2]);return 0;}
  if(argc==3&&std::string_view(argv[1])=="--xr-baked-fixture"){
    auto scene=baked_fixture(true);scene.active().transform.position={0,1.3f,-1.1f};scene.save(argv[2]);return 0;
  }
  if(argc==3&&(std::string_view(argv[1])=="--xr-fixture"||std::string_view(argv[1])=="--xr-rig-fixture")){
    auto scene=std::string_view(argv[1])=="--xr-rig-fixture"?rigged_fixture():native_fixture();
    scene.active().transform.position={0,1.3f,-1.1f};scene.save(argv[2]);return 0;
  }
  if(argc==3&&(std::string_view(argv[1])=="--authoring"||std::string_view(argv[1])=="--rigged-authoring")){
    workflow(argv[2],std::string_view(argv[1])=="--rigged-authoring");return 0;
  }
  if(argc==4&&std::string_view(argv[1])=="--verify-ui"){
    check_authored(Scene::load(argv[2]),Scene::load(argv[3]));return 0;
  }
  if(argc==2&&std::string_view(argv[1])=="--require-device"){
    try{refine_reflected_key(source,1e-6f);}
    catch(const std::exception &error){std::cout<<error.what()<<'\n';return 0;}
    throw std::runtime_error("Mirror refinement accepted an unavailable GPU");
  }
  if(argc!=1)return 2;
  const auto positions = source->positions;
  const auto indices = source->indices;
  check(refuses([&] { reflected_key_correspondence(*source, 1e-6f); }, "triangle refinement"),
        "Genuine opposite diagonals cannot use the historical corner bijection");
  const auto result = refine_reflected_key(source, 1e-6f);
  check(same(result, refine_reflected_key(source, KeyMirrorPlane{0}, 1e-6f)),
        "Historical zero-plane overload retains exact default geometry/provenance/work semantics");
  auto shifted = std::make_shared<Mesh>(*source);
  for (auto &p : shifted->positions)
    p.x += .125f;
  const auto shifted_positions = shifted->positions;
  const auto shifted_result = refine_reflected_key(shifted, KeyMirrorPlane{.125}, 1e-6f);
  bool plane_exact = shifted_result.plane_offset == .125 && shifted_result.source == shifted &&
                     !shifted_result.source.owner_before(shifted) &&
                     !shifted.owner_before(shifted_result.source) &&
                     shifted_result.corners.size() == result.corners.size() &&
                     shifted_result.opposite == result.opposite;
  for (size_t c = 0; c < result.corners.size() && plane_exact; ++c) {
    const auto &a = result.corners[c], &b = shifted_result.corners[c];
    plane_exact &= length(b.position - (a.position + Vec3{.125f, 0, 0})) < 1e-7f &&
                   a.current.triangle == b.current.triangle &&
                   a.current.barycentric == b.current.barycentric &&
                   a.reflected.triangle == b.reflected.triangle &&
                   a.reflected.barycentric == b.reflected.barycentric;
  }
  check(plane_exact && same_positions(shifted->positions, shifted_positions),
        "Explicit captured plane retains real source coordinates and exact source owners without "
        "recentering");
  check(refuses([&] { refine_reflected_key(shifted, 1e-6f); }, "hole") &&
            same_positions(shifted->positions, shifted_positions),
        "Default user zero plane is never silently shifted to the source center");
  check(refuses([&] { refine_reflected_key(source, KeyMirrorPlane{NAN}, 1e-6f); }, "plane"),
        "Nonfinite explicit plane refuses before derived output");
  check(result.source == source && source->positions.size() == 4 &&
            result.corners.size() > source->indices.size() && result.corners.size() % 3 == 0,
        "Detached refinement keeps the genuine raw4 source and owns newly derived corners");
  double area = 0, field_error = 0, normal_error = 0, tangent_error = 0;
  bool interior = false;
  for (size_t c = 0; c < result.corners.size(); ++c) {
    const auto &v = result.corners[c];
    const auto &o = result.corners[result.opposite[c]];
    check(length(at(*source, v.current) - v.position) < 1e-6f &&
              length(reflect(at(*source, v.reflected)) - v.position) < 1e-6f,
          "Every derived corner has actual source-triangle barycentrics on both sides");
    check(result.opposite[result.opposite[c]] == c &&
              length(v.position - reflect(o.position)) < 1e-6f,
          "Explicit opposite corners form a genuine reflection involution");
    field_error =
        std::max(field_error, std::abs(sampled(v.reflected) - field(-v.position.x, v.position.y)));
    for (auto w : v.current.barycentric)
      interior |= w > 1e-6 && w < 1 - 1e-6;
  }
  for (size_t c = 0; c < result.corners.size(); c += 3) {
    Vec3 p[3];
    std::array<float, 2> uv[3];
    double x = 0, y = 0;
    for (size_t k = 0; k < 3; ++k) {
      const auto &v = result.corners[c + k];
      p[k] = v.position;
      p[k].z += float(sampled(v.reflected));
      uv[k] = {v.position.x + .5f, v.position.y + .5f};
      x += v.position.x / 3.;
      y += v.position.y / 3.;
    }
    area += length(cross(result.corners[c + 1].position - result.corners[c].position,
                         result.corners[c + 2].position - result.corners[c].position)) *
            .5;
    const bool lower = y + .5 <= -x + .5;
    const double fx = lower ? -.04 : .15, fy = lower ? -.10 : .09;
    const auto expected_n = normalized(Vec3{float(-fx), float(-fy), 1});
    const auto actual_n = normalized(cross(p[1] - p[0], p[2] - p[0]));
    normal_error = std::max(normal_error, double(length(expected_n - actual_n)));
    const auto e1 = p[1] - p[0], e2 = p[2] - p[0];
    const float du1 = uv[1][0] - uv[0][0], dv1 = uv[1][1] - uv[0][1], du2 = uv[2][0] - uv[0][0],
                dv2 = uv[2][1] - uv[0][1];
    const auto actual_t = normalized((e1 * dv2 - e2 * dv1) / (du1 * dv2 - du2 * dv1));
    const auto expected_t = normalized(Vec3{1, 0, float(fx)});
    tangent_error = std::max(tangent_error, double(length(actual_t - expected_t)));
  }
  std::cout << "REFINEMENT triangles=" << result.corners.size() / 3 << " area=" << area
            << " field=" << field_error << " N=" << normal_error << " T=" << tangent_error
            << std::endl;
  check(interior && std::abs(area - 1) < 1e-6 && field_error < 1e-7,
        "Nonmatching diagonals retain full area and exact mirrored piecewise field, including real "
        "interior points");
  check(normal_error < 1e-6 && tangent_error < 1e-6,
        "Independent analytic slope and UV derivative verify geometry-provenance N/T construction");
  bool independent_seam = false;
  for (size_t i = 0; i < result.corners.size(); ++i)
    for (size_t j = i + 1; j < result.corners.size(); ++j) {
      const auto &a = result.corners[i];
      const auto &b = result.corners[j];
      if (length(a.position - b.position) < 1e-7f && a.reflected.triangle != b.reflected.triangle) {
        const double x = sampled(a.reflected) + (a.reflected.triangle ? .03 : -.02),
                     y = sampled(b.reflected) + (b.reflected.triangle ? .03 : -.02);
        independent_seam |= std::abs(x - y) > .049;
      }
    }
  check(independent_seam, "Coincident derived corners keep distinct original face fields without "
                          "choosing or averaging their seam");
  auto reversed_source = std::make_shared<Mesh>(*source);
  for (size_t c = 0; c < reversed_source->indices.size(); c += 3)
    std::swap(reversed_source->indices[c + 1], reversed_source->indices[c + 2]);
  reversed_source->normals.assign(4, {0, 0, -1});
  const auto reverse_result = refine_reflected_key(reversed_source, 1e-6f);
  bool reverse_winding = true;
  for (size_t c = 0; c < reverse_result.corners.size(); c += 3)
    reverse_winding &=
        cross(reverse_result.corners[c + 1].position - reverse_result.corners[c].position,
              reverse_result.corners[c + 2].position - reverse_result.corners[c].position)
            .z < 0;
  check(reverse_winding, "Refinement preserves actual original clockwise source winding rather "
                         "than imposing projection order");
  auto vertical = std::make_shared<Mesh>(*source);
  for (auto &p : vertical->positions)
    p = {0, p.x, p.y};
  vertical->normals.assign(4, {1, 0, 0});
  const auto vertical_result = refine_reflected_key(vertical, 1e-6f);
  bool vertical_winding = true;
  for (size_t c = 0; c < vertical_result.corners.size(); c += 3)
    vertical_winding &=
        cross(vertical_result.corners[c + 1].position - vertical_result.corners[c].position,
              vertical_result.corners[c + 2].position - vertical_result.corners[c].position)
            .x > 0;
  check(vertical_winding, "Geometry-only reflection on its fixed plane retains actual corner "
                          "winding; it mints no role-normal parity certificate");
  size_t low = 1, high = 1024 * 1024;
  while (low < high) {
    const size_t mid = low + (high - low) / 2;
    KeyMirrorLimits bound;
    bound.max_bytes = mid;
    bool accepted = false;
    try {
      accepted = same(result, refine_reflected_key(source, 1e-6f, bound));
    } catch (const std::length_error &) {
    }
    if (accepted)
      high = mid;
    else
      low = mid + 1;
  }
  KeyMirrorLimits exact_cap;
  exact_cap.max_bytes = low;
  check(same(result, refine_reflected_key(source, 1e-6f, exact_cap)),
        "Exact conservative prospective byte cap accepts deterministic output");
  exact_cap.max_bytes = low - 1;
  check(refuses([&] { refine_reflected_key(source, 1e-6f, exact_cap); }, "quota"),
        "One byte below the conservative prospective cap refuses before the prohibited allocation");
  std::cout << "EXACT_FORECAST_CAP " << low << std::endl;
  auto quantized = std::make_shared<Mesh>(*source);
  for (auto &p : quantized->positions)
    p.y += 8388600.f;
  check(
      refuses([&] { refine_reflected_key(quantized, 1e-6f); }, "represent"),
      "Derived float positions cannot hide barycentric quantization outside the actual tolerance");
  auto duplicate = std::make_shared<Mesh>(*source);
  duplicate->indices.insert(duplicate->indices.end(), source->indices.begin(),
                            source->indices.end());
  check(refuses([&] { refine_reflected_key(duplicate, 1e-6f); }, "ambiguous"),
        "Coincident distinct source faces cannot silently borrow a nearest seam owner");
  auto asymmetric = std::make_shared<Mesh>(*source);
  asymmetric->positions[1].x += .1f;
  check(refuses([&] { refine_reflected_key(asymmetric, 1e-6f); }, "hole"),
        "Unrepresented reflected geometry refuses without widening the declared tolerance");
  auto oversized = std::make_shared<Mesh>();
  oversized->positions.resize(2'000'001);
  check(refuses([&] { refine_reflected_key(oversized, 1e-6f); }, "source streams"),
        "Input stream cardinality is bounded before uninterruptible mesh validation");
  oversized.reset();
  KeyMirrorLimits limits;
  limits.max_bytes = 1;
  check(refuses([&] { refine_reflected_key(source, 1e-6f, limits); }, "quota"),
        "Actual scratch/output allocation is preflighted against its unchanged explicit cap");
  limits = {};
  limits.max_work = 1;
  check(refuses([&] { refine_reflected_key(source, 1e-6f, limits); }, "work"),
        "Actual pair work is bounded before broad-phase arrays");
  limits = {};
  size_t polls = 0;
  limits.cancelled = [&] {
    ++polls;
    return false;
  };
  refine_reflected_key(source, 1e-6f, limits);
  for (size_t cut : {size_t(1), polls / 2, polls}) {
    size_t at = 0;
    limits.cancelled = [&] { return ++at >= cut; };
    check(refuses([&] { refine_reflected_key(source, 1e-6f, limits); }, "cancelled") &&
              same_positions(source->positions, positions) && source->indices == indices,
          "Early middle and late cancellation return no partial derived domain or changed source");
  }
  limits = {};
  limits.max_work = result.work_units;
  check(same(result, refine_reflected_key(source, 1e-6f, limits)),
        "Exact measured work boundary accepts");
  limits.max_work = result.work_units - 1;
  check(refuses([&] { refine_reflected_key(source, 1e-6f, limits); }, "work"),
        "One work unit below refuses without a partial returned domain");
  allocation::calls = 0;
  allocation::count = true;
  auto measured = refine_reflected_key(source, 1e-6f);
  allocation::count = false;
  const auto calls = allocation::calls;
  check(calls > 0 && calls < 1000,
        "Detached common refinement has a finite measured allocation frontier");
  for (size_t cut = 0; cut < calls; ++cut) {
    allocation::remaining = long(cut);
    bool failed = false;
    try {
      refine_reflected_key(source, 1e-6f);
    } catch (const std::bad_alloc &) {
      failed = true;
    } catch (...) {
      allocation::remaining = -1;
      throw;
    }
    allocation::remaining = -1;
    if (!failed || !same_positions(source->positions, positions) || source->indices != indices)
      throw std::runtime_error("Persistent common-refinement allocation rollback failed at " +
                               std::to_string(cut));
  }
  check(same(result, refine_reflected_key(source, 1e-6f)),
        "All persistent allocation denials leave an exact deterministic retry and original source");
  std::cout << "ALLOCATION_PREFIXES " << calls << " POLLS " << polls << " WORK_UNITS "
            << result.work_units << '\n';

  auto broad=std::make_shared<Mesh>();
  constexpr size_t squares=5000;
  broad->positions.reserve(squares*4);
  broad->indices.reserve(squares*6);
  for(size_t i=0;i<squares;++i){
    const auto start=uint32_t(broad->positions.size());
    for(auto p:positions)broad->positions.push_back({p.x,p.y+float(i)*2,p.z});
    for(auto v:indices)broad->indices.push_back(start+v);
  }
  broad->normals.assign(broad->positions.size(),{0,0,1});
  const auto scaled=refine_reflected_key(broad,.002f);
  bool exact=scaled.corners.size()==squares*result.corners.size();
  for(size_t i=0;i<scaled.corners.size()&&exact;++i){
    const auto &c=scaled.corners[i];
    const auto &expected=result.corners[i%result.corners.size()];
    const auto square_index=i/result.corners.size();
    exact=c.current.triangle==square_index*2+expected.current.triangle&&
      c.reflected.triangle==square_index*2+expected.reflected.triangle&&
      c.current.barycentric==expected.current.barycentric&&c.reflected.barycentric==expected.reflected.barycentric&&
      scaled.opposite[i]==square_index*result.corners.size()+result.opposite[i%result.corners.size()];
  }
  check(exact&&scaled.work_units<3'000'000,"10,000 triangles pass the old pair-count limit and match every ordered corner and barycentric oracle");
  std::cout<<"INDEXED_REFINEMENT "<<broad->indices.size()/3<<" input triangles "<<scaled.corners.size()/3
           <<" output triangles "<<scaled.work_units<<" work units\nPASS "<<checks<<" detached common-refinement checks\n";
  return 0;
} catch (const std::exception &e) {
  allocation::remaining = -1;
  allocation::count = false;
  std::cerr << "FAIL " << e.what() << std::endl;
  return 1;
}
