// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/deformation.hpp"
#include <nlohmann/json.hpp>
namespace dome {
namespace {
constexpr size_t maximum_entries = 8'000'000;
[[noreturn]] void fail(const char *message) {
  throw std::invalid_argument(message);
}
void source_valid(const Mesh &mesh, uint64_t source) {
  if (mesh.positions.empty() || mesh.positions.size() > 2'000'000 ||
      mesh.indices.size() > 6'000'000)
    fail("Deformation rest mesh exceeds geometry bounds");
  mesh.validate();
  if (source != morph_topology_id(mesh))
    fail("Deformation source identity differs from rest geometry/topology");
}
} // namespace
DeformationState::DeformationState(const Mesh &mesh)
    : source(morph_topology_id(mesh)) {}
size_t DeformationState::resource_entries() const {
  size_t count = pose.size() + morph_weights.size();
  if (morphs) {
    if (morphs->targets.size() > 128)
      fail("Deformation morph target limit exceeded");
    for (const auto &target : morphs->targets) {
      if (target.delta.size() >
          maximum_entries - std::min(count, maximum_entries))
        fail("Deformation resource budget exceeded");
      count += target.delta.size();
    }
  }
  if (skin)
    for (const auto &vertex : skin->vertices) {
      if (vertex.size() > maximum_entries - std::min(count, maximum_entries))
        fail("Deformation resource budget exceeded");
      count += vertex.size();
    }
  if (count > maximum_entries)
    fail("Deformation resource budget exceeded");
  return count;
}
void DeformationState::validate(const Mesh &rest) const {
  source_valid(rest, source);
  resource_entries();
  if (!skeleton && (skin || humanoid || !pose.empty()))
    fail("Skin, humanoid mapping or pose requires a skeleton");
  if (skeleton) {
    skeleton->validate();
    if (!pose.empty())
      skeleton->world(pose);
    if (skin)
      skin->validate(rest.positions.size(), skeleton->bones.size());
    if (humanoid)
      humanoid->validate(*skeleton, false);
  }
  if (!morphs && (!morph_weights.empty() || expressions))
    fail("Morph values or expressions require morph targets");
  if (morphs) {
    morphs->validate(rest);
    if (!morph_weights.empty() &&
        morph_weights.size() != morphs->targets.size())
      fail("Morph preview weight dimensions differ from target count");
    for (float value : morph_weights)
      if (!std::isfinite(value) || value < -1 || value > 1)
        fail("Morph preview weights must be finite within [-1, 1]");
    if (expressions)
      expressions->validate(*morphs);
  }
  if (landmarks) {
    // Decode imposes name/count/coordinate bounds even on manually edited data.
    auto bounded = HumanLandmarks::decode(landmarks->encode());
    for (Vec3 axis : {bounded.left, bounded.up, bounded.front})
      if (std::abs(length(axis) - 1) > 1e-4f)
        fail("Landmark body frame must use unit axes");
    if (length(cross(bounded.left, bounded.up) - bounded.front) > 1e-4f)
      fail("Landmark body frame must be orthonormal and right-handed");
  }
}
Mesh DeformationState::preview(const Mesh &rest) const {
  validate(rest);
  Mesh result = rest;
  if (morphs && !morph_weights.empty())
    result = morphs->evaluate(rest, morph_weights);
  if (skeleton && skin && (!pose.empty() || skeleton->inverse_bind_override))
    result = deform_mesh(result, *skin, *skeleton, pose);
  return result;
}
nlohmann::json DeformationState::encode(const Mesh &rest) const {
  validate(rest);
  auto json = nlohmann::json{{"version", 1},
                             {"source", source},
                             {"pose", nlohmann::json::array()},
                             {"morph_weights", morph_weights}};
  for (const auto &t : pose)
    json["pose"].push_back(
        {{"translation", {t.translation.x, t.translation.y, t.translation.z}},
         {"rotation", {t.rotation.w, t.rotation.x, t.rotation.y, t.rotation.z}},
         {"scale", {t.scale.x, t.scale.y, t.scale.z}}});
  json["skeleton"] = skeleton ? skeleton->encode() : nlohmann::json(nullptr);
  json["skin"] = skin ? skin->encode() : nlohmann::json(nullptr);
  json["morphs"] = morphs ? morphs->encode() : nlohmann::json(nullptr);
  json["humanoid"] = humanoid ? humanoid->encode() : nlohmann::json(nullptr);
  json["landmarks"] = landmarks ? landmarks->encode() : nlohmann::json(nullptr);
  json["expressions"] =
      expressions ? expressions->encode() : nlohmann::json(nullptr);
  return json;
}
DeformationState DeformationState::decode(const nlohmann::json &j,
                                          const Mesh &rest) {
  if (!j.is_object() || j.at("version") != 1 ||
      (!j.at("source").is_number_unsigned() &&
       (!j.at("source").is_number_integer() ||
        j.at("source").get<int64_t>() < 0)))
    fail("Invalid deformation metadata version/source identity");
  DeformationState result;
  result.source = j.at("source").get<uint64_t>();
  source_valid(rest, result.source);
  const auto &pose = j.at("pose");
  const auto &values = j.at("morph_weights");
  if (!pose.is_array() || pose.size() > 256 || !values.is_array() ||
      values.size() > 128)
    fail("Deformation preview resource limits exceeded");
  // Aggregate dimensions before allocating authoring arrays.
  size_t entries = pose.size() + values.size();
  const auto &skin = j.at("skin"), &morphs = j.at("morphs");
  if (!skin.is_null()) {
    const auto &vertices = skin.at("vertices");
    if (!vertices.is_array() || vertices.size() != rest.positions.size())
      fail("Skin dimensions differ from rest geometry");
    for (const auto &v : vertices) {
      if (!v.is_array() || v.size() > 32)
        fail("Skin influence resource limit exceeded");
      entries += v.size();
      if (entries > maximum_entries)
        fail("Deformation resource budget exceeded");
    }
  }
  if (!morphs.is_null()) {
    const auto &targets = morphs.at("targets");
    if (!targets.is_array() || targets.size() > 128)
      fail("Morph target resource limit exceeded");
    for (const auto &target : targets) {
      const auto &delta = target.at("delta");
      if (!delta.is_array() || delta.size() != rest.positions.size())
        fail("Morph dimensions differ from rest geometry");
      entries += delta.size();
      if (entries > maximum_entries)
        fail("Deformation resource budget exceeded");
    }
  }
  if (!j.at("skeleton").is_null())
    result.skeleton = Skeleton::decode(j.at("skeleton"));
  if (!skin.is_null()) {
    if (!result.skeleton)
      fail("Skin requires a skeleton");
    result.skin = SkinWeights::decode(skin, rest.positions.size(),
                                      result.skeleton->bones.size());
  }
  if (!morphs.is_null())
    result.morphs = MorphSet::decode(morphs, rest);
  if (!j.at("humanoid").is_null()) {
    if (!result.skeleton)
      fail("Humanoid mapping requires a skeleton");
    result.humanoid =
        HumanoidMapping::decode(j.at("humanoid"), *result.skeleton, false);
  }
  if (!j.at("landmarks").is_null())
    result.landmarks = HumanLandmarks::decode(j.at("landmarks"));
  if (!j.at("expressions").is_null()) {
    if (!result.morphs)
      fail("Expressions require morph targets");
    result.expressions =
        AvatarExpressions::decode(j.at("expressions"), *result.morphs);
  }
  for (const auto &v : pose) {
    auto p = v.at("translation").get<std::array<float, 3>>();
    auto q = v.at("rotation").get<std::array<float, 4>>();
    auto s = v.at("scale").get<std::array<float, 3>>();
    result.pose.push_back(
        {{p[0], p[1], p[2]}, {q[0], q[1], q[2], q[3]}, {s[0], s[1], s[2]}});
  }
  result.morph_weights = values.get<std::vector<float>>();
  result.validate(rest);
  return result;
}
} // namespace dome
