// SPDX-License-Identifier: GPL-3.0-or-later
// Geometry-only diagnostic. No native admission, closure repair or weight copy.
#include "dome/affine.hpp"
#include "dome/creation.hpp"
#include "dome/mesh-processing.hpp"
#include "envelope-binding.hpp"
#include "heat-seams.hpp"
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
double distance(D a, D b) {
  return std::hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
}
// Independent point consumer: explicit double morph accumulation and LBS.
// Joint matrices come from the existing native hierarchy; no preview/deform
// vertex evaluator is reused. Independent GLB/hierarchy/render proof is later.
std::vector<D> points(const Mesh &m, const DeformationState &d) {
  auto matrices = d.skeleton->skin_matrices(d.pose);
  std::vector<D> out(m.positions.size());
  for (size_t v = 0; v < m.positions.size(); ++v) {
    D p{m.positions[v].x, m.positions[v].y, m.positions[v].z};
    if (d.morphs && !d.morph_weights.empty())
      for (size_t t = 0; t < d.morphs->targets.size(); ++t) {
        const auto &q = d.morphs->targets[t].delta[v];
        double w = d.morph_weights[t];
        p[0] += w * q.x;
        p[1] += w * q.y;
        p[2] += w * q.z;
      }
    for (auto influence : d.skin->vertices[v]) {
      const auto &a = matrices[influence.joint];
      for (int k = 0; k < 3; ++k)
        out[v][k] += double(influence.weight) * (a[k] * p[0] + a[4 + k] * p[1] +
                                                 a[8 + k] * p[2] + a[12 + k]);
    }
  }
  return out;
}
void pose(DeformationState &d, int which) {
  d.pose.clear();
  for (const auto &b : d.skeleton->bones)
    d.pose.push_back(b.rest);
  auto rotate = [&](const char *role, Vec3 axis, double degrees) {
    auto at = d.humanoid->joints.find(role);
    if (at == d.humanoid->joints.end())
      return;
    double half = degrees * std::numbers::pi / 360;
    Quat q{float(std::cos(half)), float(axis.x * std::sin(half)),
           float(axis.y * std::sin(half)), float(axis.z * std::sin(half))};
    d.pose[at->second].rotation = q * d.pose[at->second].rotation;
  };
  if (which == 0) {
    rotate("leftLowerArm", {0, 0, 1}, 20);
    rotate("rightUpperLeg", {0, 0, 1}, 20);
  }
  if (which == 1) {
    rotate("head", {0, 0, 1}, 30);
    rotate("neck", {0, 1, 0}, -15);
  }
  if (which == 2 || which == 4) {
    rotate("leftHand", {0, 0, 1}, 20);
    rotate("leftMiddleIntermediate", {1, 0, 0}, 45);
    rotate("rightFoot", {1, 0, 0}, 20);
  }
  if (which == 3) {
    rotate("spine", {0, 0, 1}, 15);
    rotate("hips", {0, 1, 0}, 10);
    rotate("leftShoulder", {0, 0, 1}, 20);
  }
  if (which == 4 && d.morphs && !d.morphs->targets.empty()) {
    if (d.morph_weights.empty())
      d.morph_weights.assign(d.morphs->targets.size(), 0);
    d.morph_weights[0] = .35f;
  }
}
} // namespace
int main(int argc, char **argv) try {
  const std::string gender = argc > 1 ? argv[1] : "female";
  if (gender != "female" && gender != "male")
    throw std::invalid_argument("Explicit gender required");
  if (argc < 3 || std::string(argv[2]) != "unsigned-control")
    throw std::invalid_argument("Explicit unsigned-control required; this is "
                                "not an admitted volume route");
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
  std::vector<BoneEnvelope> envelopes;
  size_t projected = 0;
  double maximum_fit_rms = 0;
  std::cout << std::setprecision(10)
            << "CONTROL unsigned-first-crossing surface envelopes, no "
               "volume/native admission; gender="
            << gender << " captured_parts=" << owners.size() << '\n';
  for (const auto &source : profile.segments) {
    D head = frame.point(source.head), tail = frame.point(source.tail),
      axis = sub(tail, head), mid = scale(add(head, tail), .5), u{1, 0, 0},
      v{0, 1, 0};
    const double length = norm(axis);
    const unsigned dimensions = length ? 2 : 3;
    if (length) {
      axis = scale(axis, 1 / length);
      D seed{};
      size_t k = 0;
      for (size_t j = 1; j < 3; ++j)
        if (std::abs(axis[j]) < std::abs(axis[k]))
          k = j;
      seed[k] = 1;
      u = cross3(axis, seed);
      u = scale(u, 1 / norm(u));
      v = cross3(axis, u);
    } else
      axis = {0, 0, 1};
    auto local = envelope_directions(dimensions);
    std::vector<D> directions;
    for (auto q : local)
      directions.push_back(
          add(add(scale(u, q[0]), scale(v, q[1])), scale(axis, q[2])));
    std::vector<double> radii;
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
      if (!hit || !(hit->distance > 0)) {
        ++missing;
        throw std::runtime_error(
            "Unresolved unsigned source-envelope sample at genuine bone " +
            std::to_string(source.bone));
      }
      radii.push_back(hit->distance);
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
    auto fit = fit_envelope(local, radii, dimensions);
    projected += fit.projected_eigenvalues;
    maximum_fit_rms = std::max(maximum_fit_rms, fit.relative_rms);
    EnvelopeMatrix precision = fit.precision;
    if (dimensions == 2) {
      double cap = std::sqrt(fit.precision[0] * fit.precision[4] -
                             fit.precision[1] * fit.precision[3]);
      precision = {};
      for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
          precision[i * 3 + j] =
              u[i] * (fit.precision[0] * u[j] + fit.precision[1] * v[j]) +
              v[i] * (fit.precision[3] * u[j] + fit.precision[4] * v[j]) +
              cap * axis[i] * axis[j];
    }
    for (int i = 0; i < 3; ++i)
      for (int j = i + 1; j < 3; ++j)
        precision[i * 3 + j] = precision[j * 3 + i] =
            .5 * (precision[i * 3 + j] + precision[j * 3 + i]);
    envelopes.push_back({{head, tail, source.bone}, precision});
    std::cout << "CROSS_SECTION bone=" << source.bone << " source_node="
              << target->appearance->source_joint_nodes[source.bone]
              << " name=" << rig.bones[source.bone].name << " length=" << length
              << " midpoint=" << mid[0] << ',' << mid[1] << ',' << mid[2]
              << " exits=" << exits << " entries=" << entries
              << " missing=" << missing << " grazing=" << grazing
              << " radial_rms=" << fit.relative_rms
              << " projected=" << fit.projected_eigenvalues
              << " radius_min=" << minimum << " radius_max=" << maximum
              << " owners=";
    for (auto [id, n] : hits)
      std::cout << id << ':' << n << ',';
    std::cout << '\n';
  }
  double lo = INFINITY, hi = -INFINITY;
  for (const auto &o : scene.objects())
    if (o.painted)
      for (auto p : o.painted->mesh().positions) {
        lo = std::min(lo, double(p.y));
        hi = std::max(hi, double(p.y));
      }
  const double height = hi - lo, mean_limit = height * .005,
               max_limit = height * .02;
  std::cout << "FIXED_QUALITY mean_limit=" << mean_limit
            << " max_limit=" << max_limit << " envelopes=" << envelopes.size()
            << " projected=" << projected
            << " maximum_radial_rms=" << maximum_fit_rms << '\n';
  size_t solved = 0, refused = 0, quality = 0, seam_fail = 0, native_fail = 0;
  for (const auto &o : scene.objects())
    if (o.painted && o.appearance && o.deformation && o.deformation->skin &&
        o.deformation->humanoid && o.group == target->group) {
      std::cout << "PART " << o.id << ' ' << o.name << '\n';
      try {
        Mesh metric = o.painted->mesh();
        for (auto &p : metric.positions)
          p = vec(frame.point(point(p)));
        for (auto &p : metric.normals) {
          auto n = frame.normal(point(p));
          double length = norm(n);
          p = length ? vec(scale(n, 1 / length)) : Vec3{};
        }
        EnvelopeBindReport report;
        auto weights = bind_envelopes(metric, *o.deformation->skeleton,
                                      envelopes, {}, {}, &report);
        ++solved;
        auto seams = capture_heat_seams(o, options);
        auto groups = seams.groups(o);
        for (int scenario = 0; scenario < 5; ++scenario) {
          auto reference = *o.deformation, next = reference;
          next.skin = weights;
          pose(reference, scenario);
          pose(next, scenario);
          const auto &mesh = o.painted->mesh();
          auto a = points(mesh, reference), b = points(mesh, next);
          auto native = next.preview(mesh);
          double mean = 0, maximum = 0, consumer = 0;
          bool seam = true;
          for (size_t i = 0; i < a.size(); ++i) {
            double error = distance(a[i], b[i]);
            mean += error;
            maximum = std::max(maximum, error);
            consumer =
                std::max(consumer, distance(b[i], {native.positions[i].x,
                                                   native.positions[i].y,
                                                   native.positions[i].z}));
            seam &= b[i] == b[groups[i]];
          }
          mean /= a.size();
          bool good = mean <= mean_limit && maximum <= max_limit;
          quality += !good;
          seam_fail += !seam;
          native_fail += consumer > height * 1e-5;
          std::cout << "POSE " << scenario << " artist_mean=" << mean
                    << " artist_max=" << maximum
                    << " native_consumer_max=" << consumer
                    << " seam_exact=" << seam << " quality=" << good << '\n';
        }
      } catch (const std::exception &e) {
        ++refused;
        std::cout << "REFUSED " << e.what() << '\n';
      }
    }
  bool exact = scene.encode() == original;
  std::cout << "SUMMARY solved=" << solved << " refused=" << refused
            << " quality_failures=" << quality << " seam_failures=" << seam_fail
            << " consumer_failures=" << native_fail
            << " original_scene_exact=" << exact << " no_weight_admission=1\n";
  return refused || quality || seam_fail || native_fail || !exact ? 1 : 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
