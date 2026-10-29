// SPDX-License-Identifier: GPL-3.0-or-later
#include "mesh-editor.hpp"
#include "appearance.h"
#include "ui-conventions.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Slider.H>
#include <cstdio>
namespace dome {
void edit_voxel_remesh(Document &doc) {
  doc.begin_voxel_remesh();
  try {
    int row = std::max(32, dx::size + 14), width = std::max(560, dx::size * 34);
    int sx, sy, sw, sh;
    Fl::screen_work_area(sx, sy, sw, sh);
    width = std::min(width, sw - 40);
    DomeDialog window(width, row * 8, "Voxel remesh");
    Fl_Box description(12, 6, width - 24, row * 2,
                       "Create a new sculpt from the displayed shape.\n"
                       "Source paint, rig and layers stay in a hidden object.");
    description.align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    Fl_Box value(12, row * 2, width - 24, row, "");
    Fl_Slider slider(24, row * 3, width - 48, row);
    slider.type(FL_HOR_NICE_SLIDER);
    slider.bounds(0, 1);
    slider.step(.001);
    auto controls = doc.frame().state->controls;
    slider.value(std::log(controls.value[0] / controls.minimum) /
                 std::log(controls.maximum / controls.minimum));
    struct State {
      Document *doc;
      float minimum, maximum;
    } state{&doc, controls.minimum, controls.maximum};
    slider.callback([](Fl_Widget *widget, void *data) {
      auto &state = *static_cast<State *>(data);
      auto slider = static_cast<Fl_Slider *>(widget);
      try {
        float mm = std::exp(std::log(state.minimum) + slider->value() *
                             std::log(state.maximum / state.minimum));
        state.doc->edit_voxel_size(std::clamp(mm, state.minimum, state.maximum));
      } catch (const std::exception &e) {
        state.doc->report_error(e.what());
      }
    }, &state);
    Fl_Box hint(12, row * 4, width - 24, row,
                "Voxel size in mm | Drag, wheel or arrow keys");
    Fl_Box status(12, row * 5, width - 24, row * 2);
    status.align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    Fl_Button cancel(width / 2 - 160, row * 7 + 4, 140, row - 8, "Cancel"),
        remesh(width / 2 + 20, row * 7 + 4, 140, row - 8, "Remesh");
    struct Result { Document *doc; Fl_Window *window; } result{&doc, &window};
    cancel.callback([](Fl_Widget *widget, void *) { widget->window()->hide(); });
    remesh.callback([](Fl_Widget *, void *data) {
      auto &result = *static_cast<Result *>(data);
      try {
        result.doc->end_voxel_remesh(true);
        result.window->hide();
      } catch (const std::exception &e) {
        result.doc->report_error(e.what());
      }
    }, &result);
    window.end();
    window.set_modal();
    dx::style(&window);
    window.show();
    cancel.take_focus();
    while (window.shown()) {
      auto frame = doc.frame().state;
      if (frame->controls.parameter != VrParameter::VoxelSize) {
        window.hide();
        break;
      }
      char label[64];
      snprintf(label, sizeof(label), "Voxel size: %.2f mm", double(frame->voxel_size_mm));
      value.copy_label(label);
      status.copy_label(frame->error.c_str());
      Fl::wait(.05);
    }
    if (doc.frame().state->controls.parameter == VrParameter::VoxelSize)
      doc.end_voxel_remesh(false);
  } catch (...) {
    if (doc.frame().state->controls.parameter == VrParameter::VoxelSize)
      doc.end_voxel_remesh(false);
    throw;
  }
}
} // namespace dome
