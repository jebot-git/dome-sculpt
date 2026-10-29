// SPDX-License-Identifier: GPL-3.0-or-later
#include "creator-editor.hpp"
#include "dome/creation.hpp"
#include "appearance.h"
#include "ui-conventions.hpp"
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Value_Input.H>
#include <FL/fl_ask.H>
namespace dome {
void create_humanoid_dialog(Document &document) {
  auto draft = document.draft_scene();
  auto controls = humanoid_baseline_defaults("vroid-beta-female");
  std::string initial_name = "Humanoid";
  auto selected_group = draft.scene.active().group;
  for (auto &g : draft.scene.group_path(selected_group))
    if (g.humanoid) { controls = *g.humanoid; initial_name = g.name + " copy"; }
  int row = std::max(28, dx::size + 12), width = std::max(600, dx::size * 38);
  int sx, sy, sw, sh; Fl::screen_work_area(sx, sy, sw, sh);
  width = std::min(width, sw - 40);
  int height = std::min(row * 24, sh - 60), label = width / 2;
  DomeDialog window(width, height, "Create humanoid");
  Fl_Scroll scroll(0, 0, width, height - row * 2);
  Fl_Group content(0, 0, width - 24, row * 34);
  content.resizable(nullptr);
  int y = row;
  Fl_Input name(label, y, width - label - 40, row - 4, "Name");
  name.value(initial_name.c_str()); y += row;
  Fl_Choice baseline(label, y, width - label - 40, row - 4, "Model base");
  baseline.add("VRoid beta female (CC0)"); baseline.add("VRoid beta male (CC0)"); baseline.add("MakeHuman (CC0)");
  baseline.value(controls.baseline == "vroid-beta-male" ? 1 : controls.baseline == "makehuman-hm08" ? 2 : 0);
  y += row;
  Fl_Choice profile(label, y, width - label - 40, row - 4, "Style preset");
  profile.add("Neutral"); profile.add("Toon"); profile.add("Anime"); profile.value(0);
  y += row;
  struct Binding { Fl_Value_Input *input; float *value; };
  std::vector<Binding> bindings;
  auto field = [&](const char *title, float &value, float lo, float hi, float step = .01f) {
    auto *input = new Fl_Value_Input(label, y, width - label - 40, row - 4, title);
    input->bounds(lo, hi); input->step(step); input->value(value);
    bindings.push_back({input, &value}); y += row;
  };
  field("Height (m)", controls.height, 1.2f, 2.2f);
  field("Anime proportions", controls.anime, 0, 1);
  field("Build", controls.build, -1, 1);
  field("Shoulder width", controls.shoulder_width, -1, 1);
  field("Waist width", controls.waist_width, -1, 1);
  field("Leg length", controls.leg_length, -1, 1);
  field("Arm length", controls.arm_length, -1, 1);
  field("Head width", controls.head_width, -1, 1);
  field("Head height", controls.head_height, -1, 1);
  field("Eye size", controls.eye_size, 0, 1);
  field("Eye spacing", controls.eye_spacing, -1, 1);
  field("Nose size", controls.nose_size, -1, 1);
  field("Mouth width", controls.mouth_width, -1, 1);
  field("Jaw width", controls.jaw_width, -1, 1);
  field("Hair length", controls.hair_length, 0, 1);
  field("Hair volume", controls.hair_volume, 0, 1);
  field("Fringe", controls.fringe, 0, 1);
  const char *colors[] = {"Skin red", "Skin green", "Skin blue", "Iris red", "Iris green", "Iris blue", "Hair red", "Hair green", "Hair blue"};
  for (int i = 0; i < 9; ++i) {
    auto &color = i < 3 ? controls.skin_color : i < 6 ? controls.iris_color : controls.hair_color;
    field(colors[i], color[i % 3], 0, 1);
  }
  Fl_Check_Button keep(20, y, width - 50, row, "Keep existing objects");
  keep.value(1); y += row;
  content.size(width - 24, y + row);
  content.end(); scroll.end();
  Fl_Button cancel(width / 2 - 150, height - row * 2 + 8, 140, row, "Cancel"),
      create(width / 2 + 10, height - row * 2 + 8, 140, row, "Create");
  struct Context {
    Fl_Window *window;
    Fl_Input *name;
    HumanoidControls *controls;
    std::vector<Binding> *bindings;
    Fl_Choice *baseline;
    bool accepted = false;
  } context{&window, &name, &controls, &bindings, &baseline};
  profile.callback([](Fl_Widget *w, void *data) {
    auto &c = *static_cast<Context *>(data);
    auto preset = static_cast<Fl_Choice *>(w)->value();
    const char *ids[] = {"vroid-beta-female", "vroid-beta-male", "makehuman-hm08"};
    *c.controls = humanoid_preset(HumanoidPreset(preset), ids[c.baseline->value()]);
    for (auto &binding : *c.bindings) binding.input->value(*binding.value);
  }, &context);
  baseline.callback([](Fl_Widget *w, void *data) {
    auto &c = *static_cast<Context *>(data);
    const char *ids[] = {"vroid-beta-female", "vroid-beta-male", "makehuman-hm08"};
    *c.controls = humanoid_baseline_defaults(ids[static_cast<Fl_Choice *>(w)->value()]);
    for (auto &binding : *c.bindings) binding.input->value(*binding.value);
  }, &context);
  cancel.callback([](Fl_Widget *w, void *) { w->window()->hide(); });
  create.callback([](Fl_Widget *, void *data) {
    auto &c = *static_cast<Context *>(data);
    try {
      for (auto &binding : *c.bindings) *binding.value = float(binding.input->value());
      c.controls->validate();
      std::string name = c.name->value();
      if (name.empty() || name.size() > 128) throw std::invalid_argument("Name must contain 1 to 128 bytes");
      c.accepted = true; c.window->hide();
    } catch (const std::exception &e) { fl_alert("%s", e.what()); }
  }, &context);
  window.end(); window.set_modal(); dx::style(&window); window.show();
  while (window.shown()) Fl::wait();
  if (!context.accepted) return;
  std::string chosen_name = name.value();
  bool retain = keep.value();
  document.edit_scene("Create humanoid", [controls, chosen_name, retain](Scene &scene, const auto &cancelled) {
    if (!retain) scene = Scene{};
    auto empty = scene.active().id;
    CanvasTransform transform{}; transform.position.y = -controls.height * .5f;
    append_humanoid(scene, controls, chosen_name, transform, 256, cancelled);
    if (!retain) scene.erase(empty);
  }, draft.source);
}
} // namespace dome
