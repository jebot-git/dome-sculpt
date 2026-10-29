// SPDX-License-Identifier: GPL-3.0-or-later
#include "bdf_font.hpp"
#include "dome/palette.hpp"
#include "graphics.h"
#include <fstream>
#include <iostream>
#include <unistd.h>
using namespace dome;
int main(int argc, char **argv) try {
  if (argc != 2)
    return 2;
  auto home = std::filesystem::temp_directory_path() /
              ("dome-sculpt-gpu-" + std::to_string(getpid()));
  std::filesystem::create_directory(home);
  std::filesystem::permissions(home, std::filesystem::perms::owner_all);
  setenv("XDG_RUNTIME_DIR", home.c_str(), 1);
  auto config = bdf_fonts::export_config();
  setenv("FONTCONFIG_FILE", config.c_str(), 1);
  auto face = bdf_fonts::resolve("Unifont", "Sans-Serif", 16);
  Document doc;
  doc.seed();
  if (!doc.wait_mesh())
    throw std::runtime_error("No mesh snapshot");
  NativeGraphics graphics;
  SurfaceRenderer renderer;
  SpatialPalette palette;
  GLuint fbo, color, depth;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glGenTextures(1, &color);
  glBindTexture(GL_TEXTURE_2D, color);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 640, 480, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         color, 0);
  glGenRenderbuffers(1, &depth);
  glBindRenderbuffer(GL_RENDERBUFFER, depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 640, 480);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, depth);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    throw std::runtime_error("Test framebuffer incomplete");
  auto vp = projection(-.65, .65, -.5, .5);
  CanvasTransform model{{0, 0, -1.8f}, {}, 1};
  auto capture = [&](std::string name) {
    std::vector<uint8_t> bytes(640 * 480 * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, 640, 480, GL_RGB, GL_UNSIGNED_BYTE, bytes.data());
    std::ofstream out(std::string(argv[1]) + "-" + name + ".ppm",
                      std::ios::binary);
    out << "P6\n640 480\n255\n";
    for (int y = 479; y >= 0; --y)
      out.write(reinterpret_cast<const char *>(bytes.data() + y * 640 * 3),
                640 * 3);
    if (!out)
      throw std::runtime_error("Cannot save GPU capture");
    return bytes;
  };
  renderer.draw(doc.frame().surface, model, vp, {}, 640, 480);
  auto pbr = capture("pbr");
  size_t foreground = 0;
  for (size_t i = 0; i < pbr.size(); i += 3)
    if (pbr[i] > 50 && pbr[i] > pbr[i + 2])
      ++foreground;
  if (foreground < 10000)
    throw std::runtime_error("PBR surface render is blank");
  renderer.draw(doc.frame().surface, model, vp, {}, 640, 480, false, false,
                true);
  auto light = capture("light");
  if (pbr[0] >= 80 || light[0] <= 160 || light[0] != light[1] ||
      light[1] != light[2])
    throw std::runtime_error("Appearance does not select grey backgrounds");
  renderer.draw(doc.frame().surface, model, vp, {}, 640, 480, true);
  std::array<uint8_t, 4> corner{};
  glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, corner.data());
  if (corner != std::array<uint8_t, 4>{})
    throw std::runtime_error("XR overlay background must remain transparent");
  renderer.draw(doc.frame().surface, model, vp, {}, 640, 480, false, true);
  auto toon = capture("toon");
  if (toon == pbr)
    throw std::runtime_error("Toon preview did not change shading");
  auto state = *doc.frame().state;
  state.menu = true;
  state.menu_index = 1;
  state.font_file = face.file;
  state.font_pixels = face.pixels;
  state.panel.position = {0, 0, -.45f};
  palette.draw(state, vp);
  auto menu = capture("menu");
  size_t blue = 0;
  for (size_t i = 0; i < menu.size(); i += 3)
    if (menu[i] < 150 && menu[i + 1] > 130 && menu[i + 2] > 220)
      ++blue;
  if (blue < 1000)
    throw std::runtime_error("Spatial menu selection is not visible");
  glDeleteRenderbuffers(1, &depth);
  glDeleteTextures(1, &color);
  glDeleteFramebuffers(1, &fbo);
  std::filesystem::remove_all(home);
  std::cout << "PASS Native PBR and toon GPU previews, grey appearance "
               "backgrounds, transparent XR overlay, BDF spatial menu and "
               "visible joystick selection\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
