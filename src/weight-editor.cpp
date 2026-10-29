// SPDX-License-Identifier: GPL-3.0-or-later
#include "weight-editor.hpp"
#include "appearance.h"
#include "ui-conventions.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Check_Browser.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Value_Input.H>
#include <FL/fl_ask.H>
namespace dome {
void edit_weights_dialog(Document &doc) {
  auto draft = doc.draft_scene();
  const auto &part = draft.scene.active();
  if (!part.visible || part.locked || !part.mesh_active || !part.deformation ||
      !part.deformation->skeleton || !part.deformation->skin)
    throw std::invalid_argument("Select a visible, unlocked skinned mesh");
  auto state = doc.frame().state;
  auto settings = state->weights; settings.enabled = true;
  const auto &bones = part.deformation->skeleton->bones;
  int row = std::max(32, dx::size + 14), sx, sy, sw, sh;
  Fl::screen_work_area(sx, sy, sw, sh);
  int width = std::min(std::max(680, dx::size * 42), sw - 40),
      height = std::min(row * 20, sh - 40), label = width / 2;
  DomeDialog window(width, height, "Skin weights");
  Fl_Scroll scroll(0, 0, width, height - row * 2);
  scroll.type(Fl_Scroll::VERTICAL);
  Fl_Group content(0, 0, width - 24, row * 20); content.resizable(nullptr);
  Fl_Box hint(16, 4, width - 48, row * 2,
      "Shift+drag on the mesh; trigger near the surface in XR.\n"
      "Blue is zero, red is one. Each released gesture is one undo step.");
  hint.align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
  int y = row * 2;
  Fl_Choice joint(label, y, width - label - 40, row - 4, "Joint"); y += row;
  std::vector<Fl_Menu_Item> items;
  for (const auto &bone : bones) { Fl_Menu_Item item{}; item.label(bone.name.c_str()); items.push_back(item); }
  items.emplace_back(); joint.copy(items.data()); joint.value(int(state->rig_joint));
  Fl_Choice mode(label, y, width - label - 40, row - 4, "Brush mode"); y += row;
  mode.add("Add"); mode.add("Subtract"); mode.add("Replace"); mode.add("Smooth"); mode.value(int(settings.mode));
  Fl_Value_Input strength(label, y, width - label - 40, row - 4, "Stroke strength"); y += row;
  strength.bounds(0, 1); strength.step(.01); strength.value(settings.strength);
  Fl_Value_Input value(label, y, width - label - 40, row - 4, "Target weight"); y += row;
  value.bounds(0, 1); value.step(.01); value.value(settings.value);
  Fl_Value_Input cap(label, y, width - label - 40, row - 4, "Maximum influences"); y += row;
  cap.bounds(1, 32); cap.step(1); cap.value(settings.limits.max_influences);
  Fl_Value_Input prune(label, y, width - label - 40, row - 4, "Prune threshold"); y += row;
  prune.bounds(0, .99); prune.step(.0001); prune.value(settings.limits.prune);
  Fl_Value_Input plane(label, y, width - label - 40, row - 4, "Symmetry X plane (local m)"); y += row;
  plane.bounds(-10000, 10000); plane.step(.001); plane.value(settings.symmetry_plane);
  Fl_Check_Button enabled(20, y, width - 50, row, "Paint and display skin weights"); y += row;
  enabled.value(settings.enabled);
  Fl_Check_Button linked(20, y, width - 50, row, "Include visible, unlocked matching rig parts"); y += row;
  linked.value(settings.linked);
  Fl_Check_Button symmetry(20, y, width - 50, row, "Mirror with explicit humanoid joint mapping"); y += row;
  symmetry.value(settings.symmetry); if (!part.deformation->humanoid) symmetry.deactivate();
  Fl_Box lock_label(20, y, width - 50, row, "Locked influences (retained exactly)"); y += row;
  lock_label.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
  Fl_Check_Browser locks(20, y, width - 64, row * 5); y += row * 5;
  for (size_t i = 0; i < bones.size(); ++i)
    locks.add(bones[i].name.c_str(), !settings.locks.empty() && settings.locks.at(i));
  content.size(width - 24, y + row); content.end(); scroll.end();
  Fl_Button cancel(width / 2 - 150, height - row * 2 + 8, 140, row, "Cancel"),
      apply(width / 2 + 10, height - row * 2 + 8, 140, row, "Apply");
  struct Context {
    Fl_Window *window; Fl_Choice *joint, *mode;
    Fl_Value_Input *strength, *value, *cap, *prune, *plane;
    Fl_Check_Button *enabled, *linked, *symmetry; Fl_Check_Browser *locks;
    WeightToolSettings *settings; size_t bones; bool accepted = false;
  } context{&window, &joint, &mode, &strength, &value, &cap, &prune, &plane,
            &enabled, &linked, &symmetry, &locks, &settings, bones.size()};
  cancel.callback([](Fl_Widget *w, void *) { w->window()->hide(); });
  apply.callback([](Fl_Widget *, void *p) {
    auto &c = *static_cast<Context *>(p);
    try {
      auto next = *c.settings;
      if (!std::isfinite(c.cap->value()) || std::floor(c.cap->value()) != c.cap->value() ||
          c.cap->value() < 1 || c.cap->value() > 32) throw std::invalid_argument("Maximum influences must be an integer from 1 to 32");
      next.mode = WeightBrushMode(c.mode->value()); next.strength = float(c.strength->value());
      next.value = float(c.value->value()); next.limits.max_influences = uint32_t(c.cap->value());
      next.limits.prune = float(c.prune->value()); next.symmetry_plane = float(c.plane->value());
      next.enabled = c.enabled->value(); next.linked = c.linked->value(); next.symmetry = c.symmetry->value();
      next.locks.resize(c.bones);
      for (size_t i = 0; i < c.bones; ++i) next.locks[i] = c.locks->checked(int(i + 1));
      next.validate(c.bones); *c.settings = std::move(next); c.accepted = true; c.window->hide();
    } catch (const std::exception &e) { fl_alert("%s", e.what()); }
  }, &context);
  window.end(); window.set_modal(); dx::style(&window); window.show(); cancel.take_focus();
  while (window.shown()) Fl::wait();
  if (context.accepted) doc.weight_settings(std::move(settings), uint32_t(joint.value()), draft.source);
}
} // namespace dome
