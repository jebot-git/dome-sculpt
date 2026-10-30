// SPDX-License-Identifier: GPL-3.0-or-later
#include "bake-editor.hpp"
#include "appearance.h"
#include "ui-conventions.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Progress.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/fl_draw.H>
namespace dome {
namespace {
class BakeSlider:public Fl_Value_Slider {
public:
  using Fl_Value_Slider::Fl_Value_Slider;
  bool logarithmic=false;
  double physical()const{return logarithmic?std::exp(std::log(.01)+value()*std::log(1000000.)):value();}
  void physical(double v){value(logarithmic?std::log(std::clamp(v,.01,10000.)/.01)/std::log(1000000.):v);}
  int format(char *buffer)override{return snprintf(buffer,32,logarithmic?"%.2f":"%.0f",physical());}
  int handle(int event)override{
    if(event==FL_MOUSEWHEEL){
      int d=Fl::event_dy()?Fl::event_dy():Fl::event_dx();
      double old=value();
      physical(logarithmic?physical()*std::exp(-d*.025):clamp(value()-d));
      if(value()!=old){set_changed();do_callback();}return 1;
    }
    return Fl_Value_Slider::handle(event);
  }
};
}
void edit_bake_dialog(Document &doc){
  auto draft=doc.draft_scene();auto state=doc.frame().state;
  if(!state->can_bake)throw std::logic_error("Prepare a visible, unlocked target and a different source before baking");
  auto expected=draft.source;auto target=expected.object;
  int row=std::max(36,dx::size+16),sx,sy,sw,sh;Fl::screen_work_area(sx,sy,sw,sh);
  int width=std::min(std::max(640,dx::size*42),sw-40),height=std::min(row*17,sh-40);
  DomeDialog window(width,height,"Bake surface");
  Fl_Scroll scroll(0,0,width,height-row*2);scroll.type(Fl_Scroll::VERTICAL);
  Fl_Group content(0,0,width-24,row*13);content.resizable(nullptr);
  const char *description="Bake into the selected target's UVs below editable paint.\nSource geometry, rig and paint stay intact. Bounds use world mm.";
  fl_font(dx::font,dx::size);
  auto measured=[&](const char*text,int minimum){int w=width-48,h=0;fl_measure(text,w,h,0);return std::max(minimum,h+16);};
  int header=measured(description,row*2);
  Fl_Box hint(16,4,width-48,header-4,description);
  hint.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);
  int label=std::min(width/2,dx::size*15),y=header;
  Fl_Choice source(label,y,width-label-40,row-6,"Source");y+=row;
  std::vector<std::string> names;std::vector<Fl_Menu_Item> choices;std::vector<uint64_t> ids=state->bake_sources;
  size_t selected=0;
  for(size_t i=0;i<ids.size();++i){auto &o=draft.scene.object(ids[i]);names.push_back(o.name+(o.visible?"":" [hidden]"));if(ids[i]==state->bake_source)selected=i;}
  for(auto &name:names){Fl_Menu_Item item{};item.label(name.c_str());choices.push_back(item);}choices.emplace_back();source.copy(choices.data());source.value(int(selected));
  BakeSlider front(label,y,width-label-40,row-6,"Front (mm)");y+=row;
  BakeSlider back(label,y,width-label-40,row-6,"Back (mm)");y+=row;
  BakeSlider ao(label,y,width-label-40,row-6,"AO radius (mm)");y+=row;
  BakeSlider samples(label,y,width-label-40,row-6,"AO samples");y+=row;
  BakeSlider padding(label,y,width-label-40,row-6,"Padding (pixels)");y+=row;
  auto settings=state->bake_settings;
  std::array<BakeSlider*,5> sliders{&front,&back,&ao,&samples,&padding};
  std::array<double,5> initial{settings.front_mm,settings.back_mm,settings.ao_distance_mm,double(settings.ao_samples),double(settings.padding)};
  for(size_t i=0;i<5;++i){auto &v=*sliders[i];v.type(FL_HOR_NICE_SLIDER);v.align(FL_ALIGN_LEFT);v.textfont(dx::font);v.textsize(dx::size);v.value_width(std::max(60,dx::size*6));v.logarithmic=i<3;v.bounds(0,i<3?1:i==3?256:64);v.step(i<3?.001:1);v.physical(initial[i]);}
  Fl_Check_Button partial(20,y,width-56,row,"Allow partial projection");partial.value(settings.allow_partial);y+=row;
  Fl_Check_Button backfaces(20,y,width-56,row,"Allow backface projection");backfaces.value(settings.allow_backfaces);y+=row;
  const char *help="Partial projection keeps unprojected areas and reports misses.\nDrag or wheel to adjust; 0 samples disables AO. Bake is one Undo step.";
  int help_height=measured(help,row);
  Fl_Box adjustment(16,y,width-48,help_height,help);y+=help_height;
  adjustment.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);
  Fl_Progress progress(20,y,width-60,row-8);progress.selection_color(FL_SELECTION_COLOR);progress.minimum(0);progress.maximum(100);progress.value(0);y+=row;
  Fl_Box status(16,y,width-48,row*2);status.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);y+=row*2;
  content.size(width-24,y);content.end();scroll.end();
  Fl_Button close(24,height-row*2+8,width/2-36,row,"Cancel"),bake(width/2+12,height-row*2+8,width/2-36,row,"Bake");
  struct Run {Document *doc;Fl_Window *window;Fl_Choice *source;std::array<BakeSlider*,5> fields;std::vector<uint64_t>*ids;AssetSource *expected;Fl_Check_Button *partial,*backfaces;bool running=false,ever_ran=false;} run{&doc,&window,&source,sliders,&ids,&expected,&partial,&backfaces};
  close.callback([](Fl_Widget*w,void*p){auto &r=*static_cast<Run*>(p);if(r.running && r.doc->frame().state->asset_busy)r.doc->cancel_asset_work();else w->window()->hide();},&run);
  bake.callback([](Fl_Widget*,void*p){auto &r=*static_cast<Run*>(p);try{
    ObjectBakeSettings s;s.front_mm=float(r.fields[0]->physical());s.back_mm=float(r.fields[1]->physical());s.ao_distance_mm=float(r.fields[2]->physical());s.ao_samples=uint32_t(std::lround(r.fields[3]->value()));s.padding=uint32_t(std::lround(r.fields[4]->value()));
    s.allow_partial=r.partial->value();s.allow_backfaces=r.backfaces->value();
    r.doc->bake_from_object(r.ids->at(size_t(r.source->value())),s,*r.expected);r.running=r.ever_ran=true;
  }catch(const std::exception&e){r.doc->report_error(e.what());}},&run);
  window.end();window.set_modal();dx::style(&window);window.show();close.take_focus();
  while(window.shown()){
    auto s=doc.frame().state;auto work=doc.asset_status();
    for(auto field:sliders)if(field->textfont()!=dx::font || field->textsize()!=dx::size){field->textfont(dx::font);field->textsize(dx::size);field->value_width(std::max(60,dx::size*6));field->redraw();}
    if(run.running && !s->asset_busy){
      run.running=false;
      if(s->active_object==target && s->generation==expected.generation)expected=doc.draft_scene().source;
    }
    if(s->active_object!=target || s->generation!=expected.generation){window.hide();break;}
    if(run.running){partial.deactivate();backfaces.deactivate();source.deactivate();for(auto f:sliders)f->deactivate();bake.deactivate();if(work.cancellable)close.activate();else close.deactivate();close.copy_label(work.cancelling?"Cancelling...":"Cancel bake");progress.value(std::min(99.f,work.progress*100));}
    else {partial.activate();backfaces.activate();source.activate();for(auto f:sliders)f->activate();bake.activate();close.activate();close.copy_label(run.ever_ran?"Close":"Cancel");if(run.ever_ran && s->error.empty() && s->message.find("complete")!=std::string::npos)progress.value(100);}
    std::string title="Bake surface";
    if(run.running)title+=work.cancelling?" - Cancelling...":" - Baking "+std::to_string(std::min(99,int(work.progress*100)))+"%";
    else if(run.ever_ran)title+=!s->error.empty()?" - failed":s->message.find("cancelled")!=std::string::npos?" - cancelled":" - complete";
    if(title!=window.label())window.copy_label(title.c_str());
    status.copy_label((!run.ever_ran&&!run.running?std::string("Choose a source, projection bounds and policy."):s->error.empty()?s->message:s->error).c_str());Fl::wait(.05);
  }
  if(run.running)doc.cancel_asset_work();
}
} // namespace dome
