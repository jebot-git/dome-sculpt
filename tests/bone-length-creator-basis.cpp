// SPDX-License-Identifier: GPL-3.0-or-later
#include "vrm-rest-consumer-fixtures.hpp"
#include "dome/creation.hpp"

namespace {
using Json = nlohmann::json;
using Gltf = std::unique_ptr<cgltf_data, decltype(&cgltf_free)>;

const AffineTransform reflected_shear(std::array<double, 16>{
    -1.1, 0, 0, 0, .2, .9, 0, 0, .1, .05, 1.2, 0, .3, -.1, .2, 1});
const AffineTransform similarity(std::array<double, 16>{
    0, 0, -1.25, 0, 0, 1.25, 0, 0, 1.25, 0, 0, 0, .3, -.1, .2, 1});

Gltf export_and_read(const Scene &scene, const std::filesystem::path &file) {
  const auto before = scene.encode();
  const auto surfaces = scene.surfaces();
  export_vrm(surfaces, file);
  cgltf_options options{};
  cgltf_data *raw = nullptr;
  if (cgltf_parse_file(&options, file.c_str(), &raw) != cgltf_result_success)
    throw std::runtime_error("Independent VRM parse failed");
  Gltf owner(raw, cgltf_free);
  check(cgltf_load_buffers(&options, raw, file.c_str()) == cgltf_result_success &&
            cgltf_validate(raw) == cgltf_result_success,
        "Independent cgltf validates every emitted accessor and graph");

  const auto inverse_basis = surfaces.front().appearance->source_basis_quaternion->inverse();
  double maximum = 0;
  size_t corners = 0, parts = 0;
  for (size_t n = 0; n < raw->nodes_count; ++n) {
    auto &node = raw->nodes[n];
    if (!node.mesh)
      continue;
    const auto &surface = surfaces.at(size_t(node.mesh - raw->meshes));
    const auto mesh = surface.preview_mesh();
    const auto world = surface.world_matrix();
    const auto &primitive = node.mesh->primitives[0];
    check(node.skin && primitive.indices &&
              primitive.indices->count == mesh.indices.size(),
          "Emitted creator part retains its skin and complete ordered corners");
    ++parts;
    for (size_t c = 0; c < mesh.indices.size(); ++c) {
      const auto p = mesh.positions[mesh.indices[c]];
      const auto w = world.point({p.x, p.y, p.z});
      const auto expected = inverse_basis.rotate({float(w[0]), float(w[1]), float(w[2])});
      const auto actual = consume(node, cgltf_accessor_read_index(primitive.indices, c));
      maximum = std::max(maximum, double(length(actual - expected)));
      ++corners;
    }
  }
  std::cout << file.filename() << " corners=" << corners << " max=" << maximum << '\n';
  check(parts == surfaces.size() && corners > 10000 && maximum <= 2e-5,
        "Every actual CC0 skinned corner follows the complete native placement");
  check(scene.encode() == before, "VRM export leaves the complete native project unchanged");
  return owner;
}

Json vrm_metadata(const cgltf_data &document) {
  return Json::parse(document.json, document.json + document.json_size)
      .at("extensions").at("VRM");
}

void verify_metadata(const cgltf_data &ordinary, const cgltf_data &placed,
                     const AffineTransform &native_placement, bool has_springs) {
  auto first = vrm_metadata(ordinary);
  auto next = vrm_metadata(placed);
  // VRoid's native basis is a 180 degree Y rotation. Conjugate the native
  // placement independently of the exporter and apply it to original centers.
  const D basis{-1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1};
  const auto placement = product(product(basis, native_placement.matrix()), basis);
  auto world = [](const cgltf_data &data, size_t id) {
    if (id >= data.nodes_count)
      throw std::runtime_error("VRM metadata node outside the document");
    return node_world(data.nodes[id]);
  };
  auto from = [&](size_t id) { return world(ordinary, id); };
  auto to = [&](size_t id) { return world(placed, id); };
  auto compare_node = [&](size_t a, size_t b) {
    near(to(b), product(placement, from(a)));
  };
  auto compare_point = [&](size_t a, size_t b, const Json &offset) {
    near(point(to(b), unity_point(offset)),
         point(placement, point(from(a), unity_point(offset))));
  };

  auto &a = first.at("firstPerson");
  auto &b = next.at("firstPerson");
  const size_t old_bone = a.at("firstPersonBone"), new_bone = b.at("firstPersonBone");
  compare_node(old_bone, new_bone);
  compare_point(old_bone, new_bone, a.at("firstPersonBoneOffset"));
  b["firstPersonBone"] = old_bone;
  check(a == b, "First-person offset, mesh annotations and look-at fields are preserved");

  auto &source_springs = first.at("secondaryAnimation");
  auto &target_springs = next.at("secondaryAnimation");
  auto &source_colliders = source_springs.at("colliderGroups");
  auto &target_colliders = target_springs.at("colliderGroups");
  auto &source_bones = source_springs.at("boneGroups");
  auto &target_bones = target_springs.at("boneGroups");
  check(source_colliders.size() == target_colliders.size() &&
            source_bones.size() == target_bones.size(),
        "Spring and collider group counts are preserved");
  check(has_springs ? !source_colliders.empty() && !source_bones.empty()
                    : source_colliders.empty() && source_bones.empty(),
        "The fixture covers the actual VRoid spring metadata for this baseline");
  for (size_t i = 0; i < source_colliders.size(); ++i) {
    const auto &old_group = source_colliders.at(i);
    auto &new_group = target_colliders.at(i);
    const size_t old_node = old_group.at("node"), new_node = new_group.at("node");
    compare_node(old_node, new_node);
    for (const auto &collider : old_group.at("colliders"))
      compare_point(old_node, new_node, collider.at("offset"));
    new_group["node"] = old_node;
  }

  auto compare_chain = [&](auto &&self, size_t old_id, size_t new_id, size_t depth) -> void {
    compare_node(old_id, new_id);
    const auto &old_node = ordinary.nodes[old_id];
    const auto &new_node = placed.nodes[new_id];
    if (depth > 128 || old_node.children_count != new_node.children_count)
      throw std::runtime_error("Spring subtree changed under external placement");
    for (size_t i = 0; i < old_node.children_count; ++i)
      self(self, size_t(old_node.children[i] - ordinary.nodes),
           size_t(new_node.children[i] - placed.nodes), depth + 1);
  };
  for (size_t i = 0; i < source_bones.size(); ++i) {
    const auto &old_group = source_bones.at(i);
    auto &new_group = target_bones.at(i);
    const int old_center = old_group.value("center", -1);
    const int new_center = new_group.value("center", -1);
    if (old_center >= 0 && new_center >= 0) {
      compare_node(size_t(old_center), size_t(new_center));
      new_group["center"] = old_center;
    }
    const auto &old_roots = old_group.at("bones");
    auto &new_roots = new_group.at("bones");
    if (old_roots.size() != new_roots.size())
      throw std::runtime_error("Spring root count changed under external placement");
    for (size_t r = 0; r < old_roots.size(); ++r)
      compare_chain(compare_chain, old_roots.at(r), new_roots.at(r), 0);
    new_roots = old_roots;
  }
  check(source_springs == target_springs,
        "Spring topology, collider radii/offsets, and local settings are preserved");
}

std::vector<uint8_t> output_bytes(const std::filesystem::path &file) {
  std::ifstream in(file, std::ios::binary);
  if (!in)
    throw std::runtime_error("Expected existing export was missing");
  return {std::istreambuf_iterator<char>(in), {}};
}

void verify_refusal(const Scene &scene, const std::filesystem::path &file,
                    std::string_view reason) {
  const auto before = scene.encode();
  const auto previous = output_bytes(file);
  bool rejected = false;
  try {
    export_vrm(scene.surfaces(), file);
  } catch (const std::invalid_argument &error) {
    if (std::string_view(error.what()).find(reason) == std::string_view::npos)
      throw;
    rejected = true;
  }
  check(rejected && output_bytes(file) == previous && scene.encode() == before,
        "Unsupported creator placement preserves the existing export and native scene");
}
} // namespace

