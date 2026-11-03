// SPDX-License-Identifier: GPL-3.0-or-later
// Reuse only the established EGL/FBO, snapshot and typed-MToon fixtures. The
// core-unlit oracle below is the direct normative baseColor product in CPU
// double precision; it does not call production shading or transfer helpers.
#define main existing_mtoon_suite
#include "mtoon-render.cpp"
#undef main
#include "source-color-consumer.hpp"
#include <filesystem>
namespace {
double decode_srgb(double v) {
  return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);
}
void expected_unlit(const Pixels &pixels,const Material &material,
                    const std::array<uint8_t,4> &texel,
                    const std::array<float,4> &vertex) {
  const double alpha=double(material.color[3])*texel[3]/255*vertex[3];
  double visible=alpha;
  if(material.alpha==Material::AlphaMode::Opaque)visible=1;
  else if(material.alpha==Material::AlphaMode::Mask)
    visible=alpha>=material.alpha_cutoff?1:0;
  const auto p=at(pixels);
  for(size_t k=0;k<3;++k) {
    const double linear=material.color[k]*decode_srgb(texel[k]/255.)*vertex[k];
    const int expected=int(std::lround(linear*visible*255));
    check(std::abs(p[k]-expected)<=1,"Core unlit is exactly base factor times decoded texture times linear vertex color");
  }
  check(std::abs(p[3]-int(std::lround(visible*255)))<=1,
        "Core unlit keeps Opaque/Mask/Blend alpha semantics");
}
}
int main(int argc,char **argv) try {
  if(argc<2)return 2;
  NativeGraphics graphics;
  const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR));
  const std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  std::cout<<"GL_VENDOR="<<vendor<<" GL_RENDERER="<<device<<'\n';
  check(argc<3||std::string_view(argv[2])!="--hardware"||
        (vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos&&
         device.find("llvmpipe")==std::string::npos&&device.find("softpipe")==std::string::npos&&device.find("UHD")==std::string::npos),
        "Explicit hardware request requires actual Intel Arc A770");
  const std::filesystem::path output=argv[1];std::filesystem::create_directories(output);
  Fbo fbo;fbo.bind();SurfaceRenderer native;
  Material m=white();m.color={.32f,.61f,.27f,.8f};m.emissive={.9f,.35f,.6f};
  m.metallic=.9f;m.roughness=.12f;
  auto raw=snapshot(m);const std::array<float,4> color{.6f,.8f,.4f,.75f};
  raw->source_colors.assign(4,color);
  native.set_output_encoding(SurfaceOutputEncoding::Linear);
  expected_unlit(draw(native,raw),m,{255,255,255,255},color);
  auto zero=std::make_shared<MeshSnapshot>(*raw);zero->ranges[0].material.emissive={};
  check(draw(native,raw)==draw(native,zero),"Nonzero unlit fallback emission cannot alter visible pixels");
  auto lit=std::make_shared<MeshSnapshot>(*raw);lit->ranges[0].material.unlit=false;
  auto lit_zero=std::make_shared<MeshSnapshot>(*lit);lit_zero->ranges[0].material.emissive={};
  check(delta(draw(native,lit),draw(native,lit_zero))>10,
        "Counterfactual lit PBR still uses genuine emission fallback");
  const std::array<uint8_t,4> texel{128,192,64,128};
  auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=image(texel);
  maps->emission.image=image({255,200,240,255});
  auto textures=std::make_shared<TextureMaps>();textures->source_only=true;textures->source_maps=maps;
  raw->ranges[0].textures=textures;
  for(auto mode:{Material::AlphaMode::Opaque,Material::AlphaMode::Mask,Material::AlphaMode::Blend}) {
    m.alpha=mode;m.alpha_cutoff=.25f;raw->ranges[0].material=m;
    expected_unlit(draw(native,raw),m,texel,color);
  }
  m.alpha=Material::AlphaMode::Mask;m.alpha_cutoff=.35f;raw->ranges[0].material=m;
  expected_unlit(draw(native,raw),m,texel,color);
  // Independent parsed-file consumer must obey the same normative unlit rule,
  // while export retains all PBR fallback fields and image bytes.
  m.alpha=Material::AlphaMode::Opaque;raw->ranges[0].material=m;
  ObjectSurface surface;surface.id=1;surface.mesh=quad();surface.material=m;surface.textures=textures;
  const auto path=output/"unlit-fallback.glb";export_glb({surface},path);
  color_consumer::Model parsed(path);const auto &emitted=parsed.data->materials[0];
  check(emitted.unlit&&emitted.emissive_texture.texture&&emitted.emissive_factor[0]==m.emissive[0]&&
        emitted.emissive_factor[1]==m.emissive[1]&&emitted.emissive_factor[2]==m.emissive[2],
        "Export retains authored fallback emission factor and texture instead of deleting ignored fields");
  check(color_consumer::image(emitted.emissive_texture.texture).rgba==maps->emission.image->rgba,
        "Fallback emission resource pixels are unchanged");
  // Packed source attributes obey immutable snapshot ownership; publish a new
  // snapshot when changing that stream instead of mutating a retained owner.
  auto exported=std::make_shared<MeshSnapshot>(*raw);exported->source_colors.clear();
  native.set_output_encoding(SurfaceOutputEncoding::Srgb);
  const auto wanted=draw(native,exported);color_consumer::Renderer independent;
  fbo.bind();independent.render(parsed.data,nullptr,{},true);
  const int error=delta(wanted,read());std::cout<<"UNLIT_CONSUMER maxerror="<<error<<'\n';
  check(error<=2,"Independent cgltf consumer agrees on base-only unlit despite nonzero fallback emission");
  // VRM-specific MToon dispatch is earlier than core unlit, including sources
  // whose glTF fallback declares unlit. Preserve that distinct shading model.
  for(bool legacy:{false,true}) {
    auto material=typed(legacy);material.parameters.cull=MToonCull::Off;
    material.parameters.base_color={.16f,.2f,.13f,1};material.parameters.shade_color={.02f,.03f,.01f};
    material.parameters.emission={.15f,.2f,.25f};
    auto scene=snapshot();scene->ranges[0].appearance=appearance(material);
    check(close(at(draw(native,scene)),expected(material,{}),2),
          "Typed VRM0/VRM1 MToon retains its actual emission and earlier dispatch");
  }
  std::cout<<"Core unlit fallback: "<<checks<<" checks\n";return 0;
} catch(const std::exception &e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
