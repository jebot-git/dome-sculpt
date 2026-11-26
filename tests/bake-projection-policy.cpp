// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/bake-recipe.hpp"
#include "dome/baked-emission.hpp"
#include "dome/source-placement.hpp"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
size_t checks=0;
void check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<std::endl;}
template<class F> bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}
Mesh quad(float low=-.2f,float high=.2f,float z=0,bool back=false){Mesh m;m.positions={{low,-.2f,z},{high,-.2f,z},{high,.2f,z},{low,.2f,z}};m.normals.assign(4,back?Vec3{0,0,-1}:Vec3{0,0,1});m.indices=back?std::vector<uint32_t>{0,2,1,0,3,2}:std::vector<uint32_t>{0,1,2,0,2,3};m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};return m;}
struct Fixture{Scene scene;uint64_t target,full,half,miss,back;Fixture(){target=scene.active().id;scene.active().painted=std::make_shared<PaintedMesh>(quad(),64);scene.active().mesh_active=true;auto add=[&](const char*name,Mesh m,std::array<float,4> color){auto id=scene.add(name,Volume{});auto &o=scene.object(id);o.mesh_active=true;o.visible=false;o.material.color=color;o.painted=std::make_shared<PaintedMesh>(std::move(m),64);return id;};full=add("Full red",quad(-.2f,.2f,.05f),{1,0,0,1});half=add("Half blue",quad(-.2f,0,.05f),{0,0,1,1});miss=add("No projection",quad(.5f,.8f,.05f),{0,1,0,1});back=add("Reversed green",quad(-.2f,.2f,.05f,true),{0,1,0,1});scene.select(target);}};
void ready(Document &d){if(!d.wait_asset()||!d.wait_mesh())throw std::runtime_error("Bake policy worker timeout");}
bool same_pixel(const Image &a,const Image &b,size_t at){return std::equal(a.rgba.begin()+at*4,a.rgba.begin()+at*4+4,b.rgba.begin()+at*4);}
struct Driver {Document &doc;SpatialFrame f;explicit Driver(Document&d):doc(d){f.focused=f.head_valid=true;f.head.position={0,1.6f,0};f.hands[1].tracked.valid=true;f.hands[1].tracked.grip.position={0,1.48f,-.5f};send();}void send(float x=0,float y=0,bool click=false,float pressure=0){++f.sequence;f.time+=.05;auto &h=f.hands[1];h.x=x;h.y=y;h.joystick=click;h.tracked.trigger=pressure;doc.submit(f);if(!doc.wait_input())throw std::runtime_error("Policy input timeout");}void sector(int i){float a=i*3.14159265358979323846f/6;send(std::cos(a),std::sin(a));send(std::cos(a),std::sin(a),true);send();}void open(){send();send(0,0,true);send();sector(8);sector(9);sector(6);sector(11);}void point(float x,float y){auto s=doc.frame().state;auto&h=f.hands[1];h.tracked.aimValid=true;h.tracked.aim.position=ob::transform(s->panel,{(x/640-.5f)*.6f,(.5f-y/640)*.6f,.25f});h.tracked.aim.orientation=s->panel.orientation;send(0,0,false,.8f);send();}void confirm(bool accept){point(accept?470:170,609);}};
void matching_source_bake() {
  Fixture f;
  auto &source=f.scene.object(f.full);
  source.transform.position.x=.03f;
  source.material.emissive={8.f,.5f,0.f};
  source.material.metallic=.6f;
  source.material.roughness=.4f;
  auto detail=quad(-.2f,.2f,.02f);
  detail.normals[1]=detail.normals[2]=normalized(Vec3{.2f,0,1});
  source.painted=std::make_shared<PaintedMesh>(std::move(detail),64);
  auto target=f.scene.active().painted;
  Brush brush;
  brush.from=brush.to={0,0,0};
  brush.radius=.12f;
  PaintSettings ink;
  ink.front_only=false;
  ink.color={.15f,.7f,.4f,.6f};
  target->begin_stroke();target->stamp(brush,ink);
  check(target->end_stroke(),"Matching fixture has genuine editable paint under the operation");
  const auto original=f.scene.encode();
  ObjectBakeSettings settings;
  settings.ao_samples=0;
  settings.padding=0;
  check(refuses([&]{bake_object(f.scene,f.full,settings);})&&f.scene.encode()==original,
        "A displaced matching mesh still misses in ordinary strict ray mode");
  settings.corresponding_topology=true;
  auto invalid=settings;
  invalid.cage_object=f.half;
  check(refuses([&]{bake_object(f.scene,f.full,invalid);})&&f.scene.encode()==original,
        "Matching topology rejects an explicit cage without changing the document");
  invalid=settings;
  invalid.front_mm=5;
  check(refuses([&]{bake_object(f.scene,f.full,invalid);})&&f.scene.encode()==original,
        "Matching texels beyond the front bound cannot publish a strict bake");
  check(refuses([&]{bake_object(f.scene,f.back,settings);})&&f.scene.encode()==original,
        "Equal triangle counts with different index order cannot publish a matching bake");
  bool cancel=false;
  check(refuses([&]{bake_object(f.scene,f.full,settings,[&]{return cancel;},
                               [&](float progress){cancel=progress>.8f;});})&&
            cancel&&f.scene.encode()==original,
        "Cancellation after projected raster work preserves all input owners");

  auto endpoint=f.scene;
  const auto report=bake_object(endpoint,f.full,settings);
  const auto &baked=*endpoint.active().painted->baked();
  check(report.projected==4096&&!report.missed&&!report.rays,
        "Matching samples cover the whole target with zero projection rays");
  check(bool(baked.emission_field),"The matching bake retains its FLOAT emission owner");
  const auto point=baked.emission_field->sample(.37f,.62f);
  check(std::abs(point.coverage-1.f)<1e-6f&&point.resolve({},1)==std::array<float,3>{8.f,.5f,0.f},
        "Matching samples carry physical emission independently of color and coverage");
  check(baked.maps.color.rgba[0]==255&&baked.maps.color.rgba[2]==0&&
            baked.maps.metal_rough.rgba[1]==102&&baked.maps.metal_rough.rgba[2]==153&&
            baked.maps.normal.rgba[4*32+0]>baked.maps.normal.rgba[0],
        "A source normal gradient and independent base color and ORM survive the bake");
  auto ordinary=endpoint;
  ordinary.active().painted=f.scene.active().painted;
  ordinary.active().bake_recipe.reset();
  check(ordinary.encode()==original,"Replacing the result field and recipe restores the exact source scene");
  const auto &base=target->layers().front(), &preserved=endpoint.active().painted->layers().front();
  check(base.id==preserved.id&&base.name==preserved.name&&base.opacity==preserved.opacity&&
            base.visible==preserved.visible&&base.tiles==preserved.tiles,
        "The native bake retains the original editable paint tile owners");

  const auto recipe=endpoint.active().bake_recipe->encode();
  auto malformed=recipe;
  malformed["settings"]["corresponding_topology"]=false;
  check(recipe["version"]==2&&ObjectBakeRecipe::decode(recipe)->encode()==recipe&&
            refuses([&]{ObjectBakeRecipe::decode(malformed);}),
        "Matching recipes round-trip with an explicit version and reject an invalid mode flag");
  auto rays=settings;
  rays.corresponding_topology=false;
  const auto legacy=capture_bake_recipe(f.scene,f.full,rays)->encode();
  check(legacy["version"]==1&&ObjectBakeRecipe::decode(legacy)->encode()==legacy,
        "Ordinary ray recipes reencode in their original version");

  auto with_ao=f.scene;
  auto ao=settings;
  ao.ao_samples=2;
  check(bake_object(with_ao,f.full,ao).rays==2*report.projected,
        "Matching mode counts ambient occlusion rays independently of source sampling");

  Document doc;
  doc.replace(f.scene);
  ready(doc);
  const auto filename=std::filesystem::temp_directory_path()/
    ("dome-matching-bake-"+std::to_string(getpid())+".dsculpt");
  struct Cleanup {
    std::filesystem::path filename;
    ~Cleanup(){std::error_code error;std::filesystem::remove(filename,error);}
  } cleanup{filename};
  doc.bake_from_object(f.full,settings);
  doc.save_async(filename).get();
  ready(doc);
  check(doc.frame().state->error.empty()&&Scene::load(filename).encode()==endpoint.encode()&&
            doc.undo()&&doc.copy_scene().encode()==original&&
            doc.redo()&&doc.copy_scene().encode()==endpoint.encode(),
        "Ordered Save, fresh reopen, Undo and Redo preserve exact matching Bake endpoints");

  Fixture partial;
  auto slope=quad(-.2f,.2f,.02f);
  slope.positions[1].z=slope.positions[2].z=.09f;
  partial.scene.object(partial.half).painted=std::make_shared<PaintedMesh>(std::move(slope),64);
  bake_object(partial.scene,partial.full,rays);
  const auto before=partial.scene.active().painted->baked();
  settings.allow_partial=true;
  const auto part=bake_object(partial.scene,partial.half,settings);
  const auto after=partial.scene.active().painted->baked();
  check(part.projected&&part.preserved_misses&&part.preserved_misses==part.missed&&!part.rays&&
            after->maps.color.rgba[2]==255&&same_pixel(after->maps.color,before->maps.color,63),
        "Partial matching preserves the previous bake wherever a source sample exceeds its bounds");
}

