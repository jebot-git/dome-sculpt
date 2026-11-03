// SPDX-License-Identifier: GPL-3.0-or-later
#include "heat-binding.hpp"
#include "dome/creation.hpp"
#include "heat-source.hpp"
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
using namespace dome::detail;
namespace {
Mesh sheet() {
  Mesh m;
  for (int y = 0; y < 5; ++y)
    for (int x = 0; x < 5; ++x) {
      m.positions.push_back({-.1f + x * .05f, y * .05f, 0});
      m.normals.push_back({0, 0, 1});
      m.uv.push_back({x * .25f, y * .25f});
    }
  for (int y = 0; y < 4; ++y)
    for (int x = 0; x < 4; ++x) {
      uint32_t a = y * 5 + x, b = a + 1, c = a + 6, d = a + 5;
      m.indices.insert(m.indices.end(), {a, b, c, a, c, d});
    }
  return m;
}
Skeleton bones() {
  Skeleton s;
  Bone a;
  a.name = "Left";
  a.rest.translation = {-.075f, 0, -.05f};
  a.length = .2f;
  auto b = a;
  b.name = "Right";
  b.rest.translation.x = .075f;
  s.bones = {a, b};
  return s;
}
float weight(const VertexWeights &w, uint32_t j) {
  for (auto v : w)
    if (v.joint == j)
      return v.weight;
  return 0;
}
void check(bool b, const char *message) {
  if (!b)
    throw std::runtime_error(message);
  std::cout << "PASS " << message << '\n';
}
// Independent dense FEM assembly from planar barycentric gradients, followed
// by pivoted long-double elimination; no private CSR/PCG data is consumed.
std::vector<long double> oracle(const Mesh &m, const Skeleton &s,
                                bool positive = false) {
  size_t n = m.positions.size();
  std::vector<std::vector<long double>> a(n, std::vector<long double>(n));
  std::vector<long double> mass(n), rhs(n);
  for (size_t t = 0; t < m.indices.size(); t += 3) {
    std::array<uint32_t, 3> v{m.indices[t], m.indices[t + 1], m.indices[t + 2]};
    auto p = m.positions[v[0]], q = m.positions[v[1]], r = m.positions[v[2]];
    long double det = (static_cast<long double>(q.x) - p.x) *
                          (static_cast<long double>(r.y) - p.y) -
                      (static_cast<long double>(q.y) - p.y) *
                          (static_cast<long double>(r.x) - p.x),
                area = std::abs(det) * .5L;
    std::array<long double, 3> gx{(q.y - r.y) / det, (r.y - p.y) / det,
                                  (p.y - q.y) / det},
        gy{(r.x - q.x) / det, (p.x - r.x) / det, (q.x - p.x) / det};
    for (int i = 0; i < 3; ++i) {
      mass[v[i]] += area / 3;
      for (int j = 0; j < 3; ++j)
        a[v[i]][v[j]] += area * (gx[i] * gx[j] + gy[i] * gy[j]);
    }
  }
  if (positive)
    for (size_t i = 0; i < n; ++i) {
      long double diagonal = 0;
      for (size_t j = 0; j < n; ++j)
        if (i != j) {
          if (a[i][j] > 0)
            a[i][j] = 0;
          diagonal -= a[i][j];
        }
      a[i][i] = diagonal;
    }
  for (size_t i = 0; i < n; ++i) {
    std::array<long double, 2> d;
    auto p = m.positions[i];
    for (int j = 0; j < 2; ++j) {
      auto root = s.bones[j].rest.translation;
      long double x = static_cast<long double>(p.x) - root.x,
                  y = static_cast<long double>(p.y) -
                      std::clamp(double(p.y), double(root.y),
                                 double(root.y) + s.bones[j].length),
                  z = static_cast<long double>(p.z) - root.z;
      long double distance = std::sqrt(x * x + y * y + z * z),
                  cosine = z / distance;
      d[j] = distance / (.5L * (cosine + 1.001L));
    }
    auto nearest = std::min(d[0], d[1]);
    int ties =
        int(d[0] <= nearest * (1 + 1e-6L)) + int(d[1] <= nearest * (1 + 1e-6L));
    long double h = mass[i] * ties / (nearest * nearest);
    a[i][i] += h;
    rhs[i] = d[0] <= nearest * (1 + 1e-6L) ? h / ties : 0;
  }
  for (size_t k = 0; k < n; ++k) {
    size_t pivot = k;
    for (size_t i = k + 1; i < n; ++i)
      if (std::abs(a[i][k]) > std::abs(a[pivot][k]))
        pivot = i;
    std::swap(a[pivot], a[k]);
    std::swap(rhs[pivot], rhs[k]);
    for (size_t i = k + 1; i < n; ++i) {
      auto factor = a[i][k] / a[k][k];
      for (size_t j = k; j < n; ++j)
        a[i][j] -= factor * a[k][j];
      rhs[i] -= factor * rhs[k];
    }
  }
  for (size_t i = n; i--;) {
    for (size_t j = i + 1; j < n; ++j)
      rhs[i] -= a[i][j] * rhs[j];
    rhs[i] /= a[i][i];
  }
  return rhs;
}
void creator(bool positive) {
  Scene scene;
  append_humanoid(scene, {}, "Actual CC0 heat probe", {}, 64);
  for (const auto &o : scene.objects()) {
    if (!o.painted || !o.deformation || !o.deformation->skeleton)
      continue;
    HeatBindingOptions options;
    options.operation =
        positive ? HeatOperator::PositiveGraph : HeatOperator::FiniteElements;
    options.maximum_seed_distance = 1;
    options.limits.max_influences = 4;
    HeatBindingReport report;
    std::cout << "PART " << o.id << ' ' << o.name
              << " vertices=" << o.painted->mesh().positions.size() << '\n'
              << std::flush;
    try {
      auto profile = imported_heat_profile(
          *o.deformation->skeleton, *o.appearance, options, {},
          o.deformation->humanoid ? &*o.deformation->humanoid : nullptr);
      auto effective = options;
      effective.maximum_work -= profile.work;
      effective.maximum_bytes -= profile.planned_bytes;
      std::cout << "SOURCE_ELIGIBILITY root=" << int(profile.eligible[0])
                << " eligible="
                << std::count(profile.eligible.begin(), profile.eligible.end(),
                              uint8_t(1))
                << " point_forks=" << profile.point_forks.size()
                << " capture_work=" << profile.work << '\n';
      auto w = heat_bind_surface(o.painted->mesh(), *o.deformation->skeleton,
                                 effective, {}, &report, profile.segments,
                                 profile.eligible);
      auto next = *o.deformation;
      next.skin = w;
      auto pose = next.skeleton->bones;
      std::vector<JointTransform> transforms;
      for (auto &b : pose)
        transforms.push_back(b.rest);
      if (next.humanoid) {
        for (const char *name : {"leftLowerArm", "rightUpperLeg"}) {
          auto at = next.humanoid->joints.find(name);
          if (at != next.humanoid->joints.end())
            transforms[at->second].rotation =
                Quat{.98480775f, 0, 0, .17364818f} *
                transforms[at->second].rotation;
        }
      }
      auto original = *o.deformation;
      original.pose = transforms;
      next.pose = transforms;
      auto before = original.preview(o.painted->mesh()),
           after = next.preview(o.painted->mesh());
      double mean = 0, maximum = 0;
      size_t worst = 0;
      for (size_t i = 0; i < before.positions.size(); ++i) {
        double error = length(before.positions[i] - after.positions[i]);
        mean += error;
        if (error > maximum) {
          maximum = error;
          worst = i;
        }
      }
      mean /= before.positions.size();
      std::cout << "HEAT_RESULT components=" << report.components
                << " negative_edges=" << report.negative_edges
                << " removed_stiffness=" << report.removed_negative_stiffness
                << " seeded_bones=" << report.seeded_bones
                << " iterations=" << report.iterations
                << " residual=" << report.maximum_relative_residual
                << " raw_min=" << report.minimum_unclamped_weight
                << " partition=" << report.maximum_partition_error
                << " work=" << report.work << " posed_artist_mean=" << mean
                << " posed_artist_max=" << maximum << '\n';
      if (maximum > .03) {
        auto p = o.painted->mesh().positions[worst];
        std::cout << "WORST vertex=" << worst << " bind=" << p.x << ',' << p.y
                  << ',' << p.z << '\n';
        auto worlds = next.skeleton->world();
        auto binds = next.skeleton->inverse_bind();
        for (auto *row :
             {&original.skin->vertices[worst], &next.skin->vertices[worst]}) {
          std::cout << (row == &original.skin->vertices[worst] ? "ARTIST"
                                                               : "HEAT");
          for (auto influence : *row) {
            auto j = influence.joint;
            auto bind = AffineTransform(binds[j]).inverse().point({0, 0, 0});
            std::cout << " [" << j << ':' << next.skeleton->bones[j].name
                      << " w=" << influence.weight << " W=" << worlds[j][12]
                      << ',' << worlds[j][13] << ',' << worlds[j][14]
                      << " B=" << bind[0] << ',' << bind[1] << ',' << bind[2]
                      << ']';
          }
          std::cout << '\n';
        }
      }
    } catch (const HeatPositivityFailure &e) {
      std::cout << "REFUSED_NEGATIVE joint=" << e.joint
                << " minimum=" << std::setprecision(17) << e.minimum << '\n';
    } catch (const std::exception &e) {
      std::cout << "REFUSED " << e.what() << '\n';
    }
    std::cout << std::flush;
  }
}
} // namespace
int main(int argc, char **) try {
  if (argc > 1) {
    creator(argc > 2);
    return 0;
  }
  auto mesh = sheet();
  auto rig = bones();
  HeatBindingOptions o;
  HeatBindingReport report;
  auto w = heat_bind_surface(mesh, rig, o, {}, &report);
  auto expected = oracle(mesh, rig);
  double error = 0;
  for (size_t i = 0; i < expected.size(); ++i)
    error = std::max(error,
                     double(std::abs(expected[i] - weight(w.vertices[i], 0))));
  std::cout << "DENSE_ORACLE max_error=" << error
            << " residual=" << report.maximum_relative_residual
            << " partition=" << report.maximum_partition_error
            << " iterations=" << report.iterations << " work=" << report.work
            << '\n';
  check(error < 1e-6,
        "Native open-sheet PCG matches independent dense barycentric FEM");
  check(report.maximum_relative_residual <= o.relative_tolerance &&
            report.maximum_partition_error < 1e-7,
        "True residual and unpruned partition satisfy independent bounds");
  Mesh obtuse;
  obtuse.positions = {{0, 0, 0}, {.2f, 0, 0}, {.05f, .01f, 0}, {0, .2f, 0}};
  obtuse.normals.assign(4, {0, 0, 1});
  obtuse.uv = {{0, 0}, {1, 0}, {.25f, .05f}, {0, 1}};
  obtuse.indices = {0, 1, 2, 0, 2, 3};
  auto positive = o;
  positive.operation = HeatOperator::PositiveGraph;
  HeatBindingReport positive_report;
  auto graph = heat_bind_surface(obtuse, rig, positive, {}, &positive_report);
  auto graph_expected = oracle(obtuse, rig, true),
       fem_expected = oracle(obtuse, rig, false);
  double graph_error = 0, energy_difference = 0;
  for (size_t i = 0; i < 4; ++i) {
    graph_error = std::max(
        graph_error,
        double(std::abs(graph_expected[i] - weight(graph.vertices[i], 0))));
    energy_difference =
        std::max(energy_difference,
                 double(std::abs(graph_expected[i] - fem_expected[i])));
  }
  std::cout << "POSITIVE_GRAPH oracle_error=" << graph_error
            << " raw_fem_difference=" << energy_difference
            << " negative_edges=" << positive_report.negative_edges << '\n';
  check(graph_error < 1e-6 && positive_report.negative_edges > 0,
        "Positive graph agrees with independently assembled dense graph on "
        "genuine obtuse triangles");
  check(energy_difference > 1e-4,
        "Positive graph is explicitly a distinct energy from raw FEM");
  auto nondeform = rig;
  nondeform.bones[1].deform = false;
  auto only = heat_bind_surface(mesh, nondeform, o);
  for (auto &row : only.vertices)
    check(row.size() == 1 && row[0].joint == 0 && row[0].weight == 1,
          "Nondeforming bone is absent from solved source influences");
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
