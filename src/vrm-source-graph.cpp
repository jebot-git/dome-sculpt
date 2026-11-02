// SPDX-License-Identifier: GPL-3.0-or-later
#include "vrm-source-graph.hpp"
#include "affine-factor.hpp"
#include "affine-matrix.hpp"
#include "import-skin.hpp"
#include "source-morph.hpp"
#include <set>
namespace dome::detail {
namespace {
using Json = nlohmann::json;
void stop(const std::function<bool()> &cancelled) {
  if (cancelled && cancelled())
    throw std::runtime_error("Original-space VRM export cancelled");
}
void require(bool value, const char *message) {
  if (!value)
    throw std::invalid_argument(std::string("VRM source graph: ") + message);
}
bool identity(CanvasTransform value) {
  return affine_similarity(value).matrix() == AffineTransform{}.matrix();
}
std::string human_role(std::string name, bool to_native) {
  for (const char *side : {"left", "right"}) {
    if (to_native) {
      if (name == std::string(side) + "ThumbProximal")
        return std::string(side) + "ThumbMetacarpal";
      if (name == std::string(side) + "ThumbIntermediate")
        return std::string(side) + "ThumbProximal";
    } else {
      if (name == std::string(side) + "ThumbMetacarpal")
        return std::string(side) + "ThumbProximal";
      if (name == std::string(side) + "ThumbProximal")
        return std::string(side) + "ThumbIntermediate";
    }
  }
  return name;
}
Json prefix_key(const std::vector<SceneGroup> &prefix) {
  Json result = Json::array();
  for (const auto &g : prefix)
    result.push_back({{"id", g.id},
                      {"parent", g.parent},
                      {"local", group_local_matrix(g).matrix()}});
  return result;
}
} // namespace
VrmSourceGraph vrm_source_graph(const std::vector<ObjectSurface> &objects,
                                const Json &original,
                                const std::function<bool()> &cancelled) {
  VrmSourceGraph graph;
  std::vector<bool> current;
  std::map<std::string, uint32_t> baseline_roles;
  std::map<std::string, std::optional<uint32_t>> edits;
  for (const auto &o : objects) {
    stop(cancelled);
    require(o.appearance && o.appearance->asset && o.appearance->original_bytes,
            "every source part needs immutable original identity");
    const auto &a = *o.appearance;
    require(identity(o.transform) && !o.affine_edit,
            "per-part placement needs an explicit coherent avatar-space "
            "conversion");
    std::shared_ptr<const SourceNodePlacement> placement;
    for (const auto &g : o.groups)
      if (g.source_placement)
        placement = g.source_placement;
    require(placement && placement->model &&
                placement->model->asset == a.asset &&
                placement->model->original_bytes == a.original_bytes &&
                placement->source_node == uint32_t(a.source_node),
            "source placement/mesh instance identity differs");
    auto root =
        std::find_if(o.groups.begin(), o.groups.end(), [&](const auto &g) {
          return g.id == placement->source_instance;
        });
    require(root != o.groups.end(), "native source-instance root is missing");
    std::vector<SceneGroup> prefix(o.groups.begin(), root + 1);
    if (!graph.model) {
      require(a.asset->nodes.size() <= 100000,
              "original node table exceeds100000 nodes");
      graph.model = placement->model;
      graph.instance = placement->source_instance;
      graph.prefix = prefix;
      for (const auto &n : a.asset->nodes)
        graph.current_local.push_back(n.local);
      current.resize(a.asset->nodes.size());
      require(original.at("nodes").is_array() &&
                  original.at("nodes").size() == a.asset->nodes.size(),
              "original node JSON/table identity changed");
      graph.original_nodes = original.at("nodes").get<std::vector<Json>>();
      graph.names.resize(a.asset->nodes.size());
      graph.humanoid = original.at("extensions").at("VRM").at("humanoid");
      for (const auto &bone : graph.humanoid.at("humanBones"))
        baseline_roles.emplace(
            human_role(bone.at("bone").get<std::string>(), true),
            bone.at("node").get<uint32_t>());
    }
    require(placement->model == graph.model &&
                placement->source_instance == graph.instance &&
                prefix_key(prefix) == prefix_key(graph.prefix),
            "parts do not share one original source instance/native placement");
    int previous = -1;
    for (auto at = root + 1; at != o.groups.end(); ++at) {
      require(at->source_placement &&
                  at->source_placement->model == graph.model &&
                  at->source_placement->source_instance == graph.instance,
              "source-node path contains an unrelated placement");
      require(
          identity(at->transform) && !at->affine_edit,
          "edited source-node placement needs coherent rig/metadata transport");
      auto n = at->source_placement->source_node;
      require(a.asset->nodes.at(n).parent == previous,
              "source-node ancestry no longer matches the original path");
      previous = int(n);
    }
    require(o.deformation && o.deformation->skeleton && o.deformation->skin,
            "source part needs authored native skin authority");
    const auto &d = *o.deformation;
    const auto &s = *d.skeleton;
    require(
        s.space_matrix &&
            *s.space_matrix ==
                imported_inverse(
                    a.asset->nodes.at(size_t(a.source_node)).world),
        "custom bind-space change needs explicit original-space conversion");
    require(a.source_joint_nodes.size() == s.bones.size(),
            "source joint identity table is stale");
    require(bool(d.humanoid), "source skin needs complete humanoid authority");
    d.humanoid->validate(s, true);
    for (const auto &role : human_roles()) {
      std::optional<uint32_t> before, after;
      auto b = baseline_roles.find(role.name);
      if (b != baseline_roles.end())
        before = b->second;
      auto found = d.humanoid->joints.find(role.name);
      if (found != d.humanoid->joints.end())
        after = a.source_joint_nodes.at(found->second);
      if (before != after) {
        auto edit = edits.find(role.name);
        if (edit != edits.end())
          require(edit->second == after,
                  "one humanoid role has conflicting authored assignments "
                  "across source skins");
        else
          edits.emplace(role.name, after);
      }
    }
    auto posed = s.local_matrices(d.pose);
    for (size_t bone = 0; bone < s.bones.size(); ++bone) {
      stop(cancelled);
      auto source = a.source_joint_nodes[bone];
      require(source < a.asset->nodes.size(),
              "joint identity is outside original nodes");
      const auto &native = s.bones[bone];
      const auto &original = a.asset->nodes[source];
      if (native.name != imported_bone_name(original, source)) {
        if (graph.names[source])
          require(
              *graph.names[source] == native.name,
              "one source joint has conflicting authored names across skins");
        else
          graph.names[source] = native.name;
      }

      require(native.rest_matrix && *native.rest_matrix == original.local,
              "edited original rest axes need metadata refitting");
      int parent = native.parent < 0
                       ? -1
                       : int(a.source_joint_nodes.at(size_t(native.parent)));
      require(parent == original.parent, "source joint ancestry changed");
      if (current[source])
        require(
            graph.current_local[source] == posed[bone],
            "one original joint has conflicting current poses across skins");
      else {
        current[source] = true;
        graph.current_local[source] = posed[bone];
      }
    }
  }
  require(bool(graph.model), "no original source graph");
  auto resolved = baseline_roles;
  for (const auto &[role, value] : edits) {
    if (value)
      resolved[role] = *value;
    else
      resolved.erase(role);
  }
  HumanoidMapping mapping;
  const auto &first = *objects.front().appearance;
  for (const auto &[role, source] : resolved) {
    auto at = std::find(first.source_joint_nodes.begin(),
                        first.source_joint_nodes.end(), source);
    require(at != first.source_joint_nodes.end(),
            "humanoid role source is outside the shared native joint table");
    mapping.joints.emplace(role,
                           uint32_t(at - first.source_joint_nodes.begin()));
  }
  mapping.validate(*objects.front().deformation->skeleton, true);
  Json bones = Json::array();
  std::set<std::string> emitted;
  for (const auto &bone : graph.humanoid.at("humanBones")) {
    auto role = human_role(bone.at("bone").get<std::string>(), true);
    auto at = resolved.find(role);
    if (at == resolved.end())
      continue;
    auto current = bone;
    current["node"] = at->second;
    bones.push_back(std::move(current));
    emitted.insert(role);
  }
  for (const auto &[role, source] : resolved)
    if (!emitted.contains(role))
      bones.push_back({{"bone", human_role(role, false)},
                       {"node", source},
                       {"useDefaultValues", true}});
  graph.humanoid["humanBones"] = std::move(bones);
  return graph;
}
std::vector<int> emit_vrm_source_graph(
    GltfDocument &doc, const std::vector<ObjectSurface> &objects,
    const VrmSourceGraph &graph, const std::function<bool()> &cancelled) {
  auto &j = doc.json;
  const auto &asset = *graph.model->asset;
  require(objects.size() == doc.objects.size(),
          "emitted object identity count changed");
  auto attach = [&](int parent, Json node) {
    stop(cancelled);
    if (j["nodes"].size() >= 200000)
      throw std::length_error(
          "VRM original source graph exceeds200000 emitted nodes");
    auto n = int(j["nodes"].size());
    j["nodes"].push_back(std::move(node));
    if (parent < 0)
      j["scenes"][0]["nodes"].push_back(n);
    else
      j["nodes"][parent]["children"].push_back(n);
    return n;
  };
  auto matrix = [&](int parent, const AffineTransform &value,
                    const std::string &name) {
    auto m = value.float_matrix();
    if (gltf_trs_matrix(m))
      return attach(parent, {{"name", name}, {"matrix", m}});
    for (const auto &factor : factor_edit_for_gltf(value, cancelled).nodes)
      parent =
          attach(parent, {{"name", name}, {"matrix", factor.float_matrix()}});
    return parent;
  };
  int root = -1;
  for (const auto &g : graph.prefix) {
    if (g.affine_edit)
      root = matrix(root, g.affine_edit->matrix(), g.name + " native affine");
    root = matrix(root, affine_similarity(g.transform), g.name);
    if (g.source_placement)
      root = matrix(root, g.source_placement->local(), g.name + " original");
  }
  root = attach(root, {{"name", "Original VRM shared source graph"}});
  std::vector<int> nodes(asset.nodes.size(), -1);
  std::vector<uint8_t> state(nodes.size());
  auto visit = [&](auto &&self, size_t source, size_t depth) -> int {
    require(depth <= 256, "original source node ancestry exceeds256 levels");
    stop(cancelled);
    if (state[source] == 2)
      return nodes[source];
    require(state[source] == 0, "cycle in original source graph");
    state[source] = 1;
    auto parent =
        asset.nodes[source].parent < 0
            ? root
            : self(self, size_t(asset.nodes[source].parent), depth + 1);
    require(gltf_trs_matrix(asset.nodes[source].local),
            "original local node is not a representable glTF TRS matrix");
    if (graph.current_local[source] == asset.nodes[source].local) {
      auto node = graph.original_nodes.at(source);
      for (const char *key : {"mesh", "skin", "weights", "children", "camera"})
        node.erase(key);
      nodes[source] = attach(parent, std::move(node));
    } else {
      nodes[source] =
          matrix(parent, AffineTransform(graph.current_local[source]),
                 asset.nodes[source].name);
      const auto &raw = graph.original_nodes.at(source);
      if (raw.contains("extras"))
        j["nodes"][nodes[source]]["extras"] = raw.at("extras");
      if (raw.contains("extensions"))
        j["nodes"][nodes[source]]["extensions"] = raw.at("extensions");
    }
    if (graph.names[source])
      j["nodes"][nodes[source]]["name"] = *graph.names[source];
    state[source] = 2;
    return nodes[source];
  };
  for (size_t n = 0; n < nodes.size(); ++n)
    visit(visit, n, 0);
  // Move the actual rendered surface under its original node and retarget
  // skins. Existing generated bind-space joints are no longer authoritative.
  // They are left as inert empty graph nodes for now; no metadata or skin
  // references them.
  std::vector<int> parents(j["nodes"].size(), -1);
  for (size_t parent = 0; parent < j["nodes"].size(); ++parent)
    if (j["nodes"][parent].contains("children"))
      for (const auto &child : j["nodes"][parent]["children"])
        parents.at(child.get<size_t>()) = int(parent);
  for (size_t i = 0; i < objects.size(); ++i) {
    stop(cancelled);
    auto &emitted = doc.objects[i];
    const auto &a = *objects[i].appearance;
    require(emitted.node >= 0 && size_t(emitted.node) < parents.size(),
            "surface node identity is invalid");
    auto old = parents[size_t(emitted.node)];
    require(old >= 0, "skinned surface has no native object parent");
    auto &children = j["nodes"][old]["children"];
    children.erase(
        std::remove(children.begin(), children.end(), Json(emitted.node)),
        children.end());
    if (children.empty())
      j["nodes"][old].erase("children");
    if (j["nodes"][old].contains("extras"))
      j["nodes"][old]["extras"].erase("domeSculptHumanoid");
    j["nodes"][nodes.at(size_t(a.source_node))]["children"].push_back(
        emitted.node);
    auto skin = j["nodes"][emitted.node].at("skin").get<size_t>();
    auto &output = j["skins"].at(skin);
    output["joints"] = Json::array();
    emitted.joints.clear();
    for (auto source : a.source_joint_nodes) {
      auto joint = nodes.at(source);
      emitted.joints.push_back(uint32_t(joint));
      output["joints"].push_back(joint);
    }
    output["skeleton"] = root;
  }
  return nodes;
}
} // namespace dome::detail
