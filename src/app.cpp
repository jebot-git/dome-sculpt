// SPDX-License-Identifier: GPL-3.0-or-later
#include "appearance.h"
#include "desktop-menu.h"
#include "dome/mesh.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Gl_Window.H>
#include <FL/fl_file_chooser.H>
#include <FL/gl.h>
#include <FL/glu.h>
#include <functional>
#include <iostream>
#include <sstream>
using namespace dome;
namespace {
struct Canvas : Fl_Gl_Window {
  Volume volume{.03f};
  Mesh mesh;
  Quat rotation{};
  Vec3 pan{};
  Vec3 ball{}, last_stamp{};
  float distance = 2.2f, radius = .12f;
  Tool tool = Tool::Clay;
  bool sculpting = false, have_stamp = false, dirty_mesh = true;
  int last_x = 0, last_y = 0;
  std::string capture;
  std::function<void()> changed;
  Canvas(int x, int y, int w, int h) : Fl_Gl_Window(x, y, w, h) {
    mode(FL_RGB | FL_DOUBLE | FL_DEPTH);
    end();
  }
  void seed() {
    volume = Volume(.03f);
    volume.begin_stroke();
    Brush b;
    b.radius = .4f;
    b.blend = 0;
    volume.stamp(b);
    volume.end_stroke();
    invalidate();
  }
  void invalidate() {
    dirty_mesh = true;
    redraw();
    if (changed)
      changed();
  }
  void update_mesh() {
    if (dirty_mesh) {
      mesh = surface_mesh(volume);
      dirty_mesh = false;
    }
  }
  Vec3 project_ball(int x, int y) {
    float size = float(std::max(1, std::min(w(), h())));
    return trackball((2 * (x - this->x()) - w()) / size,
                     (h() - 2 * (y - this->y())) / size);
  }
  void sculpt_at(int x, int y) {
    float aspect = float(w()) / std::max(1, h()),
          tangent = std::tan(22.5f * 3.14159265f / 180);
    Vec3 direction{(2.f * (x - this->x()) / w() - 1) * aspect * tangent,
                   (1 - 2.f * (y - this->y()) / h()) * tangent, -1};
    Vec3 origin = rotation.inverse().rotate(Vec3{0, 0, distance} - pan);
    direction = rotation.inverse().rotate(normalized(direction));
    auto hit = volume.raycast(origin, direction, 10);
    if (!hit) {
      have_stamp = false;
      return;
    }
    Brush brush;
    brush.from = have_stamp ? last_stamp : *hit;
    brush.to = *hit;
    brush.radius = radius;
    brush.normal = volume.gradient(*hit);
    brush.tool = tool;
    brush.strength = tool == Tool::Smooth ? .55f : 1;
    brush.blend = std::min(.025f, radius * .25f);
    volume.stamp(brush);
    last_stamp = *hit;
    have_stamp = true;
    invalidate();
  }
  int handle(int event) override {
    try {
      if (event == FL_FOCUS)
        return 1;
      if (event == FL_PUSH) {
        take_focus();
        last_x = Fl::event_x();
        last_y = Fl::event_y();
        ball = project_ball(last_x, last_y);
        sculpting =
            Fl::event_button() == FL_LEFT_MOUSE && Fl::event_state(FL_SHIFT);
        have_stamp = false;
        if (sculpting) {
          volume.begin_stroke();
          sculpt_at(last_x, last_y);
        }
        return 1;
      }
      if (event == FL_DRAG) {
        int x = Fl::event_x(), y = Fl::event_y();
        if (sculpting)
          sculpt_at(x, y);
        else if (Fl::event_state(FL_BUTTON1)) {
          auto next = project_ball(x, y);
          rotation = (arcball(ball, next) * rotation).unit();
          ball = next;
          redraw();
        } else {
          float scale = distance * .0015f;
          pan.x += (x - last_x) * scale;
          pan.y -= (y - last_y) * scale;
          redraw();
        }
        last_x = x;
        last_y = y;
        return 1;
      }
      if (event == FL_RELEASE) {
        if (sculpting) {
          volume.end_stroke();
          sculpting = false;
          have_stamp = false;
          invalidate();
        }
        return 1;
      }
      if (event == FL_UNFOCUS) {
        if (sculpting) {
          volume.cancel_stroke();
          sculpting = false;
          have_stamp = false;
          invalidate();
        }
        return 1;
      }
      if (event == FL_MOUSEWHEEL) {
        distance =
            std::clamp(distance * std::exp(.1f * Fl::event_dy()), .15f, 20.f);
        redraw();
        return 1;
      }
      if (event == FL_KEYDOWN && Fl::event_key() == FL_Escape && sculpting) {
        volume.cancel_stroke();
        sculpting = false;
        invalidate();
        return 1;
      }
    } catch (const std::exception &e) {
      if (volume.in_stroke())
        volume.cancel_stroke();
      sculpting = false;
      invalidate();
      fl_alert("%s", e.what());
      return 1;
    }
    return Fl_Gl_Window::handle(event);
  }
  void draw() override {
    update_mesh();
    glViewport(0, 0, pixel_w(), pixel_h());
    bool light = Fl::get_color(FL_BACKGROUND_COLOR) > 0x80808000;
    glClearColor(light ? .72f : .11f, light ? .73f : .13f, light ? .75f : .16f,
                 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45, double(pixel_w()) / std::max(1, pixel_h()), .01, 100);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float position[] = {2, 3, 4, 1};
    glLightfv(GL_LIGHT0, GL_POSITION, position);
    glTranslatef(pan.x, pan.y, -distance);
    auto matrix = rotation.matrix();
    glMultMatrixf(matrix.data());
    glColor3f(.68f, .39f, .22f);
    glBegin(GL_TRIANGLES);
    for (auto i : mesh.indices) {
      auto n = mesh.normals[i], p = mesh.positions[i];
      glNormal3f(n.x, n.y, n.z);
      glVertex3f(p.x, p.y, p.z);
    }
    glEnd();
    glDisable(GL_LIGHTING);
    glColor3f(.35f, .38f, .42f);
    glBegin(GL_LINES);
    for (int i = -10; i <= 10; ++i) {
      glVertex3f(i * .1f, -.5f, -1);
      glVertex3f(i * .1f, -.5f, 1);
      glVertex3f(-1, -.5f, i * .1f);
      glVertex3f(1, -.5f, i * .1f);
    }
    glEnd();
    if (!capture.empty()) {
      std::vector<uint8_t> pixels(size_t(pixel_w()) * pixel_h() * 3);
      glPixelStorei(GL_PACK_ALIGNMENT, 1);
      glReadBuffer(GL_BACK);
      glReadPixels(0, 0, pixel_w(), pixel_h(), GL_RGB, GL_UNSIGNED_BYTE,
                   pixels.data());
      std::ofstream out(capture, std::ios::binary);
      out << "P6\n" << pixel_w() << ' ' << pixel_h() << "\n255\n";
      for (int y = pixel_h() - 1; y >= 0; --y)
        out.write(reinterpret_cast<const char *>(pixels.data() +
                                                 size_t(y) * pixel_w() * 3),
                  pixel_w() * 3);
      out.close();
      if (!out) {
        std::cerr << "Cannot write capture\n";
        std::exit(1);
      }
      capture.clear();
      Fl::add_timeout(.1, [](void *) { std::exit(0); });
    }
  }
};
struct App {
  Fl_Double_Window window{1100, 800, "Dome Sculpt"};
  DesktopMenuBar menu{0, 0, 1100, 28};
  Canvas canvas{0, 28, 1100, 744};
  Fl_Box status{0, 772, 1100, 28};
  std::unique_ptr<DesktopMenu> universal;
  bool dirty = false;
  std::filesystem::path project;
  App() {
    window.end();
    window.resizable(canvas);
    window.size_range(640, 480);
    window.xclass("org.domed.Sculpt");
    status.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    menu.add("&File/&New", FL_CTRL + 'n', command, this);
    menu.add("&File/&Open...", FL_CTRL + 'o', command, this);
    menu.add("&File/&Save", FL_CTRL + 's', command, this);
    menu.add("&File/Save &As...", FL_CTRL | FL_SHIFT | 's', command, this);
    menu.add("&File/Export &glTF (.glb)...", FL_CTRL + 'e', command, this);
    menu.add("&File/&Close", FL_CTRL + 'q', command, this);
    menu.add("&Edit/&Undo", FL_CTRL + 'z', command, this);
    menu.add("&Edit/&Redo", FL_CTRL | FL_SHIFT | 'z', command, this);
    menu.add("&Sculpt/&Clay", '1', command, this);
    menu.add("&Sculpt/Ca&rve", '2', command, this);
    menu.add("&Sculpt/&Smooth", '3', command, this);
    menu.add("&Sculpt/&Flatten", '4', command, this);
    menu.add("&Sculpt/Smaller brush", '[', command, this);
    menu.add("&Sculpt/Larger brush", ']', command, this);
    menu.add("&Mesh/Generate &UV atlas", FL_CTRL + 'u', command, this);
    menu.add("&View/Reset &trackball", 'f', command, this);
    canvas.seed();
    canvas.changed = [this] {
      dirty = true;
      update_status();
    };
    menu.layoutChanged = [this] {
      canvas.resize(0, menu.h(), window.w(), window.h() - menu.h() - 28);
      window.redraw();
    };
    window.callback(
        [](Fl_Widget *, void *data) {
          auto &app = *static_cast<App *>(data);
          if (app.discard())
            app.window.hide();
        },
        this);
    universal = std::make_unique<DesktopMenu>(window, menu);
    update_status();
  }
  bool discard() {
    return !dirty || fl_choice("Discard unsaved sculpt changes?", "Cancel",
                               "Discard", nullptr) == 1;
  }
  void update_status() {
    const char *tools[] = {"Clay", "Carve", "Smooth", "Flatten"};
    std::ostringstream text;
    text << tools[int(canvas.tool)] << " | radius " << int(canvas.radius * 1000)
         << " mm | " << compute_backend()
         << " | Drag: trackball  Shift+drag: sculpt  Wheel: zoom";
    status.copy_label(text.str().c_str());
    status.redraw();
    window.copy_label((std::string("Dome Sculpt") +
                       (project.empty() ? " — Untitled"
                                        : " — " + project.filename().string()) +
                       (dirty ? " *" : ""))
                          .c_str());
  }
  static void command(Fl_Widget *, void *data) {
    auto &app = *static_cast<App *>(data);
    auto item = app.menu.mvalue();
    if (!item)
      return;
    try {
      app.action(item->label());
    } catch (const std::exception &e) {
      fl_alert("%s", e.what());
    }
  }
  void action(std::string label) {
    label.erase(std::remove(label.begin(), label.end(), '&'), label.end());
    if (canvas.volume.in_stroke()) {
      canvas.volume.end_stroke();
      canvas.sculpting = false;
    }
    if (label == "New") {
      if (discard()) {
        canvas.seed();
        project.clear();
        dirty = false;
      }
    } else if (label == "Open...") {
      if (!discard())
        return;
      auto path = fl_file_chooser("Open sculpt project", "*.dsculpt", nullptr);
      if (path) {
        auto next = Volume::load(path);
        canvas.volume = std::move(next);
        canvas.invalidate();
        project = path;
        dirty = false;
      }
    } else if (label == "Save" || label == "Save As...") {
      auto path = project;
      if (path.empty() || label == "Save As...") {
        auto chosen = fl_file_chooser("Save sculpt project", "*.dsculpt",
                                      project.empty() ? "Untitled.dsculpt"
                                                      : project.c_str());
        if (!chosen)
          return;
        path = chosen;
      }
      canvas.volume.save(path);
      project = path;
      dirty = false;
    } else if (label == "Close") {
      if (discard())
        window.hide();
    } else if (label == "Undo") {
      if (canvas.volume.undo())
        canvas.invalidate();
    } else if (label == "Redo") {
      if (canvas.volume.redo())
        canvas.invalidate();
    } else if (label == "Clay")
      canvas.tool = Tool::Clay;
    else if (label == "Carve")
      canvas.tool = Tool::Carve;
    else if (label == "Smooth")
      canvas.tool = Tool::Smooth;
    else if (label == "Flatten")
      canvas.tool = Tool::Flatten;
    else if (label == "Smaller brush")
      canvas.radius = std::max(.02f, canvas.radius * .8f);
    else if (label == "Larger brush")
      canvas.radius = std::min(.5f, canvas.radius * 1.25f);
    else if (label == "Generate UV atlas") {
      canvas.update_mesh();
      auto info = unwrap(canvas.mesh);
      std::ostringstream text;
      text << info.charts << " UV charts packed into " << info.width << " x "
           << info.height << " (" << int(info.utilization * 100)
           << "% occupied)";
      fl_message("%s", text.str().c_str());
    } else if (label == "Export glTF (.glb)...") {
      auto chosen =
          fl_file_chooser("Export static glTF mesh", "*.glb", "Sculpt.glb");
      if (chosen) {
        canvas.update_mesh();
        if (canvas.mesh.uv.empty())
          unwrap(canvas.mesh);
        export_glb(canvas.mesh, chosen);
      }
    } else if (label == "Reset trackball") {
      canvas.rotation = {};
      canvas.pan = {};
      canvas.distance = 2.2f;
      canvas.redraw();
    }
    update_status();
  }
};
} // namespace
int main(int argc, char **argv) try {
  if (argc == 2 && std::string(argv[1]) == "--version") {
    std::cout << "Dome Sculpt 0.1.0 / " << compute_backend()
              << " / Blender 5.4 Alpha trackball basis\n";
    return 0;
  }
  if (argc == 3 && std::string(argv[1]) == "--export-demo") {
    Volume field(.04f);
    field.begin_stroke();
    Brush b;
    b.radius = .4f;
    field.stamp(b);
    field.end_stroke();
    auto mesh = surface_mesh(field);
    unwrap(mesh);
    export_glb(mesh, argv[2]);
    return 0;
  }
  dx::init();
  App app;
  if (argc == 3 && std::string(argv[1]) == "--smoke-test")
    app.canvas.capture = argv[2];
  else if (argc == 2) {
    app.canvas.volume = Volume::load(argv[1]);
    app.canvas.invalidate();
    app.project = argv[1];
    app.dirty = false;
    app.update_status();
  } else if (argc != 1) {
    std::cerr << "Usage: dome-sculpt [project.dsculpt] [--smoke-test image.ppm "
                 "| --export-demo mesh.glb | --version]\n";
    return 2;
  }
  app.window.show();
  return Fl::run();
} catch (const std::exception &e) {
  std::cerr << e.what() << '\n';
  return 1;
}
