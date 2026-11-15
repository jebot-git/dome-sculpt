// SPDX-License-Identifier: GPL-3.0-or-later
#include "render-paint-overlay.hpp"
#include "graphics.h"
#include <iostream>
#include <cmath>
namespace {
using namespace dome;
size_t checks=0;
void check(bool v,const char *s){++checks;if(!v)throw std::runtime_error(s);}
Mesh quad(){Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
Image image(std::array<uint8_t,4> v){Image i{64,64,std::vector<uint8_t>(4096*4)};for(size_t n=0;n<i.rgba.size();++n)i.rgba[n]=v[n%4];return i;}
double linear(double x){return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);}
std::array<std::vector<float>,5> read(const detail::PaintOverlayCompiler::Resource &r){
  GLuint fbo=0;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);std::array<std::vector<float>,5> out;
  for(size_t p=0;p<5;++p){glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,r.planes[p],0);glReadBuffer(GL_COLOR_ATTACHMENT0);if(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("FLOAT test read framebuffer incomplete");out[p].resize(4096*4);glReadPixels(0,0,64,64,GL_RGBA,GL_FLOAT,out[p].data());}
  glBindFramebuffer(GL_READ_FRAMEBUFFER,0);glDeleteFramebuffers(1,&fbo);check(glGetError()==GL_NO_ERROR,"fixture-only FLOAT plane reads succeed");return out;
}
}
int main()try{
  using namespace dome;
  NativeGraphics graphics;std::cout<<"GL_VENDOR="<<glGetString(GL_VENDOR)<<" GL_RENDERER="<<glGetString(GL_RENDERER)<<'\n';
  for(bool visible:{false,true}){
    auto paint=std::make_shared<PaintedMesh>(quad(),64);
    BakedTextures baked;baked.maps.color=image({33,92,170,192});baked.maps.metal_rough=image({181,72,24,255});baked.coverage.resize(4096);
    std::vector<BakedEmissionSample> values(4096);
    for(size_t i=0;i<4096;++i){const float a=i%3==0?0x1p-24f:i%3==1?.25f:.9f;baked.coverage[i]=i%3==0?0:i%3==1?128:255;values[i]={{a*.25f,a*.5f,0},{50000*a,.00001f*a,2*a},a};}
    baked.emission_field=BakedEmissionField::create(64,values);paint->install_bake(std::move(baked));
    PaintSettings ink;ink.channel=PaintChannel::Emission;ink.emission_units=PaintEmissionUnits::Physical;ink.emission={7,.005f,4};ink.front_only=false;
    Brush brush;brush.from=brush.to={};brush.normal={0,0,1};brush.radius=2;brush.strength=.73f;
    paint->add_layer("Physical overlay");paint->begin_stroke();paint->stamp(brush,ink);check(paint->end_stroke(),"genuine editable physical overlay created");paint->layer_properties("Physical overlay",visible,.375f);
    const auto packet=detail::PaintOverlayFactory::capture_native_emission(*paint);
    detail::PaintOverlayCompiler compiler;const auto demand=detail::PaintOverlayCompiler::demand(packet,false);
    bool refused=false;try{compiler.prepare({{packet,false}},demand.bytes-1,13);}catch(const std::length_error&){refused=true;}
    check(refused&&compiler.size()==0,"FLOAT input bytes refuse exact forecast-minus-one before publication");
    compiler.prepare({{packet,false}},demand.bytes,13);
    const auto pixels=read(compiler.at(packet,false));
    for(size_t i=0;i<4096;++i){const auto owner=packet->padding()->at(i);check(owner>=0,"real atlas owns projected test texel");const size_t index=size_t(owner);const auto &base=values[index];const double common=paint->baked()->coverage[index]/255.;
      std::array<std::array<double,4>,5> expected{};
      const std::array<uint8_t,4> colour{33,92,170,192},orm{181,72,24,255};
      for(size_t k=0;k<4;++k){expected[0][k]=(k<3?linear(colour[k]/255.):colour[k]/255.)*common;expected[3][k]=common;}
      for(size_t k=0;k<3;++k){expected[1][k]=orm[k]/255.*common;expected[2][k]=base.legacy[k];expected[4][k]=base.absolute[k];}expected[1][3]=common;expected[3][3]=base.coverage;
      if(visible){const auto x=uint32_t(index%64),y=uint32_t(index/64);const auto q=paint->emission_pixel(paint->active_layer(),x,y);const double a=double(q.coverage)*paint->pixel(paint->active_layer(),x,y).data[13]/255.*.375;
        for(size_t k=0;k<3;++k){expected[2][k]=expected[2][k]*(1-a)+double(q.legacy[k])*a;expected[4][k]=expected[4][k]*(1-a)+double(q.physical[k])*a;}expected[3][3]=expected[3][3]*(1-a)+a;
      }
      expected[2][3]=expected[4][3]=expected[3][3];
      for(size_t p=0;p<5;++p)for(size_t k=0;k<4;++k){const double actual=pixels[p][i*4+k],wanted=expected[p][k];check(std::isfinite(actual)&&std::abs(actual-wanted)<=std::max(1e-30,std::abs(wanted)*2e-6),"actual GPU FLOAT contributions agree with independent binary64 source/layer oracle");}
    }
    compiler.clear(true);check(compiler.size()==0&&compiler.bytes()==0,"actual FLOAT GPU stores release completely");
  }
  std::cout<<"Baked emission GPU "<<checks<<" checks PASS\n";return 0;
}catch(const std::exception &e){std::cerr<<"Baked emission GPU FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
