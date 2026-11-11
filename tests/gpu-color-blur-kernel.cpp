// SPDX-License-Identifier: GPL-3.0-or-later
#include "paint-blur-gpu.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace dome::detail;
namespace {
size_t checks=0;double maximum=0;
void check(bool yes,const char*text){if(!yes)throw std::runtime_error(text);++checks;}
constexpr std::array<long double,15>weights={.034619L,.044859L,.055857L,.066833L,.076841L,.084894L,.090126L,.09194L,.090126L,.084894L,.076841L,.066833L,.055857L,.044859L,.034619L};
long double decode(long double c){return c<=.04045L?c/12.92L:std::pow((c+.055L)/1.055L,2.4L);}
long double encode(long double c){return c<=.0031308L?c*12.92L:1.055L*std::pow(c,1/2.4L)-.055L;}
void compare(const ColorBlurInput&in,const ColorBlurOutput&out,float strength){
 for(size_t y=0;y<32;++y)for(size_t x=0;x<32;++x){
  const size_t p=(y+7)*46+x+7,i=y*32+x;std::array<std::array<long double,4>,3>sum{},old{};long double weight=0;
  for(size_t plane=0;plane<2;++plane){for(size_t c=0;c<3;++c)old[plane][c]=decode(in.color[plane][p][c])*in.color[plane][p][3];old[plane][3]=in.color[plane][p][3];}
  old[2]={static_cast<long double>(in.metadata[p][0])*in.color[0][p][3],static_cast<long double>(in.metadata[p][1])*in.color[0][p][3],0,in.color[0][p][3]};
  for(int d=-7;d<=7;++d){
   const size_t positions[]={size_t(int(p)+d),size_t(int(p)+46*d)};
   for(auto q:positions)if(in.metadata[p][3]!=0&&in.metadata[q][3]==in.metadata[p][3]){
    long double w=weights[d+7]/2;weight+=w;
    for(size_t plane=0;plane<2;++plane){for(size_t c=0;c<3;++c)sum[plane][c]+=w*decode(in.color[plane][q][c])*in.color[plane][q][3];sum[plane][3]+=w*in.color[plane][q][3];}
    sum[2][0]+=w*in.metadata[q][0]*in.color[0][q][3];sum[2][1]+=w*in.metadata[q][1]*in.color[0][q][3];sum[2][3]+=w*in.color[0][q][3];
   }
  }
  long double gate=in.metadata[p][3]?static_cast<long double>(in.metadata[p][2])*strength:0;
  for(size_t plane=0;plane<3;++plane)for(size_t c=0;c<4;++c){
   long double expected=weight>0?old[plane][c]+(sum[plane][c]/weight-old[plane][c])*gate:old[plane][c];
   const auto alpha=weight>0?old[plane][3]+(sum[plane][3]/weight-old[plane][3])*gate:old[plane][3];
   if(c<3&&plane<2)expected=alpha>0?encode(expected/alpha):in.color[plane][p][c];
   if(c<2&&plane==2)expected=alpha>0?expected/alpha:in.metadata[p][c];
   const double error=std::abs(double(out.planes[plane][i][c])-double(expected));maximum=std::max(maximum,error);
   check(std::isfinite(out.planes[plane][i][c])&&error<=std::max(2e-12,std::abs(double(expected))*3e-6),"Independent associated RGB/authored-alpha/FLOAT coverage oracle");
  }
 }
}
}
int main(int argc,char**argv)try{
 const bool software=argc>1&&std::string(argv[1])=="software";ColorBlurGpu gpu(software);
 std::cout<<"DEVICE "<<gpu.device()<<'\n';
 if(!software){check(gpu.device().find("Arc")!=std::string::npos&&gpu.device().find("/dev/dri/renderD129")!=std::string::npos,"Strict test Arc renderer and authenticated EGLDevice node");const auto sys=std::filesystem::path("/sys/class/drm/renderD129/device");std::string vendor,device;std::ifstream(sys/"vendor")>>vendor;std::ifstream(sys/"device")>>device;check(std::filesystem::canonical(sys).filename()=="0000:03:00.0"&&vendor=="0x8086"&&device=="0x56a0","Strict test PCI vendor/device identity");}
 auto in=std::make_unique<ColorBlurInput>();auto out=std::make_unique<ColorBlurOutput>();
 for(int fixture=0;fixture<5;++fixture){
  for(size_t y=0;y<46;++y)for(size_t x=0;x<46;++x){const size_t i=y*46+x;float a=float((x*7+y*13)%29)/29;
   if(fixture==1)a=float((x+y)%3+1)*1e-7f;
   if(fixture==2)a=0;
   in->color[0][i]={float((x*5+y)%17)/17,float((x+y*3)%23)/23,float((x*7+y*2)%13)/13,a};
   in->color[1][i]={float((x*2+y*3)%19)/19,float((x*3+y)%11)/11,float((x+y*7)%31)/31,a};
   in->metadata[i]={float((x*3+y)%7)/7,float((x+y*2)%11)/11,float((x*7+y)%17)/16,float(x<23?1:2)};
   if(fixture==3&&x%3==0)in->metadata[i][3]=0;
   if(fixture==4)in->metadata[i][2]=0;
  }
  gpu.run(*in,*out,.73f,{});compare(*in,*out,.73f);
  if(fixture==1)check(out->planes[0][0][3]>0&&out->planes[0][0][3]<1.f/255,"GPU retains subUNORM8 coverage before deliberate native byte-channel encoding");
 }
 bool cancelled=false;try{gpu.run(*in,*out,.73f,[]{return true;});}catch(const std::runtime_error&){cancelled=true;}
 check(cancelled,"Cancelled dispatch refuses without CPU fallback");
 std::cout<<"PASS "<<checks<<" native GPU blur kernel controls max_error="<<maximum<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
