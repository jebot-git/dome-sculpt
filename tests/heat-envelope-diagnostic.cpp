// SPDX-License-Identifier: GPL-3.0-or-later
// Geometry-only diagnostic. No native admission, closure repair or weight copy.
#include "dome/affine.hpp"
#include "dome/creation.hpp"
#include "dome/mesh-processing.hpp"
#include "heat-source.hpp"
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <numbers>
using namespace dome;
using namespace dome::detail;
namespace {
using D = std::array<double, 3>;
D sub(D a, D b) {
  for (int k = 0; k < 3; ++k)
    a[k] -= b[k];
  return a;
}
D add(D a, D b) {
  for (int k = 0; k < 3; ++k)
    a[k] += b[k];
  return a;
}
D scale(D a, double s) {
  for (auto &x : a)
    x *= s;
  return a;
}
double dot3(D a, D b) {
  double s = 0;
  for (int k = 0; k < 3; ++k)
    s += a[k] * b[k];
  return s;
}
D cross3(D a, D b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
          a[0] * b[1] - a[1] * b[0]};
}
double norm(D a) { return std::sqrt(dot3(a, a)); }
D point(Vec3 p) { return {p.x, p.y, p.z}; }
Vec3 vec(D p) { return {float(p[0]), float(p[1]), float(p[2])}; }
template <class T>
bool owner(const std::shared_ptr<T> &a, const std::shared_ptr<T> &b) {
  return a.get() == b.get() && !a.owner_before(b) && !b.owner_before(a);
}
} // namespace
int main(int argc, char **argv) try {
  const std::string gender = argc > 1 ? argv[1] : "female";
  if (gender != "female" && gender != "male")
    throw std::invalid_argument("Explicit gender required");
  Scene scene;
  append_humanoid(scene,
                  gender == "male"
                      ? humanoid_baseline_defaults("vroid-beta-male")
                      : HumanoidControls{},
                  "Source envelope geometry", {}, 64);
  const auto original = scene.encode();
  const SculptObject *target = nullptr;
  for (const auto &o : scene.objects())
    if (o.painted && o.appearance && o.deformation && o.deformation->skeleton &&
        o.deformation->humanoid) {
      target = &o;
      break;
    }
  if (!target)
    throw std::runtime_error("No actual authenticated imported rig");
  const auto &rig = *target->deformation->skeleton;
  const auto rig_bytes = rig.encode();
  const auto frame = scene.world_matrix(target->id);
  const auto matrix = frame.matrix();
  std::vector<std::shared_ptr<const Mesh>> owners;
  std::vector<std::shared_ptr<const DeformationState>> deforms;
  std::vector<uint64_t> ids;
  std::vector<uint32_t> triangle_owner;
  size_t vertices = 0, triangles = 0;
  for (const auto &o : scene.objects())
    if (o.group == target->group) {
      if (!o.painted || !o.appearance || !o.deformation ||
          !o.deformation->skeleton ||
          !owner(o.appearance->asset, target->appearance->asset) ||
          o.appearance->source_joint_nodes !=
              target->appearance->source_joint_nodes ||
          o.deformation->skeleton->encode() != rig_bytes ||
          scene.world_matrix(o.id).matrix() != matrix)
        throw std::runtime_error(
            "Source geometry scope lacks exact "
            "asset/joint/current-bind/common-placement authority");
      auto m = o.painted->mesh_owner();
      if (!m || m.get() != &o.painted->mesh())
        throw std::runtime_error(
            "Source geometry lacks its strong immutable Mesh owner");
      vertices += m->positions.size();
      triangles += m->indices.size() / 3;
      owners.push_back(std::move(m));
      deforms.push_back(o.deformation);
      ids.push_back(o.id);
    }
  if (vertices > 2'000'000 || triangles > 2'000'000 ||
      vertices * 128 + triangles * 256 > 256u * 1024u * 1024u)
    throw std::length_error(
        "Source geometry diagnostic preflight cap exceeded");
  Mesh packed;
  packed.positions.reserve(vertices);
  packed.normals.reserve(vertices);
  packed.indices.reserve(triangles * 3);
  triangle_owner.reserve(triangles);
  const bool reversed = frame.orientation() == AffineOrientation::Reversing;
  for (size_t part = 0; part < owners.size(); ++part) {
    const auto &m = *owners[part];
    uint32_t first = packed.positions.size();
    for (size_t i = 0; i < m.positions.size(); ++i) {
      packed.positions.push_back(vec(frame.point(point(m.positions[i]))));
      auto n = frame.normal(point(m.normals[i]));
      double length = norm(n);
      packed.normals.push_back(length ? vec(scale(n, 1 / length)) : Vec3{});
    }
    for (size_t t = 0; t < m.indices.size(); t += 3) {
      packed.indices.push_back(first + m.indices[t]);
      packed.indices.push_back(first + m.indices[t + (reversed ? 2 : 1)]);
      packed.indices.push_back(first + m.indices[t + (reversed ? 1 : 2)]);
      triangle_owner.push_back(uint32_t(part));
    }
  }
  D lower{INFINITY, INFINITY, INFINITY}, upper{-INFINITY, -INFINITY, -INFINITY};
  for (auto p : packed.positions)
    for (int k = 0; k < 3; ++k) {
      lower[k] = std::min(lower[k], point(p)[k]);
      upper[k] = std::max(upper[k], point(p)[k]);
    }
  const D center = scale(add(lower, upper), .5);
  const double half_diagonal = .5 * norm(sub(upper, lower));
  size_t query_checks = 0;
  SurfaceQuery query(packed, [&] { return ++query_checks > 50'000'000; });
  HeatBindingOptions options;
  auto profile = imported_heat_profile(rig, *target->appearance, options, {},
                                       &*target->deformation->humanoid);
  size_t valid = 0, incomplete = 0;
  std::cout << std::setprecision(10) << "SCOPE gender=" << gender
            << " parts=" << owners.size() << " vertices=" << vertices
            << " triangles=" << triangles << " reversed=" << reversed << '\n';
  for (const auto &source : profile.segments) {
    D head = frame.point(source.head), tail = frame.point(source.tail),
      axis = sub(tail, head), mid = scale(add(head, tail), .5);
    double length = norm(axis);
    std::vector<D> directions;
    if (length) {
      axis = scale(axis, 1 / length);
      D seed{};
      size_t k = 0;
      for (size_t j = 1; j < 3; ++j)
        if (std::abs(axis[j]) < std::abs(axis[k]))
          k = j;
      seed[k] = 1;
      auto u = cross3(axis, seed);
      u = scale(u, 1 / norm(u));
      auto v = cross3(axis, u);
      for (int ray = 0; ray < 16; ++ray) {
        double a = 2 * std::numbers::pi * ray / 16;
        directions.push_back(add(scale(u, std::cos(a)), scale(v, std::sin(a))));
      }
    } else
      for (int k = 0; k < 3; ++k)
        for (double sign : {-1., 1.}) {
          D d{};
          d[k] = sign;
          directions.push_back(d);
        }
    size_t exits = 0, entries = 0, missing = 0, grazing = 0;
    double minimum = INFINITY, maximum = 0;
    std::map<uint64_t, size_t> hits;
    for (auto direction : directions) {
      SurfaceQueryOptions qo;
      qo.cancelled = [&] { return ++query_checks > 50'000'000; };
      double extent = norm(sub(mid, center)) + half_diagonal;
      float ray_limit =
          std::nextafter(float(extent), std::numeric_limits<float>::infinity());
      if (!(ray_limit > 0) || ray_limit > 1000)
        throw std::length_error(
            "Actual source ray extent exceeds SurfaceQuery contract");
      auto hit = query.raycast(vec(mid), vec(direction), ray_limit, qo);
      if (!hit) {
        ++missing;
        continue;
      }
      size_t t = size_t(hit->triangle) * 3;
      auto a = point(packed.positions[packed.indices[t]]),
           b = point(packed.positions[packed.indices[t + 1]]),
           c = point(packed.positions[packed.indices[t + 2]]);
      auto normal = cross3(sub(b, a), sub(c, a));
      double cosine = dot3(normal, direction) / norm(normal);
      if (cosine > 1e-7 && hit->distance > 0) {
        ++exits;
        minimum = std::min(minimum, double(hit->distance));
        maximum = std::max(maximum, double(hit->distance));
        ++hits[ids[triangle_owner[hit->triangle]]];
      } else if (cosine < -1e-7)
        ++entries;
      else
        ++grazing;
    }
    bool complete = exits == directions.size();
    valid += complete;
    incomplete += !complete;
    std::cout << "CROSS_SECTION bone=" << source.bone << " source_node="
              << target->appearance->source_joint_nodes[source.bone]
              << " name=" << rig.bones[source.bone].name << " length=" << length
              << " midpoint=" << mid[0] << ',' << mid[1] << ',' << mid[2]
              << " exits=" << exits << " entries=" << entries
              << " missing=" << missing << " grazing=" << grazing
              << " radius_min=" << minimum << " radius_max=" << maximum
              << " owners=";
    for (auto [id, n] : hits)
      std::cout << id << ':' << n << ',';
    std::cout << '\n';
  }
  if (scene.encode() != original)
    throw std::runtime_error("Geometry diagnostic changed original Scene");
  std::cout << "SUMMARY complete_samples=" << valid
            << " unresolved=" << incomplete << " query_checks=" << query_checks
            << " original_scene_exact=1 no_weight_admission=1\n";
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
