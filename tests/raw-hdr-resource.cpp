// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-paint-fixtures.hpp"
#include "dome/native-authoring.hpp"
#include "dome/humanoid-creator.hpp"
#include <fstream>
#include <iostream>
namespace {
void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
std::vector<uint8_t> bytes(const std::filesystem::path &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
}
int main(int argc,char **argv)try{
  using namespace dome;if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
  auto source=source_paint_fixture::input(0);auto &m=source.j["materials"][0];m["extensions"].erase("KHR_materials_unlit");m["emissiveFactor"]={1,.002f,.5f};m["emissiveTexture"]={{"index",0},{"texCoord",0}};m["extensions"]["KHR_materials_emissive_strength"]={{"emissiveStrength",8}};source.j["extensionsUsed"].push_back("KHR_materials_emissive_strength");
  Image unused{4095,4095,std::vector<uint8_t>(size_t(4095)*4095*4,0)};const auto png=unused.png();
  while(source.bin.size()%4)source.bin.push_back(0);const auto offset=source.bin.size();source.bin.insert(source.bin.end(),png.begin(),png.end());const auto view=source.j["bufferViews"].size();source.j["bufferViews"].push_back({{"buffer",0},{"byteOffset",offset},{"byteLength",png.size()}});source.j["images"].push_back({{"bufferView",view},{"mimeType","image/png"}});
  const auto input=out/"unused.glb";source.save(input);const auto original=bytes(input);
  Scene initial;initial.active().visible=false;ImportAuthoringOptions options;options.paint_resolution=64;
  auto scene=prepare_authoring_import(initial,input,options).scene;const auto exact=scene.encode();
  require(scene.active().appearance->decoded_images->size()==2,"Actual unused decoded source image is retained");
  const auto file=out/"native.glb";export_glb_authoring(NativeAuthoringExport{std::make_shared<const Scene>(scene.snapshot()),NativeAuthoringExport::Scope::WholeScene,{},true},file);
  auto recovered=extract_native_authoring(bytes(file));
  require(recovered&&recovered->encode()==exact&&scene.encode()==exact&&bytes(input)==original,
      "Unused source images allocate no decode scratch but remain exact native owners within the default working budget");
  std::cout<<"PASS actual4095-square unused image/native-source quota roundtrip\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
