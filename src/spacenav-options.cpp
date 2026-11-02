// SPDX-License-Identifier: GPL-3.0-or-later
#include "spacenav-options.hpp"
#include "appearance.h"
#include "ui-conventions.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Value_Input.H>
#include <FL/fl_ask.H>
#include <chrono>
namespace dome::desktop {
double spacenav_now() noexcept {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void spacenav_options(SpaceNavigator &navigation) {
  navigation.fence(spacenav_now());
  auto previous=navigation.settings();
  int sx,sy,sw,sh;Fl::screen_work_area(sx,sy,sw,sh);
  int row=std::max(28,dx::size+12),width=std::min(std::max(580,dx::size*36),sw-40);
  int height=std::min(row*17,sh-60),label=width/2;
  DomeDialog window(width,height,"3D mouse navigation");
  Fl_Scroll scroll(0,0,width,height-row*2);
  Fl_Group content(0,0,width-24,row*15);content.resizable(nullptr);
  Fl_Box hint(16,row/2,width-52,row*2,
    "Use a 3D mouse through spacenavd. Move and release once after changing focus.\n"
    "Buttons 1 and 2 frame selection and all objects. Camera movement is paused here.");
  hint.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);
  Fl_Box status(16,row*3,width-52,row,navigation.status());
  status.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE);
  Fl_Output socket(label,row*4,width-label-40,row-4,"Daemon socket");socket.value(navigation.transport().path().c_str());
  Fl_Check_Button enabled(16,row*5,width-48,row,"Enable 3D mouse navigation");enabled.value(previous.enabled);
  Fl_Value_Input pan(label,row*6,width-label-40,row-4,"Pan and zoom speed");pan.bounds(.05,5);pan.step(.05);pan.value(previous.translation);
  Fl_Value_Input rotate(label,row*7,width-label-40,row-4,"Rotation (degrees/s)");rotate.bounds(5,360);rotate.step(5);rotate.value(previous.rotation);
  Fl_Value_Input deadzone(label,row*8,width-label-40,row-4,"Deadzone (%)");deadzone.bounds(0,120./350*100);deadzone.step(.5);deadzone.value(previous.deadzone/350*100);
  std::array<Fl_Check_Button*,6> invert{};
  const char *names[]={"Invert pan X","Invert pan Y","Invert dolly","Invert rotation X","Invert rotation Y","Invert rotation Z"};
  for(size_t i=0;i<6;++i){invert[i]=new Fl_Check_Button(16+int(i%2)*(width-40)/2,row*9+int(i/2)*row,(width-40)/2,row,names[i]);invert[i]->value(previous.inverted[i]);}
  Fl_Check_Button swap(16,row*12,width-48,row,"Swap selection / all frame buttons");swap.value(previous.swap_buttons);
  Fl_Button reconnect(16,row*13,width-52,row,"Reconnect to spacenav");
  reconnect.callback([](Fl_Widget*,void *p){static_cast<SpaceNavigator*>(p)->reconnect(spacenav_now());},&navigation);
  Fl_Box scaling(16,row*14,width-52,row,"Pan and zoom speed scales with the current view distance.");scaling.align(FL_ALIGN_CENTER|FL_ALIGN_INSIDE|FL_ALIGN_WRAP);
  content.end();scroll.end();
  Fl_Button cancel(width/2-150,height-row*2+8,140,row,"Cancel"),apply(width/2+10,height-row*2+8,140,row,"Apply");
  bool accepted=false;
  cancel.callback([](Fl_Widget*w,void*){w->window()->hide();});
  apply.callback([](Fl_Widget*w,void*p){*static_cast<bool*>(p)=true;w->window()->hide();},&accepted);
  window.end();window.set_modal();dx::style(&window);cancel.take_focus();window.show();
  while(window.shown()) {
    const auto message=navigation.status();if(status.label()!=message){status.label(message);status.redraw();}
    Fl::wait(.05);
  }
  if(accepted) {
    auto next=previous;next.enabled=enabled.value();next.translation=float(pan.value());next.rotation=float(rotate.value());next.deadzone=float(deadzone.value()*3.5);
    for(size_t i=0;i<6;++i)next.inverted[i]=invert[i]->value();
    next.swap_buttons=swap.value();
    navigation.settings(next,spacenav_now());
  }
  navigation.fence(spacenav_now());
}
} // namespace dome::desktop
