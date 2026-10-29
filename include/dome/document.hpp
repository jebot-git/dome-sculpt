// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "input.hpp"
#include "navigation.hpp"
#include "scene.hpp"
#include "uv.hpp"
#include <atomic>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
namespace dome {
struct SpatialHand {
  TrackedHand tracked;
  bool joystick = false, undo = false, redo = false;
  float x = 0, y = 0;
  DeviceKind kind = DeviceKind::Controller;
};
struct SpatialFrame {
  uint64_t sequence = 0;
  double time = 0;
  bool focused = false, head_valid = false;
  CanvasTransform head;
  std::array<SpatialHand, 2> hands{};
};
struct ObjectInfo {
  uint64_t id;
  std::string name;
  bool visible, locked;
  CanvasTransform transform;
  Material material;
  bool mesh_active = false, has_mesh = false;
};
struct EditorState {
  uint64_t revision = 1, generation = 1, input_epoch = 1;
  bool dirty = false, stroking = false, menu = false;
  uint64_t active_object = 1;
  std::vector<ObjectInfo> objects;
  Tool tool = Tool::Clay;
  int menu_index = 0, menu_page = 0;
  float radius = .12f;
  CanvasTransform canvas{}, panel{};
  std::array<SpatialHand, 2> hands{};
  std::string message, error;
  std::string font_file;
  int font_pixels = 16;
  bool light = false, toon = false;
  bool mesh_active = false, asset_busy = false, can_undo = false,
       can_redo = false;
  std::array<bool, 12> menu_enabled{true, true, true, true, true, true,
                                    true, true, true, true, true, true};
  PaintSettings paint;
  VrControlState controls;
  std::shared_ptr<const UvView> uv;
  uint64_t active_layer = 0;
  bool layer_visible = false;
  std::vector<std::string> paint_layers;
  uint32_t accent = 0x6babff;
  int opacity = 255;
};
struct SurfaceRange {
  uint64_t object;
  size_t first, count;
  Material material;
  std::shared_ptr<const TextureMaps> textures{};
};
struct MeshSnapshot {
  uint64_t revision = 0, generation = 0;
  Mesh mesh;
  std::vector<SurfaceRange> ranges;
};
struct EditorSnapshot {
  std::shared_ptr<const EditorState> state;
  std::shared_ptr<const MeshSnapshot> surface;
};
// Model edits are serialized; renderers only acquire immutable atomic
// snapshots. Neither meshing, UV generation nor file I/O executes in the XR
// frame loop.
class Document {
public:
  Document();
  ~Document();
  Document(const Document &) = delete;
  Document &operator=(const Document &) = delete;
  EditorSnapshot frame() const;
  Volume copy_volume() const;
  Scene copy_scene() const;
  void replace(Scene, bool clean = true);
  uint64_t add_object(bool sphere = true);
  void duplicate_object();
  void delete_object();
  void select_object(uint64_t);
  void object_properties(uint64_t, std::string, bool visible, bool locked,
                         CanvasTransform, Material);
  void replace(Volume, bool clean = true);
  void seed();
  bool begin_desktop();
  void stamp_desktop(Brush);
  void end_desktop();
  void cancel_desktop();
  std::optional<Vec3> raycast(Vec3, Vec3) const;
  Vec3 gradient(Vec3) const;
  bool undo();
  bool redo();
  void configure(Tool, float radius);
  void appearance(std::string file, int pixels, bool light, uint32_t accent,
                  int opacity);
  void shading(bool toon);
  void prepare_mesh(uint32_t resolution = 512);
  bool wait_asset(
      std::chrono::milliseconds timeout = std::chrono::seconds(60)) const;
  void begin_uv();
  void edit_uv(UvAction, float x = 0, float y = 0);
  void end_uv(bool accept);
  void use_source();
  void cancel_spatial_menu();
  void use_mesh();
  void paint_settings(PaintSettings);
  void paint_command(SpatialCommand);
  void report_error(std::string);
  void submit(SpatialFrame);
  void finish_spatial();
  void begin_spatial();
  void rebase_space(std::optional<CanvasTransform>);
  void save(const std::filesystem::path &);
  void open(const std::filesystem::path &);
  void request_save(bool export_mesh = false);
  bool
  wait_mesh(std::chrono::milliseconds timeout = std::chrono::seconds(10)) const;
  bool wait_input(
      std::chrono::milliseconds timeout = std::chrono::seconds(10)) const;

private:
  mutable std::mutex model_mutex_;
  Scene scene_;
  std::shared_ptr<Volume> bound_volume_ = scene_.active().volume;
  Volume &volume() { return *scene_.active().volume; }
  const Volume &volume() const { return *scene_.active().volume; }
  struct History {
    std::shared_ptr<Volume> sculpt;
    std::optional<Scene> before, after;
    uint64_t from = 0, to = 0;
    std::shared_ptr<PaintedMesh> paint{};
  };
  std::deque<History> undo_, redo_;
  SpatialInput input_;
  CanvasNavigation navigation_;
  EditorState state_;
  bool desktop_ = false, anchored_ = false;
  uint64_t last_volume_ = 0, last_paint_ = 0, content_ = 1, saved_content_ = 1,
           next_content_ = 2;
  std::filesystem::path path_;
  std::optional<Scene> control_before_;
  PaintSettings control_settings_;
  float control_radius_ = 0;
  bool control_changed_ = false;
  uint64_t control_object_ = 0;
  std::optional<UvWorkspace> uv_;
  void queue_uv_locked(std::shared_ptr<const Mesh>);
  std::atomic<std::shared_ptr<const EditorState>> published_;
  std::atomic<std::shared_ptr<const MeshSnapshot>> mesh_;
  struct MeshJob {
    uint64_t revision, generation;
    Scene scene;
  };
  mutable std::mutex mesh_mutex_;
  mutable std::condition_variable mesh_changed_;
  std::optional<MeshJob> pending_mesh_;
  mutable std::mutex input_mutex_;
  mutable std::condition_variable input_changed_;
  std::deque<std::pair<uint64_t, SpatialFrame>> inputs_;
  bool reset_input_ = false, processing_ = false;
  struct FileJob {
    Scene scene;
    std::filesystem::path path;
    uint64_t revision, generation, content;
    bool export_mesh;
    bool adopt_path = false;
    std::shared_ptr<std::promise<void>> completion{};
    bool prepare_mesh = false;
    uint64_t object = 0, source_revision = 0;
    uint32_t resolution = 512;
    std::shared_ptr<Volume> source_identity{};
    std::shared_ptr<const Mesh> uv_mesh{};
    std::shared_ptr<PaintedMesh> paint_identity{};
  };
  mutable std::mutex file_mutex_;
  mutable std::condition_variable file_changed_;
  std::deque<FileJob> files_;
  bool files_stopping_ = false;
  std::atomic_bool stopping_{false};
  std::thread mesh_thread_, input_thread_, file_thread_;
  void publish_locked(bool force_geometry = false);
  void record_locked(History);
  void record_stroke_locked();
  void scene_changed_locked(Scene before);
  bool undo_locked(bool redo);
  void bind_locked();
  bool action_active_locked() const;
  bool pending_locked() const;
  void begin_action_locked();
  void stamp_action_locked(const Brush &);
  bool end_action_locked();
  void cancel_action_locked();
  void prepare_locked(uint32_t);
  void finish_locked();
  void process_locked(const SpatialFrame &);
  void file_locked(bool export_mesh);
  void command_locked(SpatialCommand);
  bool command_enabled_locked(SpatialCommand) const;
  bool parameter_enabled_locked(VrParameter) const;
  void control_event_locked(VrControlEvent);
  void finish_control_locked(bool accept);
  void mesh_loop();
  void input_loop();
  void file_loop();
};
} // namespace dome
