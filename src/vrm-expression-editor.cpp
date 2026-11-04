// SPDX-License-Identifier: GPL-3.0-or-later
#include "vrm-expression-editor.hpp"
#include "dome/document.hpp"
#include "appearance.h"
#include "ui-conventions.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Value_Input.H>
#include <FL/fl_ask.H>
namespace dome {
namespace {
struct DialogResult {Fl_Window *window;bool accepted=false;};
void cancel(Fl_Widget *widget,void*){widget->window()->hide();}
void accept(Fl_Widget*,void *data){auto &result=*static_cast<DialogResult*>(data);result.accepted=true;result.window->hide();}
AssetSource source(const VrmExpressionView &view){return {view.generation,view.revision,view.object,view.edit_authority};}
std::shared_ptr<const VrmExpressionView> ready(Document &doc){const auto view=doc.frame().state->expressions;if(!view||view->entries.empty())throw std::logic_error("Start expression preview and wait for its original source definitions");return view;}
}
void choose_expression_dialog(Document &doc){
 const auto view=ready(doc);int sx,sy,sw,sh;Fl::screen_work_area(sx,sy,sw,sh);const int row=std::max(34,dx::size+14),width=std::min(std::max(560,dx::size*28),sw-40),height=std::min(row*6,sh-40);
 DomeDialog window(width,height,"Choose VRM1 expression");Fl_Box hint(16,8,width-32,row*2,"Select an original expression.\nPreview changes need Apply.");hint.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);
 Fl_Choice choice(20,row*3,width-40,row-4,"Expression");choice.align(FL_ALIGN_TOP_LEFT);std::vector<std::string> labels;labels.reserve(view->entries.size());for(const auto &entry:view->entries)labels.push_back((entry.custom?"Custom: ":"Preset: ")+entry.name+(entry.binary?" (binary)":""));std::vector<Fl_Menu_Item> entries;entries.reserve(labels.size()+1);for(const auto &label:labels){Fl_Menu_Item entry{};entry.label(label.c_str());entries.push_back(entry);}entries.emplace_back();choice.copy(entries.data());choice.value(int(view->selected));
 const int bw=std::min(width/2-24,std::max(140,dx::size*8));Fl_Button no(width/2-bw-10,height-row-12,bw,row,"Cancel"),yes(width/2+10,height-row-12,bw,row,"Select");DialogResult result{&window};no.callback(cancel);yes.callback(accept,&result);window.end();window.set_modal();dx::style(&window);window.show();no.take_focus();while(window.shown())Fl::wait();
 if(result.accepted){const auto index=size_t(std::max(0,choice.value()));doc.configure_expression_preview(index,view->entries.at(index).weight,view->session,source(*view));}
}
void expression_weight_dialog(Document &doc){
 const auto view=ready(doc);const auto &entry=view->entries.at(view->selected);int sx,sy,sw,sh;Fl::screen_work_area(sx,sy,sw,sh);const int row=std::max(34,dx::size+14),width=std::min(std::max(560,dx::size*28),sw-40),height=std::min(row*6,sh-40);
 DomeDialog window(width,height,"Preview expression weight");const std::string help=entry.binary?"Weight: 0..1. Preview only.\nBinary turns on above 0.5.":"Weight: 0..1. Preview only.\nApply static pose to keep it.";Fl_Box hint(16,8,width-32,row*2,help.c_str());hint.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);Fl_Value_Input value(20,row*3,width-40,row-4,"Weight");value.align(FL_ALIGN_TOP_LEFT);value.bounds(0,1);value.step(.01);value.value(entry.weight);
 const int bw=std::min(width/2-24,std::max(140,dx::size*8));Fl_Button no(width/2-bw-10,height-row-12,bw,row,"Cancel"),yes(width/2+10,height-row-12,bw,row,"Preview");DialogResult result{&window};no.callback(cancel);yes.callback(accept,&result);window.end();window.set_modal();dx::style(&window);window.show();value.take_focus();while(window.shown())Fl::wait();
 if(result.accepted)doc.configure_expression_preview(view->selected,float(value.value()),view->session,source(*view));
}
}
