// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <iostream>
#include <unistd.h>
using namespace dome;
struct Driver {
  Document &doc;
  SpatialFrame frame;
  Driver(Document &d) : doc(d) {
    frame.sequence = 1;
    frame.time = 1;
    frame.focused = frame.head_valid = true;
    frame.head.position = {0, 1.6f, 0};
    frame.hands[1].tracked.valid = true;
    frame.hands[1].tracked.grip.position = {0, 1.48f, -.5f};
    send();
  }
  void send(float x = 0, float y = 0, bool click = false, float pressure = 0) {
    ++frame.sequence;
    frame.time += .05;
    auto &h = frame.hands[1];
    h.x = x;
    h.y = y;
    h.joystick = click;
    h.tracked.trigger = pressure;
    doc.submit(frame);
    if (!doc.wait_input())
      throw std::runtime_error("Controller worker timed out");
  }
  void open() {
    send();
    send(0, 0, true);
    send();
  }
  void sector(int i) {
    float a = i * 3.14159265358979323846f / 6;
    send(std::cos(a), std::sin(a));
    send(std::cos(a), std::sin(a), true);
    send();
  }
  void panel_button(bool accept) {
    auto s = doc.frame().state;
    auto &h = frame.hands[1];
    h.tracked.aimValid = true;
    float u = (accept ? 470.f : 170.f) / 640, v = 609.f / 640;
    h.tracked.aim.position =
        ob::transform(s->panel, {(u - .5f) * .6f, (.5f - v) * .6f, .25f});
    h.tracked.aim.orientation = s->panel.orientation;
    send(0, 0, false, .8f);
    send();
  }
  void confirm() {
    send(0, 0, true);
    send();
  }
  void ray(float t, int row = 0, bool stylus = false) {
    auto s = doc.frame().state;
    int n = s->controls.components;
    float y = n == 1 ? 300 : n == 4 ? 210 + 70 * row : 240 + 80 * row;
    float u = (100 + 420 * t) / 640, v = y / 640;
    auto &h = frame.hands[stylus ? 0 : 1];
    if (stylus) {
      h.tracked.valid = true;
      h.kind = DeviceKind::SpatialStylus;
      send();
    }
    h.tracked.aimValid = true;
    h.tracked.aim.position =
        ob::transform(s->panel, {(u - .5f) * .6f, (.5f - v) * .6f, .25f});
    h.tracked.aim.orientation = s->panel.orientation;
    if (stylus) {
      h.tracked.trigger = .8f;
      send();
      h.tracked.trigger = 0;
      send();
    } else {
      send(0, 0, false, .8f);
      send();
    }
  }
};
int main() try {
  auto require = [](bool v, const char *s) {
    if (!v)
      throw std::runtime_error(s);
    std::cout << "PASS " << s << '\n';
  };
  Document doc;
  doc.seed();
  auto geometry = doc.copy_volume().voxels();
  Driver hand(doc);
  hand.open();
  hand.sector(4);
  require(
      doc.frame().state->menu && !doc.frame().state->can_undo,
      "Unavailable radial commands stay visibly disabled and cannot activate");
  doc.cancel_spatial_menu();
  hand.open();
  hand.sector(9);
  hand.sector(4);
  require(doc.frame().state->controls.parameter == VrParameter::Metallic,
          "A joystick press opens a continuous material slider");
  for (int i = 0; i < 10; ++i)
    hand.send(1);
  require(std::abs(doc.copy_scene().active().material.metallic - .25f) <
                  .002f &&
              doc.frame().state->dirty,
          "Held thumbstick motion previews continuous parameter changes");
  hand.ray(.75f);
  require(std::abs(doc.copy_scene().active().material.metallic - .75f) < .002f,
          "A controller aim ray and trigger drag the same slider to its "
          "absolute position");
  hand.ray(.5f, 0, true);
  require(std::abs(doc.copy_scene().active().material.metallic - .5f) < .002f,
          "A secondary stylus can drag a slider opened by the controller");
  hand.confirm();
  require(doc.frame().state->menu_page == 2 &&
              doc.frame().state->controls.parameter == VrParameter::None,
          "Press confirmation returns to the parent menu");
  require(doc.undo() && doc.copy_scene().active().material.metallic == 0 &&
              !doc.frame().state->dirty,
          "A complete slider gesture is one exact undo transaction");
  require(
      doc.copy_volume().voxels() == geometry,
      "Slider capture prevents trigger input from sculpting behind the panel");
  hand.open();
  hand.sector(10);
  float radius = doc.frame().state->radius;
  for (int i = 0; i < 20; ++i)
    hand.send(1);
  float full = doc.frame().state->radius;
  require(std::abs(full - radius * 2) < .004f,
          "The logarithmic brush-size slider doubles naturally at full stick "
          "deflection");
  for (int i = 0; i < 20; ++i)
    hand.send(0, 1);
  float fine = doc.frame().state->radius;
  require(fine > full && fine - full < (full - radius) / 3,
          "Vertical wheel motion provides a slower fine adjustment");
  hand.frame.focused = false;
  hand.send();
  require(doc.frame().state->radius == radius &&
              doc.frame().state->controls.parameter == VrParameter::None,
          "Focus loss cancels a transient wheel adjustment");
  hand.frame.focused = true;
  hand.send();
  hand.open();
  hand.sector(9);
  hand.sector(0);
  require(doc.frame().state->controls.components == 4,
          "VR color controls expose separate RGBA sliders");
  auto original = doc.copy_scene().active().material.color;
  hand.send(0, -1);
  hand.send();
  for (int i = 0; i < 10; ++i)
    hand.send(1);
  auto color = doc.copy_scene().active().material.color;
  require(color[0] == original[0] && color[1] > original[1] + .2f &&
              color[2] == original[2],
          "Thumbstick row selection adjusts only the chosen color component");
  hand.panel_button(false);
  require(doc.copy_scene().active().material.color == original &&
              !doc.frame().state->dirty,
          "A ray-clickable Cancel button restores the original color without "
          "recording an edit");
  doc.prepare_mesh(64);
  require(doc.wait_asset(std::chrono::seconds(180)) &&
              doc.frame().state->mesh_active,
          "Layer controls use a prepared stable painting mesh");
  auto path = std::filesystem::temp_directory_path() /
              ("dome-controls-" + std::to_string(getpid()) + ".dsculpt");
  doc.save(path);
  hand.send();
  hand.open();
  hand.sector(9);
  hand.sector(3);
  hand.sector(11);
  hand.sector(6);
  require(doc.frame().state->controls.parameter == VrParameter::LayerOpacity,
          "The controller controls page exposes a layer-opacity slider");
  for (int i = 0; i < 10; ++i)
    hand.send(-1);
  hand.panel_button(true);
  require(std::abs(doc.copy_scene().active().painted->layers()[0].opacity -
                   .75f) < .002f,
          "Layer opacity follows continuous input and commits with a "
          "ray-clickable Done button");
  require(doc.undo() &&
              doc.copy_scene().active().painted->layers()[0].opacity == 1 &&
              !doc.frame().state->dirty,
          "Layer slider undo restores the saved layer and its history");
  std::filesystem::remove(path);
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
