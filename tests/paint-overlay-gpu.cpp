// SPDX-License-Identifier: GPL-3.0-or-later
#include "render-paint-overlay.hpp"
#include "graphics.h"
#include "dome/color-preparation.hpp"
#include "source-color-fixtures.hpp"
#include <iostream>
using namespace dome;
namespace {
unsigned checks=0;double maximum=0;
void check(bool value,const char *message){if(!value)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<'\n';}
bool refuses(auto f){try{f();}catch(const std::exception&){return true;}return false;}
double srgb(double x){return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);}
Mesh quad(){Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
void stroke(PaintedMesh &paint,PaintSettings setting,float strength=.73f){Brush b;b.from=b.to={0,0,0};b.normal={0,0,1};b.radius=2;b.strength=strength;setting.front_only=false;paint.begin_stroke();paint.stamp(b,setting);check(paint.end_stroke(),"Real native brush edits sparse COW data");}
using Values=std::array<std::array<double,4>,5>;
Values oracle(const PaintedMesh &paint,uint32_t x,uint32_t y,bool pbr){
 Values v{};if(auto baked=paint.baked()){size_t i=(size_t(y)*paint.resolution()+x)*4;double a=baked->coverage[i/4]/255.;const auto &colour=pbr&&baked->color_interpretation==BakedColorInterpretation::ResolvedPbr?baked->maps.pbr_color:baked->maps.color;
  for(size_t c=0;c<4;++c)v[0][c]=(c<3?srgb(colour.rgba[i+c]/255.):colour.rgba[i+c]/255.)*a;
  for(size_t c=0;c<3;++c){v[1][c]=baked->maps.metal_rough.rgba[i+c]/255.*a;v[2][c]=srgb(baked->maps.emission.rgba[i+c]/255.)*a;}
  v[1][3]=a;v[3].fill(a);v[2][3]=v[4][3]=a;
 }
 for(const auto &layer:paint.layers())if(layer.visible&&layer.opacity>0){auto q=paint.pixel(layer.id,x,y);double mask=q.data[13]/255.*layer.opacity;std::array<double,4>a{mask*q.data[9]/255.,mask*q.data[10]/255.,mask*q.data[11]/255.,mask*q.data[12]/255.};auto color=pbr?paint.pbr_pixel(layer.id,x,y):std::array<uint8_t,4>{q.data[0],q.data[1],q.data[2],q.data[3]};
  for(size_t c=0;c<4;++c)v[0][c]=v[0][c]*(1-a[0])+(c<3?srgb(color[c]/255.):color[c]/255.)*a[0];
  v[1][1]=v[1][1]*(1-a[2])+q.data[5]/255.*a[2];v[1][2]=v[1][2]*(1-a[1])+q.data[4]/255.*a[1];
  const auto value=paint.emission_pixel(layer.id,x,y);a[3]=mask*value.coverage;
  for(size_t c=0;c<3;++c){v[2][c]=v[2][c]*(1-a[3])+double(value.legacy[c])*a[3];v[4][c]=v[4][c]*(1-a[3])+double(value.physical[c])*a[3];}
  for(size_t c=0;c<4;++c)v[3][c]=v[3][c]*(1-a[c])+a[c];
 }
 v[2][3]=v[4][3]=v[3][3];return v;
}
std::array<std::vector<float>,5> read(const detail::PaintOverlayCompiler::Resource &resource){
 GLuint fbo=0;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);std::array<std::vector<float>,5> pixels;
 for(size_t plane=0;plane<5;++plane){glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,resource.planes[plane],0);glReadBuffer(GL_COLOR_ATTACHMENT0);if(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Fixture float read framebuffer incomplete");pixels[plane].resize(size_t(resource.resolution)*resource.resolution*4);glReadPixels(0,0,GLsizei(resource.resolution),GLsizei(resource.resolution),GL_RGBA,GL_FLOAT,pixels[plane].data());}
 glBindFramebuffer(GL_READ_FRAMEBUFFER,0);glDeleteFramebuffers(1,&fbo);if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Fixture float plane readback failed");return pixels;
}
void compare(const PaintedMesh &paint,const detail::PaintOverlayCompiler::Resource &resource,bool pbr){const auto pixels=read(resource);double error=0;const auto r=paint.resolution();for(uint32_t y=0;y<r;++y)for(uint32_t x=0;x<r;++x){auto expected=oracle(paint,x,y,pbr);for(size_t plane=0;plane<5;++plane)for(size_t c=0;c<4;++c){double delta=std::abs(pixels[plane][(size_t(y)*r+x)*4+c]-expected[plane][c]);double relative=delta/std::max(plane==4?1e-6:1.,std::abs(expected[plane][c]));error=std::max(error,relative);}}
 maximum=std::max(maximum,error);std::cout<<"FLOAT_RELATIVE_MAX="<<error<<'\n';check(error<4e-6,"Every GPU contribution texel matches independent double sparse-layer arithmetic");}
BakedTextures baked(uint32_t r){BakedTextures b;auto image=[&](std::array<uint8_t,4> value){Image im;im.width=im.height=r;im.rgba.resize(size_t(r)*r*4);for(size_t i=0;i<im.rgba.size();i+=4)std::copy(value.begin(),value.end(),im.rgba.begin()+i);return im;};b.maps.color=image({40,90,210,170});b.maps.metal_rough=image({190,40,230,255});b.maps.emission=image({20,40,70,255});b.maps.normal=image({128,128,255,255});b.coverage.resize(size_t(r)*r);for(uint32_t y=0;y<r;++y)for(uint32_t x=0;x<r;++x)b.coverage[size_t(y)*r+x]=uint8_t((x+y*2)%256);b.validate(r);return b;}
}
int main(int argc,char **argv)try{
 NativeGraphics graphics;std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GL_VENDOR="<<vendor<<" GL_RENDERER="<<device<<'\n';
 check(argc<2||std::string_view(argv[1])!="--hardware"||(vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos&&device.find("llvmpipe")==std::string::npos&&device.find("softpipe")==std::string::npos&&device.find("UHD")==std::string::npos),"Hardware requests require actual Intel Arc A770");
 detail::PaintOverlayCompiler compiler;auto paint=std::make_shared<PaintedMesh>(quad(),64);
 PaintSettings setting;setting.color={.2f,.7f,.3f,.4f};stroke(*paint,setting);setting.channel=PaintChannel::Metallic;setting.metallic=.8f;stroke(*paint,setting);setting.channel=PaintChannel::Roughness;setting.roughness=.1f;stroke(*paint,setting);setting.channel=PaintChannel::Emission;setting.emission={.3f,.8f,.2f};stroke(*paint,setting);
 paint->add_layer("Absolute physical");setting.emission_units=PaintEmissionUnits::Physical;setting.emission={10000,.005f,12};stroke(*paint,setting);paint->layer_properties("Absolute physical",true,.61f);
 auto packet=detail::PaintOverlayFactory::capture(*paint);auto same_padding=detail::PaintOverlayFactory::capture(*paint);check(packet->padding()==same_padding->padding()&&!packet->padding().owner_before(same_padding->padding())&&!same_padding->padding().owner_before(packet->padding()),"Packets alias the real immutable padding owner without dense copying");
 const auto original=paint->encode();glEnable(GL_BLEND);glEnable(GL_SCISSOR_TEST);glViewport(2,3,17,19);compiler.prepare({{packet,false},{packet,false}});GLint viewport[4]{};glGetIntegerv(GL_VIEWPORT,viewport);check(glIsEnabled(GL_BLEND)&&glIsEnabled(GL_SCISSOR_TEST)&&viewport[0]==2&&viewport[1]==3&&viewport[2]==17&&viewport[3]==19,"GPU compiler restores caller viewport and render switches");glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);check(compiler.size()==1,"Repeated immutable inputs reuse one five-plane owner");compare(*paint,compiler.at(packet,false),false);
 auto pixels=read(compiler.at(packet,false));const size_t center=(32*64+32)*4;check(pixels[4][center+1]>0&&pixels[4][center]>100,"Tiny and HDR physical radiance both survive float GPU composition");
 check(refuses([&]{compiler.prepare({{packet,false}},8);})&&compiler.size()==1,"Budget refusal precedes cache mutation and GPU allocation");check(paint->encode()==original,"GPU compilation leaves all native paint/source bytes exact");
 paint->begin_stroke();setting.erase=true;paint->stamp(Brush{{0,0,0},{0,0,0},{0,0,1},2,.3f},setting);paint->cancel_stroke();check(paint->encode()==original,"Cancel restores exact physical/legacy owners before reusing a GPU packet");
 auto based=paint->snapshot();based->install_bake(baked(64));auto bp=detail::PaintOverlayFactory::capture(*based);compiler.prepare({{bp,false}});compare(*based,compiler.at(bp,false),false);
 auto scene=source_color_fixture::model(true);auto &o=scene.active();PaintSettings white;white.color={1,1,1,1};stroke(*o.painted,white,.99f);auto prepared=prepare_source_colors({o.painted,o.appearance,o.material_bindings,o.deformation,o.material});auto pp=detail::PaintOverlayFactory::capture(*prepared.paint);check(pp->source_maps()==prepared.paint->source_maps(),"Packet retains the exact optional original source-map identity");compiler.prepare({{pp,false},{pp,true}});compare(*prepared.paint,compiler.at(pp,false),false);compare(*prepared.paint,compiler.at(pp,true),true);auto a=read(compiler.at(pp,false)),b=read(compiler.at(pp,true));double colour_delta=0;for(size_t i=0;i<a[0].size();++i)colour_delta=std::max(colour_delta,std::abs(double(a[0][i])-b[0][i]));check(colour_delta>.1,"Real prepared zero-colour source distinguishes PBR and authored overlay planes");
 auto recursive=baked(64);recursive.maps.overlay=pp;check(refuses([&]{recursive.validate(64);}),"Baked payload cannot recursively own an overlay packet");
 check(refuses([&]{detail::PaintOverlayFactory::capture(*paint,[]{return true;});})&&paint->encode()==original,"Factory cancellation preserves exact caller state");compiler.clear(true);check(compiler.size()==0&&compiler.bytes()==0,"Owned GPU plane resources release completely");std::cout<<"Sparse GPU overlay: "<<checks<<" checks maximum_relative="<<maximum<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
