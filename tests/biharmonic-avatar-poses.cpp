// SPDX-License-Identifier: GPL-3.0-or-later
#include "biharmonic-binding.hpp"
#include "dome/creation.hpp"
#include "heat-midpoint-source.hpp"
#include "heat-seams.hpp"
#include "heat-source.hpp"
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <numbers>
#include <set>
using namespace dome;
using namespace dome::detail;
namespace {
using D = std::array<double, 3>;
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
  std::string policy = argc > 1 ? argv[1] : "whole",
              gender = argc > 2 ? argv[2] : "female",
              visibility = argc > 3 ? argv[3] : "all",
              domain = argc > 4 ? argv[4] : "whole";
  if ((policy != "whole" && policy != "primitive") ||
      (gender != "male" && gender != "female") ||
      (visibility != "all" && visibility != "entry") ||
      (domain != "whole" && domain != "component"))
    throw std::invalid_argument("Unknown explicit heat pose control");
  bool primitive = policy == "primitive", male = gender == "male";
  double strength = argc > 5 ? std::stod(argv[5]) : 1;
  std::string geometry_normals = argc > 6 ? argv[6] : "area";
  if (geometry_normals != "area" && geometry_normals != "unit")
    throw std::invalid_argument("Unknown explicit heat geometric normal mode");
  std::string mass = argc > 7 ? argv[7] : "barycentric";
  if (mass != "barycentric" && mass != "mixed")
    throw std::invalid_argument("Unknown explicit heat mass control");
  std::string edges = argc > 8 ? argv[8] : "half";
  if (edges != "half" && edges != "incident")
    throw std::invalid_argument("Unknown explicit heat edge control");
  std::string source_rule = argc > 9 ? argv[9] : "distinct";
  if (source_rule != "distinct" && source_rule != "reference" &&
      source_rule != "reference-raw")
    throw std::invalid_argument("Unknown explicit heat source rule");
  std::string operation = argc > 10 ? argv[10] : "positive";
  if (operation != "positive" && operation != "fem" && operation != "bounded")
    throw std::invalid_argument("Unknown explicit heat operator");
  std::string handle_policy = argc > 11 ? argv[11] : "segments";
  if (handle_policy != "segments" && handle_policy != "midpoints")
    throw std::invalid_argument("Unknown explicit heat handle policy");
  std::string distance_mode = argc > 12 ? argv[12] : "signed";
  if (distance_mode != "signed" && distance_mode != "unsigned")
    throw std::invalid_argument("Unknown explicit source-distance domain");
  std::vector<size_t> support;
  std::set<uint32_t> expected_auxiliaries;
  Scene scene;
  auto controls =
      male ? humanoid_baseline_defaults("vroid-beta-male") : HumanoidControls{};
  append_humanoid(scene, controls, "Actual heat pose audit", {}, 64);
  auto original_scene = scene.encode();
  double lo = INFINITY, hi = -INFINITY;
  for (auto &o : scene.objects())
    if (o.painted)
      for (auto p : o.painted->mesh().positions) {
        lo = std::min(lo, double(p.y));
        hi = std::max(hi, double(p.y));
      }
  const double height = hi - lo, mean_limit = height * .005,
               max_limit = height * .02;
  std::cout << std::setprecision(10)
            << "FIXED_QUALITY gender=" << (male ? "male" : "female")
            << " policy=" << (primitive ? "original-primitive" : "whole-skin")
            << " visibility=" << visibility << " occlusion=" << domain
            << " heat_strength=" << strength << " normals=" << geometry_normals
            << " distance_domain=" << distance_mode
            << " handle_policy=" << handle_policy << " operation=" << operation
            << " source_rule=" << source_rule << " edges=" << edges
            << " mass=" << mass << " height=" << height
            << " mean_limit=" << mean_limit << " max_limit=" << max_limit
            << '\n';
  size_t solved = 0, refused = 0, quality = 0, seam_fail = 0, native_fail = 0;
  for (const auto &o : scene.objects()) {
    if (!o.painted || !o.deformation || !o.deformation->skin ||
        !o.deformation->humanoid)
      continue;
    std::cout << "PART " << o.id << ' ' << o.name << '\n' << std::flush;
    try {
      HeatBindingOptions options;
      options.operation = operation == "fem" ? HeatOperator::FiniteElements
                          : operation == "bounded"
                              ? HeatOperator::BoundedFiniteElements
                              : HeatOperator::PositiveGraph;
      options.visibility = visibility == "entry" ? HeatVisibility::EntryFaces
                                                 : HeatVisibility::AllFaces;
      options.occlusion = domain == "component"
                              ? HeatOcclusion::ConnectedSurface
                              : HeatOcclusion::WholeMesh;
      options.mass =
          mass == "mixed" ? HeatMass::MixedVoronoi : HeatMass::Barycentric;
      options.edges = edges == "incident" ? HeatEdges::IncidentMean
                                          : HeatEdges::CotangentHalf;
      if (source_rule != "distinct") {
        options.seed_rule = HeatSeedRule::ReferenceSegments;
        options.output = source_rule == "reference" ? HeatOutput::ReferenceLimit
                                                    : HeatOutput::Raw;
      }
      options.distance_rule = distance_mode == "unsigned"
                                  ? HeatDistanceRule::EuclideanSurface
                                  : HeatDistanceRule::OutwardNormal;
      options.maximum_seed_distance = 1;
      options.heat_strength = strength;
      options.geometry_normals = geometry_normals == "unit"
                                     ? HeatNormals::UnitFaces
                                     : HeatNormals::AreaWeighted;
      auto profile = imported_heat_profile(
          *o.deformation->skeleton, *o.appearance, options, {},
          &*o.deformation->humanoid,
          primitive ? ImportedHeatPolicy::OriginalPrimitive
                    : ImportedHeatPolicy::WholeSkin);
      options.maximum_work -= profile.work;
      options.maximum_bytes -= profile.planned_bytes;
      std::set<uint32_t> primary_set;
      for (const auto &[role, joint] : o.deformation->humanoid->joints) {
        (void)role;
        primary_set.insert(joint);
      }
      std::vector<uint32_t> primary(primary_set.begin(), primary_set.end());
      if (support.empty())
        support.resize(o.deformation->skeleton->bones.size());
      if (support.size() != o.deformation->skeleton->bones.size())
        throw std::runtime_error(
            "Actual source rig scope differs between parts");
      for (const auto &source : profile.segments)
        if (!primary_set.contains(source.bone))
          expected_auxiliaries.insert(source.bone);
      auto construction = make_heat_handles(
          profile.segments, primary, o.deformation->skeleton->bones.size(),
          handle_policy == "midpoints" ? HeatHandlePolicy::PrimaryMidpoints
                                       : HeatHandlePolicy::OriginalSegments,
          options);
      options.maximum_work -= construction.work;
      options.maximum_bytes -= construction.planned_bytes;

      auto seams = capture_heat_seams(o, options);
      options.maximum_work -= seams.work();
      options.maximum_bytes -= seams.planned_bytes();
      BiharmonicBindingOptions settings;
      settings.source = options;
      BiharmonicBindingReport report;
      auto weights = bounded_biharmonic_surface(
          o.painted->mesh(), *o.deformation->skeleton, settings,
          construction.handles, profile.eligible, seams.groups(o), {}, &report);
      ++solved;
      for (const auto &row : weights.vertices)
        for (auto value : row)
          if (value.weight > 0)
            ++support[value.joint];
      std::cout << "MATRIX vertices=" << report.vertices
                << " components=" << report.components
                << " sources=" << report.source_bones
                << " anchors=" << report.fixed_rows
                << " kkt=" << report.maximum_kkt_residual
                << " active=" << report.active_iterations
                << " cg=" << report.cg_iterations
                << " partition=" << report.maximum_partition_error
                << " work_total="
                << profile.work + construction.work + seams.work() + report.work
                << '\n';
      for (int scenario = 0; scenario < 5; ++scenario) {
        auto reference = *o.deformation, next = reference;
        next.skin = weights;
        pose(reference, scenario);
        pose(next, scenario);
        auto a = points(o.painted->mesh(), reference),
             b = points(o.painted->mesh(), next);
        auto native = next.preview(o.painted->mesh());
        double mean = 0, maximum = 0, consumer = 0;
        bool seam = true;
        auto groups = seams.groups(o);
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
    } catch (const HeatPositivityFailure &e) {
      ++refused;
      std::cout << "REFUSED_POSITIVITY joint=" << e.joint
                << " minimum=" << e.minimum << '\n';
    } catch (const HeatSeedFailure &e) {
      ++refused;
      std::cout << "REFUSED_SEED rows=" << e.rows
                << " entry_hits=" << e.entry_hits
                << " exit_hits=" << e.exit_hits
                << " grazing_hits=" << e.grazing_hits << " lower=" << e.lower[0]
                << ',' << e.lower[1] << ',' << e.lower[2]
                << " upper=" << e.upper[0] << ',' << e.upper[1] << ','
                << e.upper[2] << '\n';
    } catch (const std::exception &e) {
      ++refused;
      std::cout << "REFUSED " << e.what() << '\n';
    }
    std::cout << std::flush;
  }
  size_t absent_auxiliaries = 0;
  for (auto bone : expected_auxiliaries) {
    absent_auxiliaries += !support[bone];
    std::cout << "AUXILIARY_SUPPORT bone=" << bone
              << " vertices=" << support[bone] << '\n';
  }
  bool exact = scene.encode() == original_scene;
  std::cout << "SUMMARY solved=" << solved << " refused=" << refused
            << " quality_failures=" << quality << " seam_failures=" << seam_fail
            << " consumer_failures=" << native_fail
            << " absent_auxiliaries=" << absent_auxiliaries
            << " original_scene_exact=" << exact << '\n';
  return refused || quality || seam_fail || native_fail || absent_auxiliaries ||
                 !exact
             ? 1
             : 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
