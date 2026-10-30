// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural-editor.hpp"
#include "appearance.h"
#include "ui-conventions.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Value_Input.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
namespace dome {
std::optional<ProceduralSculptOptions> procedural_sculpt_dialog(float scale,ProceduralSculptOptions options) {
  if(!std::isfinite(scale)||scale<=0)throw std::invalid_argument("Invalid sculpt world scale");
  const double factor=double(scale)*1000;
  int row=std::max(36,dx::size+16),sx,sy,sw,sh;Fl::screen_work_area(sx,sy,sw,sh);
  int width=std::min(std::max(640,dx::size*36),sw-40),height=std::min(row*15,sh-40);
  DomeDialog window(width,height,"Procedural sculpt detail");
  Fl_Scroll scroll(0,0,width,height-row*2);scroll.type(Fl_Scroll::VERTICAL);
  Fl_Group content(0,0,width-24,row*15);content.resizable(nullptr);
  const char *hint="Project seeded panels, recesses or mixed details onto a sculpt face.\nProtection masks and mirror planes apply. A new sculpt layer is created.\nDepth should span several voxels for visible detail.";
  fl_font(dx::font,dx::size);int text_width=width-64,header=0;fl_measure(hint,text_width,header,0);header=std::max(row*3,header+20);
  Fl_Box help(20,8,width-64,header-12,hint);
  help.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);
  int y=header,label=std::min(width/2,dx::size*15);
  auto choice=[&](const char *caption,const char *items,int selected) {
    auto input=new Fl_Choice(label,y,width-label-44,row-6,caption);input->add(items);input->value(selected);y+=row;return input;
  };
  auto number=[&](const char *caption,double value,double low,double high,double step) {
    auto input=new Fl_Value_Input(label,y,width-label-44,row-6,caption);input->bounds(low,high);input->step(step);input->value(value);y+=row;return input;
  };
  auto pattern=choice("Pattern","Raised panels|Recessed cuts|Mixed",int(options.pattern));
  auto face=choice("Object-local face","+X|-X|+Y|-Y|+Z|-Z",int(options.axis*2+options.reverse));
  auto rows=number("Rows",options.rows,1,32,1),columns=number("Columns",options.columns,1,32,1);
  auto seed=number("Seed",options.seed,0,UINT32_MAX,1);
  auto density=number("Density (%)",options.density*100,0,100,1);
  auto gap=number("Gap (%)",options.gap*100,0,90,1);
  auto depth=number("Depth (world mm)",options.depth*factor,.00001*factor,10*factor,.01);
  auto rounding=number("Rounding (%)",options.rounding*100,0,100,1);
  content.size(width-24,y+12);content.end();scroll.end();
  struct Draft {
    Fl_Window *window;ProceduralSculptOptions *options;double factor;Fl_Choice *pattern,*face;
    Fl_Value_Input *rows,*columns,*seed,*density,*gap,*depth,*rounding;bool accepted=false;
  } draft{&window,&options,factor,pattern,face,rows,columns,seed,density,gap,depth,rounding};
  int bw=std::min(width/2-24,std::max(140,dx::size*8));
  Fl_Button cancel(width/2-bw-10,height-row*2+8,bw,row,"Cancel"),apply(width/2+10,height-row*2+8,bw,row,"Apply");
  cancel.callback([](Fl_Widget *w,void*){w->window()->hide();});
  apply.callback([](Fl_Widget*,void *v){auto &d=*static_cast<Draft*>(v);try {
    auto o=*d.options;
    auto integer=[](double value,double low,double high){if(!std::isfinite(value)||value<low||value>high||std::round(value)!=value)throw std::invalid_argument("Rows, columns and seed must be whole numbers within their bounds");return uint32_t(value);};
    o.pattern=ProceduralSculptOptions::Pattern(d.pattern->value());o.axis=unsigned(d.face->value())/2;o.reverse=(d.face->value()%2)!=0;
    o.rows=integer(d.rows->value(),1,32);o.columns=integer(d.columns->value(),1,32);o.seed=integer(d.seed->value(),0,UINT32_MAX);
    o.density=float(d.density->value()/100);o.gap=float(d.gap->value()/100);o.rounding=float(d.rounding->value()/100);
    o.depth=float(d.depth->value()/d.factor);o.validate();*d.options=std::move(o);d.accepted=true;d.window->hide();
  }catch(const std::exception &e){fl_alert("%s",e.what());}},&draft);
  window.end();window.set_modal();dx::style(&window);window.show();cancel.take_focus();while(window.shown())Fl::wait();
  if(draft.accepted)return options;
  return {};
}
} // namespace dome
