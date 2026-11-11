// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the production EGL owner against a deterministic vendor API. Resource
// accounting is independent of NativeGraphics and includes constructor failures.
#include "graphics.h"
#include <EGL/eglext.h>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {
enum class Fail { None, Platform, Display, Initialize, Bind, Config, NoConfig,
                  Context, Surface, Current };
Fail fail=Fail::None;
bool fallback=false, initialized=false, context_live=false, surface_live=false;
bool invalid_cleanup=false;
EGLenum selected_api=EGL_OPENGL_ES_API;
int display_token, candidate_token, context_token, surface_token, foreign_token;
EGLDisplay owned_display=EGL_NO_DISPLAY, current_display=EGL_NO_DISPLAY;
EGLContext current_context=EGL_NO_CONTEXT;
std::vector<char> cleanup;
unsigned checks=0;
void check(bool value,const char *label) {
  if(!value)throw std::runtime_error(label);
  ++checks;std::cout<<"PASS "<<label<<'\n';
}
EGLDisplay display_handle(){return reinterpret_cast<EGLDisplay>(&display_token);}
EGLDisplay candidate_handle(){return reinterpret_cast<EGLDisplay>(&candidate_token);}
EGLContext context_handle(){return reinterpret_cast<EGLContext>(&context_token);}
EGLSurface surface_handle(){return reinterpret_cast<EGLSurface>(&surface_token);}
void reset(Fail failure,bool use_fallback=false) {
  selected_api=EGL_OPENGL_ES_API;
  fail=failure;fallback=use_fallback;initialized=context_live=surface_live=false;
  invalid_cleanup=false;owned_display=current_display=EGL_NO_DISPLAY;
  current_context=EGL_NO_CONTEXT;cleanup.clear();
}
EGLDisplay EGLAPIENTRY platform_display(EGLenum platform,void*,const EGLint*) {
  if(platform==EGL_PLATFORM_DEVICE_EXT)return candidate_handle();
  return fail==Fail::Display?EGL_NO_DISPLAY:display_handle();
}
EGLBoolean EGLAPIENTRY query_devices(EGLint maximum,EGLDeviceEXT *devices,EGLint *count) {
  if(!fallback){*count=0;return EGL_FALSE;}
  if(maximum<1)return EGL_FALSE;
  devices[0]=reinterpret_cast<EGLDeviceEXT>(&candidate_token);*count=1;
  return EGL_TRUE;
}
}

extern "C" {
__eglMustCastToProperFunctionPointerType EGLAPIENTRY eglGetProcAddress(const char *name) {
  if(std::strcmp(name,"eglGetPlatformDisplayEXT")==0)
    return fail==Fail::Platform?nullptr:reinterpret_cast<__eglMustCastToProperFunctionPointerType>(platform_display);
  if(std::strcmp(name,"eglQueryDevicesEXT")==0)
    return reinterpret_cast<__eglMustCastToProperFunctionPointerType>(query_devices);
  return nullptr;
}
EGLBoolean EGLAPIENTRY eglInitialize(EGLDisplay d,EGLint *major,EGLint *minor) {
  if(fail==Fail::Initialize||(fallback&&d==display_handle()))return EGL_FALSE;
  if(d==EGL_NO_DISPLAY||initialized){invalid_cleanup=true;return EGL_FALSE;}
  initialized=true;owned_display=d;*major=1;*minor=5;return EGL_TRUE;
}
const char*EGLAPIENTRY eglQueryString(EGLDisplay d,EGLint name){return d==owned_display&&initialized&&name==EGL_VERSION?"1.5":nullptr;}
EGLint EGLAPIENTRY eglGetError(){return EGL_SUCCESS;}
EGLenum EGLAPIENTRY eglQueryAPI(){return selected_api;}
EGLBoolean EGLAPIENTRY eglBindAPI(EGLenum api) {
  if(api!=EGL_OPENGL_API&&api!=EGL_OPENGL_ES_API)invalid_cleanup=true;
  if(fail==Fail::Bind)return EGL_FALSE;
  selected_api=api;return EGL_TRUE;
}
EGLBoolean EGLAPIENTRY eglChooseConfig(EGLDisplay d,const EGLint*,EGLConfig *config,EGLint count,EGLint *found) {
  if(d!=owned_display||count!=1)invalid_cleanup=true;
  *config=reinterpret_cast<EGLConfig>(&display_token);
  *found=fail==Fail::NoConfig?0:1;
  return fail==Fail::Config?EGL_FALSE:EGL_TRUE;
}
EGLContext EGLAPIENTRY eglCreateContext(EGLDisplay d,EGLConfig,EGLContext,const EGLint*) {
  if(d!=owned_display||!initialized)invalid_cleanup=true;
  if(fail==Fail::Context)return EGL_NO_CONTEXT;
  context_live=true;return context_handle();
}
EGLSurface EGLAPIENTRY eglCreatePbufferSurface(EGLDisplay d,EGLConfig,const EGLint*) {
  if(d!=owned_display||!initialized)invalid_cleanup=true;
  if(fail==Fail::Surface)return EGL_NO_SURFACE;
  surface_live=true;return surface_handle();
}
EGLBoolean EGLAPIENTRY eglMakeCurrent(EGLDisplay d,EGLSurface draw,EGLSurface read,EGLContext c) {
  if(c==EGL_NO_CONTEXT) {
    if(d!=owned_display||draw!=EGL_NO_SURFACE||read!=EGL_NO_SURFACE)invalid_cleanup=true;
    cleanup.push_back('U');current_context=EGL_NO_CONTEXT;current_display=EGL_NO_DISPLAY;
    return EGL_TRUE;
  }
  if(d!=owned_display||!context_live||!surface_live||draw!=surface_handle()||read!=draw)invalid_cleanup=true;
  if(fail==Fail::Current)return EGL_FALSE;
  current_display=d;current_context=c;return EGL_TRUE;
}
EGLDisplay EGLAPIENTRY eglGetCurrentDisplay(){return current_display;}
EGLContext EGLAPIENTRY eglGetCurrentContext(){return current_context;}
EGLBoolean EGLAPIENTRY eglDestroySurface(EGLDisplay d,EGLSurface s) {
  if(d!=owned_display||s!=surface_handle()||!surface_live)invalid_cleanup=true;
  cleanup.push_back('S');surface_live=false;return EGL_TRUE;
}
EGLBoolean EGLAPIENTRY eglDestroyContext(EGLDisplay d,EGLContext c) {
  if(d!=owned_display||c!=context_handle()||!context_live||current_context==c)invalid_cleanup=true;
  cleanup.push_back('C');context_live=false;return EGL_TRUE;
}
EGLBoolean EGLAPIENTRY eglTerminate(EGLDisplay d) {
  if(d!=owned_display||!initialized||context_live||surface_live)invalid_cleanup=true;
  cleanup.push_back('T');initialized=false;return EGL_TRUE;
}
}

