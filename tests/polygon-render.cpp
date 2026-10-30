// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/polygon-render.hpp"
#include "dome/paint.hpp"
#include "graphics.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
using namespace dome;
namespace {
size_t checks = 0;
void check(bool v, const char *m) {
  if (!v)
    throw std::runtime_error(m);
  ++checks;
  std::cout << "PASS " << m << '\n';
}
struct Traffic {
  size_t bytes = 0, data = 0, subdata = 0, read = 0, map = 0;
};
Traffic traffic;
bool capture = false, poison = false, lifetime = false;
std::set<GLuint> buffers, vaos;
struct Fbo {
  static constexpr int size = 96;
  GLuint f = 0, c = 0, d = 0;
  Fbo() {
    glGenFramebuffers(1, &f);
    glBindFramebuffer(GL_FRAMEBUFFER, f);
    glGenTextures(1, &c);
    glBindTexture(GL_TEXTURE_2D, c);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           c, 0);
    glGenRenderbuffers(1, &d);
    glBindRenderbuffer(GL_RENDERBUFFER, d);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, d);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      throw std::runtime_error("Incomplete polygon fixture framebuffer");
  }
  ~Fbo() {
    glDeleteRenderbuffers(1, &d);
    glDeleteTextures(1, &c);
    glDeleteFramebuffers(1, &f);
  }
};
using Pixels = std::vector<uint8_t>;
using Matrix = std::array<float, 16>;
const Matrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
    camera{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1};
Pixels pixels() {
  Pixels p(Fbo::size * Fbo::size * 4);
  glReadPixels(0, 0, Fbo::size, Fbo::size, GL_RGBA, GL_UNSIGNED_BYTE, p.data());
  return p;
}
void capture_image(const char *name, const Pixels &pixels) {
  if (const char *directory = std::getenv("DOME_POLYGON_CAPTURE")) {
    std::filesystem::create_directories(directory);
    Image image{Fbo::size, Fbo::size, pixels};
    for (size_t y = 0; y < Fbo::size / 2; ++y)
      for (size_t x = 0; x < Fbo::size * 4; ++x)
        std::swap(image.rgba[y * Fbo::size * 4 + x],
                  image.rgba[(Fbo::size - 1 - y) * Fbo::size * 4 + x]);
    auto bytes = image.png();
    std::ofstream out(std::filesystem::path(directory) /
                          (std::string(name) + ".png"),
                      std::ios::binary);
    out.write(reinterpret_cast<const char *>(bytes.data()),
              std::streamsize(bytes.size()));
    if (!out)
      throw std::runtime_error("Unable to capture polygon fixture image");
  }
}
size_t alpha(const Pixels &p, int x, int y) {
  return p[(y * Fbo::size + x) * 4 + 3];
}
} // namespace
extern "C" void __real_glBufferData(GLenum, GLsizeiptr, const void *, GLenum);
extern "C" void __wrap_glBufferData(GLenum target, GLsizeiptr size,
                                    const void *p, GLenum usage) {
  if (capture) {
    ++traffic.data;
    traffic.bytes += size;
  }
  if (poison) {
    poison = false;
    __real_glBufferData(GL_TEXTURE_2D, size, p, usage);
  } else
    __real_glBufferData(target, size, p, usage);
}
extern "C" void __real_glBufferSubData(GLenum, GLintptr, GLsizeiptr,
                                       const void *);
extern "C" void __wrap_glBufferSubData(GLenum t, GLintptr o, GLsizeiptr n,
                                       const void *p) {
  if (capture)
    ++traffic.subdata;
  __real_glBufferSubData(t, o, n, p);
}
extern "C" void __real_glGetBufferSubData(GLenum, GLintptr, GLsizeiptr, void *);
extern "C" void __wrap_glGetBufferSubData(GLenum t, GLintptr o, GLsizeiptr n,
                                          void *p) {
  if (capture)
    ++traffic.read;
  __real_glGetBufferSubData(t, o, n, p);
}
extern "C" void *__real_glMapBuffer(GLenum, GLenum);
extern "C" void *__wrap_glMapBuffer(GLenum t, GLenum a) {
  if (capture)
    ++traffic.map;
  return __real_glMapBuffer(t, a);
}
extern "C" void *__real_glMapBufferRange(GLenum, GLintptr, GLsizeiptr,
                                         GLbitfield);
extern "C" void *__wrap_glMapBufferRange(GLenum t, GLintptr o, GLsizeiptr n,
                                         GLbitfield a) {
  if (capture)
    ++traffic.map;
  return __real_glMapBufferRange(t, o, n, a);
}
extern "C" void __real_glReadPixels(GLint, GLint, GLsizei, GLsizei, GLenum,
                                    GLenum, void *);
