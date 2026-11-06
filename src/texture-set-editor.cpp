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
void edit_texture_sets(Document&doc){
 const auto state=doc.frame().state;
 if(!state->can_edit_texture_sets){fl_alert("Choose an editable painting mesh and finish pending work first.");return;}
 const AssetSource source{state->generation,state->revision,state->active_object,state->edit_authority};
 int sx,sy,sw,sh;Fl::screen_work_area(sx,sy,sw,sh);
 const int row=std::max(28,dx::size+12),width=std::min(std::max(560,dx::size*36),sw-40),height=std::min(row*20,sh-60),label=width*2/5;
 DomeDialog window(width,height,"Material texture sets");Fl_Scroll scroll(0,0,width,height-row*2);Fl_Group content(0,0,width-24,row*19);content.resizable(nullptr);int y=row;
 Fl_Choice member(label,y,width-label-40,row-4,"Material set");
 std::vector<std::string>labels;labels.reserve(state->texture_sets.size());std::vector<Fl_Menu_Item>items(state->texture_sets.size()+1);
 int selected=0;for(size_t k=0;k<state->texture_sets.size();++k){const auto&s=state->texture_sets[k];labels.push_back(std::to_string(s.id)+": "+s.name+" / "+std::to_string(s.resolution)+" / "+std::to_string(s.triangles)+" faces");items[k].label(labels.back().c_str());if(s.id==state->active_texture_set)selected=int(k);}
 member.copy(items.data());member.value(selected);y+=row;
 Fl_Choice action(label,y,width-label-40,row-4,"Apply action");action.add("Select set|New set from picked faces|Assign picked faces to set|Rename set|Edit active set material|Resize active set");action.value(0);y+=row;
 Fl_Choice resolution(label,y,width-label-40,row-4,"Paint resolution");resolution.add("64|128|256|512|1024|2048");int grid=0;while(grid<5&&(64u<<grid)<state->texture_sets[size_t(selected)].resolution)++grid;resolution.value(grid);y+=row;
 Fl_Output faces(label,y,width-label-40,row-4,"Picked faces");const auto count=std::to_string(state->texture_faces.size());faces.value(count.c_str());y+=row;
 Fl_Input name(label,y,width-label-40,row-4,"New / replacement name");name.value(state->texture_sets[size_t(selected)].name.c_str());y+=row;
 const auto material=state->texture_material;
 auto field=[&](const char*name,double value,double maximum=1){auto*p=new Fl_Value_Input(label,y,width-label-40,row-4,name);p->bounds(0,maximum);p->step(.01);p->value(value);y+=row;return p;};
 std::array<Fl_Value_Input*,4>color;const char*names[]={"Red","Green","Blue","Opacity"};for(size_t k=0;k<4;++k)color[k]=field(names[k],material.color[k]);
 auto*metal=field("Metallic",material.metallic);auto*rough=field("Roughness",material.roughness);
 std::array<Fl_Value_Input*,3>emission;const char*glow[]={"Emission red","Emission green","Emission blue"};for(size_t k=0;k<3;++k)emission[k]=field(glow[k],material.emissive[k],maximum_native_emission);
 Fl_Check_Button toon(16,y,width/2-20,row,"Toon material"),sided(width/2,y,width/2-20,row,"Double sided");toon.value(material.toon);sided.value(material.double_sided);y+=row;
 Fl_Output help(16,y,width-56,row*2);help.value("Pick visible mesh faces in the viewport. Each set has independent paint layers; other sets stay visible. Resize applies to the active set and preserves every layer. Fine detail may prevent a quality-safe reduction.");content.size(width-24,y+row*3);content.end();scroll.end();
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
