// SPDX-License-Identifier: GPL-3.0-or-later
// clang-format off
#include "../src/heat-binding.cpp"
#include "../src/anatomical-continuation.hpp"
#include "dome/creation.hpp"
// clang-format on
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <numbers>
using namespace dome;
using namespace dome::detail;
namespace {
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
} // namespace
int main(int argc, char **argv) try {
  const std::string gender = argc > 1 ? argv[1] : "male";
  const bool full = argc > 2 && std::string(argv[2]) == "full";
  if (gender != "male" && gender != "female")
    throw std::invalid_argument("Unknown actual cohort");
  Scene scene;
  append_humanoid(scene,
                  gender == "male"
                      ? humanoid_baseline_defaults("vroid-beta-male")
                      : HumanoidControls{},
                  "Actual continuation", {}, 64);
  const auto original = scene.encode();
  double lo = INFINITY, hi = -INFINITY;
  uint64_t group = 0;
  for (const auto &o : scene.objects())
    if (o.painted) {
      group = o.group;
      for (auto p : o.painted->mesh().positions) {
        lo = std::min(lo, double(p.y));
        hi = std::max(hi, double(p.y));
      }
    }
  std::cout << std::setprecision(12)
            << "POLICY whole-group-screened-surface-contact-graph "
               "artist_magnitudes_in_energy=0 source_positive_intent=1 volume_claim=0 "
               "work_cap=500000000 mean_limit="
            << (hi - lo) * .005 << " max_limit=" << (hi - lo) * .02 << '\n'
            << std::flush;
  auto fields = anatomical_continuation(scene, group);
  const auto &r = fields.report;
  std::cout << "CERTIFIED rows=" << r.rows
            << " controls=" << fields.joints.size() << " samples=" << r.samples
            << " contacts=" << r.contacts << " iterations=" << r.iterations
            << " residual=" << r.residual << " minimum=" << r.minimum
            << " partition=" << r.partition << " work=" << r.work
            << " bytes=" << r.bytes << '\n'
            << std::flush;
  size_t parts = 0, quality = 0, consumer = 0, seamfail = 0;
  const size_t labels = fields.joints.size();
  for (const auto &part : fields.parts) {
    const auto &o = *part.object;
    bool selected = full || (o.appearance->source_mesh == 1 &&
                             (o.appearance->source_primitive == 0 ||
                              o.appearance->source_primitive == 4));
    if (!selected)
      continue;
    ++parts;
    SkinWeights weights;
    for (auto row : part.raw_to_row) {
      VertexWeights values;
      for (size_t j = 0; j < labels; ++j)
        if (fields.fields[row * labels + j] > 0)
          values.push_back(
              {fields.joints[j], float(fields.fields[row * labels + j])});
      // Existing explicit finite storage policy, after independent full-field
      // certificates. Its effect is included in every deformation comparison.
      weights.vertices.push_back(normalize_weights(
          std::move(values), o.deformation->skeleton->bones.size()));
    }
    weights.validate(o.painted->mesh().positions.size(),
                     o.deformation->skeleton->bones.size());
    HeatBindingOptions options;
    auto seams = capture_heat_seams(o, options);
    for (int scenario = 0; scenario < 5; ++scenario) {
      auto reference = *o.deformation, next = reference;
      next.skin = weights;
      pose(reference, scenario);
      pose(next, scenario);
      auto a = points(o.painted->mesh(), reference),
           b = points(o.painted->mesh(), next);
      auto native = next.preview(o.painted->mesh());
      double mean = 0, maximum = 0, delta = 0, full_mean = 0, full_max = 0;
      auto matrices = next.skeleton->skin_matrices(next.pose);
      size_t worst = 0;
      bool exact = true;
      auto groups = seams.groups(o);
      for (size_t i = 0; i < a.size(); ++i) {
        const auto &mesh = o.painted->mesh();
        AuditPoint rest{mesh.positions[i].x, mesh.positions[i].y,
                        mesh.positions[i].z},
            full_point{};
        if (next.morphs && !next.morph_weights.empty())
          for (size_t t = 0; t < next.morphs->targets.size(); ++t) {
            auto d = next.morphs->targets[t].delta[i];
            rest[0] += double(next.morph_weights[t]) * d.x;
            rest[1] += double(next.morph_weights[t]) * d.y;
            rest[2] += double(next.morph_weights[t]) * d.z;
          }
        for (size_t j = 0; j < labels; ++j) {
          const auto &m = matrices[fields.joints[j]];
          double w = fields.fields[part.raw_to_row[i] * labels + j];
          for (size_t k = 0; k < 3; ++k)
            full_point[k] += w * (m[k] * rest[0] + m[4 + k] * rest[1] +
                                  m[8 + k] * rest[2] + m[12 + k]);
        }
        double full_error = distance(a[i], full_point);
        full_mean += full_error;
        full_max = std::max(full_max, full_error);
        double error = distance(a[i], b[i]);
        mean += error;
        if (error > maximum) {
          maximum = error;
          worst = i;
        }
        delta = std::max(
            delta, distance(b[i], {native.positions[i].x, native.positions[i].y,
                                   native.positions[i].z}));
        exact &= b[i] == b[groups[i]];
      }
      mean /= a.size();
      full_mean /= a.size();
      bool good = mean <= (hi - lo) * .005 && maximum <= (hi - lo) * .02;
      quality += !good;
      consumer += delta > (hi - lo) * 1e-5;
      seamfail += !exact;
      std::cout << "POSE part=" << o.id << " name=" << o.name
                << " pose=" << scenario << " mean=" << mean
                << " max=" << maximum << " full_mean=" << full_mean
                << " full_max=" << full_max << " worst=" << worst
                << " quality=" << good << " seam=" << exact
                << " native=" << delta << '\n'
                << std::flush;
    }
  }
  bool exact = scene.encode() == original;
  std::cout << "SUMMARY parts=" << parts << " quality_failures=" << quality
            << " seam_failures=" << seamfail
            << " consumer_failures=" << consumer
            << " original_scene_exact=" << exact << '\n';
  return quality || seamfail || consumer || !exact ||
         (full ? parts != 19 : parts != 2);
} catch (const std::exception &e) {
  std::cerr << "REFUSED " << e.what() << '\n';
  return 2;
}
