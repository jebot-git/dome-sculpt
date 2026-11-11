// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/source-maps.hpp"
#include "dome/source-paint.hpp"
#include "paint-color-access.hpp"
#include "texture-set-stroke.hpp"
#include "texture-set-paint-access.hpp"
#include "source-color-fixtures.hpp"
#include <EGL/egl.h>
#include <condition_variable>
#include <iostream>
using namespace dome;
namespace {
size_t checks=0;std::atomic_bool gate=false;std::mutex gate_mutex;std::condition_variable gate_changed;bool entered=false,released=false;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);++checks;}
bool refuses(auto f){try{f();return false;}catch(const std::exception&e){std::cout<<"EXPECTED "<<e.what()<<'\n';return true;}}
void release(){std::lock_guard lock(gate_mutex);released=true;gate_changed.notify_all();}
struct Release{~Release(){release();}};
void arm(){std::lock_guard lock(gate_mutex);entered=released=false;gate=true;}
void await(){std::unique_lock lock(gate_mutex);if(!gate_changed.wait_for(lock,std::chrono::seconds(30),[]{return entered;}))throw std::runtime_error("Actual native GPU worker did not reach EGL acquisition barrier");}
void stroke(PaintedMesh&p,PaintSettings s,Vec3 at,float strength=1){s.front_only=false;Brush b;b.from=b.to=at;b.radius=.3f;b.normal={0,0,1};b.strength=strength;p.begin_stroke();p.stamp(b,s);check(p.end_stroke(),"Actual native paint stamps author source values");}
Mesh quad(){Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
std::shared_ptr<PaintedMesh> field(bool pbr){
 auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=std::make_shared<const Image>(Image{2,1,{30,90,180,210,180,30,90,41}});maps->normal.image=std::make_shared<const Image>(Image{1,1,{190,90,230,255}});
 auto p=PaintedMesh::from_maps(quad(),maps,64);PaintSettings ink;ink.color={.9f,.1f,.4f,.23f};stroke(*p,ink,{-.13f,.03f,0},.63f);ink.color={.1f,.8f,.3f,.83f};stroke(*p,ink,{.2f,.08f,0},.57f);
 if(pbr){std::vector<std::array<float,4>>colors(4,{.31f,.72f,.53f,.67f});p=SourceColorPreparationAccess::prepare(*p,colors,Material{},{}).paint;check(p->color_projection()&&!p->layers()[0].pbr_tiles.empty(),"Actual independent PBR layer preparation exists");}
 ink.channel=PaintChannel::Metallic;ink.metallic=.67f;stroke(*p,ink,{.02f,-.1f,0},.71f);ink.channel=PaintChannel::Roughness;ink.roughness=.21f;stroke(*p,ink,{.1f,.1f,0},.66f);
 ink.channel=PaintChannel::Emission;ink.emission_units=PaintEmissionUnits::Physical;ink.emission={10000,.0003f,7};stroke(*p,ink,{.03f,0,0},.00002f);
 ink.channel=PaintChannel::Mask;ink.mask=.23f;stroke(*p,ink,{-.1f,.13f,0},.81f);
 const auto first=p->active_layer();p->add_layer("Hidden untouched 日本語");ink.channel=PaintChannel::Color;ink.color={.2f,.4f,.8f,.45f};stroke(*p,ink,{0,0,0},.4f);p->layer_properties("Hidden untouched 日本語",false,0);p->select_layer(first);return p;
}
double linear(double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);}
double srgb(double v){return v<=.0031308?v*12.92:1.055*std::pow(v,1/2.4)-.055;}
int quantize(double v){return int(std::lround(std::clamp(v,0.,1.)*255));}
constexpr std::array<long double,15>weights={.034619L,.044859L,.055857L,.066833L,.076841L,.084894L,.090126L,.09194L,.090126L,.084894L,.076841L,.066833L,.055857L,.044859L,.034619L};
void oracle(const PaintedMesh&before,const PaintedMesh&after,float strength){
 const auto layer=before.active_layer();size_t different_alpha=0;
 for(uint32_t y=0;y<64;++y)for(uint32_t x=0;x<64;++x){const auto a=before.pixel(layer,x,y),b=after.pixel(layer,x,y);auto ap=before.pbr_pixel(layer,x,y),bp=after.pbr_pixel(layer,x,y);
  for(size_t c=4;c<14;++c)if(c!=9)check(a.data[c]==b.data[c],"All unrelated common components and independent mask remain exact");
  check(before.emission_pixel(layer,x,y)==after.emission_pixel(layer,x,y),"Physical/legacy emission RGB and subUNORM8 coverage stay bit exact");
  if(a.data[13]==0)continue;
  std::array<std::array<long double,4>,2>sum{};long double coverage=0,total=0;
  for(int offset=-7;offset<=7;++offset)for(int axis=0;axis<2;++axis){uint32_t u=uint32_t(std::clamp(int(x)+(axis==0?offset:0),0,63)),v=uint32_t(std::clamp(int(y)+(axis==1?offset:0),0,63));const auto q=before.pixel(layer,u,v);auto qp=before.pbr_pixel(layer,u,v);long double w=weights[offset+7]/2,alpha=q.data[9]/255.L;total+=w;coverage+=alpha*w;
   for(size_t c=0;c<4;++c){sum[0][c]+=(c<3?linear(q.data[c]/255.):q.data[c]/255.)*alpha*w;sum[1][c]+=(c<3?linear(qp[c]/255.):qp[c]/255.)*alpha*w;}}
  const long double old=a.data[9]/255.L,gate=a.data[13]/255.L*strength,next=old+(coverage/total-old)*gate;
  check(std::abs(int(b.data[9])-quantize(double(next)))<=1,"Independent native color coverage oracle");
  if(next>0)for(size_t c=0;c<4;++c){long double original=(c<3?linear(a.data[c]/255.):a.data[c]/255.)*old,original_pbr=(c<3?linear(ap[c]/255.):ap[c]/255.)*old;
   double value=double((original+(sum[0][c]/total-original)*gate)/next),pbr=double((original_pbr+(sum[1][c]/total-original_pbr)*gate)/next);
   check(std::abs(int(b.data[c])-quantize(c<3?srgb(value):value))<=1,"Independent common RGB/authored alpha oracle");check(std::abs(int(bp[c])-quantize(c<3?srgb(pbr):pbr))<=1,"Independent PBR RGB/authored alpha oracle");}
  different_alpha+=b.data[3]!=b.data[9];
 }
 check(different_alpha>0,"Authored alpha is demonstrably distinct from brush coverage");
 check(before.layers()[1].tiles==after.layers()[1].tiles&&before.layers()[1].pbr_tiles==after.layers()[1].pbr_tiles&&before.layers()[1].emission_tiles==after.layers()[1].emission_tiles,"Hidden zero-opacity layer retains exact COW owners");
 check(before.source_maps()==after.source_maps()&&before.color_projection()==after.color_projection()&&before.normal_projection()==after.normal_projection()&&before.mesh_owner()==after.mesh_owner(),"Original source/material/normal/geometry authorities remain exact owners");
}
void detached(bool software){
 for(bool pbr:{false,true}){auto before=field(pbr);auto p=before->snapshot();PaintBlurOptions options;options.strength=.63f;options.allow_software_validation=software;
  const auto original=p->encode();const auto result=p->blur_color_layer_gpu(options);check(result.changed&&result.tiles<=4&&result.downloaded_bytes==result.tiles*32*32*4*4*3,"Actual native operation returns bounded dirty tiles, not a whole-atlas per-frame composite");oracle(*before,*p,options.strength);const auto after=p->encode();
  check(p->undo()&&p->encode()==original,"One paint Undo restores every native byte");check(p->redo()&&p->encode()==after,"One paint Redo restores the entire accepted endpoint");
  auto canceled=before->snapshot();int callbacks=0;check(refuses([&]{canceled->blur_color_layer_gpu(options,[&]{return ++callbacks>12;});})&&canceled->encode()==original,"Cancelled staged mutation leaves original paint exact");
  options.maximum_working_bytes=1;check(refuses([&]{canceled->blur_color_layer_gpu(options);})&&canceled->encode()==original,"Memory refusal changes no native data");
 }
 auto sparse=std::make_shared<PaintedMesh>(quad(),512);PaintSettings ink;ink.front_only=false;ink.color={.8f,.1f,.2f,.4f};Brush brush;brush.from=brush.to={-.0712890625f,-.0107421875f,0};brush.radius=.005f;brush.normal={0,0,1};sparse->begin_stroke();sparse->stamp(brush,ink);check(sparse->end_stroke(),"Actual localized native stamp produces sparse production-grid data");
 auto old=sparse->snapshot();PaintBlurOptions options;options.allow_software_validation=software;auto result=sparse->blur_color_layer_gpu(options);
 check(result.changed&&result.tiles<256&&result.downloaded_bytes<512*512*4*4*3,"Sparse512 tool transfers only dirty tiles and halos, not full-atlas results");
 size_t preserved=0;for(size_t i=0;i<old->layers()[0].tiles.size();++i)preserved+=old->layers()[0].tiles[i]==sparse->layers()[0].tiles[i];
 check(preserved>=240,"Localized tool preserves at least240 original sparse COW tile owners");
 auto allocated=[](const auto&tiles){return std::count_if(tiles.begin(),tiles.end(),[](const auto&t){return bool(t);});};
 check(allocated(sparse->layers()[0].tiles)>allocated(old->layers()[0].tiles),"Halo transport creates genuine newly owned neighboring sparse tiles");
 Mesh islands;islands.positions={{-1,-.5f,0},{-.1f,-.5f,0},{-.1f,.5f,0},{-1,.5f,0},{.1f,-.5f,0},{1,-.5f,0},{1,.5f,0},{.1f,.5f,0}};islands.normals.assign(4,{0,0,1});islands.normals.insert(islands.normals.end(),4,{0,0,-1});islands.uv={{{0,0}},{{.5f,0}},{{.5f,1}},{{0,1}},{{.5f,0}},{{1,0}},{{1,1}},{{.5f,1}}};islands.indices={0,1,2,0,2,3,4,5,6,4,6,7};
 auto charts=std::make_shared<PaintedMesh>(std::move(islands),64);PaintSettings chart_ink;chart_ink.front_only=true;chart_ink.color={1,0,0,.4f};Brush wide;wide.from=wide.to={0,0,0};wide.radius=5;wide.normal={0,0,1};charts->begin_stroke();charts->stamp(wide,chart_ink);check(charts->end_stroke(),"Real first UV chart is authored by normal-aware native brush");chart_ink.color={0,0,1,.9f};wide.normal={0,0,-1};charts->begin_stroke();charts->stamp(wide,chart_ink);check(charts->end_stroke(),"Real second UV chart is authored independently");
 options.strength=1;charts->blur_color_layer_gpu(options);
 for(uint32_t y=0;y<64;++y)for(uint32_t x=0;x<64;++x){const auto&q=charts->pixel(charts->active_layer(),x,y);check(x<32?q.data[0]==255&&q.data[2]==0&&std::abs(int(q.data[3])-102)<=1:q.data[0]==0&&q.data[2]==255&&std::abs(int(q.data[3])-230)<=1,"Native indexed UV connectivity prevents cross-island RGB/alpha bleed");}
 auto original_maps=std::make_shared<PaintSourceMaps>();original_maps->color.image=std::make_shared<const Image>(Image{1,1,{210,90,30,255}});auto unsafe=capture_source_sampling(quad(),original_maps,64);
 check(!unsafe->gpu_color_blur_eligible()&&refuses([&]{unsafe->blur_color_layer_gpu(options);}),"Original/overlapping coordinate policy is explicitly guarded, never flattened");
}
void ready(Document&d){check(d.wait_asset(std::chrono::seconds(60))&&d.wait_mesh(std::chrono::seconds(60)),"Actual native worker queues drain");check(d.frame().state->error.empty(),"Actual native worker publishes no error");}
void software_document_refusal(){
 Scene scene;scene.active().mesh_active=true;scene.active().painted=field(false);const auto original=scene.encode();Document d;d.replace(scene);ready(d);
 PaintBlurOptions forbidden;forbidden.allow_software_validation=true;
 check(refuses([&]{d.blur_color_layer(forbidden);})&&d.copy_scene().encode()==original,"Document rejects the detached software-validation option before accepting authoring work");
 d.blur_color_layer();
 check(d.wait_asset(std::chrono::seconds(60))&&d.frame().state->error.find("no CPU fallback")!=std::string::npos,"Actual software-routed worker explicitly refuses the default hardware command");
 check(d.copy_scene().encode()==original&&!d.frame().state->dirty&&!d.frame().state->can_undo&&!d.frame().state->asset_busy,"Unsupported-device completion changes neither native fields nor clean/history state");
}
struct Controller{
 Document&doc;SpatialFrame frame;
 explicit Controller(Document&d):doc(d){frame.sequence=1;frame.time=1;frame.focused=frame.head_valid=true;frame.head.position={0,1.6f,0};frame.hands[1].tracked.valid=true;frame.hands[1].tracked.grip.position={0,1.48f,-.5f};send();}
 void send(float x=0,float y=0,bool click=false){++frame.sequence;frame.time+=.05;frame.hands[1].x=x;frame.hands[1].y=y;frame.hands[1].joystick=click;doc.submit(frame);check(doc.wait_input(),"Public synthetic controller frame reaches native publication fence");}
 void sector(int i){const float a=i*3.14159265358979323846f/6;send(std::cos(a),std::sin(a));send(std::cos(a),std::sin(a),true);send();}
 void open_filters(){doc.cancel_spatial_menu();send();send(0,0,true);send();sector(9);sector(3);sector(11);check(doc.frame().state->menu_page==6,"Existing controller brush-controls shortcut is preserved");sector(10);check(doc.frame().state->menu_page==32,"Controller discovers actual painting filter controls");}
};
void document(){
 Scene scene;scene.active().mesh_active=true;scene.active().painted=field(false);const auto original=scene.encode();Document d;d.replace(scene);ready(d);check(d.frame().state->can_blur_color_layer,"Desktop/XR availability advertises the genuine current field");
 d.blur_color_layer();auto save=d.save_async("gpu-layer-blur.dsculpt");save.get();ready(d);const auto after=d.copy_scene().encode();check(after!=original&&Scene::load("gpu-layer-blur.dsculpt").encode()==after,"Ordered Save/reopen captures the full GPU-accepted layer");check(d.undo()&&d.copy_scene().encode()==original,"One global Undo restores the exact pre-tool Scene");check(d.redo()&&d.copy_scene().encode()==after,"One global Redo restores the exact accepted Scene");
 d.replace(scene);ready(d);Release cleanup;arm();d.blur_color_layer();await();d.cancel_asset_work();release();check(d.wait_asset(std::chrono::seconds(60)),"Actual cancelled GPU worker terminates");check(d.copy_scene().encode()==original&&!d.frame().state->can_undo,"Actual cancel publishes no field or history");
 d.replace(scene);ready(d);arm();d.blur_color_layer();await();Scene replacement;replacement.active().name="Different generation";d.replace(replacement);release();check(d.wait_asset(std::chrono::seconds(60)),"Stale-generation GPU worker terminates");check(d.copy_scene().active().name=="Different generation"&&!d.frame().state->can_undo,"Old GPU completion cannot overwrite a replacement document");
 auto source=source_color_fixture::model();source.active().painted=source.active().painted->snapshot();PaintSettings ink;ink.color={.8f,.1f,.2f,.3f};stroke(*source.active().painted,ink,{0,0,0},.7f);source.validate();const auto before=source.snapshot();d.replace(source);ready(d);d.blur_color_layer();ready(d);const auto current=d.copy_scene();check(current.active().appearance==before.active().appearance&&current.active().deformation==before.active().deformation&&current.active().material_bindings==before.active().material_bindings,"Real imported source/rig/morph/typed material authorities survive the worker");
 check(spatial_action(32,1)==SpatialCommand::BlurColorLayer,"Actual XR command registry routes the blur action");
 Scene partitioned;std::array<TextureSetInitialField,2>fields;for(auto&f:fields)f.resolution=64;
 initialize_texture_sets(partitioned,partitioned.active().id,quad(),fields,std::array<TextureSetId,2>{0,1});select_texture_set(partitioned,partitioned.active().id,1);
 edit_texture_set(partitioned,partitioned.active().id,1,[](PaintedMesh&p,Material&){PaintSettings ink;ink.color={.8f,.1f,.2f,.4f};stroke(p,ink,{-.15f,.15f,0},.7f);return true;});
 const auto previous=partitioned.snapshot();const auto original_sets=partitioned.encode();d.replace(partitioned);ready(d);d.blur_color_layer();ready(d);const auto accepted=d.copy_scene();
 check(accepted.encode()!=original_sets&&detail::TextureSetPaintAccess::same_current(*accepted.active().painted,*previous.active().painted),"Actual selected-set tool changes only its requested field and leaves primary values exact");
 check(accepted.active().painted->mesh_owner()==accepted.active().texture_sets->at(1).painted->mesh_owner(),"Selected-set blur retains genuine shared geometry authority");
 check(d.undo()&&d.copy_scene().encode()==original_sets&&d.redo()&&d.copy_scene().encode()==accepted.encode(),"One set-field Undo/Redo retains exact material/member/layer endpoints");
 d.replace(scene);ready(d);Controller controller(d);
 arm();d.blur_color_layer();await();controller.open_filters();controller.sector(6);release();
 check(d.wait_asset(std::chrono::seconds(60))&&d.copy_scene().encode()==original&&!d.frame().state->can_undo,"Actual controller Filters Cancel stops the blocked GPU worker without publishing fields or history");
 controller.open_filters();controller.sector(1);ready(d);const auto xr=d.copy_scene().encode();check(xr!=original,"Public controller command accepts a genuine active-layer GPU mutation");
 d.cancel_spatial_menu();controller.send();controller.frame.hands[1].undo=true;controller.send();controller.frame.hands[1].undo=false;controller.send();check(d.copy_scene().encode()==original,"Actual controller Undo restores exact whole-layer endpoint");controller.frame.hands[1].redo=true;controller.send();controller.frame.hands[1].redo=false;controller.send();check(d.copy_scene().encode()==xr,"Actual controller Redo restores exact accepted GPU layer");
}
}
extern "C" EGLContext __real_eglCreateContext(EGLDisplay,EGLConfig,EGLContext,const EGLint*);
extern "C" EGLContext __wrap_eglCreateContext(EGLDisplay d,EGLConfig c,EGLContext shared,const EGLint*attributes){
 if(gate.exchange(false)){std::unique_lock lock(gate_mutex);entered=true;gate_changed.notify_all();gate_changed.wait(lock,[]{return released;});}
 return __real_eglCreateContext(d,c,shared,attributes);
}
int main(int argc,char**argv)try{const bool software=argc>1&&std::string(argv[1])=="software";if(argc>2){std::filesystem::create_directories(argv[2]);std::filesystem::current_path(argv[2]);}detached(software);if(software)software_document_refusal();else document();std::cout<<"PASS "<<checks<<" actual native GPU color blur controls\n";return 0;}catch(const std::exception&e){release();std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
