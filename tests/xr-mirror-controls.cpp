// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "shapekey-mirror-controls.hpp"
#include "import-controller-driver.hpp"
#include "dome/source-maps.hpp"
#include <atomic>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
std::atomic<unsigned> calls{0};std::atomic<double> seen_plane{0};std::atomic<float> seen_tolerance{0};std::atomic<int> seen_method{-1};std::atomic<size_t> seen_target{SIZE_MAX};
void check(bool value,const char *message){if(!value)throw std::runtime_error(message);std::cout<<"PASS "<<message<<'\n';}
}
extern "C" ShapeKeyResult mirror_real(Scene&,uint64_t,const ShapeKeyEdit&,const std::function<bool()>&) asm("__real__ZN4dome15author_shapekeyERNS_5SceneEmRKNS_12ShapeKeyEditERKSt8functionIFbvEE");
extern "C" ShapeKeyResult mirror_wrap(Scene&,uint64_t,const ShapeKeyEdit&,const std::function<bool()>&) asm("__wrap__ZN4dome15author_shapekeyERNS_5SceneEmRKNS_12ShapeKeyEditERKSt8functionIFbvEE");
extern "C" ShapeKeyResult mirror_wrap(Scene &s,uint64_t id,const ShapeKeyEdit &edit,const std::function<bool()> &cancelled){if(edit.action==ShapeKeyAction::Mirror){seen_plane=edit.mirror_plane;seen_tolerance=edit.mirror_tolerance;seen_method=int(edit.mirror_frames);seen_target=edit.target;++calls;}return mirror_real(s,id,edit,cancelled);}
namespace {
Scene fixture(){
 const float p=.07f;Mesh m;m.positions={{p-.3f,-.2f,0},{p-.1f,-.2f,0},{p-.2f,.2f,0},{p+.3f,-.2f,0},{p+.1f,-.2f,0},{p+.2f,.2f,0}};m.indices={0,1,2,3,5,4};m.normals.assign(6,{0,0,1});m.uv={{{.0625f,.0625f}},{{.4375f,.0625f}},{{.25f,.9375f}},{{.9375f,.0625f}},{{.5625f,.0625f}},{{.75f,.9375f}}};
 Scene s;auto &o=s.active();o.mesh_active=true;auto maps=std::make_shared<PaintSourceMaps>();maps->normal.image=std::make_shared<const Image>(Image{1,1,{128,128,255,255}});o.painted=PaintedMesh::from_maps(m,maps,64);o.transform.position={.2f,.3f,.1f};o.transform.scale=1.3f;
 PaintSettings ink;ink.color={.2f,.7f,.3f,.7f};Brush b;b.from=b.to={p-.2f,0,0};b.normal={0,0,1};b.radius=.09f;o.painted->begin_stroke();o.painted->stamp(b,ink);check(o.painted->end_stroke(),"Actual fixture owns nontrivial sparse painting");
 ShapeKeyEdit edit;edit.name="左 eye";author_shapekey(s,o.id,edit);edit.action=ShapeKeyAction::Replace;edit.target=0;edit.positions=m.positions;edit.positions[0]+=Vec3{.015f,0,.04f};author_shapekey(s,s.active().id,edit);edit={};edit.name="Other key";author_shapekey(s,s.active().id,edit);s.validate();return s;
}
struct Hand:ImportDriver{
 using ImportDriver::ImportDriver;
 void shapes(){open();sector(8);sector(9);sector(7);check(doc.frame().state->menu_page==24,"Actual joystick opens the native shape workspace");}
 void row(int row,float t){auto s=doc.frame().state;auto &h=frame.hands[1].tracked;h.aimValid=true;const float u=(100+420*t)/640,v=float(240+row*80)/640;h.aim.position=ob::transform(s->panel,{(u-.5f)*.6f,(.5f-v)*.6f,.25f});h.aim.orientation=s->panel.orientation;send(0,0,false,.8f);send();h.aimValid=false;}
};
void ready(Document &d){check(d.wait_asset()&&d.wait_mesh(),"Native shape worker and mesh publication finish");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
void unit(){
 VrControlState c;c.parameter=VrParameter::ShapeMirror;c.components=3;c.value={0,.5f,1.f/3,127};auto e=detail::mirror_control_request(c);check(e.target==127&&e.mirror_plane==0&&e.mirror_tolerance==.0001f&&e.mirror_frames==ShapeKeyMirrorFrames::VertexPositions,"Exact defaults and maximum native target survive normalized control encoding");
 for(double p:{-1e6,-1000.,-.07,-.0001,0.,.0001,.07,1000.,1e6}){const auto t=detail::mirror_plane_slider(p);check(std::abs(detail::mirror_plane_metres(t)-p)<=std::max(1e-9,std::abs(p)*3e-6),"Signed-log slider preserves explicit plane sign, zero and full desktop range");}
 for(int method=0;method<3;++method){c.value[0]=method*.5f;const auto want=method==0?ShapeKeyMirrorFrames::VertexPositions:method==1?ShapeKeyMirrorFrames::ExactAuthored:ShapeKeyMirrorFrames::BaseRelative;check(detail::mirror_control_request(c).mirror_frames==want,"All three explicit methods retain their authentic kernel policy");}
 c.value[3]=128;bool refused=false;try{detail::mirror_control_request(c);}catch(const std::invalid_argument&){refused=true;}check(refused,"Control payload cannot mint a target beyond the validated128 target budget");
}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);unit();
 for(int mode=0;mode<3;++mode){Document d;d.replace(fixture());ready(d);d.select_shape_target(0);const auto source=d.copy_scene();const auto original=source.encode();calls=0;Hand hand(d);hand.shapes();hand.sector(3);auto c=d.frame().state->controls;check(c.parameter==VrParameter::ShapeMirror&&c.components==3&&c.value[3]==0&&calls==0&&d.copy_scene().encode()==original,"Mirror entry stages captured target and settings without authoring");
  hand.row(3,1);check(d.frame().state->controls.value[3]==0,"Pointer outside the three rows cannot retarget the captured immutable key");
  hand.row(0,mode*.5f);hand.row(1,detail::mirror_plane_slider(.07));hand.row(2,1.f/3);c=d.frame().state->controls;auto edit=detail::mirror_control_request(c);auto expected=source;author_shapekey(expected,expected.active().id,edit);calls=0;hand.panel_button(true);const auto path=out/("method-"+std::to_string(mode)+".dsculpt");d.save_async(path).get();ready(d);
  check(calls==1&&seen_plane==edit.mirror_plane&&seen_tolerance==edit.mirror_tolerance&&seen_method==int(edit.mirror_frames)&&seen_target==0,"Actual controller delivers the selected local plane, tolerance, method and captured target to the real worker");
  check(d.copy_scene().encode()==expected.encode()&&Scene::load(path).encode()==expected.encode(),"Explicit Mirror plus immediately ordered Save preserves the complete genuine kernel endpoint");
  const auto endpoint=d.copy_scene();const auto &delta=endpoint.active().deformation->morphs->targets.back().delta;check(length(delta.at(3)-Vec3{-.015f,0,.04f})<1e-6f&&length(delta.at(0))<1e-6f,"Independent displaced counterpart oracle verifies the nonzero off-centre local mirror");
  check(d.undo()&&d.copy_scene().encode()==original&&d.redo()&&d.copy_scene().encode()==expected.encode(),"Controller Mirror is one exact complete Undo/Redo operation");
 }
 {Document d;d.replace(fixture());ready(d);d.select_shape_target(0);const auto original=d.copy_scene().encode();Hand hand(d);hand.shapes();hand.sector(3);hand.row(1,detail::mirror_plane_slider(.07));calls=0;hand.panel_button(false);check(calls==0&&d.copy_scene().encode()==original&&d.frame().state->controls.parameter==VrParameter::None,"Actual Cancel closes Mirror without touching source or history");
  hand.sector(3);hand.row(1,detail::mirror_plane_slider(.07));calls=0;auto path=out/"save-cancels.dsculpt";d.save_async(path).get();check(calls==0&&d.copy_scene().encode()==original&&Scene::load(path).encode()==original&&d.frame().state->controls.parameter==VrParameter::None,"Save cancels staged Mirror instead of implicitly authoring a target");}
 {Document d;d.replace(fixture());ready(d);d.select_shape_target(0);Hand hand(d);hand.shapes();hand.sector(3);calls=0;hand.frame.focused=false;hand.send();check(calls==0&&d.frame().state->controls.parameter==VrParameter::None,"Real focus loss cancels Mirror instead of applying staged settings");}
 {Document d;d.replace(fixture());ready(d);d.select_shape_target(0);Hand hand(d);hand.shapes();hand.sector(3);auto newer=d.copy_scene();newer.active().material.name="Legitimate newer source";const auto expected=newer.encode();d.replace(newer);ready(d);calls=0;hand.panel_button(true);check(calls==0&&d.copy_scene().encode()==expected,"An old Mirror confirmation cannot overwrite a legitimately replaced source");}
 std::cout<<"PASS XR Mirror native settings workflow\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
