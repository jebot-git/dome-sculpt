// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "input.hpp"
#include "import-browser.hpp"
#include "navigation.hpp"
#include "recovery.hpp"
#include "scene.hpp"
#include "bake-authoring.hpp"
#include "uv.hpp"
#include "weight-authoring.hpp"
#include "skin-authoring.hpp"
#include "sculpt-resolution.hpp"
#include "procedural-sculpt.hpp"
#include "retopo-authoring.hpp"
#include "polygon-controls.hpp"
#include "cut-authoring.hpp"
#include "native-stroke.hpp"
#include <atomic>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <span>
#include <thread>
namespace dome {
class MaterialSurfaceQuery;
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
  bool editable = true, ghost = false;
  CanvasTransform world_transform{};bool world_transform_valid=false;
  std::optional<AffineTransform> affine_world;
};
struct AssetWork {
  std::atomic_bool cancelled{false};
  std::atomic<float> progress{0};
  bool held_weight=false; // Immutable job kind; set before queue publication.
};
struct AssetStatus { bool cancellable=false, cancelling=false; float progress=0; };
struct EditorState {
  uint64_t revision = 1, generation = 1, input_epoch = 1;
  bool dirty = false, stroking = false, menu = false;
  bool native_pending = false, native_accepting = false;
  uint64_t native_gesture = 0;
  size_t native_samples = 0;
  uint64_t active_object = 1;
  std::vector<ObjectInfo> objects;
  Tool tool = Tool::Clay;
  int menu_index = 0, menu_page = 0;
  float radius = .12f;
  CanvasTransform canvas{}, panel{};
  std::array<SpatialHand, 2> hands{};
  std::string message, error;
  std::filesystem::path project_path;
  std::shared_ptr<const ImportView> importing;
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
  Volume::SculptSettings sculpt_settings;
  CanvasTransform sculpt_transform{};
  bool can_configure_sculpt = false, sculpt_mask_visible = false;
  ProceduralSculptOptions procedural_options;
  bool can_change_resolution = false;
  double sculpt_voxel_mm = 30;
  WeightToolSettings weights;
  bool can_paint_weights = false;
  uint64_t weight_gesture = 0;
  size_t weight_samples = 0;
  bool can_voxel_remesh = false;
  bool can_export_avatar = false;
  float voxel_size_mm = 30;
  std::array<bool, 12> menu_enabled{true, true, true, true, true, true,
                                    true, true, true, true, true, true};
  PaintSettings paint;
  VrControlState controls;
  std::shared_ptr<const UvView> uv;
  std::shared_ptr<const RetopoView> retopology;
  bool can_retopologize = false;
  bool can_cut = false;
  std::shared_ptr<const CutView> cut;
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
  bool object_grab_mode=false, isolate_held=false, object_grabbing=false;
  uint64_t held_object=0, edit_authority=1;
  std::shared_ptr<const PolygonView> polygon, polygon_handoff;
  uint64_t polygon_object = 0, polygon_handoff_object = 0,
      polygon_handoff_generation = 0, polygon_handoff_revision = 0;
  PolygonControls polygon_controls;
  bool can_polygon = false, polygon_requires_conversion = false, polygon_has_source = false;
  uint32_t pending_exports = 0;
};
struct SurfaceRange {
  uint64_t object;
  size_t first, count;
  Material material;
  std::shared_ptr<const TextureMaps> textures{};
  std::shared_ptr<const CreatorAppearance> appearance{};
  size_t first_vertex = 0;
  std::shared_ptr<const SourceMaterialBindings> material_bindings{};
  bool source_frame = false, canonical_frame = false, source_normal_only = false; // Private prototype.
  CanvasTransform object_world{};bool world_frame_valid=false;
  std::optional<AffineTransform> affine_world;
  bool reflected = false; // CPU query geometry retains original corner order.
};
struct SculptMeshingStats {
  size_t objects_reused=0,local_updates=0,full_rebuilds=0,history_fallbacks=0,capacity_fallbacks=0;
  size_t rebuilt_tiles=0,reused_tiles=0,samples_scanned=0,cells_evaluated=0;
  double extraction_ms=0,materialize_ms=0,worker_ms=0;
};
struct MeshSnapshot {
  uint64_t revision = 0, generation = 0;
  Mesh mesh;
  std::vector<SurfaceRange> ranges;
  // Empty: ordinary material view. Otherwise one value per packed vertex;
  // -1: ordinary material, [0,1]: unlit selected-joint weight visualization.
  std::vector<float> selected_weights;
  // Immutable protection overlay: empty ordinary view, otherwise packed vertex
  // count with -1 for other objects and [0,1] protection on the active sculpt.
  std::vector<float> sculpt_masks;
  uint64_t weight_gesture = 0;
  size_t weight_samples = 0;
  uint64_t native_gesture = 0;
  size_t native_samples = 0;
  std::shared_ptr<const MaterialSurfaceQuery> picking;
  std::vector<std::array<float,4>> tangents;
  SculptMeshingStats sculpt_meshing;
  // Opaque, strongly owned packed-geometry dependency identity. Empty snapshots
  // retain conservative legacy upload behavior. Scalars/materials are separate.
  std::shared_ptr<const void> geometry_identity;
  uint64_t cut_session = 0, cut_request = 0, clipped_object = 0;
  CutPlane clip_plane;
  std::vector<Vec3> source_normals, canonical_normals;
  std::vector<std::array<float,4>> source_tangents;
  uint64_t normal_frame_layout_builds=0; // Worker diagnostic, never native state.
  std::vector<std::array<float,4>> source_colors; // Linear RGBA, identity when absent.
};
struct AssetSource {
  uint64_t generation = 0, revision = 0, object = 0, edit_authority = 0;
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
  friend struct DocumentNativeTestAccess;
  friend struct DocumentTransactionTestAccess;
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
                         CanvasTransform, Material, AssetSource expected = {});
  bool object_editable(uint64_t) const;
  void configure_object_grab(bool mode, bool isolate);
  void hold_object(uint64_t);
  void release_held_object(bool accept = true);
  void object_visibility(uint64_t, bool, AssetSource expected = {});
  bool begin_object_grab(uint64_t);
  void move_held_object(CanvasTransform local);
  void end_object_grab(bool accept = true);
  std::optional<uint64_t> pick_object(Vec3,Vec3,double material_time=0) const;
  void replace(Volume, bool clean = true);
  void seed();
  bool begin_native_stroke();
  bool sample_native_stroke(NativeStrokeSample);
  void end_native_stroke(bool accept = true);
  bool wait_native_stroke(std::chrono::milliseconds timeout = std::chrono::seconds(60)) const;
  bool begin_desktop();
  void stamp_desktop(Brush);
  void end_desktop();
  void cancel_desktop();
  std::optional<Vec3> raycast(Vec3, Vec3) const;
  std::optional<Vec3> raycast(Vec3, Vec3, double material_time) const;
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
  AssetSource begin_import() const; // Capture before opening native options.
  void import_asset(const std::filesystem::path &, const ImportSettings &, AssetSource expected = {});
  ImportSettings import_settings() const;
  void configure_import(ImportSettings);
  void open_import_browser(std::filesystem::path = {});
  void import_action(SpatialCommand);
  void remesh_to_sculpt(float world_voxel_mm, AssetSource expected = {});
  void bake_from_object(uint64_t source, ObjectBakeSettings = {}, AssetSource expected = {});
  void clear_bake(AssetSource expected = {});
  void cancel_asset_work();
  void cancel_background_work();
  AssetStatus asset_status() const;
  void begin_voxel_remesh();
  void edit_voxel_size(float world_voxel_mm);
  void end_voxel_remesh(bool accept);
  bool wait_asset(
      std::chrono::milliseconds timeout = std::chrono::seconds(60)) const;
  void begin_uv();
  void edit_uv(UvAction, float x = 0, float y = 0);
  void end_uv(bool accept);
  void begin_polygon(bool convert_source = false, AssetSource expected = {});
  void edit_polygon(PolygonAction);
  void configure_polygon(PolygonControls);
  void polygon_pointer(Vec3 origin, Vec3 direction, PolygonAction, bool additive = false);
  void end_polygon(bool accept);
  void restore_polygon_source(AssetSource expected = {});
  bool wait_polygon(std::chrono::milliseconds timeout = std::chrono::seconds(60)) const;
  void begin_retopology();
  void edit_retopology(RetopoAction);
  void end_retopology(bool accept);
  void retopology_distance(float mm);
  bool wait_retopology(std::chrono::milliseconds timeout = std::chrono::seconds(60)) const;
  void begin_cut();
  void edit_cut(CutSettings);
  void end_cut(bool accept);
  void begin_cut_parameter(VrParameter);
  void edit_cut_parameter(int component, float normalized);
  void end_cut_parameter(bool accept);
  bool wait_cut(std::chrono::milliseconds timeout = std::chrono::seconds(60)) const;
  bool wait_uv(std::chrono::milliseconds timeout = std::chrono::seconds(60)) const;
  void use_source();
  void cancel_spatial_menu();
  void use_mesh();
  void paint_settings(PaintSettings);
  void weight_settings(WeightToolSettings, std::optional<uint32_t> joint = {}, AssetSource expected = {});
  void bind_skin(SkinAuthoringOptions, AssetSource expected = {});
  void paint_command(SpatialCommand);
  void sculpt_command(SpatialCommand);
  void sculpt_settings(Volume::SculptSettings, AssetSource expected = {});
  void apply_procedural_sculpt(ProceduralSculptOptions, AssetSource expected = {});
  void change_sculpt_resolution(SculptResolutionOptions, AssetSource expected = {});
  void sculpt_mask_fill(float, AssetSource expected = {});
  void sculpt_mask_invert(AssetSource expected = {});
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
  std::future<void> save_async(const std::filesystem::path &);
  std::future<void> open_async(const std::filesystem::path &);
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
  class AuthorityTransition;
  class Transaction;
  Transaction *transaction_ = nullptr;
  std::deque<History> undo_, redo_;
  SpatialInput input_;
  CanvasNavigation navigation_, object_navigation_;
  CanvasTransform held_start_transform_{},held_preview_transform_{};
  CanvasTransform object_world_transform_locked(uint64_t) const;
  bool object_editable_locked(uint64_t) const;
  bool invalidate_object_authority_locked();
  void finish_object_grab_locked(bool accept, bool release);
  bool begin_object_grab_locked(uint64_t);
  bool held_weight_pending_locked() const;
  void configure_object_grab_locked(bool,bool);
  void move_held_object_locked(CanvasTransform);
  void update_object_grab_locked(const SpatialFrame &);
  void require_object_editable_locked(uint64_t) const;
  void require_authority_locked(AssetSource) const;
  EditorState state_;
  bool desktop_ = false, anchored_ = false;
  uint64_t last_volume_ = 0, last_paint_ = 0, content_ = 1, saved_content_ = 1,
           next_content_ = 2;
  std::filesystem::path path_;
  ImportSettings import_settings_;
  AssetSource import_source_{};
  std::shared_ptr<const ImportView> import_view_;
  std::atomic<uint64_t> import_request_{0};
  void import_asset_locked(const std::filesystem::path &,const ImportSettings &,AssetSource expected = {});
  void browse_import_locked(const std::filesystem::path &);
  void import_command_locked(SpatialCommand);
  bool import_command_enabled_locked(SpatialCommand) const;
  std::unique_ptr<RecoveryService> recovery_;
  std::filesystem::path recovery_root_;
  RecoverySource recovery_source_;
  std::vector<RecoveryCandidate> recovery_catalog_;
  uint64_t recovery_content_ = 0;
  std::string recovery_discovery_error_;
  void replace_locked(Scene, bool clean, std::filesystem::path = {}, std::string message = {});
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
  std::atomic<uint64_t> weight_epoch_{0};
  uint64_t weight_scheduled_epoch_ = 0;
  size_t weight_scheduled_samples_ = 0;
  std::shared_ptr<const MeshSnapshot> weight_base_;
  void restore_weight_view_locked(const std::shared_ptr<const MeshSnapshot> &);
  void invalidate_weight_view_locked(bool restore);
  float control_voxel_mm_ = 30;
  CutSettings cut_settings_, control_cut_settings_;
  std::vector<CutSettings> cut_undo_, cut_redo_;
  AssetSource cut_source_{};
  std::shared_ptr<const MeshSnapshot> cut_base_;
  std::shared_ptr<const BakeSource> cut_bake_source_;
  std::atomic<uint64_t> cut_session_{0}, cut_request_{0};
  struct CutJob { uint64_t session=0, request=0; AssetSource expected; CutSettings settings; };
  std::optional<CutJob> cut_job_;
  mutable std::mutex cut_mutex_;
  mutable std::condition_variable cut_changed_;
  void begin_cut_locked();
  void edit_cut_locked(CutSettings, bool remember = true);
  void end_cut_locked(bool);
  void cancel_cut_locked();
  void cut_history_locked(bool redo);
  void finish_cut_parameter_locked(bool accept);
  void cut_from_hand_locked();
  void cut_loop();
  float control_resolution_voxel_ = .03f;
  AssetSource polygon_source_{};
  std::shared_ptr<const Scene> polygon_initial_;
  std::atomic<uint64_t> polygon_session_{0};
  bool polygon_stroking_ = false;
  uint32_t polygon_owner_ = 0;
  std::atomic_bool polygon_accepting_{false};
  std::atomic<size_t> polygon_pending_{0};
  struct PolygonAccept {
    mutable std::mutex mutex;
    std::condition_variable changed;
    bool done = false, success = false, held_pose = false;
    std::shared_ptr<const Scene> before, completed, original;
    std::shared_ptr<AssetWork> work;
    AssetSource source;
    uint64_t content = 0, revision = 0, isolation_target = 0;
    RecoverySource recovery_source;
    CanvasTransform pose{};
    std::array<bool,2> grips{};
    std::exception_ptr error;
  };
  std::shared_ptr<PolygonAccept> polygon_accept_;
  bool polygon_closing_ = false, polygon_predicate_locked_ = false;
  std::array<bool,2> polygon_grip_latch_{};
  struct PolygonJob {
    uint64_t session = 0;
    std::shared_ptr<const Scene> initial;
    std::shared_ptr<PolygonAccept> acceptance;
    std::optional<PolygonAction> action;
    Vec3 origin{}, direction{};
    float radius = .015f, amount = .01f, maximum = 100;
    unsigned cuts = 1;
    bool accept = false, convert_source = false, additive = false;
  };
  mutable std::mutex polygon_mutex_;
  mutable std::condition_variable polygon_changed_;
  std::deque<PolygonJob> polygon_jobs_;
  void begin_polygon_locked(bool convert_source);
  void end_polygon_locked(bool accept);
  void cancel_polygon_locked(bool cancel_acceptance = true);
  void complete_polygon_accept(const std::shared_ptr<PolygonAccept> &, bool success,
      std::shared_ptr<const Scene> = {}, uint64_t content = 0, uint64_t revision = 0, std::exception_ptr = {});
  void drain_polygon_shutdown();
  bool held_polygon_pending_locked() const;
  void queue_polygon_locked(PolygonJob, bool coalescible = false);
  void polygon_pointer_locked(Vec3, Vec3, PolygonAction, bool xr, bool additive = false, uint32_t owner = 0);
  bool polygon_source_matches_locked() const;
  void rebase_polygon_transform_locked();
  void restore_polygon_source_locked(AssetSource);
  void polygon_loop();
  void process_polygon_pointer_locked(const SpatialFrame &);
  bool polygon_command_enabled_locked(SpatialCommand) const;
  void polygon_command_locked(SpatialCommand);
  float retopo_snap_mm_ = 50, control_retopo_mm_ = 50;
  AssetSource retopo_source_{};
  std::atomic<uint64_t> retopo_session_{0};
  bool retopo_stroking_ = false, retopo_sampled_ = false, retopo_accepting_ = false;
  size_t retopo_pending_ = 0;
  struct RetopoJob {
    uint64_t session = 0;
    std::shared_ptr<const Scene> initial;
    std::optional<RetopoAction> action;
    Vec3 point{};
    float snap_mm = 50;
    bool accept = false, coalescible = false;
  };
  mutable std::mutex retopo_mutex_;
  mutable std::condition_variable retopo_changed_;
  std::deque<RetopoJob> retopo_jobs_;
  void begin_retopology_locked();
  void end_retopology_locked(bool);
  void cancel_retopology_locked();
  void queue_retopology_locked(RetopoAction, Vec3 = {}, bool coalescible = false);
  void retopo_loop();
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
  std::optional<UvValidationJob> prepare_uv_schedule_locked();
  void invalidate_uv_locked();
  void uv_loop();
  void queue_uv_operation_locked(UvAction);
  void apply_uv_action_locked(UvAction, float = 0, float = 0);
  void queue_uv_locked(std::shared_ptr<const Mesh>);
  std::atomic<std::shared_ptr<const EditorState>> published_;
  std::atomic<std::shared_ptr<const MeshSnapshot>> mesh_;
  struct FileJob;
  void capture_polygon_file(FileJob &);
  struct NativeStroke {
    uint64_t epoch = 0;
    AssetSource source;
    CanvasTransform transform;
    std::optional<AffineTransform> affine_world;
    std::shared_ptr<const DeformationState> deformation;
    std::shared_ptr<const Scene> initial;
    std::shared_ptr<const MeshSnapshot> original_preview;
    PaintSettings paint;
    std::deque<NativeStrokeSample> samples;
    size_t submitted = 0;
    std::atomic_bool cancelled{false}, accepted{false};
    bool accepting = true, done = false;
    std::string error;
    std::shared_ptr<const Scene> completed;
    uint64_t revision = 0, content = 0;
    RecoverySource recovery_source;
  };
  std::shared_ptr<NativeStroke> native_stroke_;
  std::shared_ptr<const Scene> native_source_scene_;
  mutable std::mutex native_mutex_;
  mutable std::condition_variable native_changed_;
  std::deque<std::shared_ptr<NativeStroke>> native_jobs_;
  bool native_processing_ = false;
  std::atomic<uint64_t> native_epoch_{0};
  void native_loop();
  void end_native_locked(bool);
  void cancel_native_locked();
  void drain_native_shutdown();
  void capture_native_file(FileJob &);
  struct MeshJob {
    uint64_t revision, generation;
    Scene scene;
    bool weight_view = false;
    bool linked_weights = true;
    uint32_t weight_joint = 0;
    uint64_t weight_gesture = 0;
    std::vector<WeightBrush> weight_samples;
    WeightToolSettings weight_settings;
    bool sculpt_mask_view = false;
    uint64_t native_gesture = 0;
    size_t native_samples = 0;
  };
  mutable std::mutex mesh_mutex_;
  mutable std::condition_variable mesh_changed_;
  std::optional<MeshJob> pending_mesh_;
  mutable std::mutex input_mutex_;
  mutable std::condition_variable input_changed_;
  std::deque<std::pair<uint64_t, SpatialFrame>> inputs_;
  bool reset_input_ = false, processing_ = false, spatial_accepting_ = true;
  struct ExportWork {
    enum class Phase { Working, Cancelled, Committing, Finished };
    std::atomic<Phase> phase{Phase::Working};
    bool cancellable() const { return phase.load()==Phase::Working; }
    bool cancelled() const { return phase.load()==Phase::Cancelled; }
    bool cancel() {
      auto expected=Phase::Working;
      return phase.compare_exchange_strong(expected,Phase::Cancelled);
    }
    void begin_commit() {
      auto expected=Phase::Working;
      if(!phase.compare_exchange_strong(expected,Phase::Committing))
        throw std::runtime_error("Export cancelled");
    }
  };
  struct FileJob {
    Scene scene;
    std::filesystem::path path;
    uint64_t revision, generation, content;
    bool export_mesh;
    bool adopt_path = false;
    std::shared_ptr<std::promise<void>> completion{};
    bool open_project = false;
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
    std::shared_ptr<const MeshSnapshot> rollback_preview{};
    uint64_t retopo_session = 0;
    uint64_t cut_session = 0;
    std::shared_ptr<NativeStroke> native_capture{};
    std::shared_ptr<const Scene> capture_scene{};
    uint64_t edit_authority=0,isolation_target=0;
    bool discover_import=false;
    uint64_t import_request=0;
    std::shared_ptr<PolygonAccept> polygon_capture;
    std::shared_ptr<ExportWork> export_work;
  };
  // One running job plus the bounded eight-entry ordered file queue.
  // Protected by model_mutex_; explicit Cancel owns the phase transition.
  std::array<std::shared_ptr<ExportWork>,9> exports_{};
  std::atomic<std::shared_ptr<AssetWork>> asset_work_;
  mutable std::mutex file_mutex_;
  mutable std::condition_variable file_changed_;
  std::deque<FileJob> files_;
  bool files_stopping_ = false;
  std::atomic_bool stopping_{false};
  std::thread mesh_thread_, input_thread_, file_thread_, uv_thread_, retopo_thread_, cut_thread_, native_thread_, polygon_thread_;
  void discover_import_file(const FileJob &);
  void import_listing_error(const FileJob &,const std::exception &);
  void request_worker_stop();
  void publish_locked(bool force_geometry = false, bool checkpoint = true);
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
  void queue_export_locked(FileJob);
  void retire_export_locked(const std::shared_ptr<ExportWork> &) noexcept;
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
                                    std::shared_ptr<std::string>, std::shared_ptr<AssetWork> = {}, std::shared_ptr<bool> = {},
                                    std::shared_ptr<const MeshSnapshot> = {}, uint64_t retopo_session = 0,
                                    uint64_t cut_session = 0);
  std::vector<uint64_t> bake_sources_locked() const;
  bool bake_target_ready_locked() const;
  void queue_bake_locked(uint64_t, ObjectBakeSettings, AssetSource = {});
  void queue_clear_bake_locked(AssetSource = {});
  void cancel_asset_work_locked();
  void cancel_background_work_locked();
  bool sculpt_target_ready_locked() const;
  bool resolution_target_ready_locked() const;
  void queue_procedural_locked(ProceduralSculptOptions, AssetSource = {});
  void queue_resolution_locked(SculptResolutionOptions, AssetSource = {});
  void sculpt_settings_locked(Volume::SculptSettings, AssetSource = {});
  void queue_sculpt_mask_locked(std::optional<float>, AssetSource = {});
  bool skin_target_ready_locked() const;
  std::vector<uint64_t> skin_sources_locked(SkinAuthoringMode) const;
  void queue_bind_skin_locked(SkinAuthoringOptions, AssetSource = {});
  void queue_voxel_sculpt_locked(float, AssetSource);
};
} // namespace dome
