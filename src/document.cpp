// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <unistd.h>
#include <utility>
namespace dome {
namespace {
ob::Vec3 obv(Vec3 p) { return {p.x, p.y, p.z}; }
Vec3 vec(ob::Vec3 p) { return {p.x, p.y, p.z}; }
} // namespace
Document::Document() : input_(volume_) {
  input_.radius = state_.radius;
  published_.store(std::make_shared<EditorState>(state_));
  mesh_.store(std::make_shared<MeshSnapshot>());
  try {
    mesh_thread_ = std::thread([this] { mesh_loop(); });
    input_thread_ = std::thread([this] { input_loop(); });
    file_thread_ = std::thread([this] { file_loop(); });
  } catch (...) {
    stopping_ = true;
    {
      std::lock_guard lock(file_mutex_);
      files_stopping_ = true;
    }
    mesh_changed_.notify_all();
    input_changed_.notify_all();
    file_changed_.notify_all();
    if (input_thread_.joinable())
      input_thread_.join();
    if (mesh_thread_.joinable())
      mesh_thread_.join();
    if (file_thread_.joinable())
      file_thread_.join();
    throw;
  }
}
Document::~Document() {
  stopping_ = true;
  mesh_changed_.notify_all();
  input_changed_.notify_all();
  input_thread_.join();
  {
    std::lock_guard lock(file_mutex_);
    files_stopping_ = true;
  }
  file_changed_.notify_all();
  mesh_thread_.join();
  file_thread_.join();
}
EditorSnapshot Document::frame() const {
  auto state = published_.load();
  auto mesh = mesh_.load();
  if (mesh->generation != state->generation) {
    static const auto empty = std::make_shared<MeshSnapshot>();
    mesh = empty;
  }
  return {state, mesh};
}
Volume Document::copy_volume() const {
  std::lock_guard lock(model_mutex_);
  return volume_.geometry_snapshot();
}
void Document::publish_locked(bool force) {
  bool changed = force || last_volume_ != volume_.revision();
  if (changed) {
    last_volume_ = volume_.revision();
    ++state_.revision;
  }
  state_.stroking = volume_.in_stroke();
  state_.dirty = state_.revision != saved_revision_;
  state_.tool = input_.tool();
  state_.menu = input_.menu_open();
  state_.menu_index = input_.menu_index();
  published_.store(std::make_shared<EditorState>(state_));
  if (changed) {
    std::lock_guard mesh_lock(mesh_mutex_);
    pending_mesh_.emplace(MeshJob{state_.revision, state_.generation,
                                  volume_.geometry_snapshot()});
    mesh_changed_.notify_one();
  }
}
void Document::finish_locked() {
  if (volume_.in_stroke())
    volume_.end_stroke();
  desktop_ = false;
  input_.cancel();
  navigation_.reset();
}
void Document::replace(Volume next, bool clean) {
  std::lock_guard lock(model_mutex_);
  finish_locked();
  volume_ = std::move(next);
  path_.clear();
  ++state_.generation;
  ++state_.input_epoch;
  {
    std::lock_guard queue_lock(input_mutex_);
    inputs_.clear();
    reset_input_ = true;
  }
  input_changed_.notify_all();
  state_.error.clear();
  anchored_ = false;
  publish_locked(true);
  if (clean) {
    saved_revision_ = state_.revision;
    publish_locked();
  }
}
void Document::seed() {
  Volume next(.03f);
  next.begin_stroke();
  Brush b;
  b.radius = .4f;
  b.blend = 0;
  next.stamp(b);
  next.end_stroke();
  replace(std::move(next));
}
bool Document::begin_desktop() {
  std::lock_guard lock(model_mutex_);
  if (volume_.in_stroke() || input_.menu_open())
    return false;
  input_.cancel();
  volume_.begin_stroke();
  desktop_ = true;
  publish_locked();
  return true;
}
void Document::stamp_desktop(Brush b) {
  std::lock_guard lock(model_mutex_);
  if (!desktop_)
    throw std::logic_error("Desktop does not own the active stroke");
  volume_.stamp(b);
  publish_locked();
}
void Document::end_desktop() {
  std::lock_guard lock(model_mutex_);
  if (desktop_) {
    volume_.end_stroke();
    desktop_ = false;
    input_.cancel();
    publish_locked();
  }
}
void Document::cancel_desktop() {
  std::lock_guard lock(model_mutex_);
  if (desktop_) {
    volume_.cancel_stroke();
    desktop_ = false;
    input_.cancel();
    publish_locked();
  }
}
std::optional<Vec3> Document::raycast(Vec3 p, Vec3 d) const {
  std::lock_guard lock(model_mutex_);
  return volume_.raycast(p, d, 10);
}
Vec3 Document::gradient(Vec3 p) const {
  std::lock_guard lock(model_mutex_);
  return volume_.gradient(p);
}
bool Document::undo() {
  std::lock_guard lock(model_mutex_);
  finish_locked();
  bool changed = volume_.undo();
  publish_locked();
  return changed;
}
bool Document::redo() {
  std::lock_guard lock(model_mutex_);
  finish_locked();
  bool changed = volume_.redo();
  publish_locked();
  return changed;
}
void Document::configure(Tool tool, float radius) {
  if (int(tool) < 0 || int(tool) > 3 || !std::isfinite(radius) ||
      radius < .005f || radius > 2)
    throw std::invalid_argument("Invalid tool settings");
  std::lock_guard lock(model_mutex_);
  input_.select_tool(tool);
  state_.radius = radius;
  input_.radius = radius;
  publish_locked();
}
void Document::submit(SpatialFrame frame) {
  std::lock_guard lock(input_mutex_);
  if (!frame.focused || inputs_.size() >= 64) {
    inputs_.clear();
    reset_input_ = true;
  }
  inputs_.emplace_back(published_.load()->input_epoch, std::move(frame));
  input_changed_.notify_one();
}
void Document::appearance(std::string file, int pixels, bool light,
                          uint32_t accent, int opacity) {
  std::lock_guard lock(model_mutex_);
  state_.font_file = std::move(file);
  state_.font_pixels = std::clamp(pixels, 8, 64);
  state_.light = light;
  state_.accent = accent;
  state_.opacity = std::clamp(opacity, 0, 255);
  publish_locked();
}
void Document::report_error(std::string error) {
  std::lock_guard lock(model_mutex_);
  state_.error = std::move(error);
  publish_locked();
}
void Document::shading(bool toon) {
  std::lock_guard lock(model_mutex_);
  state_.toon = toon;
  publish_locked();
}
void Document::begin_spatial() {
  rebase_space({});
  report_error({});
}
void Document::rebase_space(std::optional<CanvasTransform> old_from_new) {
  std::lock_guard lock(model_mutex_);
  if (!desktop_)
    input_.cancel();
  navigation_.reset();
  state_.hands = {};
  ++state_.input_epoch;
  {
    std::lock_guard queue_lock(input_mutex_);
    inputs_.clear();
    reset_input_ = false;
  }
  input_changed_.notify_all();
  if (old_from_new && ob::finite(*old_from_new)) {
    auto inverse = ob::inverse(*old_from_new);
    state_.canvas = ob::compose(inverse, state_.canvas);
    state_.panel = ob::compose(inverse, state_.panel);
  } else
    anchored_ = false;
  publish_locked();
}
void Document::finish_spatial() {
  {
    std::lock_guard lock(input_mutex_);
    inputs_.clear();
    reset_input_ = false;
  }
  if (!wait_input())
    throw std::runtime_error("Spatial edit worker did not finish");
  std::lock_guard lock(model_mutex_);
  if (!desktop_) {
    input_.cancel();
    navigation_.reset();
    publish_locked();
  }
}
void Document::process_locked(const SpatialFrame &f) {
  if (desktop_)
    return;
  if (!f.focused || !f.head_valid || !ob::finite(f.head) ||
      !std::isfinite(f.time) || f.time < 0) {
    input_.cancel();
    navigation_.reset();
    state_.hands = {};
    publish_locked();
    return;
  }
  if (!anchored_) {
    state_.canvas.position =
        f.head.position + ob::rotate(f.head.orientation, {0, -.12f, -.85f});
    state_.canvas.orientation = {};
    anchored_ = true;
  }
  std::array<TrackedHand, 2> hands{f.hands[0].tracked, f.hands[1].tracked};
  if (!input_.menu_open()) {
    if (auto transform = navigation_.update(hands, state_.canvas)) {
      if (volume_.in_stroke())
        volume_.end_stroke();
      input_.cancel();
      state_.canvas = *transform;
    }
    if (navigation_.grabbing()) {
      input_.cancel();
      state_.hands = f.hands;
      publish_locked();
      return;
    }
  }
  input_.radius = state_.radius / state_.canvas.scale;
  bool menu_was = input_.menu_open();
  for (int i = 0; i < 2; ++i) {
    const auto &h = f.hands[i];
    SpatialSample s;
    s.id = i + 1;
    s.sequence = f.sequence;
    s.time = f.time;
    s.focused = f.focused;
    s.tracked = h.tracked.valid;
    s.tip = world_to_model(state_.canvas, vec(h.tracked.grip.position));
    s.normal =
        vec(ob::rotate(ob::conjugate(state_.canvas.orientation),
                       ob::rotate(h.tracked.grip.orientation, {0, 0, 1})));
    s.pressure = std::clamp(h.tracked.trigger, 0.f, 1.f);
    s.trigger = s.pressure > .02f;
    s.joystick_press = h.joystick;
    s.stick_x = h.x;
    s.stick_y = h.y;
    s.kind = h.kind;
    input_.update(s);
  }
  if (!menu_was && input_.menu_open()) {
    state_.panel = {ob::transform(state_.canvas, obv(input_.menu_anchor())),
                    f.head.orientation, 1};
    state_.panel.position = state_.panel.position +
                            ob::rotate(f.head.orientation, {0, .08f, -.18f});
  }
  if (auto c = input_.take_command()) {
    finish_locked();
    switch (*c) {
    case SpatialCommand::Undo:
      volume_.undo();
      break;
    case SpatialCommand::Redo:
      volume_.redo();
      break;
    case SpatialCommand::Save:
      file_locked(false);
      break;
    case SpatialCommand::Export:
      file_locked(true);
      break;
    }
  }
  if (f.hands[0].undo || f.hands[1].undo) {
    finish_locked();
    volume_.undo();
  }
  if (f.hands[0].redo || f.hands[1].redo) {
    finish_locked();
    volume_.redo();
  }
  state_.hands = f.hands;
  publish_locked();
}
void Document::input_loop() {
  while (!stopping_) {
    SpatialFrame f;
    bool reset = false;
    uint64_t generation = 0;
    {
      std::unique_lock lock(input_mutex_);
      input_changed_.wait(lock, [&] { return stopping_ || !inputs_.empty(); });
      if (stopping_)
        break;
      generation = inputs_.front().first;
      f = std::move(inputs_.front().second);
      inputs_.pop_front();
      reset = std::exchange(reset_input_, false);
      processing_ = true;
    }
    {
      std::lock_guard lock(model_mutex_);
      try {
        if (generation == state_.input_epoch) {
          if (reset && !desktop_)
            input_.cancel();
          process_locked(f);
        }
      } catch (const std::exception &e) {
        if (!desktop_)
          input_.cancel();
        state_.error = e.what();
        publish_locked();
      }
    }
    {
      std::lock_guard lock(input_mutex_);
      processing_ = false;
    }
    input_changed_.notify_all();
  }
}
void Document::mesh_loop() {
  while (!stopping_) {
    std::optional<MeshJob> job;
    {
      std::unique_lock lock(mesh_mutex_);
      mesh_changed_.wait(
          lock, [&] { return stopping_ || pending_mesh_.has_value(); });
      if (stopping_)
        break;
      job = std::move(pending_mesh_);
      pending_mesh_.reset();
    }
    try {
      auto surface =
          surface_mesh(job->volume, [&] { return stopping_.load(); });
      auto snapshot = std::make_shared<MeshSnapshot>(
          MeshSnapshot{job->revision, job->generation, std::move(surface)});
      std::lock_guard lock(mesh_mutex_);
      if (published_.load()->generation == job->generation)
        mesh_.store(std::move(snapshot));
    } catch (const std::exception &e) {
      if (!stopping_) {
        std::lock_guard lock(model_mutex_);
        state_.error = e.what();
        publish_locked();
      }
    }
    mesh_changed_.notify_all();
  }
}
bool Document::wait_mesh(std::chrono::milliseconds timeout) const {
  auto wanted = published_.load();
  std::unique_lock lock(mesh_mutex_);
  return mesh_changed_.wait_for(lock, timeout, [&] {
    auto m = mesh_.load();
    return m->generation == wanted->generation &&
           m->revision >= wanted->revision;
  });
}
bool Document::wait_input(std::chrono::milliseconds timeout) const {
  std::unique_lock lock(input_mutex_);
  return input_changed_.wait_for(
      lock, timeout, [&] { return inputs_.empty() && !processing_; });
}
void Document::open(const std::filesystem::path &path) {
  auto next = Volume::load(path);
  replace(std::move(next));
  std::lock_guard lock(model_mutex_);
  path_ = path;
}
void Document::save(const std::filesystem::path &path) {
  Volume copy;
  uint64_t revision, generation;
  {
    std::lock_guard lock(model_mutex_);
    finish_locked();
    publish_locked();
    copy = volume_.geometry_snapshot();
    revision = state_.revision;
    generation = state_.generation;
  }
  copy.save(path);
  {
    std::lock_guard lock(model_mutex_);
    if (generation == state_.generation) {
      path_ = path;
      if (revision == state_.revision)
        saved_revision_ = revision;
      state_.error.clear();
      state_.message = "Saved " + path.string();
      publish_locked();
    }
  }
}
void Document::file_locked(bool export_mesh) {
  auto path = path_;
  if (path.empty()) {
    const char *state = getenv("XDG_STATE_HOME"), *home = getenv("HOME");
    auto dir =
        state ? std::filesystem::path(state)
              : std::filesystem::path(home ? home : "/tmp") / ".local/state";
    dir /= "dome-sculpt";
    std::filesystem::create_directories(dir);
    std::filesystem::permissions(dir, std::filesystem::perms::owner_all);
    path = dir / ("Recovery-" + std::to_string(getpid()) + ".dsculpt");
  }
  if (export_mesh)
    path.replace_extension(".glb");
  std::lock_guard lock(file_mutex_);
  if (files_.size() >= 8)
    throw std::runtime_error("File work queue is full");
  files_.push_back({volume_.geometry_snapshot(), path, state_.revision,
                    state_.generation, export_mesh});
  file_changed_.notify_one();
}
void Document::request_save(bool export_mesh) {
  std::lock_guard lock(model_mutex_);
  finish_locked();
  publish_locked();
  file_locked(export_mesh);
}
void Document::file_loop() {
  for (;;) {
    std::optional<FileJob> job;
    {
      std::unique_lock lock(file_mutex_);
      file_changed_.wait(lock,
                         [&] { return files_stopping_ || !files_.empty(); });
      if (files_.empty() && files_stopping_)
        return;
      job = std::move(files_.front());
      files_.pop_front();
    }
    try {
      if (job->export_mesh) {
        auto mesh = surface_mesh(job->volume);
        unwrap(mesh);
        export_glb(mesh, job->path);
      } else
        job->volume.save(job->path);
      std::lock_guard lock(model_mutex_);
      if (job->generation == state_.generation) {
        if (!job->export_mesh && job->revision == state_.revision)
          saved_revision_ = job->revision;
        state_.error.clear();
        state_.message =
            std::string(job->export_mesh ? "Exported " : "Saved ") +
            job->path.string();
        publish_locked();
      }
    } catch (const std::exception &e) {
      std::lock_guard lock(model_mutex_);
      state_.error = e.what();
      publish_locked();
    }
  }
}
} // namespace dome
