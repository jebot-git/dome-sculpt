// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_skin_affine_test
#include "skin-affine.cpp"
#undef main
#include <numbers>
struct Driver {
 dome::Document &doc;dome::SpatialFrame frame;
 explicit Driver(dome::Document &d):doc(d){frame.focused=frame.head_valid=true;frame.head.position={0,1.6f,0};frame.hands[1].tracked.valid=true;send();}
 void send(float x=0,float y=0,bool click=false,float force=0){++frame.sequence;frame.time+=.03;auto &h=frame.hands[1];h.x=x;h.y=y;h.joystick=click;h.tracked.trigger=force;doc.submit(frame);if(!doc.wait_input())throw std::runtime_error("Affine skin input timeout");}
 void sector(int n){float a=n*std::numbers::pi_v<float>/6;send(std::cos(a),std::sin(a));send(std::cos(a),std::sin(a),true);send();}
 void open(){send();send(0,0,true);send();sector(8);sector(10);sector(5);sector(10);sector(11);}
 void value(float t){auto state=doc.frame().state;auto &h=frame.hands[1];h.tracked.aimValid=true;h.tracked.aim.position=ob::transform(state->panel,{((100+420*t)/640.f-.5f)*.6f,(.5f-300/640.f)*.6f,.25f});h.tracked.aim.orientation=state->panel.orientation;send(0,0,false,.8f);send();}
 void confirm(){send(0,0,true);send();}
};
int main()try{
 auto scene=fixture(2,true,true);Document d;d.replace(scene);check(d.wait_mesh(),"Affine native source geometry ready");Driver hand(d);hand.open();auto before=d.copy_scene().encode();
 check(d.frame().state->menu_page==14&&d.frame().state->menu_enabled[7],"Actual joystick exposes Bind for affine target and reference");
 hand.sector(1);hand.value(1);hand.confirm();check(d.frame().state->skin_options.mode==SkinAuthoringMode::Transfer&&d.copy_scene().encode()==before,"Actual ray method selection is staged and retains complete Scene");
 hand.sector(7);check(d.wait_asset()&&d.frame().state->error.empty()&&d.copy_scene().active().deformation->skin,"Actual controller Bind executes persistent full-affine transfer");auto after=d.copy_scene().encode();compare_pose(d.copy_scene());
 check(d.undo()&&d.copy_scene().encode()==before&&d.redo()&&d.copy_scene().encode()==after,"Controller transfer shares exact one-step global history");
 std::cout<<"PASS "<<checks<<" affine controller checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
