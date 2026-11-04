// SPDX-License-Identifier: GPL-3.0-or-later
#include "anatomical-continuation.hpp"
#include <iostream>
using namespace dome;
Scene fixture(float gap, bool reverse) {
  Mesh m;
  const float w = .001f, center = (w + gap) / 2;
  for (size_t side = 0; side < 2; ++side) {
    const float x = side ? center : -center;
    const uint32_t first = m.positions.size();
    m.positions.insert(m.positions.end(), {{x - w / 2, 0, 0},
                                           {x + w / 2, 0, 0},
                                           {x + w / 2, 1, 0},
                                           {x - w / 2, 1, 0}});
    m.normals.insert(m.normals.end(), 4, Vec3{0, 0, 1});
    const float lo = side ? .6f : 0, hi = side ? 1 : .4f;
    m.uv.insert(m.uv.end(), {{lo, 0}, {hi, 0}, {hi, 1}, {lo, 1}});
    m.indices.insert(m.indices.end(), {first, first + 1, first + 2, first,
                                       first + 2, first + 3});
  }
  if (reverse)
    for (size_t c = 0; c < m.indices.size(); c += 3)
      std::swap(m.indices[c + 1], m.indices[c + 2]);
  Scene s;
  s.active().painted = std::make_shared<PaintedMesh>(m, 64);
  s.active().mesh_active = true;
  DeformationState d(m);
  d.skeleton =
      Skeleton{{{"left native finite", -1, {{-center, 0, -.0001f}}, 1, true},
                {"right native finite", -1, {{center, 0, -.0001f}}, 1, true}}};
  s.set_deformation(1, d);
  return s;
}
int main() try {
  bool all = true;
  for (float gap : {.01f, .0001f, .000001f, 0.f})
    for (bool reverse : {false, true}) {
      auto s = fixture(gap, reverse);
      const auto before = s.encode();
      detail::DeclaredWeightSupport support{
          detail::DeclaredSupportPolicy::UniqueSourceWitnesses,
          true,
          {{1, {}}},
          1};
      support.row_anchors = detail::DeclaredRowAnchors::NearestPermitted;
      const auto f = detail::anatomical_continuation(
          s, 0, {}, detail::BoneWallDomain::ConnectedSourceSurface,
          {500000000, 256u * 1024u * 1024u}, &support);
      const auto &m = s.active().painted->mesh();
      auto d = *s.active().deformation;
      d.skin = SkinWeights{};
      double maxwrong = 0, meanwrong = 0;
      for (size_t raw = 0; raw < m.positions.size(); ++raw) {
        const auto row = f.parts[0].raw_to_row[raw];
        const size_t own = raw / 4;
        const double wrong = 1 - f.fields[row * 2 + own];
        maxwrong = std::max(maxwrong, wrong);
        meanwrong += wrong;
        VertexWeights values;
        for (size_t j = 0; j < 2; ++j)
          values.push_back({uint32_t(j), float(f.fields[row * 2 + j])});
        d.skin->vertices.push_back(normalize_weights(std::move(values), 2));
      }
      for (const auto &b : d.skeleton->bones)
        d.pose.push_back(b.rest);
      d.pose[0].translation.x -= .1f;
      d.pose[1].translation.x += .1f;
      const auto p = d.preview(m);
      double maxpose = 0, meanpose = 0;
      for (size_t i = 0; i < m.positions.size(); ++i) {
        auto expected = m.positions[i];
        expected.x += i < 4 ? -.1f : .1f;
        double e = length(p.positions[i] - expected);
        maxpose = std::max(maxpose, e);
        meanpose += e;
      }
      meanwrong /= m.positions.size();
      meanpose /= m.positions.size();
      if (s.encode() != before || f.report.rows != 8)
        throw std::runtime_error(
            "Native disconnected geometry was welded/mutated");
      const bool pass = maxwrong <= .03 && meanwrong <= .01 &&
                        maxpose <= .006 && meanpose <= .002;
      all &= pass;
      std::cout << "CONTACT gap=" << gap << " reversed=" << reverse
                << " maxwrong=" << maxwrong << " meanwrong=" << meanwrong
                << " posemax_m=" << maxpose << " posemean_m=" << meanpose
                << " rows=" << f.report.rows
                << " contacts=" << f.report.contacts
                << " work=" << f.report.work << " accepted_bounds=" << pass
                << std::endl;
    }
  return all ? 0 : 1;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
