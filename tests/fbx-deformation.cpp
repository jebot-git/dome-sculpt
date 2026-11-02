// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/gltf-export.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/import-authoring.hpp"
#include "dome/import-native.hpp"
#include "cgltf.h"
#include "ufbx.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using namespace dome;
namespace {
size_t checks = 0, comparisons = 0;
void check(bool ok, const char *message) {
  if (!ok) throw std::runtime_error(message);
  ++checks;
}
std::string bytes(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream), {}};
}
using SourceScene = std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)>;
SourceScene load_reference(const std::filesystem::path &path) {
  ufbx_load_opts options{};
  options.strict = true;
  options.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_HELPER_NODES;
  options.inherit_mode_handling = UFBX_INHERIT_MODE_HANDLING_HELPER_NODES;
  options.target_axes = ufbx_axes_right_handed_y_up;
  options.target_unit_meters = 1;
  options.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
  options.evaluate_skinning = true;
  SourceScene scene(ufbx_load_file(path.c_str(), &options, nullptr), ufbx_free_scene);
  check(bool(scene), "Independent source evaluator loads the genuine FBX input");
  return scene;
}
std::vector<Vec3> native_world(const Scene &scene, uint64_t id) {
  const auto &object = scene.object(id);
  auto mesh = object.deformation ? object.deformation->preview(object.painted->mesh())
                                 : object.painted->mesh();
  const auto world = scene.world_matrix(id);
  for (auto &v : mesh.positions) {
    const auto p = world.point({v.x, v.y, v.z});
    v = {float(p[0]), float(p[1]), float(p[2])};
  }
  return mesh.positions;
}
void compare_source(const Scene &scene, std::span<const uint64_t> objects,
                    const ufbx_scene &source) {
  double maximum = 0, magnitude = 0;
  for (auto id : objects) {
    const auto &appearance = *scene.object(id).appearance;
    const auto &primitive = appearance.asset->primitives.at(appearance.source_primitive);
    const auto corners = nlohmann::json::parse(primitive.extras_json).at("dome_source_corners");
    check(appearance.source_node >= 0 && size_t(appearance.source_node) < source.nodes.count,
          "Native mesh preserves the original source node identity");
    const auto *node = source.nodes[size_t(appearance.source_node)];
    const auto *mesh = node->mesh;
    const auto &positions = mesh->skinned_position.exists ? mesh->skinned_position : mesh->vertex_position;
    const auto actual = native_world(scene, id);
    for (size_t i = 0; i < actual.size(); ++i) {
      auto p = ufbx_get_vertex_vec3(&positions, corners.at(appearance.source_vertices.at(i)).get<uint32_t>());
      if (mesh->skinned_is_local) p = ufbx_transform_position(&node->geometry_to_world, p);
      const double a[3] = {actual[i].x, actual[i].y, actual[i].z};
      for (size_t k = 0; k < 3; ++k) {
        maximum = std::max(maximum, std::abs(a[k] - p.v[k]));
        magnitude = std::max(magnitude, std::abs(p.v[k]));
      }
      ++comparisons;
    }
  }
  check(maximum <= 2e-5 + 1e-6 * magnitude,
        "Independent FBX evaluator agrees with actual native world positions");
}
Vec3 point(const float *matrix, Vec3 p) {
  return {matrix[0]*p.x+matrix[4]*p.y+matrix[8]*p.z+matrix[12],
          matrix[1]*p.x+matrix[5]*p.y+matrix[9]*p.z+matrix[13],
          matrix[2]*p.x+matrix[6]*p.y+matrix[10]*p.z+matrix[14]};
}
void compare_export(const Scene &scene, std::span<const uint64_t> objects,
                    const std::filesystem::path &path) {
  export_glb(scene.surfaces(), path);
  cgltf_options options{}; cgltf_data *raw = nullptr;
  check(cgltf_parse_file(&options, path.c_str(), &raw) == cgltf_result_success,
        "Independent glTF consumer parses the emitted FBX authoring result");
  std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data(raw, cgltf_free);
  check(cgltf_load_buffers(&options, data.get(), path.c_str()) == cgltf_result_success &&
        cgltf_validate(data.get()) == cgltf_result_success,
        "Independent consumer validates emitted buffers, accessors, skin and hierarchy");
  size_t object = 0;
  for (size_t n = 0; n < data->nodes_count; ++n) {
    const auto &node = data->nodes[n];
    if (!node.mesh) continue;
    check(object < objects.size() && node.mesh->primitives_count == 1,
          "Genuine single-primitive fixture emits one independent mesh instance");
    const auto id = objects[object++];
    const auto expected = native_world(scene, id);
    const auto &rest = scene.object(id).painted->mesh();
    const auto &primitive = node.mesh->primitives[0];
    auto *positions = cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
    check(positions && primitive.indices && primitive.indices->count == rest.indices.size(),
          "Export preserves ordered triangle-corner correspondence across independent frame seam expansion");
    const auto &deformation = scene.object(id).deformation;
    if(deformation && deformation->skin)check(node.skin != nullptr,"Export retains editable actual skin rather than flattening its current pose");
    if(deformation && deformation->morphs)check(primitive.targets_count == deformation->morphs->targets.size(),"Export retains every editable actual morph target");
    double maximum = 0, magnitude = 0;
    for (size_t corner = 0; corner < rest.indices.size(); ++corner) {
      const auto v = cgltf_accessor_read_index(primitive.indices,corner);
      const auto native_vertex = rest.indices[corner];
      check(v < positions->count,"Independent emitted corner has a valid vertex identity");
      float p[3]; check(cgltf_accessor_read_float(positions,v,p,3), "Independent POSITION reads");
      Vec3 value{p[0],p[1],p[2]};
      for (size_t t = 0; t < primitive.targets_count; ++t) {
        const auto &target = primitive.targets[t]; const cgltf_accessor *offset = nullptr;
        for (size_t a = 0; a < target.attributes_count; ++a)
          if (target.attributes[a].type == cgltf_attribute_type_position) offset = target.attributes[a].data;
        const float weight = t < node.weights_count ? node.weights[t] :
                             t < node.mesh->weights_count ? node.mesh->weights[t] : 0;
        if (offset && weight != 0) {
          float delta[3]; check(cgltf_accessor_read_float(offset,v,delta,3), "Independent morph offset reads");
          value += Vec3{delta[0],delta[1],delta[2]} * weight;
        }
      }
      Vec3 world{};
      if (node.skin) {
        double total = 0;
        for (int set = 0;; ++set) {
          auto *joints = cgltf_find_accessor(&primitive,cgltf_attribute_type_joints,set);
          auto *weights = cgltf_find_accessor(&primitive,cgltf_attribute_type_weights,set);
          check(bool(joints) == bool(weights), "Every emitted skin set has paired joint and weight authority");
          if (!joints) break;
          float j[4], w[4];
          check(cgltf_accessor_read_float(joints,v,j,4) && cgltf_accessor_read_float(weights,v,w,4),
                "Independent joint and weight accessor reads");
          for (size_t k = 0; k < 4; ++k) if (w[k] != 0) {
            const auto joint = size_t(j[k]); check(joint < node.skin->joints_count,"Valid emitted joint index");
            float bind[16], transform[16];
            check(cgltf_accessor_read_float(node.skin->inverse_bind_matrices,joint,bind,16),"Independent original bind reads");
            cgltf_node_transform_world(node.skin->joints[joint],transform);
            world += point(transform,point(bind,value)) * w[k]; total += w[k];
          }
        }
        check(std::abs(total-1) < 1e-5,"Independent emitted positive influence sum remains normalized");
      } else { float matrix[16]; cgltf_node_transform_world(&node,matrix); world = point(matrix,value); }
      const double actual[3]={world.x,world.y,world.z}, wanted[3]={expected[native_vertex].x,expected[native_vertex].y,expected[native_vertex].z};

      for (size_t k=0;k<3;++k) { maximum=std::max(maximum,std::abs(actual[k]-wanted[k])); magnitude=std::max(magnitude,std::abs(wanted[k])); }
    }
    if(maximum > 2e-5+1e-6*magnitude)std::cerr<<"GLB mismatch "<<path<<" max="<<maximum<<" magnitude="<<magnitude<<" targets="<<primitive.targets_count<<"\n";
    check(maximum <= 2e-5+1e-6*magnitude,"Independent emitted skin/morph/world evaluation preserves the authored result");
  }
  check(object == objects.size(),"Independent consumer finds every actual imported object");
}
void restart(const Scene &scene,const std::filesystem::path &path) {
  const auto encoded=scene.encode(); scene.save(path);
  check(Scene::load(path).encode()==encoded,"Native Save/restart retains complete source, paint, deformation and pose");
}
}
int main(int argc,char **argv) try {
  check(argc==2,"Provide a private output directory"); const std::filesystem::path out=argv[1];
  std::filesystem::create_directories(out);
  const std::filesystem::path inputs=DOME_FBX_FIXTURE_DIRECTORY;
  const std::vector<std::string> files={"maya_game_sausage_7500_ascii.fbx","maya_game_sausage_7500_ascii_combined.fbx",
    "blender_293_half_skinned_7400_binary.fbx","maya_blend_shape_cube_7700_ascii.fbx","blender_279_shape_weights_7400_binary.fbx"};
  for (const auto &file:files) {
    ImportAuthoringOptions options; options.paint_resolution=64;
    auto imported=prepare_authoring_import(Scene{},inputs/file,options); auto source=load_reference(inputs/file);
    const auto before=imported.scene.encode();
    compare_source(imported.scene,imported.objects,*source);
    restart(imported.scene,out/(file+".dsculpt")); compare_export(imported.scene,imported.objects,out/(file+".glb"));
    if (source->skin_clusters.count) for (int mode=0;mode<3;++mode) {
      const auto joint=source->skin_clusters[0]->bone_node->typed_id; Scene scene=imported.scene.snapshot();
      ufbx_transform_override change{}; change.node_id=joint; change.transform=source->nodes[joint]->local_transform;
      const Quat turn{float(std::cos(.15)),0,0,float(std::sin(.15))};
      if(mode==0)change.transform.translation.x+=.03125;
      if(mode==1)change.transform.rotation=ufbx_quat_normalize(ufbx_quat_mul({turn.x,turn.y,turn.z,turn.w},change.transform.rotation));
      if(mode==2){change.transform.scale.x*=.8;change.transform.scale.y*=1.2;change.transform.scale.z*=1.05;}
      size_t changed=0; double motion=0;
      for(auto id:imported.objects) {
        auto &o=scene.object(id); if(!o.deformation||!o.deformation->skeleton)continue;
        const auto &nodes=o.appearance->source_joint_nodes; auto it=std::find(nodes.begin(),nodes.end(),joint); if(it==nodes.end())continue;
        auto d=std::make_shared<DeformationState>(*o.deformation); if(d->pose.empty())for(const auto &bone:d->skeleton->bones)d->pose.push_back(bone.rest);
        auto &t=d->pose.at(size_t(it-nodes.begin())); if(mode==0)t.translation.x+=.03125f;
        if(mode==1)t.rotation=(turn*t.rotation).unit(); if(mode==2){t.scale.x*=.8f;t.scale.y*=1.2f;t.scale.z*=1.05f;}
        d->validate(o.painted->mesh()); o.deformation=std::move(d); ++changed;
        const auto old=native_world(imported.scene,id),now=native_world(scene,id);
        for(size_t v=0;v<old.size();++v)motion=std::max(motion,double(length(now[v]-old[v])));
      }
      check(changed>0&&motion>1e-5,"Pose oracle is sensitive to real translation, rotation and anisotropic scale");
      ufbx_anim_opts animation{}; animation.transform_overrides={&change,1};
      std::unique_ptr<ufbx_anim,decltype(&ufbx_free_anim)> anim(ufbx_create_anim(source.get(),&animation,nullptr),ufbx_free_anim);
      check(bool(anim),"Independent source joint override creates an evaluator");
      ufbx_evaluate_opts evaluation{}; evaluation.evaluate_skinning=true;
      SourceScene posed(ufbx_evaluate_scene(source.get(),anim.get(),0,&evaluation,nullptr),ufbx_free_scene);
      check(bool(posed),"Independent source evaluator produces the edited skin pose");
      compare_source(scene,imported.objects,*posed); const auto name=file+"-pose-"+std::to_string(mode);
      restart(scene,out/(name+".dsculpt")); compare_export(scene,imported.objects,out/(name+".glb"));
    }
    check(imported.scene.encode()==before,"Independent pose/export checks retain the exact original source endpoint");
  }
  auto negative=[&](const std::filesystem::path &path,std::string_view expected){
    bool refused=false;try{capture_native_source(path);}catch(const std::exception &e){refused=std::string_view(e.what()).find(expected)!=std::string_view::npos;}
    check(refused,"Unsupported actual source semantics refuse with their precise diagnosis");
  };
  negative(inputs/"max_transformed_skin_7500_ascii.fbx","dual-quaternion skinning");
  auto transparent=bytes(inputs/files[0]);const std::string marker="P: \"TransparencyFactor\", \"Number\", \"\", \"A\",1";
  const auto at=transparent.find(marker);check(at!=std::string::npos&&transparent.find(marker,at+1)==std::string::npos,"Derived material negative changes one genuine authored property block");
  transparent.insert(at+marker.size(),"\n\t\t\tP: \"TransparentColor\", \"Color\", \"\", \"A\",0.2,0.3,0.4");
  const auto path=out/"authored-transparency.fbx";{std::ofstream stream(path);stream<<transparent;}
  negative(path,"extended material requires explicit semantic conversion");
  std::cout<<checks<<" native FBX authoring/consumer checks; "<<comparisons<<" independent world vertices\n";
  return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
