// SPDX-License-Identifier: GPL-3.0-or-later
#include "export-editor.hpp"
#include "appearance.h"
#include "ui-conventions.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Scroll.H>
#include <FL/fl_draw.H>
namespace dome {
std::optional<AssetExportFormat> editable_export_dialog() {
  int sx,sy,sw,sh;
  Fl::screen_work_area(sx,sy,sw,sh);
  const int row=std::max(36,dx::size+16);
  const int width=std::min(std::max(580,dx::size*35),sw-40);
  const int height=std::min(row*10,sh-40);
  DomeDialog window(width,height,"Editable GLB export");
  Fl_Scroll scroll(0,0,width,height-row-24);
  scroll.type(Fl_Scroll::VERTICAL);
  Fl_Group content(0,0,width-24,row*8+40);
  content.resizable(nullptr);
  const char *description="Keeps the complete editable project, including hidden objects. "
      "Other viewers use an approximate PBR view. Open the file in Dome Sculpt to recover the original project.";
  fl_font(dx::font,dx::size);
  int measured_width=width-70,description_height=0;
  fl_measure(description,measured_width,description_height,0);
  description_height+=dx::size;
  Fl_Box info(20,12,width-64,description_height,description);
  info.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);
  const int label=std::min(width/2,std::max(180,dx::size*13));
  const int choice_y=description_height+24;
  Fl_Choice resolution(label,choice_y,width-label-44,row-6,"Portable textures");
  resolution.add("256 pixels|512 pixels|1024 pixels");
  resolution.value(1);
  const char *meaning="Baked paint uses finite textures. Fine repeated patterns and distant views may differ; native source data stays exact.";
  measured_width=width-70;int sampling_height=0;
  fl_measure(meaning,measured_width,sampling_height,0);sampling_height+=dx::size;
  const int meaning_y=choice_y+row+8;
  Fl_Box sampling(20,meaning_y,width-64,sampling_height,meaning);
  sampling.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);
  content.size(width-24,meaning_y+sampling_height+16);
  content.end();scroll.end();
  struct Draft {Fl_Window *window;Fl_Choice *resolution;bool accepted=false;} draft{&window,&resolution};
  const int button=std::min(width/2-24,std::max(140,dx::size*8));
  Fl_Button cancel(width/2-button-10,height-row-12,button,row,"Cancel");
  Fl_Button apply(width/2+10,height-row-12,button,row,"Export");
  cancel.callback([](Fl_Widget *widget,void*){widget->window()->hide();});
  apply.callback([](Fl_Widget*,void *value){auto &draft=*static_cast<Draft*>(value);draft.accepted=true;draft.window->hide();},&draft);
  window.end();window.set_modal();dx::style(&window);window.show();cancel.take_focus();
  while(window.shown())Fl::wait();
  if(!draft.accepted)return {};
  return resolution.value()==0?AssetExportFormat::EditableGlb256:
         resolution.value()==1?AssetExportFormat::EditableGlb512:AssetExportFormat::EditableGlb1024;
}
}
