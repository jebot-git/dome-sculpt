// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/document.hpp"
struct ImportDriver {
  dome::Document &doc;dome::SpatialFrame frame;
  explicit ImportDriver(dome::Document &d):doc(d){
    frame.sequence=1;frame.time=1;frame.focused=frame.head_valid=true;frame.head.position={0,1.6f,0};
    frame.hands[1].tracked.valid=true;frame.hands[1].tracked.grip.position={0,1.48f,-.5f};send();
  }
  void send(float x=0,float y=0,bool click=false,float pressure=0){
    ++frame.sequence;frame.time+=.05;auto &h=frame.hands[1];h.x=x;h.y=y;h.joystick=click;h.tracked.trigger=pressure;
    doc.submit(frame);if(!doc.wait_input())throw std::runtime_error("Import controller worker timed out");
  }
  void open(){send();send(0,0,true);send();}
  void sector(int i){float a=i*3.14159265358979323846f/6;send(std::cos(a),std::sin(a));send(std::cos(a),std::sin(a),true);send();}
  void panel_button(bool accept){auto s=doc.frame().state;auto &h=frame.hands[1];h.tracked.aimValid=true;
    float u=(accept?470.f:170.f)/640,v=609.f/640;h.tracked.aim.position=ob::transform(s->panel,{(u-.5f)*.6f,(.5f-v)*.6f,.25f});h.tracked.aim.orientation=s->panel.orientation;
    send(0,0,false,.8f);send();h.tracked.aimValid=false;
  }
  void ray(float t){auto s=doc.frame().state;auto &h=frame.hands[1];h.tracked.aimValid=true;
    float u=(100+420*t)/640,v=300.f/640;h.tracked.aim.position=ob::transform(s->panel,{(u-.5f)*.6f,(.5f-v)*.6f,.25f});h.tracked.aim.orientation=s->panel.orientation;
    send(0,0,false,.8f);send();h.tracked.aimValid=false;
  }
};
