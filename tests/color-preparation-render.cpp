// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_material_renderer_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/color-preparation.hpp"
#include "dome/source-uv-bake.hpp"
#include "source-color-consumer.hpp"
#include "source-color-fixtures.hpp"
namespace {
Image constant(uint32_t resolution,std::array<uint8_t,4> value) {
  Image image{resolution,resolution,std::vector<uint8_t>(size_t(resolution)*resolution*4)};
  for(size_t p=0;p<image.rgba.size();++p)image.rgba[p]=value[p%4];
  return image;
}
void prepare(Scene &scene) {
  auto &o=scene.active();
  auto result=prepare_source_colors({o.painted,o.appearance,o.material_bindings,o.deformation,o.material});
  o.painted=result.paint;o.material_bindings=result.material_bindings;
}
void overlay(Scene &scene) {
  auto &o=scene.active();o.painted=o.painted->snapshot();auto r=o.painted->resolution();
  BakedTextures b;b.maps.color=constant(r,{220,40,100,180});
  b.maps.pbr_color=constant(r,{30,190,240,90});
  b.maps.metal_rough=constant(r,{255,128,0,255});b.maps.emission=constant(r,{0,0,0,255});
  b.color_interpretation=BakedColorInterpretation::ResolvedPbr;b.coverage.resize(size_t(r)*r);
  for(uint32_t y=0;y<r;++y)for(uint32_t x=0;x<r;++x)b.coverage[size_t(y)*r+x]=x<r/3?0:x<2*r/3?128:255;
  o.painted->install_bake(std::move(b));
}
void paint(Scene &scene) {
  auto &o=scene.active();o.painted=o.painted->snapshot();
  Brush brush;brush.from=brush.to={0,0,0};brush.normal={0,0,1};brush.radius=.65f;
  PaintSettings settings;settings.color={.9f,.2f,.4f,.6f};settings.front_only=false;
  o.painted->begin_stroke();o.painted->stamp(brush,settings);o.painted->end_stroke();
  o.painted->layer_properties("Fractional editable paint",true,.65f);
  o.painted->add_layer("Hidden source authoring");
  o.painted->layer_properties("Hidden source authoring",false,.4f);o.painted->select_layer(1);
}
void rotate_uv(Scene &scene) {
  auto &o=scene.active();SourceUvBakeInput i;i.paint=o.painted;i.appearance=o.appearance;
  i.current_material=o.material_bindings;i.current_deformation=o.deformation;i.target=o.painted->mesh();
  for(auto &q:i.target.uv)q={1-q[1],q[0]};
  for(uint32_t v=0;v<i.target.positions.size();++v)i.source_vertices.push_back(v);
  auto a=adopt_source_uv_bake(bake_source_uv(i));
  o.painted=a.paint;o.appearance=a.appearance;o.material_bindings=a.material_bindings;o.deformation=a.deformation;
}
}
int main(int argc,char **argv)try {
  if(argc<2)return 2;
  NativeGraphics graphics;std::string gpu=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  std::cout<<"GPU "<<gpu<<'\n';
  check(argc<3||std::string(argv[2])!="--hardware"||(gpu.find("llvmpipe")==std::string::npos&&gpu.find("softpipe")==std::string::npos),"Prepared PBR fixture runs on the requested graphics device");
  Fbo framebuffer;framebuffer.bind();SurfaceRenderer native;color_consumer::Renderer consumer;
  std::filesystem::path prefix=argv[1];
  for(bool zero:{false,true}) {
    auto original=source_color_fixture::model(zero);
    auto raw_colors=original.active().appearance->asset->primitives[0].colors;
    for(int stage=0;stage<4;++stage) {
      auto scene=original;prepare(scene);if(stage>=1)overlay(scene);if(stage>=2)paint(scene);if(stage>=3)rotate_uv(scene);
      for(auto alpha:{Material::AlphaMode::Opaque,Material::AlphaMode::Mask,Material::AlphaMode::Blend})for(float morph:{0.f,.35f,1.f}) {
        auto &o=scene.active();o.material.alpha=alpha;o.material.alpha_cutoff=.35f;
        auto state=*o.deformation;state.morph_weights={morph};state.pose={{{},{},{1.5f,1,.7f}}};scene.set_deformation(o.id,state);
        Document document;document.replace(scene);
        check(document.wait_mesh()&&document.frame().state->error.empty(),"Actual worker publishes prepared layers through POSITION/NORMAL/TANGENT morphs and anisotropic skin");
        auto snapshot=document.frame().surface;
        check(snapshot->source_colors.empty(),"Prepared PBR pixels suppress the source RGBA stream instead of multiplying vertex colors twice");
        auto path=prefix;path+="-"+std::to_string(zero)+"-"+std::to_string(stage)+"-"+std::to_string(int(alpha))+"-"+std::to_string(int(morph*100))+".glb";
        export_glb(scene.preview_surfaces(),path);color_consumer::Model parsed(path);
        const auto &p=parsed.data->meshes[0].primitives[0];
        check(!color_consumer::attr(p.attributes,p.attributes_count,cgltf_attribute_type_color),"Standard export carries the explicit resolved PBR texture and omits only its current baked-in COLOR_0 attribute");
        auto expected=draw(native,snapshot);framebuffer.bind();consumer.render(parsed.data);auto actual=read();int error=delta(expected,actual);
        if(alpha==Material::AlphaMode::Opaque){bool visible=false;for(size_t p=3;p<expected.size();p+=4)visible|=expected[p]>0&&actual[p]>0;check(visible,"Both actual consumers render nonempty opaque geometry instead of matching cleared framebuffers");}
        std::cout<<"PREPARED_CONSUMER zero="<<zero<<" stage="<<stage<<" alpha="<<int(alpha)<<" morph="<<morph<<" maxerror="<<error<<'\n';
        check(error<=4,"Independent cgltf/PNG/GLSL consumer preserves prepared color, alpha, fractional bake and editable-layer order");
      }
      auto encoded=scene.encode();auto reopened=Scene::decode(nlohmann::json::from_cbor(nlohmann::json::to_cbor(encoded)));
      check(reopened.encode()==encoded&&reopened.active().appearance->asset->primitives[0].colors==raw_colors,"Prepared scene restarts exactly while retaining every original authored color value");
    }
  }
  std::cout<<"Prepared PBR consumer checks "<<checks<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
