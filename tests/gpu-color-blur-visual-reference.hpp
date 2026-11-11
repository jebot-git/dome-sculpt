// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Test-only source-raster/NEAREST projection oracle. No GL calls or app state.
#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <span>
#include <string>
#include <tuple>
#include <vector>
namespace blur_visual_reference {
using Json=nlohmann::json;
enum class Background { Panel, Cell, Accent };
struct Match {
 bool matching=false;size_t expected=0,observed=0,mismatches=0,samples=0;std::string error;
 Json json()const{return {{"matching",matching},{"expected_ink_pixels",expected},{"observed_ink_pixels",observed},{"mismatches",mismatches},{"samples",samples},{"error",error}};}
};
class Reference {
 struct Mask{std::array<int,4>bounds;std::vector<uint8_t>ink;};
 struct Sample{size_t index;bool ink;};
 Json source_;int width_=0,height_=0,unit_=1;std::map<std::string,Mask>masks_;
 std::map<std::tuple<std::string,int,int,int>,std::vector<Sample>>projections_;
 static double decode(double s){return s<=.04045?s/12.92:std::pow((s+.055)/1.055,2.4);}
 std::array<double,3>color(Background kind)const{
  std::array<double,3>c{};
  if(kind==Background::Accent){const auto v=source_.at("accent").get<uint32_t>();c={decode(double((v>>16)&255)/255),decode(double((v>>8)&255)/255),decode(double(v&255)/255)};}
  else {const auto&v=source_.at(kind==Background::Cell?"cell_background":"panel_background");for(int i=0;i<3;++i)c[i]=decode(v.at(i).get<double>()/255);}
  return c;
 }
 const std::vector<Sample>&project(const std::string&name,int eye,int w,int h){
  const auto key=std::tuple{name,eye,w,h};if(auto it=projections_.find(key);it!=projections_.end())return it->second;
  const auto&m=masks_.at(name);std::vector<Sample>samples;
  // Identical source geometry to palette.cpp and synthetic xr poses/FOV.
  // Screen pixel centers map through the .6m quad at (0,1.56,-.66), then
  // GL_NEAREST selects floor(textureCoordinate*textureSize), not area coverage.
  for(int y=0;y<h;++y)for(int x=0;x<w;++x){
   const double wx=(eye?.032:-.032)+(2*(x+.5)/w-1)*.66*std::tan(.6);
   const double wy=1.6+(2*(y+.5)/h-1)*.66*std::tan(.6);
   const int tx=int(std::floor((wx/.6+.5)*width_)),ty=int(std::floor((.5-(wy-1.56)/.6)*height_));
   if(tx>=m.bounds[0]-2*unit_&&tx<m.bounds[2]+2*unit_&&ty>=m.bounds[1]-2*unit_&&ty<m.bounds[3]+2*unit_)
    samples.push_back({(size_t(y)*w+x)*4,tx>=0&&tx<width_&&ty>=0&&ty<height_&&bool(m.ink[size_t(ty)*width_+tx])});
  }
  return projections_.emplace(key,std::move(samples)).first->second;
 }
public:
 explicit Reference(const std::string&file){
  std::ifstream input(file);if(!input)throw std::runtime_error("Missing source bitmap reference");input>>source_;
  width_=source_.at("width");height_=source_.at("height");unit_=source_.at("unit");
  if(source_.at("version")!=1||unit_<1||unit_>8||width_!=640*unit_||height_!=width_||source_.at("opacity")!=255)throw std::runtime_error("Unsupported source bitmap reference geometry/opacity");
  for(auto it=source_.at("masks").begin();it!=source_.at("masks").end();++it){Mask m;m.bounds=it.value().at("bounds").get<std::array<int,4>>();m.ink.resize(size_t(width_)*height_);
   for(const auto&p:it.value().at("ink")){const int x=p.at(0),y=p.at(1);if(x<0||y<0||x>=width_||y>=height_)throw std::runtime_error("Source glyph outside palette");m.ink[size_t(y)*width_+x]=1;}masks_.emplace(it.key(),std::move(m));}
 }
 Match match(const std::string&name,int eye,int w,int h,std::span<const uint8_t>rgba,Background background=Background::Panel,bool empty=false){
  Match result;
  try{if(eye<0||eye>1||rgba.size()!=size_t(w)*h*4)throw std::runtime_error("Invalid actual eye image");const auto&expected=project(name,eye,w,h);auto bg=color(background);std::array<double,3>fg{};for(int c=0;c<3;++c)fg[c]=decode(source_.at("foreground").at(c).get<double>()/255);
   for(const auto&s:expected){double from_bg=0,from_fg=0;for(int c=0;c<3;++c){const double v=double(rgba[s.index+c])/255;from_bg+=(v-bg[c])*(v-bg[c]);from_fg+=(v-fg[c])*(v-fg[c]);}const bool observed=from_fg<from_bg;++result.samples;result.expected+=s.ink;result.observed+=observed;result.mismatches+=s.ink!=observed;}
   result.matching=result.samples>0&&(empty||result.expected>0)&&result.mismatches==0;
  }catch(const std::exception&e){result.error=e.what();}return result;
 }
 size_t difference(const std::string&a,const std::string&b,int eye,int w,int h){const auto&x=project(a,eye,w,h);const auto&y=project(b,eye,w,h);if(x.size()!=y.size())throw std::runtime_error("Stencil comparison has different source bounds");size_t n=0;for(size_t i=0;i<x.size();++i){if(x[i].index!=y[i].index)throw std::runtime_error("Stencil projection domains differ");n+=x[i].ink!=y[i].ink;}return n;}
};
}
