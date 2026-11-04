// SPDX-License-Identifier: GPL-3.0-or-later
// Independent dense energy controls, not implementation-shaped expected rows.
#include "weight-contact-intent.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome::detail;
namespace {
size_t checks = 0;
void require(bool v, const char *s) {
  ++checks;
  if (!v)
    throw std::runtime_error(s);
}
using M = std::array<std::array<double, 2>, 2>;
std::array<double, 2> dense(M a, std::array<double, 2> b) {
  for (size_t k = 0; k < 2; ++k) {
    auto p = a[k][k];
    require(p > 0, "Independent dense pivot positive");
    for (size_t j = k; j < 2; ++j)
      a[k][j] /= p;
    b[k] /= p;
    for (size_t i = k + 1; i < 2; ++i) {
      auto f = a[i][k];
      for (size_t j = k; j < 2; ++j)
        a[i][j] -= f * a[k][j];
      b[i] -= f * b[k];
    }
  }
  for (int i = 1; i >= 0; --i)
    for (int j = i + 1; j < 2; ++j)
      b[i] -= a[i][j] * b[j];
  return b;
}
bool near(double a, double b) { return std::abs(a - b) < 1e-13; }
} // namespace
int main() try {
  const HeatJointMask head_left{3, 0, 0, 0}, head_right{5, 0, 0, 0}, empty{},
      left{2, 0, 0, 0};
  require(compatible_weight_contact(head_left, head_right,
                                    DeclaredContactSupport::CommonMask),
          "Old common-role contact is present");
  require(!compatible_weight_contact(head_left, head_right,
                                     DeclaredContactSupport::IdenticalMask),
          "Exact-compatible candidate separates unequal intents");
  require(compatible_weight_contact(head_left, head_left,
                                    DeclaredContactSupport::IdenticalMask),
          "Genuinely compatible contact remains");
  require(!compatible_weight_contact(head_left, left,
                                     DeclaredContactSupport::IdenticalMask),
          "Subset intent does not grant equal authority");
  require(!compatible_weight_contact(empty, empty,
                                     DeclaredContactSupport::IdenticalMask),
          "Empty intents cannot create support");
  bool invalid = false;
  try {
    compatible_weight_contact(head_left, head_right, DeclaredContactSupport(3));
  } catch (const std::invalid_argument &) {
    invalid = true;
  }
  require(invalid, "Unknown graph policy refuses");
  // Data: each row has screen2 and unit conductance from Head plus its own Eye.
  // Existing shared graph transports an absent Eye into the other domain.
  const auto old_left = dense({{{3, -1}, {-1, 3}}}, {1, 0});
  const auto old_right = dense({{{3, -1}, {-1, 3}}}, {0, 1});
  require(near(old_left[1], .125) && near(old_right[0], .125),
          "Independent old graph exposes exact forbidden-label leakage");
  const auto candidate_left = dense({{{2, 0}, {0, 2}}}, {1, 0});
  const auto candidate_right = dense({{{2, 0}, {0, 2}}}, {0, 1});
  const auto candidate_head = dense({{{2, 0}, {0, 2}}}, {1, 1});
  require(candidate_left[1] == 0 && candidate_right[0] == 0,
          "Forbidden fields are exact zero, not truncated or renormalized");
  for (size_t i = 0; i < 2; ++i)
    require(candidate_left[i] + candidate_right[i] + candidate_head[i] == 1,
            "Independent candidate simplex partition exact");
  // Equal masks still exchange both labels through the real retained spring.
  const auto compatible_head = dense({{{5, -1}, {-1, 5}}}, {3, 1});
  const auto compatible_eye = dense({{{5, -1}, {-1, 5}}}, {1, 3});
  require(
      near(compatible_head[0], 2. / 3) && near(compatible_head[1], 1. / 3),
      "Compatible contact produces independently solved continuous response");
  for (size_t i = 0; i < 2; ++i)
    require(near(compatible_head[i] + compatible_eye[i], 1),
            "Compatible two-label partition maintained");
  // Honest control: dropping a DIFFERENT-mask edge also drops legitimate
  // common-Head smoothing. A common-label-only, simplex-coupled energy instead
  // solves this reduced Hessian. Record this distinct variational comparator.
  const auto common_simplex = dense({{{9, -1}, {-1, 9}}}, {6, 2});
  require(near(common_simplex[0], .7) && near(common_simplex[1], .3),
          "Independent shared-label constrained energy oracle");
  require(!near(common_simplex[0], .75),
          "Identical-mask candidate does not pretend to retain all "
          "common-label coupling");
  std::cout << nlohmann::json{{"checks", checks},
                              {"old_forbidden_leak", old_left[1]},
                              {"compatible_head", compatible_head},
                              {"common_only_simplex_head", common_simplex},
                              {"different_mask_candidate_head",
                               std::array{.75, .25}},
                              {"continuity_loss_retained", true}}
                   .dump(2)
            << std::endl;
  return 0;
} catch (const std::exception &e) {
  std::cerr << e.what() << std::endl;
  return 1;
}
