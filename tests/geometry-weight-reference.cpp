// SPDX-License-Identifier: GPL-3.0-or-later
#include "anatomical-continuation.hpp"
#include "dome/source-placement.hpp"
#include <iostream>
using namespace dome;
namespace {
Mesh strip(float x) {
  Mesh m;
  m.positions = {
      {x - .02f, 0, 0}, {x + .02f, 0, 0}, {x + .02f, 1, 0}, {x - .02f, 1, 0}};
  m.normals.assign(4, {0, 0, 1});
  m.uv = {{{0, 0}}, {{1, 0}}, {{1, 1}}, {{0, 1}}};
  m.indices = {0, 1, 2, 0, 2, 3};
  return m;
}
Scene fixture() {
  Scene s;
  auto reference = AffineTransform{}.matrix();
  reference[0] = 2;
  reference[4] = .5;
  reference[5] = .5;
  reference[12] = 1;
  reference[13] = 2;
  auto target = AffineTransform{}.matrix();
  target[0] = -2;
  target[4] = .5;
  target[5] = .5;
  target[10] = 1.5;
  target[12] = -2;
  target[13] = 1;
  target[14] = -.5;
  const AffineTransform a(reference), b(target);
  const std::function<bool()> never = [] { return false; };
  detail::SkinGeometryWork work(50000000, never);
  auto left = detail::skin_world_mesh(strip(-.2f), a.inverse(), work),
       right = detail::skin_world_mesh(strip(.2f), b.inverse(), work);
  s.active().name = "Actual native reference";
  s.active().painted = std::make_shared<PaintedMesh>(left, 64);
  s.active().mesh_active = true;
  s.active().affine_edit = std::make_shared<const NativeAffineEdit>(a);
  DeformationState d(left);
  Skeleton rig;
  rig.bones = {{"authored left", -1, {{-.2f, 0, -.05f}}, 1, true},
               {"left endpoint", 0, {{0, 1, 0}}, .1f, false},
               {"authored right", -1, {{.2f, 0, -.05f}}, 1, true},
               {"right endpoint", 2, {{0, 1, 0}}, .1f, false}};
  rig.space_matrix = a.inverse().float_matrix();
  d.skeleton = rig;
  for (const auto &bone : rig.bones)
    d.pose.push_back(bone.rest);
  d.pose[0].rotation = {float(std::cos(.1)), 0, 0, float(std::sin(.1))};
  s.set_deformation(1, d);
  const auto id = s.add("Genuinely unskinned target", Volume{});
  auto &o = s.object(id);
  o.painted = std::make_shared<PaintedMesh>(right, 64);
  o.mesh_active = true;
  o.affine_edit = std::make_shared<const NativeAffineEdit>(b);
  DeformationState morph(right);
  morph.morphs = MorphSet(right);
  auto altered = right.positions;
  altered[0].z += .01f;
  morph.morphs->set_target(right, "Current target detail", altered);
  morph.morph_weights = {.35f};
  s.set_deformation(id, morph);
  s.select(id);
  return s;
}
} // namespace
int main() try {
  auto s = fixture();
  const auto before = s.encode();
  detail::DeclaredWeightSupport support{
      detail::DeclaredSupportPolicy::NearestSourceUnion,
      true,
      {{1, {}}, {2, {}}},
      1};
  auto fields = detail::anatomical_continuation(
      s, 0, {}, detail::BoneWallDomain::ConnectedSourceSurface,
      {500000000, 256u * 1024u * 1024u}, &support);
  if (fields.parts.size() != 2 ||
      fields.joints != std::vector<uint32_t>{0, 2} || s.encode() != before ||
      s.object(2).deformation->skeleton || s.object(2).deformation->skin)
    throw std::runtime_error(
        "Actual unskinned/reference/source preservation failed");
  double minimum = 1;
  for (const auto &part : fields.parts)
    for (auto row : part.raw_to_row) {
      auto own = fields.fields[row * 2 + (part.object->id == 2)];
      minimum = std::min(minimum, own);
      if (own < .9)
        throw std::runtime_error("Actual relative-frame default support failed "
                                 "fixed own-limb bound");
    }
  auto cancelled = s.snapshot();
  bool cancelled_ok = false;
  auto held = cancelled.object(1).deformation;
  const auto owners = held.use_count();
  try {
    (void)detail::anatomical_continuation(
        cancelled, 0, [&] { return held.use_count() >= owners + 2; },
        detail::BoneWallDomain::ConnectedSourceSurface,
        {500000000, 256u * 1024u * 1024u}, &support);
  } catch (const std::runtime_error &) {
    cancelled_ok = true;
  }
  if (!cancelled_ok || cancelled.encode() != before)
    throw std::runtime_error(
        "Captured-reference deterministic cancellation failed");
  auto stale = s.snapshot();
  auto owner = stale.object(1).deformation;
  const auto old_count = owner.use_count();
  bool replaced = false, rejected = false;
  try {
    (void)detail::anatomical_continuation(
        stale, 0,
        [&] {
          if (!replaced && owner.use_count() >= old_count + 2) {
            auto changed = *stale.object(1).deformation;
            changed.pose[0].translation.z += .01f;
            stale.set_deformation(1, changed);
            replaced = true;
          }
          return false;
        },
        detail::BoneWallDomain::ConnectedSourceSurface,
        {500000000, 256u * 1024u * 1024u}, &support);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  if (!replaced || !rejected ||
      stale.encode()["objects"][1] != before["objects"][1])
    throw std::runtime_error(
        "Changed actual reference cannot authorize stale target fields");
  auto foreign = s.snapshot();
  auto bad = *foreign.object(2).deformation;
  bad.skeleton = foreign.object(1).deformation->skeleton;
  bad.skeleton->bones[0].name = "Different authored identity";
  foreign.set_deformation(2, bad);
  const auto foreign_before = foreign.encode();
  bool refused = false;
  try {
    (void)detail::anatomical_continuation(
        foreign, 0, {}, detail::BoneWallDomain::ConnectedSourceSurface,
        {500000000, 256u * 1024u * 1024u}, &support);
  } catch (const std::invalid_argument &) {
    refused = true;
  }
  if (!refused || foreign.encode() != foreign_before)
    throw std::runtime_error("Unrelated existing rig was silently mapped");
  std::cout << "PASS actual_unskinned_relative_affine_reference=1 minimum_own="
            << minimum
            << " cancel=1 stale_reference=1 unrelated_rig_refused=1 "
               "source_pose_morph_exact=1 work="
            << fields.report.work << " bytes=" << fields.report.bytes << '\n';
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
