// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_mtoon_fixture
#include "mtoon-render.cpp"
#undef main
#include "dome/document.hpp"
#include <fstream>

namespace {
void settled_clear(Document &d){if(!d.wait_asset()||!d.wait_mesh()||!d.frame().state->error.empty())throw std::runtime_error("Clear probe: "+d.frame().state->error);}
void capture_clear(const std::filesystem::path &p,const std::vector<uint8_t> &data){auto bytes=Image{96,96,data}.png();std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());}
}
int main(int argc,char **argv)try{
 if(argc!=3)return 2;const std::filesystem::path source=argv[1],out=argv[2];std::filesystem::create_directories(out);
 NativeGraphics graphics;Fbo fbo;SurfaceRenderer renderer;int maximum=0;
 for(int variant:{0,7}){
  auto original=Scene::load(source/("original-"+std::to_string(variant)+".dsculpt"));
  auto converted=Scene::load(source/("converted-"+std::to_string(variant)+".dsculpt"));
  original.active().transform.scale=converted.active().transform.scale=5;
  Document before,after;before.replace(original);after.replace(converted);before.shading(false);after.shading(false);settled_clear(before);settled_clear(after);
  before.clear_bake();after.clear_bake();settled_clear(before);settled_clear(after);
  auto a=before.copy_scene(),b=after.copy_scene();fbo.bind();auto x=draw(renderer,before.frame().surface);fbo.bind();auto y=draw(renderer,after.frame().surface);
  auto error=delta(x,y);maximum=std::max(maximum,error);capture_clear(out/("raw-clear-"+std::to_string(variant)+".png"),x);capture_clear(out/("converted-clear-"+std::to_string(variant)+".png"),y);
  std::cout<<"CLEAR variant="<<variant<<" raw_has_bake="<<bool(a.active().painted->baked())<<" converted_has_bake="<<bool(b.active().painted->baked())<<" raw_projection="<<bool(a.active().painted->normal_projection())<<" converted_projection="<<bool(b.active().painted->normal_projection())<<" pixel_error="<<error<<std::endl;
 }
 auto partial=Scene::load(source/"converted-0.dsculpt");partial.active().transform.scale=partial.object(2).transform.scale=5;
 Mesh high;high.positions={{0,0,.02f},{.3f,0,.02f},{0,.3f,.02f}};high.normals.assign(3,{.3f,.4f,std::sqrt(.75f)});high.uv={{{0,0}},{{1,0}},{{0,1}}};high.indices={0,1,2};
 partial.object(2).painted=std::make_shared<PaintedMesh>(high,64);
 Document rebake;rebake.replace(partial);rebake.shading(false);settled_clear(rebake);fbo.bind();const auto prior=draw(renderer,rebake.frame().surface);
 ObjectBakeSettings settings;settings.ao_samples=0;settings.padding=0;settings.allow_partial=true;settings.front_mm=settings.back_mm=1000;
 rebake.bake_from_object(2,settings);settled_clear(rebake);fbo.bind();const auto next=draw(renderer,rebake.frame().surface);
 const auto repeat_error=delta(prior,next);capture_clear(out/"before-same-normal-partial-rebake.png",prior);capture_clear(out/"after-same-normal-partial-rebake.png",next);
 std::cout<<"PARTIAL_REBAKE same_source_direction pixel_error="<<repeat_error<<std::endl;
 check(repeat_error<=4,"Partial rebake of the same normal field preserves projected and missed regions after conversion");
 check(maximum<=4,"Clearing Bake before or after explicit conversion removes the same historical detail under the unchanged image bound");return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
