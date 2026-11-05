// SPDX-License-Identifier: GPL-3.0-or-later
// Separate AFTER-SOLVE oracle. Artist values are read only by this process;
// the native field input is a saved genuinely unskinned model in another test.
#include "cgltf.h"
#include "dome/humanoid-creator.hpp"
#include "dome/scene.hpp"
#include "dome/source-placement.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <numbers>
using namespace dome;
namespace {
using P = std::array<double, 3>;
AffineTransform matrix(const float *p) {
  std::array<double, 16> a{};
  for (size_t i = 0; i < 16; ++i)
    a[i] = p[i];
  return AffineTransform(a);
}
AffineTransform matrix(const RigMatrix &m) { return matrix(m.data()); }
P pt(const AffineTransform &m, P p) { return m.point(p); }
double distance(P a, P b) {
  return std::hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
}
nlohmann::json meta(const std::filesystem::path &p) {
  std::ifstream f(p, std::ios::binary);
  uint32_t h[5]{};
  f.read(reinterpret_cast<char *>(h), sizeof(h));
  if (!f || h[4] != 0x4e4f534a || h[3] > 4 * 1024 * 1024)
    throw std::runtime_error("Bounded original JSON");
  std::string s(h[3], '\0');
  f.read(s.data(), s.size());
  return nlohmann::json::parse(s);
}
void read(const cgltf_accessor *a, size_t v, float *p, size_t n) {
  if (!a || !cgltf_accessor_read_float(a, v, p, n))
    throw std::runtime_error("Independent original accessor read");
}
void rotation(cgltf_node &node, P axis, double degrees) {
  if (node.has_matrix)
    throw std::runtime_error(
        "Original matrix-local pose requires an explicit adapter");
  double h = degrees * std::numbers::pi / 360, s = std::sin(h), c = std::cos(h);
  const auto q =
      std::array<double, 4>{axis[0] * s, axis[1] * s, axis[2] * s, c};
  const auto b = std::array<double, 4>{node.rotation[0], node.rotation[1],
                                       node.rotation[2], node.rotation[3]};
  node.rotation[0] =
      float(q[3] * b[0] + q[0] * b[3] + q[1] * b[2] - q[2] * b[1]);
  node.rotation[1] =
      float(q[3] * b[1] - q[0] * b[2] + q[1] * b[3] + q[2] * b[0]);
  node.rotation[2] =
      float(q[3] * b[2] + q[0] * b[1] - q[1] * b[0] + q[2] * b[3]);
  node.rotation[3] =
      float(q[3] * b[3] - q[0] * b[0] - q[1] * b[1] - q[2] * b[2]);
  node.has_rotation = true;
}
P original_point(const cgltf_node &node, const cgltf_primitive &primitive,
                 size_t raw, std::span<const float> morph) {
  float p[3]{};
  read(cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0), raw,
       p, 3);
  P base{p[0], p[1], p[2]};
  for (size_t t = 0; t < primitive.targets_count; ++t) {
    const float w = t < morph.size() ? morph[t] : 0;
    if (!w)
      continue;
    for (size_t a = 0; a < primitive.targets[t].attributes_count; ++a)
      if (primitive.targets[t].attributes[a].type ==
          cgltf_attribute_type_position) {
        read(primitive.targets[t].attributes[a].data, raw, p, 3);
        for (size_t k = 0; k < 3; ++k)
          base[k] += double(w) * p[k];
      }
  }
  if (!node.skin)
    throw std::runtime_error(
        "Original independent oracle needs actual source skin");
  P out{};
  size_t terms = 0;
  for (int set = 0; set < 8; ++set) {
    auto ja = cgltf_find_accessor(&primitive, cgltf_attribute_type_joints, set),
         wa =
             cgltf_find_accessor(&primitive, cgltf_attribute_type_weights, set);
    if (!ja && !wa)
      break;
    if (!ja || !wa)
      throw std::runtime_error("Original paired weight sets");
    unsigned joints[4]{};
    float weights[4]{};
    if (!cgltf_accessor_read_uint(ja, raw, joints, 4))
      throw std::runtime_error("Independent original joint indices");
    read(wa, raw, weights, 4);
    for (size_t k = 0; k < 4; ++k)
      if (weights[k]) {
        if (joints[k] >= node.skin->joints_count)
          throw std::runtime_error("Original skin index");
        float ibm[16]{}, world[16]{};
        read(node.skin->inverse_bind_matrices, joints[k], ibm, 16);
        cgltf_node_transform_world(node.skin->joints[joints[k]], world);
        auto q = pt(matrix(world) * matrix(ibm), base);
        for (size_t c = 0; c < 3; ++c)
          out[c] += double(weights[k]) * q[c];
        ++terms;
      }
  }
  if (!terms)
    throw std::runtime_error("Original source row has no real weights");
  return out;
}
} // namespace
int main(int argc, char **argv) try {
  if (argc != 4 && argc != 5)
    throw std::invalid_argument("Choose ORIGINAL artist GLB, completed native "
                                "bound Scene and report directory");
  const std::filesystem::path original = argv[1], bound = argv[2],
                              out = argv[3];
  std::filesystem::create_directories(out);
  // The complete owning field already exists on disk before original BIN opens.
  auto source = Scene::load(bound);
  const auto input = source.encode();
  const auto j = meta(original);
  cgltf_options opts{};
  cgltf_data *raw = nullptr;
  if (cgltf_parse_file(&opts, original.c_str(), &raw) != cgltf_result_success)
    throw std::runtime_error("Independent original parse");
  std::unique_ptr<cgltf_data, decltype(&cgltf_free)> owner(raw, cgltf_free);
  if (cgltf_load_buffers(&opts, raw, original.c_str()) !=
          cgltf_result_success ||
      cgltf_validate(raw) != cgltf_result_success)
    throw std::runtime_error("Independent original buffers/validation");
  std::map<std::string, size_t> roles;
  for (const auto &e :
       j.at("extensions").at("VRM").at("humanoid").at("humanBones")) {
    std::string name = e.at("bone");
    if (name.ends_with("ThumbProximal"))
      name.replace(name.size() - 8, 8, "Metacarpal");
    else if (name.ends_with("ThumbIntermediate"))
      name.replace(name.size() - 12, 12, "Proximal");
    if (!roles.emplace(name, e.at("node").get<size_t>()).second)
      throw std::runtime_error("Ambiguous original humanoid role");
  }
  std::vector<std::array<float, 4>> rotations;
  std::vector<AffineTransform> rest;
  for (size_t n = 0; n < raw->nodes_count; ++n) {
    auto &node = raw->nodes[n];
    rotations.push_back({node.rotation[0], node.rotation[1], node.rotation[2],
                         node.rotation[3]});
    float m[16];
    cgltf_node_transform_world(&node, m);
    rest.push_back(matrix(m));
  }
  double low = INFINITY, high = -INFINITY;
  std::vector<uint64_t> targets;
  for (const auto &o : source.objects())
    if (o.mesh_active && o.painted) {
      auto w = source.world_matrix(o.id);
      for (auto p : o.painted->mesh().positions) {
        auto q = w.point({p.x, p.y, p.z});
        low = std::min(low, q[1]);
        high = std::max(high, q[1]);
      }
      if (o.deformation && o.deformation->skin && o.rig_source)
        targets.push_back(o.id);
    }
  if (targets.empty() || !(high > low))
    throw std::runtime_error(
        "Actual owning bound targets and whole-model height required");
  const double height = high - low;
  size_t failures = 0, rows = 0;
  double transport_max = 0, rest_max = 0;
  nlohmann::json records = nlohmann::json::array();
  const bool local = argc == 5 && std::string(argv[4]) == "local";
  if (argc == 5 && !local) throw std::invalid_argument("Unknown quality cohort");
  const int poses = local ? 18 : 5;
  for (int pose = 0; pose < poses; ++pose) {
    for (size_t n = 0; n < raw->nodes_count; ++n)
      std::copy(rotations[n].begin(), rotations[n].end(),
                raw->nodes[n].rotation);
    auto rotate = [&](const std::string &name, P axis, double angle) {
      auto at = roles.find(name);
      if (at != roles.end())
        rotation(raw->nodes[at->second], axis, angle);
    };
    if (!local && pose == 0) {
      rotate("leftLowerArm", {0, 0, 1}, 20);
      rotate("rightUpperLeg", {0, 0, 1}, 20);
    }
    if (!local && pose == 1) {
      rotate("head", {0, 0, 1}, 30);
      rotate("neck", {0, 1, 0}, -15);
    }
    if (!local && (pose == 2 || pose == 4)) {
      rotate("leftHand", {0, 0, 1}, 20);
      rotate("leftMiddleIntermediate", {1, 0, 0}, 45);
      rotate("rightFoot", {1, 0, 0}, 20);
    }
    if (!local && pose == 3) {
      rotate("spine", {0, 0, 1}, 15);
      rotate("hips", {0, 1, 0}, 10);
      rotate("leftShoulder", {0, 0, 1}, 20);
    }
    if (local) {
      static const std::array<std::string,18> joints{"head","head","head","head","head","head","leftHand","rightHand","leftMiddleIntermediate","rightMiddleIntermediate","leftToes","rightToes","leftLowerArm","rightLowerLeg","leftEye","leftEye","rightEye","rightEye"};
      static const std::array<P,18> axes{P{1,0,0},P{1,0,0},P{0,1,0},P{0,1,0},P{0,0,1},P{0,0,1},P{0,0,1},P{0,0,1},P{1,0,0},P{1,0,0},P{1,0,0},P{1,0,0},P{0,0,1},P{1,0,0},P{0,1,0},P{0,1,0},P{0,1,0},P{0,1,0}};
      if (!roles.contains(joints[pose])) throw std::runtime_error("Local oracle requires genuine articulated role");
      rotate(joints[pose],axes[pose],pose<6?(pose%2?-25:25):pose>=14?(pose%2?-15:15):30);
    }
    std::vector<AffineTransform> delta;
    for (size_t n = 0; n < raw->nodes_count; ++n) {
      float m[16];
      cgltf_node_transform_world(raw->nodes + n, m);
      delta.push_back(matrix(m) * rest[n].inverse());
    }
    auto posed_scene = source.snapshot();
    for (auto id : targets) {
      const auto &object = source.object(id);
      if (!object.appearance || !object.deformation->humanoid)
        throw std::runtime_error(
            "Exact raw vertex/native role authority required");
      const auto &a = *object.appearance;
      const auto &mesh = object.painted->mesh();
      if (a.source_node < 0 || size_t(a.source_node) >= raw->nodes_count ||
          a.source_mesh < 0 || a.source_primitive < 0 ||
          a.source_vertices.size() != mesh.positions.size())
        throw std::runtime_error(
            "No count-only original geometry correspondence");
      auto &node = raw->nodes[a.source_node];
      if (node.mesh != raw->meshes + a.source_mesh ||
          size_t(a.source_primitive) >= node.mesh->primitives_count)
        throw std::runtime_error(
            "Actual original node/mesh/primitive identity");
      auto &primitive = node.mesh->primitives[a.source_primitive];
      const auto world = source.world_matrix(id),
                 inverse_world = world.inverse();
      auto d = *object.deformation;
      const auto &rig = *d.skeleton;
      const auto native_rest = rig.world();
      std::vector<std::string> native_roles(rig.bones.size());
      for (const auto &[role, joint] : d.humanoid->joints) {
        if (joint >= native_roles.size() || !native_roles[joint].empty() ||
            !roles.contains(role))
          throw std::runtime_error("Ambiguous native/common-motion role map");
        native_roles[joint] = role;
      }
      std::vector<AffineTransform> native_delta, desired;
      for (size_t b = 0; b < rig.bones.size(); ++b) {
        if (native_roles[b].empty())
          throw std::runtime_error(
              "Unmapped native auxiliary needs explicit common-motion oracle");
        native_delta.push_back(delta[roles.at(native_roles[b])]);
        desired.push_back(inverse_world * native_delta.back() * world *
                          matrix(native_rest[b]));
      }
      d.pose.clear();
      for (size_t b = 0; b < rig.bones.size(); ++b) {
        auto local =
            rig.bones[b].parent < 0
                ? (rig.space_matrix
                       ? matrix(*rig.space_matrix).inverse() * desired[b]
                       : desired[b])
                : desired[size_t(rig.bones[b].parent)].inverse() * desired[b];
        auto value = rig.bones[b].rest;
        value.full_local_matrix = local.float_matrix();
        d.pose.push_back(value);
      }
      std::vector<float> original_morph(primitive.targets_count);
      for (size_t t = 0; t < original_morph.size(); ++t)
        original_morph[t] = node.weights_count > t ? node.weights[t]
                            : node.mesh->weights_count > t
                                ? node.mesh->weights[t]
                                : 0;
      if (!local && pose == 4 && d.morphs && !d.morphs->targets.empty()) {
        if (a.source_morph_targets.size() != d.morphs->targets.size() ||
            a.source_morph_targets[0] >= original_morph.size())
          throw std::runtime_error("Exact authored morph identity");
        if (d.morph_weights.empty())
          d.morph_weights.assign(d.morphs->targets.size(), 0);
        d.morph_weights[0] = .35f;
        original_morph[a.source_morph_targets[0]] = .35f;
      }
      const auto preview = d.preview(mesh);
      double maximum = 0, mean = 0;
      for (size_t v = 0; v < mesh.positions.size(); ++v) {
        float original_position[3]{};
        read(cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0),
             a.source_vertices[v], original_position, 3);
        const auto raw_rest =
            pt(rest[a.source_node], {original_position[0], original_position[1],
                                     original_position[2]});
        const auto current_rest =
            pt(world,
               {mesh.positions[v].x, mesh.positions[v].y, mesh.positions[v].z});
        rest_max = std::max(rest_max, distance(raw_rest, current_rest));
        P p{mesh.positions[v].x, mesh.positions[v].y, mesh.positions[v].z};
        if (d.morphs && !d.morph_weights.empty())
          for (size_t t = 0; t < d.morphs->targets.size(); ++t)
            for (size_t c = 0; c < 3; ++c) {
              const auto dv = d.morphs->targets[t].delta[v];
              p[c] += double(d.morph_weights[t]) * (c == 0   ? dv.x
                                                    : c == 1 ? dv.y
                                                             : dv.z);
            }
        p = pt(world, p);
        P manual{};
        for (auto weight : d.skin->vertices[v]) {
          const auto q = pt(native_delta.at(weight.joint), p);
          for (size_t c = 0; c < 3; ++c)
            manual[c] += double(weight.weight) * q[c];
        }
        const auto expected = original_point(
            node, primitive, a.source_vertices[v], original_morph);
        auto native = preview.positions[v];
        transport_max = std::max(
            transport_max,
            distance(manual, pt(world, {native.x, native.y, native.z})));
        const double error = distance(expected, manual);
        maximum = std::max(maximum, error);
        mean += error;
        ++rows;
      }
      mean /= mesh.positions.size();
      P lo{INFINITY,INFINITY,INFINITY},hi{-INFINITY,-INFINITY,-INFINITY};
      for (auto p : mesh.positions) {auto q=pt(world,{p.x,p.y,p.z});for(size_t k=0;k<3;++k){lo[k]=std::min(lo[k],q[k]);hi[k]=std::max(hi[k],q[k]);}}
      const double part_diagonal=distance(lo,hi);
      const bool pass = maximum <= .03 * height && mean <= .01 * height &&
          (!local || (maximum <= .03 * part_diagonal && mean <= .01 * part_diagonal));
      failures += !pass;
      records.push_back({{"object", id},
                         {"part_diagonal_m", part_diagonal},
                         {"local_max_fraction", maximum/part_diagonal},
                         {"local_mean_fraction", mean/part_diagonal},
                         {"name", object.name},
                         {"pose", pose},
                         {"vertices", mesh.positions.size()},
                         {"maximum_m", maximum},
                         {"mean_m", mean},
                         {"max_fraction", maximum / height},
                         {"mean_fraction", mean / height},
                         {"pass", pass}});
      std::cout << "POSE object=" << id << " pose=" << pose
                << " max=" << maximum / height << " mean=" << mean / height
                << " pass=" << pass << std::endl;
      posed_scene.set_deformation(id, d);
    }
    posed_scene.save(out / ("pose-" + std::to_string(pose) + ".dsculpt"));
  }
  if (source.encode() != input)
    throw std::runtime_error("Independent oracle mutated saved field Scene");
  nlohmann::json report{
      {"height_m", height},
      {"parts", targets.size()},
      {"poses", poses},
      {"local_part_bounds", local},
      {"cohorts", records.size()},
      {"failures", failures},
      {"evaluated_vertices", rows},
      {"rest_correspondence_max_m", rest_max},
      {"production_pose_transport_max_m", transport_max},
      {"max_fraction_bound", .03},
      {"mean_fraction_bound", .01},
      {"pose_transport_bound_m", 3e-5},
      {"artist_values_only_in_separate_post_solve_process", true},
      {"records", records}};
  std::ofstream(out / "quality.json") << report.dump(2);
  std::cout << "RESULT cohorts=" << records.size() << " failed=" << failures
            << " rest=" << rest_max << " pose_transport=" << transport_max
            << " vertices=" << rows << std::endl;
  return failures || rest_max > 3e-5 || transport_max > 3e-5 ? 1 : 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
