// SPDX-License-Identifier: GPL-3.0-or-later
#include "appearance.h"
#include "desktop-menu.h"
#include "dome/document.hpp"
#include "export-editor.hpp"
#include "editable-avatar-eligibility.hpp"
#include "dome/frame-clock.hpp"
#include "desktop-framing.hpp"
#include "spacenav-options.hpp"
#include "native-fontconfig.hpp"
#include "native-display-runtime.hpp"
#include "dome/mesh.hpp"
#include "dome/render.hpp"
#include "dome/session-appearance.hpp"
#include "object-editor.hpp"
#include "creator-editor.hpp"
#include "rig-editor.hpp"
#include "rig-fit-editor.hpp"
#include "shapekey-editor.hpp"
#include "vrm-expression-editor.hpp"
#include "weight-editor.hpp"
#include "skin-editor.hpp"
#include "sculpt-editor.hpp"
#include "resolution-editor.hpp"
#include "procedural-editor.hpp"
#include "import-editor.hpp"
#include "paint-editor.hpp"
#include "texture-set-editor.hpp"
#include "projection-editor.hpp"
#include "mesh-editor.hpp"
#include "bake-editor.hpp"
#include "ui-conventions.hpp"
#include "uv-editor.hpp"
#include "polygon-editor.hpp"
#include "polygon-view-render.hpp"
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
  std::unique_ptr<CursorRenderer> cursors;
  std::unique_ptr<PolygonRenderer> polygons;
  std::shared_ptr<const MeshSnapshot> shown_mesh;
  unsigned shown_preview_flags=0;
  Quat rotation{};
  Vec3 pan{}, orbit_pivot{};
  float zoom_limit=20.f;
  uint64_t framed_generation=0,frame_import_generation=0;
  size_t frame_import_objects=0;
  bool frame_import_pending=false;
  Vec3 ball{}, last_stamp{};
  float distance = 2.2f, radius = .12f;
  double material_time = 0;
  Tool tool = Tool::Clay;
  bool sculpting = false, have_stamp = false, dirty_mesh = true;
  int last_x = 0, last_y = 0;
  std::string capture;
  std::function<void()> changed,view_boundary;
  bool view_focused=false,mouse_navigation=false;
  Canvas(int x, int y, int w, int h) : Fl_Gl_Window(x, y, w, h) {
    mode(FL_RGB | FL_DOUBLE | FL_DEPTH | FL_OPENGL3);
    end();
    document.enable_recovery();
    document.refresh_recovery();
  }
  static void finish_capture(void *data) {
    auto &canvas=*static_cast<Canvas*>(data);
    if(auto window=canvas.window())window->hide();
  }
  void release_graphics() {
    if (!renderer && !polygons && !cursors) return;
    // A parent hide recursively calls this subwindow's virtual hide() before
    // FLTK destroys its GL context/native Wayland surface. Never recreate a
    // context from the destructor of an already hidden native window.
    if (shown() && context() && context_valid()) make_current();
    else {
      // Reuse the renderer's existing context-loss contract: do not delete
      // stale GL names against a different or already destroyed context.
      if(renderer)renderer->abandon();
      if(cursors)cursors->abandon();
      if(polygons)polygons->abandon();
    }
    cursors.reset();
    polygons.reset();
    renderer.reset();
  }
  void hide() override {
    release_graphics();
    Fl_Gl_Window::hide();
  }
  ~Canvas() {
    Fl::remove_timeout(finish_capture,this);
    release_graphics();
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
  bool frame_view(bool selected,const EditorSnapshot &f) {
    if(f.state->stroking||f.state->native_pending||f.state->object_grabbing)return false;
    auto result=detail::desktop_framing(f,rotation,float(w())/std::max(1,h()),selected);
    if(!result)return false;
    pan=result->pan;orbit_pivot=result->pivot;distance=result->distance;zoom_limit=std::max(20.f,distance*4);
    if(view_boundary)view_boundary();
    framed_generation=f.state->generation;redraw();return true;
  }
  void frame_view(bool selected){frame_view(selected,document.frame());}
  void update_mesh() {
    auto f = document.frame();
    if (f.surface != shown_mesh) {
      shown_mesh = f.surface;
      dirty_mesh = false;
    }
    if(frame_import_pending&&f.state->generation!=frame_import_generation)frame_import_pending=false;
    const bool settled=!f.state->asset_busy&&!f.state->native_pending&&!f.state->stroking&&!f.state->object_grabbing&&f.surface&&
        f.surface->revision==f.state->revision&&f.surface->generation==f.state->generation;
    if(settled){
      const bool imported=frame_import_pending&&f.state->objects.size()>frame_import_objects;
      const bool first_source=framed_generation!=f.state->generation&&
          std::any_of(f.surface->ranges.begin(),f.surface->ranges.end(),[](const auto &r){return bool(r.appearance);});
      if(imported||first_source)frame_view(false,f);
      if(frame_import_pending)frame_import_pending=false;
    }
  }
  Vec3 project_ball(int x, int y) {
    float size = float(std::max(1, std::min(w(), h())));
    return trackball((2 * (x - this->x()) - w()) / size,
                     (h() - 2 * (y - this->y())) / size);
  }
  bool native_stroke=false, suppress_drag=false, object_drag=false,polygon_drag=false,drag_affine=false;
  ProjectionCamera projection_camera() const {
    ProjectionCamera camera;camera.mode=ProjectionMode::Perspective;
    const auto inverse=rotation.inverse();const auto origin=inverse.rotate(Vec3{0,0,distance}-pan);
    camera.pose.position={origin.x,origin.y,origin.z};camera.pose.orientation={inverse.x,inverse.y,inverse.z,inverse.w};
    camera.vertical_fov=.7853981634f;camera.aspect=float(w())/std::max(1,h());
    camera.near_depth=std::clamp(distance*.001f,.00001f,.01f);camera.far_depth=100;
    return camera;
  }
  void polygon_at(int x,int y,PolygonAction action) {
    float aspect=float(w())/std::max(1,h()),tangent=std::tan(22.5f*3.14159265f/180);
    Vec3 direction{(2.f*(x-this->x())/w()-1)*aspect*tangent,(1-2.f*(y-this->y())/h())*tangent,-1};
    auto origin=rotation.inverse().rotate(Vec3{0,0,distance}-pan);
    direction=rotation.inverse().rotate(normalized(direction));
    document.polygon_pointer(origin,direction,action,Fl::event_state(FL_CTRL));invalidate();
  }
  CanvasTransform drag_local{},drag_parent{},drag_world_delta{};
  void sculpt_at(int x, int y) {
    if (!document.frame().state->stroking) { sculpting = have_stamp = false; return; }
    float aspect = float(w()) / std::max(1, h()),
          tangent = std::tan(22.5f * 3.14159265f / 180);
    Vec3 direction{(2.f * (x - this->x()) / w() - 1) * aspect * tangent,
                   (1 - 2.f * (y - this->y()) / h()) * tangent, -1};
    Vec3 origin = rotation.inverse().rotate(Vec3{0, 0, distance} - pan);
    direction = rotation.inverse().rotate(normalized(direction));
    auto state = document.frame().state;
    if (native_stroke) {
      NativeStrokeSample sample;sample.origin=origin;sample.direction=direction;sample.material_time=material_time;
      sample.brush.radius=radius;sample.brush.tool=tool;
      sample.brush.strength=!state->sculpt_settings.mask_edit&&tool==Tool::Smooth?.55f:1;
      sample.brush.blend=std::min(.025f,radius*.25f);
      if(!document.sample_native_stroke(sample)){sculpting=have_stamp=false;}
      invalidate();return;
    }
    if (state->weights.enabled && (!shown_mesh || shown_mesh->generation != state->generation ||
        shown_mesh->revision != state->revision)) { have_stamp = false; return; }
    std::optional<Vec3> hit;
    try {hit=document.raycast(origin,direction,material_time);}
    catch(const std::length_error &error) {
      document.cancel_desktop();sculpting=have_stamp=false;
      document.report_error(error.what());invalidate();return;
    }
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
    brush.strength = !document.frame().state->sculpt_settings.mask_edit && tool == Tool::Smooth ? .55f : 1;
    brush.blend = std::min(.025f, radius * .25f);
    document.stamp_desktop(brush);
    last_stamp = *hit;
    have_stamp = true;
    invalidate();
  }
  int handle(int event) override {
    try {
      if (event == FL_FOCUS) {
        view_focused=true;if(view_boundary)view_boundary();return 1;
      }
      if(event==FL_KEYDOWN&&Fl::event_key()==FL_Escape&&object_drag){document.end_object_grab(false);object_drag=false;suppress_drag=true;invalidate();return 1;}
      if (event == FL_KEYDOWN && Fl::event_key() == FL_Escape &&
          document.frame().state->menu) {
        document.cancel_spatial_menu();
        return 1;
      }
      if(event==FL_KEYDOWN&&Fl::event_key()==FL_Escape&&document.frame().state->polygon){
        if(polygon_drag){polygon_at(Fl::event_x(),Fl::event_y(),PolygonAction::CancelDrag);polygon_drag=false;suppress_drag=true;}
        else document.end_polygon(false);invalidate();return 1;
      }
      if (event == FL_KEYDOWN && Fl::event_key() == FL_Escape && !sculpting &&
          document.frame().state->retopology) {
        document.end_retopology(false); invalidate(); return 1;
      }
      if(event==FL_KEYDOWN&&Fl::event_key()==FL_Escape&&document.frame().state->cut){
        if(document.frame().state->cut->committing)document.cancel_asset_work();else document.end_cut(false);
        invalidate();return 1;
      }
      if (event == FL_PUSH) {
        mouse_navigation=true;if(view_boundary)view_boundary();
        suppress_drag=false;
        take_focus();view_focused=Fl::focus()==this;
        last_x = Fl::event_x();
        last_y = Fl::event_y();
        ball = project_ball(last_x, last_y);
        sculpting =
            Fl::event_button() == FL_LEFT_MOUSE && Fl::event_state(FL_SHIFT);
        have_stamp = false;
        if(Fl::event_button()==FL_LEFT_MOUSE&&document.frame().state->texture_face_select){
          sculpting=false;float aspect=float(w())/std::max(1,h()),tangent=std::tan(22.5f*3.14159265f/180);
          Vec3 direction{(2.f*(last_x-this->x())/w()-1)*aspect*tangent,(1-2.f*(last_y-this->y())/h())*tangent,-1};
          auto origin=rotation.inverse().rotate(Vec3{0,0,distance}-pan);direction=rotation.inverse().rotate(normalized(direction));
          document.texture_face_pointer(origin,direction,Fl::event_state(FL_CTRL),material_time);suppress_drag=true;invalidate();return 1;
        }
        if(sculpting&&document.frame().state->polygon){
          sculpting=false;auto state=document.frame().state;
          if(!state->polygon->surface||state->polygon->busy||state->polygon->committing){suppress_drag=true;return 1;}
          polygon_drag=state->polygon_controls.move_mode;
          polygon_at(last_x,last_y,polygon_drag?PolygonAction::BeginDrag:PolygonAction::Pick);return 1;
        }
        if(!sculpting && Fl::event_button()==FL_LEFT_MOUSE && document.frame().state->object_grab_mode){
          float aspect=float(w())/std::max(1,h()),tangent=std::tan(22.5f*3.14159265f/180);
          Vec3 direction{(2.f*(last_x-this->x())/w()-1)*aspect*tangent,(1-2.f*(last_y-this->y())/h())*tangent,-1};
          auto origin=rotation.inverse().rotate(Vec3{0,0,distance}-pan);direction=rotation.inverse().rotate(normalized(direction));
          auto picked=document.pick_object(origin,direction,material_time);
          if(picked && document.begin_object_grab(*picked)){object_drag=true;auto scene=document.copy_scene();drag_local=scene.object(*picked).transform;drag_affine=scene.has_affine_placement(*picked);drag_world_delta={};if(!drag_affine)drag_parent=ob::compose(scene.world_transform(*picked),ob::inverse(drag_local));}
          else suppress_drag=true;
          return 1;
        }
        if (sculpting) {
          auto state=document.frame().state;
          native_stroke=!state->weights.enabled&&!state->retopology;
          if (!(native_stroke?document.begin_native_stroke():document.begin_desktop())) {
            sculpting = false;suppress_drag=true;
            return 1;
          }
          sculpt_at(last_x, last_y);
        }
        return 1;
      }
      if (event == FL_DRAG) {
        if(suppress_drag)return 1;
        int x = Fl::event_x(), y = Fl::event_y();
        if(polygon_drag){polygon_at(x,y,PolygonAction::Drag);}
        else if(object_drag){
          if(!document.frame().state->object_grabbing){object_drag=false;suppress_drag=true;return 1;}
          auto delta=rotation.inverse().rotate(Vec3{(x-last_x)*distance*.0015f,-(y-last_y)*distance*.0015f,0});
          if(drag_affine){drag_world_delta.position=drag_world_delta.position+ob::Vec3{delta.x,delta.y,delta.z};document.move_held_object_world(drag_world_delta);}
          else{auto local=ob::rotate(ob::conjugate(drag_parent.orientation),{delta.x,delta.y,delta.z})*(1.f/drag_parent.scale);
            drag_local.position=drag_local.position+local;document.move_held_object(drag_local);}invalidate();
        }else if (sculpting)
          sculpt_at(x, y);
        else if (Fl::event_state(FL_BUTTON1)) {
          auto next = project_ball(x, y);
          auto next_rotation=(arcball(ball,next)*rotation).unit();
          pan=detail::orbit_pan(pan,orbit_pivot,rotation,next_rotation);rotation=next_rotation;
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
        mouse_navigation=Fl::event_state(FL_BUTTONS)!=0;
        suppress_drag=false;
        if(polygon_drag){polygon_at(Fl::event_x(),Fl::event_y(),PolygonAction::EndDrag);polygon_drag=false;return 1;}
        if(object_drag){document.end_object_grab(true);object_drag=false;invalidate();return 1;}
        if (sculpting) {
          document.end_desktop();
          sculpting = false;
          have_stamp = false;
          invalidate();
        }
        return 1;
      }
      if (event == FL_UNFOCUS) {
        view_focused=false;mouse_navigation=false;if(view_boundary)view_boundary();
        if(polygon_drag){polygon_at(Fl::event_x(),Fl::event_y(),PolygonAction::CancelDrag);polygon_drag=false;}
        if(object_drag){document.end_object_grab(false);object_drag=false;invalidate();}
        if (sculpting) {
          document.cancel_desktop();
          sculpting = false;
          have_stamp = false;
          invalidate();
        }
        return 1;
      }
      if (event == FL_MOUSEWHEEL) {
        if(view_boundary)view_boundary();
        distance =
            std::clamp(distance * std::exp(.1f * Fl::event_dy()), .01f, zoom_limit);
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
      if(polygon_drag){try{polygon_at(Fl::event_x(),Fl::event_y(),PolygonAction::CancelDrag);}catch(...){}polygon_drag=false;}
      if(object_drag){try{document.end_object_grab(false);}catch(...){}object_drag=false;}
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
      if (cursors) { cursors->abandon(); cursors.reset(); }
      if(polygons){polygons->abandon();polygons.reset();}
    }
    if (!renderer){renderer = std::make_unique<SurfaceRenderer>();renderer->set_deferred_resolve(true);}
    if(!polygons)polygons=std::make_unique<PolygonRenderer>();
    CanvasTransform model{{pan.x, pan.y, pan.z},
                          {rotation.x, rotation.y, rotation.z, rotation.w},
                          1};
    ob::Vec3 eye{0, 0, distance};
    float up = .3926990817f, right = std::atan(std::tan(up) * float(pixel_w()) /
                                               std::max(1, pixel_h()));
    auto vp =
        multiply(projection(-right,right,-up,up,std::clamp(distance*.001f,.00001f,.01f),std::max(100.f,distance*4)),view_matrix(eye,{}));
    auto f = document.frame();
    material_time = render_frame_time();
    renderer->set_frame_time(material_time);
    auto polygon_preview=detail::polygon_preview(f);
    renderer->draw(f.surface, model, vp, eye, pixel_w(), pixel_h(), false,
                   f.state->toon, f.state->light,f.state->objects,polygon_preview.object);
    detail::draw_polygon_preview(*polygons,polygon_preview,model,vp,f.state->accent);
    if (f.state->retopology || f.state->cut || f.state->rig_fit || (f.state->projection&&f.state->projection->settings.preview)) {
      if (!cursors){cursors = std::make_unique<CursorRenderer>();cursors->set_linear_output(true);}
      auto overlay = *f.state; overlay.canvas = model; overlay.hands = {};
      cursors->draw(overlay, vp);
    }
    renderer->resolve();
    shown_preview_flags=(f.state->retopology?1u:0u)|(f.state->cut?2u:0u)|
        ((f.state->polygon||f.state->polygon_handoff)?4u:0u)|(f.state->object_grabbing?8u:0u);
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
        throw std::runtime_error("Cannot write capture");
      }
      capture.clear();
      // Return through main so native workers, GL and process font ownership
      // are released, just as on an ordinary successful window close.
      Fl::add_timeout(.1,finish_capture,this);
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
  uint64_t seen_revision = 0, font_revision = UINT64_MAX,seen_object_authority=0;
  std::string seen_error;
  std::deque<std::string> native_commands;
  std::optional<std::future<void>> saving;
  desktop::SpaceNavigator spacenav;
  uint64_t spacenav_generation=UINT64_MAX,spacenav_authority=UINT64_MAX;
  ~App() {
    Fl::remove_timeout(tick, this);
    Fl::remove_timeout(space_tick,this);canvas.view_boundary={};
    stop_xr = true;
    if (xr_worker.joinable())
      xr_worker.join();
  }
  static void space_tick(void *data) {
    auto &a=*static_cast<App*>(data);const double now=desktop::spacenav_now();
    try {
      const auto state=a.canvas.document.frame().state;
      if(a.spacenav_generation!=state->generation||a.spacenav_authority!=state->edit_authority) {
        a.spacenav.fence(now);a.spacenav_generation=state->generation;a.spacenav_authority=state->edit_authority;
      }
      const bool owns=a.window.shown()&&a.window.visible_r()&&a.canvas.view_focused&&
        Fl::focus()==&a.canvas&&!Fl::modal()&&!Fl::grab()&&!a.canvas.mouse_navigation&&
        !Fl::event_state(FL_BUTTONS)&&
        !a.canvas.sculpting&&!a.canvas.object_drag&&!a.canvas.polygon_drag&&
        !state->stroking&&!state->native_pending&&!state->object_grabbing&&!state->menu&&
        state->controls.parameter==VrParameter::None;
      desktop::SpaceView view{a.canvas.rotation,a.canvas.pan,a.canvas.orbit_pivot,a.canvas.distance,a.canvas.zoom_limit};
      auto update=a.spacenav.tick(view,owns,now);
      if(update.action!=desktop::SpaceUpdate::Action::None)
        a.canvas.frame_view(update.action==desktop::SpaceUpdate::Action::FrameSelection);
      else if(update.moved) {
        a.canvas.rotation=view.rotation;a.canvas.pan=view.pan;a.canvas.distance=view.distance;
        a.canvas.redraw();
      }
    } catch(...) {a.spacenav.fence(now);}
    Fl::repeat_timeout(.008,space_tick,data);
  }
  static void tick(void *data) {
    auto &a = *static_cast<App *>(data);
    auto f = a.canvas.document.frame();
    const unsigned preview_flags=(f.state->retopology?1u:0u)|(f.state->cut?2u:0u)|
        ((f.state->polygon||f.state->polygon_handoff)?4u:0u)|(f.state->object_grabbing?8u:0u);
    if (f.surface != a.canvas.shown_mesh || preview_flags || preview_flags!=a.canvas.shown_preview_flags || a.seen_object_authority!=f.state->edit_authority)
      a.canvas.redraw();
    a.seen_object_authority=f.state->edit_authority;
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
    if(a.saving&&a.saving->wait_for(std::chrono::seconds(0))==std::future_status::ready){
      try{a.saving->get();}catch(const std::exception &error){a.canvas.document.report_error(error.what());}a.saving.reset();
    }
    auto native_state=a.canvas.document.frame().state;
    if(!native_state->native_pending&&!(native_state->polygon&&native_state->polygon->committing)&&!(native_state->cut&&native_state->cut->committing)&&!native_state->error.empty()&&!a.native_commands.empty()) {
      a.native_commands.clear();a.canvas.document.report_error(native_state->error+" | Pending commands cancelled; original project retained");
    }
    if(!native_state->native_pending&&!(native_state->polygon&&native_state->polygon->committing)&&!(native_state->cut&&native_state->cut->committing)&&!a.saving&&!a.native_commands.empty()) {
      auto command=std::move(a.native_commands.front());a.native_commands.pop_front();
      try{a.action(std::move(command));}catch(const std::exception &error){a.canvas.document.report_error(error.what());}
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
    int work_x=0,work_y=0,work_w=0,work_h=0;
    Fl::screen_work_area(work_x,work_y,work_w,work_h);
    const int initial_w=std::min(window.w(),std::max(1,work_w-40));
    const int initial_h=std::min(window.h(),std::max(1,work_h-40));
    window.size_range(std::min(640,initial_w),std::min(480,initial_h));
    window.size(initial_w,initial_h);
    window.xclass("org.domed.Sculpt");
    status.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    menu.add("&File/&New", FL_CTRL + 'n', command, this);
    menu.add("&File/&Open...", FL_CTRL + 'o', command, this);
    menu.add("&File/Import &mesh...", 0, command, this);
    menu.add("&File/&Save", FL_CTRL + 's', command, this);
    menu.add("&File/Save &As...", FL_CTRL | FL_SHIFT | 's', command, this);
    menu.add("&File/Export &glTF (.glb)...", FL_CTRL + 'e', command, this);
    menu.add("&File/Export editable project (.glb)...", 0, command, this);
    menu.add("&File/Export editable avatar (.vrm)...", 0, command, this);
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
    menu.add("&View/Frame &all", FL_Home, command, this);
    menu.add("&View/Frame &selection", FL_SHIFT | 'f', command, this);
    menu.add("&View/Reset &trackball", 'f', command, this);
    menu.add("&View/3D mouse navigation",0,command,this,FL_MENU_TOGGLE|FL_MENU_VALUE);
    menu.add("&View/3D mouse options...",0,command,this);
    menu.add("&View/Start or stop &XR", FL_F + 6, command, this);
    menu.add("&View/Shading/&Authored material", 0, command, this,
             FL_MENU_RADIO | FL_MENU_VALUE);
    menu.add("&View/Shading/&PBR", 0, command, this, FL_MENU_RADIO);
    menu.add("&View/Shading/&Toon preview", 0, command, this, FL_MENU_RADIO);
    menu.add("&View/Expressions/Start preview",0,command,this);
    menu.add("&View/Expressions/Choose expression...",0,command,this);
    menu.add("&View/Expressions/Weight...",0,command,this);
    menu.add("&View/Expressions/Weight -",0,command,this);
    menu.add("&View/Expressions/Weight +",0,command,this);
    menu.add("&View/Expressions/Reset weights",0,command,this);
    menu.add("&View/Expressions/Apply static pose",0,command,this);
    menu.add("&View/Expressions/Cancel preview",0,command,this);
    menu.add("&Sculpt/&Clay", '1', command, this,
             FL_MENU_RADIO | FL_MENU_VALUE);
    menu.add("&Sculpt/Ca&rve", '2', command, this, FL_MENU_RADIO);
    menu.add("&Sculpt/&Smooth", '3', command, this, FL_MENU_RADIO);
    menu.add("&Sculpt/&Flatten", '4', command, this, FL_MENU_RADIO);
    menu.add("&Sculpt/Masks and symmetry...", 0, command, this);
    menu.add("&Sculpt/Paint protection mask", 0, command, this, FL_MENU_TOGGLE);
    menu.add("&Sculpt/Resolution...",0,command,this);
    menu.add("&Sculpt/Procedural detail...",0,command,this);
    menu.add("&Sculpt/Protect all", 0, command, this);
    menu.add("&Sculpt/Clear protection mask", 0, command, this);
    menu.add("&Sculpt/Invert protection mask", 0, command, this);
    menu.add("&Sculpt/Show protection mask", 0, command, this, FL_MENU_TOGGLE);
    menu.add("&Sculpt/Mirror X", 0, command, this, FL_MENU_TOGGLE);
    menu.add("&Sculpt/Mirror Y", 0, command, this, FL_MENU_TOGGLE);
    menu.add("&Sculpt/Mirror Z", 0, command, this, FL_MENU_TOGGLE);
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
    menu.add("&Scene/VR selection mode",0,command,this,FL_MENU_TOGGLE);
    menu.add("&Scene/XR object hierarchy",0,command,this,FL_MENU_TOGGLE);
    menu.add("&Scene/Focus selected objects",0,command,this,FL_MENU_TOGGLE);
    menu.add("&Scene/Object grab mode",0,command,this,FL_MENU_TOGGLE);
    menu.add("&Scene/Hold selected object",0,command,this);
    menu.add("&Scene/Release held object",0,command,this);
    menu.add("&Scene/Ghost other objects",0,command,this,FL_MENU_TOGGLE);
    menu.add("&Scene/Objects visibility...",0,command,this);
    menu.add("&Scene/Object and material...", FL_ALT + FL_Enter, command, this);
    menu.add("&Scene/Skin weights...", 0, command, this);
    menu.add("&Scene/Bind or transfer skin...", 0, command, this);
    menu.add("&Scene/Rig and shapekeys...", 0, command, this);
    menu.add("&Scene/Shapekeys/New shapekey...",0,command,this);
    menu.add("&Scene/Shapekeys/Capture shape mix...",0,command,this);
    menu.add("&Scene/Shapekeys/Duplicate shapekey...",0,command,this);
    menu.add("&Scene/Shapekeys/Mirror shapekey...",0,command,this);
    menu.add("&Scene/Shapekeys/Rename shapekey...",0,command,this);
    menu.add("&Scene/Shapekeys/Choose shapekey...",0,command,this);
    menu.add("&Scene/Shapekeys/Sculpt selected shapekey",0,command,this,FL_MENU_TOGGLE);
    menu.add("&Scene/Shapekeys/Restore before shape authoring...",0,command,this);

    menu.add("&Mesh/Voxel remesh...", 0, command, this);
    menu.add("&Mesh/Isotropic remesh...",0,command,this);
    menu.add("&Mesh/Bake surface...",0,command,this);
    menu.add("&Mesh/Clear baked surface",0,command,this);
    menu.add("&Mesh/Convert to portable normals",0,command,this);
    menu.add("&File/Cancel background work",0,command,this);
    menu.add("&Mesh/UV editor...", FL_ALT + 'u', command, this);
    menu.add("&Mesh/Polygon edit/Start polygon editing", 0, command, this);
    menu.add("&Mesh/Polygon edit/Vertices", 0, command, this, FL_MENU_RADIO);
    menu.add("&Mesh/Polygon edit/Edges", 0, command, this, FL_MENU_RADIO);
    menu.add("&Mesh/Polygon edit/Faces", 0, command, this, FL_MENU_RADIO);
    menu.add("&Mesh/Polygon edit/Move components", 0, command, this, FL_MENU_TOGGLE);
    menu.add("&Mesh/Polygon edit/Add to selection", 0, command, this, FL_MENU_TOGGLE);
    menu.add("&Mesh/Polygon edit/Select all components", 0, command, this);
    menu.add("&Mesh/Polygon edit/Clear component selection", 0, command, this);
    menu.add("&Mesh/Polygon edit/Extrude faces", 0, command, this);
    menu.add("&Mesh/Polygon edit/Inset faces", 0, command, this);
    menu.add("&Mesh/Polygon edit/Bridge loops", 0, command, this);
    menu.add("&Mesh/Polygon edit/Dissolve edges", 0, command, this);
    menu.add("&Mesh/Polygon edit/Weld vertices", 0, command, this);
    menu.add("&Mesh/Polygon edit/Subdivide edges", 0, command, this);
    menu.add("&Mesh/Polygon edit/Connect vertices", 0, command, this);
    menu.add("&Mesh/Polygon edit/Delete faces", 0, command, this);
    menu.add("&Mesh/Polygon edit/Flip faces", 0, command, this);
    menu.add("&Mesh/Polygon edit/Polygon controls...", 0, command, this);
    menu.add("&Mesh/Polygon edit/Apply polygon edit", 0, command, this);
    menu.add("&Mesh/Polygon edit/Cancel polygon edit", 0, command, this);
    menu.add("&Mesh/Polygon edit/Restore polygon source", 0, command, this);
    menu.add("&Mesh/Retopology/Start patch", 0, command, this);
    menu.add("&Mesh/Retopology/Place points", 0, command, this, FL_MENU_RADIO | FL_MENU_VALUE);
    menu.add("&Mesh/Retopology/Move points", 0, command, this, FL_MENU_RADIO);
    menu.add("&Mesh/Retopology/Extrude edge", 0, command, this, FL_MENU_RADIO);
    menu.add("&Mesh/Retopology/Make face", 0, command, this);
    menu.add("&Mesh/Retopology/Connect edge", 0, command, this);
    menu.add("&Mesh/Retopology/Bridge edges", 0, command, this);
    menu.add("&Mesh/Retopology/Delete points", 0, command, this);
    menu.add("&Mesh/Retopology/Pin points", 0, command, this);
    menu.add("&Mesh/Retopology/Seam edge", 0, command, this);
    menu.add("&Mesh/Retopology/Relax patch", 0, command, this);
    menu.add("&Mesh/Retopology/Snap points", 0, command, this);
    menu.add("&Mesh/Retopology/Clear point selection", 0, command, this);
    menu.add("&Mesh/Retopology/Projection distance...", 0, command, this);
    menu.add("&Mesh/Retopology/Apply patch", 0, command, this);
    menu.add("&Mesh/Retopology/Cancel patch", 0, command, this);
    menu.add("&Mesh/Plane cut/Start cut",0,command,this);
    menu.add("&Mesh/Plane cut/Offset...",0,command,this);
    menu.add("&Mesh/Plane cut/Direction...",0,command,this);
    menu.add("&Mesh/Plane cut/Cap color...",0,command,this);
    menu.add("&Mesh/Plane cut/Flip kept side",0,command,this,FL_MENU_TOGGLE);
    menu.add("&Mesh/Plane cut/Cap cut",0,command,this,FL_MENU_TOGGLE|FL_MENU_VALUE);
    menu.add("&Mesh/Plane cut/Apply cut",0,command,this);
    menu.add("&Mesh/Plane cut/Cancel cut",0,command,this);
    menu.add("&Paint/Prepare painting mesh", FL_CTRL | FL_SHIFT | 'p', command,
             this);
    menu.add("&Paint/Use painted mesh", 'p', command, this, FL_MENU_RADIO);
    menu.add("&Paint/Edit source volume", FL_SHIFT | 'p', command, this,
             FL_MENU_RADIO | FL_MENU_VALUE);
    menu.add("&Paint/Humanoid rig fitting...",0,command,this);
    menu.add("&Paint/Image projection/Load projection image...",0,command,this);
    menu.add("&Paint/Image projection/Image projection options...",0,command,this);
    menu.add("&Paint/Image projection/Use projection image",0,command,this,FL_MENU_TOGGLE);
    menu.add("&Paint/Image projection/Projection from view",0,command,this);
    menu.add("&Paint/Image projection/Cancel image load",0,command,this);
    menu.add("&Paint/Texture sets...",0,command,this);
    menu.add("&Paint/Pick material faces",0,command,this,FL_MENU_TOGGLE);
    menu.add("&Paint/Clear material face selection",0,command,this);
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
          app.action("Close Window");
        },
        this);
    universal = std::make_unique<DesktopMenu>(window, menu);
    update_status();
    canvas.view_boundary=[this]{spacenav.fence(desktop::spacenav_now());};
    Fl::add_timeout(.008,space_tick,this);
    Fl::add_timeout(.03, tick, this);
  }
  bool discard() {
    dirty = canvas.document.frame().state->dirty;
    return !dirty || confirm_discard(window);
  }
  void sync_menus() {
    auto frame=canvas.document.frame();auto s=frame.state;
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
    enable("&Mesh/Isotropic remesh...",!modal&&s->can_retopologize&&s->objects.size()<256);
    enable("&Mesh/Bake surface...",!modal && s->can_bake);
    enable("&Mesh/Clear baked surface",!modal && s->can_clear_bake);
    enable("&Mesh/Convert to portable normals",!modal && s->can_convert_bake);
    enable("&File/Cancel background work",!modal && (s->can_cancel_asset||s->native_pending));
    enable("&File/Close &Window", true);
    const char *views[] = {"&View/Frame &all", "&View/Frame &selection", "&View/Reset &trackball", "&View/Start or stop &XR",
                           "&View/Shading/&Authored material", "&View/Shading/&PBR", "&View/Shading/&Toon preview",
                           "&View/3D mouse navigation","&View/3D mouse options..."};
    for (auto path : views)
      enable(path, !modal);
    for(auto path:{"&View/Shading/&Authored material","&View/Shading/&PBR","&View/Shading/&Toon preview"})
      enable(path,!modal&&s->can_preview);
    const bool can_frame=!modal&&!s->stroking&&!s->native_pending&&!s->object_grabbing;
    enable("&View/Frame &all",can_frame&&detail::desktop_frameable(frame,false));
    enable("&View/Frame &selection",can_frame&&detail::desktop_frameable(frame,true));
    enable("&File/Export editable avatar (.vrm)...",!modal&&!s->asset_busy&&detail::editable_avatar_view(frame));
    const char *document_actions[] = {"&File/&New",
                                      "&File/&Open...",
                                      "&File/&Save",
                                      "&File/Save &As...",
                                      "&File/Export &glTF (.glb)...",
                                      "&File/Export editable project (.glb)...",
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
    enable("&Scene/Bind or transfer skin...", !modal && s->can_configure_skin);
    enable("&Scene/Rig and shapekeys...", !modal && !s->asset_busy &&
           s->mesh_active && bool(s->deformation) && active != s->objects.end() &&
           active->editable);

    for(const auto *name:{"&Scene/Shapekeys/New shapekey...","&Scene/Shapekeys/Capture shape mix..."})enable(name,!modal&&s->can_author_shapes);
    for(const auto *name:{"&Scene/Shapekeys/Duplicate shapekey...","&Scene/Shapekeys/Mirror shapekey...","&Scene/Shapekeys/Rename shapekey..."})enable(name,!modal&&s->can_select_shape);
    enable("&Scene/Shapekeys/Restore before shape authoring...",!modal&&s->can_restore_shapes);
    enable("&Scene/Shapekeys/Choose shapekey...",!modal&&s->can_select_shape);
    enable("&Scene/Shapekeys/Sculpt selected shapekey",!modal&&s->can_select_shape);checked("&Scene/Shapekeys/Sculpt selected shapekey",s->shape_sculpt);
    if (active != s->objects.end()) {
      checked("&Scene/&Visible", active->visible);
      checked("&Scene/&Locked", active->locked);
    }
    checked("&Paint/Image projection/Use projection image",s->projection&&s->projection->settings.enabled);
    enable("&Paint/Image projection/Use projection image",s->projection&&s->projection->image&&!s->native_pending&&!s->stroking&&!s->shape_sculpt&&s->mesh_active);
    enable("&Paint/Image projection/Cancel image load",s->projection&&(s->projection->loading||s->projection->browsing));
    checked("&Scene/VR selection mode",s->selection_mode);checked("&Scene/XR object hierarchy",s->hierarchy_open);checked("&Scene/Focus selected objects",s->selection_focus);
    enable("&Scene/VR selection mode",s->can_select_objects);
    enable("&Scene/Focus selected objects",s->can_select_objects);
    checked("&Scene/Object grab mode",s->object_grab_mode);checked("&Scene/Ghost other objects",s->isolate_held);
    enable("&Scene/Object grab mode",!modal);enable("&Scene/Ghost other objects",!modal);
    enable("&Scene/Hold selected object",!modal&&active!=s->objects.end()&&active->visible&&!active->locked);
    enable("&Scene/Release held object",!modal&&s->held_object);enable("&Scene/Objects visibility...",!modal);
    enable("&Scene/Object and material...",!modal&&!s->asset_busy&&!s->object_grabbing&&active!=s->objects.end()&&active->editable);
    if(s->isolate_held&&s->held_object){for(auto path:{"&Scene/New sphere","&Scene/New empty sculpt","&Scene/Duplicate object","&Scene/Create &humanoid..."})enable(path,false);}
    enable("&Scene/&Visible", !modal);
    enable("&Scene/&Locked", !modal&&(!s->isolate_held||!s->held_object||s->held_object==s->active_object));
    for (auto path : {"&Sculpt/Masks and symmetry...", "&Sculpt/Paint protection mask",
                      "&Sculpt/Protect all", "&Sculpt/Clear protection mask", "&Sculpt/Invert protection mask",
                      "&Sculpt/Show protection mask", "&Sculpt/Mirror X", "&Sculpt/Mirror Y", "&Sculpt/Mirror Z"})
      enable(path, !modal && s->can_configure_sculpt);
    enable("&Sculpt/Resolution...",!modal && s->can_change_resolution);
    enable("&Sculpt/Procedural detail...",!modal && s->can_change_resolution);
    checked("&Sculpt/Paint protection mask", s->sculpt_settings.mask_edit);
    checked("&Sculpt/Show protection mask", s->sculpt_settings.mask_visible);
    checked("&Sculpt/Mirror X", (s->sculpt_settings.symmetry_axes & 1) != 0);
    checked("&Sculpt/Mirror Y", (s->sculpt_settings.symmetry_axes & 2) != 0);
    checked("&Sculpt/Mirror Z", (s->sculpt_settings.symmetry_axes & 4) != 0);
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
    enable("&Paint/Texture sets...",!modal&&s->can_edit_texture_sets);
    enable("&Paint/Pick material faces",!modal&&s->can_edit_texture_sets);
    enable("&Paint/Clear material face selection",!modal&&!s->asset_busy&&!s->texture_faces.empty());
    checked("&Paint/Pick material faces",s->texture_face_select);
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
    checked("&View/3D mouse navigation",spacenav.settings().enabled);
    checked("&View/Shading/&Authored material", !s->toon&&!s->pbr_view);
    checked("&View/Shading/&PBR", s->pbr_view);
    checked("&View/Shading/&Toon preview", s->toon);
    const auto expressions=s->expressions;
    enable("&View/Expressions/Start preview",!modal&&!expressions&&canvas.document.can_preview_expressions());
    for(auto path:{"&View/Expressions/Choose expression...","&View/Expressions/Weight...","&View/Expressions/Weight -","&View/Expressions/Weight +","&View/Expressions/Reset weights"})enable(path,!modal&&expressions&&!expressions->entries.empty());
    enable("&View/Expressions/Apply static pose",!modal&&expressions&&expressions->ready&&expressions->error.empty());
    enable("&View/Expressions/Cancel preview",!modal&&bool(expressions));
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
                                     active->editable);
    enable("&Paint/Prepare painting mesh",
           !modal && !s->asset_busy && active != s->objects.end() &&
               active->editable);
    enable("&Mesh/Generate &UV atlas", !modal && !s->asset_busy &&
                                           active != s->objects.end() &&
                                           active->editable);
    enable("&Paint/Use painted mesh",
           !modal && !s->asset_busy && active != s->objects.end() &&
               (active->has_mesh || (active->editable)));
    enable("&Paint/Edit source volume",
           !modal && s->mesh_active && !s->asset_busy);
    if(active!=s->objects.end()&&!active->editable){
      for(auto path:{"&Scene/Delete object","&Scene/Duplicate object","&Scene/Object and material...","&Mesh/Generate &UV atlas","&Paint/Add layer","&Paint/Delete layer","&Paint/Next layer","&Paint/Layer visible","&Paint/Layer opacity...","&Paint/Use painted mesh","&Paint/Edit source volume","&Paint/Prepare painting mesh"})enable(path,false);
    }
    if(s->isolate_held&&s->held_object)enable("&File/Import &mesh...",false);
    if (s->retopology || s->cut || s->polygon) {
      for (int i = 0; i < menu.size(); ++i) {
        auto label = menu.menu()[i].label();
        if (!label || (menu.mode(i) & FL_SUBMENU)) continue;
        std::string text = label;
        text.erase(std::remove(text.begin(), text.end(), '&'), text.end());
        if (text != "Undo" && text != "Redo" && text != "Close Window" &&
            text != "Frame all" && text != "Frame selection" && text != "Reset trackball" && text != "3D mouse navigation" && text != "3D mouse options..." && text != "Start or stop XR" && text != "Cancel background work")
          modes[i] |= FL_MENU_INACTIVE;
      }
    }
    auto polygon=s->polygon;
    bool polygon_editing=!modal&&polygon&&polygon->surface&&!polygon->busy&&!polygon->committing&&!s->asset_busy;
    enable("&Mesh/Polygon edit/Start polygon editing",!modal&&!polygon&&s->can_polygon);
    for(auto label:{"Vertices","Edges","Faces","Move components","Add to selection","Select all components","Clear component selection","Extrude faces","Inset faces","Bridge loops","Dissolve edges","Weld vertices","Subdivide edges","Connect vertices","Delete faces","Flip faces","Polygon controls...","Apply polygon edit"})
      enable((std::string("&Mesh/Polygon edit/")+label).c_str(),polygon_editing);
    enable("&Mesh/Polygon edit/Cancel polygon edit",!modal&&bool(polygon));
    enable("&Mesh/Polygon edit/Restore polygon source",!modal&&!polygon&&!s->asset_busy&&!s->object_grabbing&&s->polygon_has_source&&active!=s->objects.end()&&active->editable);
    checked("&Mesh/Polygon edit/Move components",s->polygon_controls.move_mode);
    checked("&Mesh/Polygon edit/Add to selection",s->polygon_controls.additive);
    for(auto [name,mode]:{std::pair{"Vertices",PolygonSelectionMode::Vertex},std::pair{"Edges",PolygonSelectionMode::Edge},std::pair{"Faces",PolygonSelectionMode::Face}})
      checked((std::string("&Mesh/Polygon edit/")+name).c_str(),polygon&&polygon->selection&&polygon->selection->mode==mode);
    if(polygon&&!modal){
      enable("&File/&Save",true);enable("&File/Save &As...",true);enable("&File/Close &Window",true);
      enable("&Edit/&Undo",!polygon->committing&&(polygon->can_undo||polygon->moving));
      enable("&Edit/&Redo",!polygon->committing&&polygon->can_redo);
    }
    if(s->can_save_after_asset&&!modal) {
      enable("&File/&Save",true);enable("&File/Save &As...",true);
    }
    enable("&Mesh/Retopology/Start patch", !modal && s->can_retopologize);
    auto retopo = s->retopology;
    bool editing = !modal && retopo && retopo->source && !retopo->committing && !s->asset_busy;
    const char *retopo_actions[] = {"Place points", "Move points", "Relax patch", "Clear point selection", "Projection distance..."};
    for (auto name : retopo_actions) enable((std::string("&Mesh/Retopology/") + name).c_str(), editing);
    size_t selected = retopo ? retopo->selected.size() : 0;
    enable("&Mesh/Retopology/Make face", editing && selected >= 3);
    enable("&Mesh/Retopology/Connect edge", editing && selected == 2);
    enable("&Mesh/Retopology/Bridge edges", editing && selected == 4);
    enable("&Mesh/Retopology/Extrude edge", editing && selected == 2);
    enable("&Mesh/Retopology/Seam edge", editing && selected == 2);
    enable("&Mesh/Retopology/Pin points", editing && selected != 0);
    enable("&Mesh/Retopology/Delete points", editing && selected != 0);
    enable("&Mesh/Retopology/Snap points", editing && selected != 0);
    enable("&Mesh/Retopology/Apply patch", editing && retopo->topology && !retopo->topology->faces().empty() && retopo->changed);
    enable("&Mesh/Retopology/Cancel patch", !modal && retopo && (!s->asset_busy || s->can_cancel_asset));
    checked("&Mesh/Retopology/Place points", !retopo || (!retopo->moving && !retopo->extruding));
    checked("&Mesh/Retopology/Move points", retopo && retopo->moving);
    checked("&Mesh/Retopology/Extrude edge", retopo && retopo->extruding);
    enable("&Mesh/Plane cut/Start cut",!modal&&s->can_cut);
    auto cut=s->cut;bool cutting=!modal&&cut&&!cut->committing&&!s->asset_busy;
    for(auto label:{"Offset...","Direction...","Cap color...","Flip kept side","Cap cut"})
      enable((std::string("&Mesh/Plane cut/")+label).c_str(),cutting&&cut->ready);
    enable("&Mesh/Plane cut/Cancel cut",!modal&&cut&&(!cut->committing||s->can_cancel_asset));
    enable("&Mesh/Plane cut/Apply cut",cutting&&!cut->busy&&cut->preview&&cut->preview_request==cut->request&&cut->preview->changed);
    checked("&Mesh/Plane cut/Cap cut",!cut||cut->settings.cap);
    checked("&Mesh/Plane cut/Flip kept side",cut&&cut->settings.flipped);
    if(expressions){
      const std::array<int,6> native_inputs{menu.find_index("&Edit/Cu&t"),menu.find_index("&Edit/&Copy"),menu.find_index("&Edit/&Paste"),menu.find_index("&Edit/Select &All"),menu.find_index("&Edit/&Undo"),menu.find_index("&Edit/&Redo")};
      // Preserve the canonical input/focus-derived modes above, including
      // owned modal fields. The preview excludes other document mutations.
      for(int i=0;i<menu.size();++i)if(menu.menu()[i].label()&&!(modes[i]&FL_SUBMENU)&&std::find(native_inputs.begin(),native_inputs.end(),i)==native_inputs.end())modes[i]|=FL_MENU_INACTIVE;
      for(auto path:views)enable(path,!modal);
      for(auto path:{"&View/Shading/&Authored material","&View/Shading/&PBR","&View/Shading/&Toon preview"})enable(path,!modal&&s->can_preview);
      for(auto path:{"&View/Expressions/Choose expression...","&View/Expressions/Weight...","&View/Expressions/Weight -","&View/Expressions/Weight +","&View/Expressions/Reset weights"})enable(path,!modal&&!expressions->entries.empty());
      enable("&View/Expressions/Apply static pose",!modal&&expressions->ready&&expressions->error.empty());
      enable("&View/Expressions/Cancel preview",!modal);enable("&File/Cancel background work",!modal);enable("&File/Close &Window",true);
    }
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
    auto current_frame = canvas.document.frame();
    auto info = current_frame.state;
    project = info->project_path;
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
    if(info->expressions){
      const auto &view=*info->expressions;text.str("");text<<"Expression preview";
      if(!view.entries.empty()){const auto &entry=view.entries.at(view.selected);text<<" | "<<entry.name<<" "<<int(std::lround(entry.weight*100))<<"%"<<(entry.binary?" (binary)":"");}
      text<<(view.ready?" | Apply static pose or Cancel":" | Updating preview");
    }
    if (info->weights.enabled) {
      text.str("");
      const char *modes[] = {"Add", "Subtract", "Replace", "Smooth"};
      text << "Weights " << modes[int(info->weights.mode)] << " | "
           << info->deformation->skeleton->bones.at(info->rig_joint).name
           << " | Shift+drag: paint | Blue: 0  Red: 1";
      if (info->weight_samples) text << (current_frame.surface->weight_samples >= info->weight_samples
          ? " | Live preview; release to apply" : " | Updating preview; release to apply");
    }
    if(info->polygon){
      const auto &p=*info->polygon;text<<"Polygon: "<<(p.selection&&p.selection->mode==PolygonSelectionMode::Vertex?"Vertices":p.selection&&p.selection->mode==PolygonSelectionMode::Edge?"Edges":"Faces")
       <<" | "<<(p.selection?p.selection->ids.size():0)<<" selected | "<<(info->polygon_controls.move_mode?"Shift-drag moves":"Shift-click selects")
       <<(p.committing?" | Applying":p.busy?" | Updating":" | Mesh > Polygon edit: operations / Apply / Cancel");
    }
    if (info->retopology) {
      auto &r = *info->retopology;
      text.str("");
      text << "Retopology: " << (r.extruding ? "Extrude edge" : r.moving ? "Move" : "Place / select")
           << " | Shift+click/drag | " << r.selected.size() << " selected";
      if (r.topology) text << " | " << r.topology->faces().size() << " faces";
      text << (r.committing ? " | Applying" : r.busy ? " | Updating" : " | Mesh > Retopology: face / apply / cancel");
    }
    if(info->cut){auto &c=*info->cut;text.str("");text<<"Plane cut | "<<c.settings.offset_mm<<" mm | "
      <<(c.settings.cap?"Cap":"Open trim")<<(c.committing?" | Applying":c.busy?" | Updating preview":" | Mesh > Plane cut: Apply / Cancel");}
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
    if(info->projection){
      const auto &projection=*info->projection;
      if(info->error.empty()&&(!projection.error.empty()||projection.loading||projection.browsing)){
        text.str("");text<<(!projection.error.empty()?"Image: "+projection.error:projection.loading?(info->message=="Cancelling image load..."?info->message:"Loading projection image..."):"Reading image folder...");
      }else if(projection.image&&projection.settings.enabled&&info->mesh_active&&!info->shape_sculpt&&!info->weights.enabled){
        const auto current=text.str();text.str("");text<<"Image "<<projection.image->name<<" | "<<current;
      }
    }
    if(!native_commands.empty())text<<" | Pending "<<native_commands.size()<<": "<<native_commands.front();
    if(info->native_pending)text<<(info->native_accepting?" | Painting ":" | Finishing stroke ")<<info->native_samples<<" samples";
    if(info->asset_busy) {
      const auto work=canvas.document.asset_status();
      if(work.cancelling)text<<" | Cancelling";
      else text<<" | "<<int(std::clamp(work.progress,0.f,1.f)*100)<<"%";
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
    const auto pending=canvas.document.frame().state;
    if(((pending->polygon&&pending->polygon->committing)||(pending->cut&&pending->cut->committing))&&
      (label=="Close Window"||label=="Quit"||label=="New"||label=="Open...")) {
      if(native_commands.size()>=8){canvas.document.report_error("Desktop command queue is full");return;}
      native_commands.push_back(std::move(label));return;
    }
    if(canvas.document.frame().state->native_pending) {
      canvas.sculpting=canvas.have_stamp=false;canvas.suppress_drag=true;
      if(label=="Undo"||label=="Cancel background work"){native_commands.clear();canvas.document.end_native_stroke(false);return;}
      if(native_commands.size()>=8){canvas.document.report_error("Desktop command queue is full; retry after the stroke finishes");return;}
      canvas.document.end_native_stroke(true);native_commands.push_back(std::move(label));return;
    }
    if(saving&&(label=="Close Window"||label=="Quit"||label=="New"||label=="Open...")) {
      if(native_commands.size()>=8){canvas.document.report_error("Desktop command queue is full");return;}
      native_commands.push_back(std::move(label));return;
    }
    auto modal = Fl::modal();
    if (label == "Cut" || label == "Copy" || label == "Paste" ||
        label == "Select All") {
      auto input = dynamic_cast<Fl_Input_ *>(Fl::focus());
      if(label=="Select All"&&canvas.document.frame().state->polygon&&(!input||input->window()!=modal)){
        canvas.document.edit_polygon(PolygonAction::SelectAll);canvas.invalidate();return;
      }
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
    if(label=="Close Window"&&canvas.document.frame().state->polygon){canvas.document.end_polygon(false);canvas.invalidate();return;}
    if (canvas.sculpting) {
      canvas.document.end_desktop();
      canvas.sculpting = false;
    }
    if(label=="Start preview"){canvas.document.begin_expression_preview();}
    else if(label=="Choose expression..."){choose_expression_dialog(canvas.document);}
    else if(label=="Weight..."){expression_weight_dialog(canvas.document);}
    else if(label=="Weight -"||label=="Weight +"){canvas.document.expression_action(label=="Weight +"?SpatialCommand::ExpressionIncrease:SpatialCommand::ExpressionDecrease);}
    else if(label=="Reset weights"){canvas.document.expression_action(SpatialCommand::ExpressionReset);}
    else if(label=="Apply static pose"){canvas.document.end_expression_preview(true);}
    else if(label=="Cancel preview"){canvas.document.end_expression_preview(false);}
    else if(label=="Humanoid rig fitting..."){edit_humanoid_fit_dialog(canvas.document);
    }else if(label=="Load projection image..."){
      const auto state=canvas.document.frame().state;const AssetSource source{state->generation,state->revision,state->active_object,state->edit_authority};
      auto selected=projection_image_dialog();
      if(selected)canvas.document.load_projection_image(*selected,canvas.projection_camera(),source);
    }else if(label=="Image projection options..."){
      auto state=canvas.document.frame().state;
      auto result=projection_dialog(canvas.document.projection_settings(),canvas.projection_camera(),state->projection&&state->projection->image?state->projection->image->name:"No image loaded");
      if(result)canvas.document.configure_projection(std::move(*result),{state->generation,state->revision,state->active_object,state->edit_authority});
    }else if(label=="Use projection image"){canvas.document.projection_action(SpatialCommand::ProjectionToggle);
    }else if(label=="Projection from view"){auto settings=canvas.document.projection_settings();settings.camera=canvas.projection_camera();canvas.document.configure_projection(settings);
    }else if(label=="Cancel image load"){canvas.document.cancel_projection_load();
    }else if (label == "Refresh recovery") {
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
      auto source=canvas.document.begin_import();
      auto selected = fl_file_chooser("Import model", "*.{obj,fbx,gltf,glb,vrm}", nullptr);
      if(selected){std::filesystem::path path(selected);
        if(auto options=import_options_dialog(path,canvas.document.import_settings())){
          const auto objects=canvas.document.frame().state->objects.size();
          canvas.document.import_asset(path,*options,source);
          canvas.frame_import_pending=true;canvas.frame_import_generation=source.generation;canvas.frame_import_objects=objects;
        }
      }
    } else if (label == "Open...") {
      if (!discard())
        return;
      auto path = fl_file_chooser("Open project or editable package", "*.{dsculpt,glb,vrm}", nullptr);
      if (path) {
        canvas.document.open_async(path);
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
      saving.emplace(canvas.document.save_async(path));
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
    } else if (label == "Bind or transfer skin...") {
      edit_skin_dialog(canvas.document);
    } else if (label == "Skin weights...") {
      edit_weights_dialog(canvas.document);

    } else if(label=="Choose shapekey..."){
      choose_shapekey_dialog(canvas.document);
    } else if(label=="Sculpt selected shapekey"){
      canvas.document.configure_shape_sculpt(!canvas.document.frame().state->shape_sculpt);
    } else if(label=="New shapekey..."||label=="Capture shape mix..."||label=="Duplicate shapekey..."||label=="Mirror shapekey..."||label=="Rename shapekey..."){
      edit_shapekey_dialog(canvas.document,label=="Capture shape mix..."?ShapeKeyAction::CaptureMix:label=="Duplicate shapekey..."?ShapeKeyAction::Duplicate:label=="Mirror shapekey..."?ShapeKeyAction::Mirror:label=="Rename shapekey..."?ShapeKeyAction::Rename:ShapeKeyAction::Create);
    } else if(label=="Restore before shape authoring..."){
      if(fl_choice("Restore the complete mesh state from before shapekey authoring, including paint, rig and material?","Cancel","Restore",nullptr)==1){ShapeKeyEdit edit;edit.action=ShapeKeyAction::Restore;canvas.document.author_shape(std::move(edit));}
    } else if (label == "Rig and shapekeys...") {
      edit_rig_dialog(canvas.document);
    } else if(label=="Bake surface..."){edit_bake_dialog(canvas.document);
    } else if(label=="Clear baked surface"){canvas.document.clear_bake();
    } else if(label=="Convert to portable normals"){canvas.document.convert_baked_normals();
    } else if(label=="Cancel background work"){canvas.document.cancel_background_work();
    } else if(label=="Procedural detail...") {
      auto draft=canvas.document.draft_scene();auto initial=canvas.document.frame().state->procedural_options;
      initial.depth=std::max(initial.depth,draft.scene.active().volume->voxel_size()*2.5f);
      auto options=procedural_sculpt_dialog(draft.scene.world_transform(draft.source.object).scale,initial);
      if(options)canvas.document.apply_procedural_sculpt(std::move(*options),draft.source);
    } else if(label=="Resolution...") {
      auto draft=canvas.document.draft_scene();const auto &source=draft.scene.active();
      SculptResolutionOptions initial;initial.voxel_size=source.volume->voxel_size();
      auto options=sculpt_resolution_dialog(initial.voxel_size,draft.scene.world_transform(source.id).scale,initial);
      if(options)canvas.document.change_sculpt_resolution(std::move(*options),draft.source);
    } else if(label=="Isotropic remesh...") {
      edit_triangle_remesh(canvas.document);
    } else if (label == "Voxel remesh...") {
      edit_voxel_remesh(canvas.document);
    } else if(label=="VR selection mode"){canvas.document.configure_selection(!canvas.document.frame().state->selection_mode);
    } else if(label=="XR object hierarchy"){canvas.document.show_object_hierarchy(!canvas.document.frame().state->hierarchy_open);
    } else if(label=="Focus selected objects"){canvas.document.focus_selection(!canvas.document.frame().state->selection_focus);
    } else if(label=="Object grab mode"){
      auto s=canvas.document.frame().state;canvas.document.configure_object_grab(!s->object_grab_mode,s->isolate_held);
    } else if(label=="Ghost other objects"){
      auto s=canvas.document.frame().state;canvas.document.configure_object_grab(s->object_grab_mode,!s->isolate_held);
    } else if(label=="Hold selected object")canvas.document.hold_object(canvas.document.frame().state->active_object);
    else if(label=="Release held object")canvas.document.release_held_object();
    else if(label=="Objects visibility...")edit_visibility(canvas.document);
    else if (label == "Object and material...") {
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
    } else if(label=="Visible"){auto state=canvas.document.frame().state;auto object=std::find_if(state->objects.begin(),state->objects.end(),[&](const auto&o){return o.id==state->active_object;});if(object!=state->objects.end())canvas.document.object_visibility(object->id,!object->visible);
    } else if (label == "Locked") {
      auto scene = canvas.document.copy_scene();
      auto &o = scene.active();
      canvas.document.object_properties(
          o.id, o.name, label == "Visible" ? !o.visible : o.visible,
          label == "Locked" ? !o.locked : o.locked, o.transform, o.material);
    } else if (label == "UV editor...") {
      edit_uv(canvas.document);
    } else if(label=="Start polygon editing"){
      auto state=canvas.document.frame().state;
      AssetSource source{state->generation,state->revision,state->active_object,state->edit_authority};
      const bool convert=state->polygon_requires_conversion;
      if(!convert||polygon_conversion_dialog())canvas.document.begin_polygon(convert,source);
    } else if(label=="Apply polygon edit"||label=="Cancel polygon edit"){
      canvas.document.end_polygon(label=="Apply polygon edit");
    } else if(label=="Restore polygon source"){
      canvas.document.restore_polygon_source();
    } else if(label=="Polygon controls..."){
      if(auto settings=polygon_controls_dialog(canvas.document.frame().state->polygon_controls))canvas.document.configure_polygon(*settings);
    } else if(label=="Move components"||label=="Add to selection"){
      auto settings=canvas.document.frame().state->polygon_controls;
      if(label=="Move components")settings.move_mode=!settings.move_mode;else settings.additive=!settings.additive;
      canvas.document.configure_polygon(settings);
    } else if(label=="Vertices"||label=="Edges"||label=="Faces"||label=="Select all components"||label=="Clear component selection"||label=="Extrude faces"||label=="Inset faces"||label=="Bridge loops"||label=="Dissolve edges"||label=="Weld vertices"||label=="Subdivide edges"||label=="Connect vertices"||label=="Delete faces"||label=="Flip faces"){
      auto action=label=="Vertices"?PolygonAction::Vertices:label=="Edges"?PolygonAction::Edges:label=="Faces"?PolygonAction::Faces:label=="Select all components"?PolygonAction::SelectAll:label=="Clear component selection"?PolygonAction::Clear:label=="Extrude faces"?PolygonAction::Extrude:label=="Inset faces"?PolygonAction::Inset:label=="Bridge loops"?PolygonAction::Bridge:label=="Dissolve edges"?PolygonAction::Dissolve:label=="Weld vertices"?PolygonAction::Weld:label=="Subdivide edges"?PolygonAction::CutEdges:label=="Connect vertices"?PolygonAction::ConnectVertices:label=="Delete faces"?PolygonAction::DeleteFaces:PolygonAction::FlipFaces;
      canvas.document.edit_polygon(action);
    } else if (label == "Start patch") {
      canvas.document.begin_retopology();
    } else if(label=="Start cut"){
      canvas.document.begin_cut();
    } else if(label=="Apply cut"||label=="Cancel cut"){
      canvas.document.end_cut(label=="Apply cut");
    } else if(label=="Offset..."||label=="Direction..."||label=="Cap color..."){
      edit_cut_parameters(canvas.document,label=="Offset..."?VrParameter::CutOffset:label=="Direction..."?VrParameter::CutDirection:VrParameter::CutCapColor);
    } else if(label=="Flip kept side"||label=="Cap cut"){
      auto settings=canvas.document.frame().state->cut->settings;
      if(label=="Flip kept side")settings.flipped=!settings.flipped;else settings.cap=!settings.cap;
      canvas.document.edit_cut(settings);
    } else if (label == "Apply patch" || label == "Cancel patch") {
      canvas.document.end_retopology(label == "Apply patch");
    } else if (label == "Place points" || label == "Move points" || label == "Extrude edge") {
      canvas.document.edit_retopology(label == "Extrude edge" ? RetopoAction::ExtrudeMode : label == "Move points" ? RetopoAction::MoveMode : RetopoAction::PlaceMode);
    } else if (label == "Projection distance...") {
      edit_retopology_distance(canvas.document);
    } else if (label == "Make face" || label == "Connect edge" || label == "Bridge edges" || label == "Delete points" ||
               label == "Pin points" || label == "Seam edge" || label == "Relax patch" ||
               label == "Snap points" || label == "Clear point selection") {
      canvas.document.edit_retopology(label == "Make face" ? RetopoAction::Face
          : label == "Bridge edges" ? RetopoAction::BridgeEdges : label == "Connect edge" ? RetopoAction::Wire : label == "Delete points" ? RetopoAction::Delete
          : label == "Pin points" ? RetopoAction::Pin : label == "Seam edge" ? RetopoAction::Seam
          : label == "Relax patch" ? RetopoAction::Relax : label == "Snap points" ? RetopoAction::Snap : RetopoAction::Clear);
    } else if (label == "Prepare painting mesh") {
      canvas.document.prepare_mesh();
    } else if (label == "Use painted mesh") {
      canvas.document.use_mesh();
    } else if (label == "Edit source volume") {
      canvas.document.use_source();
    } else if (label == "Texture sets...") {
      edit_texture_sets(canvas.document);
    } else if (label == "Pick material faces" || label == "Clear material face selection") {
      canvas.document.texture_set_action(label=="Pick material faces"?SpatialCommand::TexturePickFaces:SpatialCommand::TextureClearFaces);
    } else if (label == "Brush settings...") {
      edit_paint(canvas.document);
    } else if (label == "Masks and symmetry...") {
      auto state=canvas.document.frame().state;
      auto settings=sculpt_controls_dialog(state->sculpt_settings);
      if(settings) canvas.document.sculpt_settings(*settings,{state->generation,state->revision,state->active_object});
    } else if (label == "Paint protection mask" || label == "Protect all" ||
               label == "Clear protection mask" || label == "Invert protection mask" ||
               label == "Show protection mask" || label == "Mirror X" || label == "Mirror Y" || label == "Mirror Z") {
      canvas.document.sculpt_command(label=="Paint protection mask"?SpatialCommand::SculptMaskToggle:
          label=="Protect all"?SpatialCommand::SculptMaskFill:label=="Clear protection mask"?SpatialCommand::SculptMaskClear:
          label=="Invert protection mask"?SpatialCommand::SculptMaskInvert:label=="Show protection mask"?SpatialCommand::SculptMaskVisible:
          label=="Mirror X"?SpatialCommand::SculptMirrorX:label=="Mirror Y"?SpatialCommand::SculptMirrorY:SpatialCommand::SculptMirrorZ);
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
    } else if (label == "Export editable project (.glb)...") {
      auto format=editable_export_dialog();
      if(format) {
        auto chosen=fl_file_chooser("Export editable project", "*.glb", "Project.glb");
        if(chosen)canvas.document.export_asset(chosen,*format);
      }
    } else if (label == "Export editable avatar (.vrm)...") {
      if(!detail::editable_avatar_view(canvas.document.frame()))return;
      auto format=editable_export_dialog(EditableExportKind::AvatarV1);
      if(format){auto chosen=fl_file_chooser("Export editable avatar","*.vrm","Avatar.vrm");
        if(chosen)canvas.document.export_asset(chosen,*format);}
    } else if (label == "Export selected avatar (.vrm)...") {
      auto chosen = fl_file_chooser("Export selected avatar", "*.vrm", "Avatar.vrm");
      if (chosen) canvas.document.export_asset(chosen, AssetExportFormat::Avatar);
    } else if (label == "Start or stop XR") {
      toggle_xr();
    } else if (label == "Authored material") {
      canvas.document.authored_shading();
      canvas.redraw();
    } else if (label == "PBR") {
      canvas.document.shading(false);
      canvas.redraw();
    } else if (label == "Toon preview") {
      canvas.document.shading(true);
      canvas.redraw();
    } else if(label=="3D mouse navigation") {
      auto settings=spacenav.settings();settings.enabled=!settings.enabled;
      spacenav.settings(settings,desktop::spacenav_now());
    } else if(label=="3D mouse options...") {
      desktop::spacenav_options(spacenav);
    } else if(label=="Frame all"||label=="Frame selection"){
      canvas.frame_view(label=="Frame selection");
    } else if (label == "Reset trackball") {
      canvas.rotation = {};
      canvas.pan = {};canvas.orbit_pivot={};
      canvas.distance = 2.2f;canvas.zoom_limit=20.f;
      if(canvas.view_boundary)canvas.view_boundary();
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
    desktop::NativeFontconfigRuntime fontconfig;
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
      // A standalone session has no desktop to resume an unconfirmed patch.
      // Accepted work has drained above; discard only the remaining draft.
      if (auto view = document.frame().state->retopology; view && !view->committing)
        document.end_retopology(false);
      if (auto view = document.frame().state->cut; view && !view->committing)
        document.end_cut(false);
      if(auto view=document.frame().state->polygon;view&&!view->committing)document.end_polygon(false);
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
      // Plain scenes omit the v8 feature keys; extended scenes include nulls
      // for unaffected objects. Hash their identical semantic payload equally.
      for (const auto *key : {"material_bindings", "retopology"})
        if (value.contains(key) && value.at(key).is_null()) value.erase(key);
      objects.push_back({{"id", value.at("id")}, {"name", value.at("name")},
                         {"visible", visible}, {"mesh_active", value.at("mesh_active")},
                         {"baked",value.contains("painted") && value.at("painted").contains("baked")},
                         {"voxel_size", value.at("volume").at("voxel_size")},
                         {"payload_hash", digest(value)}});
    }
    std::cout << nlohmann::json{{"sculpt_layers", sculpt_layers},
                                {"objects", objects},
                                {"scene_hash", digest(encoded)},
                                {"retopology_vertices", scene.active().retopology ? scene.active().retopology->topology.vertices().size() : 0},
                                {"retopology_faces", scene.active().retopology ? scene.active().retopology->topology.faces().size() : 0},
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
  desktop::NativeFontconfigRuntime fontconfig;
  desktop::NativeDisplayRuntime display;
  Fl_Window::default_xclass("org.domed.Sculpt");
  dx::init();
  App app;
  if (argc == 3 && std::string(argv[1]) == "--smoke-test")
    app.canvas.capture = argv[2];
  else if (argc == 2) {
    app.canvas.document.open_async(argv[1]);
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
