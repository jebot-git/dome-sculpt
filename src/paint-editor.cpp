// SPDX-License-Identifier: GPL-3.0-or-later
#include "paint-editor.hpp"
#include "appearance.h"
#include "ui-conventions.hpp"
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Value_Input.H>
#include <FL/fl_ask.H>
namespace dome {
void edit_paint(Document &doc) {
  auto settings = doc.frame().state->paint;
  int row = std::max(28, dx::size + 12), width = std::max(560, dx::size * 34),
      sx, sy, sw, sh;
  Fl::screen_work_area(sx, sy, sw, sh);
  width = std::min(width, sw - 40);
  int height = std::min(20 * row, sh - 60), label = width * 2 / 5;
  DomeDialog window(width, height, "Paint brush");
  Fl_Scroll scroll(0, 0, width, height - 2 * row);
  Fl_Group content(0, 0, width - 24, row * 15);
  content.resizable(nullptr);
  int y = row;
  Fl_Choice channel(label, y, width - label - 40, row - 4, "Channel");
  channel.add("Base color|Metallic|Roughness|Emission|Layer mask");
  channel.value(int(settings.channel));
  y += row;
  auto field = [&](const char *name, double value, double maximum=1) {
    auto input =
        new Fl_Value_Input(label, y, width - label - 40, row - 4, name);
    input->bounds(0, maximum);
    input->value(value);
    input->step(.01);
    y += row;
    return input;
  };
  std::array<Fl_Value_Input *, 4> color;
  const char *names[] = {"Red", "Green", "Blue", "Paint opacity"};
  for (int i = 0; i < 4; ++i)
    color[i] = field(names[i], settings.color[i]);
  auto metal = field("Metallic", settings.metallic),
       rough = field("Roughness", settings.roughness);
  Fl_Choice units(label,y,width-label-40,row-4,"Emission units");
  units.add("Material-relative (0..1)|Physical radiance (0..10000)");
  units.value(settings.emission_units==PaintEmissionUnits::Physical?1:0);y+=row;
  std::array<Fl_Value_Input *, 3> emission;
  const char *glow[] = {"Emission red", "Emission green", "Emission blue"};
  for (int i = 0; i < 3; ++i)
    emission[i] = field(glow[i], settings.emission[i],units.value()?maximum_native_emission:1);
  for(auto *value:emission){value->step(0);value->tooltip("Emission is linear. Physical values are absolute; material-relative values follow authored MToon strength. Original source factors remain unchanged.");}
  struct UnitChoice {Fl_Choice *choice;std::array<Fl_Value_Input*,3> values;int selected;} unit_choice{&units,emission,units.value()};
  units.callback([](Fl_Widget*,void *data){auto &u=*static_cast<UnitChoice*>(data);const int next=u.choice->value();double peak=0;for(auto *v:u.values)peak=std::max(peak,v->value());
    if(!next&&peak>1){if(fl_choice("Relative emission supports 0..1. Normalize brush RGB to preserve its hue? This changes brush settings only.","Cancel","Normalize",nullptr)!=1){u.choice->value(u.selected);return;}for(auto *v:u.values)v->value(v->value()/peak);}
    u.selected=next;for(auto *v:u.values)v->bounds(0,next?maximum_native_emission:1);
  },&unit_choice);
  auto mask = field("Mask value", settings.mask),
       flow = field("Flow", settings.flow);
  Fl_Check_Button erase(20, y, width / 2 - 20, row, "Erase channel"),
      front(width / 2, y, width / 2 - 20, row, "Front-facing only");
  erase.value(settings.erase);
  front.value(settings.front_only);
  y += row;
  content.size(width-24,y+row);
  content.end();
  scroll.end();
  Fl_Button cancel(width / 2 - 150, height - 2 * row + 8, 140, row, "Cancel"),
      apply(width / 2 + 10, height - 2 * row + 8, 140, row, "Apply");
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
  settings.channel = PaintChannel(channel.value());
  settings.emission_units=units.value()?PaintEmissionUnits::Physical:PaintEmissionUnits::MaterialRelative;
  for (int i = 0; i < 4; ++i)
    settings.color[i] = color[i]->value();
  settings.metallic = metal->value();
  settings.roughness = rough->value();
  for (int i = 0; i < 3; ++i)
    settings.emission[i] = emission[i]->value();
  settings.mask = mask->value();
  settings.flow = flow->value();
  settings.erase = erase.value();
  settings.front_only = front.value();
  doc.paint_settings(settings);
}
} // namespace dome
