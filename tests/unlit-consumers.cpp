// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_mtoon_fixture
#include "mtoon-render.cpp"
#undef main
#include "vrm-role-consumer.hpp"
#include "prepared-normal-consumer.hpp"
#include "../src/gltf-document.hpp"
#include <bit>
namespace {
double decode(double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);}
double encode(double v){return v<=.0031308?12.92*v:1.055*std::pow(v,1/2.4)-.055;}
void oracle(const Pixels &pixels,const Material &m,std::array<uint8_t,4> texel){
  auto pixel=at(pixels);
  for(size_t k=0;k<3;++k){const int expected=int(std::lround(255*encode(m.color[k]*decode(texel[k]/255.))));
    check(std::abs(pixel[k]-expected)<=1,"Parsed core-unlit consumer follows the independent baseColor product and ignores fallback emission");}
  check(pixel[3]==255,"Parsed core-unlit consumer retains opaque coverage");
}
}
int main(int argc,char **argv)try {
  if(argc!=3)return 2;const std::string kind=argv[2];
  if(kind!="role"&&kind!="prepared")return 2;
  NativeGraphics graphics;std::cout<<"GPU "<<glGetString(GL_RENDERER)<<'\n';Fbo fbo;fbo.bind();
  const std::array<uint8_t,4> texel{128,192,64,255};
  Material m=white();m.color={.3f,.5f,.7f,1};m.emissive={.8f,.4f,.7f};
  auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=image(texel);maps->emission.image=image({255,224,192,255});
  auto textures=std::make_shared<TextureMaps>();textures->source_only=true;textures->source_maps=maps;
  ObjectSurface surface;surface.id=1;surface.mesh=quad();surface.material=m;surface.textures=textures;
  auto document=detail::make_gltf({surface});auto &j=document.json;auto &binary=document.binary;
  // This separate consumer fixture authors the exact flat quad tangent. The
  // unmodified native-export fixture in unlit-fallback remains untouched.
  const size_t offset=binary.size();
  for(size_t vertex=0;vertex<4;++vertex)for(float value:{1.f,0.f,0.f,1.f}){
    auto word=std::bit_cast<uint32_t>(value);for(size_t k=0;k<4;++k)binary.push_back(uint8_t(word>>(k*8)));}
  const size_t view=j["bufferViews"].size(),accessor=j["accessors"].size();
  j["bufferViews"].push_back({{"buffer",0},{"byteOffset",offset},{"byteLength",64},{"target",34962}});
  j["accessors"].push_back({{"bufferView",view},{"componentType",5126},{"count",4},{"type","VEC4"}});
  j["meshes"][0]["primitives"][0]["attributes"]["TANGENT"]=accessor;
  std::filesystem::create_directories(argv[1]);auto path=std::filesystem::path(argv[1])/(kind+".glb");
  detail::write_glb(std::move(document),path);role_consumer::Model model(path);
  if(kind=="role"){role_consumer::Renderer renderer;renderer.render(model.data);}
  else {prepared_normal_consumer::Renderer renderer;renderer.render(model.data);}
  oracle(read(),m,texel);std::cout<<kind<<" unlit consumer: "<<checks<<" checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
