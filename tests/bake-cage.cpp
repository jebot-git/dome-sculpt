// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_bake_controls_fixture
#include "bake-controls.cpp"
#undef main
#include "dome/source-placement.hpp"
namespace {
unsigned cage_checks=0;
void require_cage(bool ok,const char *message){check(ok,message);++cage_checks;}
Mesh cage_quad(float left,float right,float low,float high,float z){
  Mesh m;m.positions={{left,low,z},{right,low,z},{right,high,z},{left,high,z}};
  m.normals.assign(4,{0,0,1});m.indices={0,1,2,0,2,3};m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};return m;
}
Scene cage_scene(uint32_t resolution=64){
  Scene s;const auto target=s.active().id;auto low=cage_quad(-.005f,.005f,-.01f,.01f,0);
  s.active().name="Target";s.active().mesh_active=true;s.active().painted=std::make_shared<PaintedMesh>(low,resolution);
  s.active().painted->add_layer("Keep editable mask");s.active().painted->layer_properties("Keep editable mask",false,.4f);
  s.add("Off-axis blue source",Volume{});s.active().mesh_active=true;s.active().visible=false;s.active().locked=true;
  s.active().material.color={0,0,1,1};s.active().painted=std::make_shared<PaintedMesh>(cage_quad(.014f,.026f,-.011f,.011f,.01f),64);
  s.add("ケージ / envelope",Volume{});s.active().mesh_active=true;s.active().visible=false;s.active().locked=true;
  for(auto &p:low.positions){p.x+=.06f;p.z+=.03f;}
  s.active().painted=std::make_shared<PaintedMesh>(low,64);s.select(target);s.validate();return s;
}
ObjectBakeSettings cage_settings(uint64_t cage=3){ObjectBakeSettings s;s.front_mm=s.back_mm=100;s.ao_samples=0;s.padding=0;s.cage_object=cage;return s;}
template<class F> bool rejected_cage(F fn){try{fn();return false;}catch(const std::exception &){return true;}}
void ready_cage(Document &d){require_cage(d.wait_asset()&&d.wait_mesh(),"Native cage worker and immutable mesh publication finish");}
void controller_cage(Driver &hand,Document &d,uint64_t id){
  hand.sector(0);const auto c=d.frame().state->controls;
  require_cage(c.parameter==VrParameter::BakeSource&&c.components==2,"Source and explicit cage share two accessible controller rows");
  const auto &ids=d.frame().state->bake_cages;const auto found=std::find(ids.begin(),ids.end(),id);
  require_cage(found!=ids.end(),"Controller candidate list retains the explicit cage identity");
  hand.value(ids.size()<2?0:float(found-ids.begin())/float(ids.size()-1),1);
}
}
#ifndef DOME_BAKE_CAGE_HELPERS_ONLY
int main(int argc,char **argv)try{
  if(argc==3&&std::string(argv[1])=="--ui-fixture"){cage_scene(128).save(argv[2]);return 0;}
  if(argc==3&&std::string(argv[1])=="--inspect"){
    auto s=Scene::load(argv[2]);std::cout<<nlohmann::json{{"scene",s.encode()},{"baked",bool(s.active().painted->baked())}}.dump()<<'\n';return 0;
  }
  const auto out=std::filesystem::path(argc>1?argv[1]:"bake-cage");std::filesystem::create_directories(out);
  auto s=cage_scene();const auto before=s.encode();auto automatic=s;auto settings=cage_settings(0);
  require_cage(rejected_cage([&]{bake_object(automatic,2,settings);})&&automatic.encode()==before,
    "Independent off-axis source cannot be reached by ordinary normal rays");
  settings=cage_settings();auto report=bake_object(s,2,settings);
  require_cage(report.projected==4096&&!report.missed,"Edited cage redirects every projection ray to the independently placed off-axis source");
  const auto endpoint=s.encode();auto image=s.active().painted->baked()->maps.color;
  bool blue=true;for(size_t i=0;i<image.rgba.size();i+=4)blue&=image.rgba[i]==0&&image.rgba[i+1]==0&&image.rgba[i+2]==255&&image.rgba[i+3]==255;
  require_cage(blue,"Every baked color pixel matches the independent solid-blue source oracle");
  require_cage(endpoint["objects"][1]==before["objects"][1]&&endpoint["objects"][2]==before["objects"][2]&&
    endpoint.at("objects").at(0).at("painted").at("layers")==before.at("objects").at(0).at("painted").at("layers"),
    "Complete hidden locked source and cage plus target editable paint remain exact");
  auto file=out/"cage.dsculpt";s.save(file);require_cage(Scene::load(file).encode()==endpoint,"Editable cage geometry and baked target survive native save and restart");
  auto transformed=cage_scene();auto raw=AffineTransform{}.matrix();raw[0]=-2;raw[4]=.35;raw[10]=.7;raw[12]=.03;
  auto affine=AffineTransform(raw);auto cage=transformed.object(3).painted->mesh();
  for(auto &p:cage.positions){auto q=affine.inverse().point({p.x,p.y,p.z});p={float(q[0]),float(q[1]),float(q[2])};}
  transformed.object(3).painted=std::make_shared<PaintedMesh>(cage,64);transformed.object(3).affine_edit=std::make_shared<const NativeAffineEdit>(affine);
  auto transformed_before=transformed.encode();auto affine_report=bake_object(transformed,2,settings);
  require_cage(affine_report.projected==4096&&!affine_report.missed&&transformed.encode()["objects"][2]==transformed_before["objects"][2],
    "Reflected sheared cage uses its complete displayed affine placement without rewriting its mesh");
  auto posed_cage=cage_scene();auto base=posed_cage.active().painted->mesh();
  auto moved=posed_cage.object(3).painted->mesh().positions;
  auto deformation=std::make_shared<DeformationState>(base);deformation->morphs.emplace(base);
  deformation->morphs->set_target(base,"Displayed cage envelope",moved);deformation->morph_weights={1};
  posed_cage.object(3).painted=std::make_shared<PaintedMesh>(base,64);posed_cage.set_deformation(3,*deformation);
  const auto posed_before=posed_cage.encode();auto posed_report=bake_object(posed_cage,2,settings);
  require_cage(posed_report.projected==4096&&!posed_report.missed&&posed_cage.encode()["objects"][2]==posed_before["objects"][2],
    "Cage rays use the actual displayed morph while exact REST geometry and deformation remain editable");
  auto prepared=cage_scene();auto normal=settings;normal.prepare_normals=true;auto prepared_report=bake_object(prepared,2,normal);
  require_cage(prepared_report.projected==4096&&prepared.active().painted->normal_projection(),
    "Explicit normal preparation transports cage IDs into the real split low bake basis");
  for(int kind=0;kind<5;++kind){auto invalid=cage_scene();auto option=settings;
    if(kind==0)option.cage_object=invalid.active().id;
    if(kind==1)option.cage_object=999999;
    if(kind>=2){auto m=invalid.object(3).painted->mesh();
      if(kind==2)std::swap(m.indices[0],m.indices[1]);
      if(kind==3)for(auto &p:m.positions)p.z=-.03f;
      if(kind==4)for(auto &p:m.positions)p.z=.2f;
      invalid.object(3).painted=std::make_shared<PaintedMesh>(m,64);
    }
    const auto original=invalid.encode();require_cage(rejected_cage([&]{bake_object(invalid,2,option);})&&invalid.encode()==original,
      "Self/missing/reordered/inward/out-of-bounds cage refuses without changing the complete scene");
  }
  auto cancelled=cage_scene();size_t polls=0;require_cage(rejected_cage([&]{bake_object(cancelled,2,settings,[&]{return ++polls>12;});})&&cancelled.encode()==before,
    "Cancellation during actual cage/bake work leaves the complete source and target unchanged");
  Document d;d.replace(cage_scene());ready_cage(d);Driver hand(d);hand.open();controller_cage(hand,d,3);hand.ray(.25f,.95f);
  require_cage(!d.frame().state->bake_settings.cage_object&&!d.frame().state->dirty&&d.copy_scene().encode()==before,
    "Controller Cancel discards the draft cage choice without adding history");
  controller_cage(hand,d,3);hand.confirm();require_cage(d.frame().state->bake_settings.cage_object==3&&!d.frame().state->dirty&&d.copy_scene().encode()==before,
    "Controller confirmation retains only the explicit read-only cage configuration");
  controller_cage(hand,d,0);hand.f.focused=false;hand.send();require_cage(d.frame().state->controls.parameter==VrParameter::None&&d.frame().state->bake_settings.cage_object==3&&d.copy_scene().encode()==before,
    "Tracking focus loss discards only the unconfirmed cage draft");
  d.bake_from_object(2,settings);auto save=d.save_async(out/"native-cage.dsculpt");save.get();ready_cage(d);
  require_cage(d.frame().state->error.empty()&&d.copy_scene().active().painted->baked()&&Scene::load(out/"native-cage.dsculpt").encode()==d.copy_scene().encode(),
    "Native worker cage bake and its ordered Save publish one complete consistent endpoint");
  auto native=d.copy_scene().encode();require_cage(d.undo()&&d.copy_scene().encode()==before&&!d.frame().state->can_undo&&d.redo()&&d.copy_scene().encode()==native,
    "Native custom cage baking is exactly one global Undo/Redo transaction");ready_cage(d);
  auto missing=settings;missing.cage_object=999999;d.bake_from_object(2,missing);ready_cage(d);
  require_cage(!d.frame().state->error.empty()&&d.frame().state->bake_settings.cage_object==999999&&d.copy_scene().encode()==native,
    "Missing explicit native cage fails visibly and cannot silently fall back to normal projection");
  d.replace(cage_scene());ready_cage(d);require_cage(!d.frame().state->bake_settings.cage_object,
    "Document replacement clears document-local cage identity before reused IDs can alias another mesh");
  std::cout<<"PASS "<<cage_checks<<" custom cage workflow checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
#endif
