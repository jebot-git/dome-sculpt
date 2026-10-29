// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
static void require(bool v, const char *s) {
  if (!v)
    throw std::runtime_error(s);
  std::cout << "PASS " << s << '\n';
}
static Mesh plane() {
  Mesh m;
  m.positions = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
  m.normals.assign(4, {0, 0, 1});
  m.indices = {0, 1, 2, 0, 2, 3};
  m.uv = {{.1f, .1f}, {.6f, .1f}, {.6f, .6f}, {.1f, .6f}};
  return m;
}
int main() try {
  auto m = plane();
  UvWorkspace edit(m, 128);
  require(edit.view().valid && edit.view().topology->islands.size() == 1,
          "Adjacent UV faces form one editable chart");
  require(!uv_changed(m, *edit.view().mesh),
          "Opening an older UV mesh without pins/cuts is a semantic no-op");
  edit.edit(UvAction::Pick, .35f, .35f);
  edit.edit(UvAction::Seam);
  require(
      edit.view().topology->islands.size() == 2 &&
          edit.view().mesh->positions.size() == 6,
      "A marked shared edge splits its UV corner fans into independent charts");
  auto cut = *edit.view().mesh;
  edit.edit(UvAction::Undo);
  require(edit.view().topology->islands.size() == 1 &&
              !uv_changed(*edit.view().mesh, m),
          "Draft undo restores seam topology exactly");
  edit.edit(UvAction::Redo);
  require(edit.view().topology->islands.size() == 2,
          "Draft redo restores the cut and its chart identities");
  auto selected = edit.view().face;
  edit.edit(UvAction::Scale, .7f);
  require(edit.view().valid,
          "A selected cut chart scales without moving its neighbor");
  auto island = edit.view().topology->face_island[selected];
  for (size_t f = 0; f < cut.indices.size() / 3; ++f)
    if (edit.view().topology->face_island[f] != island)
      for (int k = 0; k < 3; ++k)
        require(edit.view().mesh->uv[cut.indices[f * 3 + k]] ==
                    cut.uv[cut.indices[f * 3 + k]],
                "Unselected chart vertices remain exact");
  edit.edit(UvAction::Pin);
  auto pinned = edit.view().mesh->uv;
  edit.edit(UvAction::Move, .1f, .1f);
  require(edit.view().mesh->uv == pinned,
          "Pinned UVs reject accidental island movement");
  edit.edit(UvAction::Pin);
  edit.edit(UvAction::Move, .05f, 0);
  edit.edit(UvAction::Pack);
  require(edit.view().valid,
          "Packing places cut charts inside one tile without overlap");
  edit.edit(UvAction::Move, 2, 0);
  require(!edit.view().valid,
          "UVs outside the tile are visibly invalid and cannot be committed");
  UvWorkspace vertex(m, 128);
  vertex.edit(UvAction::VertexMode);
  auto old = vertex.view().mesh->uv;
  vertex.edit(UvAction::Move, .1f, .1f);
  require(vertex.view().mesh->uv == old,
          "Vertex mode requires a picked vertex before moving");
  vertex.edit(UvAction::Pick, .1f, .1f);
  vertex.edit(UvAction::Move, .025f, .025f);
  require(vertex.view().valid && vertex.view().mesh->uv[1] == old[1] &&
              vertex.view().mesh->uv[0] != old[0],
          "Vertex edits change only the selected UV vertex");
  Mesh grid;
  for (int y = 0; y < 3; ++y)
    for (int x = 0; x < 3; ++x) {
      grid.positions.push_back({float(x), float(y), 0});
      grid.normals.push_back({0, 0, 1});
      grid.uv.push_back({.1f + .35f * x, .1f + .35f * y});
    }
  for (int y = 0; y < 2; ++y)
    for (int x = 0; x < 2; ++x) {
      uint32_t a = y * 3 + x;
      grid.indices.insert(grid.indices.end(),
                          {a, a + 1, a + 4, a, a + 4, a + 3});
    }
  grid.uv[4] = {.6f, .35f};
  UvWorkspace relax(grid, 128);
  relax.edit(UvAction::VertexMode);
  relax.edit(UvAction::Pick, .6f, .35f);
  relax.edit(UvAction::Pin);
  relax.edit(UvAction::Relax);
  require(relax.view().mesh->uv[4] == grid.uv[4],
          "Relaxation retains an explicitly pinned interior vertex");
  relax.edit(UvAction::Pin);
  relax.edit(UvAction::Relax);
  require(
      relax.view().valid &&
          std::abs(relax.view().mesh->uv[4][0] - .45f) < .01f &&
          relax.view().mesh->uv[0] == grid.uv[0],
      "Relaxation improves an interior UV while retaining chart boundaries");
  PaintedMesh paint(m, 128);
  PaintSettings settings;
  settings.color = {.02f, .04f, .9f, .7f};
  Brush b;
  b.from = b.to = {.5f, .5f, 0};
  b.radius = 3;
  b.normal = {0, 0, 1};
  paint.begin_stroke();
  paint.stamp(b, settings);
  paint.end_stroke();
  paint.add_layer("Detail");
  settings.channel = PaintChannel::Metallic;
  settings.metallic = .8f;
  paint.begin_stroke();
  paint.stamp(b, settings);
  paint.end_stroke();
  UvWorkspace moved(m, 128);
  moved.edit(UvAction::Move, .2f, .1f);
  auto next = paint.reparameterize(*moved.view().mesh);
  require(next->layers().size() == 2 && next->layers()[1].name == "Detail" &&
              next->active_layer() == paint.active_layer(),
          "UV transfer retains editable layer identities, order and selection");
  auto maps = next->composite(Material{});
  auto at = (size_t(57) * 128 + 70) * 4;
  require(maps->color.rgba[at + 2] > 220 && maps->color.rgba[at + 3] < 200 &&
              maps->metal_rough.rgba[at + 2] > 190,
          "Transferred color, opacity and metal paint follow the same surface "
          "point");
  auto round = PaintedMesh::decode(next->encode());
  require(round->mesh().uv == next->mesh().uv &&
              round->mesh().uv_pins == next->mesh().uv_pins,
          "Edited UVs, pins and paint survive CBOR-compatible persistence");
  auto original = paint.encode();
  auto metadata = paint.reparameterize(*UvWorkspace(m, 128).view().mesh);
  require(metadata->composite(Material{})->color.rgba ==
              paint.composite(Material{})->color.rgba,
          "Metadata-only UV edits preserve exact texture bytes");
  Scene scene;
  scene.active().painted = std::make_shared<PaintedMesh>(paint);
  scene.active().mesh_active = true;
  Document doc;
  doc.replace(scene);
  doc.begin_uv();
  doc.edit_uv(UvAction::Move, .2f, .1f);
  require(doc.frame().state->dirty && !doc.begin_desktop(),
          "The UV draft owns input and reports unsaved changes");
  doc.end_uv(false);
  require(!doc.frame().state->dirty &&
              doc.copy_scene().active().painted->encode() == original,
          "Cancelling the UV draft retains exact paint and the clean history "
          "position");
  doc.begin_uv();
  doc.edit_uv(UvAction::Move, .2f, .1f);
  doc.end_uv(true);
  require(doc.wait_asset(std::chrono::seconds(180)) &&
              doc.frame().state->error.empty(),
          "UV commit resamples paint through the background asset worker");
  require(
      doc.undo() && !doc.frame().state->dirty &&
          doc.copy_scene().active().painted->encode() == original,
      "One undo restores the complete pre-UV asset and saved state exactly");
  require(doc.redo() && doc.copy_scene().active().painted->mesh().uv != m.uv,
          "UV redo restores the edited mesh and transferred layers");
  doc.begin_uv();
  doc.edit_uv(UvAction::Move, 3, 0);
  bool rejected = false;
  try {
    doc.end_uv(true);
  } catch (const std::exception &) {
    rejected = true;
  }
  require(rejected && doc.frame().state->uv,
          "An invalid Apply keeps the editable draft open");
  doc.end_uv(false);
  // Real controller input reaches the same UV workspace through an XR palette.
  doc.replace(scene);
  SpatialFrame frame;
  frame.sequence = 1;
  frame.time = 1;
  frame.focused = frame.head_valid = true;
  frame.head.position = {0, 1.6f, 0};
  frame.hands[1].tracked.valid = true;
  frame.hands[1].tracked.grip.position = {0, 1.48f, -.5f};
  auto send = [&](float x = 0, float y = 0, bool press = false,
                  float pressure = 0) {
    ++frame.sequence;
    frame.time += .05;
    auto &h = frame.hands[1];
    h.x = x;
    h.y = y;
    h.joystick = press;
    h.tracked.trigger = pressure;
    doc.submit(frame);
    if (!doc.wait_input())
      throw std::runtime_error("UV controller input worker timed out");
  };
  auto sector = [&](int sector) {
    float a = sector * 3.14159265358979323846f / 6;
    send(std::cos(a), std::sin(a));
    send(std::cos(a), std::sin(a), true);
    send();
  };
  send();
  send(0, 0, true);
  send();
  sector(9);
  sector(7);
  require(doc.frame().state->uv &&
              doc.frame().state->controls.parameter == VrParameter::UvEdit,
          "Joystick Material / UV editor opens the in-headset editing panel");
  auto aim = [&](float x, float y, float pressure) {
    auto s = doc.frame().state;
    auto &h = frame.hands[1];
    h.tracked.aimValid = true;
    h.tracked.aim.position = ob::transform(
        s->panel, {(x / 640 - .5f) * .6f, (.5f - y / 640) * .6f, .25f});
    h.tracked.aim.orientation = s->panel.orientation;
    send(0, 0, false, pressure);
  };
  auto original_uv = doc.frame().state->uv->mesh->uv;
  auto edge = [&]() {
    auto &u = doc.frame().state->uv->mesh->uv;
    return std::hypot(u[1][0] - u[0][0], u[1][1] - u[0][1]);
  };
  float initial_edge = edge();
  aim(178, 85, .8f);
  send(); // Select Scale, then release/neutral.
  for (int i = 0; i < 10; i++)
    send(-1);
  float coarse = edge();
  require(std::abs(coarse / initial_edge - std::exp(-.5f * .69314718f)) < .002f,
          "Continuous UV stick scaling uses elapsed time and logarithmic size");
  for (int i = 0; i < 10; i++)
    send(0, -1);
  require(std::abs(edge() / coarse - std::exp(-.05f * .69314718f)) < .002f,
          "UV vertical fine wheel scales at one tenth horizontal speed");
  auto before_turn = doc.frame().state->uv->mesh->uv;
  float turn_edge = edge();
  aim(272, 85, .8f);
  send(); // Turn mode.
  for (int i = 0; i < 10; i++)
    send(1);
  auto after_turn = doc.frame().state->uv->mesh->uv;
  auto angle = [](const auto &u) {
    return std::atan2(u[1][1] - u[0][1], u[1][0] - u[0][0]);
  };
  require(std::abs(edge() - turn_edge) < 1e-5f &&
              std::abs(angle(after_turn) - angle(before_turn) -
                       .5f * .523598776f) < .002f,
          "Continuous UV turning changes angle while preserving island scale");
  frame.focused = false;
  send();
  frame.focused = true;
  send();
  require(!doc.frame().state->uv && !doc.frame().state->dirty,
          "Continuous UV transforms cancel exactly on focus loss");
  doc.cancel_spatial_menu();
  send();
  send(0, 0, true);
  send();
  sector(9);
  sector(7);
  require(doc.frame().state->uv->mesh->uv == original_uv,
          "Cancelled scale/turn draft retains exact saved UV coordinates");
  aim(257, 257, .8f);
  aim(278, 278, .8f);
  send();
  require(std::abs(doc.frame().state->uv->mesh->uv[0][0] - .15f) < 1e-5f,
          "A held controller ray drags the selected UV chart in both axes");
  aim(470, 609, .8f);
  send();
  require(doc.wait_asset(std::chrono::seconds(180)) && !doc.frame().state->uv &&
              doc.undo() && !doc.frame().state->dirty,
          "Controller Apply transfers paint and records one reversible UV "
          "transaction");
  doc.cancel_spatial_menu();
  send();
  send(0, 0, true);
  send();
  sector(9);
  sector(7);
  send(1);
  frame.focused = false;
  send();
  require(!doc.frame().state->uv && !doc.frame().state->dirty,
          "XR focus loss discards the UV draft without changing paint");
  frame.focused = true;
  send();
  doc.replace(scene);
  send();
  send(0, 0, true);
  send();
  sector(9);
  sector(7);
  auto panel = doc.frame().state->panel;
  std::vector<SpatialFrame> burst;
  auto queued = [&](float x, float y, float pressure, float px, float py,
                    bool focused = true) {
    ++frame.sequence;
    frame.time += .05;
    frame.focused = focused;
    auto &h = frame.hands[1];
    h.x = x;
    h.y = y;
    h.joystick = false;
    h.tracked.trigger = pressure;
    h.tracked.aimValid = true;
    h.tracked.aim.position = ob::transform(
        panel, {(px / 640 - .5f) * .6f, (.5f - py / 640) * .6f, .25f});
    h.tracked.aim.orientation = panel.orientation;
    burst.push_back(frame);
  };
  queued(0, 0, .8f, 178, 85);
  queued(0, 0, 0, 178, 85);
  for (int i = 0; i < 20; ++i)
    queued(-1, 0, 0, 178, 85);
  queued(0, 0, .8f, 470, 609);
  queued(0, 0, 0, 470, 609, false);
  doc.submit(burst);
  require(doc.wait_input(std::chrono::seconds(180)) &&
              doc.wait_asset(std::chrono::seconds(180)),
          "A burst of controller events and its focus boundary drain in order");
  require(doc.copy_scene().active().painted->mesh().uv != m.uv,
          "Focus loss preserves a confirmed UV edit queued before the focus "
          "boundary");
  require(doc.undo() && !doc.frame().state->dirty,
          "A queued accepted UV edit remains one exact undo transaction");
  frame.focused = true;
  send();
  send(0, 0, true);
  send();
  sector(9);
  sector(7);
  aim(178, 85, .8f);
  send();
  require(doc.frame().state->uv->tool == 1,
          "A controller ray selects continuous UV scaling");
  for (int i = 0; i < 20; ++i)
    send(-1);
  auto span = [](const Mesh &mesh) {
    float lo = 1, hi = 0;
    for (auto uv : mesh.uv) {
      lo = std::min(lo, uv[0]);
      hi = std::max(hi, uv[0]);
    }
    return hi - lo;
  };
  float scaled = span(*doc.frame().state->uv->mesh);
  require(std::abs(scaled - .25f) < 1e-4f,
          "Full stick deflection halves UV scale in one second");
  for (int i = 0; i < 20; ++i)
    send(0, -1);
  float fine = span(*doc.frame().state->uv->mesh);
  require(fine < scaled && scaled - fine < (span(m) - scaled) / 3,
          "The vertical UV scale wheel adjusts ten times more finely");
  aim(272, 85, .8f);
  send();
  require(doc.frame().state->uv->tool == 2,
          "The same controller panel selects continuous UV turning");
  auto vertex_angle = [](const Mesh &mesh) {
    std::array<float, 2> center{};
    for (auto uv : mesh.uv)
      for (int k = 0; k < 2; ++k)
        center[k] += uv[k] / mesh.uv.size();
    return std::atan2(mesh.uv[0][1] - center[1], mesh.uv[0][0] - center[0]);
  };
  float vertex_before_turn = vertex_angle(*doc.frame().state->uv->mesh);
  for (int i = 0; i < 20; ++i)
    send(1);
  float turned = vertex_angle(*doc.frame().state->uv->mesh);
  require(std::abs((turned - vertex_before_turn) - .523598776f) < 1e-4f,
          "UV turning follows elapsed controller time at thirty degrees per "
          "second");
  for (int i = 0; i < 20; ++i)
    send(0, 1);
  require(std::abs((vertex_angle(*doc.frame().state->uv->mesh) - turned) -
                   .052359878f) < 1e-4f,
          "The UV rotation wheel retains fine angular control");
  doc.cancel_spatial_menu();
  require(!doc.frame().state->dirty &&
              doc.copy_scene().active().painted->mesh().uv == m.uv,
          "Cancelling continuous transforms restores exact saved UVs");
  auto malformed = next->encode();
  malformed["mesh"]["uv_pins"] = nlohmann::json::binary({1});
  bool bad = false;
  try {
    PaintedMesh::decode(malformed);
  } catch (const std::exception &) {
    bad = true;
  }
  require(bad,
          "Malformed persisted pin buffers are rejected before publication");
  Document sphere;
  sphere.seed();
  sphere.prepare_mesh(64);
  require(sphere.wait_asset(std::chrono::seconds(180)),
          "Sculpt surface extraction and automatic atlasing finish");
  sphere.begin_uv();
  require(sphere.frame().state->uv->valid,
          "The actual sculpt atlas opens as valid editable UV charts");
  sphere.end_uv(false);
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
