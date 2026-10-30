// SPDX-License-Identifier: GPL-3.0-or-later
#include "sculpt-editor.hpp"
#include "sculpt-ui-values.hpp"
#include "appearance.h"
#include "ui-conventions.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl_Value_Input.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
namespace dome {
std::optional<Volume::SculptSettings> sculpt_controls_dialog(Volume::SculptSettings settings) {
  int row=std::max(36,dx::size+16),sx,sy,sw,sh;Fl::screen_work_area(sx,sy,sw,sh);
  int width=std::min(std::max(680,dx::size*40),sw-40),height=std::min(row*15,sh-40);
  int label=std::min(width/2,dx::size*15),value_width=std::min(width-label-60,std::max(90,dx::size*5));
  DomeDialog window(width,height,"Sculpt masks and symmetry");
  Fl_Scroll scroll(0,0,width,height-row*2);scroll.type(Fl_Scroll::VERTICAL);
  Fl_Group content(0,0,width-24,row*15);content.resizable(nullptr);
  const char *help="Masks protect all sculpt tools across layers.\nMirror planes use object-local millimetres.";
  fl_font(dx::font,dx::size);int measured_width=width-48,header=0;fl_measure(help,measured_width,header,0);
  header=std::max(row*2,header+16);
  Fl_Box hint(16,4,width-48,header-4,help);hint.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);
  int y=header;
  Fl_Check_Button edit(20,y,width-50,row,"Edit protection mask");edit.value(settings.mask_edit);y+=row;
  Fl_Choice tool(label,y,width-label-40,row-6,"Mask tool");tool.add("Add protection");tool.add("Remove protection");tool.add("Smooth protection");
  tool.value(int(settings.mask_tool));y+=row;
  struct Paired {
    Fl_Value_Input *number=nullptr;Fl_Slider *slider=nullptr;bool plane=false;
    void from_number() {slider->value(plane?sculpt_plane_slider(float(number->value()/1000)):number->value());}
    void from_slider() {number->value(plane?sculpt_plane_metres(float(slider->value()))*1000:slider->value());}
  };
  std::array<Paired,4> pairs;
  auto pair=[&](int index,const char *name,double value,bool plane) {
    bool stacked=width-label-value_width-48<120;
    if(stacked) {
      auto caption=new Fl_Box(20,y,width-64,row-6,name);
      caption->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);y+=row;
    }
    int left=stacked?20:label;
    auto number=new Fl_Value_Input(left,y,value_width,row-6,stacked?nullptr:name);
    auto slider=new Fl_Slider(left+value_width+8,y,width-left-value_width-48,row-6);
    slider->type(FL_HOR_NICE_SLIDER);slider->bounds(0,1);slider->step(.0001);
    number->bounds(plane?-100000:0,plane?100000:1);number->step(plane?.1:.01);number->value(value);
    pairs[index]={number,slider,plane};auto p=&pairs[index];p->from_number();
    number->callback([](Fl_Widget*,void *v){static_cast<Paired*>(v)->from_number();},p);
    slider->callback([](Fl_Widget*,void *v){static_cast<Paired*>(v)->from_slider();},p);y+=row;
  };
  pair(0,"Mask strength",settings.mask_strength,false);
  Fl_Check_Button overlay(20,y,width-50,row,"Show protected areas");overlay.value(settings.mask_visible);y+=row;
  Fl_Box mirror_hint(16,y,width-48,row,"Mirror brush paths and normals");mirror_hint.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE);y+=row;
  int third=(width-64)/3;
  Fl_Check_Button x(20,y,third,row,"Mirror X"),yy(20+third,y,third,row,"Mirror Y"),z(20+third*2,y,third,row,"Mirror Z");
  x.value(settings.symmetry_axes&1);yy.value(settings.symmetry_axes&2);z.value(settings.symmetry_axes&4);y+=row;
  pair(1,"Plane X (mm)",settings.symmetry_origin.x*1000,true);
  pair(2,"Plane Y (mm)",settings.symmetry_origin.y*1000,true);
  pair(3,"Plane Z (mm)",settings.symmetry_origin.z*1000,true);
  Fl_Button reset(label,y,width-label-40,row-6,"Center planes");y+=row;
  reset.callback([](Fl_Widget*,void *v){auto &p=*static_cast<std::array<Paired,4>*>(v);for(int i=1;i<4;++i){p[i].number->value(0);p[i].from_number();}},&pairs);
  const char *domain="Fill, Clear and Invert are separate mask commands.\nFill protects future geometry; layer visibility stays independent.";
  int footer_width=width-48,footer=0;fl_measure(domain,footer_width,footer,0);footer=std::max(row*2,footer+16);
  Fl_Box domain_hint(16,y,width-48,footer,domain);domain_hint.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);y+=footer;
  content.size(width-24,y);content.end();scroll.end();
  int button_width=std::min(width/2-24,std::max(140,dx::size*8));
  Fl_Button cancel(width/2-button_width-10,height-row*2+8,button_width,row,"Cancel"),
      apply(width/2+10,height-row*2+8,button_width,row,"Apply");
  struct Draft {
    Fl_Window *window;Fl_Check_Button *edit,*overlay,*x,*y,*z;Fl_Choice *tool;
    std::array<Paired,4> *pairs;Volume::SculptSettings *settings;bool accepted=false;
  } draft{&window,&edit,&overlay,&x,&yy,&z,&tool,&pairs,&settings};
  cancel.callback([](Fl_Widget*w,void*){w->window()->hide();});
  apply.callback([](Fl_Widget*,void *v){
    auto &d=*static_cast<Draft*>(v);
    try {
      auto next=*d.settings;
      for(auto &p:*d.pairs)if(!std::isfinite(p.number->value()) || p.number->value()<p.number->minimum() || p.number->value()>p.number->maximum())
        throw std::invalid_argument("Enter finite values within the displayed control ranges");
      if(d.tool->value()<0 || d.tool->value()>2)throw std::invalid_argument("Choose a mask tool");
      next.mask_edit=d.edit->value();next.mask_visible=d.overlay->value();next.mask_tool=Volume::MaskTool(d.tool->value());
      next.mask_strength=float((*d.pairs)[0].number->value());
      next.symmetry_axes=(d.x->value()?1u:0u)|(d.y->value()?2u:0u)|(d.z->value()?4u:0u);
      float *axes[]={&next.symmetry_origin.x,&next.symmetry_origin.y,&next.symmetry_origin.z};
      for(int k=0;k<3;++k) if((*d.pairs)[k+1].number->value()!=double(*axes[k]*1000))
        *axes[k]=float((*d.pairs)[k+1].number->value()/1000);
      *d.settings=next;d.accepted=true;d.window->hide();
    }catch(const std::exception &e){fl_alert("%s",e.what());}
  },&draft);
  window.end();window.set_modal();dx::style(&window);window.show();cancel.take_focus();
  while(window.shown())Fl::wait();
  if(draft.accepted)return settings;
  return std::nullopt;
}
}
