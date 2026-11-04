// SPDX-License-Identifier: GPL-3.0-or-later
#include "bone-shadow-query.hpp"
#include "weight-surface-system.hpp"
#include <iostream>
using namespace dome::detail;
int main() try {
  using P = bone_shadow::Point;
  std::vector<P> p{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
  std::vector<uint32_t> indices{0, 1, 2}, groups{0, 1, 2};
  weight_surface::Budget work{0, 1000000, {}};
  BoneShadowQuery points(p, indices, 1024 * 1024, work, groups,
                         BoneOriginPolicy::NativeOpenSegment);
  BoneShadowQuery lines(p, indices, 1024 * 1024, work, groups,
                        BoneOriginPolicy::NativeFiniteLineIntegral);
  BoneShadowQuery::Scratch scratch(points), line_scratch(lines);
  BoneQueryReport report;
  const P pivot{0, 0, 1};
  const auto v =
      points.integrate(0, pivot, pivot, 1, 0, 1, scratch, work, report,
                       BoneWallDomain::ConnectedSourceSurface, true, true,
                       BoneOriginPolicy::NativeOpenSegment);
  if (std::abs(v - .5) > 1e-14)
    throw std::runtime_error(
        "Point kernel differs from independent mass/(distanceSquared+mass)");
  bool reject = false;
  try {
    lines.integrate(0, pivot, pivot, 1, 0, 1, line_scratch, work, report,
                    BoneWallDomain::ConnectedSourceSurface, true, true,
                    BoneOriginPolicy::NativeFiniteLineIntegral);
  } catch (const std::invalid_argument &) {
    reject = true;
  }
  if (!reject)
    throw std::runtime_error("Finite-line admission accepted a point");
  reject = false;
  try {
    points.integrate(0, pivot, pivot, 1, 0, 1, line_scratch, work, report,
                     BoneWallDomain::ConnectedSourceSurface, true, true,
                     BoneOriginPolicy::NativeFiniteLineIntegral);
  } catch (const std::invalid_argument &) {
    reject = true;
  }
  if (!reject)
    throw std::runtime_error("Point query admitted finite-line policy");
  // Scratch is deliberately reusable: prepare() must invalidate foreign
  // geometry/policy-owned caches rather than treating capacity as authority.
  const auto before = work.used;
  const auto reused = points.integrate(
      0, pivot, pivot, 1, 0, 1, line_scratch, work, report,
      BoneWallDomain::ConnectedSourceSurface, true, true,
      BoneOriginPolicy::NativeOpenSegment);
  if (reused != v || work.used <= before)
    throw std::runtime_error("Foreign scratch did not replay independent point proof");
  // Independent similarity controls: common world scale multiplies both
  // distance squared and lumped area; point kernel must stay exactly .5.
  for (double scale : {.25, 4.}) {
    for (int axis = 0; axis < 3; ++axis) {
      auto transform = [&](P a) {
        P b{};
        for (size_t k = 0; k < 3; ++k)
          b[(k + axis) % 3] = float(a[k] * scale + (k + 1) * 2.);
        return b;
      };
      std::vector<P> transformed;
      for (auto a : p) transformed.push_back(transform(a));
      BoneShadowQuery q(transformed, indices, 1024 * 1024, work, groups,
                        BoneOriginPolicy::NativeOpenSegment);
      BoneShadowQuery::Scratch qs(q);
      auto a = transform(pivot);
      auto result = q.integrate(0, a, a, scale * scale, 0, 1, qs, work,
                                report, BoneWallDomain::ConnectedSourceSurface,
                                true, true, BoneOriginPolicy::NativeOpenSegment);
      if (std::abs(result - .5) > 1e-14)
        throw std::runtime_error("Point kernel changed under actual similarity metric");
    }
  }
  std::cout << "PASS point analytic.5 +unchanged finite-line refusal +policy "
               "admission rejection +foreign cache replay\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << e.what() << std::endl;
  return 1;
}
