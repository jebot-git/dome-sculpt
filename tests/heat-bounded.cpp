// SPDX-License-Identifier: GPL-3.0-or-later
// Independent exhaustive active-set oracle versus the actual private solver.
#include "../src/heat-binding.cpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
using namespace dome::detail;
using Matrix = std::array<std::array<long double, 4>, 4>;
using Vector = std::array<long double, 4>;
std::vector<std::byte> mesh_bytes(const Mesh &m) {
  std::vector<std::byte> result;
  auto append = [&](const auto &values) {
    const uint64_t count = values.size();
    auto size = std::as_bytes(std::span(&count, 1));
    result.insert(result.end(), size.begin(), size.end());
    auto data = std::as_bytes(std::span(values));
    result.insert(result.end(), data.begin(), data.end());
  };
  append(m.positions);
  append(m.normals);
  append(m.uv);
  append(m.indices);
  append(m.uv_pins);
  append(m.uv_seams);
  return result;
}
Vector oracle(const Matrix &a, const Vector &b) {
  Vector best{};
  long double minimum = INFINITY;
  for (unsigned code = 0; code < 81; ++code) {
    unsigned next = code;
    std::array<unsigned, 4> status{};
    std::vector<size_t> free;
    Vector x{};
    for (size_t i = 0; i < 4; ++i) {
      status[i] = next % 3;
      next /= 3;
      if (!status[i])
        free.push_back(i);
      else
        x[i] = status[i] == 2;
    }
    Matrix m{};
    Vector rhs{};
    for (size_t i = 0; i < free.size(); ++i) {
      size_t row = free[i];
      rhs[i] = b[row];
      for (size_t k = 0; k < 4; ++k)
        if (status[k])
          rhs[i] -= a[row][k] * x[k];
      for (size_t j = 0; j < free.size(); ++j)
        m[i][j] = a[row][free[j]];
    }
    bool valid = true;
    for (size_t k = 0; k < free.size(); ++k) {
      size_t pivot = k;
      for (size_t i = k + 1; i < free.size(); ++i)
        if (std::abs(m[i][k]) > std::abs(m[pivot][k]))
          pivot = i;
      if (std::abs(m[pivot][k]) < 1e-20L) {
        valid = false;
        break;
      }
      std::swap(m[k], m[pivot]);
      std::swap(rhs[k], rhs[pivot]);
      for (size_t i = k + 1; i < free.size(); ++i) {
        long double factor = m[i][k] / m[k][k];
        for (size_t j = k; j < free.size(); ++j)
          m[i][j] -= factor * m[k][j];
        rhs[i] -= factor * rhs[k];
      }
    }
    if (!valid)
      continue;
    for (size_t i = free.size(); i--;) {
      for (size_t j = i + 1; j < free.size(); ++j)
        rhs[i] -= m[i][j] * rhs[j];
      rhs[i] /= m[i][i];
      x[free[i]] = rhs[i];
    }
    long double energy = 0;
    for (size_t i = 0; i < 4; ++i) {
      if (x[i] < -1e-12L || x[i] > 1 + 1e-12L) {
        valid = false;
        break;
      }
      long double g = -b[i];
      for (size_t j = 0; j < 4; ++j)
        g += a[i][j] * x[j];
      if ((status[i] == 1 && g < -1e-12L) || (status[i] == 2 && g > 1e-12L) ||
          (!status[i] && std::abs(g) > 1e-12L)) {
        valid = false;
        break;
      }
      energy += .5L * x[i] * (g - b[i]);
    }
    if (valid && energy < minimum) {
      minimum = energy;
      best = x;
    }
  }
  if (!std::isfinite(minimum))
    throw std::runtime_error("Independent exhaustive box QP has no optimum");
  return best;
}
int main() try {
  // Exact standard-FEM gradient matrix for (0,0),(20,0),(5,1),(0,20).
  const Matrix laplace{{{{379.L / 50, 37.L / 20, -47.L / 5, -3.L / 100}},
                        {{37.L / 20, 13.L / 20, -5.L / 2, 0}},
                        {{-47.L / 5, -5.L / 2, 12, -1.L / 10}},
                        {{-3.L / 100, 0, -1.L / 10, 13.L / 100}}}};
  size_t cases = 0, nontrivial = 0;
  double maximum_error = 0, maximum_kkt = 0;
  std::function<bool()> cancel;
  HeatBindingOptions options;
  options.operation = HeatOperator::BoundedFiniteElements;
  for (long double heat : {1.L, 10.L})
    for (Vector target : std::array<Vector, 4>{{{{1, 0, 0, 0}},
                                                {{0, 1, 1, 1}},
                                                {{.3L, .4L, .2L, .9L}},
                                                {{0, 0, 0, 0}}}})
      for (bool reverse : {false, true}) {
        Matrix a = laplace;
        Vector b;
        for (size_t i = 0; i < 4; ++i) {
          a[i][i] += heat;
          b[i] = heat * target[i];
        }
        auto expected = oracle(a, b);
        System s;
        s.starts.push_back(0);
        std::vector<double> rhs(4), x(4);
        auto order = [&](size_t i) { return reverse ? 3 - i : i; };
        for (size_t i = 0; i < 4; ++i) {
          auto row = order(i);
          s.diagonal.push_back(double(a[row][row]));
          rhs[i] = double(b[row]);
          for (size_t j = 0; j < 4; ++j)
            if (i != j) {
              s.columns.push_back(uint32_t(j));
              s.values.push_back(double(a[row][order(j)]));
            }
          s.starts.push_back(s.columns.size());
        }
        Budget budget{0, 1000000, cancel};
        auto raw = solve(s, rhs, x, options, budget);
        std::vector<double> clipped = x;
        for (auto &value : clipped)
          value = std::clamp(value, 0., 1.);
        auto bounded = solve_bounds(s, rhs, x, options, budget);
        double changed = 0;
        for (size_t i = 0; i < 4; ++i) {
          if (x[i] < 0 || x[i] > 1)
            throw std::runtime_error("Box iterate violated actual bounds");
          maximum_error = std::max(maximum_error,
                                   double(std::abs(x[i] - expected[order(i)])));
          changed = std::max(changed, std::abs(x[i] - clipped[i]));
        }
        nontrivial += changed > 1e-4;
        maximum_kkt = std::max(maximum_kkt, bounded.residual);
        if (raw.residual > options.relative_tolerance ||
            bounded.residual > options.relative_tolerance)
          throw std::runtime_error(
              "Bounded solver did not meet actual linear/KKT bounds");
        ++cases;
      }
  if (maximum_error > 1e-7 || !nontrivial)
    throw std::runtime_error("Bounded field differs from exhaustive optimum or "
                             "merely returns clipped guesses");
  std::cout << "BOX_ORACLE cases=" << cases << " error=" << maximum_error
            << " kkt=" << maximum_kkt << " nontrivial=" << nontrivial << '\n';
  Mesh m;
  m.positions = {{0, 0, 0}, {20, 0, 0}, {5, 1, 0}, {0, 20, 0}};
  m.normals.assign(4, {0, 0, 1});
  m.uv.assign(4, {0, 0});
  m.indices = {0, 1, 2, 0, 2, 3};
  Skeleton rig;
  rig.bones.resize(2);
  rig.bones[0].name = "A";
  rig.bones[1].name = "B";
  rig.bones[0].rest.translation = {0, 0, -.1f};
  rig.bones[1].rest.translation = {20, 0, -.1f};
  // Four genuine synthetic point handles give A=L+I and b=(1,0,0,0)
  // for bone A. This is the independent negative-potential case above, so
  // actual entry/failure tests must exercise bound optimization, not just an
  // already feasible unconstrained field. These are not artist-source IDs.
  const std::array<HeatSourceSegment, 4> sources{
      {{{0, 0, -std::sqrt(20.) * 1.0005}, {0, 0, -std::sqrt(20.) * 1.0005}, 0},
       {{20, 0, -std::sqrt(10. / 3) * 1.0005},
        {20, 0, -std::sqrt(10. / 3) * 1.0005},
        1},
       {{5, 1, -std::sqrt(20.) * 1.0005}, {5, 1, -std::sqrt(20.) * 1.0005}, 1},
       {{0, 20, -std::sqrt(50. / 3) * 1.0005},
        {0, 20, -std::sqrt(50. / 3) * 1.0005},
        1}}};
  options.seed_rule = HeatSeedRule::ReferenceSegments;
  auto input = mesh_bytes(m);
  auto bones = rig.encode();
  HeatBindingReport report;
  report.work = 173;
  options.maximum_seed_distance = 100;
  options.edges = HeatEdges::IncidentMean;
  bool refused = false;
  try {
    heat_bind_surface(m, rig, options, {}, &report, sources);
  } catch (const std::invalid_argument &) {
    refused = true;
  }
  if (!refused || report.work != 173 || mesh_bytes(m) != input ||
      rig.encode() != bones)
    throw std::runtime_error("Convex route admitted indefinite boundary matrix "
                             "or changed inputs/report on refusal");
  options.edges = HeatEdges::CotangentHalf;
  auto strict = options;
  strict.operation = HeatOperator::FiniteElements;
  bool negative = false;
  try {
    heat_bind_surface(m, rig, strict, {}, &report, sources);
  } catch (const HeatPositivityFailure &e) {
    negative = e.joint == 0 && e.minimum < -.019 && e.minimum > -.020;
  }
  if (!negative || report.work != 173 || mesh_bytes(m) != input ||
      rig.encode() != bones)
    throw std::runtime_error("Unchanged strict raw FEM control lost its "
                             "genuine negative field or changed source/report");
  std::cout << "STRICT_FEM genuine negative potential preserved before "
               "separate bounded-energy solve\n";
  auto result = heat_bind_surface(m, rig, options, {}, &report, sources);
  result.validate(4, 2);
  if (!report.bound_sweeps ||
      report.maximum_kkt_residual > options.relative_tolerance ||
      mesh_bytes(m) != input || rig.encode() != bones)
    throw std::runtime_error(
        "Actual bound bind violated KKT or exact source preservation");
  std::cout << "PASS exact active-set optimum, genuine bound optimization, "
               "permutation, actual bind/refusal/source/report\n";
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
