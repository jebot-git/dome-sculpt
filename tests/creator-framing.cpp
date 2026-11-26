// SPDX-License-Identifier: GPL-3.0-or-later
#include "creator-framing.hpp"
#include "dome/creation.hpp"
#include "dome/rig-edit.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
using namespace dome;
namespace {
size_t checks=0;
void check(bool yes,const char*why){if(!yes)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<'\n';}
bool same(CanvasTransform a,CanvasTransform b){return a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z&&a.orientation.x==b.orientation.x&&a.orientation.y==b.orientation.y&&a.orientation.z==b.orientation.z&&a.orientation.w==b.orientation.w&&a.scale==b.scale;}
std::array<SpatialView,2> eyes(CanvasTransform head,float angle=.6f){
 std::array<SpatialView,2> result;
 for(size_t i=0;i<2;++i){result[i].pose=ob::compose(head,{{i?.032f:-.032f,0,0},{},1});result[i].fov={-angle,angle,-angle,angle};}
 return result;
}
bool inside(ob::Vec3 stage,const std::array<SpatialView,2>&views){
 for(const auto&eye:views){const auto p=ob::transform(ob::inverse(eye.pose),stage);if(p.z>=-.05f||p.z<=-100.f)return false;
  const double x=p.x/-p.z,y=p.y/-p.z;
  if(x<std::tan(eye.fov[0])-1e-5||x>std::tan(eye.fov[1])+1e-5||y<std::tan(eye.fov[2])-1e-5||y>std::tan(eye.fov[3])+1e-5)return false;
 }return true;
}
void corners(Vec3 lo,Vec3 hi,CanvasTransform canvas,const std::array<SpatialView,2>&views){
 bool all=true;for(unsigned i=0;i<8;++i)all&=inside(ob::transform(canvas,{i&1?hi.x:lo.x,i&2?hi.y:lo.y,i&4?hi.z:lo.z}),views);
 check(all,"Independent physical-eye projection contains all accepted bound corners");
}
void math(){
 CanvasTransform head{{.4f,1.7f,-.2f},{},1};CanvasTransform canvas{{.4f,1.58f,-1.05f},{},1};
 auto views=eyes(head);const Vec3 lo{-.72f,-.85f,-.16f},hi{.72f,.85f,.16f};
 auto a=detail::initial_creator_fit(lo,hi,canvas,head,views);check(bool(a),"Real stereo bounds admit first-avatar fit");corners(lo,hi,*a,views);
 auto narrow=eyes(head,.4f);auto b=detail::initial_creator_fit(lo,hi,canvas,head,narrow);check(bool(b)&&b->position.z<a->position.z,"Narrow actual FoV requires more distance, without fixture constants");corners(lo,hi,*b,narrow);
 views[0].fov={-.42f,.71f,-.5f,.63f};views[1].fov={-.68f,.43f,-.62f,.48f};
 views[0].pose.orientation={0,std::sin(.025f),0,std::cos(.025f)};views[1].pose.orientation={0,-std::sin(.025f),0,std::cos(.025f)};
 auto c=detail::initial_creator_fit(lo,hi,canvas,head,views);check(bool(c),"Asymmetric canted-eye fit uses both original projections");corners(lo,hi,*c,views);
 views=eyes(head);for(auto&eye:views)eye.fov={.3f,.7f,-.5f,.5f};
 auto offaxis=detail::initial_creator_fit(lo,hi,canvas,head,views);check(bool(offaxis),"Valid off-axis stereo overlap fits without changing physical head direction");corners(lo,hi,*offaxis,views);
 views=eyes(head);views[0].fov={-.6f,.7f,-.6f,.6f};views[1].fov={.45f,.5f,-.4f,.4f};
 auto narrow_overlap=detail::initial_creator_fit(lo,hi,canvas,head,views);check(bool(narrow_overlap),"Unequal eye frusta use their common overlap instead of averaging incompatible center rays");corners(lo,hi,*narrow_overlap,views);
 views=eyes(head);views[0].fov={-.7f,-.4f,-.5f,.5f};views[1].fov={.4f,.7f,-.5f,.5f};check(!detail::initial_creator_fit(lo,hi,canvas,head,views),"Disjoint physical eye frusta leave presentation unchanged");
 head.orientation=ob::multiply({0,std::sin(.3f),0,std::cos(.3f)},{std::sin(-.1f),0,0,std::cos(-.1f)});views=eyes(head);
 auto d=detail::initial_creator_fit(lo,hi,canvas,head,views);check(bool(d),"Translated and rotated physical head admits fit without changing its pose");corners(lo,hi,*d,views);
 check(a->scale==canvas.scale&&a->orientation.w==canvas.orientation.w,"Fit retains canvas scale and orientation");
 views[1].fov[1]=views[1].fov[0];check(!detail::initial_creator_fit(lo,hi,canvas,head,views),"Degenerate actual eye FoV is refused");
 views=eyes(head);views[0].fov[2]=std::numeric_limits<float>::quiet_NaN();check(!detail::initial_creator_fit(lo,hi,canvas,head,views),"Nonfinite actual eye FoV is refused");
 views=eyes(head,.0001f);check(!detail::initial_creator_fit(lo,hi,canvas,head,views),"A fit beyond the actual renderer far clip is refused instead of hiding the avatar");
 views=eyes(head);views[1].pose.orientation.w=0;check(!detail::initial_creator_fit(lo,hi,canvas,head,views),"Invalid eye pose is refused");
 views=eyes(head);check(!detail::initial_creator_fit(hi,lo,canvas,head,views),"Invalid accepted bounds are refused");
 head={};views=eyes(head);canvas={{0,0,-3},{},1};auto steady=detail::initial_creator_fit(lo,hi,canvas,head,views);check(steady&&same(*steady,canvas),"Already fitting presentation stays exact");
}
struct Driver {
 Document&doc;SpatialFrame f;
 explicit Driver(Document&d):doc(d){f.head.position={0,1.6f,0};f.head_valid=f.focused=true;f.views=eyes(f.head);doc.begin_spatial();send();}
 void send(){++f.sequence;f.time+=.02;doc.submit(f);if(!doc.wait_input(std::chrono::seconds(30)))throw std::runtime_error("Input worker timeout");if(!doc.frame().state->error.empty())throw std::runtime_error(doc.frame().state->error);}
 void navigate(bool return_back=false){f.hands[0].tracked.valid=true;f.hands[0].tracked.squeeze=1;f.hands[0].tracked.grip.position={-.2f,1.2f,-.5f};send();f.hands[0].tracked.grip.position.x+=.2f;send();if(return_back){f.hands[0].tracked.grip.position.x-=.2f;send();}f.hands[0].tracked.squeeze=0;send();}
};
void ready(Document&doc){if(!doc.wait_asset(std::chrono::seconds(180))||!doc.wait_mesh(std::chrono::seconds(180)))throw std::runtime_error("Accepted creator mesh timeout");if(!doc.frame().state->error.empty())throw std::runtime_error(doc.frame().state->error);}
Scene apply(Document&doc,std::optional<HumanoidControls> requested={}){
 auto expected=doc.copy_scene();auto controls=requested.value_or(humanoid_baseline_defaults("vroid-beta-female"));CanvasTransform placement;placement.position.y=-controls.height*.5f;append_humanoid(expected,controls,"Humanoid",placement,256);
 doc.begin_humanoid_creator();if(requested){auto settings=doc.frame().state->creator->settings;settings.controls=*requested;settings.preset.reset();doc.configure_humanoid_creator(std::move(settings),doc.frame().state->creator->session);}doc.end_humanoid_creator(true);ready(doc);check(doc.copy_scene().encode()==expected.encode(),"Accepted creator native geometry and placement equal original operation exactly");return expected;
}
std::vector<char> bytes(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);return{std::istreambuf_iterator<char>(f),{}};}
void avatar_inside(Document&doc,const SpatialFrame&f){
 auto scene=doc.copy_scene();const auto group=scene.active().group;const auto snapshot=doc.frame();bool all=true;size_t n=0;
 for(const auto&surface:scene.preview_surfaces()){
  if(scene.object(surface.id).group!=group)continue;const auto mesh=surface.preview_mesh();const auto world=surface.world_matrix();
  for(const auto&p:mesh.positions){const auto q=world.point({p.x,p.y,p.z});all&=inside(ob::transform(snapshot.state->canvas,{float(q[0]),float(q[1]),float(q[2])}),*f.views);++n;}
 }
 check(n>9000&&all,"Every actual posed first-avatar vertex fits both independent physical eye projections");
}
void native(const std::filesystem::path&root){
 {
  Document doc;doc.seed();ready(doc);check(!doc.frame().state->dirty,"Pristine seeded workspace retains its original clean state");Driver driver(doc);const auto original=doc.copy_scene().encode();const auto anchor=doc.frame().state->canvas;
  doc.begin_humanoid_creator();doc.end_humanoid_creator(false);driver.send();check(doc.copy_scene().encode()==original&&same(anchor,doc.frame().state->canvas)&&!doc.frame().state->dirty,"Pristine creator Cancel preserves native scene and presentation exactly");
  doc.begin_humanoid_creator();driver.f.focused=false;driver.send();check(!doc.frame().state->creator&&doc.copy_scene().encode()==original&&same(anchor,doc.frame().state->canvas)&&!doc.frame().state->dirty,"Pristine creator focus-loss cancellation preserves scene and anchor exactly");driver.f.focused=true;driver.send();
  const auto expected=apply(doc);doc.save_async(root/"before-fit.dsculpt").get();driver.f.views.reset();driver.send();check(same(anchor,doc.frame().state->canvas),"Missing actual stereo views defer fit rather than inventing a camera");
  driver.f.views=eyes(driver.f.head);driver.send();const auto fitted=doc.frame().state->canvas;check(!same(anchor,fitted),"First accepted pristine creator gets one initial presentation fit");avatar_inside(doc,driver.f);check(!doc.frame().state->dirty,"Session presentation fit does not dirty an already saved native document");check(doc.copy_scene().encode()==expected.encode(),"Session-only fit preserves complete native geometry/source/placement bytes");doc.save_async(root/"after-fit.dsculpt").get();check(bytes(root/"before-fit.dsculpt")==bytes(root/"after-fit.dsculpt"),"Ordered native Save bytes remain exact before and after view fit");
  driver.f.head.position.x+=.3f;driver.f.views=eyes(driver.f.head,.35f);driver.send();check(same(fitted,doc.frame().state->canvas),"Later head motion and narrower FoV do not repeat one-shot fit");
 }
 {
  Document doc;doc.seed();ready(doc);Driver driver(doc);auto styled=humanoid_preset(HumanoidPreset::Anime,"vroid-beta-male");styled.height=2.1f;styled.hair_length=.5f;styled.hair_volume=.5f;apply(doc,styled);driver.send();avatar_inside(doc,driver.f);
 }
 {
  Document doc;doc.seed();ready(doc);Driver driver(doc);const auto anchor=doc.frame().state->canvas;apply(doc);const auto accepted=doc.copy_scene().encode();doc.begin_humanoid_creator();doc.end_humanoid_creator(false);driver.send();check(same(anchor,doc.frame().state->canvas)&&doc.copy_scene().encode()==accepted,"Canceling a later draft retires a still-pending first-avatar fit exactly");
 }
 {
  Document doc;doc.seed();ready(doc);const auto expected=apply(doc);doc.begin_spatial();SpatialFrame unfocused;unfocused.sequence=1;unfocused.time=.01;doc.submit(unfocused);check(doc.wait_input(std::chrono::seconds(30)),"Initial unfocused XR sample is drained");
  Driver driver(doc);avatar_inside(doc,driver.f);check(doc.copy_scene().encode()==expected.encode(),"Desktop-first creation waits for its first physical stereo presentation without changing native state");
 }
 {
  Document doc;doc.seed();ready(doc);auto same_scene=doc.copy_scene();doc.replace(same_scene);ready(doc);Driver driver(doc);const auto anchor=doc.frame().state->canvas;apply(doc);driver.send();check(same(anchor,doc.frame().state->canvas),"Existing/reopened identical pristine bytes do not qualify for initial-workspace fitting");
 }
 {
  Document doc;doc.seed();ready(doc);Driver driver(doc);const auto anchor=doc.frame().state->canvas;driver.navigate(true);const auto returned=doc.frame().state->canvas;check(std::abs(anchor.position.x-returned.position.x)<1e-6f,"Real navigation fixture moves away and returns to original placement");apply(doc);driver.send();check(same(returned,doc.frame().state->canvas),"Prior canvas navigation disqualifies fit even after returning to original transform");
 }
 {
  Document doc;doc.seed();ready(doc);Driver driver(doc);apply(doc);driver.navigate();const auto moved=doc.frame().state->canvas;driver.send();check(same(moved,doc.frame().state->canvas),"Navigation after Apply retires pending initial fit");
 }
 {
  Document doc;doc.seed();ready(doc);Driver driver(doc);const auto anchor=doc.frame().state->canvas;const auto expected=apply(doc);driver.f.focused=false;driver.send();driver.f.focused=true;driver.send();check(same(anchor,doc.frame().state->canvas)&&doc.copy_scene().encode()==expected.encode(),"Focus loss after accepted Apply retires pending fit without discarding accepted source");
 }
 {
  Document doc;doc.seed();ready(doc);Driver driver(doc);doc.add_object(false);check(doc.undo(),"Native authoring-history fixture undoes to original content");ready(doc);const auto anchor=doc.frame().state->canvas;apply(doc);driver.send();check(same(anchor,doc.frame().state->canvas),"An edited then undone workspace is not pristine for automatic fitting");
 }
 {
  Scene authored;append_humanoid(authored,humanoid_preset(HumanoidPreset::Toon),"Existing author");auto&part=authored.active();RigEdit edit;edit.joint=JointAdjustment{part.deformation->humanoid->joints.at("head"),{0,5,0},{}};edit_rig(authored,part.id,edit);
  Document doc;doc.replace(authored);ready(doc);Driver driver(doc);const auto anchor=doc.frame().state->canvas;const auto expected=apply(doc);driver.send();check(same(anchor,doc.frame().state->canvas)&&doc.copy_scene().encode()==expected.encode(),"Existing authored plus5m joint stress retains complete source and original presentation");
 }
}
}
int main(int argc,char**argv)try{if(argc!=2)throw std::runtime_error("usage: creator-framing-test OUTPUT");std::filesystem::create_directories(argv[1]);math();native(argv[1]);std::cout<<"Passed "<<checks<<" creator framing checks\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
