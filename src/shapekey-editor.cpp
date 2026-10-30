// SPDX-License-Identifier: GPL-3.0-or-later
#include "shapekey-editor.hpp"
#include "appearance.h"
#include "ui-conventions.hpp"
#include <FL/Fl_Choice.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Value_Input.H>
#include <FL/Fl_Scroll.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
namespace dome {
void edit_shapekey_dialog(Document &doc,ShapeKeyAction action){
 auto draft=doc.draft_scene();const auto &o=draft.scene.active();auto state=doc.frame().state;
 if(!state->can_author_shapes)throw std::invalid_argument("Choose an editable mesh after finishing current editing or object motion");
 const bool selected=action==ShapeKeyAction::Duplicate||action==ShapeKeyAction::Mirror||action==ShapeKeyAction::Rename;
 const auto *keys=o.deformation&&o.deformation->morphs?&*o.deformation->morphs:nullptr;
 if(selected&&(!keys||keys->targets.empty()))throw std::invalid_argument("This mesh has no shape targets");
 int sx,sy,sw,sh;Fl::screen_work_area(sx,sy,sw,sh);int row=std::max(32,dx::size+14),width=std::min(std::max(560,dx::size*34),sw-40),height=std::min(row*8,sh-40);
 const char *title=action==ShapeKeyAction::Mirror?"Mirror shapekey":action==ShapeKeyAction::Duplicate?"Duplicate shapekey":action==ShapeKeyAction::Rename?"Rename shapekey":action==ShapeKeyAction::CaptureMix?"Capture shape mix":"New shapekey";
 DomeDialog window(width,height,title);Fl_Scroll scroll(0,0,width,height-row*2);scroll.type(Fl_Scroll::VERTICAL);
 Fl_Group content(0,0,width-24,row*6);content.resizable(nullptr);
 const char *help="Original mesh state stays restorable.\nMirror uses the local X=0 plane.";
 fl_font(dx::font,dx::size);int text_width=width-56,header=0;fl_measure(help,text_width,header,0);header=std::max(row*2,header+16);
 Fl_Box hint(16,4,width-56,header-4,help);hint.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);
 int y=header+row;Fl_Choice target(20,y,width-64,row-4,"Source target");target.align(FL_ALIGN_TOP_LEFT);y+=row*2;
 std::vector<Fl_Menu_Item> entries;if(keys)for(const auto &key:keys->targets){Fl_Menu_Item item{};item.label(key.name.c_str());entries.push_back(item);}entries.emplace_back();target.copy(entries.data());target.value(int(state->morph_target));if(!selected)target.deactivate();
 Fl_Input name(20,y,width-64,row-4,"Name (empty: automatic)");name.align(FL_ALIGN_TOP_LEFT);y+=row*2;
 if(action==ShapeKeyAction::Rename)name.value(keys->targets[state->morph_target].name.c_str());
 Fl_Value_Input tolerance(20,y,width-64,row-4,"Local tolerance (mm)");tolerance.align(FL_ALIGN_TOP_LEFT);y+=row;tolerance.bounds(.001,1000);tolerance.step(.01);tolerance.value(.1);if(action!=ShapeKeyAction::Mirror)tolerance.deactivate();
 content.size(width-24,y+16);content.end();scroll.end();
 int button_width=std::min(width/2-24,std::max(140,dx::size*9));
 Fl_Button cancel(width/2-button_width-10,height-row*2+8,button_width,row,"Cancel"),apply(width/2+10,height-row*2+8,button_width,row,"Apply");
 struct State{Fl_Window *window;Fl_Choice *target;Fl_Input *name;Fl_Value_Input *tolerance;ShapeKeyAction action;std::optional<ShapeKeyEdit> request;} local{&window,&target,&name,&tolerance,action,{}};
 target.callback([](Fl_Widget*,void *p){auto &s=*static_cast<State*>(p);if(s.action==ShapeKeyAction::Rename)s.name->value(s.target->text());},&local);
 cancel.callback([](Fl_Widget *w,void*){w->window()->hide();});
 apply.callback([](Fl_Widget*,void *p){auto &s=*static_cast<State*>(p);try{
  ShapeKeyEdit edit;edit.action=s.action;edit.target=uint32_t(std::max(0,s.target->value()));edit.name=s.name->value();
  double value=s.tolerance->value();if(!std::isfinite(value)||value<.001||value>1000)throw std::invalid_argument("Mirror tolerance is outside the supported range");edit.mirror_tolerance=float(value*.001);
  if(s.action==ShapeKeyAction::Rename&&edit.name.empty())throw std::invalid_argument("Enter a nonempty shape name");s.request=std::move(edit);s.window->hide();
 }catch(const std::exception &e){fl_alert("%s",e.what());}},&local);
 window.end();window.set_modal();dx::style(&window);window.show();cancel.take_focus();while(window.shown())Fl::wait();
 if(local.request)doc.author_shape(std::move(*local.request),draft.source);
}
} // namespace dome
