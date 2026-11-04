// SPDX-License-Identifier: GPL-3.0-or-later
#include "anatomical-continuation.hpp"
#include <iostream>
#include <map>
using namespace dome;
namespace {
Scene fixture(int subdivision, bool alternate) {
  const int nx = 22 * subdivision, ny = 100 * subdivision,
            leg = 4 * subdivision, base = 4 * subdivision;
  const float h = .01f / subdivision;
  Mesh m;
  std::map<std::pair<int, int>, uint32_t> vertices;
  auto vertex = [&](int x, int y) {
    auto [at, inserted] =
        vertices.try_emplace({x, y}, uint32_t(m.positions.size()));
    if (inserted) {
      m.positions.push_back({x * h, y * h, 0});
      m.normals.push_back({0, 0, 1});
      m.uv.push_back({float(double(x + nx) / (2 * nx)), float(double(y) / ny)});
    }
    return at->second;
  };
  for (int y = 0; y < ny; ++y)
    for (int x = -nx; x < nx; ++x) {
      if (y >= base && x >= -nx + leg && x < nx - leg)
        continue;
      const auto a = vertex(x, y), b = vertex(x + 1, y),
                 c = vertex(x + 1, y + 1), d = vertex(x, y + 1);
      if (alternate && ((x + y) & 1))
        m.indices.insert(m.indices.end(), {a, b, d, b, c, d});
      else
        m.indices.insert(m.indices.end(), {a, b, c, a, c, d});
    }
  Scene s;
  s.active().name = "Connected open U surface";
  s.active().painted = std::make_shared<PaintedMesh>(m, 64);
  s.active().mesh_active = true;
  DeformationState state(m);
  state.skeleton =
      Skeleton{{{"authored left", -1, {{-.2f, 0, -.05f}}, 1, true},
                {"authored right", -1, {{.2f, 0, -.05f}}, 1, true}}};
  s.set_deformation(1, state);
  return s;
}
} // namespace
int main() try {
  bool all = true;
  for (int refinement : {1, 2})
    for (bool alternate : {false, true}) {
      auto s = fixture(refinement, alternate);
      const auto &m = s.active().painted->mesh();
      detail::DeclaredWeightSupport domain{
          detail::DeclaredSupportPolicy::NearestSourceUnion,
          true,
          {{1, {}}},
          1};
      domain.row_anchors = detail::DeclaredRowAnchors::NearestPermitted;
      const auto start = std::chrono::steady_clock::now();
      auto result = detail::anatomical_continuation(
          s, 0, {}, detail::BoneWallDomain::ConnectedSourceSurface,
          {500000000, 256u * 1024u * 1024u}, &domain);
      auto d = *s.active().deformation;
      SkinWeights skin;
      double maximum = 0, mean = 0;
      size_t evaluated = 0;
      for (size_t raw = 0; raw < m.positions.size(); ++raw) {
        const auto row = result.parts[0].raw_to_row[raw];
        VertexWeights weights;
        for (size_t j = 0; j < result.joints.size(); ++j)
          if (result.fields[row * result.joints.size() + j] > 0)
            weights.push_back(
                {result.joints[j],
                 float(result.fields[row * result.joints.size() + j])});
        skin.vertices.push_back(normalize_weights(std::move(weights), 2));
        if (m.positions[raw].y >= .5f) {
          const auto own = m.positions[raw].x < 0 ? 0 : 1;
          const double wrong = 1 - result.fields[row * 2 + own];
          maximum = std::max(maximum, wrong);
          mean += wrong;
          ++evaluated;
        }
      }
      mean /= evaluated;
      d.skin = skin;
      for (auto bone : d.skeleton->bones)
        d.pose.push_back(bone.rest);
      d.pose[0].translation.x -= .1f;
      d.pose[1].translation.x += .1f;
      auto posed = d.preview(m);
      double pose_max = 0, pose_mean = 0;
      for (size_t i = 0; i < m.positions.size(); ++i)
        if (m.positions[i].y >= .5f) {
          auto expected = m.positions[i];
          expected.x += expected.x < 0 ? -.1f : .1f;
          double e = length(posed.positions[i] - expected);
          pose_max = std::max(pose_max, e);
          pose_mean += e;
        }
      pose_mean /= evaluated;
      const bool pass = maximum <= .03 && mean <= .01 && pose_max <= .006 &&
                        pose_mean <= .002;
      all &= pass;
      std::cout << "OPEN_U refinement=" << refinement
                << " alternate=" << alternate
                << " vertices=" << m.positions.size()
                << " max_wrong=" << maximum << " mean_wrong=" << mean
                << " pose_max_m=" << pose_max << " pose_mean_m=" << pose_mean
                << " work=" << result.report.work
                << " bytes=" << result.report.bytes << " seconds="
                << std::chrono::duration<double>(
                       std::chrono::steady_clock::now() - start)
                       .count()
                << " accepted_bounds=" << pass << std::endl;
    }
  return all ? 0 : 1;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
