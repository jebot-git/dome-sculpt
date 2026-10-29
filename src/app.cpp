// SPDX-License-Identifier: GPL-3.0-or-later
#include "appearance.h"
#include "desktop-menu.h"
#include "dome/document.hpp"
#include "dome/frame-clock.hpp"
#include "dome/mesh.hpp"
#include "dome/render.hpp"
#include "dome/session-appearance.hpp"
#include "object-editor.hpp"
#include "creator-editor.hpp"
#include "rig-editor.hpp"
#include "weight-editor.hpp"
#include "paint-editor.hpp"
#include "mesh-editor.hpp"
#include "bake-editor.hpp"
#include "ui-conventions.hpp"
#include "uv-editor.hpp"
#ifdef DOME_HAS_XR
#include "dome/xr.hpp"
#endif
#include <FL/Fl_Box.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Gl_Window.H>
#include <FL/gl.h>
#include <FL/glu.h>
#include <bit>
#include <atomic>
#include <csignal>
#include <functional>
#include <iostream>
#include <sstream>
#include <thread>
using namespace dome;
namespace {
#ifdef DOME_HAS_XR
// A signal handler only requests a stop. Frame completion, input draining,
// OpenXR cleanup and final saving happen normally on the application threads.
std::atomic_bool standalone_xr_stop{false};
static_assert(std::atomic_bool::is_always_lock_free);
void request_standalone_xr_stop(int) {
  standalone_xr_stop.store(true, std::memory_order_relaxed);
}
class StandaloneXrSignals {
  using Handler = void (*)(int);
  Handler previous_int_, previous_term_;
public:
  StandaloneXrSignals() {
    standalone_xr_stop.store(false);
    previous_int_ = std::signal(SIGINT, request_standalone_xr_stop);
    if (previous_int_ == SIG_ERR) throw std::runtime_error("Cannot install XR interrupt handler");
    previous_term_ = std::signal(SIGTERM, request_standalone_xr_stop);
    if (previous_term_ == SIG_ERR) {
      std::signal(SIGINT, previous_int_);
      throw std::runtime_error("Cannot install XR termination handler");
    }
  }
  ~StandaloneXrSignals() {
    std::signal(SIGTERM, previous_term_);
    std::signal(SIGINT, previous_int_);
  }
};
#endif
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
    document.enable_recovery();
    document.refresh_recovery();
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
    renderer->set_frame_time(render_frame_time());
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
  DomeEventDispatch events;
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
    menu.add("&File/Import &mesh...", 0, command, this);
    menu.add("&File/&Save", FL_CTRL + 's', command, this);
    menu.add("&File/Save &As...", FL_CTRL | FL_SHIFT | 's', command, this);
    menu.add("&File/Export &glTF (.glb)...", FL_CTRL + 'e', command, this);
    menu.add("&File/Export selected avatar (.vrm)...", 0, command, this);
    menu.add("&File/Recovery/Refresh recovery", 0, command, this);
    menu.add("&File/Recovery/Recover project...", 0, command, this);
    menu.add("&File/Close &Window", FL_CTRL + 'w', command, this);
    if (!dx::dome())
      menu.add("&File/&Quit", FL_CTRL + 'q', command, this);
    menu.add("&Edit/&Undo", FL_CTRL + 'z', command, this);
    menu.add("&Edit/&Redo", FL_CTRL | FL_SHIFT | 'z', command, this);
    menu.add("&Edit/Cu&t", FL_CTRL + 'x', command, this);
    menu.add("&Edit/&Copy", FL_CTRL + 'c', command, this);
    menu.add("&Edit/&Paste", FL_CTRL + 'v', command, this);
    menu.add("&Edit/Select &All", FL_CTRL + 'a', command, this);
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
    menu.add("&Scene/Create &humanoid...", 0, command, this);
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
    menu.add("&Scene/Skin weights...", 0, command, this);
    menu.add("&Scene/Rig and shapekeys...", 0, command, this);
    menu.add("&Mesh/Voxel remesh...", 0, command, this);
    menu.add("&Mesh/Bake surface...",0,command,this);
    menu.add("&Mesh/Clear baked surface",0,command,this);
    menu.add("&File/Cancel background work",0,command,this);
    menu.add("&Mesh/UV editor...", FL_ALT + 'u', command, this);
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
    menu.add("&Sculpt/Layers/Sculpt layer properties...", 0, command, this);
    menu.add("&Sculpt/Layers/Add sculpt layer", 0, command, this);
    menu.add("&Sculpt/Layers/Next sculpt layer", 0, command, this);
    menu.add("&Sculpt/Layers/Previous sculpt layer", 0, command, this);
    menu.add("&Sculpt/Layers/Sculpt layer visible", 0, command, this,
             FL_MENU_TOGGLE);
    menu.add("&Sculpt/Layers/Sculpt layer locked", 0, command, this,
             FL_MENU_TOGGLE);
    menu.add("&Sculpt/Layers/Cycle sculpt strength", 0, command, this);
    menu.add("&Sculpt/Layers/Delete sculpt layer", 0, command, this);
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
    std::vector<int> modes;
    for (int i = 0; i < menu.size(); ++i) {
      auto item = menu.menu() + i;
      int flags = menu.mode(i);
      if (item->label() && !(flags & FL_SUBMENU))
        flags = (!modal && !s->asset_busy) ? flags & ~FL_MENU_INACTIVE
                                           : flags | FL_MENU_INACTIVE;
      modes.push_back(flags);
    }
    auto enable = [&](const char *path, bool value) {
      int i = menu.find_index(path);
      if (i >= 0)
        modes[i] =
            value ? modes[i] & ~FL_MENU_INACTIVE : modes[i] | FL_MENU_INACTIVE;
    };
    auto checked = [&](const char *path, bool value) {
      int i = menu.find_index(path);
      if (i >= 0)
        modes[i] = value ? modes[i] | FL_MENU_VALUE : modes[i] & ~FL_MENU_VALUE;
    };
    enable("&File/Recovery/Refresh recovery", !modal && !s->recovery_busy);
    enable("&File/Recovery/Recover project...", !modal && !s->recovery_busy &&
           !s->dirty && !s->asset_busy && !s->recovery_candidates.empty());
    enable("&Mesh/Voxel remesh...", !modal && s->can_voxel_remesh);
    enable("&Mesh/Bake surface...",!modal && s->can_bake);
    enable("&Mesh/Clear baked surface",!modal && s->can_clear_bake);
    enable("&File/Cancel background work",!modal && s->can_cancel_asset);
    enable("&File/Close &Window", true);
    const char *views[] = {"&View/Reset &trackball", "&View/Start or stop &XR",
                           "&View/Shading/&PBR", "&View/Shading/&Toon preview"};
    for (auto path : views)
      enable(path, !modal);
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
    auto input = dynamic_cast<Fl_Input_ *>(Fl::focus());
    if (input && (!input->active_r() || !input->visible_r() ||
                  input->window() != (modal ? Fl::modal() : &window)))
      input = nullptr;
    bool selection = input && input->insert_position() != input->mark();
    bool copyable = selection && input->input_type() != FL_SECRET_INPUT;
    enable("&Edit/Cu&t", copyable && !input->readonly());
    enable("&Edit/&Copy", copyable);
    enable("&Edit/&Paste",
           input && !input->readonly() &&
               Fl::clipboard_contains(Fl::clipboard_plain_text));
    enable("&Edit/Select &All", input && input->size() > 0);
    enable("&Edit/&Undo",
           modal ? (input ? input->can_undo() : s->uv && s->uv->can_undo)
                 : s->can_undo);
    enable("&Edit/&Redo",
           modal ? (input ? input->can_redo() : s->uv && s->uv->can_redo)
                 : s->can_redo);
    enable("&File/Export selected avatar (.vrm)...", !modal && !s->asset_busy && s->can_export_avatar);
    enable("&Scene/Delete object",
           !modal && !s->asset_busy && s->objects.size() > 1);
    enable("&Scene/Next object", !modal && s->objects.size() > 1);
    enable("&Scene/Previous object", !modal && s->objects.size() > 1);
    auto active =
        std::find_if(s->objects.begin(), s->objects.end(),
                     [&](auto &o) { return o.id == s->active_object; });
    enable("&Scene/Skin weights...", !modal && !s->asset_busy && s->can_paint_weights);
    enable("&Scene/Rig and shapekeys...", !modal && !s->asset_busy &&
           s->mesh_active && bool(s->deformation) && active != s->objects.end() &&
           active->visible && !active->locked);
    if (active != s->objects.end()) {
      checked("&Scene/&Visible", active->visible);
      checked("&Scene/&Locked", active->locked);
    }
    enable("&Scene/&Visible", !modal);
    enable("&Scene/&Locked", !modal);
    bool sculpt_ready = !modal && !s->asset_busy && !s->mesh_active &&
                        active != s->objects.end() && active->visible &&
                        !active->locked;
    enable("&Sculpt/Layers/Add sculpt layer",
           sculpt_ready && s->sculpt_layers.size() < 16);
    enable("&Sculpt/Layers/Next sculpt layer",
           sculpt_ready && s->sculpt_layers.size() > 1);
    enable("&Sculpt/Layers/Previous sculpt layer",
           sculpt_ready && s->sculpt_layers.size() > 1);
    enable("&Sculpt/Layers/Delete sculpt layer",
           sculpt_ready && s->sculpt_layers.size() > 1);
    for (auto path : {"&Sculpt/Layers/Sculpt layer visible",
                      "&Sculpt/Layers/Sculpt layer locked",
                      "&Sculpt/Layers/Cycle sculpt strength",
                      "&Sculpt/Layers/Sculpt layer properties..."})
      enable(path, sculpt_ready);
    checked("&Sculpt/Layers/Sculpt layer visible", s->sculpt_layer_visible);
    checked("&Sculpt/Layers/Sculpt layer locked", s->sculpt_layer_locked);
    const char *tools[] = {"&Sculpt/&Clay", "&Sculpt/Ca&rve", "&Sculpt/&Smooth",
                           "&Sculpt/&Flatten"};
    for (int i = 0; i < 4; ++i) {
      checked(tools[i], int(s->tool) == i);
      enable(tools[i], !modal && !s->asset_busy);
    }
    enable("&Sculpt/Smaller brush",
           !modal && !s->asset_busy && s->radius > .005f);
    enable("&Sculpt/Larger brush", !modal && !s->asset_busy && s->radius < 2);
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
    enable("&Paint/Add layer", !modal && !s->asset_busy && s->mesh_active &&
                                   s->paint_layers.size() < 16);
    enable("&Paint/Delete layer", !modal && !s->asset_busy && s->mesh_active &&
                                      s->paint_layers.size() > 1);
    enable("&Paint/Next layer", !modal && !s->asset_busy && s->mesh_active &&
                                    s->paint_layers.size() > 1);
    enable("&Mesh/UV editor...", !modal && !s->asset_busy && s->mesh_active &&
                                     active != s->objects.end() &&
                                     active->visible && !active->locked);
    enable("&Paint/Prepare painting mesh",
           !modal && !s->asset_busy && active != s->objects.end() &&
               active->visible && !active->locked);
    enable("&Mesh/Generate &UV atlas", !modal && !s->asset_busy &&
                                           active != s->objects.end() &&
                                           active->visible && !active->locked);
    enable("&Paint/Use painted mesh",
           !modal && !s->asset_busy && active != s->objects.end() &&
               (active->has_mesh || (active->visible && !active->locked)));
    enable("&Paint/Edit source volume",
           !modal && s->mesh_active && !s->asset_busy);
    for (int i = 0; i < menu.size(); ++i)
      if (menu.mode(i) != modes[i])
        menu.mode(i, modes[i]);
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
    if (!info->mesh_active)
      text << " | " << info->sculpt_layer_name << " ("
           << info->sculpt_layers.size() << " layers, "
           << int(info->sculpt_layer_opacity * 100) << "%)"
           << (info->sculpt_editable ? "" : " [read only]");
    if (info->mesh_active) {
      text.str("");
      const char *channels[] = {"Color", "Metallic", "Roughness", "Emission",
                                "Mask"};
      text << "Paint " << channels[int(info->paint.channel)] << " | radius "
           << int(canvas.radius * 1000) << " mm | " << info->paint_layers.size()
           << " layers | Shift+drag: paint | P: painted mesh | Shift+P: source";
    }
    if (info->weights.enabled) {
      text.str("");
      const char *modes[] = {"Add", "Subtract", "Replace", "Smooth"};
      text << "Weights " << modes[int(info->weights.mode)] << " | "
           << info->deformation->skeleton->bones.at(info->rig_joint).name
           << " | Shift+drag: paint | Blue: 0  Red: 1";
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
    auto recovery = canvas.document.recovery_status();
    if (!recovery.error.empty()) text << " | Recovery: " << recovery.error;
    else if (!info->recovery_error.empty()) text << " | Recovery: " << info->recovery_error;
    else if (info->recovery_busy) text << " | Loading recovery";
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
    if (label == "Cut" || label == "Copy" || label == "Paste" ||
        label == "Select All") {
      auto input = dynamic_cast<Fl_Input_ *>(Fl::focus());
      if (!input || !input->active_r() || !input->visible_r() ||
          input->window() != (modal ? modal : &window))
        return;
      if (label == "Select All")
        input->insert_position(input->size(), 0);
      else if (label == "Paste" && !input->readonly())
        Fl::paste(*input, 1);
      else if (input->input_type() != FL_SECRET_INPUT) {
        if (label == "Copy")
          input->copy(1);
        else if (label == "Cut" && !input->readonly()) {
          input->copy(1);
          input->cut();
        }
      }
      return;
    }
    if (modal && modal != &window) {
      if (label == "Close Window")
        modal->do_callback();
      else if (auto input = dynamic_cast<Fl_Input_ *>(Fl::focus())) {
        if (label == "Undo")
          input->undo();
        else if (label == "Redo")
          input->redo();
      } else if (canvas.document.frame().state->uv) {
        if (label == "Undo")
          canvas.document.edit_uv(UvAction::Undo);
        else if (label == "Redo")
          canvas.document.edit_uv(UvAction::Redo);
      }
      return;
    }
    if (canvas.sculpting) {
      canvas.document.end_desktop();
      canvas.sculpting = false;
    }
    if (label == "Refresh recovery") {
      canvas.document.refresh_recovery();
    } else if (label == "Recover project...") {
      if (recover_project(canvas.document)) project.clear();
    } else if (label == "New") {
      if (discard()) {
        canvas.seed();
        project.clear();
        dirty = false;
      }
    } else if (label == "Import mesh...") {
      auto path = fl_file_chooser("Import static mesh", "*.{obj,gltf,glb}", nullptr);
      if (path) canvas.document.import_asset(path);
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
      canvas.radius = std::max(.005f, canvas.radius * .8f);
    else if (label == "Larger brush")
      canvas.radius = std::min(2.f, canvas.radius * 1.25f);
    else if (label == "Create humanoid...") {
      create_humanoid_dialog(canvas.document);
    } else if (label == "New sphere" || label == "New empty sculpt") {
      canvas.document.add_object(label == "New sphere");
    } else if (label == "Duplicate object") {
      canvas.document.duplicate_object();
    } else if (label == "Delete object") {
      canvas.document.delete_object();
    } else if (label == "Skin weights...") {
      edit_weights_dialog(canvas.document);
    } else if (label == "Rig and shapekeys...") {
      edit_rig_dialog(canvas.document);
    } else if(label=="Bake surface..."){edit_bake_dialog(canvas.document);
    } else if(label=="Clear baked surface"){canvas.document.clear_bake();
    } else if(label=="Cancel background work"){canvas.document.cancel_asset_work();
    } else if (label == "Voxel remesh...") {
      edit_voxel_remesh(canvas.document);
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
    } else if (label == "UV editor...") {
      edit_uv(canvas.document);
    } else if (label == "Prepare painting mesh") {
      canvas.document.prepare_mesh();
    } else if (label == "Use painted mesh") {
      canvas.document.use_mesh();
    } else if (label == "Edit source volume") {
      canvas.document.use_source();
    } else if (label == "Brush settings...") {
      edit_paint(canvas.document);
    } else if (label == "Sculpt layer properties...") {
      edit_sculpt_layer(canvas.document);
    } else if (label == "Add sculpt layer") {
      canvas.document.sculpt_command(SpatialCommand::SculptAdd);
    } else if (label == "Next sculpt layer") {
      canvas.document.sculpt_command(SpatialCommand::SculptNext);
    } else if (label == "Previous sculpt layer") {
      canvas.document.sculpt_command(SpatialCommand::SculptPrevious);
    } else if (label == "Sculpt layer visible") {
      canvas.document.sculpt_command(SpatialCommand::SculptVisible);
    } else if (label == "Sculpt layer locked") {
      canvas.document.sculpt_command(SpatialCommand::SculptLock);
    } else if (label == "Cycle sculpt strength") {
      canvas.document.sculpt_command(SpatialCommand::SculptOpacity);
    } else if (label == "Delete sculpt layer") {
      canvas.document.sculpt_command(SpatialCommand::SculptDelete);
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
      canvas.document.prepare_mesh();
    } else if (label == "Export glTF (.glb)...") {
      auto chosen =
          fl_file_chooser("Export glTF scene", "*.glb", "Sculpt.glb");
      if (chosen) {
        canvas.document.export_asset(chosen);
      }
    } else if (label == "Export selected avatar (.vrm)...") {
      auto chosen = fl_file_chooser("Export selected avatar", "*.vrm", "Avatar.vrm");
      if (chosen) canvas.document.export_asset(chosen, AssetExportFormat::Avatar);
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
    StandaloneXrSignals termination;
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
    document.enable_recovery();
    document.refresh_recovery();
    SessionAppearanceMonitor appearance(document);
    if (paint) {
      document.prepare_mesh(256);
      if (!document.wait_asset() || !document.frame().state->mesh_active)
        throw std::runtime_error(document.frame().state->error);
    }
    document.wait_mesh();
    {
      XrSculpt xr(document, &standalone_xr_stop, overlay);
      xr.run(frames);
    }
    if (!save.empty()) {
      if (!document.wait_asset(std::chrono::seconds(180)))
        throw std::runtime_error(
            "Pending mesh/UV work did not finish before saving");
      document.save(save);
    }
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
    uint64_t uv_hash = 1469598103934665603ULL;
    if (scene.active().painted)
      for (auto uv : scene.active().painted->mesh().uv)
        for (float f : uv) {
          auto word = std::bit_cast<uint32_t>(f);
          for (int i = 0; i < 4; ++i) {
            uv_hash ^= uint8_t(word >> (i * 8));
            uv_hash *= 1099511628211ULL;
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
    auto sculpt_layers = nlohmann::json::array();
    for (auto &l : volume.layers())
      sculpt_layers.push_back({{"id", l.id},
                               {"name", l.name},
                               {"visible", l.visible},
                               {"locked", l.locked},
                               {"opacity", l.opacity},
                               {"samples", l.delta.size()}});
    auto encoded = scene.encode();
    auto digest = [](const nlohmann::json &value) {
      uint64_t result = 1469598103934665603ULL;
      for (auto byte : nlohmann::json::to_cbor(value)) {
        result ^= byte;
        result *= 1099511628211ULL;
      }
      return result;
    };
    auto objects = nlohmann::json::array();
    for (auto value : encoded.at("objects")) {
      bool visible = value.at("visible");
      value.erase("visible");
      objects.push_back({{"id", value.at("id")}, {"name", value.at("name")},
                         {"visible", visible}, {"mesh_active", value.at("mesh_active")},
                         {"baked",value.contains("painted") && value.at("painted").contains("baked")},
                         {"voxel_size", value.at("volume").at("voxel_size")},
                         {"payload_hash", digest(value)}});
    }
    std::cout << nlohmann::json{{"sculpt_layers", sculpt_layers},
                                {"objects", objects},
                                {"scene_hash", digest(encoded)},
                                {"active_sculpt_layer", volume.active_layer()},
                                {"paint_blue_texels", blue_texels},
                                {"uv_hash", uv_hash},
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
  Fl_Window::default_xclass("org.domed.Sculpt");
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
