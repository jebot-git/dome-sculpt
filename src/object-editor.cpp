// SPDX-License-Identifier: GPL-3.0-or-later
#include "object-editor.hpp"
#include "appearance.h"
#include "ui-conventions.hpp"
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Value_Input.H>
namespace dome {
bool recover_project(Document &document) {
  auto state = document.frame().state;
  if (state->recovery_candidates.empty()) return false;
  int row = std::max(28, dx::size + 12), width = std::max(560, dx::size * 34);
  DomeDialog window(width, row * 6, "Recover project");
  Fl_Choice choice(20, row, width - 40, row);
  std::vector<Fl_Menu_Item> items(state->recovery_candidates.size() + 1);
  for (size_t i = 0; i < state->recovery_candidates.size(); ++i)
    items[i].label(state->recovery_candidates[i].c_str());
  choice.copy(items.data());
  choice.value(0);
  Fl_Button cancel(width / 2 - 150, row * 4, 140, row, "Cancel"),
      restore(width / 2 + 10, row * 4, 140, row, "Recover");
  bool accepted = false;
  cancel.callback([](Fl_Widget *w, void *) { w->window()->hide(); });
  restore.callback([](Fl_Widget *w, void *v) {
    *static_cast<bool *>(v) = true;
    w->window()->hide();
  }, &accepted);
  window.end();
  window.set_modal();
  dx::style(&window);
  window.show();
  while (window.shown()) Fl::wait();
  if (accepted && choice.value() >= 0) {
    document.restore_recovery(size_t(choice.value()));
    return true;
  }
  return false;
}
void edit_object(Document &document) {
  auto state = document.frame().state;
  auto item =
      std::find_if(state->objects.begin(), state->objects.end(),
                   [&](auto &o) { return o.id == state->active_object; });
  if (item == state->objects.end())
    return;
  auto object = *item;
  int row = std::max(28, dx::size + 12), width = std::max(560, dx::size * 34);
  int sx, sy, sw, sh;
  Fl::screen_work_area(sx, sy, sw, sh);
  width = std::min(width, sw - 40);
  int height = std::min(row * 24, sh - 60), label = width * 2 / 5;
  DomeDialog window(width, height, "Object and material");
  Fl_Scroll scroll(0, 0, width, height - row * 2);
  Fl_Group content(0, 0, width - 24, row * 22);
  content.resizable(nullptr);
  int y = row;
  auto field = [&](const char *name, double value, double lo, double hi,
                   double step) {
    auto input =
        new Fl_Value_Input(label, y, width - label - 40, row - 4, name);
    input->value(value);
    input->bounds(lo, hi);
    input->step(step);
    y += row;
    return input;
  };
  Fl_Input name(label, y, width - label - 40, row - 4, "Name");
  name.value(object.name.c_str());
  y += row;
  Fl_Check_Button visible(20, y, width / 2 - 20, row, "Visible"),
      locked(width / 2, y, width / 2 - 20, row, "Locked");
  visible.value(object.visible);
  locked.value(object.locked);
  y += row;
  auto t = object.transform;
  auto px = field("Position X (m)", t.position.x, -100, 100, .01),
       py = field("Position Y (m)", t.position.y, -100, 100, .01),
       pz = field("Position Z (m)", t.position.z, -100, 100, .01);
  constexpr float degrees = 180 / 3.14159265358979323846f;
  auto q = t.orientation;
  auto rx = field("Rotation X (degrees)",
                  degrees * std::atan2(2 * (q.w * q.x + q.y * q.z),
                                       1 - 2 * (q.x * q.x + q.y * q.y)),
                  -360, 360, 1);
  auto ry = field(
      "Rotation Y (degrees)",
      degrees * std::asin(std::clamp(2 * (q.w * q.y - q.z * q.x), -1.f, 1.f)),
      -360, 360, 1);
  auto rz = field("Rotation Z (degrees)",
                  degrees * std::atan2(2 * (q.w * q.z + q.x * q.y),
                                       1 - 2 * (q.y * q.y + q.z * q.z)),
                  -360, 360, 1);
  auto scale = field("Scale", t.scale, .001, 100, .01);
  auto material = object.material;
  Fl_Input material_name(label, y, width - label - 40, row - 4,
                         "Material name");
  material_name.value(material.name.c_str());
  y += row;
  std::array<Fl_Value_Input *, 4> color;
  const char *color_names[] = {"Red", "Green", "Blue", "Opacity"};
  for (int i = 0; i < 4; ++i)
    color[i] = field(color_names[i], material.color[i], 0, 1, .01);
  auto metallic = field("Metallic", material.metallic, 0, 1, .01),
       roughness = field("Roughness", material.roughness, 0, 1, .01);
  std::array<Fl_Value_Input *, 3> emission;
  const char *emission_names[] = {"Emission red", "Emission green",
                                  "Emission blue"};
  for (int i = 0; i < 3; ++i)
    emission[i] = field(emission_names[i], material.emissive[i], 0, 1, .01);
  Fl_Check_Button toon(20, y, width / 2 - 20, row, "Toon preview"),
      double_sided(width / 2, y, width / 2 - 20, row, "Double sided");
  toon.value(material.toon);
  double_sided.value(material.double_sided);
  y += row;
  content.end();
  scroll.end();
  Fl_Button cancel(width / 2 - 150, height - row * 2 + 8, 140, row, "Cancel"),
      apply(width / 2 + 10, height - row * 2 + 8, 140, row, "Apply");
  struct Result {
    Fl_Window *window;
    bool accepted = false;
  } result{&window};
  cancel.callback(
      [](Fl_Widget *, void *v) { static_cast<Result *>(v)->window->hide(); },
      &result);
  apply.callback(
      [](Fl_Widget *, void *v) {
        auto &r = *static_cast<Result *>(v);
        r.accepted = true;
        r.window->hide();
      },
      &result);
  window.end();
  window.set_modal();
  dx::style(&window);
  window.show();
  while (window.shown())
    Fl::wait();
  if (!result.accepted)
    return;
  t.position = {float(px->value()), float(py->value()), float(pz->value())};
  t.scale = scale->value();
  float x = rx->value() / degrees / 2, yangle = ry->value() / degrees / 2,
        z = rz->value() / degrees / 2;
  Quat rotation = Quat{std::cos(z), 0, 0, std::sin(z)} *
                  Quat{std::cos(yangle), 0, std::sin(yangle), 0} *
                  Quat{std::cos(x), std::sin(x), 0, 0};
  t.orientation = {rotation.x, rotation.y, rotation.z, rotation.w};
  material.name = material_name.value();
  material.metallic = metallic->value();
  material.roughness = roughness->value();
  for (int i = 0; i < 4; ++i)
    material.color[i] = color[i]->value();
  for (int i = 0; i < 3; ++i)
    material.emissive[i] = emission[i]->value();
  material.toon = toon.value();
  material.double_sided = double_sided.value();
  document.object_properties(object.id, name.value(), visible.value(),
                             locked.value(), t, material);
}
void edit_sculpt_layer(Document &document) {
  auto state = document.frame().state;
  auto object = state->active_object, layer = state->active_sculpt_layer;
  int row = std::max(28, dx::size + 12), width = std::max(480, dx::size * 28);
  DomeDialog window(width, row * 7, "Sculpt layer properties");
  Fl_Input name(width / 3, row, width * 2 / 3 - 24, row - 4, "Name");
  name.value(state->sculpt_layer_name.c_str());
  Fl_Value_Input strength(width / 3, row * 2, width * 2 / 3 - 24, row - 4,
                          "Strength");
  strength.bounds(0, 1);
  strength.step(.01);
  strength.value(state->sculpt_layer_opacity);
  Fl_Check_Button visible(24, row * 3, width / 2 - 24, row, "Visible"),
      locked(width / 2, row * 3, width / 2 - 24, row, "Locked");
  visible.value(state->sculpt_layer_visible);
  locked.value(state->sculpt_layer_locked);
  Fl_Button cancel(width / 2 - 150, row * 5, 140, row, "Cancel"),
      apply(width / 2 + 10, row * 5, 140, row, "Apply");
  struct Result {
    Fl_Window *window;
    bool accepted = false;
  } result{&window};
  cancel.callback(
      [](Fl_Widget *, void *p) { static_cast<Result *>(p)->window->hide(); },
      &result);
  apply.callback(
      [](Fl_Widget *, void *p) {
        auto &r = *static_cast<Result *>(p);
        r.accepted = true;
        r.window->hide();
      },
      &result);
  window.end();
  window.set_modal();
  dx::style(&window);
  window.show();
  while (window.shown())
    Fl::wait();
  if (result.accepted)
    document.sculpt_layer_properties(object, layer, name.value(),
                                     visible.value(), locked.value(),
                                     float(strength.value()));
}
} // namespace dome