int main() try {
  for(auto failure:{Fail::Platform,Fail::Display,Fail::Initialize,Fail::Bind,
                    Fail::Config,Fail::NoConfig,Fail::Context,Fail::Surface,Fail::Current}) {
    reset(failure);
    bool refused=false;
    try {NativeGraphics graphics;}catch(const std::runtime_error &){refused=true;}
    check(refused,"A failing vendor stage refuses construction");
    check(!initialized&&!context_live&&!surface_live&&!invalid_cleanup,
          "Failed construction releases only successfully acquired vendor resources");
    check(selected_api==EGL_OPENGL_ES_API,"Failed construction restores the caller's selected rendering API");
  }
  for(bool use_fallback:{false,true}) {
    reset(Fail::None,use_fallback);
    {
      NativeGraphics graphics;
      check(graphics.display==(use_fallback?candidate_handle():display_handle())&&
            graphics.context==context_handle()&&graphics.surface==surface_handle()&&
            initialized&&context_live&&surface_live&&current_context==graphics.context,
            "Primary and device-fallback success retain the actual acquired resources");
    }
    check(!initialized&&!context_live&&!surface_live&&!invalid_cleanup&&
          cleanup==std::vector<char>({'U','S','C','T'}),
          "Successful teardown unbinds before releasing surface, context and display exactly once");
  }
  for(auto failure:{Fail::Bind,Fail::Config,Fail::NoConfig,Fail::Context,Fail::Surface,Fail::Current}) {
    reset(failure,true);
    bool refused=false;
    try {NativeGraphics graphics;}catch(const std::runtime_error &){refused=true;}
    const auto expected=failure==Fail::Surface?std::vector<char>({'C','T'}):
                        failure==Fail::Current?std::vector<char>({'S','C','T'}):
                        std::vector<char>({'T'});
    check(refused&&!initialized&&!context_live&&!surface_live&&!invalid_cleanup&&cleanup==expected,
          "Device fallback unwinds each partial acquisition in resource-dependency order");
  }
  reset(Fail::Current);
  current_display=reinterpret_cast<EGLDisplay>(&foreign_token);
  current_context=reinterpret_cast<EGLContext>(&foreign_token);
  auto previous_display=current_display;auto previous_context=current_context;
  try {NativeGraphics graphics;}catch(const std::runtime_error &){}
  check(current_display==previous_display&&current_context==previous_context&&
        !initialized&&!context_live&&!surface_live&&!invalid_cleanup,
        "Failed make-current preserves an unrelated current context while releasing this acquisition");
  reset(Fail::None);
  {
    NativeGraphics graphics;
    current_display=reinterpret_cast<EGLDisplay>(&foreign_token);
    current_context=reinterpret_cast<EGLContext>(&foreign_token);
    selected_api=EGL_OPENGL_ES_API;
    previous_display=current_display;previous_context=current_context;
  }
  check(current_display==previous_display&&current_context==previous_context&&
        cleanup==std::vector<char>({'S','C','T'})&&!invalid_cleanup&&selected_api==EGL_OPENGL_ES_API,
        "Teardown never unbinds an unrelated context made current by another owner");
  check(!std::is_copy_constructible_v<NativeGraphics>&&!std::is_copy_assignable_v<NativeGraphics>,
        "An EGL resource owner cannot be copied into a second destructor");
  std::cout<<checks<<" production EGL ownership checks passed\n";return 0;
}catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
