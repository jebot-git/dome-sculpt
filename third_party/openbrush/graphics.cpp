// SPDX-License-Identifier: MIT
// From de-xr Ghostty XR/FLTerm XR; see third_party/LICENSE.native-graphics.
#include "graphics.h"
#include <EGL/eglext.h>
#include <iostream>
#include <stdexcept>

NativeGraphics::NativeGraphics() {
  try {
    auto platformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
        eglGetProcAddress("eglGetPlatformDisplayEXT"));
    if (!platformDisplay)
      throw std::runtime_error(
          "EGL platform displays are required for native XR rendering");
    display = platformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY,
                              nullptr);
    EGLint major = 0, minor = 0;
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, &major, &minor)) {
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
              eglInitialize(candidate, &major, &minor)) {
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
    throw;
  }
}
NativeGraphics::~NativeGraphics() { release(); }
void NativeGraphics::release() noexcept {
  if (display != EGL_NO_DISPLAY) {
    // Failed make-current leaves the caller's previous context current. It may
    // also have explicitly switched contexts since construction; do not unbind
    // that other owner while releasing this instance's resources.
    if (context != EGL_NO_CONTEXT && eglGetCurrentDisplay() == display &&
        eglGetCurrentContext() == context)
      eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (surface != EGL_NO_SURFACE)
      eglDestroySurface(display, surface);
    if (context != EGL_NO_CONTEXT)
      eglDestroyContext(display, context);
    eglTerminate(display);
    surface = EGL_NO_SURFACE;
    context = EGL_NO_CONTEXT;
    display = EGL_NO_DISPLAY;
    config = {};
  }
}
