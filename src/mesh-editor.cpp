// SPDX-License-Identifier: GPL-3.0-or-later
#include "mesh-editor.hpp"
#include "appearance.h"
#include "ui-conventions.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Slider.H>
#include <FL/fl_draw.H>
#include <cstdio>
namespace dome {
namespace {
class VoxelSlider : public Fl_Slider {
public:
  using Fl_Slider::Fl_Slider;
  int handle(int event) override {
    if (event == FL_MOUSEWHEEL) {
      int delta = Fl::event_dy() ? Fl::event_dy() : Fl::event_dx();
      if (delta && value(clamp(value() - delta * .01))) {
        set_changed();
        do_callback();
      }
      return 1;
    }
    return Fl_Slider::handle(event);
  }
};
} // namespace
void edit_voxel_remesh(Document &doc) {
  doc.begin_voxel_remesh();
  try {
    int row = std::max(32, dx::size + 14), width = std::max(560, dx::size * 34);
    int sx, sy, sw, sh;
    Fl::screen_work_area(sx, sy, sw, sh);
    width = std::min(width, sw - 40);
    const char *description_text =
        "Create a new sculpt from the displayed shape.\n"
        "Source paint, rig and layers stay in a hidden object.";
    const char *hint_text = "Drag, wheel or arrow keys";
    fl_font(dx::font, dx::size);
    auto measured_height = [&](const char *text, int minimum) {
      int text_width = width - 32, text_height = 0;
      fl_measure(text, text_width, text_height, 0);
      return std::max(minimum, text_height + 12);
    };
    int header = measured_height(description_text, row * 2);
    int value_height = measured_height("Voxel size: 1000.00 mm", row);
    int hint_height = measured_height(hint_text, row);
    int body_height = header + value_height + hint_height + row * 3;
    int height = std::min(body_height + row, sh - 40);
    DomeDialog window(width, height, "Voxel remesh");
    Fl_Scroll scroll(0, 0, width, height - row);
    scroll.type(Fl_Scroll::VERTICAL);
    Fl_Group content(0, 0, width - 24, body_height);
    content.resizable(nullptr);
    Fl_Box description(12, 6, width - 32, header - 6, description_text);
    description.align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    Fl_Box value(12, header, width - 32, value_height, "");
    value.align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    VoxelSlider slider(24, header + value_height, width - 48, row);
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
    Fl_Box hint(12, header + value_height + row, width - 32, hint_height, hint_text);
    hint.align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    Fl_Box status(12, header + value_height + row + hint_height, width - 32, row * 2);
    status.align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    content.end();
    scroll.end();
    Fl_Button cancel(24, height - row + 4, width / 2 - 36, row - 8, "Cancel"),
        remesh(width / 2 + 12, height - row + 4, width / 2 - 36, row - 8, "Remesh");
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
