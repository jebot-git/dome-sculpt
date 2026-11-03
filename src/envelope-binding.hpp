// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "heat-binding.hpp"
#include <bit>
#include <limits>
#include <numbers>
namespace dome::detail {
using EnvelopeVector = std::array<double, 3>;
using EnvelopeMatrix = std::array<double, 9>;
struct EnvelopeFit {
  EnvelopeMatrix precision{};
  double relative_rms = 0;
  size_t projected_eigenvalues = 0;
};
inline std::vector<EnvelopeVector> envelope_directions(unsigned dimensions) {
  std::vector<EnvelopeVector> out;
  if (dimensions == 2) {
    for (int k = 0; k < 16; ++k) {
      double a = 2 * std::numbers::pi * k / 16;
      out.push_back({std::cos(a), std::sin(a), 0});
    }
  } else if (dimensions == 3) {
    for (int x = -1; x <= 1; ++x)
      for (int y = -1; y <= 1; ++y)
        for (int z = -1; z <= 1; ++z)
          if (x || y || z) {
            double length = std::sqrt(double(x * x + y * y + z * z));
            out.push_back({x / length, y / length, z / length});
          }
  } else
    throw std::invalid_argument("Envelope fit needs two or three dimensions");
  return out;
}
// Declared two-stage geometric model: radial least squares, then nearest
// symmetric metric in Frobenius norm with eigenvalues in [1e-6*lambdaMax,
// lambdaMax]. Projection is always part of this model, never an error fallback.
// Its residual/projection count is retained; this is not a volume certificate.
inline EnvelopeFit fit_envelope(std::span<const EnvelopeVector> directions,
                                std::span<const double> radii,
                                unsigned dimensions) {
  if ((dimensions != 2 && dimensions != 3) ||
      directions.size() != radii.size() || directions.size() < 6 ||
      directions.size() > 64)
    throw std::invalid_argument("Invalid bounded envelope fit dimensions");
  const unsigned n = dimensions == 2 ? 3 : 6;
  std::array<std::array<long double, 6>, 6> h{};
  std::array<long double, 6> rhs{};
  auto row = [&](EnvelopeVector d) {
    return dimensions == 2
               ? std::array<long double, 6>{d[0] * d[0],
                                            d[1] * d[1],
                                            2 * d[0] * d[1],
                                            0,
                                            0,
                                            0}
               : std::array<long double, 6>{d[0] * d[0],     d[1] * d[1],
                                            d[2] * d[2],     2 * d[0] * d[1],
                                            2 * d[0] * d[2], 2 * d[1] * d[2]};
  };
  for (size_t k = 0; k < radii.size(); ++k) {
    double r = radii[k];
    double length2 = 0;
    for (unsigned j = 0; j < dimensions; ++j)
      length2 += directions[k][j] * directions[k][j];
    if (!std::isfinite(length2) || std::abs(length2 - 1) > 1e-12)
      throw std::invalid_argument(
          "Envelope fit direction is not finite unit geometry");
    if (!(r > 0) || !std::isfinite(r))
      throw std::invalid_argument("Envelope has unresolved radial samples");
    auto v = row(directions[k]);
    long double y = 1 / (static_cast<long double>(r) * r);
    for (unsigned i = 0; i < n; ++i) {
      rhs[i] += v[i] * y;
      for (unsigned j = 0; j < n; ++j)
        h[i][j] += v[i] * v[j];
    }
  }
  for (unsigned k = 0; k < n; ++k) {
    unsigned pivot = k;
    for (unsigned i = k + 1; i < n; ++i)
      if (std::abs(h[i][k]) > std::abs(h[pivot][k]))
        pivot = i;
    if (std::abs(h[pivot][k]) < 1e-12L)
      throw std::invalid_argument(
          "Envelope sampling does not identify a quadratic metric");
    std::swap(h[k], h[pivot]);
    std::swap(rhs[k], rhs[pivot]);
    for (unsigned i = k + 1; i < n; ++i) {
      long double factor = h[i][k] / h[k][k];
      for (unsigned j = k; j < n; ++j)
        h[i][j] -= factor * h[k][j];
      rhs[i] -= factor * rhs[k];
    }
  }
  for (unsigned i = n; i--;) {
    for (unsigned j = i + 1; j < n; ++j)
      rhs[i] -= h[i][j] * rhs[j];
    rhs[i] /= h[i][i];
  }
  EnvelopeMatrix a{}, vectors{1, 0, 0, 0, 1, 0, 0, 0, 1};
  a[0] = double(rhs[0]);
  a[4] = double(rhs[1]);
  if (dimensions == 2)
    a[1] = a[3] = double(rhs[2]);
  else {
    a[8] = double(rhs[2]);
    a[1] = a[3] = double(rhs[3]);
    a[2] = a[6] = double(rhs[4]);
    a[5] = a[7] = double(rhs[5]);
  }
  bool converged = false;
  for (int iteration = 0; iteration < 64; ++iteration) {
    unsigned p = 0, q = 1;
    double off = 0, scale = 0;
    for (unsigned i = 0; i < dimensions; ++i) {
      scale = std::max(scale, std::abs(a[i * 3 + i]));
      for (unsigned j = i + 1; j < dimensions; ++j)
        if (std::abs(a[i * 3 + j]) > off) {
          off = std::abs(a[i * 3 + j]);
          p = i;
          q = j;
        }
    }
    if (off <=
        32 * std::numeric_limits<double>::epsilon() * std::max(scale, 1e-300)) {
      converged = true;
      break;
    }
    double angle =
               .5 * std::atan2(2 * a[p * 3 + q], a[q * 3 + q] - a[p * 3 + p]),
           c = std::cos(angle), s = std::sin(angle), ap = a[p * 3 + p],
           aq = a[q * 3 + q], cross = a[p * 3 + q];
    a[p * 3 + p] = c * c * ap - 2 * c * s * cross + s * s * aq;
    a[q * 3 + q] = s * s * ap + 2 * c * s * cross + c * c * aq;
    a[p * 3 + q] = a[q * 3 + p] = 0;
    for (unsigned k = 0; k < dimensions; ++k)
      if (k != p && k != q) {
        double x = a[k * 3 + p], y = a[k * 3 + q];
        a[k * 3 + p] = a[p * 3 + k] = c * x - s * y;
        a[k * 3 + q] = a[q * 3 + k] = s * x + c * y;
      }
    for (unsigned k = 0; k < dimensions; ++k) {
      double x = vectors[k * 3 + p], y = vectors[k * 3 + q];
      vectors[k * 3 + p] = c * x - s * y;
      vectors[k * 3 + q] = s * x + c * y;
    }
  }
  if (!converged)
    throw std::runtime_error(
        "Envelope eigensystem did not converge within its bound");
  double maximum = 0;
  for (unsigned i = 0; i < dimensions; ++i)
    maximum = std::max(maximum, a[i * 3 + i]);
  if (!(maximum > 0) || !std::isfinite(maximum))
    throw std::invalid_argument("Envelope has no finite positive metric");
  EnvelopeFit fit;
  for (unsigned k = 0; k < dimensions; ++k) {
    double value = std::max(a[k * 3 + k], maximum * 1e-6);
    fit.projected_eigenvalues += value != a[k * 3 + k];
    for (unsigned i = 0; i < dimensions; ++i)
      for (unsigned j = 0; j < dimensions; ++j)
        fit.precision[i * 3 + j] +=
            vectors[i * 3 + k] * value * vectors[j * 3 + k];
  }
  for (unsigned i = 0; i < dimensions; ++i)
    for (unsigned j = i + 1; j < dimensions; ++j)
      fit.precision[i * 3 + j] = fit.precision[j * 3 + i] =
          .5 * (fit.precision[i * 3 + j] + fit.precision[j * 3 + i]);
  for (size_t k = 0; k < radii.size(); ++k) {
    double predicted = 0;
    for (unsigned i = 0; i < dimensions; ++i)
      for (unsigned j = 0; j < dimensions; ++j)
        predicted +=
            directions[k][i] * fit.precision[i * 3 + j] * directions[k][j];
    double error = predicted * radii[k] * radii[k] - 1;
    fit.relative_rms += error * error;
  }
  fit.relative_rms = std::sqrt(fit.relative_rms / radii.size());
  if (!std::isfinite(fit.relative_rms))
    throw std::runtime_error("Envelope radial residual is nonfinite");
  return fit;
}
struct BoneEnvelope {
  HeatSourceSegment source;
  EnvelopeMatrix precision;
};
struct EnvelopeBindReport {
  size_t work = 0, vertices = 0;
};
// A separate geometric relative-entropy assignment: alpha=1/max(D^2,1e-8)^2,
// minimize sum w*log(w/alpha) on the simplex, then native prune/topK. No PDE,
// artist values, nearest-source fallback or volume-aware claim.
inline SkinWeights bind_envelopes(const Mesh &world, const Skeleton &rig,
                                  std::span<const BoneEnvelope> envelopes,
                                  WeightLimits limits = {},
                                  const std::function<bool()> &cancelled = {},
                                  EnvelopeBindReport *report = nullptr,
                                  size_t maximum_work = 500'000'000,
                                  size_t maximum_bytes = 256u * 1024u * 1024u) {
  if (cancelled && cancelled())
    throw std::runtime_error("Envelope binding cancelled");
  const size_t n = world.positions.size(), b = envelopes.size(),
               nt = world.indices.size() / 3;
  if (n > 2'000'000 || nt > 2'000'000 || !b || b > 256 || rig.bones.empty() ||
      rig.bones.size() > 256 || !limits.max_influences ||
      limits.max_influences > 32 || !maximum_work ||
      maximum_work > 500'000'000 || maximum_bytes > 512u * 1024u * 1024u ||
      maximum_bytes < 65536)
    throw std::invalid_argument(
        "Invalid envelope bind dimensions/resource caps");
  const size_t per_vertex =
      b * (64 + 4 * std::bit_width(limits.max_influences + 1)) + 64;
  const size_t base_work = nt * 16 + rig.bones.size() * rig.bones.size() * 32;
  if (base_work > maximum_work || n > (maximum_work - base_work) / per_vertex ||
      n > (maximum_bytes - 65536) /
              (sizeof(VertexWeights) +
               2 * (size_t(limits.max_influences) + 1) * sizeof(Influence)))
    throw std::length_error(
        "Envelope bind exceeds preconstruction work/byte cap");
  world.validate();
  rig.validate();
  normalize_weights({{0, 1}}, rig.bones.size(), {}, limits);
  std::array<bool, 256> seen{};
  for (const auto &e : envelopes) {
    if (e.source.bone >= rig.bones.size() || !rig.bones[e.source.bone].deform ||
        seen[e.source.bone])
      throw std::invalid_argument(
          "Envelope source identity is invalid, repeated or nondeforming");
    seen[e.source.bone] = true;
    for (double x : e.precision)
      if (!std::isfinite(x))
        throw std::invalid_argument("Nonfinite envelope metric");
    const auto &a = e.precision;
    for (int i = 0; i < 3; ++i)
      for (int j = i + 1; j < 3; ++j)
        if (a[i * 3 + j] != a[j * 3 + i])
          throw std::invalid_argument("Envelope metric is not symmetric");
    const long double det =
        (long double)a[0] *
            ((long double)a[4] * a[8] - (long double)a[5] * a[7]) -
        (long double)a[1] *
            ((long double)a[3] * a[8] - (long double)a[5] * a[6]) +
        (long double)a[2] *
            ((long double)a[3] * a[7] - (long double)a[4] * a[6]);
    if (!(a[0] > 0) ||
        !((long double)a[0] * a[4] - (long double)a[1] * a[3] > 0) ||
        !(det > 0) || !std::isfinite(det))
      throw std::invalid_argument("Envelope metric is not positive definite");
    for (double x : e.source.head)
      if (!std::isfinite(x))
        throw std::invalid_argument("Envelope source head is nonfinite");
    for (double x : e.source.tail)
      if (!std::isfinite(x))
        throw std::invalid_argument("Envelope source tail is nonfinite");
  }
  SkinWeights out;
  out.vertices.reserve(n);
  std::vector<double> scores(b);
  std::vector<Influence> ranked(b);
  for (auto p : world.positions) {
    if (cancelled && cancelled())
      throw std::runtime_error("Envelope binding cancelled");
    double largest = -INFINITY;
    for (size_t k = 0; k < b; ++k) {
      const auto &e = envelopes[k];
      EnvelopeVector delta, axis;
      double length2 = 0, numerator = 0;
      for (int c = 0; c < 3; ++c) {
        axis[c] = e.source.tail[c] - e.source.head[c];
        delta[c] = double(c == 0 ? p.x : c == 1 ? p.y : p.z) - e.source.head[c];
        length2 += axis[c] * axis[c];
        numerator += delta[c] * axis[c];
      }
      double t = length2 ? std::clamp(numerator / length2, 0., 1.) : 0;
      for (int c = 0; c < 3; ++c)
        delta[c] -= t * axis[c];
      double distance2 = 0;
      for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
          distance2 += delta[i] * e.precision[i * 3 + j] * delta[j];
      if (!std::isfinite(distance2) || distance2 < -1e-12)
        throw std::runtime_error(
            "Envelope produced an invalid geometric distance");
      scores[k] = -2 * std::log(std::max(distance2, 1e-8));
      largest = std::max(largest, scores[k]);
    }
    double total = 0;
    for (auto &score : scores) {
      score = std::exp(score - largest);
      total += score;
    }
    for (size_t k = 0; k < b; ++k)
      ranked[k] = {envelopes[k].source.bone, float(scores[k] / total)};
    const size_t count = std::min(b, size_t(limits.max_influences));
    std::partial_sort(ranked.begin(), ranked.begin() + count, ranked.end(),
                      [](Influence a, Influence b) {
                        return a.weight != b.weight ? a.weight > b.weight
                                                    : a.joint < b.joint;
                      });
    VertexWeights row(ranked.begin(), ranked.begin() + count);
    out.vertices.push_back(
        normalize_weights(std::move(row), rig.bones.size(), {}, limits));
  }
  if (cancelled && cancelled())
    throw std::runtime_error("Envelope binding cancelled");
  out.validate(n, rig.bones.size());
  if (report)
    *report = {base_work + n * per_vertex, n};
  return out;
}
} // namespace dome::detail
