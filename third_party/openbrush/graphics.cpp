// SPDX-License-Identifier: MIT
// From de-xr Ghostty XR/FLTerm XR; see third_party/LICENSE.native-graphics.
#include "graphics.h"
#include <EGL/eglext.h>
#include <iostream>
#include <map>
#include <mutex>
#include <limits>
#include <cstdio>
#include <stdexcept>

namespace {
struct DisplayLease { size_t owners;bool helper_initialized;EGLint major,minor; };
std::mutex display_mutex;
std::map<EGLDisplay,DisplayLease> display_leases;
bool acquire_display(EGLDisplay display,EGLint&major,EGLint&minor){
  std::lock_guard lock(display_mutex);
  if(auto found=display_leases.find(display);found!=display_leases.end()){
    if(found->second.owners==std::numeric_limits<size_t>::max())throw std::overflow_error("Native EGL display ownership exhausted");
    ++found->second.owners;major=found->second.major;minor=found->second.minor;return true;
  }
  // EGL initialization is not reference-counted by eglInitialize calls. A
  // previously initialized external display is borrowed: its owner must keep
  // it initialized, and this helper must never terminate it.
  const char*version=eglQueryString(display,EGL_VERSION);
  const bool borrowed=version!=nullptr;
  if(borrowed){if(std::sscanf(version,"%d.%d",&major,&minor)!=2){major=1;minor=0;}}
  else{(void)eglGetError();if(!eglInitialize(display,&major,&minor))return false;}
  try{display_leases.emplace(display,DisplayLease{1,!borrowed,major,minor});}
  catch(...){if(!borrowed)eglTerminate(display);throw;}
  return true;
}
void release_display(EGLDisplay display)noexcept{
  std::lock_guard lock(display_mutex);
  const auto found=display_leases.find(display);
  if(found==display_leases.end())return; // initialization/lease allocation failed
  if(--found->second.owners)return;
  const bool terminate=found->second.helper_initialized;
  display_leases.erase(found);
  if(terminate)eglTerminate(display);
}
}

NativeGraphics::NativeGraphics() {
  const auto caller_api=eglQueryAPI();
  try {
    auto platformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
        eglGetProcAddress("eglGetPlatformDisplayEXT"));
    if (!platformDisplay)
      throw std::runtime_error(
          "EGL platform displays are required for native XR rendering");
    display = platformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY,
                              nullptr);
    EGLint major = 0, minor = 0;
    if (display == EGL_NO_DISPLAY || !acquire_display(display, major, minor)) {
      auto query = reinterpret_cast<PFNEGLQUERYDEVICESEXTPROC>(
          eglGetProcAddress("eglQueryDevicesEXT"));
      EGLDeviceEXT devices[16];
      EGLint count = 0;
      display = EGL_NO_DISPLAY;
      if (query && query(16, devices, &count))
        for (int i = 0; i < count; ++i) {
          auto candidate =
              platformDisplay(EGL_PLATFORM_DEVICE_EXT, devices[i], nullptr);
          if (candidate != EGL_NO_DISPLAY &&
              acquire_display(candidate, major, minor)) {
            display = candidate;
            break;
          }
        }
      if (display == EGL_NO_DISPLAY)
        throw std::runtime_error("Cannot initialize a windowless EGL display");
    }
    if (!eglBindAPI(EGL_OPENGL_API))
      throw std::runtime_error("EGL desktop OpenGL API is unavailable");
    const EGLint attributes[] = {EGL_SURFACE_TYPE,
                                 EGL_PBUFFER_BIT,
                                 EGL_RENDERABLE_TYPE,
                                 EGL_OPENGL_BIT,
                                 EGL_RED_SIZE,
                                 8,
                                 EGL_GREEN_SIZE,
                                 8,
                                 EGL_BLUE_SIZE,
                                 8,
                                 EGL_ALPHA_SIZE,
                                 8,
                                 EGL_NONE};
    EGLint count = 0;
    if (!eglChooseConfig(display, attributes, &config, 1, &count) || count == 0)
      throw std::runtime_error("No EGL OpenGL configuration is available");
    const EGLint contextAttributes[] = {EGL_CONTEXT_MAJOR_VERSION,
                                        3,
                                        EGL_CONTEXT_MINOR_VERSION,
                                        3,
                                        EGL_CONTEXT_OPENGL_PROFILE_MASK,
                                        EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                                        EGL_NONE};
    context =
        eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
    if (context == EGL_NO_CONTEXT)
      throw std::runtime_error("Cannot create the windowless EGL OpenGL context");
    const EGLint surfaceAttributes[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
    surface = eglCreatePbufferSurface(display, config, surfaceAttributes);
    if (surface == EGL_NO_SURFACE ||
        !eglMakeCurrent(display, surface, surface, context))
      throw std::runtime_error("Cannot create the windowless EGL OpenGL context");
    std::cerr << "Native XR graphics: windowless EGL " << major << '.' << minor
              << '\n';
  } catch (...) {
    // A throwing constructor has no destructor. Release even a partial EGL
    // acquisition before propagating the original setup failure.
    release();
    if(eglQueryAPI()!=caller_api)eglBindAPI(caller_api);
    throw;
  }
}
NativeGraphics::~NativeGraphics() { release(); }
void NativeGraphics::release() noexcept {
  if (display != EGL_NO_DISPLAY) {
    // Failed make-current leaves the caller's previous context current. It may
    // also have explicitly switched contexts since construction; do not unbind
    // that other owner while releasing this instance's resources.
    const auto caller_api=eglQueryAPI();
    bool rebound=false;
    if(context!=EGL_NO_CONTEXT&&caller_api!=EGL_OPENGL_API)rebound=eglBindAPI(EGL_OPENGL_API)==EGL_TRUE;
    if (context != EGL_NO_CONTEXT && (caller_api==EGL_OPENGL_API||rebound) &&
        eglGetCurrentDisplay() == display && eglGetCurrentContext() == context)
      eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (surface != EGL_NO_SURFACE)
      eglDestroySurface(display, surface);
    if (context != EGL_NO_CONTEXT)
      eglDestroyContext(display, context);
    if(rebound)eglBindAPI(caller_api);
    release_display(display);
    surface = EGL_NO_SURFACE;
    context = EGL_NO_CONTEXT;
    display = EGL_NO_DISPLAY;
    config = {};
  }
}
