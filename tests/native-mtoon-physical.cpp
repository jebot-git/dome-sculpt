// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_typed_source_controls
#include "mtoon-render.cpp"
#include "dome/document.hpp"
#include "dome/material-bindings.hpp"
#undef main
#include "dome/scene-file.hpp"
namespace {
std::shared_ptr<SourceMaterialBindings> binding(bool legacy,const Material &native,
    const std::shared_ptr<const Image> &base,const std::shared_ptr<const Image> &emission) {
  auto out=std::make_shared<SourceMaterialBindings>();out->material=typed(legacy);
  out->native_reference={native.color,native.emissive,int(native.alpha),native.alpha_cutoff,native.double_sided};
  out->material.parameters.base_color=native.color;out->material.parameters.emission={native.emissive[0],native.emissive[1],native.emissive[2]};
  out->material.textures[0].index=0;out->roles[0].image=base;out->material.textures[3].index=1;out->roles[3].image=emission;
  for(size_t role:{0u,3u})out->roles[role].min_filter=out->roles[role].mag_filter=GL_NEAREST;
  out->validate();return out;
}
void ink(PaintedMesh &paint,PaintSettings settings) {
  Brush brush;brush.from=brush.to={0,0,0};brush.normal={0,0,1};brush.radius=2;brush.strength=.8f;settings.front_only=false;
  paint.begin_stroke();paint.stamp(brush,settings);check(paint.end_stroke(),"Actual native physical brush creates sparse float emission");
}
Scene native_typed_physical(bool legacy) {
  Scene scene;scene.active().visible=false;const auto id=scene.add("Native typed float emission",Volume{});scene.select(id);
  auto &object=scene.active();object.mesh_active=true;object.material=white();object.material.unlit=false;
  object.material.color={.6f,.7f,.5f,1};object.material.emissive={.15f,.03f,.08f};
  auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=std::make_shared<const Image>(Image{2,2,{255,40,80,255,40,240,80,255,80,40,255,255,220,180,40,255}});
  maps->emission.image=std::make_shared<const Image>(Image{2,2,{180,40,20,255,20,80,200,255,40,200,80,255,220,100,20,255}});
  maps->color.min_filter=maps->color.mag_filter=maps->emission.min_filter=maps->emission.mag_filter=GL_NEAREST;
  object.painted=PaintedMesh::from_maps(quad(),maps,64);
  auto holder=binding(legacy,object.material,maps->color.image,maps->emission.image);holder->material.parameters.emission_strength=legacy?1:1.5f;object.material_bindings=holder;
  PaintSettings settings;settings.channel=PaintChannel::Emission;settings.emission_units=PaintEmissionUnits::Physical;settings.emission={2.2f,.03f,1.3f};ink(*object.painted,settings);
  object.painted->layer_properties("Layer 1",true,.63f);return scene;
}
void ready_native(Document &doc) {check(doc.wait_mesh(std::chrono::seconds(30))&&doc.frame().surface,"Actual Document publishes its own complete typed native preview");if(!doc.frame().state->error.empty())throw std::runtime_error(doc.frame().state->error);}
}
int main(int argc,char **argv)try {
  if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
  NativeGraphics graphics;std::cout<<"GPU "<<glGetString(GL_VENDOR)<<" / "<<glGetString(GL_RENDERER)<<std::endl;Fbo fbo;fbo.bind();SurfaceRenderer renderer;renderer.set_frame_time(0);
  for(bool legacy:{true,false}) {auto scene=native_typed_physical(legacy);const auto exact=scene.encode();scene.save(out/(legacy?"native-v0.dsculpt":"native-v1.dsculpt"));Document doc;doc.replace(scene);ready_native(doc);
    try {draw(renderer,doc.frame().surface);}catch(...){check(doc.copy_scene().encode()==exact,"Actual typed preview refusal preserves complete float/source/native Scene");throw;}
    check(doc.copy_scene().encode()==exact,"Actual typed float preview preserves all source owners and float project bytes");}
  std::cout<<"PASS "<<checks<<" native typed physical controls\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
