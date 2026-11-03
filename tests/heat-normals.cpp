// SPDX-License-Identifier: GPL-3.0-or-later
// Independent analytic geometric-normal controls. Includes the actual private
// assembly only to inspect its rows; there is no production testing hook.
#include "../src/heat-binding.cpp"
#include <iostream>
using namespace dome;
using namespace dome::detail;
int main() try {
  Mesh m;
  m.positions = {{0, 0, 0}, {2, 0, 0}, {0, 2, 0},
                 {0, 0, 0}, {0, 0, 1}, {1, 0, 0}};
  m.normals.assign(6, {1, 0, 0}); // Artist normals must not authorize geometry.
  m.uv.assign(6, {0, 0});
  m.indices = {0, 1, 2, 3, 4, 5};
  std::vector<uint32_t> rows{0, 1, 2, 0, 4, 5};
  std::function<bool()> cancel;
  auto construct = [&](HeatNormals mode) {
    Budget budget{0, 100000, cancel};
    return assemble(m, budget, HeatOperator::PositiveGraph, rows, mode);
  };
  for (int permutation = 0; permutation < 2; ++permutation) {
    for (int reverse = 0; reverse < 2; ++reverse) {
      auto a = construct(HeatNormals::AreaWeighted);
      auto u = construct(HeatNormals::UnitFaces);
      if (a.positions.size() != 5 || u.positions != a.positions ||
          u.mass != a.mass || u.diagonal != a.diagonal ||
          u.values != a.values || u.raw_to_row != a.raw_to_row)
        throw std::runtime_error(
            "Normal policy changed topology, mass or stiffness");
      const long double sign = reverse ? -1.L : 1.L;
      const std::array<long double, 3> area{0, sign / std::sqrt(17.L),
                                            4 * sign / std::sqrt(17.L)},
          unit{0, sign / std::sqrt(2.L), sign / std::sqrt(2.L)};
      for (int k = 0; k < 3; ++k)
        if (std::abs(a.normals[0][k] - area[k]) > 2e-15L ||
            std::abs(u.normals[0][k] - unit[k]) > 2e-15L)
          throw std::runtime_error("Geometric normal differs from independent "
                                   "analytic construction");
      std::swap(m.indices[1], m.indices[2]);
      std::swap(m.indices[4], m.indices[5]);
    }
    std::swap_ranges(m.indices.begin(), m.indices.begin() + 3,
                     m.indices.begin() + 3);
  }
  std::cout << "PASS independent unequal-area unit/area normals, virtual seam, "
               "artist-normal independence, orientation/permutation and "
               "unchanged operator\n";
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