void affine_volume_source() {
  Scene scene;
  const auto target=scene.active().id;
  Volume volume(.01f);
  Brush brush;brush.radius=.2f;
  volume.begin_stroke();volume.stamp(brush);
  check(volume.end_stroke(),"The affine donor contains an actual sculpt volume");
  const auto donor=scene.add("Hidden affine sculpt",std::move(volume));
  auto &source=scene.object(donor);
  source.visible=false;
  source.material.color={.9f,.15f,.05f,1};
  const std::array<double,16> placement{
      -1.3,0,0,0, .25,.8,0,0, 0,0,1.2,0, 2,1,.4,1};
  source.affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(placement));
  scene.select(target);
  auto patch=quad(-.035f,.035f,.65f);
  for(auto &vertex:patch.positions){vertex.x+=2;vertex.y=1+vertex.y*.175f;}
  scene.active().mesh_active=true;
  scene.active().painted=std::make_shared<PaintedMesh>(std::move(patch),64);
  ObjectBakeSettings settings;settings.front_mm=2;settings.back_mm=50;
  settings.ao_samples=0;settings.padding=0;
  Document doc;doc.replace(scene);ready(doc);
  check(doc.frame().state->bake_sources==std::vector<uint64_t>{donor},
        "The native chooser offers the translated reflected sculpt as a hidden donor");
  const auto before=doc.copy_scene().encode();
  doc.bake_from_object(donor,settings);ready(doc);
  if(!doc.frame().state->error.empty())throw std::runtime_error(doc.frame().state->error);
  const auto baked_scene=doc.copy_scene();
  const auto baked=baked_scene.active().painted->baked();
  check(baked&&std::count(baked->coverage.begin(),baked->coverage.end(),255)==4096&&
        baked->maps.color.rgba[0]>235&&baked->maps.color.rgba[2]<70,
        "World-metric Bake projects all front-facing texels onto the reflected sculpt and uses its material");
  check(baked_scene.object(donor).volume->encode()==scene.object(donor).volume->encode()&&
        doc.undo()&&doc.copy_scene().encode()==before,
        "Affine volume Bake preserves the original sculpt data and exact Undo");
}

}
int main(int argc,char**argv)try {
 if(argc==3&&std::string(argv[1])=="--inspect"){std::cout<<Scene::load(argv[2]).encode().dump()<<'\n';return 0;}
 Fixture f;ObjectBakeSettings options;options.front_mm=options.back_mm=100;options.ao_samples=0;options.padding=0;
 auto first=bake_object(f.scene,f.full,options);check(first.projected==4096&&!first.missed,"Default strict native bake projects the complete source");
 const bool topology_fixture=argc==3&&std::string_view(argv[1])=="--ui-topology-fixture";
 if((argc==3&&std::string_view(argv[1])=="--ui-fixture")||topology_fixture){
   f.scene.erase(f.full);f.scene.erase(f.miss);f.scene.erase(f.back);
   auto &source=f.scene.object(f.half);
   source.painted=std::make_shared<PaintedMesh>(quad(-.2f,topology_fixture?.2f:0,.02f),64);
   if(topology_fixture)source.transform.position.x=.03f;
   f.scene.select(f.target);f.scene.save(argv[2]);return 0;
 }
 matching_source_bake();
 affine_volume_source();

 auto before=f.scene.encode();auto original=f.scene.active().painted->baked();auto layers=f.scene.active().painted->encode()["layers"];
 check(refuses([&]{bake_object(f.scene,f.half,options);})&&f.scene.encode()==before,"Strict projection still refuses partial hits without changing any target or source");
 options.allow_partial=true;auto partial=bake_object(f.scene,f.half,options);auto result=f.scene.active().painted->baked();
 check(partial.projected==2048&&partial.missed==2048&&partial.preserved_misses==2048,"Explicit partial mode reports exact projected, missed and retained texel counts");
 bool untouched=true,changed=true;for(size_t y=0;y<64;++y)for(size_t x=0;x<64;++x){size_t at=y*64+x;if(x>=32){for(auto images:{std::pair{&result->maps.color,&original->maps.color},std::pair{&result->maps.metal_rough,&original->maps.metal_rough},std::pair{&result->maps.emission,&original->maps.emission},std::pair{&result->maps.normal,&original->maps.normal}})untouched&=same_pixel(*images.first,*images.second,at);untouched&=result->coverage[at]==original->coverage[at];}else changed&=result->maps.color.rgba[at*4]==0&&result->maps.color.rgba[at*4+2]==255;}
 check(untouched&&changed,"All missed color/ORM/emission/normal/coverage bytes survive while actual hits update");
 check(f.scene.active().painted->encode()["layers"]==layers,"Partial rebake leaves every editable paint layer exact");
 auto endpoint=f.scene.encode();check(refuses([&]{bake_object(f.scene,f.miss,options);})&&f.scene.encode()==endpoint,"A completely missed source refuses explicitly instead of installing an empty or fake-success bake");
 options.allow_partial=false;check(refuses([&]{bake_object(f.scene,f.back,options);})&&f.scene.encode()==endpoint,"Backfaces remain refused by default");
 options.allow_backfaces=true;auto reversed=bake_object(f.scene,f.back,options);check(reversed.projected==4096&&!reversed.missed,"Explicit backface option projects the requested reversed surface");
 Fixture native;options={};options.front_mm=options.back_mm=100;options.ao_samples=0;options.padding=0;bake_object(native.scene,native.full,options);Document d;d.replace(native.scene);ready(d);auto baseline=d.copy_scene().encode();
 d.bake_from_object(native.half,options);ready(d);check(!d.frame().state->error.empty()&&!d.frame().state->can_undo&&d.copy_scene().encode()==baseline,"Document strict refusal preserves history and complete source/target authority");
 options.allow_partial=true;d.bake_from_object(native.half,options);ready(d);check(d.frame().state->error.empty()&&d.frame().state->message.find("2048 missed")!=std::string::npos,"Partial Document bake publishes normal status with visible miss counts");auto after=d.copy_scene().encode();
 check(d.undo()&&d.copy_scene().encode()==baseline&&!d.frame().state->can_undo&&d.redo()&&d.copy_scene().encode()==after,"Accepted partial bake is exactly one native Undo/Redo transaction");ready(d);
 Document xr;xr.replace(native.scene);ready(xr);auto untouched_scene=xr.copy_scene().encode();Driver hand(xr);hand.open();check(xr.frame().state->controls.parameter==VrParameter::BakeProjection&&xr.frame().state->controls.components==3,"Controller baking page exposes partial, backface and matching topology policies");
 hand.point(520,240);hand.point(520,320);hand.point(520,400);hand.confirm(false);
 check(!xr.frame().state->bake_settings.allow_partial&&!xr.frame().state->bake_settings.allow_backfaces&&
           !xr.frame().state->bake_settings.corresponding_topology&&xr.copy_scene().encode()==untouched_scene,
       "XR Cancel discards all projection choices without changing the document");
 hand.sector(11);hand.point(520,240);hand.point(520,320);hand.point(520,400);hand.confirm(true);
 check(xr.frame().state->bake_settings.allow_partial&&xr.frame().state->bake_settings.allow_backfaces&&
           xr.frame().state->bake_settings.corresponding_topology&&!xr.frame().state->dirty&&!xr.frame().state->can_undo,
       "XR confirmation changes only explicit bake configuration");
 hand.sector(11);hand.point(100,240);hand.f.focused=false;hand.send();check(xr.frame().state->controls.parameter==VrParameter::None&&xr.frame().state->bake_settings.allow_partial&&xr.copy_scene().encode()==untouched_scene,"OpenXR-style focus loss discards unconfirmed policy while retaining prior choices and source");
 std::cout<<checks<<" native bake projection policy checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
