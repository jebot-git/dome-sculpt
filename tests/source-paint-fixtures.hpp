// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "import-authoring-fixtures.hpp"
namespace source_paint_fixture {
inline import_fixture::Fixture input(int kind){
 import_fixture::Fixture f;while(f.bin.size()%4)f.bin.push_back(0);auto &p=f.j["meshes"][0]["primitives"][0];p["attributes"]["TEXCOORD_0"]=f.values({-1.25f,-.5f,1.75f,-.5f,1.75f,2.5f,-1.25f,2.5f},2,"VEC2");
 f.j["nodes"][0]["translation"]={0,0,0};f.j["nodes"][0]["children"]={1};f.j["nodes"][1]["translation"]={0,0,0};f.j["nodes"][1]["scale"]={5,5,5};
 auto &m=f.j["materials"][0];
 if(kind!=0&&kind!=2)m["pbrMetallicRoughness"].erase("baseColorTexture");
 if(kind>=1&&kind<=3){m["extensions"]["VRMC_materials_mtoon"]={{"specVersion","1.0"}};f.j["extensionsUsed"].push_back("VRMC_materials_mtoon");
  if(kind==1||kind==2){
   auto &mt=m["extensions"]["VRMC_materials_mtoon"];mt["shadeMultiplyTexture"]={{"index",0},{"texCoord",1},{"extensions",{{"KHR_texture_transform",{{"offset",{.13f,-.27f}},{"scale",{.8f,1.1f}},{"rotation",.17f},{"texCoord",1}}}}}};
   mt["shadeColorFactor"]={.4f,.6f,.8f};mt["shadingShiftFactor"]=-1;mt["shadingToonyFactor"]=0;
   f.j["extensionsUsed"].push_back("KHR_texture_transform");
  }
  if(kind==3){m["extensions"]["VRMC_materials_mtoon"]["matcapTexture"]={{"index",0}};m["extensions"]["VRMC_materials_mtoon"]["matcapFactor"]={.4f,.3f,.2f};}
 }
 if(kind>=4){m["alphaMode"]="OPAQUE";m.erase("alphaCutoff");}
 if(kind==5){p["attributes"].erase("TEXCOORD_0");p["attributes"].erase("TEXCOORD_1");}
 return f;
}
}
