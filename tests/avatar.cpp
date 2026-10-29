// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/avatar.hpp"
#include <cassert>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
void rejects(auto f) {
  bool failed = false;
  try {
    f();
  } catch (const std::exception &e) {
    failed = true;
    std::cout << e.what() << '\n';
  }
  assert(failed);
}
HumanLandmarks landmarks() {
  HumanLandmarks l;
  l.points = {
      {"hips", {0, 1, 0}}, {"spine", {0, 1.2f, 0}}, {"head", {0, 1.7f, 0}}};
  for (std::string s : {"left", "right"}) {
    float x = s == "left" ? 1 : -1;
    l.points[s + "UpperLeg"] = {x * .15f, 1, 0};
    l.points[s + "LowerLeg"] = {x * .15f, .55f, 0};
    l.points[s + "Foot"] = {x * .15f, .1f, 0};
    l.points[s + "UpperArm"] = {x * .25f, 1.5f, 0};
    l.points[s + "LowerArm"] = {x * .55f, 1.4f, 0};
    l.points[s + "Hand"] = {x * .8f, 1.3f, 0};
  }
  return l;
}
Mesh surface(const HumanLandmarks &l) {
  Mesh m;
  for (auto [n, p] : l.points) {
    uint32_t i = m.positions.size();
    m.positions.push_back(p + Vec3{-.02f, 0, 0});
    m.positions.push_back(p + Vec3{.02f, 0, 0});
    m.positions.push_back(p + Vec3{0, .02f, 0});
    m.indices.insert(m.indices.end(), {i, i + 1, i + 2});
  }
  m.normals.assign(m.positions.size(), {0, 0, 1});
  m.uv.resize(m.positions.size());
  return m;
}
int main() {
  assert(human_roles().size() == 55);
  auto l = landmarks();
  auto mesh = surface(l);
  auto fit = fit_humanoid(mesh, l);
  assert(fit.skeleton.bones.size() == 15);
  fit.mapping.validate(fit.skeleton);
  auto segments = fit.skeleton.segments();
  for (auto [n, i] : fit.mapping.joints)
    assert(length(segments[i][0] - l.points.at(n)) < 1e-5f);
  auto mirror = fit.mapping.mirror(fit.skeleton);
  for (size_t i = 0; i < mirror.size(); ++i)
    assert(mirror[mirror[i]] == i);
  assert(HumanoidMapping::decode(fit.mapping.encode(), fit.skeleton).joints ==
         fit.mapping.joints);
  assert(HumanLandmarks::decode(l.encode()).points.size() == 15);
  auto broken = fit.mapping;
  broken.joints["head"] = broken.joints["spine"];
  rejects([&] { broken.validate(fit.skeleton); });
  broken = fit.mapping;
  broken.joints.erase("head");
  rejects([&] { broken.validate(fit.skeleton); });
  broken = fit.mapping;
  broken.joints.erase("leftHand");
  rejects([&] { broken.mirror(fit.skeleton); });
  auto skeleton = fit.skeleton;
  skeleton.bones[fit.mapping.joints.at("leftFoot")].parent =
      int(fit.mapping.joints.at("rightLowerLeg"));
  rejects([&] { fit.mapping.validate(skeleton); });
  skeleton = fit.skeleton;
  skeleton.bones[0].rest.scale.x = -1;
  rejects([&] { fit.mapping.validate(skeleton); });
  // Optional roles skip absent torso roles, but upperChest requires chest.
  auto optional = l;
  optional.points["neck"] = {0, 1.6f, 0};
  optional.points["chest"] = {0, 1.4f, 0};
  auto optional_fit = fit_humanoid(surface(optional), optional);
  optional_fit.mapping.validate(optional_fit.skeleton);
  optional.points.erase("chest");
  optional.points["upperChest"] = {0, 1.45f, 0};
  rejects([&] { fit_humanoid(surface(optional), optional); });
  // Additional non-humanoid helper between lower leg and foot remains legal.
  skeleton = fit.skeleton;
  uint32_t foot = fit.mapping.joints.at("leftFoot");
  Bone helper;
  helper.name = "helper";
  helper.parent = skeleton.bones[foot].parent;
  helper.rest = skeleton.bones[foot].rest;
  skeleton.bones[foot].rest = {};
  skeleton.bones[foot].parent = int(skeleton.bones.size());
  skeleton.bones.push_back(helper);
  fit.mapping.validate(skeleton);
  auto rotated = l;
  Quat q = arcball({0, 1, 0}, normalized({.3f, 1, .2f}));
  rotated.left = q.rotate(l.left);
  rotated.up = q.rotate(l.up);
  rotated.front = q.rotate(l.front);
  for (auto &[n, p] : rotated.points)
    p = q.rotate(p) + Vec3{3, 2, 1};
  auto rf = fit_humanoid(surface(rotated), rotated);
  auto rs = rf.skeleton.segments();
  for (auto [n, i] : rf.mapping.joints)
    assert(length(rs[i][0] - rotated.points.at(n)) < 1e-5f);
  auto bad = l;
  bad.points["leftHand"] = bad.points.at("leftLowerArm");
  rejects([&] { fit_humanoid(surface(bad), bad); });
  bad = l;
  bad.points["leftHand"].x = -.8f;
  rejects([&] { fit_humanoid(surface(bad), bad); });
  bad = l;
  bad.points["head"].z = 10;
  rejects([&] { fit_humanoid(mesh, bad); });
  bad = l;
  bad.front = {1, 0, 0};
  rejects([&] { fit_humanoid(mesh, bad); });
  rejects([&] { fit_humanoid(mesh, l, {}, [] { return true; }); });
  HumanFitOptions options;
  options.maximum_work = 1;
  rejects([&] { fit_humanoid(mesh, l, options); });
  auto json = fit.mapping.encode();
  json["joints"]["hips"] = -1;
  rejects([&] { HumanoidMapping::decode(json, fit.skeleton); });
  json = l.encode();
  json["version"] = 2;
  rejects([&] { HumanLandmarks::decode(json); });
  // Only unneeded branches disappear. Helpers/weighted unmapped joints survive.
  skeleton = fit.skeleton;
  Bone unused;
  unused.name = "unused";
  unused.parent = 0;
  skeleton.bones.push_back(unused);
  Bone used = unused;
  used.name = "weighted";
  skeleton.bones.push_back(used);
  SkinWeights w;
  w.vertices = {{{fit.mapping.joints.at("leftHand"), .3f},
                 {uint32_t(skeleton.bones.size() - 1), .7f}}};
  auto clean = compact_avatar(skeleton, w, fit.mapping);
  assert(clean.skeleton.bones.size() == skeleton.bones.size() - 1);
  assert(clean.old_to_new[skeleton.bones.size() - 2] == UINT32_MAX);
  assert(clean.weights.vertices[0][0].weight == .3f);
  auto before = skeleton.world(), after = clean.skeleton.world();
  for (size_t i = 0; i < before.size(); ++i)
    if (clean.old_to_new[i] != UINT32_MAX)
      assert(before[i] == after[clean.old_to_new[i]]);
  MorphSet morph(mesh);
  std::vector<Vec3> edited = mesh.positions;
  edited[0].z = .1f;
  morph.set_target(mesh, "mouth", edited);
  edited[0].z = .2f;
  morph.set_target(mesh, "blink", edited);
  AvatarExpressions expr;
  for (std::string v : {"aa", "ih", "ou", "ee", "oh"})
    expr.visemes[v] = {{"mouth", 1}};
  expr.validate(morph, true);
  expr.rename_shape(morph, "mouth", "vowel");
  for (auto [n, b] : expr.visemes)
    assert(b[0].shape == "vowel");
  auto decoded = AvatarExpressions::decode(expr.encode(), morph);
  decoded.validate(morph, true);
  rejects([&] { expr.rename_shape(morph, "vowel", "blink"); });
  assert(morph.targets[0].name == "vowel");
  auto invalid = expr;
  invalid.visemes["aa"].push_back({"vowel", 1});
  rejects([&] { invalid.validate(morph); });
  invalid = expr;
  invalid.visemes["aa"][0].shape = "absent";
  rejects([&] { invalid.validate(morph); });
  invalid = expr;
  invalid.visemes["aa"][0].weight = 2;
  rejects([&] { invalid.validate(morph); });
  std::cout << "avatar fixtures passed\n";
}
