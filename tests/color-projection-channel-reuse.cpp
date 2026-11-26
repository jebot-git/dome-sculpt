// SPDX-License-Identifier: GPL-3.0-or-later
#include "color-projection-quality.hpp"
#include "color-projection-reference-725.hpp"
#include <nlohmann/json.hpp>
#include <bit>
#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
using namespace dome;
namespace {
size_t checks=0;
void check(bool ok,const char*why){if(!ok)throw std::runtime_error(why);++checks;}
using Colors=std::vector<std::array<float,4>>;
struct Result {detail::ColorProjectionBound value;std::string error;size_t callbacks=0;double seconds=0;Colors colors;Image image;};
Result run(bool optimized,const Mesh&mesh,const Colors&colors,const Image&image,uint64_t budget,size_t mutate=0,size_t cancel=0){
 Result r;r.colors=colors;r.image=image;
 auto callback=[&]{++r.callbacks;if(r.callbacks==mutate){r.colors.back()[1]=.125f;r.image.rgba[(size_t(r.image.height/2)*r.image.width+r.image.width/2)*4+1]=17;}return r.callbacks==cancel;};
 const auto begin=std::chrono::steady_clock::now();try{
  if(optimized)r.value=detail::color_projection_bound(mesh,r.colors,r.image,budget,callback);
  else {auto old=reference725::color_projection_bound(mesh,r.colors,r.image,budget,callback);r.value={old.maximum_error,old.tested_cells,old.intersecting_cells};}
 }catch(const std::exception&e){r.error=e.what();}
 r.seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();return r;
}
std::array<float,4> rounded(const detail::ColorProjectionBound&v){std::array<float,4>a;for(size_t c=0;c<4;++c)a[c]=std::min(1.f,std::nextafter(float(v.maximum_error[c]),INFINITY));return a;}
Result compare(const char*label,const Mesh&mesh,const Colors&colors,const Image&image,uint64_t budget=128'000'000,size_t mutate=0,size_t cancel=0,bool reverse=false){
 Result old,now;if(reverse){now=run(true,mesh,colors,image,budget,mutate,cancel);old=run(false,mesh,colors,image,budget,mutate,cancel);}else{old=run(false,mesh,colors,image,budget,mutate,cancel);now=run(true,mesh,colors,image,budget,mutate,cancel);}
 check(old.error==now.error,"Both algorithms preserve exact rejection/cancellation diagnostics");check(old.callbacks==now.callbacks,"Cancellation callback schedule remains exact");check(old.colors==now.colors&&old.image.rgba==now.image.rgba,"Callback-visible inputs mutate identically");
 if(old.error.empty()){
  check(old.value.tested_cells==now.value.tested_cells&&old.value.intersecting_cells==now.value.intersecting_cells,"Every tested/clipped intersecting cell count remains exact");
  for(size_t c=0;c<4;++c)check(std::bit_cast<uint64_t>(old.value.maximum_error[c])==std::bit_cast<uint64_t>(now.value.maximum_error[c]),"Every channel maximum remains bit exact");
  check(nlohmann::json::to_cbor(nlohmann::json(rounded(old.value)))==nlohmann::json::to_cbor(nlohmann::json(rounded(now.value))),"Outward-rounded serialized projection receipt bytes remain exact");
 }
 std::cout<<"DIFFERENTIAL "<<label<<" cells="<<old.value.tested_cells<<" intersecting="<<old.value.intersecting_cells<<" callbacks="<<old.callbacks<<" old_seconds="<<old.seconds<<" new_seconds="<<now.seconds<<" reverse="<<reverse<<" error="<<old.error<<std::endl;return now;
}
Mesh quad(){Mesh m;m.positions={{0,0,0},{1,0,0},{1,1,0},{0,1,0}};m.normals.assign(4,{0,0,1});m.uv={{0,0},{1,0},{1,1},{0,1}};m.indices={0,1,2,0,2,3};return m;}
void procedural(){
 std::mt19937 random(0x725ea100);const auto mesh=quad();
 for(size_t seed=0;seed<48;++seed){Colors colors(4);Image image{24,17,std::vector<uint8_t>(24*17*4)};for(auto&c:colors)for(auto&v:c)v=float(random()%1025)/1024;for(auto&v:image.rgba)v=uint8_t(random());
  switch(seed%4){case 0:for(auto&c:colors)c.fill(c[0]);for(size_t p=0;p<image.rgba.size();p+=4)std::fill_n(image.rgba.begin()+p,4,image.rgba[p]);break;
  case 1:for(auto&c:colors){c[1]=c[0];c[3]=c[2];}for(size_t p=0;p<image.rgba.size();p+=4){image.rgba[p+1]=image.rgba[p];image.rgba[p+3]=image.rgba[p+2];}break;
  case 2:for(auto&c:colors)c.fill(1);std::fill(image.rgba.begin(),image.rgba.end(),255);image.rgba[(8*24+12)*4+1]=23;break;default:break;}
  auto result=compare("procedural",mesh,colors,image,128'000'000,0,0,seed&1);check(result.error.empty(),"Finite neighboring/nonuniform fields are actually exercised");
 }
 Colors white(4,{1,1,1,1});Image image{64,64,std::vector<uint8_t>(64*64*4,255)};
 auto accepted=compare("white",mesh,white,image);check(accepted.error.empty(),"Equivalent channels accept");
 compare("exact-budget",mesh,white,image,accepted.value.tested_cells);auto short_budget=compare("one-cell-short",mesh,white,image,accepted.value.tested_cells-1);check(short_budget.error.find("budget")!=std::string::npos,"One-cell-tight work budget still refuses");
 auto changed=compare("callback-channel-change",mesh,white,image,128'000'000,3);check(changed.error.empty()&&changed.value.maximum_error[1]>changed.value.maximum_error[0],"A legally changed neighboring channel takes its distinct general path");
 auto cancelled=compare("callback-cancel",mesh,white,image,128'000'000,0,3);check(cancelled.error.find("cancelled")!=std::string::npos,"Original in-cell cancellation still refuses");
 auto loose=mesh;const float d=std::ldexp(1.f,-23);loose.uv={{0,0},{1,1-d},{1-d,1-2*d},{0,1}};auto lo=compare("ill-conditioned",loose,white,image);check(lo.error.empty()&&lo.value.maximum_error==std::array<double,4>{1,1,1,1},"Original conservative ill-conditioned policy remains");
 auto degenerate=mesh;degenerate.uv[1]=degenerate.uv[0];auto de=compare("degenerate",degenerate,white,image);check(!de.error.empty(),"Degenerate UV still refuses");
 auto invalid=white;invalid[0][0]=std::numeric_limits<float>::quiet_NaN();check(!compare("nonfinite",mesh,invalid,image).error.empty(),"Nonfinite authority still refuses");
 auto outside=mesh;outside.uv[0][0]=-.001f;check(!compare("outside",outside,white,image).error.empty(),"Non-atlas UV still refuses");
}
uint32_t word(const nlohmann::json::binary_t&b,size_t p){return uint32_t(b.at(p))|uint32_t(b.at(p+1))<<8|uint32_t(b.at(p+2))<<16|uint32_t(b.at(p+3))<<24;}
Mesh mesh_record(const nlohmann::json&j){
 Mesh m;const auto&p=j.at("positions").get_binary();const auto&n=j.at("normals").get_binary();const auto&uv=j.at("uv").get_binary();const auto&i=j.at("indices").get_binary();check(!p.empty()&&p.size()%12==0&&n.size()==p.size()&&uv.size()/8==p.size()/12&&uv.size()%8==0&&i.size()%12==0,"Actual persisted mesh buffers retain exact original dimensions");
 for(size_t x=0;x<p.size();x+=12)m.positions.push_back({std::bit_cast<float>(word(p,x)),std::bit_cast<float>(word(p,x+4)),std::bit_cast<float>(word(p,x+8))});
 for(size_t x=0;x<n.size();x+=12)m.normals.push_back({std::bit_cast<float>(word(n,x)),std::bit_cast<float>(word(n,x+4)),std::bit_cast<float>(word(n,x+8))});
 for(size_t x=0;x<uv.size();x+=8)m.uv.push_back({std::bit_cast<float>(word(uv,x)),std::bit_cast<float>(word(uv,x+4))});for(size_t x=0;x<i.size();x+=4)m.indices.push_back(word(i,x));
 if(j.contains("uv_pins")){const auto&v=j.at("uv_pins").get_binary();m.uv_pins.assign(v.begin(),v.end());}if(j.contains("uv_seams")){const auto&v=j.at("uv_seams").get_binary();m.uv_seams.assign(v.begin(),v.end());}m.validate();return m;
}
void actual(const std::filesystem::path&path){
 std::ifstream f(path,std::ios::binary);check(bool(f),"Actual immutable native input exists");std::vector<uint8_t>bytes{std::istreambuf_iterator<char>(f),{}};auto all=nlohmann::json::from_cbor(bytes);bytes.clear();bytes.shrink_to_fit();const auto*scene=&all;while(!scene->contains("objects"))scene=&scene->at("payload");
 const auto id=scene->at("active");const nlohmann::json*object=nullptr;for(const auto&o:scene->at("objects"))if(o.at("id")==id)object=&o;check(object,"Actual active target exists");const auto*paint=&object->at("painted");while(paint->contains("payload"))paint=&paint->at("payload");check(paint->at("resolution")==2048,"Actual full target remains2048");const auto mesh=mesh_record(paint->at("mesh"));
 const auto&materials=scene->at("source_materials");const auto&binding=materials.at("bindings").at(object->at("material_bindings").get<size_t>());const auto&preparation=materials.at("color_preparations").at(binding.at("color_preparation").get<size_t>());const auto receipt=preparation.at("receipt");check(receipt.at("source_field")==1&&receipt.at("resolution")==2048,"Actual source is explicit Identity field on original2048 grid");
 const auto&image=scene->at("source_images").at(paint->at("color_projection").at("image").get<size_t>());Image projected;projected.width=image.at("width");projected.height=image.at("height");const auto&rgba=image.at("rgba").get_binary();projected.rgba.assign(rgba.begin(),rgba.end());projected.validate();Colors colors(mesh.positions.size(),{1,1,1,1});
 auto result=compare("actual2048-forward",mesh,colors,projected);check(result.error.empty()&&result.value.tested_cells==5934012,"Every original full-size quality cell is retained");auto rebuilt=receipt;rebuilt["projection_error_bound"]=rounded(result.value);rebuilt["tested_quality_cells"]=result.value.tested_cells;check(nlohmann::json::to_cbor(rebuilt)==nlohmann::json::to_cbor(receipt),"Every original actual native receipt byte is preserved");
 auto reverse=compare("actual2048-reverse",mesh,colors,projected,128'000'000,0,0,true);check(reverse.error.empty()&&reverse.value.maximum_error==result.value.maximum_error,"Both evaluation orders retain exact original maxima");
 auto neighbor=projected;std::array<double,2>center{};for(size_t k=0;k<3;++k)for(size_t axis=0;axis<2;++axis)center[axis]+=double(mesh.uv.at(mesh.indices.at(k))[axis])/3.;const auto x=std::clamp(int(center[0]*projected.width),0,int(projected.width)-1),y=std::clamp(int(center[1]*projected.height),0,int(projected.height)-1);neighbor.rgba[(size_t(y)*neighbor.width+x)*4+1]=17;
 auto different=compare("actual2048-neighbor-G",mesh,colors,neighbor);check(different.error.empty()&&different.value.maximum_error[1]>paint->at("color_projection").at("error_bound")[1].get<double>(),"Differing neighboring channel rejects original tight claimed receipt");check(different.value.maximum_error[0]==result.value.maximum_error[0]&&different.value.maximum_error[2]==result.value.maximum_error[2]&&different.value.maximum_error[3]==result.value.maximum_error[3],"Other channels preserve their independent maxima");
 std::cout<<"ACTUAL_NATIVE_RECEIPT exact=1 cells="<<result.value.tested_cells<<" width="<<projected.width<<" height="<<projected.height<<std::endl;
}
}
int main(int argc,char**argv)try{if(argc>2)return 2;procedural();if(argc==2)actual(argv[1]);std::cout<<"PASS "<<checks<<" channel reuse differential controls"<<std::endl;return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
