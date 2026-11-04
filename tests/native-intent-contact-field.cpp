// SPDX-License-Identifier: GPL-3.0-or-later
// Explicit private candidate producer. No original artist BIN/weights input.
#include "dome/rig-rest.hpp"
#include "geometry-weight-authoring.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
int main(int argc, char **argv) try {
  if (argc != 3)
    throw std::runtime_error("Choose authentic fitted unskinned Scene and "
                             "candidate output directory");
  const std::filesystem::path out = argv[2];
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
  support.contact_support = detail::DeclaredContactSupport::SharedSimplex;
  support.rig_geometry = detail::DeclaredRigGeometry::NativeBoneAxes;
  for (auto id : targets)
    support.parts.push_back({id, {}});
  const size_t wrapper_work = 4096 + targets.size() * 32,
               wrapper_bytes =
                   sizeof(support) +
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
                        {"policy", "private shared-label constrained simplex with "
                                   "original finite axes, no fallback"}}
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
