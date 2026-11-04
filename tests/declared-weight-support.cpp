// SPDX-License-Identifier: GPL-3.0-or-later
#include "anatomical-continuation.hpp"
#include "dome/creation.hpp"
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
  detail::DeclaredWeightSupport input{{{1, {}}}};
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
  std::cout << "PASS unskinned_open_disconnected_default=1 rows="
            << out.report.rows << " minimum_own=" << minimum
            << " work=" << out.report.work << " bytes=" << out.report.bytes
            << " residual=" << out.report.residual
            << " partition=" << out.report.partition << '\n';
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
