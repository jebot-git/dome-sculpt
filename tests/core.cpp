// SPDX-License-Identifier: GPL-3.0-or-later
#include "controller-profiles.h"
#include "dome/input.hpp"
#include "dome/mesh.hpp"
#include "dome/navigation.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <unistd.h>
using namespace dome;
namespace {
int checks = 0;
void check(bool ok, const char *name) {
  if (!ok)
    throw std::runtime_error(name);
  ++checks;
  std::cout << "PASS " << name << '\n';
}
template <class F> bool fails(F f) {
  try {
    f();
    return false;
  } catch (const std::exception &) {
    return true;
  }
}
void add(Volume &v, Vec3 from = {}, Vec3 to = {}, float radius = .35f,
         Tool tool = Tool::Clay) {
  v.begin_stroke();
  Brush b;
  b.from = from;
  b.to = to;
  b.radius = radius;
  b.tool = tool;
  b.blend = 0;
  v.stamp(b);
  v.end_stroke();
}
bool watertight(const Mesh &mesh) {
  std::map<std::pair<uint32_t, uint32_t>, int> edges;
  for (size_t i = 0; i < mesh.indices.size(); i += 3)
    for (int k = 0; k < 3; ++k) {
      auto a = mesh.indices[i + k], b = mesh.indices[i + (k + 1) % 3];
      if (a > b)
        std::swap(a, b);
      ++edges[{a, b}];
    }
  return !edges.empty() && std::all_of(edges.begin(), edges.end(),
                                       [](auto e) { return e.second == 2; });
}
uint32_t word(const std::vector<uint8_t> &b, size_t at) {
  return uint32_t(b.at(at)) | uint32_t(b.at(at + 1)) << 8 |
         uint32_t(b.at(at + 2)) << 16 | uint32_t(b.at(at + 3)) << 24;
}
} // namespace
int main() try {
  Volume field(.05f);
  check(field.voxels().empty() && field.sample({}) > 0,
        "A new sparse volume is empty");
  check(fails([] { Volume bad(0); }) && fails([] { Volume bad(NAN); }),
        "Invalid voxel sizes are rejected");
  add(field, {}, {}, .35f);
  check(field.sample({}) < 0 && field.sample({.6f, 0, 0}) > 0,
        "Clay creates a signed solid");
  auto solid = field.voxels();
  check(field.undo() && field.voxels().empty(),
        "Undo removes the complete brush transaction");
  check(field.redo() && field.voxels() == solid,
        "Redo exactly restores every changed sample");
  field.begin_stroke();
  Brush carve;
  carve.radius = .16f;
  carve.blend = 0;
  carve.tool = Tool::Carve;
  field.stamp(carve);
  field.cancel_stroke();
  check(field.voxels() == solid,
        "Cancelling a stroke restores the complete volume");
  add(field, {}, {}, .16f, Tool::Carve);
  check(field.sample({}) > 0 && field.sample({.25f, 0, 0}) < 0,
        "Carve creates a cavity without erasing its outer wall");
  field.undo();
  field.begin_stroke();
  Brush zero;
  zero.pressure = 0;
  field.stamp(zero);
  check(!field.end_stroke() && field.voxels() == solid,
        "Zero stylus pressure produces no edit or undo entry");
  field.begin_stroke();
  Brush bad;
  bad.radius = NAN;
  check(fails([&] { field.stamp(bad); }) && field.voxels() == solid,
        "Invalid brush input cannot partially mutate the volume");
  field.cancel_stroke();
  Volume sweep(.05f);
  add(sweep, {-.4f, 0, 0}, {.4f, 0, 0}, .12f);
  check(sweep.sample({}) < 0 && sweep.sample({-.39f, 0, 0}) < 0 &&
            sweep.sample({.39f, 0, 0}) < 0,
        "Fast strokes fill the swept capsule without gaps");
  auto hit = field.raycast({0, 0, 2}, {0, 0, -1});
  check(hit && std::abs(hit->z - .35f) < .03f,
        "Surface ray picking locates the sculpted solid");
  Mesh mesh = surface_mesh(field);
  mesh.validate();
  check(mesh.positions.size() > 100 && mesh.indices.size() > 300,
        "Sparse field extraction produces a nondegenerate triangle surface");
  std::map<std::pair<uint32_t, uint32_t>, int> edges;
  double volume = 0;
  for (size_t i = 0; i < mesh.indices.size(); i += 3) {
    auto a = mesh.indices[i], b = mesh.indices[i + 1], c = mesh.indices[i + 2];
    volume +=
        dot(mesh.positions[a], cross(mesh.positions[b], mesh.positions[c])) /
        6.;
    for (auto e : {std::pair{a, b}, std::pair{b, c}, std::pair{c, a}}) {
      if (e.first > e.second)
        std::swap(e.first, e.second);
      ++edges[e];
    }
  }
  check(std::all_of(edges.begin(), edges.end(),
                    [](auto e) { return e.second == 2; }),
        "Extracted sphere is watertight at exact-zero lattice intersections");
  check(volume > .14 && volume < .20,
        "Surface winding is outward and encloses the expected volume");
  Volume tunnel(.04f);
  add(tunnel);
  add(tunnel, {0, 0, -.5f}, {0, 0, .5f}, .13f, Tool::Carve);
  auto tunnel_mesh = surface_mesh(tunnel);
  tunnel_mesh.validate();
  check(watertight(tunnel_mesh),
        "A carved through-hole retains closed manifold surface topology");
  auto atlas = unwrap(mesh, 512, 4);
  check(atlas.charts > 0 && atlas.width > 0 && atlas.utilization > .3f,
        "Automatic atlas generates compact charts");
  check(mesh.uv.size() == mesh.positions.size() &&
            std::all_of(mesh.uv.begin(), mesh.uv.end(),
                        [](auto uv) {
                          return uv[0] >= 0 && uv[0] <= 1 && uv[1] >= 0 &&
                                 uv[1] <= 1;
                        }),
        "UV seams retain matching vertices and bounded texture coordinates");
  Volume detail(.04f);
  detail.begin_stroke();
  Brush soft;
  soft.radius = .4f;
  detail.stamp(soft);
  detail.end_stroke();
  Mesh detail_mesh = surface_mesh(detail);
  auto triangle_count = detail_mesh.indices.size();
  unwrap(detail_mesh, 512);
  check(detail_mesh.indices.size() == triangle_count,
        "Atlas normalization preserves tiny metre-scale sculpt triangles "
        "without dropping faces");
  auto dir = std::filesystem::temp_directory_path() /
             ("dome-sculpt-test-" + std::to_string(getpid()));
  std::filesystem::create_directory(dir);
  field.save(dir / "sculpt.dsculpt");
  auto restored = Volume::load(dir / "sculpt.dsculpt");
  check(restored.voxels() == field.voxels() &&
            restored.voxel_size() == field.voxel_size(),
        "Native project persistence preserves the exact field");
  auto malformed = nlohmann::json{{"format", "dome-sculpt"},
                                  {"version", 1},
                                  {"voxel_size", .05},
                                  {"samples", {{0.5, 0, 0, 0.0}}}};
  auto write_bad = [&] {
    auto data = nlohmann::json::to_cbor(malformed);
    std::ofstream out(dir / "bad.dsculpt", std::ios::binary);
    out.write(reinterpret_cast<const char *>(data.data()), data.size());
  };
  write_bad();
  check(fails([&] { Volume::load(dir / "bad.dsculpt"); }),
        "Project loading rejects fractional lattice coordinates");
  malformed["samples"][0][0] = UINT64_MAX;
  write_bad();
  check(fails([&] { Volume::load(dir / "bad.dsculpt"); }),
        "Project loading rejects oversized integer coordinates before "
        "conversion");
  export_glb(mesh, dir / "sculpt.glb");
  std::ifstream f(dir / "sculpt.glb", std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), {});
  check(word(bytes, 0) == 0x46546c67 && word(bytes, 4) == 2 &&
            word(bytes, 8) == bytes.size() && word(bytes, 16) == 0x4e4f534a,
        "GLB has valid little-endian headers and aligned chunks");
  auto json = nlohmann::json::parse(bytes.begin() + 20,
                                    bytes.begin() + 20 + word(bytes, 12));
  check(json["accessors"][0]["count"] == mesh.positions.size() &&
            json["meshes"][0]["primitives"][0]["attributes"].contains(
                "TEXCOORD_0"),
        "glTF includes geometry, normals, UVs and a PBR material");
  check(fails([&] { export_glb(mesh, dir / "invalid.vrm"); }),
        "A static mesh is never misrepresented as a complete VRM avatar");
  Vec3 start = trackball(0, 0), end = trackball(.4f, .2f);
  Quat rotation = arcball(start, end);
  check(length(rotation.rotate(start) - end) < 1e-5f,
        "Trackball rotation follows the sphere/hyperbola navigation basis");
  Volume tracked(.04f);
  SpatialInput input(tracked);
  SpatialSample s;
  s.id = 1;
  s.sequence = 1;
  s.time = 1;
  s.tracked = s.focused = true;
  s.trigger = true;
  s.pressure = 1;
  input.update(s);
  check(tracked.voxels().empty(),
        "Acquiring a held trigger cannot create an unintended stroke");
  s.trigger = false;
  s.sequence++;
  s.time += .01;
  input.update(s);
  s.trigger = true;
  s.sequence++;
  s.time += .01;
  input.update(s);
  s.tip = {.2f, 0, 0};
  s.sequence++;
  s.time += .01;
  input.update(s);
  s.trigger = false;
  s.sequence++;
  s.time += .01;
  input.update(s);
  check(tracked.undo_count() == 1 && tracked.sample({.1f, 0, 0}) < 0,
        "Tracked controller poses form one undoable pressure-sensitive sweep");
  auto before = tracked.voxels();
  s.trigger = true;
  s.tip = {0, .3f, 0};
  s.sequence++;
  s.time += .01;
  input.update(s);
  s.focused = false;
  s.sequence++;
  input.update(s);
  check(tracked.voxels() == before,
        "Focus loss cancels a partially completed spatial stroke");
  s.focused = true;
  s.trigger = false;
  s.joystick_press = false;
  s.sequence++;
  s.time += .01;
  input.update(s);
  s.joystick_press = true;
  s.sequence++;
  s.time += .01;
  input.update(s);
  check(input.menu_open(), "Joystick press spawns a spatial tool menu");
  s.joystick_press = false;
  s.stick_y = 1;
  s.stick_x = 1;
  s.sequence++;
  s.time += .01;
  input.update(s);
  s.joystick_press = true;
  s.sequence++;
  s.time += .01;
  input.update(s);
  check(!input.menu_open() && input.tool() == Tool::Carve,
        "Joystick direction and a second press select the carving tool");
  Volume dual(.04f);
  SpatialInput dual_input(dual);
  SpatialSample left{};
  left.id = 1;
  left.sequence = 1;
  left.time = 1;
  left.tracked = left.focused = true;
  SpatialSample right = left;
  right.id = 2;
  right.kind = DeviceKind::SpatialStylus;
  dual_input.update(left);
  dual_input.update(right);
  right.trigger = true;
  right.pressure = .75f;
  right.sequence++;
  right.time += .01;
  dual_input.update(right);
  left.sequence++;
  left.time += .01;
  dual_input.update(left);
  right.trigger = false;
  right.sequence++;
  right.time += .01;
  dual_input.update(right);
  check(dual.undo_count() == 1 && !dual.voxels().empty(),
        "An idle left hand cannot block a right-hand spatial stylus stroke");
  auto dual_before = dual.voxels();
  right.trigger = true;
  right.tip = {0, .2f, 0};
  right.sequence++;
  right.time += .01;
  dual_input.update(right);
  right.sequence++;
  right.time += .5;
  dual_input.update(right);
  check(dual.voxels() == dual_before && !dual.in_stroke(),
        "A tracking sample gap cancels the stroke and disarms a held stylus");
  CanvasNavigation navigation;
  CanvasTransform transform;
  std::array<TrackedHand, 2> hands{};
  hands[0].valid = true;
  hands[0].squeeze = 1;
  navigation.update(hands, transform);
  hands[0].grip.position = {.2f, 0, 0};
  auto moved = navigation.update(hands, transform);
  check(moved && std::abs(moved->position.x - .2f) < 1e-5f,
        "OpenBrush one-hand canvas grab preserves relative motion");
  hands[1].valid = true;
  hands[1].squeeze = 1;
  hands[0].grip.position = {-.2f, 0, 0};
  hands[1].grip.position = {.2f, 0, 0};
  navigation.reset();
  navigation.update(hands, transform);
  hands[0].grip.position = {-.4f, 0, 0};
  hands[1].grip.position = {.4f, 0, 0};
  auto scaled = navigation.update(hands, transform);
  check(scaled && std::abs(scaled->scale - 2) < 1e-5f,
        "OpenBrush two-hand canvas grab scales about the shared midpoint");
  check(scaled && length(world_to_model(*scaled, {.4f, 0, 0}) -
                         Vec3{.2f, 0, 0}) < 1e-5f,
        "Tracked brush positions map into the scaled canvas coordinate system");
  auto profiles = ob::controllerProfiles({"XR_META_touch_controller_plus"});
  auto touch = std::find_if(profiles.begin(), profiles.end(), [](auto p) {
    return p.path.find("touch_controller_plus") != std::string::npos;
  });
  auto bindings = touch != profiles.end()
                      ? ob::controllerBindings(*touch)
                      : std::vector<ob::ControllerBinding>{};
  check(touch != profiles.end() &&
            std::any_of(bindings.begin(), bindings.end(),
                        [](auto b) {
                          return b.action == "recenter" &&
                                 b.path ==
                                     "/user/hand/right/input/thumbstick/click";
                        }),
        "Extension-gated OpenBrush Touch Plus joystick bindings remain "
        "available");
  check(fails([&] { field.sample({NAN, 0, 0}); }) &&
            fails([&] { field.raycast({}, {0, 0, 0}); }),
        "Invalid or zero-direction picking input is rejected");
  Volume tiny(.00001f);
  tiny.begin_stroke();
  Brush huge;
  huge.radius = 5;
  huge.from = {-100, -100, -100};
  huge.to = {100, 100, 100};
  check(fails([&] { tiny.stamp(huge); }) && tiny.voxels().empty(),
        "Oversized sweeps fail before sample allocation or integer overflow");
  tiny.cancel_stroke();
  std::filesystem::remove_all(dir);
  std::cout << checks << " checks passed using " << compute_backend() << '\n';
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
