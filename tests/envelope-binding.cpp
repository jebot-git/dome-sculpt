// SPDX-License-Identifier: GPL-3.0-or-later
#include "envelope-binding.hpp"
#include <iostream>
using namespace dome;
using namespace dome::detail;
int main() try {
  double worst = 0;
  size_t cases = 0;
  for (unsigned dimensions : {2u, 3u})
    for (double scale : {.01, 1., 100.})
      for (double angle : {0., .37, 1.2}) {
        EnvelopeMatrix a =
            dimensions == 2 ? EnvelopeMatrix{4, 1, 0, 1, 9, 0, 0, 0, 0}
                            : EnvelopeMatrix{4, 1, .3, 1, 9, -.4, .3, -.4, 16};
        const double c = std::cos(angle), s = std::sin(angle);
        EnvelopeMatrix rotation{c, -s, 0, s, c, 0, 0, 0, 1}, m{};
        for (int i = 0; i < 3; ++i)
          for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k)
              for (int l = 0; l < 3; ++l)
                m[i * 3 + j] += rotation[i * 3 + k] * a[k * 3 + l] *
                                rotation[j * 3 + l] / (scale * scale);
        auto d = envelope_directions(dimensions);
        std::vector<double> radii;
        for (auto v : d) {
          long double value = 0;
          for (unsigned i = 0; i < dimensions; ++i)
            for (unsigned j = 0; j < dimensions; ++j)
              value += (long double)v[i] * m[i * 3 + j] * v[j];
          radii.push_back(1 / std::sqrt(double(value)));
        }
        auto fit = fit_envelope(d, radii, dimensions);
        for (unsigned i = 0; i < dimensions; ++i)
          for (unsigned j = 0; j < dimensions; ++j)
            worst = std::max(worst,
                             std::abs(fit.precision[i * 3 + j] - m[i * 3 + j]) /
                                 std::max(1., std::abs(m[i * 3 + j])));
        if (fit.projected_eigenvalues || fit.relative_rms > 1e-12)
          throw std::runtime_error("Exact ellipsoid unexpectedly required "
                                   "projection or lost radial geometry");
        ++cases;
      }
  if (worst > 1e-11)
    throw std::runtime_error(
        "Analytic rotated/scaled ellipsoid metric mismatch");
  Mesh mesh;
  mesh.positions = {{0, 0, 0}, {-.5f, 0, 0}, {0, 1, 0}};
  mesh.normals.assign(3, {0, 0, 1});
  mesh.uv.assign(3, {0, 0});
  mesh.indices = {0, 1, 2};
  Skeleton rig;
  rig.bones.resize(2);
  rig.bones[0].name = "A";
  rig.bones[1].name = "B";
  std::array<BoneEnvelope, 2> e{
      {{{{-1, 0, 0}, {-1, 0, 0}, 0}, {1, 0, 0, 0, 1, 0, 0, 0, 1}},
       {{{1, 0, 0}, {1, 0, 0}, 1}, {1, 0, 0, 0, 1, 0, 0, 0, 1}}}};
  auto result = bind_envelopes(mesh, rig, e, {2, 0});
  auto weight = [](const VertexWeights &w) {
    for (auto i : w)
      if (i.joint == 0)
        return double(i.weight);
    return 0.;
  };
  if (std::abs(weight(result.vertices[0]) - .5) > 1e-7 ||
      std::abs(weight(result.vertices[1]) - 81. / 82) > 1e-7)
    throw std::runtime_error("Envelope weights differ from independent "
                             "inverse-fourth/simplex solution");
  for (double scale : {.01, 100.}) {
    auto transformed = mesh;
    auto next = e;
    for (auto &p : transformed.positions)
      p = p * float(scale);
    for (auto &v : next) {
      for (auto &x : v.source.head)
        x *= scale;
      for (auto &x : v.source.tail)
        x *= scale;
      for (auto &x : v.precision)
        x /= scale * scale;
    }
    auto scaled = bind_envelopes(transformed, rig, next, {2, 0});
    for (size_t i = 0; i < 3; ++i)
      if (std::abs(weight(result.vertices[i]) - weight(scaled.vertices[i])) >
          1e-7)
        throw std::runtime_error(
            "Envelope assignment changed under coherent unit scaling");
  }
  auto forged = e;
  forged[0].precision[0] = -1;
  bool refused = false;
  try {
    bind_envelopes(mesh, rig, forged);
  } catch (const std::invalid_argument &) {
    refused = true;
  }
  if (!refused)
    throw std::runtime_error("Forged indefinite metric admitted");
  EnvelopeBindReport report{173, 19};
  bool stopped = false;
  try {
    bind_envelopes(mesh, rig, e, {}, [] { return true; }, &report);
  } catch (const std::runtime_error &) {
    stopped = true;
  }
  if (!stopped || report.work != 173 || report.vertices != 19)
    throw std::runtime_error("Cancellation published phantom envelope result");
  std::cout << "PASS analytic_ellipsoids=" << cases << " metric_error=" << worst
            << " simplex/endpoint/scaling/forged-metric/cancel controls\n";
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
