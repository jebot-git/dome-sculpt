// SPDX-License-Identifier: GPL-3.0-or-later
#include "texture-set-editor.hpp"
#include "appearance.h"
#include "ui-conventions.hpp"
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Value_Input.H>
#include <FL/fl_ask.H>
namespace dome {
namespace {
void choose_paint_resolution(Fl_Choice &choice,uint32_t current) {
 choice.add("64|128|256|512|1024|2048");
 int grid=0;
 while(grid<5&&(64u<<grid)<current)++grid;
 choice.value(grid);
}

void resize_native_paint(Document &doc,const AssetSource &source,uint32_t current) {
 int x,y,w,h;
 Fl::screen_work_area(x,y,w,h);
 const int row=std::max(28,dx::size+12);
 const int width=std::min(std::max(400,dx::size*29),w-40);
 DomeDialog window(width,row*4,"Paint resolution");
 const int label=width/2;
 Fl_Choice resolution(label,row,width-label-16,row-4,"Grid size (pixels)");
 choose_paint_resolution(resolution,current);
 Fl_Button cancel(label-150,row*3,140,row-4,"Cancel");
 Fl_Button apply(label+10,row*3,140,row-4,"Apply");
 bool accepted=false;
 cancel.callback([](Fl_Widget *widget,void *){widget->window()->hide();});
 apply.callback([](Fl_Widget *widget,void *data){
  *static_cast<bool *>(data)=true;
  widget->window()->hide();
 },&accepted);
 window.end();
 window.set_modal();
 dx::style(&window);
 window.show();
 cancel.take_focus();
 while(window.shown())Fl::wait();
 if(accepted){
  TextureSetEdit edit;
  edit.action=TextureSetAction::Resolution;
  edit.resolution=64u<<resolution.value();
  doc.edit_texture_set(std::move(edit),source);
 }
}
}
void edit_texture_sets(Document&doc){
 const auto state=doc.frame().state;
 const AssetSource source{state->generation,state->revision,state->active_object,state->edit_authority};
 const bool selection_only=!state->can_edit_texture_sets;
 if(selection_only&&state->texture_sets.size()==1&&doc.can_resize_native_paint()){
  resize_native_paint(doc,source,state->texture_sets.front().resolution);
  return;
 }
 if(selection_only&&!doc.can_select_texture_sets()){
  fl_alert("Choose an editable painting mesh and finish pending work first.");return;
 }
 int sx,sy,sw,sh;Fl::screen_work_area(sx,sy,sw,sh);
 const int row=std::max(28,dx::size+12),width=std::min(std::max(560,dx::size*36),sw-40),height=std::min(row*(selection_only?5:20),sh-60),label=width*2/5;
 DomeDialog window(width,height,"Material texture sets");Fl_Scroll scroll(0,0,width,height-row*2);Fl_Group content(0,0,width-24,row*19);content.resizable(nullptr);int y=row;
 Fl_Choice member(label,y,width-label-40,row-4,"Material set");
 std::vector<std::string>labels;labels.reserve(state->texture_sets.size());std::vector<Fl_Menu_Item>items(state->texture_sets.size()+1);
 int selected=0;for(size_t k=0;k<state->texture_sets.size();++k){const auto&s=state->texture_sets[k];labels.push_back(std::to_string(s.id)+": "+s.name+" / "+std::to_string(s.resolution)+" / "+std::to_string(s.triangles)+" faces");items[k].label(labels.back().c_str());if(s.id==state->active_texture_set)selected=int(k);}
 member.copy(items.data());member.value(selected);y+=row;
 Fl_Group operations(0,y,width-24,row*17);operations.resizable(nullptr);
 Fl_Choice action(label,y,width-label-40,row-4,"Apply action");action.add("Select set|New set from picked faces|Assign picked faces to set|Rename set|Edit active set material|Resize active set|Use MToon 1.0");action.value(0);y+=row;
 Fl_Choice resolution(label,y,width-label-40,row-4,"Paint resolution");choose_paint_resolution(resolution,state->texture_sets[size_t(selected)].resolution);y+=row;
 Fl_Output faces(label,y,width-label-40,row-4,"Picked faces");const auto count=std::to_string(state->texture_faces.size());faces.value(count.c_str());y+=row;
 Fl_Input name(label,y,width-label-40,row-4,"New / replacement name");name.value(state->texture_sets[size_t(selected)].name.c_str());y+=row;
 const auto material=state->texture_material;
 auto field=[&](const char*name,double value,double maximum=1){auto*p=new Fl_Value_Input(label,y,width-label-40,row-4,name);p->bounds(0,maximum);p->step(.01);p->value(value);y+=row;return p;};
 std::array<Fl_Value_Input*,4>color;const char*names[]={"Red","Green","Blue","Opacity"};for(size_t k=0;k<4;++k)color[k]=field(names[k],material.color[k]);
 auto*metal=field("Metallic",material.metallic);auto*rough=field("Roughness",material.roughness);
 std::array<Fl_Value_Input*,3>emission;const char*glow[]={"Emission red","Emission green","Emission blue"};for(size_t k=0;k<3;++k)emission[k]=field(glow[k],material.emissive[k],maximum_native_emission);
 Fl_Check_Button toon(16,y,width/2-20,row,"Toon material"),sided(width/2,y,width/2-20,row,"Double sided");toon.value(material.toon);sided.value(material.double_sided);y+=row;
 Fl_Output help(16,y,width-56,row*2);help.value("Pick visible mesh faces in the viewport. Each set has independent paint layers; other sets stay visible. Resize applies to the active set and preserves every layer. Fine detail may prevent a quality-safe reduction.");operations.end();
 if(selection_only)operations.hide();
 content.size(width-24,selection_only?row*3:y+row*3);content.end();scroll.end();
 Fl_Button cancel(width/2-150,height-row*1.5,140,row,"Cancel"),apply(width/2+10,height-row*1.5,140,row,"Apply");bool accepted=false;
 cancel.callback([](Fl_Widget*w,void*){w->window()->hide();});apply.callback([](Fl_Widget*w,void*p){*static_cast<bool*>(p)=true;w->window()->hide();},&accepted);
 window.end();window.set_modal();dx::style(&window);window.show();while(window.shown())Fl::wait();if(!accepted)return;
 if(member.value()<0||size_t(member.value())>=state->texture_sets.size())throw std::invalid_argument("Select an actual material set");
 TextureSetEdit edit;edit.set=state->texture_sets[size_t(member.value())].id;edit.name=name.value();
 switch(action.value()){
 case 0:edit.action=TextureSetAction::Select;break;
 case 1:edit.action=TextureSetAction::Create;edit.faces=state->texture_faces;break;
 case 2:edit.action=TextureSetAction::AssignFaces;edit.faces=state->texture_faces;break;
 case 3:edit.action=TextureSetAction::Rename;break;
 case 6:edit.action=TextureSetAction::MToon;break;
 case 5:
  if(edit.set!=state->active_texture_set)throw std::invalid_argument("Select that material set before resizing its paint");
  if(resolution.value()<0||resolution.value()>5)throw std::invalid_argument("Choose a real paint resolution");
  edit.action=TextureSetAction::Resolution;edit.resolution=64u<<resolution.value();break;
 case 4:
  if(edit.set!=state->active_texture_set)throw std::invalid_argument("Select that material set before editing its values");
  edit.action=TextureSetAction::Material;edit.material=material;
  for(size_t k=0;k<4;++k)edit.material.color[k]=float(color[k]->value());for(size_t k=0;k<3;++k)edit.material.emissive[k]=float(emission[k]->value());
  edit.material.metallic=float(metal->value());edit.material.roughness=float(rough->value());edit.material.toon=toon.value();edit.material.double_sided=sided.value();break;
 default:throw std::invalid_argument("Unknown texture-set action");
 }
 doc.edit_texture_set(std::move(edit),source);
}
} // namespace dome
