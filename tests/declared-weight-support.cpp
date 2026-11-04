// SPDX-License-Identifier: GPL-3.0-or-later
#include "anatomical-continuation.hpp"
#include "dome/creation.hpp"
#include "dome/source-placement.hpp"
#include <iostream>
using namespace dome;
int main() try {
  Scene s;
  Mesh m;
  for (float center : {-.2f, .2f}) {
    const uint32_t b = m.positions.size();
    m.positions.insert(m.positions.end(), {{center - .02f, 0, 0},
                                           {center + .02f, 0, 0},
                                           {center + .02f, 1, 0},
                                           {center - .02f, 1, 0}});
    m.normals.insert(m.normals.end(), 4, {0, 0, 1});
    const float u = center < 0 ? .01f : .51f;
    m.uv.insert(m.uv.end(),
                {{u, .01f}, {u + .48f, .01f}, {u + .48f, .99f}, {u, .99f}});
    m.indices.insert(m.indices.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
  }
  s.active().painted = std::make_shared<PaintedMesh>(m, 64);
  s.active().mesh_active = true;
  DeformationState d(m);
  Skeleton rig;
  rig.bones = {{"authored left", -1, {{-.2f, 0, -.05f}}, 1, true},
               {"left endpoint", 0, {{0, 1, 0}}, .1f, false},
               {"authored right", -1, {{.2f, 0, -.05f}}, 1, true},
               {"right endpoint", 2, {{0, 1, 0}}, .1f, false}};
  d.skeleton = rig;
  s.set_deformation(1, d);
  const auto before = s.encode();
  detail::DeclaredWeightSupport input{
      detail::DeclaredSupportPolicy::NearestSourceUnion, false, {{1, {}}}};
  auto out = detail::anatomical_continuation(
      s, 0, {}, detail::BoneWallDomain::ConnectedSourceSurface,
      {500'000'000, 256u * 1024u * 1024u}, &input);
  if (out.parts.size() != 1 || out.joints != std::vector<uint32_t>{0, 2} ||
      out.report.rows != 8 || s.encode() != before)
    throw std::runtime_error(
        "Actual unskinned input/roles/complete Scene not preserved");
  double minimum = 1;
  for (size_t i = 0; i < 8; ++i) {
    const auto row = out.parts[0].raw_to_row[i];
    const double own = out.fields[row * 2 + (i >= 4)];
    minimum = std::min(minimum, own);
    if (!(own > .9))
      throw std::runtime_error(
          "Default geometry/deform support cross-limb quality failed own=" +
          std::to_string(own));
  }
  const auto base_fields = out.fields;
  double metric_sensitivity = 0;
  for (bool reflected : {false, true}) {
    auto placed = s;
    auto matrix = AffineTransform{}.matrix();
    matrix[0] = reflected ? -2 : 2;
    matrix[4] = .5;
    matrix[5] = .5;
    matrix[10] = 1.5;
    matrix[12] = 100000000;
    matrix[13] = -200000000;
    placed.active().affine_edit =
        std::make_shared<const NativeAffineEdit>(AffineTransform(matrix));
    const auto placed_before = placed.encode();
    input.world_metric = true;
    auto actual = detail::anatomical_continuation(
        placed, 0, {}, detail::BoneWallDomain::ConnectedSourceSurface,
        {500'000'000, 256u * 1024u * 1024u}, &input);
    double raw_delta = 0;
    for (size_t i = 0; i < actual.fields.size(); ++i)
      raw_delta =
          std::max(raw_delta, std::abs(actual.fields[i] - base_fields[i]));
    std::cout << "AFFINE_RAW_DIAGNOSTIC reflected=" << reflected
              << " raw_delta=" << raw_delta
              << " scene_exact=" << (placed.encode() == placed_before)
              << std::endl;
    if (placed.encode() != placed_before)
      throw std::runtime_error("Actual common-affine source Scene changed");
    for (size_t raw = 0; raw < 8; ++raw) {
      const auto row = actual.parts[0].raw_to_row[raw];
      VertexWeights values;
      for (size_t j = 0; j < actual.joints.size(); ++j)
        if (actual.fields[row * actual.joints.size() + j] > 0)
          values.push_back(
              {actual.joints[j],
               float(actual.fields[row * actual.joints.size() + j])});
      auto stored = normalize_weights(std::move(values), 4, {}, WeightLimits{});
      if (stored != VertexWeights{{raw < 4 ? 0u : 2u, 1.f}})
        throw std::runtime_error("Actual common-affine emitted fixed4 field "
                                 "differs from known unskinned limb identity");
    }
    // Independent explicit coordinate bake, not skin_world_mesh/reframe
    // helpers.
    auto linear = matrix;
    linear[12] = linear[13] = linear[14] = 0;
    auto baked = s;
    auto transformed = m;
    auto point = [&](Vec3 p) {
      return Vec3{float(linear[0] * p.x + linear[4] * p.y + linear[8] * p.z),
                  float(linear[1] * p.x + linear[5] * p.y + linear[9] * p.z),
                  float(linear[2] * p.x + linear[6] * p.y + linear[10] * p.z)};
    };
    for (auto &p : transformed.positions)
      p = point(p);
    if (reflected)
      for (size_t i = 0; i < transformed.indices.size(); i += 3)
        std::swap(transformed.indices[i + 1], transformed.indices[i + 2]);
    baked.active().painted = std::make_shared<PaintedMesh>(transformed, 64);
    DeformationState rd(transformed);
    rd.skeleton = rig;
    rd.skeleton->space_matrix = AffineTransform(linear).float_matrix();
    baked.set_deformation(1, rd);
    input.policy = detail::DeclaredSupportPolicy::AllDeformControls;
    input.world_metric = false;
    auto reference = detail::anatomical_continuation(
        baked, 0, {}, detail::BoneWallDomain::ConnectedSourceSurface,
        {500'000'000, 256u * 1024u * 1024u}, &input);
    input.world_metric = true;
    auto control = detail::anatomical_continuation(
        placed, 0, {}, detail::BoneWallDomain::ConnectedSourceSurface,
        {500'000'000, 256u * 1024u * 1024u}, &input);
    if (reference.fields.size() != control.fields.size())
      throw std::runtime_error("Affine reference row dimensions changed");
    for (size_t i = 0; i < control.fields.size(); ++i)
      if (std::abs(reference.fields[i] - control.fields[i]) > 2e-12)
        throw std::runtime_error("Actual full-affine metric differs from "
                                 "independent coordinate-baked field");
    input.world_metric = false;
    auto local_control = detail::anatomical_continuation(
        s, 0, {}, detail::BoneWallDomain::ConnectedSourceSurface,
        {500'000'000, 256u * 1024u * 1024u}, &input);
    for (size_t i = 0; i < control.fields.size(); ++i)
      metric_sensitivity =
          std::max(metric_sensitivity,
                   std::abs(control.fields[i] - local_control.fields[i]));
    input.policy = detail::DeclaredSupportPolicy::NearestSourceUnion;
  }
  if (!(metric_sensitivity > 1e-4))
    throw std::runtime_error("Wrong local metric negative has no sensitivity");
  std::cout << "PASS unskinned_open_disconnected_default=1 rows="
            << out.report.rows << " minimum_own=" << minimum
            << " work=" << out.report.work << " bytes=" << out.report.bytes
            << " metric_wrong_sensitivity=" << metric_sensitivity
            << " residual=" << out.report.residual
            << " partition=" << out.report.partition << '\n';
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
