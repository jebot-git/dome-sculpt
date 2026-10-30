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
void edit_cut_parameters(Document &doc,VrParameter parameter){
  doc.begin_cut_parameter(parameter);
  try{
    auto controls=doc.frame().state->controls;
    int row=std::max(32,dx::size+14),sx,sy,sw,sh;Fl::screen_work_area(sx,sy,sw,sh);
    int width=std::min(std::max(520,dx::size*34),sw-40);
    fl_font(dx::font,dx::size);int text_width=width-40,text_height=0;
    fl_measure(controls.title.c_str(),text_width,text_height,0);int header=std::max(row,text_height+16);
    const char *hint_text="Drag, wheel or arrow keys.\nDone keeps the plane; Apply cut creates the mesh.";
    text_width=width-36;text_height=0;fl_measure(hint_text,text_width,text_height,0);
    int hint_height=std::max(row*2,text_height+16);
    int body=header+row*controls.components*2+hint_height+row*3,height=std::min(body+row,sh-40);
    DomeDialog window(width,height,"Plane cut controls");
    Fl_Scroll scroll(0,0,width,height-row);scroll.type(Fl_Scroll::VERTICAL);
    Fl_Group content(0,0,width-24,body);content.resizable(nullptr);
    Fl_Box title(12,4,width-36,header-4,controls.title.c_str());title.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);
    struct Callback{Document *doc;int component;};std::vector<Callback> callbacks;callbacks.reserve(controls.components);
    std::vector<std::unique_ptr<Fl_Box>> labels;std::vector<std::unique_ptr<VoxelSlider>> sliders;
    for(int n=0;n<controls.components;++n){
      labels.push_back(std::make_unique<Fl_Box>(12,header+row*n*2,width-36,row));
      auto slider=std::make_unique<VoxelSlider>(24,header+row*(n*2+1),width-60,row);
      slider->type(FL_HOR_NICE_SLIDER);slider->bounds(0,1);slider->step(.001);
      slider->value((controls.value[n]-controls.minimum)/(controls.maximum-controls.minimum));
      callbacks.push_back({&doc,n});slider->callback([](Fl_Widget *w,void *data){auto &c=*static_cast<Callback *>(data);
        try{c.doc->edit_cut_parameter(c.component,float(static_cast<Fl_Slider *>(w)->value()));}catch(const std::exception &e){c.doc->report_error(e.what());}
      },&callbacks.back());sliders.push_back(std::move(slider));
    }
    Fl_Box hint(12,header+row*controls.components*2,width-36,hint_height,hint_text);
    hint.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);
    Fl_Box status(12,header+row*controls.components*2+hint_height,width-36,row*3);
    status.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);
    content.end();scroll.end();
    Fl_Button cancel(16,height-row+4,width/2-24,row-8,"Cancel"),done(width/2+8,height-row+4,width/2-24,row-8,"Done");
    cancel.callback([](Fl_Widget *w,void *){w->window()->hide();});
    done.callback([](Fl_Widget *w,void *data){auto &doc=*static_cast<Document *>(data);
      try{doc.end_cut_parameter(true);w->window()->hide();}catch(const std::exception &e){doc.report_error(e.what());}
    },&doc);
    window.end();window.set_modal();dx::style(&window);window.show();cancel.take_focus();
    while(window.shown()){
      auto frame=doc.frame().state;if(frame->controls.parameter!=parameter){window.hide();break;}
      for(int n=0;n<controls.components;++n){char label[80];
        if(parameter==VrParameter::CutOffset)snprintf(label,sizeof(label),"Offset: %.2f mm",double(frame->controls.value[n]));
        else if(parameter==VrParameter::CutDirection)snprintf(label,sizeof(label),"%s: %.1f degrees",n?"Pitch":"Yaw",double(frame->controls.value[n]));
        else snprintf(label,sizeof(label),"%c: %.0f%%","RGBA"[n],double(frame->controls.value[n]*100));
        labels[n]->copy_label(label);}
      status.copy_label(frame->error.c_str());
      Fl::wait(.05);
    }
    if(doc.frame().state->controls.parameter==parameter)doc.end_cut_parameter(false);
  }catch(...){if(doc.frame().state->controls.parameter==parameter)doc.end_cut_parameter(false);throw;}
}
void edit_retopology_distance(Document &doc) {
  auto view = doc.frame().state->retopology;
  if (!view || view->committing) return;
  int row = std::max(32, dx::size + 14), sx, sy, sw, sh;
  Fl::screen_work_area(sx, sy, sw, sh);
  int width = std::min(std::max(440, dx::size * 28), sw - 40);
  int height = std::min(row * 5, sh - 40);
  DomeDialog window(width, height, "Retopology projection distance");
  Fl_Scroll scroll(0, 0, width, height - row);
  scroll.type(Fl_Scroll::VERTICAL);
  Fl_Group content(0, 0, width - 24, row * 4);
  content.resizable(nullptr);
  Fl_Box value(12, 4, width - 36, row * 2);
  value.align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
  VoxelSlider slider(24, row * 2, width - 60, row);
  slider.type(FL_HOR_NICE_SLIDER); slider.bounds(0, 1); slider.step(.001);
  slider.value(std::log(view->snap_mm) / std::log(200.f));
  content.end(); scroll.end();
  Fl_Button cancel(16, height - row + 4, width / 2 - 24, row - 8, "Cancel"),
      apply(width / 2 + 8, height - row + 4, width / 2 - 24, row - 8, "Apply");
  bool accepted = false;
  cancel.callback([](Fl_Widget *w, void *) { w->window()->hide(); });
  apply.callback([](Fl_Widget *w, void *data) { *static_cast<bool *>(data) = true; w->window()->hide(); }, &accepted);
  window.end(); window.set_modal(); dx::style(&window); window.show(); cancel.take_focus();
  while (window.shown()) {
    char label[128]; snprintf(label, sizeof(label), "Maximum source distance: %.1f mm\nDrag, wheel or arrow keys", std::pow(200., slider.value()));
    value.copy_label(label); Fl::wait(.05);
  }
  if (accepted) doc.retopology_distance(float(std::pow(200., slider.value())));
}
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
