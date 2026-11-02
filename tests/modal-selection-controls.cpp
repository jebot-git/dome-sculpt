// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_vr_controls_suite
#include "vr-controls.cpp"
#undef main
#include <nlohmann/json.hpp>
namespace {
unsigned checks=0;
void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
void ready(Document &doc){if(!doc.wait_mesh(std::chrono::seconds(30))||!doc.frame().state->error.empty())throw std::runtime_error("Modal selection: "+doc.frame().state->error);}
void held_panel(Driver &hand,float x,float y){auto state=hand.doc.frame().state;auto &tracked=hand.frame.hands[1].tracked;tracked.aimValid=true;tracked.aim.position=ob::transform(state->panel,{(x/640-.5f)*.6f,(.5f-y/640)*.6f,.25f});tracked.aim.orientation=state->panel.orientation;hand.send(0,0,false,.8f);}
}
int main()try{
 for(int reason=0;reason<5;++reason){
  Document doc;doc.seed();ready(doc);doc.configure_selection(true);ready(doc);const auto before=doc.copy_scene().encode();Driver hand(doc);hand.open();hand.sector(10);
  check(doc.frame().state->controls.parameter==VrParameter::BrushRadius&&doc.frame().state->selection_mode,"Selection remains active while the actual owned modal radius panel opens");
  const auto value=doc.frame().state->controls.value[0];hand.ray(.8f);
  check(doc.frame().state->controls.value[0]!=value,"Selection-on modal ray pressure changes its owned control instead of being suppressed");
  check(doc.copy_scene().encode()==before,"Modal trigger pressure cannot mutate scene sculpt/paint/shape/weight authority");
  if(reason==0||reason==4)held_panel(hand,reason==0?170:470,609);
  else{held_panel(hand,400,300);if(reason==1)doc.cancel_spatial_menu();else if(reason==2){hand.frame.focused=false;hand.send(0,0,false,.8f);}else{hand.frame.hands[1].tracked.valid=false;hand.send(0,0,false,.8f);}}
  check(doc.frame().state->controls.parameter==VrParameter::None,"Cancel, close, focus loss, tracking loss or Confirm retires the actual control owner");
  hand.frame.focused=true;hand.frame.hands[1].tracked.valid=true;hand.frame.hands[1].tracked.aimValid=false;doc.cancel_spatial_menu();
  auto surface=doc.frame().surface;if(surface->mesh.positions.empty())throw std::runtime_error("Scene fixture has no sculpt surface");const auto p=surface->mesh.positions.front();
  hand.frame.hands[1].tracked.grip.position=ob::transform(doc.frame().state->canvas,{p.x,p.y,p.z});hand.send(0,0,false,.8f);hand.send(0,0,false,.8f);
  check(doc.frame().state->selection_mode&&doc.copy_scene().encode()==before,"A held trigger on return cannot leak an edit while selection mode remains active");
  doc.configure_selection(false);hand.send(0,0,false,.8f);hand.send(0,0,false,.8f);
  check(doc.copy_scene().encode()==before,"Leaving selection with a held trigger still requires a real physical release");
  hand.send();hand.send(0,0,false,.8f);hand.send(0,0,false,.8f);hand.send();ready(doc);
  check(doc.copy_scene().encode()!=before,"A fresh released-then-pressed trigger rearms ordinary editing after modal shutdown");
  check(doc.undo()&&doc.copy_scene().encode()==before,"Rearmed editing keeps one exact Undo without hidden leaked strokes");
 }
 std::cout<<"PASS "<<checks<<" owned modal selection/held-trigger checks\n";return 0;
}catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
