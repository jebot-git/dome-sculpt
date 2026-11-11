// SPDX-License-Identifier: GPL-3.0-or-later
// Source-only bitmap title reference; no GL, Document or application callback.
#include "palette-text.hpp"
#include "dome/session-appearance.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <set>
#include <cmath>
#include "native-fontconfig.hpp"
int main(int argc,char**argv)try{
 if(argc!=2)return 2;
 dome::desktop::NativeFontconfigRuntime native_fontconfig;
 const auto style=dome::session_appearance();const int unit=std::max(1,(style.pixels+15)/16),width=640*unit;
 dome::palette_text::Font font(style.file,style.pixels);nlohmann::json j={{"version",1},{"font_file",style.file},{"font_pixels",style.pixels},{"unit",unit},{"width",width},{"height",width},{"light",style.light},{"opacity",style.opacity},{"baseline",345*unit},{"center",320*unit},{"source","Actualpalette.cpp text placement and bitmap alpha!=0; GL_NEAREST"},{"accent",style.accent},{"foreground",style.light?nlohmann::json{30,37,46}:nlohmann::json{231,235,241}},{"panel_background",style.light?nlohmann::json{229,233,239}:nlohmann::json{31,37,45}},{"cell_background",style.light?nlohmann::json{200,209,220}:nlohmann::json{55,65,80}}};
 for(const char*title:{"Dome Sculpt","Objects","Mesh tools","Mesh baking"}){
  const int room=2*(320*unit-8*unit);auto line=font.layout(title,room);const int x=320*unit-line.advance/2;std::set<std::pair<int,int>>ink;
  for(const auto&placed:line.glyphs){const auto&g=*placed.bitmap;for(int y=0;y<g.height;++y)for(int col=0;col<g.width;++col)if(g.alpha[size_t(y)*g.width+col]&&x+placed.x+g.left+col>=320*unit-room/2&&x+placed.x+g.left+col<320*unit+(room+1)/2)ink.emplace(x+placed.x+g.left+col,345*unit-placed.y-g.top+y);}
  nlohmann::json pixels=nlohmann::json::array();int left=width,top=width,right=0,bottom=0;for(auto[a,b]:ink){pixels.push_back({a,b});left=std::min(left,a);top=std::min(top,b);right=std::max(right,a);bottom=std::max(bottom,b);}
  j["masks"][title]={{"advance",line.advance},{"bounds",{left,top,right+1,bottom+1}},{"ink",pixels}};
 }
 j["font_manifest"]=font.source_manifest();std::ofstream out(argv[1]);out<<j.dump(2)<<'\n';return !out;
}catch(const std::exception&e){fprintf(stderr,"%s\n",e.what());return 1;}
