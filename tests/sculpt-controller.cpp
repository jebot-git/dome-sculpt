// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "sculpt-ui-values.hpp"
#include <iostream>
using namespace dome;
struct Driver {
  Document &doc;
  SpatialFrame frame;
  Driver(Document &d) : doc(d) {
    frame.sequence = 1;
    frame.time = 1;
    frame.focused = frame.head_valid = true;
    frame.head.position = {0, 1.6f, 0};
    frame.hands[1].tracked.valid = true;
    frame.hands[1].tracked.grip.position = {0, 1.48f, -.5f};
    send();
  }
  void send(float x = 0, float y = 0, bool click = false, float pressure = 0) {
    ++frame.sequence;
    frame.time += .05;
    auto &h = frame.hands[1];
    h.x = x;
    h.y = y;
    h.joystick = click;
    h.tracked.trigger = pressure;
    doc.submit(frame);
    if (!doc.wait_input())
      throw std::runtime_error("Controller worker timed out");
  }
  void open() {
    send();
    send(0, 0, true);
    send();
  }
  void sector(int i) {
    float a = i * 3.14159265358979323846f / 6;
    send(std::cos(a), std::sin(a));
    send(std::cos(a), std::sin(a), true);
    send();
  }
  void panel_button(bool accept) {
    auto s = doc.frame().state;
    auto &h = frame.hands[1];
    h.tracked.aimValid = true;
    float u = (accept ? 470.f : 170.f) / 640, v = 609.f / 640;
    h.tracked.aim.position =
        ob::transform(s->panel, {(u - .5f) * .6f, (.5f - v) * .6f, .25f});
    h.tracked.aim.orientation = s->panel.orientation;
    send(0, 0, false, .8f);
    send();
  }
  void confirm() {
    send(0, 0, true);
    send();
  }
  void ray(float t, int row = 0, bool stylus = false) {
    auto s = doc.frame().state;
    int n = s->controls.components;
    float y = n == 1 ? 300 : n == 4 ? 210 + 70 * row : 240 + 80 * row;
    float u = (100 + 420 * t) / 640, v = y / 640;
    auto &h = frame.hands[stylus ? 0 : 1];
    if (stylus) {
      h.tracked.valid = true;
      h.kind = DeviceKind::SpatialStylus;
      send();
    }
    h.tracked.aimValid = true;
    h.tracked.aim.position =
        ob::transform(s->panel, {(u - .5f) * .6f, (.5f - v) * .6f, .25f});
    h.tracked.aim.orientation = s->panel.orientation;
    if (stylus) {
      h.tracked.trigger = .8f;
      send();
      h.tracked.trigger = 0;
      send();
    } else {
      send(0, 0, false, .8f);
      send();
    }
  }
};
int main() try {
  auto require=[](bool value,const char *message) { if(!value)throw std::runtime_error(message); };
  Document doc;doc.seed();doc.replace(doc.copy_volume());
  Driver hand(doc);
  auto masks=[&] { hand.open();hand.sector(8);hand.sector(11);hand.sector(11);require(doc.frame().state->menu_page==15,"Sculpt layers leads to mask page15"); };
  masks();hand.sector(6);
  require(doc.frame().state->menu && doc.frame().state->menu_page==15 && doc.frame().state->sculpt_settings.symmetry_axes==1,"mirror X keeps mask menu open");
  auto before=doc.frame().state->sculpt_settings;
  hand.sector(2);require(doc.frame().state->controls.parameter==VrParameter::SculptMaskStrength,"continuous mask strength enters panel");
  hand.ray(.2f);hand.panel_button(false);
  require(doc.frame().state->controls.parameter==VrParameter::None && doc.frame().state->sculpt_settings.mask_strength==before.mask_strength,"Cancel leaves continuous controls exact");
  hand.sector(10);require(doc.frame().state->controls.components==4,"planes and visibility use four large controls");
  hand.ray(.6f,0);hand.ray(0,3);hand.panel_button(true);
  auto settings=doc.frame().state->sculpt_settings;
  require(std::abs(settings.symmetry_origin.x-sculpt_plane_metres(.6f))<1e-5f && !settings.mask_visible,"ray planes and view publish accepted settings");
  require(doc.undo(),"undo all plane/view controls together");
  require(doc.frame().state->sculpt_settings.symmetry_origin.x==0 && doc.frame().state->sculpt_settings.mask_visible,"plane/view undo is exact");
  masks();hand.sector(1);hand.ray(1);hand.panel_button(true);
  require(doc.frame().state->sculpt_settings.mask_tool==Volume::MaskTool::Smooth,"native XR mode selector chooses mask Smooth");
  hand.sector(0);
  require(!doc.frame().state->menu && doc.frame().state->sculpt_settings.mask_edit,"Paint mask closes menu for actual pressure input");
  // Use an Add brush for visible protection and retain a committed prefix.
  settings=doc.frame().state->sculpt_settings;settings.mask_tool=Volume::MaskTool::Add;doc.sculpt_settings(settings);
  hand.frame.hands[1].tracked.aimValid=false;
  hand.frame.hands[1].tracked.grip.position=ob::transform(doc.frame().state->canvas,{0,0,.22f});
  hand.send();hand.send(0,0,false,.8f);hand.send();
  auto committed=doc.copy_volume().mask_values();require(!committed.empty(),"tracked mask pressure commits protection");
  hand.frame.hands[1].tracked.grip.position=ob::transform(doc.frame().state->canvas,{.13f,0,.18f});
  hand.send(0,0,false,.8f);hand.frame.focused=false;hand.send();
  require(doc.copy_volume().mask_values()==committed && !doc.frame().state->stroking,"focus loss rolls back held protection stroke");
  hand.frame.focused=true;hand.send();
  masks();hand.sector(0);
  require(!doc.frame().state->menu && doc.frame().state->sculpt_settings.mask_edit && doc.copy_volume().mask_values()==committed,"Resume mask preserves current mode and protection");
  hand.open();hand.sector(0);
  require(!doc.frame().state->menu && !doc.frame().state->sculpt_settings.mask_edit && doc.copy_volume().mask_values()==committed,"explicit geometry tool exits mask painting retaining protection");
  masks();hand.sector(8);require(doc.frame().state->menu_page==7,"mask Back preserves sculpt layers route");
  hand.sector(10);require(doc.frame().state->menu_page==8,"Recovery remains on original sculpt menu cell10");
  require(spatial_parameter(16,9)==VrParameter::RetopoDistance && spatial_parameter(14,0)==VrParameter::SkinSource,"Retopo16 and Skin14 parameter routes retained");
  std::cout<<"PASS XR page15, continuous controls, resume, pressure, focus rollback, geometry exit and existing routes\n";
  return 0;
} catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
