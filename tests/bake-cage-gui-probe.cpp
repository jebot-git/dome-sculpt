// SPDX-License-Identifier: GPL-3.0-or-later
// Test-only read-only observer of actual FLTK widgets after the real wait.
// Input still goes through X events and the client's normal callbacks.
#include <FL/Fl.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Valuator.H>
#include <FL/Fl_Window.H>
#include <dlfcn.h>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <fstream>
#include <cstdio>
namespace {
using Json=nlohmann::json;
Json widget(Fl_Widget *w){
  Json j={{"label",w->label()?w->label():""},{"x",w->x()},{"y",w->y()},{"w",w->w()},{"h",w->h()},
    {"visible",bool(w->visible_r())},{"active",bool(w->active_r())},{"focused",Fl::focus()==w},
    {"label_size",w->labelsize()}};
  if(auto *choice=dynamic_cast<Fl_Choice*>(w)){j["choice"]=choice->value();j["selected"]=choice->mvalue()&&choice->mvalue()->label()?choice->mvalue()->label():"";}
  if(auto *value=dynamic_cast<Fl_Valuator*>(w))j["value"]=value->value();
  if(auto *scroll=dynamic_cast<Fl_Scroll*>(w))j["scroll_y"]=scroll->yposition();
  if(auto *group=dynamic_cast<Fl_Group*>(w)){j["children"]=Json::array();for(int i=0;i<group->children();++i)j["children"].push_back(widget(group->child(i)));}
  return j;
}
void snapshot()noexcept{
  const auto *path=std::getenv("DOME_CAGE_WIDGET_REPORT");if(!path)return;
  try{Json windows=Json::array();for(auto *w=Fl::first_window();w;w=Fl::next_window(w))if(w->shown())windows.push_back(widget(w));
    const auto temporary=std::string(path)+".tmp";{std::ofstream out(temporary);out<<windows.dump();}std::rename(temporary.c_str(),path);
  }catch(...){} // Observation cannot change the client's control flow.
}
}
double Fl::wait(double seconds){
  static const auto real=reinterpret_cast<double(*)(double)>(dlsym(RTLD_NEXT,"_ZN2Fl4waitEd"));
  const auto result=real(seconds);snapshot();return result;
}
