// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop-framing.hpp"
#include <iostream>
using namespace dome;
namespace {
unsigned checks=0;
void check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<'\n';}
bool close(Vec3 a,Vec3 b){return length(a-b)<.0001f;}
void fits(const detail::DesktopFraming &view,Quat rotation,float aspect,std::span<const Vec3> points){
 bool inside=true;for(auto p:points){auto camera=rotation.rotate(p)+view.pan;auto depth=view.distance-camera.z;inside&=depth>0&&std::abs(camera.x)<depth*aspect*std::tan(3.14159265358979323846/8)&&std::abs(camera.y)<depth*std::tan(3.14159265358979323846/8);}
 check(inside,"Independent perspective inequalities contain every eligible transformed vertex with positive depth");
 auto origin=rotation.inverse().rotate(Vec3{0,0,view.distance}-view.pan);auto direction=rotation.inverse().rotate({0,0,-1});
 check(close(origin+direction*view.distance,view.pivot),"Actual native inverse-camera ray passes through the framed orbit pivot");
}
}
int main()try{
 auto mesh=std::make_shared<MeshSnapshot>();auto state=std::make_shared<EditorState>();mesh->generation=state->generation=7;
 mesh->mesh.positions={{10,0,0},{12,2,0},{10,2,1},{-5,0,-2},{-4,2,-1},{-5,2,0},{10000,10000,10000},{10001,10000,10000},{10000,10001,10000}};mesh->mesh.indices={0,1,2,3,4,5,6,7,8};
 mesh->ranges={{1,0,3,{}},{2,3,3,{}},{3,6,3,{}}};state->objects={{1,"Chosen",true,false},{2,"Other",true,false},{3,"Hidden",false,false}};state->active_object=1;state->selected_objects={1};EditorSnapshot f{state,mesh};
 auto all=detail::desktop_framing(f,{},1.4f,false);check(all&&close(all->pivot,{3.5f,1,-.5f}),"Frame all excludes a hidden distant source and frames the complete visible geometry");fits(*all,{},1.4f,std::span(mesh->mesh.positions).first(6));
 auto selected=detail::desktop_framing(f,{},.7f,true);check(selected&&close(selected->pivot,{11,1,.5f}),"Portrait Frame selection centers only the actual selected object");fits(*selected,{},.7f,std::span(mesh->mesh.positions).first(3));
 state->selected_objects={1,2};auto multiple=detail::desktop_framing(f,{},1.4f,true);check(multiple&&close(multiple->pivot,all->pivot),"Multi-object selection frames the whole chosen collection");
 state->selected_objects.clear();auto active=detail::desktop_framing(f,{},.7f,true);check(active&&close(active->pivot,selected->pivot),"Ordinary active-object framing works outside multiselect mode");
 state->selected_objects={3};check(!detail::desktop_frameable(f,true)&&detail::desktop_frameable(f,false),"Frame menu eligibility follows visible selection without scanning geometry");check(!detail::desktop_framing(f,{},1,true),"A hidden-only selection does not replace the current camera");state->selected_objects={1};
 mesh->ranges[0].world_frame_valid=true;state->objects[0].world_transform_valid=true;state->objects[0].world_transform.position={20,4,3};state->objects[0].world_transform.scale=2;
 std::vector<Vec3> live;for(auto p:std::span(mesh->mesh.positions).first(3))live.push_back(p*2+Vec3{20,4,3});
 auto moved=detail::desktop_framing(f,{},1,true);check(moved&&close(moved->pivot,{42,6,4}),"Framing follows the same live similarity placement delta as rendering before repacking");fits(*moved,{},1,live);
 mesh->ranges[0].affine_world=AffineTransform{};state->objects[0].affine_world=AffineTransform{};auto unchanged_affine=detail::desktop_framing(f,{},1,true);check(unchanged_affine&&close(unchanged_affine->pivot,moved->pivot),"Equal affine witnesses preserve the renderer similarity-preview fallback");
 state->objects[0].world_transform_valid=false;std::array<double,16> before{2,0,0,0,0,1,0,0,0,0,3,0,1,2,3,1},after{-2,.4,0,0,.7,1,0,0,0,0,.8,0,10,6,5,1};mesh->ranges[0].affine_world=AffineTransform(before);state->objects[0].affine_world=AffineTransform(after);
 live.clear();for(auto p:std::span(mesh->mesh.positions).first(3)){float x=(p.x-1)/2,y=p.y-2,z=(p.z-3)/3;live.push_back({-2*x+.7f*y+10,.4f*x+y+6,.8f*z+5});}
 Quat turn{std::cos(.3f),0,std::sin(.3f),0};auto affine=detail::desktop_framing(f,turn,.6f,true);check(bool(affine),"Reflected/sheared live original-node placement can be framed without TRS approximation");fits(*affine,turn,.6f,live);
 Quat next{std::cos(.7f),std::sin(.7f),0,0};auto pan=affine->pan+Vec3{.2f,-.1f,.3f};auto updated=detail::orbit_pan(pan,affine->pivot,turn,next);check(close(turn.rotate(affine->pivot)+pan,next.rotate(affine->pivot)+updated),"Trackball orbit preserves the framed center even after a manual pan");
 mesh->generation=6;check(!detail::desktop_framing(f,{},1,false),"A replaced document cannot frame geometry from an obsolete generation");mesh->generation=7;check(!detail::desktop_framing(f,{},0,false),"An invalid viewport cannot produce a new camera");
 state->objects[0].visible=state->objects[1].visible=false;check(!detail::desktop_framing(f,{},1,false),"An empty visible scene preserves existing camera state");
 std::cout<<checks<<" desktop framing checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
