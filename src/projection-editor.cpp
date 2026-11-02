// SPDX-License-Identifier: GPL-3.0-or-later
#include "projection-editor.hpp"
#include "ui-conventions.hpp"
#include "appearance.h"
#include <FL/Fl_Box.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Value_Input.H>
#include <FL/fl_ask.H>
namespace dome {
std::optional<std::filesystem::path> projection_image_dialog(){
  // File selection stays native. PNG pixel decoding belongs exclusively to the
  // bounded Document worker, not the toolkit's optional synchronous preview.
  Fl_File_Chooser chooser(".","*.{png,PNG}",Fl_File_Chooser::SINGLE,"Projection image");
  chooser.preview(0);chooser.previewButton->hide();chooser.textfont(dx::font);chooser.textsize(dx::size);
  int x,y,w,h;Fl::screen_work_area(x,y,w,h);
  chooser.show();chooser.resize(x+16,y+16,std::min(chooser.w(),w-32),std::min(chooser.h(),h-32));
  while(chooser.shown())Fl::wait();
  if(const auto path=chooser.value();path&&*path)return std::filesystem::path(path);
  return {};
}
std::optional<ProjectionToolSettings> projection_dialog(ProjectionToolSettings settings,ProjectionCamera view,const std::string &image){
 int sx,sy,sw,sh;Fl::screen_work_area(sx,sy,sw,sh);int row=std::max(36,dx::size+16),width=std::min(720,sw-32),height=std::min(row*12,sh-32);
 DomeDialog window(width,height,"Image projection");
 Fl_Scroll scroll(0,0,width,height-row*2);scroll.type(Fl_Scroll::VERTICAL);
 Fl_Group content(0,0,width-24,row*9+20);content.resizable(nullptr);
 Fl_Box name(16,8,width-32,row,image.c_str());name.align(FL_ALIGN_INSIDE|FL_ALIGN_CENTER);
 Fl_Box hint(16,row+8,width-32,row*2,"Frame model, then use From current view.\nXR: Place projector uses its own trigger.");hint.align(FL_ALIGN_INSIDE|FL_ALIGN_CENTER|FL_ALIGN_WRAP);
 Fl_Choice field(16,row*3+8,width-40,row-6);
 for(size_t i=0;i<projection_parameter_count;++i)field.add(projection_parameter_text(settings,i).c_str());field.value(0);
 Fl_Box value(16,row*4+8,width-32,row);value.align(FL_ALIGN_INSIDE|FL_ALIGN_CENTER);
 Fl_Button minus(20,row*5+8,(width-60)/2,row-6,"Decrease"),plus(40+(width-60)/2,row*5+8,(width-60)/2,row-6,"Increase");
 Fl_Button from_view(20,row*6+8,width-40,row-6,"From current view");
 Fl_Check_Button enabled(20,row*7+8,width/2-30,row,"Use image"),stencil(width/2,row*7+8,width/2-20,row,"Stencil");
 Fl_Value_Input x(50,row*8+8,(width-130)/3,row-6,"X"),y(80+(width-130)/3,row*8+8,(width-130)/3,row-6,"Y"),z(110+2*(width-130)/3,row*8+8,(width-130)/3,row-6,"Z");
 content.end();scroll.end();
 Fl_Button cancel(width/2-150,height-row-8,140,row-6,"Cancel"),apply(width/2+10,height-row-8,140,row-6,"Apply");
 struct Draft{ProjectionToolSettings settings;ProjectionCamera view;Fl_Choice *field;Fl_Box *value;Fl_Check_Button *enabled,*stencil;Fl_Value_Input *x,*y,*z;Fl_Window *window;bool accepted=false;
 void update(){value->copy_label(projection_parameter_text(settings,size_t(field->value())).c_str());x->value(settings.camera.pose.position.x);y->value(settings.camera.pose.position.y);z->value(settings.camera.pose.position.z);}
 } d{settings,view,&field,&value,&enabled,&stencil,&x,&y,&z,&window};
 enabled.value(settings.enabled);stencil.value(settings.image.usage==ProjectionUsage::Stencil);
 auto coordinates=[](Fl_Widget*,void*p){auto &d=*static_cast<Draft*>(p);d.settings.camera.pose.position={float(d.x->value()),float(d.y->value()),float(d.z->value())};};
 for(auto *input:{&x,&y,&z}){input->when(FL_WHEN_CHANGED);input->callback(coordinates,&d);}
 field.callback([](Fl_Widget*,void*p){static_cast<Draft*>(p)->update();},&d);
 struct Step{Draft *d;int direction;};Step down{&d,-1},up{&d,1};
 auto step=[](Fl_Widget*,void*p){auto &s=*static_cast<Step*>(p);try{adjust_projection_parameter(s.d->settings,size_t(s.d->field->value()),s.direction);s.d->update();}catch(const std::exception&e){fl_alert("%s",e.what());}};minus.callback(step,&down);plus.callback(step,&up);
 from_view.callback([](Fl_Widget*,void*p){auto &d=*static_cast<Draft*>(p);d.settings.camera=d.view;d.update();},&d);
 cancel.callback([](Fl_Widget*w,void*){w->window()->hide();});
 apply.callback([](Fl_Widget*,void*p){auto &d=*static_cast<Draft*>(p);try{d.settings.enabled=d.enabled->value();d.settings.image.usage=d.stencil->value()?ProjectionUsage::Stencil:ProjectionUsage::Color;d.settings.camera.pose.position={float(d.x->value()),float(d.y->value()),float(d.z->value())};d.settings.validate();d.accepted=true;d.window->hide();}catch(const std::exception&e){fl_alert("%s",e.what());}},&d);
 d.update();window.end();window.set_modal();dx::style(&window);window.show();cancel.take_focus();while(window.shown())Fl::wait();if(d.accepted)return d.settings;return {};
}
}
