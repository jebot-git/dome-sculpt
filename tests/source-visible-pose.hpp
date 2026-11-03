// SPDX-License-Identifier: GPL-3.0-or-later
// Independent double point consumer and unchanged original5pose cohort.
#pragma once
#include "dome/deformation.hpp"
#include <numbers>
namespace source_visible_test {
using namespace dome;
using AuditPoint = std::array<double, 3>;
double distance(AuditPoint a, AuditPoint b) {
  return std::hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
}
// Independent point consumer: explicit double morph accumulation and LBS.
// Joint matrices come from the existing native hierarchy; no preview/deform
// vertex evaluator is reused. Independent GLB/hierarchy/render proof is later.
std::vector<AuditPoint> points(const Mesh &m, const DeformationState &d) {
  auto matrices = d.skeleton->skin_matrices(d.pose);
  std::vector<AuditPoint> out(m.positions.size());
  for (size_t v = 0; v < m.positions.size(); ++v) {
    AuditPoint p{m.positions[v].x, m.positions[v].y, m.positions[v].z};
    if (d.morphs && !d.morph_weights.empty())
      for (size_t t = 0; t < d.morphs->targets.size(); ++t) {
        const auto &q = d.morphs->targets[t].delta[v];
        double w = d.morph_weights[t];
        p[0] += w * q.x;
        p[1] += w * q.y;
        p[2] += w * q.z;
      }
    for (auto influence : d.skin->vertices[v]) {
      const auto &a = matrices[influence.joint];
      for (int k = 0; k < 3; ++k)
        out[v][k] += double(influence.weight) * (a[k] * p[0] + a[4 + k] * p[1] +
                                                 a[8 + k] * p[2] + a[12 + k]);
    }
  }
  return out;
}
void pose(DeformationState &d, int which) {
  d.pose.clear();
  for (const auto &b : d.skeleton->bones)
    d.pose.push_back(b.rest);
  auto rotate = [&](const char *role, Vec3 axis, double degrees) {
    auto at = d.humanoid->joints.find(role);
    if (at == d.humanoid->joints.end())
      return;
    double half = degrees * std::numbers::pi / 360;
    Quat q{float(std::cos(half)), float(axis.x * std::sin(half)),
           float(axis.y * std::sin(half)), float(axis.z * std::sin(half))};
    d.pose[at->second].rotation = q * d.pose[at->second].rotation;
  };
  if (which == 0) {
    rotate("leftLowerArm", {0, 0, 1}, 20);
    rotate("rightUpperLeg", {0, 0, 1}, 20);
  }
  if (which == 1) {
    rotate("head", {0, 0, 1}, 30);
    rotate("neck", {0, 1, 0}, -15);
  }
  if (which == 2 || which == 4) {
    rotate("leftHand", {0, 0, 1}, 20);
    rotate("leftMiddleIntermediate", {1, 0, 0}, 45);
    rotate("rightFoot", {1, 0, 0}, 20);
  }
  if (which == 3) {
    rotate("spine", {0, 0, 1}, 15);
    rotate("hips", {0, 1, 0}, 10);
    rotate("leftShoulder", {0, 0, 1}, 20);
  }
  if (which == 4 && d.morphs && !d.morphs->targets.empty()) {
    if (d.morph_weights.empty())
      d.morph_weights.assign(d.morphs->targets.size(), 0);
    d.morph_weights[0] = .35f;
  }
}
} // namespace source_visible_test
