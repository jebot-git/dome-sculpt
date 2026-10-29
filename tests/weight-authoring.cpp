// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/creation.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
void check(bool ok, const char *s) { if (!ok) throw std::runtime_error(s); std::cout << "PASS " << s << '\n'; }
template<class F> bool rejects(F f) { try { f(); return false; } catch (...) { return true; } }
Scene fixture() {
  Mesh m; m.positions = {{-.5f, -.5f, 0}, {.5f, -.5f, 0}, {0, .5f, 0}};
  m.normals.assign(3, {0, 0, 1}); m.indices = {0, 1, 2}; m.uv = {{{0, 0}}, {{1, 0}}, {{.5f, 1}}};
  Scene s; auto group = s.add_group("Weight rig"); auto &o = s.active(); o.group = group;
  o.painted = std::make_shared<PaintedMesh>(m, 64); o.mesh_active = true;
  DeformationState d(m); d.skeleton = Skeleton{{{"root", -1, {}, .5f}, {"tip", 0, {{0, .5f, 0}}, .2f}}};
  d.skin = SkinWeights{}; d.skin->vertices.assign(3, {{0, 1}}); s.set_deformation(o.id, d);
  s.duplicate(1); s.select(1); return s;
}
float value(const Scene &s, uint64_t object, size_t vertex, uint32_t joint) {
  for (auto i : s.object(object).deformation->skin->vertices.at(vertex)) if (i.joint == joint) return i.weight;
  return 0;
}
WeightToolSettings tool() { WeightToolSettings s; s.enabled = true; s.strength = .5f; s.mode = WeightBrushMode::Replace; return s; }
Brush stamp(Vec3 p) { Brush b; b.from = b.to = p; b.radius = .2f; b.normal = {0, 0, 1}; b.pressure = 1; return b; }
void bake_active(Scene &scene) {
  auto painted = std::make_shared<PaintedMesh>(*scene.active().painted);
  auto image = [](std::array<uint8_t,4> color) {
    Image result{64,64,std::vector<uint8_t>(64*64*4)};
    for (size_t i=0;i<result.rgba.size();++i) result.rgba[i]=color[i%4];
    return result;
  };
  BakedTextures bake;
  bake.maps.color=image({200,150,100,255}); bake.maps.metal_rough=image({255,128,0,255});
  bake.maps.emission=image({0,0,0,255}); bake.maps.normal=image({128,128,255,255});
  bake.coverage.assign(64*64,255); painted->install_bake(std::move(bake));
  scene.active().painted=std::move(painted);
}
struct Controller {
  Document &doc; SpatialFrame f;
  Controller(Document &d) : doc(d) { f.focused = f.head_valid = true; f.head.position = {0, 1.6f, 0}; f.hands[1].tracked.valid = true; send(); }
  void send(float pressure = 0, float x = 0, float y = 0, bool click = false) {
    ++f.sequence; f.time += .02; auto &h = f.hands[1]; h.tracked.trigger = pressure; h.x = x; h.y = y; h.joystick = click;
    doc.submit(f); if (!doc.wait_input()) throw std::runtime_error("Input timeout");
  }
  void sector(int i) { float a = i * 3.14159265f / 6; send(0, std::cos(a), std::sin(a)); send(0, std::cos(a), std::sin(a), true); send(); }
  void weights() { send(); send(0, 0, 0, true); send(); sector(8); sector(10); sector(5); sector(10); }
};
} // namespace
int main(int argc, char **argv) try {
  if (argc == 3 && std::string_view(argv[1]) == "--ui-fixture") { fixture().save(argv[2]); return 0; }
  auto original = fixture(); auto source = original.encode();
  WeightBrush b; b.from = b.to = original.active().painted->mesh().positions[0]; b.radius = .2f; b.strength = 1; b.joint = 1;
  auto once = original.snapshot(), dense = original.snapshot(); auto settings = tool();
  auto paint = once.active().painted;
  auto result = paint_scene_weights(once, 1, std::span<const WeightBrush>(&b, 1), settings);
  std::vector<WeightBrush> repeated(100, b); paint_scene_weights(dense, 1, repeated, settings);
  check(once.encode() == dense.encode(), "One hundred repeated frame samples equal one geometric weight gesture exactly");
  check(std::abs(value(once, 1, 0, 1) - .5f) < 1e-6f && value(once, 1, 1, 1) == 0 && result.changed == 2,
        "Independent capsule expectation changes only the covered vertex on both linked parts");
  check(original.encode() == source && once.active().painted == paint,
        "Weight editing retains rest geometry, paint and immutable source snapshots");
  auto moved = original.snapshot(); moved.group(moved.active().group).transform = {{1, 2, 3}, {0, .70710678f, 0, .70710678f}, 2};
  paint_scene_weights(moved, 1, std::span<const WeightBrush>(&b, 1), settings);
  check(std::abs(value(moved, 2, 0, 1) - .5f) < 1e-6f,
        "Common rig-local stroke applies once through rotated scaled group placement");
  auto hidden = original.snapshot(); hidden.object(2).visible = false; paint_scene_weights(hidden, 1, std::span<const WeightBrush>(&b, 1), settings);
  check(value(hidden, 1, 0, 1) == .5f && value(hidden, 2, 0, 1) == 0, "Hidden linked parts retain their original weights");
  auto locked = original.snapshot(); settings.locks = {0, 1}; auto constrained = paint_scene_weights(locked, 1, std::span<const WeightBrush>(&b, 1), settings);
  check(!constrained.changed && constrained.constrained == 2 && locked.encode() == source,
        "Locked influences remain exact with explicit aggregate constraint reporting");
  settings = tool();
  auto cancelled = original.snapshot(); check(rejects([&] { paint_scene_weights(cancelled, 1, repeated, settings, [] { return true; }); }) && cancelled.encode() == source,
        "Cancelled worker leaves all linked parts exact");
  Document doc; doc.replace(original.snapshot()); doc.weight_settings(settings, 1); check(doc.wait_mesh(), "Weight debug and posed picking snapshot completes on mesh worker");
  check(doc.frame().surface->selected_weights.size() == doc.frame().surface->mesh.positions.size() && doc.frame().surface->picking,
        "Published immutable snapshot includes exact weight attributes and worker-built BVH");
  {
    auto baked=original.snapshot(); bake_active(baked);
    Document combined; combined.replace(std::move(baked)); combined.weight_settings(settings,1);
    check(combined.wait_mesh() && combined.frame().state->error.empty(),
          "Baked normal surfaces and weight visualization publish together on the mesh worker");
    auto surface=combined.frame().surface;
    check(surface->selected_weights.size()==surface->mesh.positions.size() &&
          surface->tangents.size()==surface->mesh.positions.size() && surface->picking &&
          surface->ranges.front().textures && !surface->ranges.front().textures->normal.rgba.empty(),
          "Canonical tangent expansion retains every weight attribute and posed picking geometry");
  }
  {
    Scene avatar; append_humanoid(avatar,humanoid_baseline_defaults("vroid-beta-female"),"CC0",{},64);
    auto id=avatar.active().id; bake_active(avatar);
    auto rest=avatar.active().painted->mesh(); auto appearance=avatar.active().appearance;
    auto source_vertices=appearance->source_vertices;
    Document combined; combined.replace(std::move(avatar)); combined.weight_settings(settings,1);
    check(combined.wait_mesh(std::chrono::seconds(60)) && combined.frame().state->error.empty(),
          "Actual CC0 avatar publishes canonical baked normals with linked weight visualization");
    auto surface=combined.frame().surface;
    auto range=std::find_if(surface->ranges.begin(),surface->ranges.end(),[&](const auto &r){return r.object==id;});
    check(range!=surface->ranges.end() && range->appearance && range->count==rest.indices.size() &&
          range->appearance->source_vertices.size()==rest.indices.size(),
          "Baked avatar render-only corner expansion retains a complete original-image vertex remap");
    for(size_t corner=0;corner<rest.indices.size();++corner)
      if(range->appearance->source_vertices[corner]!=source_vertices.at(rest.indices[corner]))
        throw std::runtime_error("Expanded source UV identity differs from original corner");
    check(surface->selected_weights.size()==surface->mesh.positions.size() &&
          surface->tangents.size()==surface->mesh.positions.size() &&
          combined.copy_scene().object(id).appearance->source_vertices==source_vertices &&
          appearance->source_vertices==source_vertices,
          "Every expanded source identity is exact while authored source records stay immutable");
  }
  check(!doc.frame().state->dirty && doc.begin_desktop(), "Opening weight view changes no saved scene and can acquire a stroke");
  doc.stamp_desktop(stamp(b.from)); doc.cancel_desktop();
  check(doc.copy_scene().encode() == source && !doc.frame().state->dirty, "Cancelled desktop gesture changes no weights or history");
  check(doc.begin_desktop(), "Desktop acquires a second weight gesture");
  for (int i = 0; i < 100; ++i) doc.stamp_desktop(stamp(b.from));
  doc.end_desktop(); check(doc.wait_asset() && doc.frame().state->error.empty(), "Released desktop weight gesture commits on ordered worker");
  check(std::abs(value(doc.copy_scene(), 1, 0, 1) - .5f) < 1e-6f && doc.frame().state->dirty,
        "Desktop gesture persists expected normalized weight independently of sample count");
  auto after = doc.copy_scene().encode();
  check(doc.undo() && doc.copy_scene().encode() == source && doc.redo() && doc.copy_scene().encode() == after,
        "Complete linked weight stroke has one exact global undo/redo entry");
  Document noop; noop.replace(original.snapshot()); settings.locks = {0, 1}; noop.weight_settings(settings, 1); noop.wait_mesh();
  check(noop.begin_desktop(), "Locked-weight fixture acquires tool without changing model"); noop.stamp_desktop(stamp(b.from)); noop.end_desktop(); noop.wait_asset();
  check(noop.frame().state->error.empty() && !noop.frame().state->dirty && !noop.frame().state->can_undo && noop.copy_scene().encode() == source,
        "Wholly constrained gesture reports constraints without creating dirty state or a phantom undo step");
  Document posed; auto scene = original.snapshot(); auto d = *scene.active().deformation;
  d.pose = {d.skeleton->bones[0].rest, d.skeleton->bones[1].rest}; d.pose[0].translation.z = .4f;
  scene.set_deformation(1, d); scene.object(2).visible = false; posed.replace(std::move(scene)); posed.weight_settings(tool(), 1); posed.wait_mesh();
  auto hit = posed.raycast({0, 0, 2}, {0, 0, -1});
  check(hit && std::abs(hit->z - .4f) < 1e-5f, "Desktop weight picking targets displayed posed geometry rather than rest mesh");
  Document xr; xr.replace(original.snapshot()); settings = tool(); xr.weight_settings(settings, 1); xr.wait_mesh(); Controller hand(xr);
  auto p = ob::transform(xr.frame().state->canvas, {b.from.x, b.from.y, b.from.z}); hand.f.hands[1].tracked.grip.position = p;
  hand.send(); for (int i = 0; i < 40; ++i) hand.send(.8f); hand.send();
  check(xr.wait_asset() && xr.frame().state->error.empty() && std::abs(value(xr.copy_scene(), 1, 0, 1) - .4f) < 1e-5f,
        "Actual controller pressure gesture uses the shared once-per-stroke weight transaction");
  check(xr.undo() && xr.copy_scene().encode() == source, "Controller weight gesture undoes exactly");
  xr.wait_mesh(); hand.weights();
  check(xr.frame().state->menu_page == 13, "Joystick reaches native skin-weight tool page");
  hand.sector(7);
  check(xr.frame().state->weights.locks.size() == 2 && xr.frame().state->weights.locks[1], "Joystick locks the selected exact joint influence");
  return 0;
} catch (const std::exception &e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
