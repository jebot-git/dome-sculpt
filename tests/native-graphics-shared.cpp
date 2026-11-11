// SPDX-License-Identifier: GPL-3.0-or-later
#define GL_GLEXT_PROTOTYPES
#include <GL/glcorearb.h>
#include "graphics.h"
#include "paint-blur-gpu.hpp"
#include <EGL/eglext.h>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <atomic>
namespace {
unsigned checks=0;
std::atomic_bool fail_context=false;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);++checks;}
// Deliberately external: no NativeGraphics lease/initializer is used here.
struct External {
 EGLDisplay display=EGL_NO_DISPLAY;EGLContext context=EGL_NO_CONTEXT;EGLSurface surface=EGL_NO_SURFACE;EGLenum api;
 explicit External(EGLenum selected):api(selected){try{
  auto get=reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
  if(!get)throw std::runtime_error("Missing surfaceless EGL display entry point");
  display=get(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,nullptr);EGLint major=0,minor=0;
  if(display==EGL_NO_DISPLAY||!eglInitialize(display,&major,&minor)||!eglBindAPI(api))throw std::runtime_error("Cannot initialize external EGL owner");
  const EGLint config_attributes[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,api==EGL_OPENGL_API?EGL_OPENGL_BIT:EGL_OPENGL_ES2_BIT,EGL_NONE};EGLConfig config{};EGLint count=0;
  if(!eglChooseConfig(display,config_attributes,&config,1,&count)||count!=1)throw std::runtime_error("Cannot choose external API config");
  const EGLint gl_attributes[]={EGL_CONTEXT_MAJOR_VERSION,3,EGL_CONTEXT_MINOR_VERSION,3,EGL_NONE};const EGLint es_attributes[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
  context=eglCreateContext(display,config,EGL_NO_CONTEXT,api==EGL_OPENGL_API?gl_attributes:es_attributes);
  const EGLint size[]={EGL_WIDTH,1,EGL_HEIGHT,1,EGL_NONE};surface=eglCreatePbufferSurface(display,config,size);
  if(context==EGL_NO_CONTEXT||surface==EGL_NO_SURFACE||!eglMakeCurrent(display,surface,surface,context))throw std::runtime_error("Cannot bind external API owner");
 }catch(...){release();throw;}}
 void release()noexcept{if(display==EGL_NO_DISPLAY)return;const auto before=eglQueryAPI();eglBindAPI(api);if(eglGetCurrentContext()==context)eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);if(surface!=EGL_NO_SURFACE)eglDestroySurface(display,surface);if(context!=EGL_NO_CONTEXT)eglDestroyContext(display,context);eglTerminate(display);eglBindAPI(before);display=EGL_NO_DISPLAY;}
 ~External(){release();}
};
}
extern "C" EGLContext __real_eglCreateContext(EGLDisplay,EGLConfig,EGLContext,const EGLint*);
extern "C" EGLContext __wrap_eglCreateContext(EGLDisplay d,EGLConfig c,EGLContext shared,const EGLint*attributes){if(fail_context.exchange(false))return EGL_NO_CONTEXT;return __real_eglCreateContext(d,c,shared,attributes);}
int main(int argc,char**argv)try{
 const bool software=argc>1&&std::string(argv[1])=="software";
 {
  NativeGraphics renderer;GLuint retained=0;glGenTextures(1,&retained);glBindTexture(GL_TEXTURE_2D,retained);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,nullptr);
  std::exception_ptr failure;
  std::thread worker([&]{try{dome::detail::ColorBlurGpu gpu(software);auto in=std::make_unique<dome::detail::ColorBlurInput>();auto out=std::make_unique<dome::detail::ColorBlurOutput>();for(auto&m:in->metadata)m={.4f,.8f,1,1};gpu.run(*in,*out,.5f,{});}catch(...){failure=std::current_exception();}});worker.join();if(failure)std::rethrow_exception(failure);
  EGLint version=0;
  check(eglQueryContext(renderer.display,renderer.context,EGL_CONTEXT_CLIENT_VERSION,&version)==EGL_TRUE,"Worker teardown preserves concurrently live renderer's initialized EGLDisplay/context");
  check(eglMakeCurrent(renderer.display,renderer.surface,renderer.surface,renderer.context)==EGL_TRUE&&glIsTexture(retained),"Renderer retains its actual GL texture and can bind after worker context destruction");glDeleteTextures(1,&retained);
  std::atomic_bool refused=false,api_restored=false;fail_context=true;
  std::thread failing([&]{const auto api=eglQueryAPI();try{NativeGraphics partial;}catch(const std::runtime_error&){refused=true;}api_restored=eglQueryAPI()==api;});failing.join();
  check(refused&&api_restored,"Failed shared-display construction restores the worker's previous selected API");
  check(eglQueryContext(renderer.display,renderer.context,EGL_CONTEXT_CLIENT_VERSION,&version)==EGL_TRUE&&eglGetCurrentContext()==renderer.context,"A partial worker failure retains the live renderer's display/context/current ownership");
 }
 for(auto api:{EGL_OPENGL_API,EGL_OPENGL_ES_API}){
  External external(api);{
   NativeGraphics borrowed;check(borrowed.display==external.display,"Helper genuinely borrows the externally initialized same EGLDisplay");
   check(eglBindAPI(api)&&eglMakeCurrent(external.display,external.surface,external.surface,external.context),"External caller selects its own API/current context before helper teardown");
  }
  EGLint version=0;
  check(eglQueryAPI()==api&&eglGetCurrentContext()==external.context&&eglGetCurrentDisplay()==external.display,"Helper preserves a different caller's current API/context on the borrowed display");
  check(eglQueryString(external.display,EGL_VERSION)&&eglQueryContext(external.display,external.context,EGL_CONTEXT_CLIENT_VERSION,&version),"Borrowed display remains initialized until its external owner terminates it");
 }
 std::cout<<"PASS "<<checks<<" real shared/borrowed EGL display ownership controls\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
