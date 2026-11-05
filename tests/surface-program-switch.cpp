// SPDX-License-Identifier: GPL-3.0-or-later
#define main established_mtoon_render_fixture
#include "mtoon-render.cpp"
#undef main
int main(int argc,char **argv)try{
 NativeGraphics graphics;const std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<'\n';
 check(argc<2||std::string_view(argv[1])!="--hardware"||(device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos),"Program transitions execute on requested hardware");
 Fbo target;target.bind();SurfaceRenderer renderer;
 auto plain=snapshot();plain->ranges[0].material.color={.2f,.7f,.4f,1};
 const auto geometry=std::make_shared<const int>(1);plain->geometry_identity=geometry;
 const auto original=draw(renderer,plain);const auto uploads=renderer.buffer_stats().geometry_uploads;
 for(bool legacy:{false,true}){
  auto material=typed(legacy);material.parameters.cull=MToonCull::Off;material.parameters.base_color={.8f,.2f,.1f,1};material.parameters.shade_color={.3f,.1f,.2f};
  auto toon=std::make_shared<MeshSnapshot>(*plain);toon->ranges[0].material=white();toon->ranges[0].appearance=appearance(material);
  const auto typed_pixels=draw(renderer,toon);
  check(close(at(typed_pixels),expected(material,{})),"Lazy typed shader retains independent V0/V1 lighting");
  check(draw(renderer,plain)==original,"Typed to PBR transition restores exact original pixels");
  auto mixed=std::make_shared<MeshSnapshot>(*plain);mixed->ranges[0].count=3;mixed->ranges.push_back(toon->ranges[0]);mixed->ranges[1].object=2;mixed->ranges[1].first=3;mixed->ranges[1].count=3;
  const auto together=draw(renderer,mixed);
  check(at(together,72,24)==at(original,72,24)&&at(together,24,72)==at(typed_pixels,24,72),"Mixed snapshot keeps independent PBR and typed triangles in one program");
  check(draw(renderer,plain)==original&&draw(renderer,toon)==typed_pixels,"Repeated mixed PBR typed switches preserve exact complete images");
  auto weighted=std::make_shared<MeshSnapshot>(*mixed);weighted->selected_weights.assign(4,.5f);
  check(close(at(draw(renderer,weighted),72,24),{0,255,0,255})&&close(at(read(),24,72),{0,255,0,255}),"Both shader branches retain selected influence heatmaps");
  auto clipped=std::make_shared<MeshSnapshot>(*mixed);clipped->clipped_object=2;clipped->clip_plane={{1,0,0},0};
  const auto cut=draw(renderer,clipped);check(at(cut,24,72)[3]==0&&at(cut,72,24)==at(original,72,24),"Mixed clipping retains object identity through program changes");
  check(draw(renderer,plain)==original,"Clipping and weight uniforms cannot leak into the PBR program");
 }
 check(renderer.buffer_stats().geometry_uploads==uploads,"Changing surface programs reuses exact geometry ownership without new uploads");
 check(glGetError()==GL_NO_ERROR,"Surface program transitions leave valid GL state");
 std::cout<<"PASS "<<checks<<" surface program transition checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
