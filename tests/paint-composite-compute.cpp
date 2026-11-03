// SPDX-License-Identifier: GPL-3.0-or-later
#include "paint-composite-backend.hpp"
#include "dome/paint.hpp"
#include "dome/brush.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <random>
#ifdef DOME_TEST_SYCL
#include <sycl/sycl.hpp>
#endif
using namespace dome;
using namespace dome::paint_compute;
namespace {
size_t checks=0;void check(bool b,const char*m){++checks;if(!b)throw std::runtime_error(m);}
uint8_t byte(float v){return uint8_t(std::lround(std::clamp(v,0.f,1.f)*255));}
float unit(uint8_t v){return float(v)/255;}
Output reference(const Input &in,size_t at){
 const auto &p=in.parameters;float c[]{p.color[0],p.color[1],p.color[2],p.color[3]},e[]{p.emission[0],p.emission[1],p.emission[2]},rough=p.roughness,metal=p.metallic,cc=0,ec=0;const auto owner=in.padding[at];
 if(owner>=0){const auto x=uint32_t(owner)%p.resolution,y=uint32_t(owner)/p.resolution,t=(y/32)*((p.resolution+31)/32)+x/32,pixel=(y%32)*32+x%32;
  for(const auto &l:in.layers){auto id=in.tiles[l.first_tile+t];if(id==UINT32_MAX)continue;const auto &v=in.pixels[size_t(id)*1024+pixel];float mask=unit(v.data[13])*l.opacity;
   auto blend=[&](float base,int data,int coverage,bool srgb){float a=mask*unit(v.data[coverage]),paint=unit(v.data[data]);if(srgb)paint=srgb_to_linear(paint);return base*(1-a)+paint*a;};
   float a=mask*unit(v.data[9]);cc=cc*(1-a)+a;a=mask*unit(v.data[12]);ec=ec*(1-a)+a;
   for(int k=0;k<4;++k)c[k]=blend(c[k],k,9,k<3);rough=blend(rough,5,11,false);metal=blend(metal,4,10,false);for(int k=0;k<3;++k)e[k]=blend(e[k],6+k,12,true);
  }
 }
 Output o;for(int k=0;k<3;++k){o.color[k]=byte(linear_to_srgb(c[k]));o.emission[k]=byte(linear_to_srgb(e[k]));}o.color[3]=byte(c[3]);o.orm[0]=255;o.orm[1]=byte(rough);o.orm[2]=byte(metal);o.orm[3]=o.emission[3]=255;o.color_coverage=byte(cc);o.emission_coverage=byte(ec);o.transparent=c[3]<.999f;return o;
}
Input fixture(uint32_t r,uint32_t layers,uint32_t seed){
 Input in;auto &p=in.parameters;p.resolution=r;p.tiles=((r+31)/32)*((r+31)/32);p.layers=layers;p.color[0]=.23f;p.color[1]=.61f;p.color[2]=.037f;p.color[3]=seed&1?.999f:1;p.emission[0]=.013f;p.emission[1]=.037f;p.emission[2]=.071f;p.metallic=.31f;p.roughness=.68f;
 std::mt19937 rng(seed);in.padding.resize(size_t(r)*r);for(size_t i=0;i<in.padding.size();++i)in.padding[i]=i%17?int32_t((i*37)%in.padding.size()):-1;
 for(uint32_t li=0;li<layers;++li){in.layers.push_back({li%3==0?.73f:li%3==1?.37f:1,uint32_t(in.tiles.size())});for(uint32_t t=0;t<p.tiles;++t){if((t+li)%5==0){in.tiles.push_back(UINT32_MAX);continue;}in.tiles.push_back(uint32_t(in.pixels.size()/1024));for(size_t i=0;i<1024;++i){Pixel pixel;for(auto &b:pixel.data)b=uint8_t(rng());if(i%11==0)pixel.data[13]=0;in.pixels.push_back(pixel);}}}
 return in;
}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception&e){std::cout<<"REFUSAL "<<e.what()<<'\n';return true;}}
}
int main(int argc,char**argv)try{
 auto in=fixture(64,3,17);if(argc>1&&std::string_view(argv[1])=="--missing"){check(refuses([&]{paint_compute::composite(in);}),"Missing selected device silently returned CPU paint composite");std::cout<<"PASS selected-backend missing-device refusal\n";return 0;}
#ifdef DOME_TEST_SYCL
 sycl::device device{sycl::gpu_selector_v};auto name=device.get_info<sycl::info::device::name>();auto pci=device.get_info<sycl::ext::intel::info::device::pci_address>();std::cout<<"ACTUAL_GPU "<<name<<" pci="<<pci<<'\n';if(std::getenv("DOME_REQUIRE_ARC_PCI03"))check(name.find("Arc")!=std::string::npos&&name.find("A770")!=std::string::npos&&pci=="0000:03:00.0","Wrong requested paint compute device");
#endif
 const auto &q=quantization();std::mt19937 rng(0x832ec5);
 for(size_t i=0;i<200000;++i){float v=float(rng())/float(UINT32_MAX);check(srgb_byte(v,q)==byte(linear_to_srgb(v)),"Canonical float-to-sRGB byte map differs from native pow/round");check(linear_byte(v)==byte(v),"Linear byte rounding differs at float half boundary");}
 for(auto t:q.threshold)for(int offset=-8;offset<=8;++offset){float v=std::bit_cast<float>(uint32_t(int64_t(t)+offset));check(srgb_byte(v,q)==byte(linear_to_srgb(v)),"sRGB byte threshold neighbor lost exact canonical rounding");}
 for(uint32_t r:{32u,64u,288u})for(uint32_t n:{0u,1u,3u,7u}){
  in=fixture(r,n,r+n);const auto original=in.pixels;const auto padding=in.padding;const auto tiles=in.tiles;const auto out=paint_compute::composite(in);
  check(out.size()==padding.size(),"Composite output size mismatch");for(size_t at=0;at<out.size();++at){const auto expected=reference(in,at);if(std::memcmp(&out[at],&expected,sizeof(Output))){std::cerr<<"DIFF backend="<<compute_backend()<<" r="<<r<<" layers="<<n<<" pixel="<<at<<'\n';throw std::runtime_error("Selected composite differs from independent native14-channel image oracle");}++checks;}
  check(in.pixels.size()==original.size()&&(original.empty()||!std::memcmp(in.pixels.data(),original.data(),original.size()*sizeof(Pixel)))&&in.padding==padding&&in.tiles==tiles,"Composite mutated immutable input data/owners");
 }
 in=fixture(64,3,99);for(int i=0;i<5;++i){auto bad=in;if(i==0)bad.padding[0]=-2;if(i==1)bad.tiles[1]=UINT32_MAX-1;if(i==2)bad.layers[0].first_tile=1;if(i==3)bad.parameters.color[0]=NAN;if(i==4)bad.parameters.resolution=4096;check(refuses([&]{paint_compute::composite(bad);}),"Invalid packet was dispatched");}
 auto saved=in.pixels;size_t total=0;paint_compute::composite(in,[&]{++total;return false;});size_t polls=0;check(refuses([&]{paint_compute::composite(in,[&]{return ++polls>=total;});}),"Cancelled composite returned staged pixels");check(in.pixels.size()==saved.size()&&!std::memcmp(in.pixels.data(),saved.data(),saved.size()*sizeof(Pixel)),"Cancellation changed original paint data");check(paint_compute::composite(in).size()==in.padding.size(),"Fresh composite retry failed after cancellation");
 std::cout<<"PASS "<<checks<<" exact paint-composite checks on "<<compute_backend()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
