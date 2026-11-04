// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/rig-rest.hpp"
#include "dome/skin-authoring.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
Scene fixture() {
  Mesh m;
  m.positions = {
      {-.3f, 1.1f, -.8f}, {.3f, 1.4f, -.8f}, {0, 1.9f, -.8f}, {0, 1.45f, -.8f}};
  m.indices = {0, 1, 2, 3, 1, 2};
  m.normals.assign(4, {0, 0, 1});
  m.uv = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
  Scene s;
  s.active().name = "Actual native finite reference";
  s.active().painted = std::make_shared<PaintedMesh>(m, 64);
  s.active().mesh_active = true;
  s.active().visible = false;
  DeformationState ref(m);
  const Quat axis{.70710678f, .70710678f, 0, 0};
  ref.skeleton = Skeleton{{{"Native authored finite handle",
                            -1,
                            {{-.4f, 1.f, -.9f}, axis},
                            .2f,
                            true}}};
  s.set_deformation(1, ref);
  auto id = s.add("Actual unskinned isolated-contact target", Volume{});
  auto &o = s.object(id);
  o.painted = s.object(1).painted->snapshot();
  o.mesh_active = true;
  DeformationState target(m);
  target.morphs = MorphSet(m);
  auto moved = m.positions;
  for (auto &p : moved)
    p.z += .001f;
  target.morphs->set_target(m, "Original artist detail", moved);
  target.morph_weights = {.25f};
  s.set_deformation(id, target);
  s.select(id);
  s.validate();
  return s;
}
nlohmann::json inspect(const Scene &s) {
  const auto &o = s.active();
  return {{"scene", s.encode()},
          {"name", o.name},
          {"bound", bool(o.deformation && o.deformation->skin)},
          {"rig_source", bool(o.rig_source)}};
}
int main(int argc, char **argv) try {
  if (argc != 3)
    throw std::invalid_argument("Choose operation and path");
  std::string_view op = argv[1];
  if (op == "--ui-fixture") {
    fixture().save(argv[2]);
    return 0;
  }
  auto s = Scene::load(argv[2]);
  if (op == "--inspect") {
    std::cout << inspect(s).dump() << std::endl;
    return 0;
  }
  if (op == "--restore") {
    restore_rig_source(s, s.active().id, false);
    std::cout << inspect(s).dump() << std::endl;
    return 0;
  }
  SkinAuthoringOptions o;
  o.mode = SkinAuthoringMode::NativeVisible;
  o.reference = 1;
  if (op == "--pointwise") {
    const auto before = s.encode();
    bool failed = false;
    try {
      author_native_visible_skin(s, std::array<uint64_t, 1>{s.active().id}, o);
    } catch (const std::runtime_error &e) {
      failed = std::string(e.what()).find("origin/coplanar/grazing") !=
               std::string::npos;
    }
    if (!failed || s.encode() != before)
      throw std::runtime_error(
          "Actual pointwise negative must refuse atomically");
    std::cout << "PASS genuine pointwise origin negative retained" << std::endl;
    return 0;
  }
  if (op != "--expected")
    throw std::invalid_argument("Unknown operation");
  o.native_visible.contact_handling = NativeContactHandling::FiniteLineIntegral;
  NativeVisibleSkinReport r;
  if (!author_native_visible_skin(s, std::array<uint64_t, 1>{s.active().id}, o,
                                  {}, &r) ||
      !r.measure_zero_origins)
    throw std::runtime_error("Actual owning finite-line output must use an "
                             "authenticated isolated-contact certificate");
  std::cout << inspect(s).dump() << std::endl;
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << std::endl;
  return 1;
}
