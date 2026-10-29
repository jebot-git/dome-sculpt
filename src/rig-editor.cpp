// SPDX-License-Identifier: GPL-3.0-or-later
#include "rig-editor.hpp"
#include "dome/rig-edit.hpp"
#include "appearance.h"
#include "ui-conventions.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Value_Input.H>
#include <FL/fl_ask.H>
namespace dome {
void edit_rig_dialog(Document &doc) {
  auto draft = doc.draft_scene();
  const auto &o = draft.scene.active();
  if (!o.visible || o.locked || !o.mesh_active || !o.painted || !o.deformation)
    throw std::invalid_argument("Select a visible, unlocked rigged mesh");
  auto data = o.deformation;
  int row = std::max(32, dx::size + 14), sx, sy, sw, sh;
  Fl::screen_work_area(sx, sy, sw, sh);
  int width = std::min(std::max(640, dx::size * 40), sw - 40);
  int height = std::min(row * 16, sh - 40), label = width / 2;
  DomeDialog window(width, height, "Rig and shapekeys");
  Fl_Scroll scroll(0, 0, width, height - row * 2);
  scroll.type(Fl_Scroll::VERTICAL);
  Fl_Group content(0, 0, width - 24, row * 14);
  content.resizable(nullptr);
  Fl_Box hint(16, 4, width - 48, row * 2,
      "Local rotation and translation are added to the current pose.\n"
      "Apply commits one undo step; Cancel retains the current pose.");
  hint.align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
  int y = row * 2;
  Fl_Choice joint(label, y, width - label - 40, row - 4, "Joint"); y += row;
  std::vector<Fl_Menu_Item> joints, shapes;
  if (data->skeleton && data->skin) {
    for (const auto &bone : data->skeleton->bones) {
      Fl_Menu_Item item{}; item.label(bone.name.c_str()); joints.push_back(item);
    }
    joints.emplace_back(); joint.copy(joints.data()); joint.value(0);
  } else joint.deactivate();
  Fl_Value_Input rx(label, y, width - label - 40, row - 4, "Rotate X (degrees)"); y += row;
  Fl_Value_Input ry(label, y, width - label - 40, row - 4, "Rotate Y (degrees)"); y += row;
  Fl_Value_Input rz(label, y, width - label - 40, row - 4, "Rotate Z (degrees)"); y += row;
  Fl_Value_Input tx(label, y, width - label - 40, row - 4, "Translate X (m)"); y += row;
  Fl_Value_Input ty(label, y, width - label - 40, row - 4, "Translate Y (m)"); y += row;
  Fl_Value_Input tz(label, y, width - label - 40, row - 4, "Translate Z (m)"); y += row;
  std::array<Fl_Value_Input *, 6> fields{&rx, &ry, &rz, &tx, &ty, &tz};
  for (size_t i = 0; i < fields.size(); ++i) {
    fields[i]->bounds(i < 3 ? -180 : -10, i < 3 ? 180 : 10);
    fields[i]->step(i < 3 ? .1 : .001);
    if (joints.empty()) fields[i]->deactivate();
  }
  Fl_Check_Button linked(20, y, width - 50, row, "Pose all matching rig parts in this group");
  linked.value(1); y += row;
  Fl_Check_Button reset(20, y, width - 50, row, "Reset pose before applying the adjustments"); y += row;
  if (joints.empty()) { linked.deactivate(); reset.deactivate(); }
  Fl_Choice morph(label, y, width - label - 40, row - 4, "Shapekey (selected part)"); y += row;
  Fl_Value_Input weight(label, y, width - label - 40, row - 4, "Shapekey weight"); y += row;
  weight.bounds(-1, 1); weight.step(.01);
  Fl_Check_Button apply_shape(20, y, width - 50, row, "Apply selected shapekey weight"); y += row;
  if (data->morphs && !data->morphs->targets.empty()) {
    for (const auto &target : data->morphs->targets) {
      Fl_Menu_Item item{}; item.label(target.name.c_str()); shapes.push_back(item);
    }
    shapes.emplace_back(); morph.copy(shapes.data()); morph.value(0);
    weight.value(data->morph_weights.empty() ? 0 : data->morph_weights[0]);
  } else { morph.deactivate(); weight.deactivate(); apply_shape.deactivate(); }
  content.size(width - 24, y + row); content.end(); scroll.end();
  Fl_Button cancel(width / 2 - 150, height - row * 2 + 8, 140, row, "Cancel"),
      apply(width / 2 + 10, height - row * 2 + 8, 140, row, "Apply");
  struct State {
    Fl_Window *window; Fl_Choice *joint, *morph; Fl_Value_Input *weight;
    Fl_Check_Button *linked, *reset, *shape;
    std::array<Fl_Value_Input *, 6> fields;
    std::shared_ptr<const DeformationState> data;
    std::optional<RigEdit> edit;
  } state{&window, &joint, &morph, &weight, &linked, &reset, &apply_shape, fields, data, {}};
  joint.callback([](Fl_Widget *, void *p) {
    for (auto field : static_cast<State *>(p)->fields) field->value(0);
  }, &state);
  morph.callback([](Fl_Widget *, void *p) {
    auto &s = *static_cast<State *>(p);
    s.weight->value(s.data->morph_weights.empty() ? 0 : s.data->morph_weights.at(s.morph->value()));
    s.shape->value(0);
  }, &state);
  weight.callback([](Fl_Widget *, void *p) { static_cast<State *>(p)->shape->value(1); }, &state);
  cancel.callback([](Fl_Widget *w, void *) { w->window()->hide(); });
  apply.callback([](Fl_Widget *, void *p) {
    auto &s = *static_cast<State *>(p);
    try {
      RigEdit edit; edit.linked_pose = s.linked->value(); edit.reset_pose = s.reset->value();
      JointAdjustment joint; joint.joint = uint32_t(std::max(0, s.joint->value()));
      auto value = [&](size_t i) {
        double v = s.fields[i]->value(), limit = i < 3 ? 180 : 10;
        if (!std::isfinite(v) || std::abs(v) > limit) throw std::invalid_argument("Adjustment is outside the supported range");
        return float(v);
      };
      joint.degrees = {value(0), value(1), value(2)};
      joint.translation = {value(3), value(4), value(5)};
      if (length(joint.degrees) || length(joint.translation)) edit.joint = joint;
      if (s.shape->value()) {
        float v = float(s.weight->value());
        if (!std::isfinite(v) || std::abs(v) > 1) throw std::invalid_argument("Shapekey weight must be within [-1, 1]");
        edit.morph = MorphWeightEdit{uint32_t(s.morph->value()), v};
      }
      if (!edit.joint && !edit.morph && !edit.reset_pose) { s.window->hide(); return; }
      s.edit = std::move(edit); s.window->hide();
    } catch (const std::exception &e) { fl_alert("%s", e.what()); }
  }, &state);
  window.end(); window.set_modal(); dx::style(&window); window.show(); cancel.take_focus();
  while (window.shown()) Fl::wait();
  if (state.edit) doc.edit_scene("Edit rig and shapekeys",
    [id = o.id, edit = *state.edit](Scene &scene, const auto &cancelled) { edit_rig(scene, id, edit, cancelled); }, draft.source);
}
} // namespace dome
