// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>

namespace dome::detail {
inline void require_hardware_opengl() {
  const auto *name = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
  if (!name || !*name)
    throw std::runtime_error("Dome Sculpt requires a current hardware OpenGL context");

  std::string renderer(name);
  std::ranges::transform(renderer,renderer.begin(),[](unsigned char c) { return std::tolower(c); });
  constexpr std::string_view software_renderers[]{
      "llvmpipe", "softpipe", "swrast", "software rasterizer", "software renderer",
      "swiftshader", "gdi generic", "microsoft basic render driver"};
  bool software = std::ranges::any_of(software_renderers,[&](auto driver) {
    return renderer.find(driver)!=std::string::npos;
  });

  if (const auto display=eglGetCurrentDisplay(); display!=EGL_NO_DISPLAY) {
    const auto attribute=reinterpret_cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(eglGetProcAddress("eglQueryDisplayAttribEXT"));
    const auto query=reinterpret_cast<PFNEGLQUERYDEVICESTRINGEXTPROC>(eglGetProcAddress("eglQueryDeviceStringEXT"));
    EGLAttrib device=0;
    if (attribute&&query&&attribute(display,EGL_DEVICE_EXT,&device)&&device) {
      const auto *extensions=query(reinterpret_cast<EGLDeviceEXT>(device),EGL_EXTENSIONS);
      software |= extensions && std::string_view(extensions).find("EGL_MESA_device_software")!=std::string_view::npos;
    }
  }
  if (software)
    throw std::runtime_error(std::string("Dome Sculpt requires hardware-accelerated OpenGL. Current renderer is ")+name);
}
}
