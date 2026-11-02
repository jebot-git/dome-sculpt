// SPDX-License-Identifier: GPL-3.0-or-later
#include "rig-fit-editor.hpp"
#include "dome/document.hpp"
#include "appearance.h"
#include "ui-conventions.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Value_Input.H>
#include <FL/fl_ask.H>
namespace dome {
namespace {
Vec3 world_point(const AffineTransform &m,Vec3 p){auto v=m.point({p.x,p.y,p.z});return {float(v[0]),float(v[1]),float(v[2])};}
}
void edit_humanoid_fit_dialog(Document &doc){
 doc.begin_rig_fit();const auto published=doc.frame().state->rig_fit;if(!published)throw std::logic_error("Humanoid fitting is unavailable");
 int row=std::max(36,dx::size+16),sx,sy,sw,sh;Fl::screen_work_area(sx,sy,sw,sh);
 int width=std::min(std::max(700,dx::size*40),sw-40),height=std::min(row*16,sh-40),label=width/2;
 DomeDialog window(width,height,"Humanoid rig fitting");Fl_Scroll scroll(0,0,width,height-row*2);scroll.type(Fl_Scroll::VERTICAL);
 Fl_Group content(0,0,width-24,row*25);content.resizable(nullptr);
 Fl_Box hint(16,4,width-48,row*3,"Place every required landmark, then review the body frame and optional fingers.\nCoordinates and distances are world metres. Original painting and shapekeys are retained.\nApply fits on the worker and commits one undo step.");hint.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);
 int y=row*3;
 Fl_Choice role(label,y,width-label-40,row-4,"Landmark");y+=row;std::vector<Fl_Menu_Item> roles;std::vector<std::string> names;names.reserve(human_roles().size());
 for(size_t i=0;i<human_roles().size();++i)names.push_back(rig_fit_role_label(i));for(auto &n:names){Fl_Menu_Item item{};item.label(n.c_str());roles.push_back(item);}roles.emplace_back();role.copy(roles.data());role.value(int(published->role));
 Fl_Check_Button enabled(20,y,width-56,row,"Landmark is placed");y+=row;
 auto number=[&](const char *name,double value,double lo,double hi){auto *w=new Fl_Value_Input(label,y,width-label-40,row-4,name);w->bounds(lo,hi);w->step(.001);w->value(value);y+=row;return w;};
 std::array<Fl_Value_Input *,3> xyz={number("World X (m)",0,-10000,10000),number("World Y (m)",0,-10000,10000),number("World Z (m)",0,-10000,10000)};
 auto *step=number("Adjustment step (m)",published->step,.00001,10);
 Fl_Choice frame(label,y,width-label-40,row-4,"Model body frame");y+=row;frame.add("Keep current body frame|+Y up, +Z front|+Z up, -Y front|+Y up, -Z front|+Z up, +Y front");frame.value(0);
 auto *surface=number("Maximum surface distance (m)",published->options.fit.maximum_surface_distance,.00001,10);
 auto *minimum=number("Minimum segment length (m)",published->options.fit.minimum_segment_length,.000001,10);
 auto *terminal=number("Terminal bone length (m)",published->options.fit.terminal_length,.000001,10);
 Fl_Check_Button bind(20,y,width-56,row,"Bind skin weights to the fitted skeleton");bind.value(published->options.bind);y+=row;
 auto *seed=number("Skin seed distance (m)",published->options.binding.maximum_seed_distance,.00001,10000);
 auto *margin=number("Skin seed margin (m)",published->options.binding.seed_margin,0,10000);
 auto *falloff=number("Skin falloff",published->options.binding.falloff,1,8);
 auto *influences=number("Maximum skin influences",published->options.binding.limits.max_influences,1,32);influences->step(1);
 content.size(width-24,y+row);content.end();scroll.end();
 struct Draft {
  Document *doc;RigFitView view;Fl_Choice *role,*frame;Fl_Check_Button *enabled,*bind;std::array<Fl_Value_Input *,3> xyz;
  Fl_Value_Input *step,*surface,*minimum,*terminal,*seed,*margin,*falloff,*influences;Fl_Window *window;bool accepted=false,updating=false;
  void load(){updating=true;view.role=size_t(role->value());const auto &name=human_roles().at(view.role).name;auto it=view.landmarks.points.find(name);enabled->value(it!=view.landmarks.points.end());auto p=world_point(view.world,it==view.landmarks.points.end()?Vec3{}:it->second);xyz[0]->value(p.x);xyz[1]->value(p.y);xyz[2]->value(p.z);updating=false;}
  void update(){if(updating)return;view.role=size_t(role->value());const auto &name=human_roles().at(view.role).name;if(enabled->value()){
   Vec3 p{float(xyz[0]->value()),float(xyz[1]->value()),float(xyz[2]->value())};if(!finite(p))throw std::invalid_argument("Landmark coordinates must be finite");view.landmarks.points[name]=world_point(view.world.inverse(),p);
  }else view.landmarks.points.erase(name);
  view.step=float(step->value());view.options.fit.maximum_surface_distance=float(surface->value());view.options.fit.minimum_segment_length=float(minimum->value());view.options.fit.terminal_length=float(terminal->value());view.options.bind=bind->value();view.options.binding.maximum_seed_distance=float(seed->value());view.options.binding.seed_margin=float(margin->value());view.options.binding.falloff=float(falloff->value());view.options.binding.limits.max_influences=uint32_t(influences->value());doc->configure_rig_fit(view);}
 } draft{&doc,*published,&role,&frame,&enabled,&bind,xyz,step,surface,minimum,terminal,seed,margin,falloff,influences,&window};
 role.callback([](Fl_Widget *,void *p){auto &d=*static_cast<Draft*>(p);try{d.load();d.doc->configure_rig_fit(d.view);}catch(const std::exception &e){fl_alert("%s",e.what());}},&draft);
 auto changed=[](Fl_Widget *,void *p){auto &d=*static_cast<Draft*>(p);try{d.update();}catch(const std::exception &e){fl_alert("%s",e.what());}};
 for(auto *w:xyz)w->callback(changed,&draft);for(auto *w:{step,surface,minimum,terminal,seed,margin,falloff,influences})w->callback(changed,&draft);enabled.callback(changed,&draft);bind.callback(changed,&draft);
 frame.callback([](Fl_Widget *,void *p){auto &d=*static_cast<Draft*>(p);try{
  static const std::array<Vec3,4> left={Vec3{1,0,0},Vec3{1,0,0},Vec3{-1,0,0},Vec3{-1,0,0}};
  static const std::array<Vec3,4> up={Vec3{0,1,0},Vec3{0,0,1},Vec3{0,1,0},Vec3{0,0,1}};
  if(d.frame->value()==0)return;
  d.view.landmarks.left=left.at(size_t(d.frame->value()-1));d.view.landmarks.up=up.at(size_t(d.frame->value()-1));d.view.landmarks.front=cross(d.view.landmarks.left,d.view.landmarks.up);d.update();
 }catch(const std::exception &e){fl_alert("%s",e.what());}},&draft);
 const int button_width=std::min(width/2-24,std::max(140,dx::size*8));Fl_Button cancel(width/2-button_width-10,height-row*2+8,button_width,row,"Cancel"),apply(width/2+10,height-row*2+8,button_width,row,"Apply");
 cancel.callback([](Fl_Widget *w,void *){w->window()->hide();});apply.callback([](Fl_Widget *,void *p){auto &d=*static_cast<Draft*>(p);try{d.update();d.doc->accept_rig_fit();d.accepted=true;d.window->hide();}catch(const std::exception &e){fl_alert("%s",e.what());}},&draft);
 draft.load();window.end();window.set_modal();dx::style(&window);window.show();cancel.take_focus();while(window.shown())Fl::wait();if(!draft.accepted)doc.cancel_rig_fit({published->generation,published->revision,published->object,published->edit_authority});
}
} // namespace dome
