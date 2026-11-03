// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/sampled-anatomical-domain.hpp"
#include "../src/heat-binding.cpp"
#include <iostream>
#include <numbers>
using namespace dome;
using namespace dome::detail;
Mesh tube() {
  Mesh m;
  constexpr int around = 12;
  for (int y = 0; y < 4; ++y)
    for (int k = 0; k < around; ++k) {
      float angle = 2 * std::numbers::pi_v<float> * k / around;
      float radius = y == 1 ? .1f : 1.f;
      m.positions.push_back(
          {radius * std::cos(angle), float(y), radius * std::sin(angle)});
      m.normals.push_back({std::cos(angle), 0, std::sin(angle)});
      m.uv.push_back({float(k) / around, float(y) / 3});
    }
  for (int y = 0; y < 3; ++y)
    for (int k = 0; k < around; ++k) {
      uint32_t a = y * around + k, b = y * around + (k + 1) % around,
               c = a + around, d = b + around;
      m.indices.insert(m.indices.end(), {a, c, d, a, d, b});
    }
  return m;
}
int main() try {
  auto m = tube(), original = m;
  std::function<bool()> cancel;
  Budget budget{0, 20'000'000, cancel};
  auto s = assemble(m, budget, HeatOperator::FiniteElements, {},
                    HeatNormals::UnitFaces, HeatMass::MixedVoronoi);
  HeatBindingOptions options;
  options.maximum_work = 20'000'000;
  options.maximum_seed_distance = 10;
  options.visibility = HeatVisibility::EntryFaces;
  options.occlusion = HeatOcclusion::ConnectedSurface;
  std::vector<std::array<uint64_t, 4>> usage(s.positions.size());
  for (size_t row = 0; row < s.parent.size(); ++row)
    usage[s.parent[row]][0] = 3;
  const std::array<HeatSourceSegment, 2> bones{
      {{{0, 0, 0}, {0, 1.5, 0}, 0}, {{0, 1.5, 0}, {0, 3, 0}, 1}}};
  std::array<std::array<double, 3>, 2> axes{{{0, 1, 0}, {0, 1, 0}}};
  auto out = diagnose_sampled_anatomical_domain(s, m, bones, axes, usage,
                                                options, budget);
  std::vector<uint32_t> a, b;
  for (uint32_t row = 0; row < s.positions.size(); ++row) {
    if (out.candidate_anchors[row][0] & 1)
      a.push_back(row);
    if (out.candidate_anchors[row][0] & 2)
      b.push_back(row);
  }
  // Independent exhaustive geometry oracle on this star-shaped fixture:
  // its geometrically nearest rows have an unobstructed radial segment.
  std::vector<std::array<uint64_t, 4>> exhaustive(s.positions.size());
  std::array<std::vector<uint32_t>, 2> old_columns;
  for (size_t source = 0; source < bones.size(); ++source) {
    double old_best = INFINITY;
    for (auto p : m.positions) {
      double y =
          std::clamp(double(p.y), bones[source].head[1], bones[source].tail[1]);
      old_best = std::min(
          old_best, std::hypot(double(p.x), double(p.y) - y, double(p.z)));
    }
    for (uint32_t row = 0; row < m.positions.size(); ++row) {
      auto p = m.positions[row];
      double y =
          std::clamp(double(p.y), bones[source].head[1], bones[source].tail[1]);
      if (std::hypot(double(p.x), double(p.y) - y, double(p.z)) <=
          old_best * (1 + 1e-4))
        old_columns[source].push_back(row);
    }
    size_t samples =
        std::max(size_t(2), size_t(std::ceil(1.5 / out.mean_edge)) + 1);
    for (size_t point = 0; point < samples; ++point) {
      double y = bones[source].head[1] + 1.5 * point / (samples - 1),
             best = INFINITY;
      for (auto p : m.positions)
        best = std::min(best,
                        std::hypot(double(p.x), double(p.y) - y, double(p.z)));
      for (uint32_t row = 0; row < m.positions.size(); ++row) {
        auto p = m.positions[row];
        if (std::hypot(double(p.x), double(p.y) - y, double(p.z)) <=
            best * (1 + 1e-4))
          exhaustive[row][0] |= uint64_t(1) << source;
      }
    }
  }
  if (old_columns[0] != old_columns[1] || old_columns[0].size() != 12)
    throw std::runtime_error(
        "Independent global-minimum collision control changed");
  if (exhaustive != out.candidate_anchors)
    throw std::runtime_error(
        "Heap stopping differs from exhaustive physical point projection");
  if (a.empty() || b.empty() || a == b || !out.all_axially_complete)
    throw std::runtime_error("Longitudinal handles collapsed or lost support");
  bool lower = false, upper = false;
  for (auto row : a)
    lower |= s.positions[row][1] == 0;
  for (auto row : b)
    upper |= s.positions[row][1] == 3;
  if (!lower || !upper)
    throw std::runtime_error(
        "Extended bone endpoints lost their physical domain");
  auto outside = bones;
  outside[1] = {{0, 4, 0}, {0, 5, 0}, 1};
  auto rejected = diagnose_sampled_anatomical_domain(s, m, outside, axes, usage,
                                                     options, budget);
  if (rejected.all_axially_complete ||
      rejected.records.back().support != AnatomicalSupport::Outside)
    throw std::runtime_error("Outside bone was falsely granted unit ownership");
  for (auto mask : rejected.candidate_anchors)
    if (mask[0] & 2)
      throw std::runtime_error("Outside bone acquired anchors");
  outside[1] = {{0, 4, 0}, {0, 4, 0}, 1};
  rejected = diagnose_sampled_anatomical_domain(s, m, outside, axes, usage,
                                                options, budget);
  if (rejected.all_axially_complete ||
      rejected.records.back().support != AnatomicalSupport::Outside)
    throw std::runtime_error("Outside point acquired anatomical ownership");
  auto reflected = m;
  for (auto &p : reflected.positions)
    p.x = -p.x;
  for (size_t at = 0; at < reflected.indices.size(); at += 3)
    std::swap(reflected.indices[at + 1], reflected.indices[at + 2]);
  auto sr = assemble(reflected, budget, HeatOperator::FiniteElements, {},
                     HeatNormals::UnitFaces, HeatMass::MixedVoronoi);
  std::vector<std::array<uint64_t, 4>> reflected_usage(sr.positions.size());
  for (auto root : sr.parent)
    reflected_usage[root][0] = 3;
  auto mirrored = diagnose_sampled_anatomical_domain(
      sr, reflected, bones, axes, reflected_usage, options, budget);
  if (mirrored.candidate_anchors != out.candidate_anchors)
    throw std::runtime_error("Reflection changed physical anchor identity");
  auto permuted = m;
  std::reverse(permuted.positions.begin(), permuted.positions.end());
  std::reverse(permuted.normals.begin(), permuted.normals.end());
  std::reverse(permuted.uv.begin(), permuted.uv.end());
  for (auto &v : permuted.indices)
    v = uint32_t(m.positions.size() - 1 - v);
  auto sp = assemble(permuted, budget, HeatOperator::FiniteElements, {},
                     HeatNormals::UnitFaces, HeatMass::MixedVoronoi);
  std::vector<std::array<uint64_t, 4>> permuted_usage(sp.positions.size());
  for (auto root : sp.parent)
    permuted_usage[root][0] = 3;
  auto permuted_out = diagnose_sampled_anatomical_domain(
      sp, permuted, bones, axes, permuted_usage, options, budget);
  for (size_t row = 0; row < sp.positions.size(); ++row)
    if (permuted_out.candidate_anchors[row] !=
        out.candidate_anchors[sp.positions.size() - 1 - row])
      throw std::runtime_error(
          "Source permutation changed physical projection");
  std::function<bool()> stop;
  Budget *running = nullptr;
  stop = [&] { return running && running->used > 2000; };
  Budget interrupted{0, 20'000'000, stop};
  running = &interrupted;
  bool cancelled_mid = false;
  try {
    diagnose_sampled_anatomical_domain(s, m, bones, axes, usage, options,
                                       interrupted);
  } catch (const std::runtime_error &) {
    cancelled_mid = true;
  }
  if (!cancelled_mid || interrupted.used <= 2000)
    throw std::runtime_error(
        "Actual mid-construction cancellation not reached");
  auto same = [](const auto &a, const auto &b) {
    auto x = std::as_bytes(std::span(a)), y = std::as_bytes(std::span(b));
    return x.size() == y.size() && std::equal(x.begin(), x.end(), y.begin());
  };
  if (!same(m.positions, original.positions) ||
      !same(m.indices, original.indices) ||
      !same(m.normals, original.normals) || !same(m.uv, original.uv))
    throw std::runtime_error("Anatomical diagnostic changed artist arrays");
  auto tiny = options;
  tiny.maximum_bytes = 4096;
  bool refused = false;
  try {
    diagnose_sampled_anatomical_domain(s, m, bones, axes, usage, tiny, budget);
  } catch (const std::length_error &) {
    refused = true;
  }
  if (!refused)
    throw std::runtime_error("Anatomical byte cap ignored");
  std::cout << "PASS longitudinal columns distinct points=" << out.points
            << " rays=" << out.rays << " mean_edge=" << out.mean_edge
            << " work=" << budget.used
            << " outside bone/point refused reflection/source exact\n";
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
