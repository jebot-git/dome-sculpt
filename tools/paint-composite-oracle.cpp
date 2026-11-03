// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <chrono>
using namespace dome;
int main(int argc,char**argv)try{
 if(argc!=2&&argc!=3)throw std::invalid_argument("Oracle output directory and optional immutable input directory required");std::filesystem::create_directories(argv[1]);
 for(uint32_t r:{512u,1024u}){
  Mesh m;m.positions={{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}};m.normals.assign(4,{0,0,1});m.uv={{{.08f,.08f}},{{.92f,.08f}},{{.92f,.92f}},{{.08f,.92f}}};m.indices={0,1,2,0,2,3};auto p=PaintedMesh::from_mesh(m,r,{});
  for(int layer=0;layer<3;++layer){if(layer)p->add_layer("Oracle "+std::to_string(layer));p->layer_properties("Oracle "+std::to_string(layer),true,layer==0?.73f:.37f);
   for(int c=0;c<5;++c){PaintSettings s;s.channel=PaintChannel(c);s.color={.017f,.47f,.81f,.63f};s.emission={.91f,.031f,.27f};s.metallic=.83f;s.roughness=.17f;s.mask=.41f;s.flow=.72f;Brush b;b.radius=.29f;b.pressure=.79f;b.strength=.63f;b.normal={0,0,1};b.from={-.79f,-.65f+c*.29f+layer*.04f,0};b.to={.83f,-.42f+c*.25f,0};p->begin_stroke();p->stamp(b,s);p->end_stroke();}
   if(layer==2)p->layer_properties("Oracle "+std::to_string(layer),false,.37f);
  }
  if(argc==3){std::ifstream input(std::filesystem::path(argv[2])/(std::to_string(r)+"-paint.cbor"),std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});if(bytes.empty())throw std::runtime_error("Missing immutable source Paint record");p=PaintedMesh::decode(nlohmann::json::from_cbor(bytes));}
  Material mat;mat.color={.23f,.61f,.037f,.999f};mat.emissive={.013f,.037f,.071f};mat.metallic=.31f;mat.roughness=.68f;auto raw=nlohmann::json::to_cbor(p->encode());std::ofstream rawfile(std::filesystem::path(argv[1])/(std::to_string(r)+"-paint.cbor"),std::ios::binary);rawfile.write(reinterpret_cast<const char*>(raw.data()),raw.size());rawfile.close();
  auto began=std::chrono::steady_clock::now();auto maps=p->composite(mat);const auto cost=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();
  auto save=[&](std::string name,const Image &im){std::ofstream f(std::filesystem::path(argv[1])/(std::to_string(r)+"-"+name+".rgba"),std::ios::binary);f.write(reinterpret_cast<const char*>(im.rgba.data()),im.rgba.size());if(!f)throw std::runtime_error("Cannot write oracle image");};
  save("color",maps->color);save("orm",maps->metal_rough);save("emission",maps->emission);save("coverage",maps->color_coverage);save("emission-coverage",maps->emission_coverage);
  std::cout<<r<<" transparent="<<maps->transparent<<" revision="<<p->revision()<<" composite_ms="<<cost<<'\n';
 }
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
