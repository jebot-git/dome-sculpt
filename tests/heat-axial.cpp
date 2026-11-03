// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/heat-binding.cpp"
#include "heat-axial-source.hpp"
#include <iostream>
using namespace dome;
using namespace dome::detail;
int main() try {
  Mesh m;
  m.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
  m.normals.assign(3, {0, 0, 1});
  m.uv.assign(3, {0, 0});
  m.indices = {0, 1, 2};
  Skeleton rig;
  rig.bones.resize(4);
  for (int i = 0; i < 4; ++i)
    rig.bones[i].name = std::to_string(i);
  const std::array<HeatSourceSegment, 4> sources{
      {{{-1, 0, -.1}, {0, 0, -.1}, 0},
       {{0, 0, -.1}, {1, 0, -.1}, 1},
       {{0, 0, -.2}, {0, 1, -.2}, 2},
       {{0, 0, -.5}, {0, 0, -.5}, 3}}};
  rig.bones[1].parent = 0;
  rig.bones[2].parent = 1;
  HumanoidMapping mapping;
  mapping.joints = {{"hips", 0}, {"spine", 1}, {"leftShoulder", 2}};
  const std::array<uint32_t, 3> primary{0, 1, 3};
  HeatBindingOptions o;
  o.operation = HeatOperator::BoundedFiniteElements;
  o.visibility = HeatVisibility::EntryFaces;
  o.seed_rule = HeatSeedRule::ReferenceSegments;
  o.maximum_seed_distance = 1000;
  const auto a = make_heat_handles(sources, primary, 4,
                                   HeatHandlePolicy::OriginalSegments, o),
             b = make_heat_handles(sources, primary, 4,
                                   HeatHandlePolicy::PrimaryMidpoints, o);
  const auto typed = make_axial_heat_handles(sources, rig, mapping, o);
  auto same = [](const HeatSourceSegment &x, const HeatSourceSegment &y) {
    return x.bone == y.bone && x.head == y.head && x.tail == y.tail;
  };
  if (!same(typed.handles[0], sources[0]) ||
      !same(typed.handles[1], sources[1]) ||
      typed.handles[2].head != D{0, .5, -.2} ||
      typed.handles[2].head != typed.handles[2].tail ||
      !same(typed.handles[3], sources[3]) || typed.midpoint_sources != 1)
    throw std::runtime_error("Typed axial/limb/auxiliary geometry differs");
  if (b.midpoint_sources != 2 || b.handles[0].head != D{-.5, 0, -.1} ||
      b.handles[0].head != b.handles[0].tail ||
      !same(b.handles[2], sources[2]) || !same(b.handles[3], sources[3]))
    throw std::runtime_error(
        "Mixed-role centroid construction altered point/auxiliary authority");
  for (size_t i = 0; i < 4; ++i)
    if (!same(a.handles[i], sources[i]))
      throw std::runtime_error("Original-segment control changed");
  std::function<bool()> cancel;
  auto get = [&](const Mesh &mesh, std::span<const HeatSourceSegment> handles) {
    Budget budget{0, 100000, cancel};
    auto s = assemble(mesh, budget, o.operation, {});
    seed(s, mesh, rig, o, budget, handles, {});
    return s;
  };
  auto old = get(m, a.handles), next = get(m, b.handles),
       typed_field = get(m, typed.handles);
  if (typed_field.seeds[0] != old.seeds[0] ||
      typed_field.heat[0] != old.heat[0])
    throw std::runtime_error(
        "Typed axial source/H differs from analytic segment control");
  const long double old_distance = .1L / 1.0005L, new_distance = .2L / 1.0005L;
  if (old.ties[0] != 2 || !(old.seeds[0][0] & 1) || !(old.seeds[0][0] & 2) ||
      next.ties[0] != 1 || !(next.seeds[0][0] & 4) ||
      std::abs(old.heat[0] / ((1.L / 6) * 2 / (old_distance * old_distance)) -
               1) > 2e-14L ||
      std::abs(next.heat[0] / ((1.L / 6) / (new_distance * new_distance)) - 1) >
          2e-14L)
    throw std::runtime_error("Endpoint/junction H differs from independent "
                             "analytic point/segment oracle");
  for (double scale : {.01, 100.})
    for (double angle : {0., .37, 1.2}) {
      const double c = std::cos(angle), s = std::sin(angle);
      auto transform = [&](D p) {
        return D{scale * (c * p[0] + s * p[2]), scale * p[1],
                 scale * (-s * p[0] + c * p[2])};
      };
      auto transformed = sources;
      auto mesh = m;
      for (auto &source : transformed) {
        source.head = transform(source.head);
        source.tail = transform(source.tail);
      }
      for (auto &p : mesh.positions)
        p = vec(transform(point(p)));
      auto handles = make_heat_handles(transformed, primary, 4,
                                       HeatHandlePolicy::PrimaryMidpoints, o);
      for (size_t i = 0; i < 4; ++i) {
        auto expected = transform(b.handles[i].head);
        for (int k = 0; k < 3; ++k)
          if (std::abs(handles.handles[i].head[k] - expected[k]) >
              1e-12 * scale)
            throw std::runtime_error(
                "Centroid failed affine rotation/unit-scale commutation");
      }
      auto typed_next = make_axial_heat_handles(transformed, rig, mapping, o);
      for (size_t i = 0; i < 4; ++i) {
        const auto head = transform(typed.handles[i].head),
                   tail = transform(typed.handles[i].tail);
        for (int k = 0; k < 3; ++k)
          if (std::abs(typed_next.handles[i].head[k] - head[k]) >
                  1e-12 * scale ||
              std::abs(typed_next.handles[i].tail[k] - tail[k]) > 1e-12 * scale)
            throw std::runtime_error(
                "Typed role geometry changed under units/rotation");
      }
      auto result = get(mesh, handles.handles);
      if (result.seeds[0] != next.seeds[0] ||
          std::abs(result.heat[0] / next.heat[0] - 1) > 2e-6)
        throw std::runtime_error(
            "Mixed-role choice/H changed under coherent scale/rotation");
    }
  auto short_work = o;
  short_work.maximum_work = b.work - 1;
  bool refused = false;
  try {
    make_heat_handles(sources, primary, 4, HeatHandlePolicy::PrimaryMidpoints,
                      short_work);
  } catch (const std::length_error &) {
    refused = true;
  }
  if (!refused)
    throw std::runtime_error("Handle work preflight ignored");
  auto short_bytes = o;
  short_bytes.maximum_bytes = b.planned_bytes - 1;
  refused = false;
  try {
    make_heat_handles(sources, primary, 4, HeatHandlePolicy::PrimaryMidpoints,
                      short_bytes);
  } catch (const std::length_error &) {
    refused = true;
  }
  if (!refused)
    throw std::runtime_error("Handle byte preflight ignored");
  refused = false;
  try {
    make_heat_handles(sources, primary, 4, HeatHandlePolicy::PrimaryMidpoints,
                      o, [] { return true; });
  } catch (const std::runtime_error &) {
    refused = true;
  }
  if (!refused)
    throw std::runtime_error("Handle cancellation ignored");
  for (int control = 0; control < 4; ++control) {
    auto limit = o;
    if (control == 0)
      limit.maximum_work = typed.work - 1;
    if (control == 1)
      limit.maximum_bytes = typed.planned_bytes - 1;
    auto roles = mapping;
    if (control == 3)
      roles.joints["neck"] = 0;
    bool failed = false;
    try {
      make_axial_heat_handles(sources, rig, roles, limit,
                              [control] { return control == 2; });
    } catch (const std::exception &) {
      failed = true;
    }
    if (!failed)
      throw std::runtime_error("Typed work/bytes/cancel/role guard failed");
  }
  std::cout << "PASS typed axial/limb/auxiliary and independent mixed-role "
               "centroid/unchanged-point/auxiliary "
               "geometry, endpoint-H, six scale/rotation, work/bytes/cancel "
               "controls\n";
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
