// SPDX-License-Identifier: GPL-3.0-or-later
#include "resolution-editor.hpp"
#include "resolution-ui-values.hpp"
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
std::optional<SculptResolutionOptions> sculpt_resolution_dialog(float voxel,float scale,SculptResolutionOptions options) {
  ResolutionUiValues values(voxel,scale,std::move(options));
  int row=std::max(36,dx::size+16),sx,sy,sw,sh;Fl::screen_work_area(sx,sy,sw,sh);
  int width=std::min(std::max(680,dx::size*40),sw-40),height=std::min(row*15,sh-40);
  DomeDialog window(width,height,"Sculpt resolution");
  Fl_Scroll scroll(0,0,width,height-row*2);scroll.type(Fl_Scroll::VERTICAL);
  Fl_Group content(0,0,width-24,row*15);content.resizable(nullptr);
  const char *help="Change sampling of the current sculpt.\nLayers, protection and original painting are retained.";
  fl_font(dx::font,dx::size);int text_width=width-48,header=0;fl_measure(help,text_width,header,0);header=std::max(row*2,header+16);
  Fl_Box hint(16,4,width-48,header-4,help);hint.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);
  int y=header;char current_text[128];snprintf(current_text,sizeof(current_text),"Current spacing: %.6g world mm",values.current_mm);
  Fl_Box current(16,y,width-48,row,current_text);current.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE);y+=row;
  auto number=[&](const char *label,double value,double minimum,double maximum) {
    const int label_width=std::min(width/2,dx::size*17);
    bool stacked=fl_width(label)>label_width-20;
    if(stacked){auto caption=new Fl_Box(20,y,width-56,row-4,label);caption->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);y+=row;}
    auto input=new Fl_Value_Input(stacked?24:label_width,y,stacked?width-64:width-label_width-40,row-6,stacked?nullptr:label);
    input->bounds(minimum,maximum);
    double step=std::pow(10.,std::floor(std::log10(values.current_mm))-2);
    input->step(std::max(values.minimum_mm,step));input->value(value);y+=row;
    return input;
  };
  auto target=number("Voxel spacing (world mm)",values.target_mm,values.minimum_mm,values.maximum_mm);
  Fl_Slider slider(24,y,width-64,row-6);slider.type(FL_HOR_NICE_SLIDER);slider.bounds(0,1);slider.step(.0001);slider.value(values.normalized());y+=row;
  Fl_Button finer(24,y,(width-80)/2,row-6,"Half spacing"),coarser(40+(width-80)/2,y,(width-80)/2,row-6,"Double spacing");y+=row;
  Fl_Check_Button automatic(20,y,width-56,row,"Automatic quality bounds");automatic.value(values.automatic);y+=row;
  auto surface=number("Maximum surface change (world mm)",values.surface_mm,values.minimum_mm*.001,values.factor);
  auto layer=number("Maximum layer change (world mm)",values.layer_mm,values.minimum_mm*.001,values.factor*1e6);
  auto normal=number("Maximum normal change (degrees)",values.normal_degrees,0,90);
  auto mask=number("Maximum protection change (%)",values.mask_percent,0,100);normal->step(1);mask->step(1);
  const char *quality="Coarse grids may lose thin details. Unsupported quality or scene limits are refused.";
  int footer_width=width-48,footer=0;fl_measure(quality,footer_width,footer,0);footer=std::max(row*2,footer+16);
  Fl_Box quality_hint(16,y,width-48,footer,quality);quality_hint.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);y+=footer;
  content.size(width-24,y);content.end();scroll.end();
  struct Draft {
    ResolutionUiValues *values;Fl_Value_Input *target,*surface,*layer,*normal,*mask;Fl_Slider *slider;
    Fl_Check_Button *automatic;Fl_Window *window;bool accepted=false;
    void update() {
      values->automatic=automatic->value();values->automatic_bounds();
      if(values->automatic){surface->value(values->surface_mm);layer->value(values->layer_mm);surface->deactivate();layer->deactivate();}
      else {surface->activate();layer->activate();}
    }
  } draft{&values,target,surface,layer,normal,mask,&slider,&automatic,&window};
  target->callback([](Fl_Widget*,void *v){auto &d=*static_cast<Draft*>(v);d.values->target_mm=d.target->value();if(std::isfinite(d.values->target_mm) && d.values->target_mm>0)d.slider->value(d.values->normalized());d.update();},&draft);
  slider.callback([](Fl_Widget*,void *v){auto &d=*static_cast<Draft*>(v);d.values->slider(d.slider->value());d.target->value(d.values->target_mm);d.update();},&draft);
  automatic.callback([](Fl_Widget*,void *v){static_cast<Draft*>(v)->update();},&draft);
  struct Preset {Draft *draft;double factor;};Preset fine{&draft,.5},coarse{&draft,2};
  auto preset=[](Fl_Widget*,void *v){auto &p=*static_cast<Preset*>(v);auto &d=*p.draft;d.values->target_mm=std::clamp(d.target->value()*p.factor,d.values->minimum_mm,d.values->maximum_mm);d.target->value(d.values->target_mm);d.slider->value(d.values->normalized());d.update();};
  finer.callback(preset,&fine);coarser.callback(preset,&coarse);draft.update();
  int button_width=std::min(width/2-24,std::max(140,dx::size*8));
  Fl_Button cancel(width/2-button_width-10,height-row*2+8,button_width,row,"Cancel"),apply(width/2+10,height-row*2+8,button_width,row,"Apply");
  cancel.callback([](Fl_Widget *widget,void*){widget->window()->hide();});
  apply.callback([](Fl_Widget*,void *v){auto &d=*static_cast<Draft*>(v);try {
    d.values->target_mm=d.target->value();d.values->automatic=d.automatic->value();d.values->surface_mm=d.surface->value();d.values->layer_mm=d.layer->value();d.values->normal_degrees=d.normal->value();d.values->mask_percent=d.mask->value();
    d.values->options=d.values->accepted();d.accepted=true;d.window->hide();
  }catch(const std::exception &e){fl_alert("%s",e.what());}},&draft);
  window.end();window.set_modal();dx::style(&window);window.show();cancel.take_focus();while(window.shown())Fl::wait();
  if(draft.accepted)return values.options;
  return {};
}
}
