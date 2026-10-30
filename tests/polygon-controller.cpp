// SPDX-License-Identifier: GPL-3.0-or-later
#include "polygon-fixture.hpp"
#include "import-controller-driver.hpp"
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {unsigned checks=0;void check(bool v,const char *m){if(!v)throw std::runtime_error(m);++checks;std::cout<<"PASS "<<m<<std::endl;}
void ready(Document &doc){check(doc.wait_polygon(),"Ordered polygon preparation completes outside input loop");if(!doc.frame().state->error.empty())throw std::runtime_error(doc.frame().state->error);}
void point(ImportDriver &hand,Vec3 local,float pressure){auto s=hand.doc.frame().state;auto object=std::find_if(s->objects.begin(),s->objects.end(),[&](auto &o){return o.id==s->active_object;});auto world=ob::compose(s->canvas,object->world_transform);auto &h=hand.frame.hands[1].tracked;h.aimValid=true;h.aim.position=ob::transform(world,{local.x,local.y,local.z});h.aim.orientation=world.orientation;hand.send(0,0,false,pressure);}
}
int main()try{
 Document doc;doc.replace(polygon_test::fixture());check(doc.wait_mesh(),"Native controller source surface ready");auto original=doc.copy_scene().encode();ImportDriver hand(doc);
 hand.open();hand.sector(8);hand.sector(9);hand.sector(2);check(doc.frame().state->menu_page==19,"Actual joystick MeshTools cell2 navigates to Polygon19");
 hand.sector(0);ready(doc);check(doc.frame().state->polygon&&!doc.frame().state->menu,"Joystick Start creates native N-gon workspace and returns to pressure canvas");
 point(hand,{0,0,3},.8f);point(hand,{0,0,3},0);ready(doc);check(doc.frame().state->polygon->selection->ids.size()==1&&doc.copy_scene().encode()==original,"Actual tracked aim/pressure selects one original face without mutating source");
 hand.open();check(doc.frame().state->menu_page==19,"Any joystick reopens active polygon workspace page");hand.sector(7);ready(doc);check(doc.frame().state->polygon->topology->faces().size()==10&&doc.frame().state->menu,"XR Extrude creates real side faces and retains operation menu");
 hand.sector(9);check(doc.frame().state->menu_page==22,"Topology button opens Polygon22 without losing draft selection");hand.sector(8);check(doc.frame().state->menu_page==19,"Standard Back8 returns from topology to Polygon19");
 hand.sector(6);check(doc.frame().state->controls.parameter==VrParameter::PolygonDistance&&doc.frame().state->controls.logarithmic&&doc.frame().state->controls.value[0]==10,"XR distance opens a large logarithmic world-millimetre slider");hand.ray(.5f);hand.panel_button(false);check(doc.frame().state->polygon_controls.distance_mm==10&&doc.copy_scene().encode()==original,"Cancel slider keeps settings, scene and global history exact");
 doc.cancel_spatial_menu();hand.open();hand.sector(4);check(doc.frame().state->polygon_controls.move_mode&&!doc.frame().state->menu,"Move/select control changes interaction mode and returns to canvas");
 auto source=doc.frame().state->polygon->surface;point(hand,{0,0,3},.8f);point(hand,{.1f,0,3},.8f);ready(doc);check(doc.frame().state->polygon->moving&&doc.frame().state->polygon->move_delta.x>.09f&&doc.frame().state->polygon->surface==source,"Actual continuous pressure movement publishes GPU delta while immutable source geometry is reused");
 hand.frame.focused=false;hand.send();ready(doc);check(!doc.frame().state->polygon->moving&&doc.copy_scene().encode()==original,"Focus loss cancels current component gesture while retaining committed source");
 hand.frame.focused=true;hand.send();hand.open();hand.sector(10);ready(doc);check(!doc.frame().state->polygon&&doc.copy_scene().active().polygon->topology.faces().size()==10,"Actual joystick Apply accepts local topology through one guarded transaction");
 check(doc.undo()&&doc.copy_scene().encode()==original&&!doc.frame().state->can_undo,"Single Undo after XR Apply restores full original paint, pose and N-gon source");
 check(doc.redo(),"XR accepted topology can be restored before testing retained-source access");auto accepted=doc.copy_scene().encode();
 hand.open();hand.sector(8);hand.sector(9);hand.sector(2);hand.sector(0);
 check(doc.frame().state->controls.parameter==VrParameter::PolygonConversion&&doc.frame().state->controls.maximum==2&&doc.frame().state->controls.value[0]==0,"Accepted object exposes a large edit/original panel with Cancel as the default");
 hand.panel_button(true);check(!doc.frame().state->polygon&&doc.frame().state->controls.parameter==VrParameter::None&&doc.copy_scene().encode()==accepted,"Confirming untouched default Cancel performs no conversion or source restore");
 doc.cancel_spatial_menu();hand.open();hand.sector(8);hand.sector(9);hand.sector(2);hand.sector(0);hand.ray(1);
 check(doc.frame().state->controls.value[0]>1.5f,"Actual XR ray selects Restore rather than the default Cancel choice");hand.panel_button(true);
 check(doc.wait_asset()&&doc.frame().state->error.empty(),"Actual owner-bound XR pressure confirms retained-source Restore on its existing asset worker");
 check(doc.copy_scene().encode()==original&&!doc.frame().state->polygon,"XR source Restore recovers full original sculpt, masks, paint and editable N-gons after accepted topology");
 check(doc.undo()&&doc.copy_scene().encode()==accepted,"Retained-source Restore is one global undoable transaction");
 // File commands must cancel a staged destructive source choice rather than
 // implicitly confirming it as an ordinary scalar preview.
 auto save_path=std::filesystem::temp_directory_path()/("dome-polygon-source-choice-"+std::to_string(getpid())+".dsculpt");
 struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code e;std::filesystem::remove(path,e);}} cleanup{save_path};
 doc.save(save_path);hand.open();hand.sector(8);hand.sector(9);hand.sector(2);hand.sector(0);hand.ray(1);
 check(doc.frame().state->controls.parameter==VrParameter::PolygonConversion&&doc.frame().state->controls.value[0]>1.5f,"Unconfirmed source Restore remains a staged owner-bound command");
 doc.request_save();doc.save_async(save_path).get();
 check(doc.frame().state->controls.parameter==VrParameter::None&&!doc.frame().state->polygon&&doc.copy_scene().encode()==accepted&&Scene::load(save_path).encode()==accepted,"Save cancels staged Restore and writes the unchanged committed scene instead of executing a destructive choice");
 check(spatial_action(0,4)==SpatialCommand::Undo&&spatial_parameter(14,0)==VrParameter::SkinSource&&spatial_parameter(16,9)==VrParameter::RetopoDistance&&spatial_parameter(17,1)==VrParameter::CutOffset&&spatial_action(20,0)==SpatialCommand::ObjectGrabMode&&spatial_action(21,0)==SpatialCommand::ImportBrowse&&spatial_parameter(23,6)==VrParameter::ImportUnitScale,"Root Undo and Skin/Retopo/Cut/Holding/Import page registry coexist with polygon pages");
 std::cout<<"PASS "<<checks<<" polygon controller checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
