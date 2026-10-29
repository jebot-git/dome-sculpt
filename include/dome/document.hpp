// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "input.hpp"
#include "mesh.hpp"
#include "navigation.hpp"
#include <atomic>
#include <condition_variable>
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
struct EditorState {
  uint64_t revision = 1, generation = 1, input_epoch = 1;
  bool dirty = false, stroking = false, menu = false;
  Tool tool = Tool::Clay;
  int menu_index = 0;
  float radius = .12f;
  CanvasTransform canvas{}, panel{};
  std::array<SpatialHand, 2> hands{};
  std::string message, error;
  std::string font_file;
  int font_pixels = 16;
  bool light = false, toon = false;
  uint32_t accent = 0x6babff;
  int opacity = 255;
};
struct MeshSnapshot {
  uint64_t revision = 0, generation = 0;
  Mesh mesh;
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
  Volume volume_;
  SpatialInput input_;
  CanvasNavigation navigation_;
  EditorState state_;
  bool desktop_ = false, anchored_ = false;
  uint64_t last_volume_ = 0, saved_revision_ = 1;
  std::filesystem::path path_;
  std::atomic<std::shared_ptr<const EditorState>> published_;
  std::atomic<std::shared_ptr<const MeshSnapshot>> mesh_;
  struct MeshJob {
    uint64_t revision, generation;
    Volume volume;
  };
  mutable std::mutex mesh_mutex_;
  mutable std::condition_variable mesh_changed_;
  std::optional<MeshJob> pending_mesh_;
  mutable std::mutex input_mutex_;
  mutable std::condition_variable input_changed_;
  std::deque<std::pair<uint64_t, SpatialFrame>> inputs_;
  bool reset_input_ = false, processing_ = false;
  struct FileJob {
    Volume volume;
    std::filesystem::path path;
    uint64_t revision, generation;
    bool export_mesh;
  };
  std::mutex file_mutex_;
  std::condition_variable file_changed_;
  std::deque<FileJob> files_;
  bool files_stopping_ = false;
  std::atomic_bool stopping_{false};
  std::thread mesh_thread_, input_thread_, file_thread_;
  void publish_locked(bool force_geometry = false);
  void finish_locked();
  void process_locked(const SpatialFrame &);
  void file_locked(bool export_mesh);
  void mesh_loop();
  void input_loop();
  void file_loop();
};
} // namespace dome
