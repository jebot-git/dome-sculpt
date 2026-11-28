// SPDX-License-Identifier: GPL-3.0-or-later
#include "bake-editor.hpp"
#include "bake-source-choice.hpp"
#include "dome/texture-sets.hpp"
#include "dome/bake-recipe.hpp"
#include "dome/material-bindings.hpp"
#include "dome/bake-distance.hpp"
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
  bool logarithmic=false,fractional=false;
  double units=1;
  double physical()const{return logarithmic?bake_distance_from_unit(value()):value()*units;}
  void physical(double v){value(logarithmic?bake_distance_to_unit(v):v/units);}
  int format(char *buffer)override{return snprintf(buffer,32,fractional?"%.3f":logarithmic?(physical()>0&&physical()<.01?"%.3g":"%.2f"):"%.0f",physical());}
  int handle(int event)override{
    if(event==FL_MOUSEWHEEL){
      // Nested groups can offer wheel events to an offscreen child. Leave
      // those to the containing scroll view instead of changing hidden values.
      if(!Fl::event_inside(this))return 0;
      for(auto *p=parent();p;p=p->parent())
        if(dynamic_cast<Fl_Scroll*>(p)&&!Fl::event_inside(p))return 0;
      int d=Fl::event_dy()?Fl::event_dy():Fl::event_dx();
      double old=value();
      physical(logarithmic?bake_distance_step(physical(),-d*.025):clamp(value()-d*step())*units);
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
  int width=std::min(std::max(640,dx::size*42),sw-40),height=std::min(row*21,sh-40);
  DomeDialog window(width,height,"Bake surface");
  Fl_Scroll scroll(0,0,width,height-row*2);scroll.type(Fl_Scroll::VERTICAL);
  Fl_Group content(0,0,width-24,row*17);content.resizable(nullptr);
  const auto &object=draft.scene.object(target);
  const bool texture_set=bool(object.texture_sets);
  const bool retained_native=object.material_bindings&&object.material_bindings->native_paint_sampling;
  const bool existing_basis=texture_set||retained_native;
  const std::string description=texture_set
      ?"Bake beneath editable paint in "+active_texture_set(object).name+". Bounds use world mm."
      :retained_native?"Bake into the retained paint atlas below the editable layers. Bounds use world mm."
      :"Bake into the selected target's UVs below editable paint.\nSource geometry, rig and paint stay intact. Bounds use world mm.";
  fl_font(dx::font,dx::size);
  auto measured=[&](const char*text,int minimum){fl_font(dx::font,dx::size);int w=width-48,h=0;fl_measure(text,w,h,0);return std::max(minimum,h+16);};
  int header=measured(description.c_str(),row*2);
  Fl_Box hint(16,4,width-48,header-4,description.c_str());
  hint.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);
  int label=std::min(width/2,dx::size*15),y=header;
  Fl_Choice source(label,y,width-label-40,row-6,"Source");y+=row;
  source.tooltip("Select donors in the object hierarchy and make the target active last to use Selected objects.");
  std::vector<std::string> names;std::vector<Fl_Menu_Item> choices;
  const auto source_count=detail::bake_source_choice_count(*state);
  const auto current=detail::bake_source_choice_index(*state,detail::current_bake_donors(*state));
  for(size_t i=0;i<source_count;++i)names.push_back(detail::bake_source_choice_label(*state,i));
  const auto selected=current==source_count?0:current;
  for(auto &name:names){Fl_Menu_Item item{};item.label(name.c_str());choices.push_back(item);}choices.emplace_back();source.copy(choices.data());source.value(int(selected));
  Fl_Choice cage(label,y,width-label-40,row-6,"Cage mesh");y+=row;
  std::vector<uint64_t> cage_ids=state->bake_cages;
  std::vector<std::string> cage_names;std::vector<Fl_Menu_Item> cage_choices;size_t selected_cage=0;
  for(size_t i=0;i<cage_ids.size();++i){
    const auto id=cage_ids[i];
    auto found=std::find_if(draft.scene.objects().begin(),draft.scene.objects().end(),[id](const auto &o){return o.id==id;});
    cage_names.push_back(!id?"Normal offset (automatic)":found==draft.scene.objects().end()?"Unavailable cage - reselect":found->name+(found->visible?"":" [hidden]"));
    if(id==state->bake_settings.cage_object)selected_cage=i;
  }
  for(auto &name:cage_names){Fl_Menu_Item item{};item.label(name.c_str());cage_choices.push_back(item);}cage_choices.emplace_back();cage.copy(cage_choices.data());cage.value(int(selected_cage));
  Fl_Button recall(label,y,width-label-40,row-6,"Use last bake");y+=row;
  Fl_Choice mask(label,y,width-label-40,row-6,"Target mask");y+=row;
  const auto &mask_ids=state->bake_mask_layers;
  std::vector<std::string> mask_names{"Whole field"};
  mask_names.insert(mask_names.end(),state->paint_layers.begin(),state->paint_layers.end());
  mask_names.resize(mask_ids.size(),"Unavailable mask layer");
  std::vector<Fl_Menu_Item> mask_choices;
  for(auto &name:mask_names){
    Fl_Menu_Item item{};item.label(name.c_str());mask_choices.push_back(item);
  }
  mask_choices.emplace_back();mask.copy(mask_choices.data());
  const auto mask_index=std::ranges::find(mask_ids,state->bake_settings.mask_layer);
  mask.value(mask_index==mask_ids.end()?0:int(mask_index-mask_ids.begin()));
  mask.tooltip("For a selected paint layer, mask values below 50% protect texels from projection and padding.");
  BakeSlider front(label,y,width-label-40,row-6,"Front (mm)");y+=row;
  BakeSlider back(label,y,width-label-40,row-6,"Back (mm)");y+=row;
  BakeSlider ao(label,y,width-label-40,row-6,"AO radius (mm)");y+=row;
  BakeSlider samples(label,y,width-label-40,row-6,"AO samples");y+=row;
  BakeSlider padding(label,y,width-label-40,row-6,"Padding (pixels)");y+=row;
  auto settings=state->bake_settings;
  BakeSlider color_error(label,0,width-label-40,row-6,"Color error (0..1)");
  constexpr double degrees=57.29577951308232;
  BakeSlider normal_error(label,0,width-label-40,row-6,"Rest error (deg)");normal_error.units=.25*degrees;normal_error.tooltip("Maximum rest-space normal quantization error, in degrees. Pose and morph evaluation use the authored portable targets.");
  std::array<BakeSlider*,7> sliders{&front,&back,&ao,&samples,&padding,&color_error,&normal_error};
  std::array<double,7> initial{settings.front_mm,settings.back_mm,settings.ao_distance_mm,double(settings.ao_samples),double(settings.padding),settings.color_error,settings.normal_error*degrees};
  for(size_t i=0;i<7;++i){auto &v=*sliders[i];v.type(FL_HOR_NICE_SLIDER);v.align(FL_ALIGN_LEFT);v.textfont(dx::font);v.textsize(dx::size);v.value_width(std::max(60,dx::size*6));v.logarithmic=i<3;v.fractional=i>=5;v.bounds(0,i<3||i>=5?1:i==3?256:64);v.step(i<3||i>=5?.001:1);v.physical(initial[i]);}
  Fl_Check_Button partial(20,y,width-56,row,"Allow partial projection");partial.value(settings.allow_partial);y+=row;
  Fl_Check_Button backfaces(20,y,width-56,row,"Allow backface projection");backfaces.value(settings.allow_backfaces);y+=row;
  Fl_Check_Button corresponding(20,y,width-56,row,"Match source triangles");
  corresponding.value(settings.corresponding_topology);y+=row;
  corresponding.tooltip("Use the same ordered vertices and triangles within the chosen projection bounds.");
  Fl_Check_Button sampling(20,y,width-56,row,"Four samples per texel");
  sampling.value(settings.supersample);y+=row;
  sampling.tooltip("Sample UV coverage and projection at four positions per texel. This increases bake time and memory use.");
  Fl_Check_Button colors(20,y,width-56,row,"Prepare source colors");colors.value(settings.prepare_colors);y+=row;
  color_error.position(label,y);y+=row;
  Fl_Check_Button normals(20,y,width-56,row,"Author portable normal targets");normals.value(settings.prepare_normals);normals.tooltip("Authors portable normal and tangent targets. Pose and morph shading can differ from legacy dynamic shading; original source data is retained.");y+=row;
  normal_error.position(label,y);y+=row;
  if(existing_basis){colors.deactivate();normals.deactivate();color_error.deactivate();normal_error.deactivate();}
  std::string help="Partial projection keeps unprojected areas and reports misses.\n";
  if(!existing_basis)help+="Portable normal authoring creates a new shading model while retaining the original data.\n";
  help+="For a cage, duplicate the target and edit its vertices while keeping topology. 0 samples disables AO.\nDrag or wheel to adjust. Bake is one Undo step.";
  int help_height=measured(help.c_str(),row);
  Fl_Box adjustment(16,y,width-48,help_height,help.c_str());y+=help_height;
  adjustment.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);
  Fl_Progress progress(20,y,width-60,row-8);progress.selection_color(FL_SELECTION_COLOR);progress.minimum(0);progress.maximum(100);progress.value(0);y+=row;
  Fl_Box status(16,y,width-48,row*2);status.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);y+=row*2;
  content.size(width-24,y);content.end();scroll.end();
  Fl_Button close(24,height-row*2+8,width/2-36,row,"Cancel"),bake(width/2+12,height-row*2+8,width/2-36,row,"Bake");
  struct Run {Document *doc;Fl_Window *window;Fl_Choice *source,*cage,*mask;std::array<BakeSlider*,7> fields;const EditorState *state;const std::vector<uint64_t>*cage_ids,*mask_ids;AssetSource *expected;Fl_Check_Button *partial,*backfaces,*corresponding,*sampling,*colors,*normals;std::shared_ptr<const ObjectBakeRecipe> recipe;bool running=false,ever_ran=false;} run{&doc,&window,&source,&cage,&mask,sliders,state.get(),&cage_ids,&mask_ids,&expected,&partial,&backfaces,&corresponding,&sampling,&colors,&normals,state->bake_recipe};
  recall.callback([](Fl_Widget*,void *p){auto &r=*static_cast<Run*>(p);try{
    if(!r.recipe)throw std::logic_error("No successful bake has been saved for this target");
    r.recipe->validate();const auto &v=r.recipe->settings;
    const auto source=detail::bake_source_choice_index(*r.state,detail::recipe_bake_donors(*r.recipe));
    auto cage=std::find(r.cage_ids->begin(),r.cage_ids->end(),v.cage_object);
    auto mask=std::ranges::find(*r.mask_ids,v.mask_layer);
    if(source==detail::bake_source_choice_count(*r.state)||cage==r.cage_ids->end()||mask==r.mask_ids->end())
      throw std::logic_error("Saved bake dependencies changed; reopen the options");
    r.source->value(int(source));r.cage->value(int(cage-r.cage_ids->begin()));
    r.mask->value(int(mask-r.mask_ids->begin()));
    const double values[]{v.front_mm,v.back_mm,v.ao_distance_mm,double(v.ao_samples),double(v.padding),v.color_error,v.normal_error*57.29577951308232};
    for(size_t i=0;i<r.fields.size();++i){r.fields[i]->physical(values[i]);r.fields[i]->redraw();}
    r.partial->value(v.allow_partial);r.backfaces->value(v.allow_backfaces);
    r.corresponding->value(v.corresponding_topology);
    r.sampling->value(v.supersample);
    r.colors->value(r.colors->active()&&v.prepare_colors);r.normals->value(r.normals->active()&&v.prepare_normals);r.window->redraw();
  }catch(const std::exception &e){r.doc->report_error(e.what());}},&run);
  close.callback([](Fl_Widget*w,void*p){auto &r=*static_cast<Run*>(p);if(r.running && r.doc->frame().state->asset_busy)r.doc->cancel_asset_work();else w->window()->hide();},&run);
  bake.callback([](Fl_Widget*,void*p){auto &r=*static_cast<Run*>(p);try{
    ObjectBakeSettings s;s.front_mm=float(r.fields[0]->physical());s.back_mm=float(r.fields[1]->physical());s.ao_distance_mm=float(r.fields[2]->physical());s.ao_samples=uint32_t(std::lround(r.fields[3]->value()));s.padding=uint32_t(std::lround(r.fields[4]->value()));
    s.prepare_colors=r.colors->value();s.color_error=float(r.fields[5]->value());
    s.prepare_normals=r.normals->value();s.normal_error=float(r.fields[6]->physical()/57.29577951308232);
    s.allow_partial=r.partial->value();s.allow_backfaces=r.backfaces->value();
    s.corresponding_topology=r.corresponding->value();
    s.supersample=r.sampling->value();
    s.mask_layer=r.mask_ids->at(size_t(r.mask->value()));
    if(s.mask_layer&&(s.prepare_colors||s.prepare_normals))
      throw std::invalid_argument("Choose Whole field for color or normal preparation, or turn off preparation for masked Bake");
    if(!s.corresponding_topology)s.cage_object=r.cage_ids->at(size_t(r.cage->value()));
    r.doc->bake_from_objects(detail::bake_source_choice(*r.state,size_t(r.source->value())),s,*r.expected);
    r.running=r.ever_ran=true;
  }catch(const std::exception&e){r.doc->report_error(e.what());}},&run);
  window.end();window.set_modal();dx::style(&window);window.show();close.take_focus();
  while(window.shown()){
    auto s=doc.frame().state;auto work=doc.asset_status();run.recipe=s->bake_recipe;
    if(run.recipe&&!run.running)recall.activate();else recall.deactivate();
    for(auto field:sliders)if(field->textfont()!=dx::font || field->textsize()!=dx::size){field->textfont(dx::font);field->textsize(dx::size);field->value_width(std::max(60,dx::size*6));field->redraw();}
    if(run.running && !s->asset_busy){
      run.running=false;
      if(s->active_object==target && s->generation==expected.generation)expected=doc.draft_scene().source;
    }
    if(s->active_object!=target || s->generation!=expected.generation){window.hide();break;}
    if(run.running){normals.deactivate();colors.deactivate();partial.deactivate();backfaces.deactivate();corresponding.deactivate();sampling.deactivate();source.deactivate();cage.deactivate();mask.deactivate();for(auto f:sliders)f->deactivate();bake.deactivate();if(work.cancellable)close.activate();else close.deactivate();close.copy_label(work.cancelling?"Cancelling...":"Cancel bake");progress.value(std::min(99.f,work.progress*100));}
    else {partial.activate();backfaces.activate();sampling.activate();source.activate();mask.activate();
      const auto donors=detail::bake_source_choice(*state,size_t(source.value()));
      if(donors.size()>1){corresponding.value(0);corresponding.deactivate();}else corresponding.activate();
      if(corresponding.value())cage.deactivate();else cage.activate();bake.activate();
      for(size_t i=0;i<(existing_basis?5:sliders.size());++i)sliders[i]->activate();
      if(!existing_basis){colors.activate();normals.activate();}
      if(donors.empty()||detail::missing_bake_donor(donors)||
         (!corresponding.value()&&cage_ids.at(size_t(cage.value()))==unavailable_bake_object)||
         size_t(mask.value())>state->paint_layers.size())bake.deactivate();
      close.activate();close.copy_label(run.ever_ran?"Close":"Cancel");if(run.ever_ran && s->error.empty() && s->message.find("complete")!=std::string::npos)progress.value(100);}
    std::string title="Bake surface";
    if(run.running)title+=work.cancelling?" - Cancelling...":" - Baking "+std::to_string(std::min(99,int(work.progress*100)))+"%";
    else if(run.ever_ran)title+=!s->error.empty()?" - failed":s->message.find("cancelled")!=std::string::npos?" - cancelled":" - complete";
    if(title!=window.label())window.copy_label(title.c_str());
    auto status_text=!run.ever_ran&&!run.running?std::string("Choose a source, projection bounds and policy."):s->error.empty()?s->message:s->error;
    int status_height=measured(status_text.c_str(),row*2);
    status.copy_label(status_text.c_str());
    if(status.h()!=status_height){status.size(width-48,status_height);content.size(width-24,status.y()+status_height-content.y());scroll.redraw();}
    Fl::wait(.05);
  }
  if(run.running)doc.cancel_asset_work();
}
} // namespace dome
