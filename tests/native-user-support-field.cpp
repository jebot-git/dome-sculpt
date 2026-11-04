// SPDX-License-Identifier: GPL-3.0-or-later
// Explicit private candidate producer. No original artist BIN/weights input.
#include "dome/rig-rest.hpp"
#include "geometry-weight-authoring.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
int main(int argc, char **argv) try {
  if (argc != 4)
    throw std::runtime_error("Choose authentic fitted unskinned Scene and "
                             "explicit user-support JSON and output directory");
  const std::filesystem::path out = argv[3];
  std::filesystem::create_directories(out);
  auto scene = Scene::load(argv[1]);
  const auto before = scene.encode();
  uint64_t reference = 0;
  std::vector<uint64_t> targets;
  for (const auto &o : scene.objects()) {
    if (o.deformation && o.deformation->skeleton)
      reference = o.id;
    if (o.mesh_active && o.painted && o.appearance) {
      if (!o.appearance->asset || !o.appearance->asset->skins.empty())
        throw std::runtime_error("Not a genuine unskinned input");
      for (const auto &p : o.appearance->asset->primitives)
        if (!p.influences.empty())
          throw std::runtime_error("Artist influences in input");
      targets.push_back(o.id);
    }
  }
  if (!reference || targets.size() != 19)
    throw std::runtime_error("Complete genuine19-part native input required");
  detail::DeclaredWeightSupport support;
  support.policy = detail::DeclaredSupportPolicy::UniqueSourceWitnesses;
  support.world_metric = true;
  support.reference = reference;
  support.row_anchors = detail::DeclaredRowAnchors::NearestPermitted;
  support.source_requirements =
      detail::DeclaredSourceRequirements::GeometryEvidence;
  support.source_span = detail::DeclaredSourceSpan::AuthoredFinite;
  support.origin_boundary =
      detail::DeclaredOriginBoundary::NativeFiniteLineIntegral;
  support.contact_support = detail::DeclaredContactSupport::CommonMask;
  support.rig_geometry = detail::DeclaredRigGeometry::NativeBoneAxes;
  nlohmann::json user;
  { std::ifstream f(argv[2]); f >> user; }
  const auto &reference_object = scene.object(reference);
  const auto &rig = *reference_object.deformation->skeleton;
  const auto &roles = reference_object.deformation->humanoid->joints;
  const std::function<bool()> never = [] { return false; };
  detail::weight_surface::Budget construction{0, 500000000, never};
  for (auto id : targets) {
    detail::DeclaredWeightPart part{id, {}};
    const auto key = std::to_string(id);
    if (user.contains(key) && user.at(key).contains("exclude_roles")) {
      for (const auto &role : user.at(key).at("exclude_roles"))
        part.user_excluded_joints.push_back(roles.at(role.get<std::string>()));
    } else if (user.contains(key)) {
      const auto &o = scene.object(id);
      const auto &m = o.painted->mesh();
      size_t local = 0;
      auto components = detail::heat_intent_components(
          m, {}, local, construction.maximum - construction.used, never);
      construction.step(local);
      std::vector<detail::HeatSourceSegment> permitted;
      auto segments = rig.segments();
      const auto &request = user.at(key);
      const bool pin = request.is_array() || request.at("boundary").get<bool>();
      const auto &requested_roles = request.is_array() ? request : request.at("roles");
      for (const auto &name : requested_roles) {
        auto j = roles.at(name.get<std::string>());
        auto a = segments[j][0], b = segments[j][1];
        auto metric = scene.world_matrix(reference);
        auto h = metric.point({a.x, a.y, a.z});
        auto t = metric.point({b.x, b.y, b.z});
        permitted.push_back({{float(h[0]),float(h[1]),float(h[2])},
                             {float(t[0]),float(t[1]),float(t[2])},j});
      }
      if (permitted.empty()) throw std::runtime_error("Empty explicit role support");
      detail::NearestWeightSources nearest(permitted, construction);
      auto world = scene.world_matrix(id);
      std::map<uint32_t,uint32_t> chosen;
      for (uint32_t v = 0; v < components.size(); ++v) {
        if (components[v] != v) continue;
        if (!pin) {
          std::vector<uint32_t> declared;
          for (const auto &source : permitted) declared.push_back(source.bone);
          part.declarations.push_back({v,std::move(declared)});
          continue;
        }
        auto p = m.positions[v]; auto q = world.point({p.x,p.y,p.z});
        auto selection = nearest.nearest({float(q[0]),float(q[1]),float(q[2])}, construction);
        size_t count = 0; uint32_t joint = 0;
        for (uint32_t j = 0; j < rig.bones.size(); ++j)
          if (selection[j/64] & (uint64_t(1) << (j%64))) { ++count; joint=j; }
        if (count != 1) throw std::runtime_error("User component anchor ambiguous");
        part.declarations.push_back({v,{joint}});
      }
      part.user_singleton_boundary = pin;
    }
    support.parts.push_back(std::move(part));
  }
  size_t declaration_bytes = 0;
  for (const auto &part : support.parts) {
    declaration_bytes += part.user_excluded_joints.capacity() * sizeof(uint32_t);
    declaration_bytes += part.declarations.capacity() * sizeof(detail::HeatComponentDeclaration);
    for (const auto &entry : part.declarations)
      declaration_bytes += entry.joints.capacity() * sizeof(uint32_t) + 64;
  }
  const size_t wrapper_work = construction.used + 4096 + targets.size() * 32,
               wrapper_bytes =
                   sizeof(support) + declaration_bytes +
                   targets.size() * sizeof(detail::DeclaredWeightPart) + 4096;
  detail::GeometryWeightBindOptions options;
  options.limits = {4, 1e-5f};
  options.maximum_work = 500000000 - wrapper_work;
  options.maximum_workspace = 256u * 1024u * 1024u - wrapper_bytes;
  detail::GeometryWeightBindReport report;
  const auto start = std::chrono::steady_clock::now();
  scene.save(out / "fitted-unskinned.dsculpt");
  if (!detail::author_geometry_weight_skin(scene, targets, support, options, {},
                                           &report))
    throw std::runtime_error("No actual changes");
  const auto seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
          .count();
  scene.save(out / "bound.dsculpt");
  std::ofstream(out / "report.json")
      << nlohmann::json{{"work", report.work + wrapper_work},
                        {"workspace", report.workspace + wrapper_bytes},
                        {"seconds", seconds},
                        {"contacts", report.field.contacts},
                        {"rows", report.field.rows},
                        {"changed", report.changed},
                        {"partition", report.field.partition},
                        {"kkt", report.field.residual},
                        {"sweeps", report.field.iterations},
                        {"policy", "private optional user-support oracle with "
                                   "explicit user singleton boundaries, original finite axes, no fallback"}}
             .dump(2);
  std::cout << "SAVED candidate contacts=" << report.field.contacts
            << " work=" << report.work + wrapper_work
            << " bytes=" << report.workspace + wrapper_bytes
            << " seconds=" << seconds << std::endl;
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << std::endl;
  return 1;
}
