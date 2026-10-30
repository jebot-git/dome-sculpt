// SPDX-License-Identifier: GPL-3.0-or-later
#include "import-editor.hpp"
#include "appearance.h"
#include "ui-conventions.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Value_Input.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
namespace dome {
std::optional<ImportSettings> import_options_dialog(const std::filesystem::path &path,ImportSettings settings) {
  settings.validate();int sx,sy,sw,sh;Fl::screen_work_area(sx,sy,sw,sh);
  int row=std::max(36,dx::size+16),width=std::min(std::max(650,dx::size*40),sw-40),height=std::min(row*14,sh-40);
  DomeDialog window(width,height,"Import options");
  Fl_Scroll scroll(0,0,width,height-row*2);scroll.type(Fl_Scroll::VERTICAL);
  Fl_Group content(0,0,width-24,row*13);content.resizable(nullptr);
  std::string filename=path.filename().string();
  Fl_Box name(20,8,width-56,row*2,filename.c_str());name.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);
  int y=row*2+16,label_width=std::max(180,width/3);
  auto choice=[&](const char *label,const char *values,int selected){
    auto field=new Fl_Choice(label_width,y,width-label_width-40,row-6,label);field->add(values);field->value(selected);y+=row;return field;
  };
  auto format=choice("Format","From filename|GLB|glTF|OBJ|FBX",int(settings.choices.format));
  auto units=choice("Input units","From file|Metres|Centimetres|Millimetres|Custom",int(settings.choices.units));
  Fl_Value_Input custom(label_width,y,width-label_width-40,row-6,"Metres per unit");custom.bounds(1e-12,1e12);custom.step(.001);custom.value(settings.choices.custom_metres_per_unit);y+=row;
  auto axes=choice("Input axes","From file|Y-up|Z-up",int(settings.choices.axes));
  auto normals=choice("Normals","Preserve|Generate flat|Generate smooth",int(settings.choices.normals));
  int resolution=0;for(auto n=settings.paint_resolution;n>64;n>>=1)++resolution;
  auto paint=choice("Paint resolution","64|128|256|512|1024|2048",resolution);
  Fl_Check_Button materials(20,y,width-56,row,"Import materials and textures");materials.value(settings.choices.materials);y+=row;
  const char *retained="Original source files are retained. Hierarchy, instances, skins and morphs are preserved.";
  Fl_Box info(20,y,width-56,row*2,retained);info.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);y+=row*2;
  const char *meaning="From file reads FBX units and axes. Unitless OBJ assumes metres. Explicit normal generation replaces source shading normals.";
  Fl_Box help(20,y,width-56,row*3,meaning);help.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);y+=row*3;
  content.size(width-24,y);content.end();scroll.end();
  struct Draft {
    Fl_Choice *format,*units,*axes,*normals,*paint;Fl_Value_Input *custom;Fl_Check_Button *materials;
    Fl_Window *window;ImportSettings result;bool native=false,accepted=false;
    void update(){
      bool active=format->value()==3||format->value()==4||(format->value()==0&&native);
      for(auto field:{units,axes,normals}){if(active)field->activate();else {field->value(0);field->deactivate();}}
      if(active)materials->activate();else {materials->value(1);materials->deactivate();}
      if(active&&units->value()==int(ImportUnits::Custom))custom->activate();else custom->deactivate();
    }
  } draft{format,units,axes,normals,paint,&custom,&materials,&window,settings};
  auto extension=path.extension().string();for(auto &c:extension)c=char(std::tolower(static_cast<unsigned char>(c)));
  draft.native=extension==".obj"||extension==".fbx";
  format->callback([](Fl_Widget*,void*v){static_cast<Draft*>(v)->update();},&draft);
  units->callback([](Fl_Widget*,void*v){static_cast<Draft*>(v)->update();},&draft);draft.update();
  int button_width=std::min(width/2-24,std::max(140,dx::size*8));
  Fl_Button cancel(width/2-button_width-10,height-row*2+8,button_width,row,"Cancel"),apply(width/2+10,height-row*2+8,button_width,row,"Import");
  cancel.callback([](Fl_Widget*w,void*){w->window()->hide();});
  apply.callback([](Fl_Widget*,void*v){auto &d=*static_cast<Draft*>(v);try{
    d.result.choices.format=ImportSourceFormat(d.format->value());d.result.choices.units=ImportUnits(d.units->value());
    d.result.choices.axes=ImportAxes(d.axes->value());d.result.choices.normals=ImportNormals(d.normals->value());
    d.result.choices.custom_metres_per_unit=d.custom->value();d.result.choices.materials=d.materials->value();
    d.result.paint_resolution=64u<<d.paint->value();d.result.validate();d.accepted=true;d.window->hide();
  }catch(const std::exception&e){fl_alert("%s",e.what());}},&draft);
  window.end();window.set_modal();dx::style(&window);window.show();cancel.take_focus();
  while(window.shown())Fl::wait();
  if(draft.accepted)return draft.result;
  return {};
}
} // namespace dome
