// SPDX-License-Identifier: GPL-3.0-or-later
#include "weight-declared-support.hpp"
#include <iostream>
using namespace dome;
int main() try {
  const std::function<bool()> never = [] { return false; };
  detail::weight_surface::Budget work{0, 50000000, never};
  Skeleton rig;
  rig.bones = {{"native authored handle",
                -1,
                {{0, 0, 0}, {.70710678f, 0, 0, .70710678f}},
                .5f,
                true},
               {"nondeform fork A", 0, {{1, 0, 0}}, .1f, false},
               {"nondeform fork B", 0, {{0, 0, 1}}, .1f, false},
               {"native leaf", -1, {{1, 0, 0}}, 2, true}};
  auto profile = detail::declared_weight_profile(
      rig, work, detail::DeclaredRigGeometry::NativeBoneAxes);
  if (profile.segments.size() != 2 ||
      profile.eligible != std::vector<uint8_t>{1, 0, 0, 1})
    throw std::runtime_error("Actual native deform flags changed");
  const auto &native = profile.segments[0];
  if (std::hypot(native.tail[0] + .5, native.tail[1], native.tail[2]) > 1e-6)
    throw std::runtime_error(
        "Native Y/length tail was replaced with unrelated child geometry");
  const auto &leaf = profile.segments[1];
  if (leaf.head != std::array<double, 3>{1, 0, 0} ||
      leaf.tail != std::array<double, 3>{1, 2, 0})
    throw std::runtime_error(
        "Native leaf lost its authored physical handle length");
  auto child_control = detail::declared_weight_profile(
      rig, work, detail::DeclaredRigGeometry::ChildHeadControl);
  if (child_control.segments.size() != 3 ||
      child_control.segments[0].tail == native.tail ||
      child_control.segments.back().tail != child_control.segments.back().head)
    throw std::runtime_error(
        "Independent old child-head/point controls are not sensitive");
  auto matrix = AffineTransform{}.float_matrix();
  matrix[0] = 2;
  matrix[4] = .5f;
  matrix[5] = .5f;
  rig.space_matrix = matrix;
  auto metric = detail::declared_weight_profile(
      rig, work, detail::DeclaredRigGeometry::NativeBoneAxes);
  const auto &tail = metric.segments[1];
  const double length =
      std::hypot(tail.tail[0] - tail.head[0], tail.tail[1] - tail.head[1],
                 tail.tail[2] - tail.head[2]);
  if (std::abs(length - std::sqrt(2.)) > 1e-6 || length == rig.bones[3].length)
    throw std::runtime_error(
        "Physical affine handle length incorrectly used native scalar length");
  std::cout << "PASS native_axis_fork_leaf_flags_worldmetric=1 "
               "child_head_negative=1 work="
            << work.used << '\n';
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
