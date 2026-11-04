// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_mtoon_render_main
#include "mtoon-render.cpp"
#undef main
#include "dome/document.hpp"
#include "baked-normal-field.hpp"
#include <fstream>
namespace {
void settled_layers(Document &d){
  if(!d.wait_asset()||!d.wait_mesh()||!d.frame().state->error.empty())throw std::runtime_error("Layered native worker: "+d.frame().state->error);
}
void capture_layers(const std::filesystem::path &p,const std::vector<uint8_t> &v){
  auto bytes=Image{96,96,v}.png();std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
}
}
int main(int argc,char **argv)try{
  if(argc!=3)return 2;const std::filesystem::path source=argv[1],out=argv[2];std::filesystem::create_directories(out);
  NativeGraphics graphics;Fbo fbo;SurfaceRenderer renderer;
  for(int variant:{0,7}){
    auto raw=Scene::load(source/("original-"+std::to_string(variant)+".dsculpt"));raw.active().transform.scale=5;
    Document before,after;before.replace(raw);after.replace(raw);before.shading(false);after.shading(false);settled_layers(before);settled_layers(after);
    fbo.bind();const auto original=draw(renderer,before.frame().surface);
    after.convert_baked_normals();settled_layers(after);auto converted=after.copy_scene();
    const auto origin=converted.active().material_bindings->canonical_attributes;
    check(origin&&!converted.active().painted->normal_projection()&&!converted.active().material_bindings->normal_preparation&&
      converted.active().painted->baked()->normal_field->origin()==origin,"Real conversion keeps baked detail separate from intrinsic planes and authentic attribute provenance");
    fbo.bind();const auto current=draw(renderer,after.frame().surface);const auto convert_error=delta(original,current);
    capture_layers(out/("converted-"+std::to_string(variant)+".png"),current);
    check(convert_error<=4,"Actual converted native frame remains within unchanged image bound");
    before.clear_bake();after.clear_bake();settled_layers(before);settled_layers(after);
    fbo.bind();const auto a=draw(renderer,before.frame().surface);fbo.bind();const auto b=draw(renderer,after.frame().surface);
    capture_layers(out/("raw-clear-"+std::to_string(variant)+".png"),a);capture_layers(out/("converted-clear-"+std::to_string(variant)+".png"),b);
    check(delta(a,b)<=4,"Actual worker Clear Bake removes converted detail and restores genuine original shading");
    auto cleared=after.copy_scene();check(!cleared.active().painted->normal_projection()&&!cleared.active().painted->baked()&&
      cleared.active().material_bindings->canonical_attributes==origin&&origin->before_paint()->baked(),"Clear drops active field but retains complete original normal/material/deformation provenance");
    after.undo();settled_layers(after);fbo.bind();check(delta(current,draw(renderer,after.frame().surface))==0,"Clear Undo restores exact converted shading");
    after.redo();settled_layers(after);fbo.bind();check(delta(b,draw(renderer,after.frame().surface))==0,"Clear Redo restores exact intrinsic shading");
    std::cout<<"CLEAR variant="<<variant<<" conversion_error="<<convert_error<<" clear_error="<<delta(a,b)<<'\n';
  }
  for(bool clear_first:{false,true}){
    auto raw=Scene::load(source/"original-0.dsculpt");raw.active().transform.scale=raw.object(2).transform.scale=5;
    Mesh high;high.positions={{0,0,.02f},{.3f,0,.02f},{0,.3f,.02f}};high.normals.assign(3,{.3f,.4f,std::sqrt(.75f)});
    high.uv={{{0,0}},{{1,0}},{{0,1}}};high.indices={0,1,2};raw.object(2).painted=std::make_shared<PaintedMesh>(high,64);
    Document legacy,converted;legacy.replace(raw);converted.replace(raw);legacy.shading(false);converted.shading(false);settled_layers(legacy);settled_layers(converted);
    converted.convert_baked_normals();settled_layers(converted);
    if(clear_first){legacy.clear_bake();converted.clear_bake();settled_layers(legacy);settled_layers(converted);}
    fbo.bind();const auto prior=draw(renderer,converted.frame().surface);
    ObjectBakeSettings settings;settings.ao_samples=0;settings.padding=0;settings.allow_partial=true;settings.front_mm=settings.back_mm=1000;
    legacy.bake_from_object(2,settings);converted.bake_from_object(2,settings);settled_layers(legacy);settled_layers(converted);
    fbo.bind();const auto a=draw(renderer,legacy.frame().surface);fbo.bind();const auto b=draw(renderer,converted.frame().surface);
    capture_layers(out/(clear_first?"clear-then-rebake.png":"same-normal-partial-rebake.png"),b);
    check(delta(a,b)<=4,"Actual partial Bake agrees with legacy original in projected and missed regions, including after Clear");
    if(!clear_first)check(delta(prior,b)<=4,"Partial rebake of the same direction preserves the actual previous field");
    const auto saved=converted.copy_scene();const auto &field=saved.active().painted->baked()->normal_field;
    check(field&&field->intrinsic_baseline()==clear_first&&field->replacements()==1,"Rebake uses current resolved detail or genuine cleared intrinsic baseline without resurrecting old bake");
    converted.clear_bake();settled_layers(converted);
    std::cout<<"REBAKE clear_first="<<clear_first<<" legacy_error="<<delta(a,b)<<" prior_error="<<delta(prior,b)<<'\n';
  }
  return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
