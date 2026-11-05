// SPDX-License-Identifier: GPL-3.0-or-later
#include "skin-editor.hpp"
#include "skin-methods.hpp"
#include "anatomical-eligibility.hpp"
#include "appearance.h"
#include "ui-conventions.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Check_Browser.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Value_Input.H>
#include <FL/fl_ask.H>
namespace dome {
void edit_skin_dialog(Document &doc) {
  auto draft=doc.draft_scene();const auto &target=draft.scene.active();
  if(!target.visible || target.locked || !target.mesh_active || !target.painted)
    throw std::invalid_argument("Select a visible, unlocked prepared mesh");
  auto state=doc.frame().state;auto options=state->skin_options;
  int row=std::max(32,dx::size+14),sx,sy,sw,sh;Fl::screen_work_area(sx,sy,sw,sh);
  int width=std::min(std::max(720,dx::size*45),sw-40),height=std::min(row*21,sh-40),label=width/2;
  DomeDialog window(width,height,"Bind or transfer skin");
  Fl_Scroll scroll(0,0,width,height-row*2);scroll.type(Fl_Scroll::VERTICAL);
  Fl_Group content(0,0,width-24,row*22);content.resizable(nullptr);
  Fl_Box hint(16,4,width-48,row*3,
    "Bind in the reference rest frame.\n"
    "Keep target paint, morphs and pose.\n"
    "Different rigs need joint mapping.");
  hint.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);
  int y=row*3;
  Fl_Choice mode(label,y,width-label-40,row-4,"Method");y+=row;
  mode.add("Legacy automatic bind");mode.add("Transfer skin weights");mode.add("Anatomical source bind (REST)");mode.add("Automatic (surface heat)");mode.value(detail::skin_method_choice(options.mode));
  Fl_Choice source(label,y,width-label-40,row-4,"Reference mesh");y+=row;
  Fl_Value_Input seed(label,y,width-label-40,row-4,"Seed radius (mm)");y+=row;
  Fl_Value_Input margin(label,y,width-label-40,row-4,"Seed margin (mm)");y+=row;
  Fl_Value_Input distance(label,y,width-label-40,row-4,"Transfer (mm)");y+=row;
  Fl_Value_Input falloff(label,y,width-label-40,row-4,"Distance falloff");y+=row;
  Fl_Value_Input cap(label,y,width-label-40,row-4,"Max influences");y+=row;
  Fl_Value_Input prune(label,y,width-label-40,row-4,"Prune threshold");y+=row;
  seed.bounds(.001,10000000);seed.step(.1);seed.value(options.seed_distance*1000);
  margin.bounds(0,10000000);margin.step(.1);margin.value(options.seed_margin*1000);
  distance.bounds(.001,10000000);distance.step(.1);distance.value(options.transfer_distance*1000);
  falloff.bounds(1,8);falloff.step(.1);falloff.value(options.falloff);
  cap.bounds(1,32);cap.step(1);cap.value(options.limits.max_influences);
  prune.bounds(0,.99);prune.step(.0001);prune.value(options.limits.prune);
  Fl_Check_Button keep(20,y,width-50,row,"Preserve checked influences");y+=row;
  Fl_Check_Browser locks(20,y,width-64,row*6);y+=row*6;
  if(target.deformation && target.deformation->skeleton && target.deformation->skin) {
    const auto &bones=target.deformation->skeleton->bones;
    keep.value(state->skin_use_locks);
    for(size_t i=0;i<bones.size();++i)
      locks.add(bones[i].name.c_str(),state->weights.locks.size()==bones.size() && state->weights.locks[i]);
  } else {keep.value(0);keep.deactivate();locks.deactivate();}
  Fl_Check_Button selected(20,y,width-50,row,"Bind explicitly selected meshes");y+=row;
  Fl_Value_Input work(label,y,width-label-40,row-4,"Work (millions)");y+=row;
  Fl_Value_Input workspace(label,y,width-label-40,row-4,"Working data (MiB)");y+=row;
  Fl_Value_Input total(label,y,width-label-40,row-4,"Owning request total (MiB)");y+=row;
  work.bounds(1,500);work.step(1);work.value(double(options.anatomical.maximum_work)/1e6);
  workspace.bounds(1,256);workspace.step(1);workspace.value(double(options.anatomical.maximum_workspace)/(1024*1024));
  total.bounds(1,512);total.step(1);total.value(double(options.anatomical.maximum_total)/(1024*1024));
  selected.value(0);
  const auto expected_selection=state->selected_objects;
  content.size(width-24,y+row);content.end();scroll.end();
  int button_width=std::min(width/2-24,std::max(140,dx::size*8));
  Fl_Button cancel(width/2-button_width-10,height-row*2+8,button_width,row,"Cancel"),
      apply(width/2+10,height-row*2+8,button_width,row,"Bind / transfer");
  struct Context {
    Fl_Window *window;Fl_Choice *source,*mode;Fl_Button *apply;const Scene *scene;
    uint64_t target;SkinAuthoringOptions *options;std::vector<uint64_t> ids;
    std::vector<std::string> names;
    std::array<Fl_Value_Input*,6> fields;Fl_Check_Button *keep;Fl_Check_Browser *locks;
    Fl_Check_Button *selected;std::array<Fl_Value_Input*,3> budgets;Fl_Box *hint;
    bool has_selection=false;bool accepted=false;
    void refresh() {
      uint64_t previous=source->value()>=0 && size_t(source->value())<ids.size()?ids[source->value()]:options->reference;
      ids.clear();names.clear();std::vector<Fl_Menu_Item> items;
      names.reserve(scene->objects().size());items.reserve(scene->objects().size()+1);
      for(const auto &o:scene->objects()) if((mode->value()!=2 || detail::anatomical_source_target(scene->object(target))) && o.mesh_active && o.painted && o.deformation &&
          o.deformation->skeleton && (mode->value()!=1 || (o.deformation->skin && o.id!=target)) && (mode->value()!=2 || (o.deformation->humanoid && o.deformation->humanoid->joints.contains("head") && o.deformation->humanoid->joints.contains("leftEye") && o.deformation->humanoid->joints.contains("rightEye")))) {
        ids.push_back(o.id);names.push_back(o.name+(o.visible?"":" [hidden]"));
      }
      for(const auto &name:names){Fl_Menu_Item item{};item.label(name.c_str());items.push_back(item);}
      items.emplace_back();source->copy(items.data());
      auto found=std::find(ids.begin(),ids.end(),previous);source->value(found==ids.end()?0:int(found-ids.begin()));
      if(ids.empty())apply->deactivate();else apply->activate();
      const bool anatomical=mode->value()==2,heat=mode->value()==3,bounded=anatomical||heat;
      for(auto *field:fields)field->deactivate();
      if(!anatomical){fields[4]->activate();fields[5]->activate();if(heat)fields[0]->activate();else if(mode->value()==0){fields[0]->activate();fields[1]->activate();fields[3]->activate();}else fields[2]->activate();}
      for(auto *field:budgets)if(bounded)field->activate();else field->deactivate();
      if(bounded&&has_selection)selected->activate();else selected->deactivate();
      hint->label(heat?"Automatic weights from visible bone support.\nKeep paint, morphs and pose.\nDifferent rest rigs need explicit joint mapping.":anatomical?"Complete retained unskinned source group.\nAnatomical surface/contact weights in REST.\nFixed4 influences / prune0.00001; originals retained.":"Bind in the reference rest frame.\nKeep target paint, morphs and pose.\nDifferent rigs need joint mapping.");
      apply->label(anatomical?"Bind anatomy":"Bind / transfer");
    }
  } context{&window,&source,&mode,&apply,&draft.scene,target.id,&options,{},{},
            {&seed,&margin,&distance,&falloff,&cap,&prune},&keep,&locks,&selected,{&work,&workspace,&total},&hint,!expected_selection.empty()};
  context.refresh();
  mode.callback([](Fl_Widget*,void *p){static_cast<Context*>(p)->refresh();},&context);
  cancel.callback([](Fl_Widget *w,void*){w->window()->hide();});
  apply.callback([](Fl_Widget*,void *p){
    auto &c=*static_cast<Context*>(p);
    try {
      if(c.source->value()<0 || size_t(c.source->value())>=c.ids.size())throw std::invalid_argument("Choose an eligible skinned reference mesh");
      auto next=*c.options;next.reference=c.ids[c.source->value()];next.mode=detail::skin_method_mode(c.mode->value());
      next.seed_distance=float(c.fields[0]->value()/1000);next.seed_margin=float(c.fields[1]->value()/1000);
      next.transfer_distance=float(c.fields[2]->value()/1000);next.falloff=float(c.fields[3]->value());
      double influences=c.fields[4]->value();
      if(!std::isfinite(influences)||influences<1||influences>32||std::floor(influences)!=influences)
        throw std::invalid_argument("Max influences must be an integer from1 to32");
      next.limits.max_influences=uint32_t(influences);next.limits.prune=float(c.fields[5]->value());
      if(next.mode==SkinAuthoringMode::Anatomical||next.mode==SkinAuthoringMode::SurfaceHeat){
        auto bounded=[](double v,size_t max,size_t unit){if(!std::isfinite(v)||v<1||v>double(max)||std::floor(v)!=v)throw std::invalid_argument("Enter a whole anatomical resource bound in the displayed range");return size_t(v)*unit;};
        next.anatomical.maximum_work=bounded(c.budgets[0]->value(),500,1'000'000);
        next.anatomical.maximum_workspace=bounded(c.budgets[1]->value(),256,1024*1024);
        next.anatomical.maximum_total=bounded(c.budgets[2]->value(),512,1024*1024);
      }
      next.locks.clear();if(c.keep->value())for(int i=1;i<=c.locks->nitems();++i)next.locks.push_back(c.locks->checked(i));
      next.validate();*c.options=std::move(next);c.accepted=true;c.window->hide();
    }catch(const std::exception &e){fl_alert("%s",e.what());}
  },&context);
  window.end();window.set_modal();dx::style(&window);window.show();cancel.take_focus();
  while(window.shown())Fl::wait();
  if(context.accepted){if((options.mode==SkinAuthoringMode::Anatomical||options.mode==SkinAuthoringMode::SurfaceHeat)&&selected.value())doc.bind_skin_selected(std::move(options),draft.source,expected_selection);else doc.bind_skin(std::move(options),draft.source);}
}
} // namespace dome
