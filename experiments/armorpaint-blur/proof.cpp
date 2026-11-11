// SPDX-License-Identifier: GPL-3.0-or-later
// Independent binary64 validation consumer; no oracle calls adapter/shader code.
#include "blur_adapter.hpp"
#include "graphics.h"
#include <EGL/eglext.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
using armorpaint_proof::Blur;
// Test-only API failure injection; exercise partial acquisition without forcing
// global driver OOM. No wrapper is part of the reusable adapter static library.
namespace {int fail_kind=0,shader_calls=0;std::array<int,4> live{};}
extern "C" {
GLuint __real_glCreateShader(GLenum);void __real_glDeleteShader(GLuint);
GLuint __real_glCreateProgram();void __real_glDeleteProgram(GLuint);
void __real_glGenFramebuffers(GLsizei,GLuint*);void __real_glDeleteFramebuffers(GLsizei,const GLuint*);
void __real_glGenVertexArrays(GLsizei,GLuint*);void __real_glDeleteVertexArrays(GLsizei,const GLuint*);
GLuint __wrap_glCreateShader(GLenum kind){++shader_calls;if(fail_kind==1||(fail_kind==2&&shader_calls==2))return 0;auto n=__real_glCreateShader(kind);if(n)++live[0];return n;}
void __wrap_glDeleteShader(GLuint n){if(n)--live[0];__real_glDeleteShader(n);}
GLuint __wrap_glCreateProgram(){if(fail_kind==3)return 0;auto n=__real_glCreateProgram();if(n)++live[1];return n;}
void __wrap_glDeleteProgram(GLuint n){if(n)--live[1];__real_glDeleteProgram(n);}
void __wrap_glGenFramebuffers(GLsizei count,GLuint* n){if(fail_kind==4){std::fill(n,n+count,0);return;}__real_glGenFramebuffers(count,n);for(int i=0;i<count;++i)if(n[i])++live[2];}
void __wrap_glDeleteFramebuffers(GLsizei count,const GLuint* n){for(int i=0;i<count;++i)if(n[i])--live[2];__real_glDeleteFramebuffers(count,n);}
void __wrap_glGenVertexArrays(GLsizei count,GLuint* n){if(fail_kind==5){std::fill(n,n+count,0);return;}__real_glGenVertexArrays(count,n);for(int i=0;i<count;++i)if(n[i])++live[3];}
void __wrap_glDeleteVertexArrays(GLsizei count,const GLuint* n){for(int i=0;i<count;++i)if(n[i])--live[3];__real_glDeleteVertexArrays(count,n);}
}
namespace {
int assertions=0;double maximum_relative=0,maximum_absolute=0;
void check(bool v,const char* m){++assertions;if(!v)throw std::runtime_error(m);}
bool refuses(auto f){try{f();}catch(const std::runtime_error&){return true;}return false;}
struct Texture {
 GLuint id=0;int width,height,channels;
 Texture(int w,int h,GLenum format,int n,const std::vector<float>& data):width(w),height(h),channels(n){glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexImage2D(GL_TEXTURE_2D,0,format,w,h,0,n==1?GL_RED:GL_RGBA,GL_FLOAT,data.empty()?nullptr:data.data());}
 ~Texture(){glDeleteTextures(1,&id);}
 std::vector<float> read()const{std::vector<float> v(std::size_t(width)*height*channels);glBindTexture(GL_TEXTURE_2D,id);glGetTexImage(GL_TEXTURE_2D,0,channels==1?GL_RED:GL_RGBA,GL_FLOAT,v.data());return v;}
};
// Decimal coefficients are an independent specification transcription, not
// generated from the upstream C string or read from generated shader output.
constexpr std::array<long double,15> coefficients={.034619L,.044859L,.055857L,.066833L,.076841L,.084894L,.090126L,.09194L,.090126L,.084894L,.076841L,.066833L,.055857L,.044859L,.034619L};
std::vector<long double> oracle(int w,int h,const std::vector<float>& source,const std::vector<float>& mask,float strength){
 std::vector<long double> out(std::size_t(w)*h*4);
 for(int y=0;y<h;++y)for(int x=0;x<w;++x){
  std::array<long double,4> sum{};
  // Independent gather written as a source-index/coefficient list.
  for(int offset=-7;offset<=7;++offset){
   const auto p=std::size_t(std::clamp(x+offset,0,w-1)+y*w)*4;
   const auto q=std::size_t(x+std::clamp(y+offset,0,h-1)*w)*4;
   long double weight=coefficients[offset+7]/2;
   for(int c=0;c<3;++c)sum[c]+=weight*(static_cast<long double>(source[p+c])*source[p+3]+static_cast<long double>(source[q+c])*source[q+3]);
   sum[3]+=weight*(static_cast<long double>(source[p+3])+source[q+3]);
  }
  auto i=std::size_t(x+y*w)*4;long double gate=static_cast<long double>(mask[i/4])*strength;
  for(int c=0;c<4;++c){long double old=c==3?source[i+3]:static_cast<long double>(source[i+c])*source[i+3];out[i+c]=old+(sum[c]-old)*gate;}
 }
 return out;
}
void verify(const std::vector<float>& got,const std::vector<long double>& expected){
 check(got.size()==expected.size(),"output length");
 for(std::size_t i=0;i<got.size();++i){double e=double(expected[i]),error=std::abs(double(got[i])-e);check(std::isfinite(got[i]),"finite float result");maximum_absolute=std::max(maximum_absolute,error);if(std::abs(e)>1e-20)maximum_relative=std::max(maximum_relative,error/std::abs(e));check(error<=std::max(2e-12,std::abs(e)*3e-6),"GPU independently matches associated RGB/FLOAT coverage");}
}
}
int main(int argc,char** argv)try{
 if(argc!=3)throw std::runtime_error("usage: proof software|arc output-prefix");
 NativeGraphics graphics;
 const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR));
 const std::string renderer=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
 std::cout<<"VENDOR="<<vendor<<" RENDERER="<<renderer<<'\n';
 // Authenticate the device backing the CURRENT EGL display, rather than trust
 // DRI_PRIME or the human-readable GL renderer string alone.
 auto query_display=reinterpret_cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(eglGetProcAddress("eglQueryDisplayAttribEXT"));
 auto query_device=reinterpret_cast<PFNEGLQUERYDEVICESTRINGEXTPROC>(eglGetProcAddress("eglQueryDeviceStringEXT"));
 EGLAttrib device=0;check(query_display&&query_device&&query_display(graphics.display,EGL_DEVICE_EXT,&device),"current EGLDisplay has authenticated EGLDevice");
 const char* node=query_device(reinterpret_cast<EGLDeviceEXT>(device),EGL_DRM_RENDER_NODE_FILE_EXT);
 std::string drm=node?node:"",pci,vendor_id,device_id;
 if(!drm.empty()){
  auto sys=std::filesystem::path("/sys/class/drm")/std::filesystem::path(drm).filename()/"device";
  pci=std::filesystem::canonical(sys).filename().string();std::ifstream(sys/"vendor")>>vendor_id;std::ifstream(sys/"device")>>device_id;
 }
 std::cout<<"EGL_DRM_RENDER_NODE="<<drm<<" PCI="<<pci<<" VENDOR_ID="<<vendor_id<<" DEVICE_ID="<<device_id<<'\n';
 if(std::string(argv[1])=="arc")check(renderer.find("Arc")!=std::string::npos&&renderer.find("A770")!=std::string::npos&&renderer.find("llvmpipe")==std::string::npos&&drm=="/dev/dri/renderD129"&&pci=="0000:03:00.0"&&vendor_id=="0x8086"&&device_id=="0x56a0","strict Arc EGLDevice/DRM/PCI hardware identity");
 else check(renderer.find("llvmpipe")!=std::string::npos,"strict software identity");
 for(int failure=1;failure<=5;++failure){auto before=live;shader_calls=0;fail_kind=failure;check(refuses([]{Blur candidate;}),"zero-object GPU API allocation failure refused");fail_kind=0;check(live==before&&glGetError()==GL_NO_ERROR,"partial acquisition releases every tracked GL object");}
 Blur blur;std::vector<float> all_results;
 const int w=33,h=17;
 for(int test=0;test<7;++test){
  std::vector<float> source(w*h*4),mask(w*h);
  for(int y=0;y<h;++y)for(int x=0;x<w;++x){auto i=std::size_t(x+y*w);float a=float((x*13+y*7)%31)/31;
   source[i*4]=float((x*17+y*11)%53)/7;source[i*4+1]=float((x*7+y*19)%67)/29;source[i*4+2]=float((x*3+y*5)%43)/37;source[i*4+3]=a;
   mask[i]=float((x*5+y*3)%19)/18;
   if(test==0){source[i*4]=10000;source[i*4+1]=.0005f;source[i*4+2]=7;source[i*4+3]=.000002f;}
   if(test==2){source[i*4+3]=0;}
   if(test==3){mask[i]=0;}
   if(test==4){source[i*4]=source[i*4+1]=source[i*4+2]=0;source[i*4+3]=0;mask[i]=1;if(x==1&&y==h-2){source[i*4]=6;source[i*4+3]=.00013f;}}
   if(test==5){source[i*4+3]=float((x+y)%3+1)*1e-7f;}
  }
  const float strength=test==6?0:.73f;
  Texture src(w,h,GL_RGBA32F,4,source),msk(w,h,GL_R32F,1,mask),out(w,h,GL_RGBA32F,4,{});
  // Exercise caller state restoration and an incomplete fragment color mask.
  glEnable(GL_BLEND);glEnable(GL_SCISSOR_TEST);glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glEnable(GL_STENCIL_TEST);glEnable(GL_FRAMEBUFFER_SRGB);glEnable(GL_RASTERIZER_DISCARD);glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);glColorMask(GL_FALSE,GL_TRUE,GL_FALSE,GL_FALSE);glViewport(2,3,11,13);glActiveTexture(GL_TEXTURE4);glBindTexture(GL_TEXTURE_2D,src.id);
  blur.run(src.id,msk.id,out.id,strength);
  GLint viewport[4]{},active=0,texture=0,polygon[2]{};GLboolean color_mask[4]{};
  glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);glGetIntegerv(GL_POLYGON_MODE,polygon);glGetBooleanv(GL_COLOR_WRITEMASK,color_mask);
  check(viewport[0]==2&&viewport[1]==3&&viewport[2]==11&&viewport[3]==13&&active==GL_TEXTURE4&&GLuint(texture)==src.id&&polygon[0]==GL_LINE&&!color_mask[0]&&color_mask[1]&&!color_mask[2]&&!color_mask[3],"caller bindings/viewport/raster/color-mask restored");
  for(auto cap:{GL_BLEND,GL_SCISSOR_TEST,GL_DEPTH_TEST,GL_CULL_FACE,GL_STENCIL_TEST,GL_FRAMEBUFFER_SRGB,GL_RASTERIZER_DISCARD}){check(glIsEnabled(cap),"caller enable switch restored");glDisable(cap);}
  glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  auto result=out.read();verify(result,oracle(w,h,source,mask,strength));
  check(src.read()==source&&msk.read()==mask,"immutable input/mask bytes unchanged");
  if(test==0){check(result[3]>1e-7f&&result[3]<1.f/255,"subUNORM8 independent FLOAT coverage survives");check(result[0]/result[3]>9999,"HDR physical radiance survives association");}
  if(test==2)check(std::all_of(result.begin(),result.end(),[](float x){return x==0;}),"uncovered arbitrary RGB cannot invent radiance");
  if(test==4)check(result[(1+(h-2)*w)*4+3]>0&&result[(2+(h-2)*w)*4+3]>0&&result[(2+(h-3)*w)*4+3]==0,"cross kernel impulse includes neighbors and excludes diagonal");
  check(glGetError()==GL_NO_ERROR,"no GL error");all_results.insert(all_results.end(),result.begin(),result.end());
  if(test==1){
   auto before=out.read();
   check(refuses([&]{blur.run(src.id,msk.id,src.id,strength);}),"alias feedback refused");
   check(refuses([&]{blur.run(src.id,msk.id,out.id,strength,4);}),"bounded memory refusal");
   check(refuses([&]{blur.run(src.id,msk.id,out.id,std::numeric_limits<float>::quiet_NaN());}),"NaN strength refused");
   Texture quantized(w,h,GL_RGBA8,4,source);
   check(refuses([&]{blur.run(quantized.id,msk.id,out.id,strength);}),"silent UNORM8 quantization refused");
   Texture wrong_size(w-1,h,GL_R32F,1,{});
   check(refuses([&]{blur.run(src.id,wrong_size.id,out.id,strength);}),"mismatching domains refused");
   GLuint wrong_target=0;glGenTextures(1,&wrong_target);glBindTexture(GL_TEXTURE_CUBE_MAP,wrong_target);
   glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,src.id);
   check(refuses([&]{blur.run(wrong_target,msk.id,out.id,strength);})&&glGetError()==GL_NO_ERROR,"wrong-target source refused cleanly");
   check(out.read()==before&&src.read()==source&&msk.read()==mask,"wrong-target source preserves real owners");
   check(refuses([&]{blur.run(src.id,wrong_target,out.id,strength);})&&glGetError()==GL_NO_ERROR,"wrong-target mask refused cleanly");
   check(out.read()==before&&src.read()==source&&msk.read()==mask,"wrong-target mask preserves real owners");
   check(refuses([&]{blur.run(src.id,msk.id,wrong_target,strength);})&&glGetError()==GL_NO_ERROR,"wrong-target output refused cleanly");
   check(out.read()==before&&src.read()==source&&msk.read()==mask,"wrong-target output preserves preceding owners");
   glDeleteTextures(1,&wrong_target);
   check(out.read()==before,"all preflight refusals preserve output exactly");
  }
  std::cout<<"PASS case="<<test<<'\n';
 }
 const std::string prefix=argv[2];std::ofstream raw(prefix+".f32",std::ios::binary);raw.write(reinterpret_cast<const char*>(all_results.data()),all_results.size()*sizeof(float));raw.close();
 std::ofstream report(prefix+".json");report<<"{\"vendor\":\""<<vendor<<"\",\"renderer\":\""<<renderer<<"\",\"egl_drm_render_node\":\""<<drm<<"\",\"pci\":\""<<pci<<"\",\"pci_vendor\":\""<<vendor_id<<"\",\"pci_device\":\""<<device_id<<"\",\"cases\":7,\"assertions\":"<<assertions<<",\"width\":"<<w<<",\"height\":"<<h<<",\"maximum_absolute_error\":"<<maximum_absolute<<",\"maximum_relative_error\":"<<maximum_relative<<",\"texture_bytes\":"<<w*h*36<<",\"validation_readback_only\":true}\n";
 std::cout<<"PASS assertions="<<assertions<<" maxAbsolute="<<maximum_absolute<<" maxRelative="<<maximum_relative<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
