// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "input.hpp"
#include "navigation.hpp"
#include "recovery.hpp"
#include "scene.hpp"
#include "bake-authoring.hpp"
#include "uv.hpp"
#include "weight-authoring.hpp"
#include "skin-authoring.hpp"
#include <atomic>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <span>
#include <thread>
namespace dome {
class SurfaceQuery;
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
struct AssetWork {
  std::atomic_bool cancelled{false};
  std::atomic<float> progress{0};
};
struct AssetStatus { bool cancellable=false, cancelling=false; float progress=0; };
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
  std::string recovery_error;
  std::vector<std::string> recovery_candidates;
  bool recovery_busy = false;
  size_t recovery_selected = 0;
  std::string font_file;
  int font_pixels = 16;
  bool light = false, toon = false;
  bool mesh_active = false, asset_busy = false, can_undo = false,
       can_redo = false;
  std::shared_ptr<const DeformationState> deformation;
  size_t rig_joint = 0, morph_target = 0;
  SkinAuthoringOptions skin_options;
  std::vector<uint64_t> skin_sources;
  bool skin_use_locks = true, can_configure_skin = false, can_bind_skin = false;
  bool can_bake = false, can_clear_bake = false, can_cancel_asset = false;
  float asset_progress = 0;
  uint64_t bake_source = 0;
  std::vector<uint64_t> bake_sources;
  ObjectBakeSettings bake_settings;
  WeightToolSettings weights;
  bool can_paint_weights = false;
  bool can_voxel_remesh = false;
  bool can_export_avatar = false;
  float voxel_size_mm = 30;
  std::array<bool, 12> menu_enabled{true, true, true, true, true, true,
                                    true, true, true, true, true, true};
  PaintSettings paint;
  VrControlState controls;
  std::shared_ptr<const UvView> uv;
  uint64_t active_sculpt_layer = 0;
  std::vector<std::string> sculpt_layers;
  std::string sculpt_layer_name;
  bool sculpt_layer_visible = true, sculpt_layer_locked = false,
       sculpt_editable = true;
  float sculpt_layer_opacity = 1;
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
  std::shared_ptr<const CreatorAppearance> appearance{};
  size_t first_vertex = 0;
};
struct MeshSnapshot {
  uint64_t revision = 0, generation = 0;
  Mesh mesh;
  std::vector<SurfaceRange> ranges;
  // Empty: ordinary material view. Otherwise one value per packed vertex;
  // -1: ordinary material, [0,1]: unlit selected-joint weight visualization.
  std::vector<float> selected_weights;
  std::shared_ptr<const SurfaceQuery> picking;
  std::vector<std::array<float,4>> tangents;
};
struct AssetSource {
  uint64_t generation = 0, revision = 0, object = 0;
};
struct SceneDraft {
  Scene scene;
  AssetSource source;
};
enum class AssetExportFormat { Glb, Avatar };
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
  SceneDraft draft_scene() const;
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
  // Native asset operations run on the ordered worker against a detached scene.
  // No callback may retain mutable aliases or access UI/Document/model state.
  using SceneOperation = std::function<void(Scene &, const std::function<bool()> &)>;
  void edit_scene(std::string label, SceneOperation, AssetSource expected = {});
  void import_asset(const std::filesystem::path &, uint32_t resolution = 512);
  void remesh_to_sculpt(float world_voxel_mm, AssetSource expected = {});
  void bake_from_object(uint64_t source, ObjectBakeSettings = {}, AssetSource expected = {});
  void clear_bake(AssetSource expected = {});
  void cancel_asset_work();
  AssetStatus asset_status() const;
  void begin_voxel_remesh();
  void edit_voxel_size(float world_voxel_mm);
  void end_voxel_remesh(bool accept);
  bool wait_asset(
      std::chrono::milliseconds timeout = std::chrono::seconds(60)) const;
  void begin_uv();
  void edit_uv(UvAction, float x = 0, float y = 0);
  void end_uv(bool accept);
  bool wait_uv(std::chrono::milliseconds timeout = std::chrono::seconds(60)) const;
  void use_source();
  void cancel_spatial_menu();
  void use_mesh();
  void paint_settings(PaintSettings);
  void weight_settings(WeightToolSettings, std::optional<uint32_t> joint = {}, AssetSource expected = {});
  void bind_skin(SkinAuthoringOptions, AssetSource expected = {});
  void paint_command(SpatialCommand);
  void sculpt_command(SpatialCommand);
  void sculpt_layer_properties(uint64_t object, uint64_t layer, std::string,
                               bool visible, bool locked, float opacity);
  void report_error(std::string);
  void enable_recovery(std::filesystem::path = default_recovery_directory());
  void refresh_recovery();
  void restore_recovery(size_t index = 0);
  RecoveryStatus recovery_status() const;
  void submit(SpatialFrame);
  // Platform adapters may deliver a burst with original sample times. Focus
  // loss is ordered after earlier button transitions, not a queue discard.
  void submit(std::span<const SpatialFrame>);
  void finish_spatial();
  void begin_spatial();
  void rebase_space(std::optional<CanvasTransform>);
  void save(const std::filesystem::path &);
  // Captures a committed scene and performs ordered file work asynchronously.
  // Ignoring the returned promise never blocks the UI thread.
  std::future<void> export_asset(const std::filesystem::path &,
                                AssetExportFormat = AssetExportFormat::Glb);
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
  std::unique_ptr<RecoveryService> recovery_;
  std::filesystem::path recovery_root_;
  RecoverySource recovery_source_;
  std::vector<RecoveryCandidate> recovery_catalog_;
  uint64_t recovery_content_ = 0;
  std::string recovery_discovery_error_;
  void replace_locked(Scene, bool clean);
  void checkpoint_locked();
  void refresh_recovery_locked();
  void restore_recovery_locked(size_t);
  std::optional<Scene> control_before_;
  PaintSettings control_settings_;
  WeightToolSettings control_weights_;
  float control_radius_ = 0;
  bool control_changed_ = false;
  uint64_t control_object_ = 0;
  AssetSource control_source_{};
  uint64_t rig_object_ = 0;
  size_t control_rig_index_ = 0;
  bool weight_stroking_ = false;
  AssetSource weight_source_{};
  uint32_t weight_joint_ = 0;
  WeightToolSettings weight_stroke_settings_{};
  std::vector<WeightBrush> weight_samples_;
  float control_voxel_mm_ = 30;
  std::optional<UvWorkspace> uv_;
  uint64_t uv_session_ = 0;
  bool uv_accept_pending_ = false;
  bool uv_solving_ = false;
  struct UvValidationJob {
    uint64_t request, session, generation;
    std::shared_ptr<const Mesh> mesh;
    std::optional<UvAction> operation;
    std::shared_ptr<UvWorkspace> workspace;
  };
  mutable std::mutex uv_mutex_;
  mutable std::condition_variable uv_changed_;
  std::optional<UvValidationJob> uv_validation_;
  std::weak_ptr<const Mesh> uv_scheduled_;
  std::atomic<uint64_t> uv_request_{0};
  void schedule_uv_locked();
  void invalidate_uv_locked();
  void uv_loop();
  void queue_uv_operation_locked(UvAction);
  void apply_uv_action_locked(UvAction, float = 0, float = 0);
  void queue_uv_locked(std::shared_ptr<const Mesh>);
  std::atomic<std::shared_ptr<const EditorState>> published_;
  std::atomic<std::shared_ptr<const MeshSnapshot>> mesh_;
  struct MeshJob {
    uint64_t revision, generation;
    Scene scene;
    bool weight_view = false;
    bool linked_weights = true;
    uint32_t weight_joint = 0;
  };
  mutable std::mutex mesh_mutex_;
  mutable std::condition_variable mesh_changed_;
  std::optional<MeshJob> pending_mesh_;
  mutable std::mutex input_mutex_;
  mutable std::condition_variable input_changed_;
  std::deque<std::pair<uint64_t, SpatialFrame>> inputs_;
  bool reset_input_ = false, processing_ = false, spatial_accepting_ = true;
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
    std::shared_ptr<const std::vector<uint32_t>> uv_source_vertices{};
    std::shared_ptr<PaintedMesh> paint_identity{};
    RecoverySource recovery_source{};
    bool create_parent = false;
    bool discover_recovery = false;
    std::optional<RecoveryCandidate> restore_recovery{};

    uint64_t uv_session = 0;
    bool uv_desktop = false;
    SceneOperation scene_operation{};
    std::string operation_label{};
    std::shared_ptr<std::string> operation_report{};
    std::shared_ptr<AssetWork> work{};
    std::shared_ptr<bool> operation_changed{};
    AssetExportFormat export_format = AssetExportFormat::Glb;
  };
  std::atomic<std::shared_ptr<AssetWork>> asset_work_;
  mutable std::mutex file_mutex_;
  mutable std::condition_variable file_changed_;
  std::deque<FileJob> files_;
  bool files_stopping_ = false;
  std::atomic_bool stopping_{false};
  std::thread mesh_thread_, input_thread_, file_thread_, uv_thread_;
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
  void file_locked(bool export_mesh, AssetExportFormat = AssetExportFormat::Glb);
  void process_uv_transfer(FileJob &);
  void command_locked(SpatialCommand);
  bool command_enabled_locked(SpatialCommand) const;
  bool parameter_enabled_locked(VrParameter) const;
  void control_event_locked(VrControlEvent);
  void finish_control_locked(bool accept);
  void mesh_loop();
  void input_loop();
  void file_loop();
  void queue_scene_operation(std::string, SceneOperation, AssetSource,
                             std::shared_ptr<std::string>);
  void queue_scene_operation_locked(std::string, SceneOperation, AssetSource,
                                    std::shared_ptr<std::string>, std::shared_ptr<AssetWork> = {}, std::shared_ptr<bool> = {});
  std::vector<uint64_t> bake_sources_locked() const;
  bool bake_target_ready_locked() const;
  void queue_bake_locked(uint64_t, ObjectBakeSettings, AssetSource = {});
  void queue_clear_bake_locked(AssetSource = {});
  void cancel_asset_work_locked();
  bool skin_target_ready_locked() const;
  std::vector<uint64_t> skin_sources_locked(SkinAuthoringMode) const;
  void queue_bind_skin_locked(SkinAuthoringOptions, AssetSource = {});
  void queue_voxel_sculpt_locked(float, AssetSource);
};
} // namespace dome
