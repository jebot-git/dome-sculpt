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
 auto require=[](bool value,const char *message){if(!value)throw std::runtime_error(message);std::cout<<"PASS "<<message<<'\n';};
 Document doc;Volume v(.04f);v.begin_stroke();Brush b;b.radius=.237f;b.from=b.to={.013f,.017f,-.011f};b.blend=0;v.stamp(b);v.end_stroke();doc.replace(v.geometry_snapshot());require(doc.wait_mesh(),"Initial source surface ready");
 Driver hand(doc);auto open=[&]{hand.open();hand.sector(8);hand.sector(11);hand.sector(7);require(doc.frame().state->controls.parameter==VrParameter::SculptResolution,"Page7 cell7 opens large logarithmic resolution panel");};
 auto original=doc.copy_volume().voxels();auto revision=doc.frame().state->revision;
 open();require(doc.frame().state->controls.logarithmic && std::abs(doc.frame().state->controls.value[0]-40)<1e-5f,"Current resolution shown in world millimetres");hand.ray(.66f);hand.panel_button(false);
 require(doc.copy_volume().voxels()==original && doc.frame().state->revision==revision && !doc.frame().state->can_undo,"XR Cancel discards staged resolution without model/history changes");
 doc.cancel_spatial_menu();open();hand.panel_button(true);require(doc.wait_asset(),"Same-voxel confirm ready");require(doc.frame().state->revision==revision && !doc.frame().state->can_undo,"Unmoved XR confirm is exact no-op despite world-unit conversion");
 doc.cancel_spatial_menu();open();hand.ray(float(std::log(.02/.00001)/std::log(100000.)));require(doc.copy_volume().voxels()==original && doc.frame().state->revision==revision,"Moving slider does not mutate or mesh source");hand.panel_button(true);
 require(doc.wait_asset() && doc.wait_mesh(),"Confirmed resolution worker and immutable surface ready");require(doc.frame().state->error.empty() && std::abs(doc.copy_volume().voxel_size()-.02f)<1e-7f,"XR pressure confirmation publishes validated finer field");require(doc.undo() && doc.copy_volume().voxels()==original && !doc.frame().state->can_undo,"XR resolution is exactly one undo transaction");
 doc.cancel_spatial_menu();open();hand.ray(.64f);hand.frame.focused=false;hand.send();require(doc.frame().state->controls.parameter==VrParameter::None && doc.copy_volume().voxels()==original && !doc.frame().state->can_undo,"Focus loss discards unconfirmed resolution");
 require(spatial_action(0,4)==SpatialCommand::Undo && spatial_parameter(7,7)==VrParameter::SculptResolution && spatial_parameter(16,9)==VrParameter::RetopoDistance && spatial_parameter(14,0)==VrParameter::SkinSource,"Root Undo, Retopo16, Skin14 remain available");
 std::cout<<"PASS native XR resolution navigation, cancel, no-op, staged pressure, one undo and focus rollback\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
