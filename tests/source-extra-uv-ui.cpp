// SPDX-License-Identifier: GPL-3.0-or-later
#include "import-authoring-fixtures.hpp"
#include <iostream>
using namespace dome;
int main(int argc,char **argv)try {
 if(argc!=3)return 2;const std::string_view mode=argv[1];
 if(mode=="--empty-project"){Scene{}.save(argv[2]);return 0;}
 if(mode=="--inspect"){std::cout<<Scene::load(argv[2]).encode().dump()<<'\n';return 0;}
 if(mode!="--ui-fixture")return 2;
 import_fixture::Fixture source;while(source.bin.size()%4)source.bin.push_back(0);const int uv=source.values({0,0,0,1,1,1,1,0},2,"VEC2");source.j["meshes"][0]["primitives"][0]["attributes"]["TEXCOORD_2"]=uv;
 auto &m=source.j["materials"][0];m.erase("extensions");m["alphaMode"]="OPAQUE";m.erase("alphaCutoff");m["pbrMetallicRoughness"].erase("baseColorTexture");m["pbrMetallicRoughness"]["baseColorFactor"]={.65f,.75f,.85f,1};
 Image normal{32,32,{}};for(int y=0;y<32;++y)for(int x=0;x<32;++x)normal.rgba.insert(normal.rgba.end(),{uint8_t(128+55*std::sin(x*.17f)),uint8_t(128+45*std::cos(y*.13f)),235,255});const auto png=normal.png();while(source.bin.size()%4)source.bin.push_back(0);const auto view=source.j["bufferViews"].size();source.j["bufferViews"].push_back({{"buffer",0},{"byteOffset",source.bin.size()},{"byteLength",png.size()}});source.bin.insert(source.bin.end(),png.begin(),png.end());const auto image=source.j["images"].size();source.j["images"].push_back({{"bufferView",view},{"mimeType","image/png"}});const auto texture=source.j["textures"].size();source.j["textures"].push_back({{"source",image},{"sampler",0}});m["normalTexture"]={{"index",texture},{"texCoord",2}};source.j["samplers"][0]={{"wrapS",33071},{"wrapT",33071},{"magFilter",9728},{"minFilter",9728}};source.save(argv[2]);return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
