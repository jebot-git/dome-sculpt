// SPDX-License-Identifier: GPL-3.0-or-later
#include "appearance.h"
#include "desktop-menu.h"
#include "dome/document.hpp"
#include "dome/mesh.hpp"
#include "dome/render.hpp"
#include "dome/session-appearance.hpp"
#include "object-editor.hpp"
#include "paint-editor.hpp"
#include "ui-conventions.hpp"
#ifdef DOME_HAS_XR
#include "dome/xr.hpp"
#endif
#include <FL/Fl_Box.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Gl_Window.H>
#include <FL/fl_file_chooser.H>
#include <FL/gl.h>
#include <FL/glu.h>
#include <bit>
#include <functional>
#include <iostream>
#include <sstream>
#include <thread>
using namespace dome;
namespace {
struct Canvas : Fl_Gl_Window {
  Document document;
  std::unique_ptr<SurfaceRenderer> renderer;
  std::shared_ptr<const MeshSnapshot> shown_mesh;
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
    mode(FL_RGB | FL_DOUBLE | FL_DEPTH | FL_OPENGL3);
    end();
  }
  ~Canvas() {
    if (renderer) {
      make_current();
      renderer.reset();
    }
  }
  void seed() {
    document.seed();
    document.wait_mesh();
    invalidate();
  }
  void invalidate() {
    dirty_mesh = true;
    redraw();
    if (changed)
      changed();
  }
  void update_mesh() {
    auto f = document.frame();
    if (f.surface != shown_mesh) {
      shown_mesh = f.surface;
      mesh = f.surface->mesh;
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
    auto hit = document.raycast(origin, direction);
    if (!hit) {
      have_stamp = false;
      return;
    }
    Brush brush;
    brush.from = have_stamp ? last_stamp : *hit;
    brush.to = *hit;
    brush.radius = radius;
    brush.normal = document.gradient(*hit);
    brush.tool = tool;
    brush.strength = tool == Tool::Smooth ? .55f : 1;
    brush.blend = std::min(.025f, radius * .25f);
    document.stamp_desktop(brush);
    last_stamp = *hit;
    have_stamp = true;
    invalidate();
  }
  int handle(int event) override {
    try {
      if (event == FL_FOCUS)
        return 1;
      if (event == FL_KEYDOWN && Fl::event_key() == FL_Escape &&
          document.frame().state->menu) {
        document.cancel_spatial_menu();
        return 1;
      }
      if (event == FL_PUSH) {
        take_focus();
        last_x = Fl::event_x();
        last_y = Fl::event_y();
        ball = project_ball(last_x, last_y);
        sculpting =
            Fl::event_button() == FL_LEFT_MOUSE && Fl::event_state(FL_SHIFT);
        have_stamp = false;
        if (sculpting) {
          if (!document.begin_desktop()) {
            sculpting = false;
            return 1;
          }
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
          document.end_desktop();
          sculpting = false;
          have_stamp = false;
          invalidate();
        }
        return 1;
      }
      if (event == FL_UNFOCUS) {
        if (sculpting) {
          document.cancel_desktop();
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
        document.cancel_desktop();
        sculpting = false;
        invalidate();
        return 1;
      }
    } catch (const std::exception &e) {
      if (sculpting)
        document.cancel_desktop();
      sculpting = false;
      invalidate();
      fl_alert("%s", e.what());
      return 1;
    }
    return Fl_Gl_Window::handle(event);
  }
  void draw() override {
    update_mesh();
    if (!context_valid() && renderer) {
      renderer->abandon();
      renderer.reset();
    }
    if (!renderer)
      renderer = std::make_unique<SurfaceRenderer>();
    CanvasTransform model{{pan.x, pan.y, pan.z},
                          {rotation.x, rotation.y, rotation.z, rotation.w},
                          1};
    ob::Vec3 eye{0, 0, distance};
    float up = .3926990817f, right = std::atan(std::tan(up) * float(pixel_w()) /
                                               std::max(1, pixel_h()));
    auto vp =
        multiply(projection(-right, right, -up, up), view_matrix(eye, {}));
    auto f = document.frame();
    renderer->draw(f.surface, model, vp, eye, pixel_w(), pixel_h(), false,
                   f.state->toon, f.state->light);
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
  SculptWindow window{1100, 800, "Dome Sculpt"};
  DesktopMenuBar menu{0, 0, 1100, 28};
  Canvas canvas{0, 28, 1100, 744};
  Fl_Box status{0, 772, 1100, 28};
  std::unique_ptr<DesktopMenu> universal;
  bool dirty = false;
  std::filesystem::path project;
  std::thread xr_worker;
  std::atomic_bool stop_xr{false}, xr_running{false};
  uint64_t seen_revision = 0, font_revision = UINT64_MAX;
  std::string seen_error;
  ~App() {
    Fl::remove_timeout(tick, this);
    stop_xr = true;
    if (xr_worker.joinable())
      xr_worker.join();
  }
  static void tick(void *data) {
    auto &a = *static_cast<App *>(data);
    auto f = a.canvas.document.frame();
    if (f.surface != a.canvas.shown_mesh)
      a.canvas.redraw();
    a.dirty = f.state->dirty;
    a.canvas.tool = f.state->tool;
    a.canvas.radius = f.state->radius;
    if (a.font_revision != dx::font_revision) {
      a.font_revision = dx::font_revision;
      int opacity = 255;
      try {
        std::ifstream input(dx::config());
        nlohmann::json j;
        if (input) {
          input >> j;
          opacity = j.value("opacity", 255);
        }
      } catch (const std::exception &) {
      }
      a.canvas.document.appearance(
          dx::selected_font.file, dx::size,
          Fl::get_color(FL_BACKGROUND_COLOR) > 0x80808000,
          Fl::get_color(FL_SELECTION_COLOR) >> 8, chrome_opacity(opacity));
      a.canvas.redraw();
    }
    a.sync_menus();
    a.update_status();
    Fl::repeat_timeout(.03, tick, data);
  }
  void toggle_xr() {
#ifdef DOME_HAS_XR
    if (xr_running) {
      stop_xr = true;
      return;
    }
    if (xr_worker.joinable())
      xr_worker.join();
    stop_xr = false;
    xr_running = true;
    xr_worker = std::thread([this] {
      try {
        XrSculpt xr(canvas.document, &stop_xr);
        xr.run();
      } catch (const std::exception &e) {
        try {
          canvas.document.finish_spatial();
        } catch (const std::exception &) {
        }
        canvas.document.report_error(e.what());
        std::cerr << "XR: " << e.what() << '\n';
      }
      xr_running = false;
    });
#else
    fl_alert("This build has no OpenXR support");
#endif
  }

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
    menu.add("&File/Close &Window", FL_CTRL + 'w', command, this);
    if (!dx::dome())
      menu.add("&File/&Quit", FL_CTRL + 'q', command, this);
    menu.add("&Edit/&Undo", FL_CTRL + 'z', command, this);
    menu.add("&Edit/&Redo", FL_CTRL | FL_SHIFT | 'z', command, this);
    menu.add("&View/Reset &trackball", 'f', command, this);
    menu.add("&View/Start or stop &XR", FL_F + 6, command, this);
    menu.add("&View/Shading/&PBR", 0, command, this,
             FL_MENU_RADIO | FL_MENU_VALUE);
    menu.add("&View/Shading/&Toon preview", 0, command, this, FL_MENU_RADIO);
    menu.add("&Sculpt/&Clay", '1', command, this,
             FL_MENU_RADIO | FL_MENU_VALUE);
    menu.add("&Sculpt/Ca&rve", '2', command, this, FL_MENU_RADIO);
    menu.add("&Sculpt/&Smooth", '3', command, this, FL_MENU_RADIO);
    menu.add("&Sculpt/&Flatten", '4', command, this, FL_MENU_RADIO);
    menu.add("&Sculpt/Smaller brush", '[', command, this);
    menu.add("&Sculpt/Larger brush", ']', command, this);
    menu.add("&Scene/New sphere", FL_CTRL | FL_SHIFT | 'n', command, this);
    menu.add("&Scene/New empty sculpt", 0, command, this);
    menu.add("&Scene/Duplicate object", FL_CTRL + 'd', command, this);
    menu.add("&Scene/Delete object", FL_Delete, command, this);
    menu.add("&Scene/Previous object", FL_CTRL + FL_Page_Up, command, this);
    menu.add("&Scene/Next object", FL_CTRL + FL_Page_Down, command, this);
    menu.add("&Scene/&Visible", 0, command, this,
             FL_MENU_TOGGLE | FL_MENU_VALUE);
    menu.add("&Scene/&Locked", 0, command, this, FL_MENU_TOGGLE);
    menu.add("&Scene/Object and material...", FL_ALT + FL_Enter, command, this);
    menu.add("&Paint/Prepare painting mesh", FL_CTRL | FL_SHIFT | 'p', command,
             this);
    menu.add("&Paint/Use painted mesh", 'p', command, this, FL_MENU_RADIO);
    menu.add("&Paint/Edit source volume", FL_SHIFT | 'p', command, this,
             FL_MENU_RADIO | FL_MENU_VALUE);
    menu.add("&Paint/Brush settings...", FL_ALT | 'p', command, this);
    menu.add("&Paint/Add layer", FL_CTRL | 'l', command, this);
    menu.add("&Paint/Next layer", 0, command, this);
    menu.add("&Paint/Layer visible", 0, command, this, FL_MENU_TOGGLE);
    menu.add("&Paint/Delete layer", 0, command, this);
    menu.add("&Paint/Cycle layer opacity", 0, command, this);
    menu.add("&Paint/Channel/Base color", '5', command, this,
             FL_MENU_RADIO | FL_MENU_VALUE);
    menu.add("&Paint/Channel/Metallic", '6', command, this, FL_MENU_RADIO);
    menu.add("&Paint/Channel/Roughness", '7', command, this, FL_MENU_RADIO);
    menu.add("&Paint/Channel/Emission", '8', command, this, FL_MENU_RADIO);
    menu.add("&Paint/Channel/Layer mask", '9', command, this, FL_MENU_RADIO);
    menu.add("&Paint/Color/Red paint", 0, command, this);
    menu.add("&Paint/Color/Green paint", 0, command, this);
    menu.add("&Paint/Color/Blue paint", 0, command, this);
    menu.add("&Paint/Color/White paint", 0, command, this);
    menu.add("&Paint/Color/Black paint", 0, command, this);
    menu.add("&Paint/Toggle eraser", '0', command, this, FL_MENU_TOGGLE);
    menu.add("&Mesh/Generate &UV atlas", FL_CTRL + 'u', command, this);
    canvas.document.appearance(dx::selected_font.file, dx::size,
                               Fl::get_color(FL_BACKGROUND_COLOR) > 0x80808000,
                               Fl::get_color(FL_SELECTION_COLOR) >> 8, 255);
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
    Fl::add_timeout(.03, tick, this);
  }
  bool discard() {
    dirty = canvas.document.frame().state->dirty;
    return !dirty || confirm_discard(window);
  }
  void sync_menus() {
    auto s = canvas.document.frame().state;
    bool modal = Fl::modal() && Fl::modal() != &window;
    auto enable = [&](const char *path, bool value) {
      dx::enable_menu(menu, path, value);
    };
    auto checked = [&](const char *path, bool value) {
      int index = menu.find_index(path);
      if (index < 0)
        return;
      int flags = menu.mode(index),
          next = value ? flags | FL_MENU_VALUE : flags & ~FL_MENU_VALUE;
      if (flags != next)
        menu.mode(index, next);
    };
    const char *document_actions[] = {"&File/&New",
                                      "&File/&Open...",
                                      "&File/&Save",
                                      "&File/Save &As...",
                                      "&File/Export &glTF (.glb)...",
                                      "&Scene/New sphere",
                                      "&Scene/New empty sculpt",
                                      "&Scene/Duplicate object",
                                      "&Scene/Object and material...",
                                      "&Mesh/Generate &UV atlas"};
    for (auto path : document_actions)
      enable(path, !modal && !s->asset_busy);
    auto input = modal ? dynamic_cast<Fl_Input_ *>(Fl::focus()) : nullptr;
    enable("&Edit/&Undo", modal ? (input && input->can_undo()) : s->can_undo);
    enable("&Edit/&Redo", modal ? (input && input->can_redo()) : s->can_redo);
    enable("&Scene/Delete object",
           !modal && !s->asset_busy && s->objects.size() > 1);
    enable("&Scene/Next object", !modal && s->objects.size() > 1);
    enable("&Scene/Previous object", !modal && s->objects.size() > 1);
    auto active =
        std::find_if(s->objects.begin(), s->objects.end(),
                     [&](auto &o) { return o.id == s->active_object; });
    if (active != s->objects.end()) {
      checked("&Scene/&Visible", active->visible);
      checked("&Scene/&Locked", active->locked);
    }
    enable("&Scene/&Visible", !modal);
    enable("&Scene/&Locked", !modal);
    const char *tools[] = {"&Sculpt/&Clay", "&Sculpt/Ca&rve", "&Sculpt/&Smooth",
                           "&Sculpt/&Flatten"};
    for (int i = 0; i < 4; ++i) {
      checked(tools[i], int(s->tool) == i);
      enable(tools[i], !modal && !s->asset_busy);
    }
    checked("&Paint/Use painted mesh", s->mesh_active);
    checked("&Paint/Edit source volume", !s->mesh_active);
    checked("&Paint/Toggle eraser", s->paint.erase);
    checked("&Paint/Layer visible", s->layer_visible);
    const char *channels[] = {
        "&Paint/Channel/Base color", "&Paint/Channel/Metallic",
        "&Paint/Channel/Roughness", "&Paint/Channel/Emission",
        "&Paint/Channel/Layer mask"};
    for (int i = 0; i < 5; ++i) {
      checked(channels[i], int(s->paint.channel) == i);
      enable(channels[i], !modal && !s->asset_busy);
    }
    checked("&View/Shading/&PBR", !s->toon);
    checked("&View/Shading/&Toon preview", s->toon);
    const char *layer_actions[] = {
        "&Paint/Add layer", "&Paint/Next layer", "&Paint/Layer visible",
        "&Paint/Delete layer", "&Paint/Cycle layer opacity"};
    for (auto path : layer_actions)
      enable(path, !modal && !s->asset_busy && s->mesh_active);
    enable("&Paint/Delete layer",
           !modal && s->mesh_active && s->paint_layers.size() > 1);
    enable("&Paint/Next layer",
           !modal && s->mesh_active && s->paint_layers.size() > 1);
    enable("&Paint/Prepare painting mesh",
           !modal && !s->asset_busy && active != s->objects.end() &&
               active->visible && !active->locked);
    enable("&Paint/Use painted mesh", !modal && !s->asset_busy);
    enable("&Paint/Edit source volume",
           !modal && s->mesh_active && !s->asset_busy);
  }
  void update_status() {
    const char *tools[] = {"Clay", "Carve", "Smooth", "Flatten"};
    std::ostringstream text;
    text << tools[int(canvas.tool)] << " | radius " << int(canvas.radius * 1000)
         << " mm | " << compute_backend()
         << " | Drag: trackball  Shift+drag: sculpt  Wheel: zoom";
    auto info = canvas.document.frame().state;
    auto active =
        std::find_if(info->objects.begin(), info->objects.end(),
                     [&](auto &o) { return o.id == info->active_object; });
    if (active != info->objects.end())
      text << " | " << active->name << " (" << info->objects.size()
           << " objects)" << (active->locked ? " [locked]" : "")
           << (active->visible ? "" : " [hidden]");
    if (info->mesh_active) {
      text.str("");
      const char *channels[] = {"Color", "Metallic", "Roughness", "Emission",
                                "Mask"};
      text << "Paint " << channels[int(info->paint.channel)] << " | radius "
           << int(canvas.radius * 1000) << " mm | " << info->paint_layers.size()
           << " layers | Shift+drag: paint | P: painted mesh | Shift+P: source";
    }
    if (!info->error.empty()) {
      text.str("");
      text << info->error;
    } else if (!info->message.empty()) {
      text.str("");
      text << info->message;
    } else if (xr_running) {
      auto hints = text.str();
      text.str("");
      text << "XR active | " << hints;
    }
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
    auto modal = Fl::modal();
    if (modal && modal != &window) {
      if (label == "Close Window")
        modal->hide();
      else if (auto input = dynamic_cast<Fl_Input_ *>(Fl::focus())) {
        if (label == "Undo")
          input->undo();
        else if (label == "Redo")
          input->redo();
      }
      return;
    }
    if (canvas.sculpting) {
      canvas.document.end_desktop();
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
        canvas.document.open(path);
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
      canvas.document.save(path);
      project = path;
      dirty = false;
    } else if (label == "Close Window" || label == "Quit") {
      if (discard())
        window.hide();
    } else if (label == "Undo") {
      if (canvas.document.undo())
        canvas.invalidate();
    } else if (label == "Redo") {
      if (canvas.document.redo())
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
    else if (label == "New sphere" || label == "New empty sculpt") {
      canvas.document.add_object(label == "New sphere");
    } else if (label == "Duplicate object") {
      canvas.document.duplicate_object();
    } else if (label == "Delete object") {
      canvas.document.delete_object();
    } else if (label == "Object and material...") {
      edit_object(canvas.document);
    } else if (label == "Next object" || label == "Previous object") {
      auto state = canvas.document.frame().state;
      auto found =
          std::find_if(state->objects.begin(), state->objects.end(),
                       [&](auto &o) { return o.id == state->active_object; });
      int index = int(found - state->objects.begin()),
          count = int(state->objects.size());
      int next = (index + count + (label == "Next object" ? 1 : -1)) % count;
      canvas.document.select_object(state->objects[next].id);
    } else if (label == "Visible" || label == "Locked") {
      auto scene = canvas.document.copy_scene();
      auto &o = scene.active();
      canvas.document.object_properties(
          o.id, o.name, label == "Visible" ? !o.visible : o.visible,
          label == "Locked" ? !o.locked : o.locked, o.transform, o.material);
    } else if (label == "Prepare painting mesh") {
      canvas.document.prepare_mesh();
    } else if (label == "Use painted mesh") {
      canvas.document.use_mesh();
    } else if (label == "Edit source volume") {
      canvas.document.use_source();
    } else if (label == "Brush settings...") {
      edit_paint(canvas.document);
    } else if (label == "Add layer")
      canvas.document.paint_command(SpatialCommand::AddLayer);
    else if (label == "Next layer")
      canvas.document.paint_command(SpatialCommand::NextLayer);
    else if (label == "Layer visible")
      canvas.document.paint_command(SpatialCommand::LayerVisible);
    else if (label == "Delete layer")
      canvas.document.paint_command(SpatialCommand::DeleteLayer);
    else if (label == "Cycle layer opacity")
      canvas.document.paint_command(SpatialCommand::LayerOpacity);
    else if (label == "Base color")
      canvas.document.paint_command(SpatialCommand::PaintColor);
    else if (label == "Metallic")
      canvas.document.paint_command(SpatialCommand::PaintMetal);
    else if (label == "Roughness")
      canvas.document.paint_command(SpatialCommand::PaintRough);
    else if (label == "Emission")
      canvas.document.paint_command(SpatialCommand::PaintGlow);
    else if (label == "Layer mask")
      canvas.document.paint_command(SpatialCommand::PaintMask);
    else if (label == "Red paint")
      canvas.document.paint_command(SpatialCommand::PaintRed);
    else if (label == "Green paint")
      canvas.document.paint_command(SpatialCommand::PaintGreen);
    else if (label == "Blue paint")
      canvas.document.paint_command(SpatialCommand::PaintBlue);
    else if (label == "White paint")
      canvas.document.paint_command(SpatialCommand::PaintWhite);
    else if (label == "Black paint")
      canvas.document.paint_command(SpatialCommand::PaintBlack);
    else if (label == "Toggle eraser")
      canvas.document.paint_command(SpatialCommand::ErasePaint);
    else if (label == "Generate UV atlas") {
      canvas.mesh = surface_mesh(canvas.document.copy_volume());
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
        export_glb(canvas.document.copy_scene().surfaces(true), chosen);
      }
    } else if (label == "Start or stop XR") {
      toggle_xr();
    } else if (label == "PBR") {
      canvas.document.shading(false);
      canvas.redraw();
    } else if (label == "Toon preview") {
      canvas.document.shading(true);
      canvas.redraw();
    } else if (label == "Reset trackball") {
      canvas.rotation = {};
      canvas.pan = {};
      canvas.distance = 2.2f;
      canvas.redraw();
    }
    if (label == "Clay" || label == "Carve" || label == "Smooth" ||
        label == "Flatten")
      canvas.document.use_source();
    if (label == "Clay" || label == "Carve" || label == "Smooth" ||
        label == "Flatten" || label == "Smaller brush" ||
        label == "Larger brush")
      canvas.document.configure(canvas.tool, canvas.radius);
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
  if (argc == 4 && std::string(argv[1]) == "--export-project") {
    export_glb(Scene::load(argv[2]).surfaces(true), argv[3]);
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
#ifdef DOME_HAS_XR
  if (argc >= 2 && std::string(argv[1]) == "--probe") {
    XrSculpt::probe();
    return 0;
  }
  if (argc >= 2 && std::string(argv[1]) == "--xr") {
    Document document;
    document.seed();
    unsigned frames = 0;
    std::filesystem::path save;
    bool overlay = true, paint = false;
    for (int i = 2; i < argc; ++i) {
      std::string option = argv[i];
      if (option == "--open" && i + 1 < argc)
        document.open(argv[++i]);
      else if (option == "--save" && i + 1 < argc)
        save = argv[++i];
      else if (option == "--frames" && i + 1 < argc)
        frames = std::stoul(argv[++i]);
      else if (option == "--paint")
        paint = true;
      else if (option == "--exclusive")
        overlay = false;
      else
        throw std::runtime_error("Unknown XR option: " + option);
    }
    SessionAppearanceMonitor appearance(document);
    if (paint) {
      document.prepare_mesh(256);
      if (!document.wait_asset() || !document.frame().state->mesh_active)
        throw std::runtime_error(document.frame().state->error);
    }
    document.wait_mesh();
    {
      XrSculpt xr(document, nullptr, overlay);
      xr.run(frames);
    }
    if (!save.empty())
      document.save(save);
    return 0;
  }
#endif
  if (argc == 3 && std::string(argv[1]) == "--project-info") {
    auto scene = Scene::load(argv[2]);
    auto &volume = *scene.active().volume;
    std::vector<std::pair<Int3, float>> samples(volume.voxels().begin(),
                                                volume.voxels().end());
    std::sort(samples.begin(), samples.end(),
              [](auto &a, auto &b) { return a.first < b.first; });
    uint64_t hash = 1469598103934665603ULL;
    for (auto [p, value] : samples) {
      uint32_t words[] = {uint32_t(p.x), uint32_t(p.y), uint32_t(p.z),
                          std::bit_cast<uint32_t>(value)};
      for (auto word : words)
        for (int i = 0; i < 4; ++i) {
          hash ^= uint8_t(word >> (i * 8));
          hash *= 1099511628211ULL;
        }
    }
    uint64_t paint_hash = 1469598103934665603ULL;
    size_t blue_texels = 0;
    if (scene.active().painted) {
      auto map = scene.active().painted->composite(scene.active().material);
      for (auto byte : map->color.rgba) {
        paint_hash ^= byte;
        paint_hash *= 1099511628211ULL;
      }
      for (size_t i = 0; i < map->color.rgba.size(); i += 4)
        blue_texels += map->color.rgba[i + 2] > map->color.rgba[i] * 1.5f &&
                       map->color.rgba[i + 2] > 100;
    }
    std::cout << nlohmann::json{{"paint_blue_texels", blue_texels},
                                {"paint_hash", paint_hash},
                                {"painted", scene.active().mesh_active},
                                {"paint_layers",
                                 scene.active().painted
                                     ? scene.active().painted->layers().size()
                                     : 0},
                                {"object_count", scene.objects().size()},
                                {"active_object", scene.active().id},
                                {"active_color", scene.active().material.color},
                                {"active_metallic",
                                 scene.active().material.metallic},
                                {"active_roughness",
                                 scene.active().material.roughness},
                                {"samples", samples.size()},
                                {"voxel_size", volume.voxel_size()},
                                {"field_hash", hash}}
                     .dump()
              << '\n';
    return 0;
  }
  dx::init();
  App app;
  if (argc == 3 && std::string(argv[1]) == "--smoke-test")
    app.canvas.capture = argv[2];
  else if (argc == 2) {
    app.canvas.document.open(argv[1]);
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
