// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/biharmonic-binding.cpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
using namespace dome::detail;
struct Dense {
  std::array<std::array<double, 4>, 4> q{};
};
std::array<long double, 4> independent(const Dense &a,
                                       const std::array<uint8_t, 4> &fixed,
                                       const std::array<double, 4> &values) {
  long double best = INFINITY;
  std::array<long double, 4> answer{};
  for (int code = 0; code < 81; ++code) {
    int digits = code;
    std::array<int, 4> state{}, free{};
    int n = 0;
    std::array<long double, 4> x{};
    for (int i = 0; i < 4; ++i) {
      state[i] = digits % 3;
      digits /= 3;
      if (fixed[i]) {
        state[i] = 3;
        x[i] = values[i];
      } else if (state[i] == 0)
        free[n++] = i;
      else
        x[i] = state[i] == 1 ? 0 : 1;
    }
    long double matrix[4][5]{};
    for (int r = 0; r < n; ++r) {
      for (int c = 0; c < n; ++c)
        matrix[r][c] = a.q[free[r]][free[c]];
      for (int j = 0; j < 4; ++j)
        if (state[j])
          matrix[r][n] -= a.q[free[r]][j] * x[j];
    }
    bool singular = false;
    for (int c = 0; c < n; ++c) {
      int pivot = c;
      for (int r = c + 1; r < n; ++r)
        if (std::abs(matrix[r][c]) > std::abs(matrix[pivot][c]))
          pivot = r;
      if (std::abs(matrix[pivot][c]) < 1e-24L) {
        singular = true;
        break;
      }
      for (int k = c; k <= n; ++k)
        std::swap(matrix[c][k], matrix[pivot][k]);
      long double divisor = matrix[c][c];
      for (int k = c; k <= n; ++k)
        matrix[c][k] /= divisor;
      for (int r = 0; r < n; ++r)
        if (r != c) {
          long double factor = matrix[r][c];
          for (int k = c; k <= n; ++k)
            matrix[r][k] -= factor * matrix[c][k];
        }
    }
    if (singular)
      continue;
    bool feasible = true;
    for (int r = 0; r < n; ++r) {
      x[free[r]] = matrix[r][n];
      feasible &= x[free[r]] >= -1e-14L && x[free[r]] <= 1 + 1e-14L;
    }
    if (!feasible)
      continue;
    long double e = 0;
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j)
        e += x[i] * a.q[i][j] * x[j] / 2;
    if (e < best) {
      best = e;
      answer = x;
    }
  }
  if (!std::isfinite(best))
    throw std::runtime_error("Independent active-state enumeration infeasible");
  return answer;
}
std::vector<std::byte> bytes(const Mesh &m) {
  std::vector<std::byte> out;
  auto add = [&](const auto &v) {
    auto data = std::as_bytes(std::span(v));
    out.insert(out.end(), data.begin(), data.end());
  };
  add(m.positions);
  add(m.normals);
  add(m.uv);
  add(m.indices);
  add(m.uv_pins);
  add(m.uv_seams);
  return out;
}
Mesh strip(int layers = 1) {
  Mesh m;
  const int columns = 8, rows = 4;
  for (int layer = 0; layer < layers; ++layer) {
    uint32_t start = uint32_t(m.positions.size());
    for (int y = 0; y <= rows; ++y)
      for (int x = 0; x <= columns; ++x) {
        m.positions.push_back(
            {2.f * x / columns, float(y) / rows, layer * .0001f});
        m.normals.push_back({0, 0, 1});
        m.uv.push_back({float(x) / columns, float(y) / rows});
      }
    for (int y = 0; y < rows; ++y)
      for (int x = 0; x < columns; ++x) {
        uint32_t a = start + y * (columns + 1) + x, b = a + 1,
                 c = a + columns + 1, d = c + 1;
        m.indices.insert(m.indices.end(), {a, b, d, a, d, c});
      }
  }
  return m;
}
int main() try {
  Mesh tiny;
  tiny.positions = {{0, 0, 0}, {2, 0, 0}, {1.5f, 1, 0}, {0, 1.5f, 0}};
  tiny.normals.assign(4, {0, 0, 1});
  tiny.uv.assign(4, {0, 0});
  tiny.indices = {0, 1, 2, 0, 2, 3};
  std::function<bool()> cancel;
  Budget budget{0, 10'000'000, cancel};
  auto l = assemble(tiny, budget, HeatOperator::FiniteElements, {},
                    HeatNormals::UnitFaces, HeatMass::Barycentric);
  BiharmonicOperator q(l, budget);
  long double dense_l[4][4]{}, mass[4]{};
  for (size_t t = 0; t < 6; t += 3) {
    auto i = tiny.indices[t], j = tiny.indices[t + 1], k = tiny.indices[t + 2];
    std::array<uint32_t, 3> id{i, j, k};
    long double x[3]{tiny.positions[i].x, tiny.positions[j].x,
                     tiny.positions[k].x},
        y[3]{tiny.positions[i].y, tiny.positions[j].y, tiny.positions[k].y};
    long double det = (x[1] - x[0]) * (y[2] - y[0]) -
                      (y[1] - y[0]) * (x[2] - x[0]),
                area = std::abs(det) / 2;
    long double gx[3], gy[3];
    for (int a = 0; a < 3; ++a) {
      gx[a] = (y[(a + 1) % 3] - y[(a + 2) % 3]) / det;
      gy[a] = (x[(a + 2) % 3] - x[(a + 1) % 3]) / det;
      mass[id[a]] += area / 3;
    }
    for (int a = 0; a < 3; ++a)
      for (int b = 0; b < 3; ++b)
        dense_l[id[a]][id[b]] += area * (gx[a] * gx[b] + gy[a] * gy[b]);
  }
  long double average = 0;
  for (auto m : mass)
    average += m / 4;
  Dense oracle;
  double max_operator = 0;
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) {
      long double value = 0;
      for (int k = 0; k < 4; ++k)
        value += average * dense_l[k][i] * dense_l[k][j] / mass[k];
      oracle.q[i][j] = double(value);
    }
  for (int j = 0; j < 4; ++j) {
    std::array<double, 4> x{}, out{};
    x[j] = 1;
    q.apply(x, out, budget);
    for (int i = 0; i < 4; ++i)
      max_operator = std::max(max_operator, std::abs(out[i] - oracle.q[i][j]));
  }
  if (max_operator > 2e-14)
    throw std::runtime_error("Biharmonic operator differs from independent "
                             "gradient-FEM/mass dense product");
  std::array<uint8_t, 4> fixed{1, 0, 0, 1};
  std::array<double, 4> values{1, 0, 0, 0}, field{1, .5, .5, 0};
  auto expected = independent(oracle, fixed, values);
  BoundedQuadraticOptions qp;
  qp.relative_tolerance = 1e-10;
  auto solved = solve_bounded_quadratic(q, fixed, values, field, qp, budget);
  BiharmonicOperator accelerated(l, budget, fixed, true, 1'000'000);
  auto accelerated_field = field;
  accelerated_field[1] = accelerated_field[2] = .5;
  auto accelerated_report = solve_bounded_quadratic(
      accelerated, fixed, values, accelerated_field, qp, budget);
  for (size_t i = 0; i < 4; ++i)
    if (std::abs(accelerated_field[i] - expected[i]) > 1e-8)
      throw std::runtime_error(
          "Squared Laplacian preconditioner changed dense QP result");
  (void)accelerated_report;
  BiharmonicOperator direct(l, budget, fixed, false, 1'000'000, true);
  auto direct_field = field;
  direct_field[1] = direct_field[2] = .5;
  auto direct_report =
      solve_bounded_quadratic(direct, fixed, values, direct_field, qp, budget);
  for (size_t i = 0; i < 4; ++i)
    if (std::abs(direct_field[i] - expected[i]) > 1e-8)
      throw std::runtime_error(
          "Sparse LDL changed independent dense QP minimum");
  const long double det =
      static_cast<long double>(oracle.q[1][1]) * oracle.q[2][2] -
      static_cast<long double>(oracle.q[1][2]) * oracle.q[2][1];
  std::array<uint8_t, 4> status{3, 0, 0, 3};
  for (size_t j = 1; j < 3; ++j) {
    std::array<double, 4> r{}, z{};
    r[j] = 1;
    direct.full->apply(r, z, status, budget);
    for (size_t i = 1; i < 3; ++i) {
      long double exact = (i == j ? (i == 1 ? oracle.q[2][2] : oracle.q[1][1])
                                  : -oracle.q[1][2]) /
                          det;
      if (std::abs(z[i] - exact) > 1e-10)
        throw std::runtime_error(
            "Sparse LDL inverse differs from independent2x2 principal inverse");
    }
  }
  auto factors = direct.full->factorizations;
  std::array<double, 4> r{0, 0, 1, 0}, z{};
  status[1] = 1;
  direct.full->apply(r, z, status, budget);
  if (std::abs(z[2] - 1 / oracle.q[2][2]) > 1e-10 || z[1] != 0 ||
      direct.full->factorizations != factors + 1)
    throw std::runtime_error(
        "Same-address changed free mask reused a wrong factor");
  auto copy = status;
  direct.full->apply(r, z, copy, budget);
  if (direct.full->factorizations != factors + 1)
    throw std::runtime_error("Equal free masks were needlessly refactored");
  // A restricted inverse is intentionally NOT the inverse of the smaller
  // principal Q: prove the exact dense inverse block, with both input and
  // output projected. Active changes reuse one base factor.
  BiharmonicOperator restricted(l, budget, fixed, false, 1'000'000, true, true);
  std::array<uint8_t, 4> working{3, 1, 0, 3};
  std::array<double, 4> forcing{123, 456, 1, 789}, projected{};
  restricted.precondition(forcing, projected, working, budget);
  const double expected_inverse22 = double(oracle.q[1][1] / det);
  if (std::abs(projected[2] - expected_inverse22) > 1e-10 ||
      projected[0] != 0 || projected[1] != 0 || projected[3] != 0 ||
      std::abs(expected_inverse22 - 1 / oracle.q[2][2]) < 1e-4 ||
      restricted.full->factorizations != 1)
    throw std::runtime_error(
        "Restricted inverse differs from independent dense block");
  working[1] = 0;
  forcing = {0, 1, 0, 0};
  restricted.precondition(forcing, projected, working, budget);
  if (std::abs(projected[1] - double(oracle.q[2][2] / det)) > 1e-10 ||
      std::abs(projected[2] + double(oracle.q[1][2] / det)) > 1e-10 ||
      restricted.full->factorizations != 1)
    throw std::runtime_error("Active-mask change lost projected inverse reuse");
  bool missing_base_refused = false;
  working[0] = 0;
  try {
    restricted.precondition(forcing, projected, working, budget);
  } catch (const std::invalid_argument &) {
    missing_base_refused = true;
  }
  if (!missing_base_refused)
    throw std::runtime_error(
        "Restricted mask released a base Dirichlet constraint");
  auto restricted_field = field;
  restricted_field[1] = restricted_field[2] = .5;
  auto restricted_report = solve_bounded_quadratic(
      restricted, fixed, values, restricted_field, qp, budget);
  for (size_t i = 0; i < 4; ++i)
    if (std::abs(restricted_field[i] - expected[i]) > 1e-8)
      throw std::runtime_error(
          "Restricted preconditioner changed dense QP result");
  if (restricted.full->factorizations != 1)
    throw std::runtime_error("Restricted QP needlessly refactored base matrix");
  std::cout << "RESTRICTED dense_block=" << expected_inverse22
            << " smaller_inverse=" << 1 / oracle.q[2][2]
            << " factors=" << restricted.full->factorizations
            << " kkt=" << restricted_report.kkt_residual << '\n';
  // Release a saved bound: exact inverse block on old-free1 plus positive
  // Jacobi on released2. The off-block entry must be ZERO, not a Schur guess.
  BiharmonicSparseLDL lagged_factor(l, 1'000'000, budget);
  std::array<uint8_t, 4> saved{3, 0, 1, 3}, released{3, 0, 0, 3};
  forcing = {0, 2, 3, 0};
  lagged_factor.apply_restricted(forcing, projected, saved, released, budget,
                                 true);
  if (std::abs(projected[1] - 2 / oracle.q[1][1]) > 1e-10 ||
      std::abs(projected[2] - 3 / oracle.q[2][2]) > 1e-10 ||
      !(forcing[1] * projected[1] + forcing[2] * projected[2] > 0))
    throw std::runtime_error(
        "Released block differs from independent SPD diagonal blocks");
  BiharmonicOperator lagged(l, budget, fixed, false, 1'000'000, true, true,
                            true);
  auto lagged_field = field;
  lagged_field[1] = lagged_field[2] = .5;
  auto lagged_report = solve_block_bounded_quadratic(lagged, fixed, values,
                                                     lagged_field, qp, budget);
  for (size_t i = 0; i < 4; ++i)
    if (std::abs(lagged_field[i] - expected[i]) > 1e-8)
      throw std::runtime_error(
          "Lagged preconditioner changed independent box solution");
  std::cout << "LAGGED kkt=" << lagged_report.kkt_residual << '\n';
  std::cout << "SPARSE nnz=" << direct.full->matrix_nonzeros
            << " L=" << direct.full->factor_nonzeros
            << " planned=" << direct.full->planned_bytes
            << " kkt=" << direct_report.kkt_residual << '\n';

  for (size_t i = 0; i < 4; ++i)
    if (std::abs(field[i] - expected[i]) > 1e-8)
      throw std::runtime_error(
          "Surface QP differs from independent active-state oracle");
  Skeleton rig;
  rig.bones.resize(2);
  rig.bones[0].name = "A";
  rig.bones[1].name = "B";
  const std::array<HeatSourceSegment, 2> sources{
      {{{0, 0, -.1}, {0, 1, -.1}, 0}, {{2, 0, -.1}, {2, 1, -.1}, 1}}};
  BiharmonicBindingOptions o;
  o.source.distance_rule = HeatDistanceRule::EuclideanSurface;
  o.source.seed_rule = HeatSeedRule::ReferenceSegments;
  o.source.visibility = HeatVisibility::EntryFaces;
  o.source.occlusion = HeatOcclusion::ConnectedSurface;
  o.source.maximum_seed_distance = 10;
  o.source.limits = {2, 0};
  o.source.maximum_work = 10'000'000;
  o.solve.maximum_cg_iterations = 512;
  o.preconditioner = BiharmonicPreconditioner::LaggedSparseLDL;
  for (int layers : {1, 2}) {
    auto m = strip(layers);
    auto before = bytes(m);
    auto rig_before = rig.encode();
    BiharmonicBindingReport report;
    auto weights =
        bounded_biharmonic_surface(m, rig, o, sources, {}, {}, {}, &report);
    if (report.components != size_t(layers) ||
        report.vertices != m.positions.size() || bytes(m) != before ||
        rig.encode() != rig_before)
      throw std::runtime_error(
          "Open/near-contact components merged or source data changed");
    auto weight = [&](size_t vertex, uint32_t joint) {
      double result = 0;
      for (auto v : weights.vertices[vertex])
        if (v.joint == joint)
          result += v.weight;
      return result;
    };
    for (int layer = 0; layer < layers; ++layer)
      for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 9; ++x) {
          size_t i = layer * 45 + y * 9 + x,
                 mirror = layer * 45 + y * 9 + 8 - x;
          if (std::abs(weight(i, 0) - weight(mirror, 1)) > 2e-6 ||
              (x == 0 && weight(i, 0) != 1) || (x == 8 && weight(i, 1) != 1))
            throw std::runtime_error(
                "Visible projected boundary/symmetry failed");
          if (layer && weights.vertices[i] != weights.vertices[y * 9 + x])
            throw std::runtime_error(
                "Separate near-contact layer changed independent field");
        }
    std::cout << "SURFACE layers=" << layers << " vertices=" << report.vertices
              << " anchors=" << report.fixed_rows
              << " cg=" << report.cg_iterations
              << " active=" << report.active_iterations
              << " kkt=" << report.maximum_kkt_residual
              << " partition=" << report.maximum_partition_error
              << " work=" << report.work << '\n';
  }
  auto far = strip();
  uint32_t first = uint32_t(far.positions.size());
  far.positions.insert(far.positions.end(),
                       {{20, 0, 0}, {21, 0, 0}, {20, 1, 0}});
  far.normals.insert(far.normals.end(), 3, {0, 0, 1});
  far.uv.insert(far.uv.end(), 3, {0, 0});
  far.indices.insert(far.indices.end(), {first, first + 1, first + 2});
  BiharmonicBindingReport sentinel;
  sentinel.work = 173;
  bool refused = false;
  try {
    bounded_biharmonic_surface(far, rig, o, sources, {}, {}, {}, &sentinel);
  } catch (const HeatSeedFailure &) {
    refused = true;
  }
  if (!refused || sentinel.work != 173)
    throw std::runtime_error("Unseeded component silently assigned output");
  std::cout
      << "PASS independent Q=L'M^-1L maxerror=" << max_operator
      << " KKT=" << solved.kkt_residual
      << " open/near-contact projection, exact inputs, unseeded refusal\n";
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
