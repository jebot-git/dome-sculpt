// SPDX-License-Identifier: GPL-3.0-or-later
#include "polygon-editor.hpp"
#include "appearance.h"
#include "ui-conventions.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl_Value_Input.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
namespace dome {
bool polygon_conversion_dialog() {
  int row = std::max(36, dx::size + 16), sx, sy, sw, sh;
  Fl::screen_work_area(sx, sy, sw, sh);
  int width = std::min(std::max(680, dx::size * 40), sw - 40),
      height = std::min(row * 9, sh - 40);
  DomeDialog window(width, height, "Convert to editable polygons");
  Fl_Scroll scroll(0, 0, width, height - row * 2);
  scroll.type(Fl_Scroll::VERTICAL);
  Fl_Group content(0, 0, width - 24, height);
  content.resizable(nullptr);
  const char *help =
      "Convert the visible shape to native polygon geometry.\n"
      "Original sculpt, paint, rig, morphs and source files remain restorable.\n"
      "The editable result uses recalculated polygon normals.\n"
      "Unsupported material transport is refused before changing the scene.";
  fl_font(dx::font, dx::size);
  int text_width = width - 48, header = 0;
  fl_measure(help, text_width, header, 0);
  header = std::max(row * 2, header + 16);
  Fl_Box hint(16, 4, width - 48, header - 4, help);
  hint.align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
  content.size(width - 24, header + 8);
  content.end();
  scroll.end();
  int button_width = std::min(width / 2 - 24, std::max(140, dx::size * 9));
  Fl_Button cancel(width / 2 - button_width - 10, height - row * 2 + 8,
                   button_width, row, "Cancel"),
      convert(width / 2 + 10, height - row * 2 + 8, button_width, row,
              "Convert");
  bool accepted = false;
  cancel.callback([](Fl_Widget *w, void *) { w->window()->hide(); });
  convert.callback(
      [](Fl_Widget *w, void *p) {
        *static_cast<bool *>(p) = true;
        w->window()->hide();
      },
      &accepted);
  window.end();
  window.set_modal();
  dx::style(&window);
  window.show();
  cancel.take_focus();
  while (window.shown())
    Fl::wait();
  return accepted;
}
std::optional<PolygonControls>
polygon_controls_dialog(PolygonControls controls) {
  int row = std::max(36, dx::size + 16), sx, sy, sw, sh;
  Fl::screen_work_area(sx, sy, sw, sh);
  int width = std::min(std::max(680, dx::size * 40), sw - 40),
      height = std::min(row * 11, sh - 40);
  DomeDialog window(width, height, "Polygon edit controls");
  Fl_Scroll scroll(0, 0, width, height - row * 2);
  scroll.type(Fl_Scroll::VERTICAL);
  Fl_Group content(0, 0, width - 24, row * 10);
  content.resizable(nullptr);
  const char *help = "Pick vertices, edges or complete N-gon faces.\n"
                     "Move drags the selected components on a view-facing plane.\n"
                     "Distances use scene millimetres; the original source stays intact until Apply.";
  fl_font(dx::font, dx::size);
  int text_width = width - 48, header = 0;
  fl_measure(help, text_width, header, 0);
  header = std::max(row * 2, header + 16);
  Fl_Box hint(16, 4, width - 48, header - 4, help);
  hint.align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
  int y = header, label = std::min(width / 2, dx::size * 15);
  Fl_Value_Input distance(label, y, width - label - 40, row - 6,
                          "Distance (mm)");
  y += row;
  distance.bounds(.001, 1000000);
  distance.step(.1);
  distance.value(controls.distance_mm);
  Fl_Slider slider(20, y, width - 64, row - 6);
  y += row;
  slider.type(FL_HOR_NICE_SLIDER);
  slider.bounds(std::log(.001), std::log(1000000.));
  slider.step(.001);
  slider.value(std::log(controls.distance_mm));
  struct Pair {
    Fl_Value_Input *number;
    Fl_Slider *slider;
  } pair{&distance, &slider};
  distance.callback(
      [](Fl_Widget *, void *p) {
        auto &c = *static_cast<Pair *>(p);
        if (std::isfinite(c.number->value()) && c.number->value() > 0)
          c.slider->value(std::log(c.number->value()));
      },
      &pair);
  slider.callback(
      [](Fl_Widget *, void *p) {
        auto &c = *static_cast<Pair *>(p);
        c.number->value(std::exp(c.slider->value()));
      },
      &pair);
  Fl_Value_Input cuts(label, y, width - label - 40, row - 6, "Edge cuts");
  y += row;
  cuts.bounds(1, 64);
  cuts.step(1);
  cuts.value(controls.cuts);
  Fl_Check_Button move(20, y, width - 64, row, "Move picked selection");
  y += row;
  move.value(controls.move_mode);
  Fl_Check_Button additive(20, y, width - 64, row,
                           "Add / toggle picked components");
  y += row;
  additive.value(controls.additive);
  content.size(width - 24, y + 8);
  content.end();
  scroll.end();
  int button_width = std::min(width / 2 - 24, std::max(140, dx::size * 8));
  Fl_Button cancel(width / 2 - button_width - 10, height - row * 2 + 8,
                   button_width, row, "Cancel"),
      apply(width / 2 + 10, height - row * 2 + 8, button_width, row, "Apply");
  struct Draft {
    Fl_Value_Input *distance, *cuts;
    Fl_Check_Button *move, *additive;
    PolygonControls *value;
    bool accepted = false;
  } draft{&distance, &cuts, &move, &additive, &controls};
  cancel.callback([](Fl_Widget *w, void *) { w->window()->hide(); });
  apply.callback(
      [](Fl_Widget *w, void *p) {
        auto &d = *static_cast<Draft *>(p);
        try {
          double distance = d.distance->value(), cuts = d.cuts->value();
          if (!std::isfinite(distance) || distance < .001 ||
              distance > 1000000 || !std::isfinite(cuts) || cuts < 1 ||
              cuts > 64 || std::floor(cuts) != cuts)
            throw std::invalid_argument(
                "Enter a distance from0.001 to1000000mm and an integer cut "
                "count from1 to64");
          if (distance != d.value->distance_mm)
            d.value->distance_mm = float(distance);
          d.value->cuts = unsigned(cuts);
          d.value->move_mode = d.move->value();
          d.value->additive = d.additive->value();
          d.accepted = true;
          w->window()->hide();
        } catch (const std::exception &e) {
          fl_alert("%s", e.what());
        }
      },
      &draft);
  window.end();
  window.set_modal();
  dx::style(&window);
  window.show();
  cancel.take_focus();
  while (window.shown())
    Fl::wait();
  if (draft.accepted)
    return controls;
  return {};
}
} // namespace dome
