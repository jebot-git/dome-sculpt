// SPDX-License-Identifier: GPL-3.0-or-later
// The native importer supplies the single independent cgltf implementation.
#include "cgltf.h"
#include "dome/document.hpp"
#include "dome/creation.hpp"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <unistd.h>
#include <png.h>
using namespace dome;
namespace {
void check(bool ok, const char *s) { if (!ok) throw std::runtime_error(s); std::cout << "PASS " << s << '\n'; }
template<class F> bool rejects(F f) { try { f(); return false; } catch (...) { return true; } }
Scene fixture() {
  Mesh m; m.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}; m.indices = {0, 1, 2};
  m.normals.assign(3, {0, 0, 1}); m.uv = {{{0, 0}}, {{1, 0}}, {{0, 1}}};
  Scene s; s.active().painted = std::make_shared<PaintedMesh>(m, 64);
  s.active().mesh_active = true; s.active().material.color = {1, 0, 0, 1}; return s;
}
auto parse(const std::filesystem::path &path) {
  cgltf_options options{}; cgltf_data *data = nullptr;
  check(cgltf_parse_file(&options, path.c_str(), &data) == cgltf_result_success,
        "Independent parser accepts background export container");
  std::unique_ptr<cgltf_data, decltype(&cgltf_free)> owner(data, cgltf_free);
  check(cgltf_load_buffers(&options, data, path.c_str()) == cgltf_result_success && cgltf_validate(data) == cgltf_result_success,
        "Independent validator accepts emitted mesh, hierarchy and buffer ranges");
  return owner;
}
void finish(std::future<void> result) {
  check(result.wait_for(std::chrono::seconds(120)) == std::future_status::ready, "Queued export completes without requiring UI event pumping");
  result.get();
}
std::vector<char> bytes(const std::filesystem::path &p) {
  std::ifstream in(p, std::ios::binary); return {std::istreambuf_iterator<char>(in), {}};
}
std::array<uint8_t, 4> base_pixel(const cgltf_data &data) {
  const auto &material = data.materials[0];
  auto texture = material.pbr_metallic_roughness.base_color_texture.texture;
  check(texture && texture->image && texture->image->buffer_view,
        "Snapshot export retains an independently readable embedded base-color image");
  auto view = texture->image->buffer_view;
  const auto *input = static_cast<const uint8_t *>(view->buffer->data) + view->offset;
  png_image image{}; image.version = PNG_IMAGE_VERSION;
  check(png_image_begin_read_from_memory(&image, input, view->size), "Independent PNG decoder accepts exported snapshot image");
  image.format = PNG_FORMAT_RGBA; std::vector<uint8_t> pixels(PNG_IMAGE_SIZE(image));
  bool ok = png_image_finish_read(&image, nullptr, pixels.data(), 0, nullptr);
  png_image_free(&image); check(ok && pixels.size() >= 4, "Exported snapshot image has valid decoded RGBA pixels");
  return {pixels[0], pixels[1], pixels[2], pixels[3]};
}
struct Controller {
  Document &doc; SpatialFrame f;
  Controller(Document &d) : doc(d) { f.focused = f.head_valid = true; f.head.position = {0, 1.6f, 0}; f.hands[1].tracked.valid = true; send(); }
  void send(float x = 0, float y = 0, bool press = false) {
    ++f.sequence; f.time += .05; auto &h = f.hands[1]; h.x = x; h.y = y; h.joystick = press;
    doc.submit(f); if (!doc.wait_input()) throw std::runtime_error("Input timeout");
  }
  void sector(int i) { float a = i * 3.14159265f / 6; send(std::cos(a), std::sin(a)); send(std::cos(a), std::sin(a), true); send(); }
};
} // namespace
int main(int argc, char **argv) try {
  if (argc == 3 && std::string_view(argv[1]) == "--ui-fixture") {
    auto scene = fixture(); append_humanoid(scene, humanoid_baseline_defaults("vroid-beta-female"), "CC0", {}, 64);
    scene.save(argv[2]); return 0;
  }
  auto dir = std::filesystem::temp_directory_path() / ("dome-export-" + std::to_string(getpid()));
  std::filesystem::create_directories(dir);
  Document doc; doc.replace(fixture());
  auto export_snapshot = doc.export_asset(dir / "snapshot.glb");
  auto source = doc.copy_scene(); auto &o = source.active(); auto changed = o.material; changed.color = {0, 1, 0, 1};
  doc.object_properties(o.id, o.name, o.visible, o.locked, o.transform, changed);
  finish(std::move(export_snapshot)); auto glb = parse(dir / "snapshot.glb");
  auto pixel = base_pixel(*glb);
  check(glb->materials_count == 1 && pixel[0] > 250 && pixel[1] < 3,
        "Export captures the requested red snapshot while later live edits remain green");
  check(doc.frame().state->dirty && doc.copy_scene().active().material.color[1] == 1,
        "Export never marks unrelated later authoring changes saved");
  auto bad = doc.export_asset(dir / "missing/mesh.glb");
  check(rejects([&] { finish(std::move(bad)); }) && !doc.frame().state->error.empty(),
        "Background file errors reach promise and native status without changing the scene");
  check(rejects([&] { doc.export_asset(dir / "invalid.vrm", AssetExportFormat::Avatar); }),
        "Non-avatar selections cannot queue a misleading VRM export");
  auto scene = fixture(); append_humanoid(scene, humanoid_baseline_defaults("vroid-beta-female"), "CC0", {}, 64);
  Document avatar; avatar.replace(std::move(scene));
  check(avatar.frame().state->can_export_avatar, "Actual source group enables avatar export controls");
  avatar.save(dir / "avatar.dsculpt");
  auto native = avatar.copy_scene().encode();
  finish(avatar.export_asset(dir / "selected.vrm", AssetExportFormat::Avatar));
  auto vrm = parse(dir / "selected.vrm");
  check(vrm->meshes_count == 19 && vrm->skins_count == 1,
        "Selected avatar export includes nineteen parts and one rig while excluding unrelated visible mesh");
  check(!avatar.frame().state->dirty && avatar.copy_scene().encode() == native,
        "Avatar export retains exact native source archives, pose, paint and project dirty state");
  Controller hand(avatar); hand.send(0, 0, true); hand.send(); hand.sector(8); hand.sector(10); hand.sector(5); hand.sector(11);
  auto until = std::chrono::steady_clock::now() + std::chrono::seconds(60);
  while (!std::filesystem::exists(dir / "avatar.vrm") && std::chrono::steady_clock::now() < until) std::this_thread::sleep_for(std::chrono::milliseconds(10));
  check(std::filesystem::exists(dir / "avatar.vrm") && avatar.frame().state->error.empty(),
        "Actual joystick rig-menu command exports selected avatar next to the native project");
  check(parse(dir / "avatar.vrm")->meshes_count == 19, "Headset export has independently valid selected-group contents");
  auto original = bytes(dir / "selected.vrm");
  auto controls = humanoid_baseline_defaults("vroid-beta-female"); controls.hair_length = .3f;
  Scene groomed; append_humanoid(groomed, controls, "Generated groom", {}, 64); avatar.replace(std::move(groomed));
  auto unsupported = avatar.export_asset(dir / "selected.vrm", AssetExportFormat::Avatar);
  check(rejects([&] { finish(std::move(unsupported)); }) && bytes(dir / "selected.vrm") == original,
        "Unsupported generated-part VRM path reports failure and preserves existing valid output exactly");
  std::filesystem::remove_all(dir);
  return 0;
} catch (const std::exception &e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
