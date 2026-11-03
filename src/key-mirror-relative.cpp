// SPDX-License-Identifier: GPL-3.0-or-later
#include "key-mirror-relative.hpp"
#include <algorithm>
#include <cmath>
namespace dome::detail {
namespace {
struct D {
  double x, y, z;
};
D operator+(D a, D b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
D operator-(D a, D b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
D operator*(D a, double b) { return {a.x * b, a.y * b, a.z * b}; }
double dot(D a, D b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
D cross(D a, D b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
double norm(D a) { return std::sqrt(dot(a, a)); }
D vector(Vec3 a) { return {a.x, a.y, a.z}; }
D reflect(D a) { return {-a.x, a.y, a.z}; }
Vec3 stored(D a) {
  Vec3 v{float(a.x), float(a.y), float(a.z)};
  if (!finite(v))
    throw std::invalid_argument("Relative Mirror produced an unrepresentable artist direction");
  return v;
}
struct Basis {
  D x, y, z;
  double determinant;
  D apply(D a) const {
    const double u = dot(a, cross(y, z)) / determinant, v = dot(x, cross(a, z)) / determinant,
                 w = dot(x, cross(y, a)) / determinant;
    return {u, v, w};
  }
  D compose(D a) const { return x * a.x + y * a.y + z * a.z; }
};
Basis basis(D x, D y, D z) {
  const double d = dot(x, cross(y, z)), scale = norm(x) * norm(y) * norm(z);
  if (!std::isfinite(d) || !std::isfinite(scale) || scale == 0 || std::abs(d) <= scale * 1e-8)
    throw std::invalid_argument("Relative Mirror has no invertible actual artist basis");
  return {x, y, z, d};
}
D normal_only(D source, D target, D delta) {
  // Unique shortest proper rotation is a declared N-only conversion. Antipodal
  // bases have no supplied tangent/axis authority and cannot invent one.
  source = source * (1 / norm(source));
  target = target * (1 / norm(target));
  const double cosine = std::clamp(dot(source, target), -1., 1.);
  if (cosine <= -1 + 1e-12)
    throw std::invalid_argument("N-only relative Mirror has an ambiguous antipodal base");
  const auto axis = cross(source, target);
  return delta + cross(axis, delta) + cross(axis, cross(axis, delta)) * (1 / (1 + cosine));
}
} // namespace
MirrorRelativeAttributes relative_mirror_attributes(const Mesh &geometry,
                                                    const NormalFrames &before,
                                                    const NormalFrameTarget &source, uint64_t id,
                                                    const KeyMirrorCorrespondence &map,
                                                    std::span<const std::array<float, 2>> uv,
                                                    const KeyMirrorLimits &limits) {
  if (!id || !limits.max_bytes || limits.max_bytes > 256 * 1024 * 1024 || !limits.max_work ||
      limits.max_work > 256 * 1024 * 1024)
    throw std::invalid_argument("Invalid relative Mirror policy");
  auto stop = [&] {
    if (limits.cancelled && limits.cancelled())
      throw std::runtime_error("Relative Mirror artist transport cancelled");
  };
  stop();
  const size_t count = before.normals.size();
  if (count > 6'000'000 || count != geometry.indices.size() || count != map.corners.size() ||
      count % 3 || (!before.tangents.empty() && uv.size() != count) ||
      (!source.normals.empty() && source.normals.size() != count) ||
      (!source.tangents.empty() && source.tangents.size() != count))
    throw std::invalid_argument("Relative Mirror needs exact ordered role/source corner streams");
  if ((source.normals.size() + source.tangents.size()) > limits.max_bytes / sizeof(Vec3) ||
      count > limits.max_work / 4)
    throw std::length_error("Relative Mirror preflight exceeds bounded output/work quota");
  before.validate();
  if (std::none_of(before.targets.begin(), before.targets.end(), [&](const auto &t) { return &t == &source; }))
    throw std::invalid_argument("Relative Mirror needs its actual named role target");
  if (std::any_of(before.targets.begin(), before.targets.end(), [&](const auto &t) { return t.id == id; }))
    throw std::invalid_argument("Relative Mirror must allocate a new stable target identity");
  geometry.validate();
  MirrorRelativeAttributes result;
  result.target.id = id;
  if (!source.normals.empty())
    result.target.normals.resize(count);
  if (!source.tangents.empty())
    result.target.tangents.resize(count);
  for (size_t c = 0; c < count; c += 3) {
    stop();
    int sigma = 0, tau = 0;
    for (size_t k = 0; k < 3; ++k) {
      const auto at = map.corners[c + k];
      if (at >= count || map.corners[at] != c + k)
        throw std::invalid_argument("Relative Mirror borrowed an invalid corner involution");
      if (k && at / 3 != map.corners[c] / 3)
        throw std::invalid_argument("Relative Mirror lacks actual source triangle correspondence");
    }
    if (!before.tangents.empty()) {
      double u[3], v[3], su[3], sv[3];
      for (size_t k = 0; k < 3; ++k) {
        u[k] = uv[c + k][0];
        v[k] = uv[c + k][1];
        su[k] = uv[map.corners[c + k]][0];
        sv[k] = uv[map.corners[c + k]][1];
        if (!std::isfinite(u[k]) || !std::isfinite(v[k]) || !std::isfinite(su[k]) ||
            !std::isfinite(sv[k]))
          throw std::invalid_argument("Relative Mirror has nonfinite role coordinates");
      }
      const double u1 = u[1] - u[0], u2 = u[2] - u[0], v1 = v[1] - v[0], v2 = v[2] - v[0],
                   a1 = su[1] - su[0], a2 = su[2] - su[0], b1 = sv[1] - sv[0], b2 = sv[2] - sv[0],
                   det = u1 * v2 - u2 * v1, a = a1 * v2 - a2 * v1, b = a2 * u1 - a1 * u2,
                   d = b2 * u1 - b1 * u2, e = b1 * v2 - b2 * v1, j = a * d - b * e;
      if (!std::isfinite(det) || !std::isfinite(j) || !det || !j || !a || !d ||
          (a > 0) == (d > 0) || j >= 0)
        throw std::invalid_argument("Relative Mirror needs genuine unambiguous diagonal chart "
                                    "parity; cross-axis conversion remains separate");
      sigma = (a > 0) == (det > 0) ? 1 : -1;
      tau = (d > 0) == (det > 0) ? 1 : -1;
      result.report.maximum_chart_cross_ratio =
          std::max(result.report.maximum_chart_cross_ratio,
                   std::max(std::abs(b), std::abs(e)) / std::max(std::abs(a), std::abs(d)));
    }
    for (size_t k = 0; k < 3; ++k) {
      const auto at = c + k, s = map.corners[at];
      const auto ns = vector(before.normals[s]), nt = vector(before.normals[at]), rn = reflect(ns);
      result.report.maximum_normal_base_difference =
          std::max(result.report.maximum_normal_base_difference, norm(nt - rn));
      if (before.tangents.empty()) {
        if (!source.normals.empty())
          result.target.normals[at] =
              stored(normal_only(rn, nt, reflect(vector(source.normals[s]))));
        continue;
      }
      const auto ts = before.tangents[s], tt = before.tangents[at];
      if (ts[3] != tt[3])
        throw std::invalid_argument(
            "Relative Mirror cannot reauthor absent tangent handedness morphs");
      const D st{ts[0], ts[1], ts[2]}, dt{tt[0], tt[1], tt[2]};
      const auto sb = cross(ns, st) * ts[3], db = cross(nt, dt) * tt[3];
      const auto input = basis(reflect(st) * sigma, reflect(sb) * tau, rn),
                 output = basis(dt, db, nt);
      auto convert = [&](D delta) { return output.compose(input.apply(delta)); };
      result.report.maximum_tangent_base_difference =
          std::max(result.report.maximum_tangent_base_difference, norm(dt - reflect(st) * sigma));
      for (const D axis : {D{1, 0, 0}, D{0, 1, 0}, D{0, 0, 1}})
        result.report.maximum_basis_scale_difference = std::max(
            result.report.maximum_basis_scale_difference, std::abs(norm(convert(axis)) - 1));
      if (!source.normals.empty())
        result.target.normals[at] = stored(convert(reflect(vector(source.normals[s]))));
      if (!source.tangents.empty())
        result.target.tangents[at] = stored(convert(reflect(vector(source.tangents[s])) * sigma));
    }
  }
  stop();
  return result;
}
} // namespace dome::detail