int main(int argc, char **argv) try {
  if (argc != 2)
    return 2;
  const auto dir = std::filesystem::path(argv[1]);
  std::filesystem::create_directories(dir);
  for (bool male : {false, true}) {
    Scene scene;
    auto controls = humanoid_baseline_defaults(male ? "vroid-beta-male" : "vroid-beta-female");
    // The male base has no spring geometry. Its non-uniformly shaped mesh can
    // still use an exact shared affine parent with first-person metadata.
    if (male)
      controls.head_width = .04f;
    const auto parent = scene.add_group("Placement");
    const auto group = append_humanoid(scene, controls, "Native CC0", {}, 64);
    scene.group(group).parent = parent;
    const auto prefix = male ? "male-" : "female-";
    const auto ordinary = export_and_read(scene, dir / (std::string(prefix) + "identity.vrm"));
    scene.group(parent).affine_edit = std::make_shared<const NativeAffineEdit>(reflected_shear);
    const auto placed = dir / (std::string(prefix) + "metric.vrm");
    const auto external = export_and_read(scene, placed);
    verify_metadata(*ordinary, *external, reflected_shear, !male);

    if (!male) {
      const auto part = scene.surfaces().front().id;
      scene.object(part).affine_edit = std::make_shared<const NativeAffineEdit>(similarity);
      verify_refusal(scene, placed, "one shared placement");
    }
  }

  Scene styled;
  auto controls = humanoid_baseline_defaults("vroid-beta-female");
  controls.head_width = .04f;
  const auto group = append_humanoid(styled, controls, "Styled CC0", {}, 64);
  const auto ordinary = export_and_read(styled, dir / "styled-identity.vrm");
  styled.group(group).affine_edit = std::make_shared<const NativeAffineEdit>(similarity);
  const auto placed = dir / "styled-similarity.vrm";
  const auto external = export_and_read(styled, placed);
  verify_metadata(*ordinary, *external, similarity, true);
  styled.group(group).affine_edit = std::make_shared<const NativeAffineEdit>(reflected_shear);
  verify_refusal(styled, placed, "nonlinear creator spring refitting");
  return 0;
} catch (const std::exception &error) {
  std::cerr << "FAIL " << error.what() << '\n';
  return 1;
}
