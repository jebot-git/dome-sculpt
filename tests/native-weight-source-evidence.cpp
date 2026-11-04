// SPDX-License-Identifier: GPL-3.0-or-later
#define main contact_quality_main
#include "native-weight-contact-stress.cpp"
#undef main
int main() try {
  size_t checks = 0;
  auto check = [&](bool v, const char *s) {
    ++checks;
    if (!v)
      throw std::runtime_error(s);
  };
  const std::function<bool()> never = [] { return false; };
  detail::weight_surface::Budget work{0, 50000000, never};
  detail::HeatBindingOptions options;
  auto source = fixture(0, false);
  auto original = source.encode();
  auto profile = detail::declared_weight_profile(
      *source.active().deformation->skeleton, work,
      detail::DeclaredRigGeometry::NativeBoneAxes);
  auto intent = [&](detail::DeclaredSupportPolicy policy,
                    const detail::DeclaredWeightPart &part) {
    return detail::declared_weight_intent(
        source.active(), part, profile, options, work, policy,
        source.active().painted->mesh(), source.active().deformation);
  };
  auto unique =
      intent(detail::DeclaredSupportPolicy::UniqueSourceWitnesses, {1, {}});
  auto old = intent(detail::DeclaredSupportPolicy::NearestSourceUnion, {1, {}});
  for (size_t i = 0; i < 8; ++i) {
    check(unique.masks(source.active())[i][0] == (i < 4 ? 1 : 2),
          "Foreign boundary tie became owning component evidence");
    check(old.masks(source.active())[i][0] == 3,
          "Preserved old nearest-union control is not sensitive");
  }
  check(source.encode() == original,
        "Source evidence changed native source arrays/state");
  auto ambiguous = source.snapshot();
  auto d = *ambiguous.active().deformation;
  d.skeleton->bones[0].rest.translation.x = 0;
  d.skeleton->bones[1].rest.translation.x = 0;
  ambiguous.set_deformation(1, d);
  auto both = detail::declared_weight_profile(
      *ambiguous.active().deformation->skeleton, work,
      detail::DeclaredRigGeometry::NativeBoneAxes);
  bool refused = false;
  try {
    (void)detail::declared_weight_intent(
        ambiguous.active(), {1, {}}, both, options, work,
        detail::DeclaredSupportPolicy::UniqueSourceWitnesses,
        ambiguous.active().painted->mesh(), ambiguous.active().deformation);
  } catch (const std::invalid_argument &) {
    refused = true;
  }
  check(refused, "All-tied component silently guessed joint intent");
  auto declared = detail::declared_weight_intent(
      ambiguous.active(), {1, {{0, {0}}, {4, {1}}}}, both, options, work,
      detail::DeclaredSupportPolicy::UniqueSourceWitnesses,
      ambiguous.active().painted->mesh(), ambiguous.active().deformation);
  for (size_t i = 0; i < 8; ++i)
    check(declared.masks(ambiguous.active())[i][0] == (i < 4 ? 1 : 2),
          "Explicit genuine user component intent lost exact joint identity");
  auto changed = *source.active().deformation;
  changed.pose.clear();
  for (auto b : changed.skeleton->bones)
    changed.pose.push_back(b.rest);
  changed.pose[0].translation.z += .01f;
  source.set_deformation(1, changed);
  refused = false;
  try {
    (void)unique.masks(source.active());
  } catch (const std::logic_error &) {
    refused = true;
  }
  check(refused,
        "Changed strong reference/target owner retained stale evidence");
  for (int bad = 0; bad < 3; ++bad) {
    auto s = fixture(0, false);
    const auto before = s.encode();
    detail::DeclaredWeightSupport support{
        detail::DeclaredSupportPolicy::UniqueSourceWitnesses,
        true,
        {{1, {}}},
        1};
    support.row_anchors = detail::DeclaredRowAnchors::NearestPermitted;
    if (bad == 0)
      support.policy = static_cast<detail::DeclaredSupportPolicy>(99);
    if (bad == 1)
      support.rig_geometry = static_cast<detail::DeclaredRigGeometry>(99);
    if (bad == 2)
      support.row_anchors = static_cast<detail::DeclaredRowAnchors>(99);
    refused = false;
    try {
      (void)detail::anatomical_continuation(
          s, 0, {}, detail::BoneWallDomain::ConnectedSourceSurface,
          {50000000, 64u * 1024u * 1024u}, &support);
    } catch (const std::invalid_argument &) {
      refused = true;
    }
    check(refused && s.encode() == before,
          "Unknown policy silently chose another construction");
  }
  auto unsupported = fixture(0, false);
  d = *unsupported.active().deformation;
  d.skeleton->bones.push_back(
      {"authored deform auxiliary", -1, {{2, 0, 0}}, 1, true});
  unsupported.set_deformation(1, d);
  const auto unsupported_before = unsupported.encode();
  detail::DeclaredWeightSupport support{
      detail::DeclaredSupportPolicy::UniqueSourceWitnesses, true, {{1, {}}}, 1};
  support.row_anchors = detail::DeclaredRowAnchors::NearestPermitted;
  refused = false;
  try {
    (void)detail::anatomical_continuation(
        unsupported, 0, {}, detail::BoneWallDomain::ConnectedSourceSurface,
        {50000000, 64u * 1024u * 1024u}, &support);
  } catch (const std::runtime_error &e) {
    refused = std::string(e.what()).find("no declared group support bone=2") !=
              std::string::npos;
  }
  check(refused && unsupported.encode() == unsupported_before,
        "Unsupported deform auxiliary was silently dropped");
  d.skeleton->bones.back().deform = false;
  unsupported.set_deformation(1, d);
  const auto f = detail::anatomical_continuation(
      unsupported, 0, {}, detail::BoneWallDomain::ConnectedSourceSurface,
      {50000000, 64u * 1024u * 1024u}, &support);
  check(f.joints == std::vector<uint32_t>{0, 1},
        "Genuine current nondeform flag was ignored");
  std::cout << "PASS component_evidence_checks=" << checks
            << " work=" << work.used << '\n';
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
