// SPDX-License-Identifier: MIT
// From de-xr Ghostty XR/FLTerm XR; see third_party/LICENSE.native-graphics.
#pragma once
#include <EGL/egl.h>

struct NativeGraphics {
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLConfig config{};
  EGLContext context = EGL_NO_CONTEXT;
  EGLSurface surface = EGL_NO_SURFACE;
  NativeGraphics();
  ~NativeGraphics();
};
