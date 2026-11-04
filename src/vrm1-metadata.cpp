// SPDX-License-Identifier: GPL-3.0-or-later
#include "vrm1-metadata.hpp"
#include "dome/avatar.hpp"
#include <algorithm>
#include <cmath>
#include <optional>
#include <set>
namespace dome::detail {
namespace {
using J = nlohmann::json;
void require(bool value, const char *what) {
  if (!value)
    throw std::invalid_argument(std::string("VRM1 metadata: ") + what);
}
void stop(const std::function<bool()> &cancelled) {
  if (cancelled && cancelled())
    throw std::runtime_error("VRM1 metadata mapping cancelled");
}
size_t index(const J &v, size_t count) {
  require(v.is_number_integer(), "reference must be integral");
  if (v.is_number_unsigned()) {
    auto n = v.get<uint64_t>();
    require(n < count, "reference is outside its source table");
    return size_t(n);
  }
  auto n = v.get<int64_t>();
  require(n >= 0 && uint64_t(n) < count,
          "reference is outside its source table");
  return size_t(n);
}
double number(const J &v, double low = -1e20, double high = 1e20) {
  require(v.is_number(), "numeric field required");
  const auto n = v.get<double>();
  require(std::isfinite(n) && n >= low && n <= high,
          "numeric field outside finite range");
  return n;
}
void vector(const J &v, size_t size) {
  require(v.is_array() && v.size() == size, "numeric vector dimension differs");
  for (const auto &x : v)
    number(x);
}
const std::string &text(const J &v) {
  require(v.is_string(), "string field required");
  return v.get_ref<const std::string &>();
}
void choice(const J &v, std::initializer_list<const char *> values) {
  auto &s = text(v);
  for (auto item : values)
    if (s == item)
      return;
  require(false, "enumeration value is unsupported");
}
void version(const J &v) {
  require(v.is_object() && text(v.at("specVersion")) == "1.0",
          "requires exact supported specification 1.0");
}
void scan(const J &value, const std::function<bool()> &cancelled,
          size_t &visits, size_t depth = 0) {
  require(depth <= 64 && ++visits <= 1000000, "structural budget exceeded");
  if ((visits & 1023) == 0)
    stop(cancelled);
  if (value.is_number_float())
    number(value);
  if (value.is_string())
    require(text(value).size() <= 1024 * 1024, "string budget exceeded");
  if (value.is_structured())
    for (const auto &child : value)
      scan(child, cancelled, visits, depth + 1);
}
std::vector<int> parents(const J &nodes) {
  std::vector<int> p(nodes.size(), -1);
  for (size_t n = 0; n < nodes.size(); ++n)
    if (nodes[n].contains("children")) {
      require(nodes[n]["children"].is_array(), "children must be an array");
      for (const auto &child : nodes[n]["children"]) {
        auto id = index(child, nodes.size());
        require(p[id] < 0 && id != n, "duplicate or self parent");
        p[id] = int(n);
      }
    }
  return p;
}
bool ancestor(const std::vector<int> &p, size_t head, size_t tail) {
  for (size_t depth = 0; depth <= 256; ++depth) {
    if (head == tail)
      return true;
    if (p.at(tail) < 0)
      return false;
    tail = size_t(p[tail]);
  }
  require(false, "ancestry depth or cycle exceeds bounds");
  return false;
}
int mapped(const J &value, const std::vector<int> &ids) {
  auto id = index(value, ids.size());
  require(ids[id] >= 0, "source identity has no emitted witness");
  return ids[id];
}
} // namespace
void validate_vrm1_metadata(const J &source,
                            const std::function<bool()> &cancelled,
                            bool standard_export) {
  stop(cancelled);
  const auto &ext = source.at("extensions");
  require(!ext.contains("VRM"), "mixed VRM0/VRM1 versions");
  const auto &vrm = ext.at("VRMC_vrm");
  version(vrm);
  const auto &used = source.at("extensionsUsed");
  require(used.is_array() &&
              std::find(used.begin(), used.end(), "VRMC_vrm") != used.end(),
          "root metadata must declare its actual V1 extension");
  const auto &nodes = source.at("nodes"), &meshes = source.at("meshes");
  require(nodes.is_array() && nodes.size() <= 100000 && meshes.is_array(),
          "invalid source graph tables");
  require(meshes.size() <= 65536, "source mesh count exceeds native bounds");
  for (const char *table : {"materials", "images", "textures"})
    if (source.contains(table))
      require(source.at(table).is_array() && source.at(table).size() <= 65536,
              "source resource table exceeds native bounds");
  const auto material_count =
      source.contains("materials") ? source.at("materials").size() : 0;
  const auto image_count =
      source.contains("images") ? source.at("images").size() : 0;
  size_t visits = 0;
  scan(vrm, cancelled, visits);
  auto p = parents(nodes);
  for (size_t n = 0; n < nodes.size(); ++n) {
    stop(cancelled);
    size_t at = n;
    for (size_t depth = 0; p[at] >= 0; ++depth) {
      require(depth < 256, "node hierarchy cycle or depth exceeds bounds");
      at = size_t(p[at]);
    }
  }
  const auto &meta = vrm.at("meta");
  require(meta.is_object() && !text(meta.at("name")).empty() &&
              !text(meta.at("licenseUrl")).empty(),
          "missing metadata identity/license");
  require(meta.at("authors").is_array() && !meta.at("authors").empty(),
          "missing author identities");
  for (const auto &author : meta.at("authors"))
    require(!text(author).empty(), "empty author identity");
  if (meta.contains("thumbnailImage"))
    index(meta.at("thumbnailImage"), image_count);
  for (const char *key :
       {"version", "copyrightInformation", "contactInformation"})
    if (meta.contains(key))
      text(meta.at(key));
  if (meta.contains("references")) {
    require(meta.at("references").is_array(),
            "metadata references must be an array");
    if (standard_export)
      require(!meta.at("references").empty(),
              "empty optional meta.references is invalid for standard V1 "
              "export; immutable source retained");
    for (const auto &reference : meta.at("references"))
      text(reference);
  }
  for (const char *key :
       {"allowExcessivelyViolentUsage", "allowExcessivelySexualUsage",
        "allowPoliticalOrReligiousUsage", "allowAntisocialOrHateUsage",
        "allowRedistribution"})
    if (meta.contains(key))
      require(meta.at(key).is_boolean(), "metadata permission must be boolean");
  if (meta.contains("avatarPermission"))
    choice(meta.at("avatarPermission"),
           {"onlyAuthor", "onlySeparatelyLicensedPerson", "everyone"});
  if (meta.contains("commercialUsage"))
    choice(meta.at("commercialUsage"),
           {"personalNonProfit", "personalProfit", "corporation"});
  if (meta.contains("creditNotation"))
    choice(meta.at("creditNotation"), {"required", "unnecessary"});
  if (meta.contains("modification"))
    choice(meta.at("modification"), {"prohibited", "allowModification",
                                     "allowModificationRedistribution"});
  const auto &bones = vrm.at("humanoid").at("humanBones");
  require(bones.is_object() && bones.size() <= human_roles().size(),
          "invalid humanoid role table");
  std::set<size_t> joints;
  for (const auto &bone : bones.items()) {
    require(std::any_of(human_roles().begin(), human_roles().end(),
                        [&](const auto &r) { return r.name == bone.key(); }),
            "unknown humanoid role");
    require(joints.insert(index(bone.value().at("node"), nodes.size())).second,
            "duplicate humanoid node");
  }
  for (const auto &role : human_roles())
    if (role.required)
      require(bones.contains(role.name), "missing required humanoid role");
  if (vrm.contains("firstPerson") &&
      vrm.at("firstPerson").contains("meshAnnotations")) {
    const auto &a = vrm.at("firstPerson").at("meshAnnotations");
    require(a.is_array(), "mesh annotations must be an array");
    for (const auto &annotation : a) {
      auto n = index(annotation.at("node"), nodes.size());
      require(nodes[n].contains("mesh"),
              "first-person annotation has no source mesh");
      choice(annotation.at("type"),
             {"auto", "both", "thirdPersonOnly", "firstPersonOnly"});
    }
  }
  if (vrm.contains("lookAt")) {
    const auto &look = vrm.at("lookAt");
    require(look.is_object(), "look-at must be an object");
    if (look.contains("type"))
      choice(look.at("type"), {"bone", "expression"});
    if (look.contains("offsetFromHeadBone"))
      vector(look.at("offsetFromHeadBone"), 3);
    for (const char *key :
         {"rangeMapHorizontalInner", "rangeMapHorizontalOuter",
          "rangeMapVerticalDown", "rangeMapVerticalUp"})
      if (look.contains(key)) {
        const auto &range = look.at(key);
        require(range.is_object(), "look-at range must be an object");
        if (range.contains("inputMaxValue"))
          number(range.at("inputMaxValue"), 0);
        if (range.contains("outputScale"))
          number(range.at("outputScale"), 0);
      }
  }
  if (vrm.contains("expressions")) {
    require(vrm.at("expressions").is_object(), "expressions must be an object");
    for (const char *table : {"preset", "custom"})
      if (vrm.at("expressions").contains(table)) {
        const auto &expressions = vrm.at("expressions").at(table);
        require(expressions.is_object(), "expression table must be an object");
        for (const auto &entry : expressions.items()) {
          stop(cancelled);
          const auto &expression = entry.value();
          require(expression.is_object(), "expression must be an object");
          if (expression.contains("isBinary"))
            require(expression.at("isBinary").is_boolean(),
                    "isBinary must be boolean");
          for (const char *mode :
               {"overrideBlink", "overrideMouth", "overrideLookAt"})
            if (expression.contains(mode))
              choice(expression.at(mode), {"none", "block", "blend"});
          for (const char *key : {"morphTargetBinds", "materialColorBinds",
                                  "textureTransformBinds"})
            if (expression.contains(key)) {
              const auto &binds = expression.at(key);
              require(binds.is_array() && !binds.empty(),
                      "expression binding must be a nonempty array");
              for (const auto &bind : binds) {
                stop(cancelled);
                if (std::string_view(key) == "morphTargetBinds") {
                  auto n = index(bind.at("node"), nodes.size());
                  require(nodes[n].contains("mesh"),
                          "morph binding has no mesh");
                  auto mesh = index(nodes[n].at("mesh"), meshes.size());
                  for (const auto &primitive : meshes.at(mesh).at("primitives"))
                    index(bind.at("index"), primitive.contains("targets")
                                                ? primitive.at("targets").size()
                                                : 0);
                  number(bind.at("weight"), 0, 1);
                } else {
                  index(bind.at("material"), material_count);
                  if (std::string_view(key) == "materialColorBinds") {
                    choice(bind.at("type"),
                           {"color", "emissionColor", "shadeColor",
                            "matcapColor", "rimColor", "outlineColor"});
                    vector(bind.at("targetValue"), 4);
                  } else {
                    if(bind.contains("scale"))vector(bind.at("scale"), 2);
                    if(bind.contains("offset"))vector(bind.at("offset"), 2);
                  }
                }
              }
            }
        }
      }
  }
  if (ext.contains("VRMC_springBone")) {
    const auto &spring = ext.at("VRMC_springBone");
    version(spring);
    scan(spring, cancelled, visits);
    const auto colliders = spring.value("colliders", J::array()),
               groups = spring.value("colliderGroups", J::array()),
               springs = spring.value("springs", J::array());
    require(colliders.is_array() && groups.is_array() && springs.is_array(),
            "spring tables must be arrays");
    for (const auto &c : colliders) {
      index(c.at("node"), nodes.size());
      const auto &shape = c.at("shape");
      require(shape.is_object() &&
                  (shape.contains("sphere") != shape.contains("capsule")),
              "collider needs exactly sphere or capsule");
      auto &value = shape.at(shape.contains("sphere") ? "sphere" : "capsule");
      vector(value.at("offset"), 3);
      number(value.at("radius"), 0);
      if (shape.contains("capsule"))
        vector(value.at("tail"), 3);
    }
    for (const auto &group : groups) {
      require(group.at("colliders").is_array() &&
                  !group.at("colliders").empty(),
              "collider group must be nonempty");
      for (const auto &collider : group.at("colliders"))
        index(collider, colliders.size());
    }
    for (const auto &s : springs) {
      stop(cancelled);
      const auto &chain = s.at("joints");
      require(chain.is_array() && !chain.empty(),
              "spring chain must be nonempty");
      std::optional<size_t> previous;
      for (const auto &joint : chain) {
        auto n = index(joint.at("node"), nodes.size());
        if (previous)
          require(n != *previous && ancestor(p, *previous, n),
                  "spring order must follow genuine source ancestry");
        previous = n;
        for (const char *field : {"hitRadius", "stiffness", "gravityPower"})
          if (joint.contains(field))
            number(joint.at(field), 0);
        if (joint.contains("dragForce"))
          number(joint.at("dragForce"), 0, 1);
        if (joint.contains("gravityDir"))
          vector(joint.at("gravityDir"), 3);
      }
      if (s.contains("center"))
        require(ancestor(p, index(s.at("center"), nodes.size()),
                         index(chain[0].at("node"), nodes.size())),
                "spring center is not a first-joint ancestor");
      if (s.contains("colliderGroups")) {
        require(s.at("colliderGroups").is_array(),
                "spring collider groups must be an array");
        for (const auto &group : s.at("colliderGroups"))
          index(group, groups.size());
      }
    }
  }
  if (ext.contains("VRMC_springBone")) {
    const auto springs = ext.at("VRMC_springBone").value("springs", J::array());
    std::vector<int> owners(nodes.size(), -1);
    size_t work = 0;
    auto tick = [&] {
      if (++work > 64000000)
        throw std::length_error("VRM1 spring ownership work bound exceeded");
      if ((work & 1023) == 0)
        stop(cancelled);
    };
    for (size_t chain = 0; chain < springs.size(); ++chain) {
      std::optional<size_t> previous;
      for (const auto &joint : springs[chain].at("joints")) {
        auto node = index(joint.at("node"), nodes.size());
        for (;;) {
          tick();
          require(
              owners[node] < 0 || owners[node] == int(chain),
              "overlapping spring chains include the same or skipped joint");
          owners[node] = int(chain);
          if (!previous || node == *previous)
            break;
          require(p[node] >= 0,
                  "spring path no longer reaches its previous joint");
          node = size_t(p[node]);
        }
        previous = index(joint.at("node"), nodes.size());
      }
    }
    for (size_t chain = 0; chain < springs.size(); ++chain)
      if (springs[chain].contains("center")) {
        auto node = index(springs[chain].at("center"), nodes.size());
        for (;;) {
          tick();
          require(owners[node] < 0 || owners[node] == int(chain),
                  "spring center depends on a different spring chain");
          if (p[node] < 0)
            break;
          node = size_t(p[node]);
        }
      }
  }
  std::vector<uint8_t> aim(nodes.size());
  std::vector<int> constraints(nodes.size(), -1);
  for (size_t n = 0; n < nodes.size(); ++n)
    if (nodes[n].contains("extensions") &&
        nodes[n].at("extensions").contains("VRMC_node_constraint")) {
      const auto &constraint =
          nodes[n].at("extensions").at("VRMC_node_constraint");
      version(constraint);
      scan(constraint, cancelled, visits);
      const auto &kind = constraint.at("constraint");
      require(kind.is_object(), "constraint must be an object");
      size_t count = 0;
      for (const char *mode : {"roll", "aim", "rotation"})
        if (kind.contains(mode)) {
          ++count;
          const auto &value = kind.at(mode);
          auto source = index(value.at("source"), nodes.size());
          require(source != n, "constraint cannot reference itself");
          constraints[n] = int(source);
          if (value.contains("weight"))
            number(value.at("weight"), 0, 1);
          if (std::string_view(mode) == "roll")
            choice(value.at("rollAxis"), {"X", "Y", "Z"});
          if (std::string_view(mode) == "aim") {
            aim[n] = 1;
            choice(value.at("aimAxis"),
                   {"PositiveX", "NegativeX", "PositiveY", "NegativeY",
                    "PositiveZ", "NegativeZ"});
          }
        }
      require(count == 1, "constraint must contain exactly one supported kind");
    }
  std::vector<uint8_t> states(nodes.size());
  size_t dependency_work = 0;
  auto visit = [&](auto &&self, size_t n, size_t depth) -> void {
    stop(cancelled);
    require(depth <= 256, "constraint dependency depth exceeds bounds");
    if (states[n] == 2)
      return;
    require(states[n] == 0, "constraint dependency cycle");
    states[n] = 1;
    if (constraints[n] >= 0) {
      self(self, size_t(constraints[n]), depth + 1);
      if (aim[n])
        for (int start : {constraints[n], p[n]})
          for (int at = start; at >= 0; at = p[size_t(at)]) {
            if (++dependency_work > 64000000)
              throw std::length_error("VRM1 constraint work bound exceeded");
            if (constraints[size_t(at)] >= 0)
              self(self, size_t(at), depth + 1);
          }
    }
    states[n] = 2;
  };
  for (size_t n = 0; n < nodes.size(); ++n)
    visit(visit, n, 0);
  stop(cancelled);
}
J remap_vrm1_metadata(const J &source, const Vrm1IdentityMap &map,
                      const std::function<bool()> &cancelled) {
  validate_vrm1_metadata(source, cancelled, true);
  J result = J::object();
  auto vrm = source.at("extensions").at("VRMC_vrm");
  if (vrm.at("meta").contains("thumbnailImage"))
    vrm["meta"]["thumbnailImage"] =
        mapped(vrm.at("meta").at("thumbnailImage"), map.images);
  for (auto &bone : vrm["humanoid"]["humanBones"])
    bone["node"] = mapped(bone.at("node"), map.nodes);
  if (vrm.contains("firstPerson") &&
      vrm.at("firstPerson").contains("meshAnnotations")) {
    J annotations = J::array();
    for (const auto &a : vrm["firstPerson"]["meshAnnotations"]) {
      const auto n = index(a.at("node"), map.surfaces.size());
      require(!map.surfaces[n].empty(),
              "annotation source node has no emitted surfaces");
      for (const auto &surface : map.surfaces[n]) {
        auto current = a;
        current["node"] = surface.node;
        annotations.push_back(std::move(current));
      }
    }
    vrm["firstPerson"]["meshAnnotations"] = std::move(annotations);
  }
  if (vrm.contains("expressions"))
    for (const char *table : {"preset", "custom"})
      if (vrm["expressions"].contains(table))
        for (auto &expression : vrm["expressions"][table])
          for (const char *key : {"morphTargetBinds", "materialColorBinds",
                                  "textureTransformBinds"})
            if (expression.contains(key)) {
              J binds = J::array();
              for (const auto &bind : expression.at(key)) {
                stop(cancelled);
                if (std::string_view(key) == "morphTargetBinds") {
                  auto source_node =
                      index(bind.at("node"), map.surfaces.size());
                  require(!map.surfaces[source_node].empty(),
                          "expression source node has no emitted surfaces");
                  for (const auto &surface : map.surfaces[source_node]) {
                    auto current = bind;
                    current["node"] = surface.node;
                    current["index"] =
                        mapped(bind.at("index"), surface.morph_targets);
                    binds.push_back(std::move(current));
                  }
                } else {
                  auto material =
                      index(bind.at("material"), map.materials.size());
                  require(!map.materials[material].empty(),
                          "expression material has no emitted identity");
                  for (auto target : map.materials[material]) {
                    auto current = bind;
                    current["material"] = target;
                    binds.push_back(std::move(current));
                  }
                }
              }
              expression[key] = std::move(binds);
            }
  result["VRMC_vrm"] = std::move(vrm);
  if (source.at("extensions").contains("VRMC_springBone")) {
    auto spring = source.at("extensions").at("VRMC_springBone");
    if (spring.contains("colliders"))
      for (auto &collider : spring["colliders"])
        collider["node"] = mapped(collider.at("node"), map.nodes);
    if (spring.contains("springs"))
      for (auto &chain : spring["springs"]) {
        if (chain.contains("center"))
          chain["center"] = mapped(chain.at("center"), map.nodes);
        for (auto &joint : chain["joints"])
          joint["node"] = mapped(joint.at("node"), map.nodes);
      }
    result["VRMC_springBone"] = std::move(spring);
  }
  return result;
}
void remap_vrm1_constraints(J &output, const J &source,
                            const Vrm1IdentityMap &map,
                            const std::function<bool()> &cancelled) {
  for (size_t n = 0; n < source.at("nodes").size(); ++n) {
    stop(cancelled);
    const auto &node = source.at("nodes")[n];
    if (!node.contains("extensions") ||
        !node.at("extensions").contains("VRMC_node_constraint"))
      continue;
    auto current = node.at("extensions").at("VRMC_node_constraint");
    for (const char *kind : {"roll", "aim", "rotation"})
      if (current["constraint"].contains(kind)) {
        auto &value = current["constraint"][kind];
        value["source"] = mapped(value.at("source"), map.nodes);
      }
    require(n < map.nodes.size() && map.nodes[n] >= 0,
            "constraint destination has no emitted identity");
    output["nodes"][map.nodes[n]]["extensions"]["VRMC_node_constraint"] =
        std::move(current);
  }
}
} // namespace dome::detail
