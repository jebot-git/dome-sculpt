// SPDX-License-Identifier: GPL-3.0-or-later
#include "heat-component-intent.hpp"
#include "dome/creation.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
using namespace dome::detail;
int main(int argc, char **argv) try {
  Scene scene;
  bool male = argc > 1 && std::string(argv[1]) == "male";
  append_humanoid(scene,
                  male ? humanoid_baseline_defaults("vroid-beta-male")
                       : HumanoidControls{},
                  "Component intent", {}, 64);
  auto before = scene.encode();
  size_t parts = 0, components = 0, skin_checks = 0;
  for (const auto &o : scene.objects())
    if (o.painted && o.deformation && o.deformation->skin) {
      HeatBindingOptions options;
      auto seams = capture_heat_seams(o, options);
      auto certificate = capture_imported_component_intent(o, seams, options);
      if (certificate.kind() !=
              HeatComponentIntentKind::OriginalPositiveUsage ||
          !certificate.matches(o))
        throw std::runtime_error("Imported intent kind/owner lost");
      auto masks = certificate.masks(o);
      const auto &rig = *o.deformation->skeleton;
      for (size_t v = 0; v < masks.size(); ++v)
        for (auto value : o.deformation->skin->vertices[v])
          if (value.weight > 0 && rig.bones[value.joint].deform) {
            ++skin_checks;
            if (!(masks[v][value.joint / 64] &
                  (uint64_t(1) << (value.joint % 64))))
              throw std::runtime_error("Actual source-supported joint missing "
                                       "from component intent");
          }
      auto foreign = o;
      foreign.deformation = std::make_shared<DeformationState>(*o.deformation);
      if (certificate.matches(foreign))
        throw std::runtime_error("Equal bytes foreign deform owner admitted");
      bool refused = false;
      try {
        certificate.masks(foreign);
      } catch (const std::invalid_argument &) {
        refused = true;
      }
      if (!refused)
        throw std::runtime_error("Stale component intent exposed masks");
      std::cout << "PART " << o.id << " components=" << certificate.components()
                << " work=" << certificate.work() << '\n';
      ++parts;
      components += certificate.components();
    }
  if (parts != 19 || scene.encode() != before)
    throw std::runtime_error("Component intent changed actual source Scene");
  std::cout << "PASS parts=" << parts << " components=" << components
            << " current_joint_support_checks=" << skin_checks
            << " exact_source/foreign_owner\n";
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
