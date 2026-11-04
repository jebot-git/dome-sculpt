// SPDX-License-Identifier: GPL-3.0-or-later
// Independent emitted-format consumer of already completed fields. No original
// artist weight BIN is an input; current native preview is compared with parsed
// emitted POSITION/morph/skin/bind/node matrices for every triangle corner.
#include "cgltf.h"
#include "dome/rig-rest.hpp"
#include "dome/scene.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <set>
using namespace dome;
namespace {
size_t checks = 0;
void ensure(bool v, const char *why) {
  ++checks;
  if (!v)
    throw std::runtime_error(why);
}
using P = std::array<double, 3>;
P point(const float *m, P p) {
  return {m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12],
          m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13],
          m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14]};
}
P parsed(const cgltf_node &node, const cgltf_primitive &p, size_t v) {
  auto positions = cgltf_find_accessor(&p, cgltf_attribute_type_position, 0);
  float base[3]{};
  ensure(positions && cgltf_accessor_read_float(positions, v, base, 3),
         "Independent POSITION read");
  P value{base[0], base[1], base[2]};
  for (size_t t = 0; t < p.targets_count; ++t) {
    float w = node.weights_count > t         ? node.weights[t]
              : node.mesh->weights_count > t ? node.mesh->weights[t]
                                             : 0;
    for (size_t a = 0; a < p.targets[t].attributes_count; ++a)
      if (p.targets[t].attributes[a].type == cgltf_attribute_type_position) {
        float delta[3]{};
        ensure(cgltf_accessor_read_float(p.targets[t].attributes[a].data, v,
                                         delta, 3),
               "Independent morph POSITION read");
        for (size_t c = 0; c < 3; ++c)
          value[c] += double(w) * delta[c];
      }
  }
  ensure(node.skin && node.skin->inverse_bind_matrices,
         "Actual skin/inverse-bind authority retained");
  P result{};
  size_t influences = 0;
  for (int set = 0; set < 8; ++set) {
    auto joints = cgltf_find_accessor(&p, cgltf_attribute_type_joints, set);
    auto weights = cgltf_find_accessor(&p, cgltf_attribute_type_weights, set);
    if (!joints && !weights)
      break;
    ensure(joints && weights, "Paired joint/weight sets");
    unsigned j[4]{};
    float w[4]{};
    ensure(cgltf_accessor_read_uint(joints, v, j, 4) &&
               cgltf_accessor_read_float(weights, v, w, 4),
           "Independent weight reads");
    for (size_t k = 0; k < 4; ++k)
      if (w[k]) {
        ensure(j[k] < node.skin->joints_count,
               "Independent joint index in range");
        float bind[16]{}, world[16]{};
        ensure(cgltf_accessor_read_float(node.skin->inverse_bind_matrices, j[k],
                                         bind, 16),
               "Independent inverse bind read");
        cgltf_node_transform_world(node.skin->joints[j[k]], world);
        const auto transformed = point(world, point(bind, value));
        for (size_t c = 0; c < 3; ++c)
          result[c] += double(w[k]) * transformed[c];
        ++influences;
      }
  }
  ensure(influences > 0 && influences <= 4,
         "Emitted source method keeps fixed4 nonzero influences");
  return result;
}
} // namespace
int main(int argc,char**argv)try {
 if(argc!=3)throw std::invalid_argument("Actual linked-pose saved Scene, output directory");
 const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);
 auto scene=Scene::load(argv[1]);const auto scene_before=scene.encode();
 std::vector<uint64_t> targets;for(const auto&o:scene.objects())if(o.appearance&&o.rig_source&&o.deformation&&o.deformation->skin)targets.push_back(o.id);
 ensure(targets.size()==19,"Exact actual19 complete linked-pose recipients");
 size_t corners=0,primitives=0;double max_error=0;int pose=0;auto reports=nlohmann::json::array();
    auto surfaces = scene.export_surfaces();
    std::erase_if(surfaces, [&](const auto &s) {
      return std::find(targets.begin(), targets.end(), s.id) == targets.end();
    });
    ensure(surfaces.size() == targets.size(),
           "Every explicitly bound target retained once; no missing original "
           "part or blank default object");
    const auto file = out / ("pose-" + std::to_string(pose) + ".glb");
    export_glb(surfaces, file);
    cgltf_options opts{};
    cgltf_data *raw = nullptr;
    ensure(cgltf_parse_file(&opts, file.c_str(), &raw) == cgltf_result_success,
           "Independent actual current GLB parse");
    std::unique_ptr<cgltf_data, decltype(&cgltf_free)> owner(raw, cgltf_free);
    ensure(cgltf_load_buffers(&opts, raw, file.c_str()) ==
                   cgltf_result_success &&
               cgltf_validate(raw) == cgltf_result_success,
           "Independent complete actual GLB validates");
    std::set<uint64_t> seen;
    for (size_t n = 0; n < raw->nodes_count; ++n) {
      const auto &node = raw->nodes[n];
      if (!node.mesh)
        continue;
      const std::string name = node.mesh->name ? node.mesh->name : "";
      const SculptObject *object = nullptr;
      for (auto id : targets)
        if (scene.object(id).name == name) {
          ensure(!object, "Unique retained target name correspondence");
          object = &scene.object(id);
        }
      ensure(object && seen.insert(object->id).second,
             "No emitted role/part fabrication, duplication or omission");
      const auto &mesh = object->painted->mesh();
      const auto evaluated = object->deformation->preview(mesh);
      const auto world = scene.world_matrix(object->id);
      size_t offset = 0;
      double local_max = 0;
      for (size_t pi = 0; pi < node.mesh->primitives_count; ++pi) {
        const auto &p = node.mesh->primitives[pi];
        ++primitives;
        const auto *position =
            cgltf_find_accessor(&p, cgltf_attribute_type_position, 0);
        ensure(position, "Current emitted POSITION present");
        const size_t count = p.indices ? p.indices->count : position->count;
        ensure(offset + count <= mesh.indices.size(),
               "Current ordered triangle-corner bound");
        ensure(cgltf_find_accessor(&p, cgltf_attribute_type_normal, 0) &&
                   cgltf_find_accessor(&p, cgltf_attribute_type_texcoord, 0),
               "Original current normal/UV role not dropped by skin export");
        ensure(p.targets_count ==
                   (object->deformation->morphs
                        ? object->deformation->morphs->targets.size()
                        : 0),
               "All current authored morph target identities retained");
        for (size_t c = 0; c < count; ++c) {
          const auto index =
              p.indices ? cgltf_accessor_read_index(p.indices, c) : c;
          const auto actual = parsed(node, p, index);
          const auto v = evaluated.positions[mesh.indices[offset + c]];
          const auto expected = world.point({v.x, v.y, v.z});
          const double e =
              std::hypot(actual[0] - expected[0], actual[1] - expected[1],
                         actual[2] - expected[2]);
          local_max = std::max(local_max, e);
          ++corners;
        }
        offset += count;
      }
      ensure(offset == mesh.indices.size(),
             "Every original current triangle corner exported once");
      max_error = std::max(max_error, local_max);
      reports.push_back({{"pose", pose},
                         {"object", object->id},
                         {"name", name},
                         {"max_error_m", local_max}});
    }
    ensure(seen.size() == targets.size(),
           "All bound target parts survive independent parse");
    ensure(scene.encode() == scene_before,
           "Actual export preserves complete immutable source Scene");
    std::cout << "EXPORTED pose=" << pose << " parts=" << seen.size()
              << " max_error=" << max_error << std::endl;

 ensure(max_error<=3e-5,"Independent parsed emitted actual linked-pose corners within unchanged3e-5m");
 std::ofstream(out/"quality.json")<<nlohmann::json{{"checks",checks},{"parts",targets.size()},{"corners",corners},{"primitives",primitives},{"max_error_m",max_error},{"bound_m",3e-5},{"records",reports}}.dump(2);
 std::cout<<"PASS actual_linked_pose_export checks="<<checks<<" parts="<<targets.size()<<" corners="<<corners<<" max_error="<<max_error<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
