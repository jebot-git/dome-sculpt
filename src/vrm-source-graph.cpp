// SPDX-License-Identifier: GPL-3.0-or-later
#include "vrm-source-graph.hpp"
#include "affine-factor.hpp"
#include "affine-matrix.hpp"
#include "import-skin.hpp"
#include "source-morph.hpp"
#include "source-rest-graph.hpp"
#include "dome/polygon-scene.hpp"
#include "dome/humanoid-creator.hpp"
#include "bone-metadata.hpp"
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
                                const std::function<bool()> &cancelled,size_t max_snapshot_bytes) {
  VrmSourceGraph graph;
  // Normalize only the new fitted route. Ordinary unedited source exports keep
  // their existing allocation/geometry path. The format's existing256MiB output
  // bound is also the explicit detached REST-snapshot budget; no renderer cap.
  std::optional<SourceRestGraph> fitted;
  if(std::any_of(objects.begin(),objects.end(),[](const auto &o){return bool(o.rig_source);})){auto selected=std::find_if(objects.begin(),objects.end(),[](const auto &o){return o.deformation&&o.deformation->skin;});require(selected!=objects.end(),"fitted source export needs an actual source skin");fitted=capture_source_rest_graph(objects,selected->id,max_snapshot_bytes,cancelled);}

  std::vector<bool> current;
  std::vector<std::optional<float>> joint_lengths;
  const ObjectSurface *rig_source = nullptr;
  std::map<std::string, uint32_t> baseline_roles;
  std::map<std::string, std::optional<uint32_t>> edits;
  for (const auto &o : objects) {
    stop(cancelled);
    require(o.appearance && o.appearance->asset && o.appearance->original_bytes,
            "every source part needs immutable original identity");
    const auto &a = *o.appearance;
    if (o.deformation && o.deformation->skin)
      require(identity(o.transform) && !o.affine_edit,
              "skinned per-part placement needs an explicit coherent avatar-space conversion");
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
      for (const auto &n : a.asset->nodes){
        graph.current_local.push_back(n.local);graph.parents.push_back(n.parent);
        graph.rest_local.push_back(n.local);graph.rest_world.push_back(n.world);
      }
      if(fitted){require(fitted->model==graph.model&&fitted->instance==graph.instance,"fitted normalized source owner differs");graph.parents=fitted->parents;graph.rest_local=fitted->rest_local;graph.rest_world=fitted->rest_world;graph.current_local=source_rest_pose(*fitted,cancelled).local;for(size_t n=0;n<a.asset->nodes.size();++n)graph.fitted_rest|=graph.parents[n]!=a.asset->nodes[n].parent||graph.rest_local[n]!=a.asset->nodes[n].local;}

      current.resize(a.asset->nodes.size());
      require(original.at("nodes").is_array() &&
                  original.at("nodes").size() == a.asset->nodes.size(),
              "original node JSON/table identity changed");
      graph.original_nodes = original.at("nodes").get<std::vector<Json>>();
      graph.names.resize(a.asset->nodes.size());
      joint_lengths.resize(a.asset->nodes.size());
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
    const bool skinned = o.deformation && o.deformation->skin;
    int previous = -1;
    bool rigid_native_tail = false;
    for (auto at = root + 1; at != o.groups.end(); ++at) {
      if(!at->source_placement){
        // A rigid object's native subgroups are artist placement after its
        // authenticated original endpoint. The emitter retains their full
        // world delta in the rigid mesh attachment. They are not new original
        // source nodes, and cannot replace/interleave that source chain.
        require(!skinned && previous==a.source_node,
                "native subgroup must follow the exact rigid source endpoint");
        rigid_native_tail=true;
        continue;
      }
      require(!rigid_native_tail,
              "source placement cannot follow a native rigid subgroup");
      require(at->source_placement &&
                  at->source_placement->model == graph.model &&
                  at->source_placement->source_instance == graph.instance,
              "source-node path contains an unrelated placement");
      if (skinned)
        require(identity(at->transform) && !at->affine_edit,
                "edited source-node placement needs coherent rig/metadata transport");
      auto n = at->source_placement->source_node;
      require(a.asset->nodes.at(n).parent == previous,
              "source-node ancestry no longer matches the original path");
      previous = int(n);
    }
    require(previous==a.source_node,
            "source-node path does not reach the actual mesh endpoint");
    require(skinned == (a.asset->nodes.at(size_t(a.source_node)).skin >= 0),
            "native/source skin ownership changed");
    if (!skinned) {
      require(a.source_joint_nodes.empty() &&
                  (!o.deformation || (!o.deformation->skeleton && !o.deformation->humanoid)),
              "rigid source part must not invent skeleton/joint authority");
      continue;
    }
    require(o.deformation->skeleton.has_value(),
            "source part needs authored native skeleton authority");
    if (!rig_source) rig_source = &o;
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
      if(joint_lengths[source])require(*joint_lengths[source]==native.length,
          "one actual source joint has conflicting per-part localY lengths; resolve that authority explicitly");
      else joint_lengths[source]=native.length;
      if (native.name != imported_bone_name(original, source)) {
        if (graph.names[source])
          require(
              *graph.names[source] == native.name,
              "one source joint has conflicting authored names across skins");
        else
          graph.names[source] = native.name;
      }

      require(native.rest_matrix && *native.rest_matrix == graph.rest_local[source],
              "current source REST differs from authenticated native metadata frame");
      int parent = native.parent < 0
                       ? -1
                       : int(a.source_joint_nodes.at(size_t(native.parent)));
      require(parent == graph.parents[source], "current source joint ancestry differs from authenticated metadata frame");
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
  require(bool(graph.model) && rig_source,
          "original-space VRM needs at least one authenticated humanoid skin");
  auto resolved = baseline_roles;
  for (const auto &[role, value] : edits) {
    if (value)
      resolved[role] = *value;
    else
      resolved.erase(role);
  }
  HumanoidMapping mapping;
  const auto &first = *rig_source->appearance;
  for (const auto &[role, source] : resolved) {
    auto at = std::find(first.source_joint_nodes.begin(),
                        first.source_joint_nodes.end(), source);
    require(at != first.source_joint_nodes.end(),
            "humanoid role source is outside the shared native joint table");
    mapping.joints.emplace(role,
                           uint32_t(at - first.source_joint_nodes.begin()));
  }
  mapping.validate(*rig_source->deformation->skeleton, true);
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
  graph.rest_metadata=prepare_vrm0_rest_metadata(graph,original.at("extensions").at("VRM"),cancelled);
  return graph;
}
std::vector<int> emit_vrm_source_graph(
    GltfDocument &doc, const std::vector<ObjectSurface> &objects,
    const VrmSourceGraph &graph, const std::function<bool()> &cancelled,std::vector<int> *metadata_nodes) {
  auto &j = doc.json;
  const auto &asset = *graph.model->asset;
  require(objects.size() == doc.objects.size(),
          "emitted object identity count changed");
  // make_gltf's native placement/joint nodes are an implementation graph,
  // not another authored source. Keep its actual surface payloads and skin
  // accessors, then build exactly one original-node graph below. Leaving the
  // generated forest active duplicates every joint and makes our own output
  // exceed native hierarchy limits on fresh import.
  std::vector<Json> surfaces;
  std::vector<size_t> source_surfaces(asset.nodes.size());
  surfaces.reserve(objects.size());
  for(size_t i=0;i<objects.size();++i){
    stop(cancelled);
    const auto &emitted=doc.objects[i];
    require(emitted.node>=0 && size_t(emitted.node)<j.at("nodes").size(),
            "surface node identity is invalid");
    auto surface=j.at("nodes").at(size_t(emitted.node));
    require(surface.contains("mesh") && surface.at("mesh").get<int>()==emitted.mesh &&
                !surface.contains("children"),"generated surface payload is not a mesh leaf");
    ++source_surfaces.at(size_t(objects[i].appearance->source_node));
    surfaces.push_back(std::move(surface));
  }
  j["nodes"]=Json::array();
  j["scenes"][0]["nodes"]=Json::array();
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
    if(g.humanoid){g.humanoid->validate();
      j["nodes"][root]["extras"]["domeSculptCreator"]={
        {"type","humanoid"},{"version",1},{"controls",g.humanoid->encode()}};
    }
    if (g.source_placement)
      root = matrix(root, g.source_placement->local(), g.name + " original");
  }
  root = attach(root, {{"name", "Original VRM shared source graph"}});
  AffineTransform prefix_world;
  for (const auto &g : graph.prefix)
    prefix_world = prefix_world * group_local_matrix(g);
  std::vector<AffineTransform> source_world(asset.nodes.size());
  std::vector<int> nodes(asset.nodes.size(), -1);
  std::vector<int> branch_roots(asset.nodes.size(),-1);
  std::vector<uint8_t> state(nodes.size());
  auto visit = [&](auto &&self, size_t source, size_t depth) -> int {
    require(depth <= 256, "original source node ancestry exceeds256 levels");
    stop(cancelled);
    if (state[source] == 2)
      return nodes[source];
    require(state[source] == 0, "cycle in original source graph");
    state[source] = 1;
    auto parent =
        graph.parents.at(source) < 0
            ? root
            : self(self, size_t(graph.parents.at(source)), depth + 1);
    source_world[source] =
        (graph.parents.at(source) < 0 ? prefix_world :
         source_world.at(size_t(graph.parents.at(source)))) *
        AffineTransform(graph.current_local[source]);
    require(gltf_trs_matrix(asset.nodes[source].local),
            "original local node is not a representable glTF TRS matrix");
    branch_roots[source]=int(j["nodes"].size());
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
  // Numeric source IDs are not the authored child order. Preserve that order
  // exactly wherever ancestry is unchanged, including spring first-child
  // tails, then append genuine reparented children deterministically.
  std::vector<std::vector<uint32_t>> current_children(nodes.size());
  for(size_t n=0;n<nodes.size();++n)if(graph.parents[n]>=0)
    current_children.at(size_t(graph.parents[n])).push_back(uint32_t(n));
  for(size_t parent=0;parent<nodes.size();++parent){
    stop(cancelled);Json children=Json::array();
    for(auto child:asset.nodes[parent].children)
      if(graph.parents.at(child)==int(parent))children.push_back(branch_roots.at(child));
    for(auto child:current_children[parent])
      if(asset.nodes[child].parent!=int(parent))children.push_back(branch_roots.at(child));
    if(children.empty())j["nodes"][nodes[parent]].erase("children");
    else j["nodes"][nodes[parent]]["children"]=std::move(children);
  }
  for(const auto &object:objects)if(object.deformation&&object.deformation->skin){
    const auto &lengths=object.deformation->skeleton->bones;
    const auto &sources=object.appearance->source_joint_nodes;
    for(size_t slot=0;slot<sources.size();++slot){stop(cancelled);
      write_bone_length(j["nodes"][nodes.at(sources[slot])],lengths.at(slot).length);
    }
  }
  if(metadata_nodes)*metadata_nodes=nodes;
  if(graph.rest_metadata){
    std::map<uint32_t,int> attachments;
    auto attach_frame=[&](const std::optional<VrmMetadataFrame> &frame){if(!frame)return;auto at=attachments.find(frame->source);if(at!=attachments.end()){if(metadata_nodes)(*metadata_nodes)[frame->source]=at->second;return;}const auto leaf=matrix(nodes.at(frame->source),AffineTransform(frame->local),asset.nodes.at(frame->source).name+" attachment");attachments.emplace(frame->source,leaf);if(metadata_nodes)(*metadata_nodes)[frame->source]=leaf;};
    for(const auto &frame:graph.rest_metadata->colliders)attach_frame(frame);
    for(const auto &frame:graph.rest_metadata->centers)attach_frame(frame);
  }
  // Reattach real surfaces only. One unambiguous surface can live directly on
  // its original node, retaining that node's name/extras/ordered children.
  // Material/primitive splits and nonidentity rigid artist placements retain
  // separate mesh leaves; no dummy skins or skeletal end-sites are invented.
  for (size_t i = 0; i < objects.size(); ++i) {
    stop(cancelled);
    auto &emitted = doc.objects[i];
    const auto &a = *objects[i].appearance;
    auto surface=std::move(surfaces[i]);
    int target = nodes.at(size_t(a.source_node));
    bool direct=source_surfaces.at(size_t(a.source_node))==1;
    for(auto at=surface.begin();at!=surface.end();++at)
      if(at.key()!="name" && at.key()!="mesh" && at.key()!="skin" && at.key()!="weights" &&
         at.key()!="translation" && at.key()!="rotation" && at.key()!="scale" && at.key()!="matrix")direct=false;
    if (!objects[i].deformation || !objects[i].deformation->skin) {
      require(!surface.contains("skin") && emitted.joints.empty(),
              "rigid source surface unexpectedly acquired a skin");
      const auto local = source_world.at(size_t(a.source_node)).inverse() *
                         objects[i].world_matrix();
      if (local.matrix() != AffineTransform{}.matrix()){
        direct=false;
        target = matrix(target, local, objects[i].name + " native rigid placement");
      }
      for (const char *key : {"translation","rotation","scale","matrix"})
        surface.erase(key);
    }else{
      for(const char *key:{"translation","rotation","scale","matrix"})
        require(!surface.contains(key),"generated skinned surface has an unexpected local transform");
    }
    if(direct){
      for(const char *key:{"mesh","skin","weights"})
        if(surface.contains(key))j["nodes"][target][key]=std::move(surface[key]);
      emitted.node=target;
    }else emitted.node=attach(target,std::move(surface));
    if (!objects[i].deformation || !objects[i].deformation->skin)continue;
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