extern "C" void __wrap_glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h,
                                    GLenum f, GLenum t, void *p) {
  if (capture)
    ++traffic.read;
  __real_glReadPixels(x, y, w, h, f, t, p);
}
extern "C" void __real_glGenBuffers(GLsizei, GLuint *);
extern "C" void __wrap_glGenBuffers(GLsizei n, GLuint *p) {
  __real_glGenBuffers(n, p);
  if (lifetime)
    for (int i = 0; i < n; ++i)
      buffers.insert(p[i]);
}
extern "C" void __real_glDeleteBuffers(GLsizei, const GLuint *);
extern "C" void __wrap_glDeleteBuffers(GLsizei n, const GLuint *p) {
  if (lifetime)
    for (int i = 0; i < n; ++i)
      buffers.erase(p[i]);
  __real_glDeleteBuffers(n, p);
}
extern "C" void __real_glGenVertexArrays(GLsizei, GLuint *);
extern "C" void __wrap_glGenVertexArrays(GLsizei n, GLuint *p) {
  __real_glGenVertexArrays(n, p);
  if (lifetime)
    for (int i = 0; i < n; ++i)
      vaos.insert(p[i]);
}
extern "C" void __real_glDeleteVertexArrays(GLsizei, const GLuint *);
extern "C" void __wrap_glDeleteVertexArrays(GLsizei n, const GLuint *p) {
  if (lifetime)
    for (int i = 0; i < n; ++i)
      vaos.erase(p[i]);
  __real_glDeleteVertexArrays(n, p);
}
int main(int argc, char **argv) try {
  NativeGraphics graphics;
  std::string device = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
  std::cout << "GPU " << device << '\n';
  check(argc < 2 || std::string(argv[1]) != "--hardware" ||
            (device.find("llvmpipe") == std::string::npos &&
             device.find("softpipe") == std::string::npos),
        "Polygon preview runs on the requested graphics device");
  Fbo fbo;
  glViewport(0, 0, Fbo::size, Fbo::size);
  lifetime = true;
  {
    PolygonRenderer renderer;
    PolygonWorkspace workspace(PolygonMesh::cube());
    workspace.select(workspace.view().topology->faces()[1].id);
    auto source = workspace.view();
    auto draw = [&](const PolygonView &v, bool linear = false) {
      glBindFramebuffer(GL_FRAMEBUFFER, fbo.f);
      glClearColor(0, 0, 0, 0);
      glDepthMask(GL_TRUE);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      traffic = {};
      capture = true;
      try {
        renderer.draw(v, camera, identity, 0xff0000, linear);
      } catch (...) {
        capture = false;
        throw;
      }
      capture = false;
      return pixels();
    };
    auto baseline = draw(source);
    capture_image("baseline", baseline);
    check(traffic.bytes > 0 && traffic.data > 0 && traffic.read == 0 &&
              traffic.map == 0,
          "First immutable polygon draws real pixels with uploads and no "
          "geometry readback");
    auto stats = renderer.stats();
    check(alpha(baseline, 48, 48) == 255 && alpha(baseline, 80, 48) == 0,
          "Independent coverage oracle matches the undeformed unit cube");
    check(draw(source) == baseline && traffic.bytes == 0 && traffic.data == 0 &&
              traffic.subdata == 0 &&
              renderer.stats().geometry_uploads == stats.geometry_uploads,
          "Second stereo view reuses geometry and selection stores without any "
          "transfers");
    workspace.begin_move();
    workspace.move_to({.25f, 0, 0});
    auto preview = draw(workspace.view());
    capture_image("move-preview", preview);
    check(traffic.bytes == 0 && traffic.read == 0 && traffic.map == 0 &&
              workspace.view().surface == source.surface,
          "Actual component motion changes only GPU uniforms while CPU "
          "geometry remains immutable");
    check(alpha(preview, 80, 48) == 255 && preview != baseline,
          "Selected face really moves into independently predicted screen "
          "coverage");
    check(draw(workspace.view()) == preview && traffic.bytes == 0,
          "Repeated stereo motion preview is pixel-exact with zero uploads");
    workspace.move_to({.3f, 0, 0});
    auto further = draw(workspace.view());
    check(further != preview && traffic.bytes == 0,
          "Additional continuous motion still performs no geometry or scalar "
          "transfer");
    workspace.end_move(false);
    check(
        draw(workspace.view()) == baseline && traffic.bytes == 0,
        "Cancel reuses the exact source buffers and restores original pixels");
    workspace.begin_move();
    workspace.move_to({.25f, 0, 0});
    draw(workspace.view());
    workspace.end_move(true);
    auto accepted = draw(workspace.view());
    check(accepted == preview &&
              renderer.stats().geometry_uploads == stats.geometry_uploads + 1,
          "Accepted End uploads one final CPU result matching its GPU-only "
          "preview");
    auto changed = workspace.view();
    check(workspace.undo() && draw(workspace.view()) == baseline,
          "Actual local Undo restores original rendered topology and pixels");
    auto retained = renderer.stats();
    poison = true;
    bool refused = false;
    try {
      draw(changed);
    } catch (const std::runtime_error &) {
      refused = true;
    }
    check(refused && !poison &&
              renderer.stats().geometry_uploads == retained.geometry_uploads,
          "Actual GL allocation failure does not publish partially replaced "
          "polygon stores");
    check(draw(workspace.view()) == baseline && traffic.bytes == 0,
          "Old polygon buffers remain drawable after a real GL error");
    check(draw(changed) == accepted, "The rejected prepared geometry retries "
                                     "to its exact successful pixels");
    auto bad = changed;
    auto invalid = std::make_shared<PolygonDrawData>(*bad.display);
    invalid->move_weights.pop_back();
    bad.display = invalid;
    retained = renderer.stats();
    refused = false;
    try {
      draw(bad);
    } catch (const std::invalid_argument &) {
      refused = true;
    }
    check(refused &&
              renderer.stats().geometry_uploads == retained.geometry_uploads,
          "Mismatched scalar domains reject before GPU authority changes");
    check(draw(changed) == accepted && traffic.bytes == 0,
          "Invalid scalar input leaves valid cached geometry and selection "
          "reusable");
    auto moved_data = std::make_shared<PolygonDrawData>(*changed.display);
    moved_data->move_weights[0] = std::numeric_limits<float>::quiet_NaN();
    bad = changed;
    bad.display = moved_data;
    refused = false;
    try {
      draw(bad);
    } catch (const std::invalid_argument &) {
      refused = true;
    }
    check(refused && draw(changed) == accepted,
          "Nonfinite selection weights cannot corrupt rendered polygon state");
    workspace.mode(PolygonSelectionMode::Vertex);
    workspace.select(7);
    auto points = draw(workspace.view());
    capture_image("vertex-selection", points);
    size_t red = 0;
    for (int y = 68; y < 78; ++y)
      for (int x = 68; x < 78; ++x) {
        auto i = (y * Fbo::size + x) * 4;
        red += points[i] > 245 && points[i + 1] < 10 && points[i + 2] < 10;
      }
    if (red <= 8) {
      size_t total = 0;
      int xmin = 96, xmax = -1, ymin = 96, ymax = -1;
      for (int y = 0; y < 96; ++y)
        for (int x = 0; x < 96; ++x) {
          auto i = (y * 96 + x) * 4;
          if (points[i] > 245 && points[i + 1] < 10 && points[i + 2] < 10) {
            ++total;
            xmin = std::min(xmin, x);
            xmax = std::max(xmax, x);
            ymin = std::min(ymin, y);
            ymax = std::max(ymax, y);
          }
        }
      std::cerr << "Point red pixels local=" << red << " total=" << total
                << " bounds=" << xmin << "," << ymin << ".." << xmax << ","
                << ymax << "\n";
      std::ofstream image("polygon-point-diagnostic.ppm", std::ios::binary);
      image << "P6\n96 96\n255\n";
      for (size_t i = 0; i < points.size(); i += 4)
        image.write(reinterpret_cast<const char *>(points.data() + i), 3);
    }
    check(
        red > 8,
        "Selected vertex feedback produces actual accent-colored point pixels");
    check(renderer.stats().geometry_uploads > 0 && traffic.read == 0 &&
              traffic.map == 0,
          "Selection overlays use scalar/index uploads without reading "
          "geometry back");
    PolygonWorkspace tilted(PolygonMesh::cube());
    tilted.mode(PolygonSelectionMode::Vertex);
    tilted.select(7);
    auto untilted = draw(tilted.view());
    tilted.begin_move();
    tilted.move_to({0, 0, .15f});
    auto tilted_preview = draw(tilted.view());
    capture_image("tilted-preview", tilted_preview);
    auto center = (48 * Fbo::size + 48) * 4;
    check(tilted_preview[center] != untilted[center],
          "Partial vertex motion updates actual N-gon lighting before CPU "
          "reconstruction");
    tilted.end_move(true);
    check(draw(tilted.view()) == tilted_preview,
          "Analytic GPU face normals and partial-vertex positions match "
          "accepted CPU geometry exactly");
    auto srgb = draw(workspace.view(), false),
         linear = draw(workspace.view(), true);
    check(alpha(srgb, 48, 48) == alpha(linear, 48, 48) &&
              srgb[(48 * 96 + 48) * 4] > linear[(48 * 96 + 48) * 4],
          "Linear overlay target changes RGB transfer while retaining exact "
          "coverage");
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    renderer.draw(workspace.view(), camera, identity);
    GLboolean mask;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &mask);
    check(glIsEnabled(GL_BLEND) && !glIsEnabled(GL_DEPTH_TEST) && !mask,
          "Polygon pass restores surrounding renderer depth and blend state");
    check(glGetError() == GL_NO_ERROR,
          "Polygon geometry, selection, staging recovery and uniforms leave no "
          "GL errors");
  }
  check(buffers.empty() && vaos.empty(),
        "All actually created polygon buffers and vertex arrays are released");
  lifetime = false;
  std::cout << checks << " polygon GPU checks passed\n";
  return 0;
} catch (const std::exception &e) {
  capture = false;
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
