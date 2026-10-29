// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <unistd.h>
#include <utility>
namespace dome {
namespace {
ob::Vec3 obv(Vec3 p) { return {p.x, p.y, p.z}; }
Vec3 vec(ob::Vec3 p) { return {p.x, p.y, p.z}; }
} // namespace
Document::Document() : input_(volume()) {
  input_.radius = state_.radius;
  input_.committed = [this] { record_stroke_locked(); };
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
  return volume().geometry_snapshot();
}
Scene Document::copy_scene() const {
  std::lock_guard lock(model_mutex_);
  return scene_.snapshot();
}
void Document::record_locked(History entry) {
  entry.from = content_;
  entry.to = next_content_++;
  content_ = entry.to;
  redo_.clear();
  undo_.push_back(std::move(entry));
  while (undo_.size() > 128)
    undo_.pop_front();
}
void Document::record_stroke_locked() {
  auto target = scene_.active().volume;
  record_locked({target, {}, {}});
  size_t count = std::count_if(undo_.begin(), undo_.end(),
                               [&](auto &h) { return h.sculpt == target; });
  for (auto it = undo_.begin();
       count > target->undo_count() && it != undo_.end();) {
    if (it->sculpt == target) {
      it = undo_.erase(it);
      --count;
    } else
      ++it;
  }
}
void Document::bind_locked() {
  input_.bind(volume());
  bound_volume_ = scene_.active().volume;
  last_volume_ = volume().revision();
  ++state_.input_epoch;
  std::lock_guard lock(input_mutex_);
  inputs_.clear();
  reset_input_ = false;
  input_changed_.notify_all();
}
void Document::scene_changed_locked(Scene before) {
  try {
    scene_.validate();
  } catch (...) {
    scene_ = std::move(before);
    bind_locked();
    publish_locked();
    throw;
  }
  record_locked({{}, std::move(before), scene_});
  bind_locked();
  publish_locked(true);
}
bool Document::undo_locked(bool redo) {
  auto &source = redo ? redo_ : undo_, &dest = redo ? undo_ : redo_;
  if (source.empty())
    return false;
  auto &h = source.back();
  if (h.sculpt) {
    if (!(redo ? h.sculpt->redo() : h.sculpt->undo()))
      return false;
    for (auto &o : scene_.objects())
      if (o.volume == h.sculpt) {
        scene_.select(o.id);
        break;
      }
  } else
    scene_ = redo ? *h.after : *h.before;
  content_ = redo ? h.to : h.from;
  dest.push_back(std::move(h));
  source.pop_back();
  bind_locked();
  publish_locked(true);
  return true;
}
void Document::replace(Volume next, bool clean) {
  Scene scene;
  scene.active().volume = std::make_shared<Volume>(std::move(next));
  replace(std::move(scene), clean);
}
uint64_t Document::add_object(bool sphere) {
  std::lock_guard lock(model_mutex_);
  finish_locked();
  Scene before = scene_;
  Volume next(volume().voxel_size());
  if (sphere) {
    next.begin_stroke();
    Brush b;
    b.radius = .25f;
    b.blend = 0;
    next.stamp(b);
    next.end_stroke();
  }
  CanvasTransform transform = scene_.active().transform;
  transform.position.x += .6f;
  auto id = scene_.add(sphere ? "Clay sphere" : "Empty sculpt", std::move(next),
                       transform);
  scene_changed_locked(std::move(before));
  return id;
}
void Document::duplicate_object() {
  std::lock_guard lock(model_mutex_);
  finish_locked();
  Scene before = scene_;
  auto id = scene_.duplicate(scene_.active().id);
  auto &o = scene_.object(id);
  o.transform.position.x = std::min(100.f, o.transform.position.x + .6f);
  scene_changed_locked(std::move(before));
}
void Document::delete_object() {
  std::lock_guard lock(model_mutex_);
  finish_locked();
  Scene before = scene_;
  scene_.erase(scene_.active().id);
  scene_changed_locked(std::move(before));
}
void Document::select_object(uint64_t id) {
  std::lock_guard lock(model_mutex_);
  finish_locked();
  scene_.select(id);
  bind_locked();
  publish_locked();
}
void Document::object_properties(uint64_t id, std::string name, bool visible,
                                 bool locked, CanvasTransform transform,
                                 Material material) {
  material.validate();
  validate_transform(transform);
  std::lock_guard lock(model_mutex_);
  finish_locked();
  Scene before = scene_;
  auto &o = scene_.object(id);
  o.name = std::move(name);
  o.visible = visible;
  o.locked = locked;
  o.transform = transform;
  o.material = std::move(material);
  scene_changed_locked(std::move(before));
}
void Document::publish_locked(bool force) {
  bool changed = force || last_volume_ != volume().revision();
  if (changed) {
    last_volume_ = volume().revision();
    ++state_.revision;
  }
  state_.active_object = scene_.active().id;
  state_.objects.clear();
  for (auto &o : scene_.objects())
    state_.objects.push_back(
        {o.id, o.name, o.visible, o.locked, o.transform, o.material});
  state_.stroking = volume().in_stroke();
  state_.dirty = content_ != saved_content_ || volume().pending_changes();
  state_.tool = input_.tool();
  state_.menu = input_.menu_open();
  state_.menu_index = input_.menu_index();
  state_.menu_page = input_.menu_page();
  published_.store(std::make_shared<EditorState>(state_));
  if (changed) {
    std::lock_guard mesh_lock(mesh_mutex_);
    pending_mesh_.emplace(
        MeshJob{state_.revision, state_.generation, scene_.snapshot()});
    mesh_changed_.notify_one();
  }
}
void Document::finish_locked() {
  if (volume().in_stroke() && volume().end_stroke())
    record_stroke_locked();
  desktop_ = false;
  input_.cancel();
  navigation_.reset();
}
void Document::replace(Scene next, bool clean) {
  next.validate();
  std::lock_guard lock(model_mutex_);
  finish_locked();
  scene_ = std::move(next);
  bind_locked();
  undo_.clear();
  redo_.clear();
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
  content_ = next_content_++;
  if (clean)
    saved_content_ = content_;
  publish_locked(true);
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
  if (volume().in_stroke() || input_.menu_open() || !scene_.active().visible ||
      scene_.active().locked)
    return false;
  input_.cancel();
  volume().begin_stroke();
  desktop_ = true;
  publish_locked();
  return true;
}
void Document::stamp_desktop(Brush b) {
  std::lock_guard lock(model_mutex_);
  if (!desktop_)
    throw std::logic_error("Desktop does not own the active stroke");
  auto t = scene_.active().transform;
  b.from = world_to_model(t, b.from);
  b.to = world_to_model(t, b.to);
  b.radius /= t.scale;
  b.blend /= t.scale;
  auto n = ob::rotate(ob::conjugate(t.orientation), obv(b.normal));
  b.normal = vec(n);
  volume().stamp(b);
  publish_locked();
}
void Document::end_desktop() {
  std::lock_guard lock(model_mutex_);
  if (desktop_) {
    if (volume().end_stroke())
      record_stroke_locked();
    desktop_ = false;
    input_.cancel();
    publish_locked();
  }
}
void Document::cancel_desktop() {
  std::lock_guard lock(model_mutex_);
  if (desktop_) {
    volume().cancel_stroke();
    desktop_ = false;
    input_.cancel();
    publish_locked();
  }
}
std::optional<Vec3> Document::raycast(Vec3 p, Vec3 d) const {
  std::lock_guard lock(model_mutex_);
  auto t = scene_.active().transform;
  if (!scene_.active().visible || scene_.active().locked)
    return {};
  auto direction = vec(ob::rotate(ob::conjugate(t.orientation), obv(d)));
  auto hit = volume().raycast(world_to_model(t, p), direction, 10 / t.scale);
  return hit ? std::optional<Vec3>(vec(ob::transform(t, obv(*hit))))
             : std::nullopt;
}
Vec3 Document::gradient(Vec3 p) const {
  std::lock_guard lock(model_mutex_);
  auto t = scene_.active().transform;
  return vec(
      ob::rotate(t.orientation, obv(volume().gradient(world_to_model(t, p)))));
}
bool Document::undo() {
  std::lock_guard lock(model_mutex_);
  finish_locked();
  bool changed = undo_locked(false);
  publish_locked();
  return changed;
}
bool Document::redo() {
  std::lock_guard lock(model_mutex_);
  finish_locked();
  bool changed = undo_locked(true);
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
  state_.font_pixels = std::clamp(pixels, 4, 256);
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
      if (volume().in_stroke())
        if (volume().end_stroke())
          record_stroke_locked();
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
  auto object_canvas = ob::compose(state_.canvas, scene_.active().transform);
  input_.radius = state_.radius / object_canvas.scale;
  input_.edit_allowed = scene_.active().visible && !scene_.active().locked;
  bool menu_was = input_.menu_open();
  for (int i = 0; i < 2; ++i) {
    const auto &h = f.hands[i];
    SpatialSample s;
    s.id = i + 1;
    s.sequence = f.sequence;
    s.time = f.time;
    s.focused = f.focused;
    s.tracked = h.tracked.valid;
    s.tip = world_to_model(object_canvas, vec(h.tracked.grip.position));
    s.normal =
        vec(ob::rotate(ob::conjugate(object_canvas.orientation),
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
    state_.panel = {ob::transform(object_canvas, obv(input_.menu_anchor())),
                    f.head.orientation, 1};
    state_.panel.position = state_.panel.position +
                            ob::rotate(f.head.orientation, {0, .08f, -.18f});
  }
  if (auto c = input_.take_command()) {
    finish_locked();
    command_locked(*c);
  }

  if (f.hands[0].undo || f.hands[1].undo) {
    finish_locked();
    undo_locked(false);
  }
  if (f.hands[0].redo || f.hands[1].redo) {
    finish_locked();
    undo_locked(true);
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
      auto objects =
          job->scene.surfaces(false, [&] { return stopping_.load(); });
      auto snapshot = std::make_shared<MeshSnapshot>();
      snapshot->revision = job->revision;
      snapshot->generation = job->generation;
      for (auto &o : objects) {
        size_t offset = snapshot->mesh.positions.size(),
               first = snapshot->mesh.indices.size();
        for (auto p : o.mesh.positions)
          snapshot->mesh.positions.push_back(
              vec(ob::transform(o.transform, obv(p))));
        for (auto n : o.mesh.normals)
          snapshot->mesh.normals.push_back(
              vec(ob::rotate(o.transform.orientation, obv(n))));
        for (auto index : o.mesh.indices)
          snapshot->mesh.indices.push_back(uint32_t(offset + index));
        snapshot->ranges.push_back(
            {o.id, first, o.mesh.indices.size(), o.material});
      }
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
  auto next = Scene::load(path);
  replace(std::move(next));
  std::lock_guard lock(model_mutex_);
  path_ = path;
}
void Document::save(const std::filesystem::path &path) {
  auto completion = std::make_shared<std::promise<void>>();
  auto result = completion->get_future();
  {
    std::lock_guard lock(model_mutex_);
    finish_locked();
    publish_locked();
    std::lock_guard file_lock(file_mutex_);
    if (files_.size() >= 8)
      throw std::runtime_error("File work queue is full");
    files_.push_back({scene_.snapshot(), path, state_.revision,
                      state_.generation, content_, false, true, completion});
  }
  file_changed_.notify_one();
  result.get();
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
  files_.push_back({scene_.snapshot(), path, state_.revision, state_.generation,
                    content_, export_mesh});
  file_changed_.notify_one();
}
void Document::request_save(bool export_mesh) {
  std::lock_guard lock(model_mutex_);
  finish_locked();
  publish_locked();
  file_locked(export_mesh);
}
void Document::command_locked(SpatialCommand command) {
  switch (command) {
  case SpatialCommand::Undo:
    undo_locked(false);
    return;
  case SpatialCommand::Redo:
    undo_locked(true);
    return;
  case SpatialCommand::Save:
    file_locked(false);
    return;
  case SpatialCommand::Export:
    file_locked(true);
    return;
  case SpatialCommand::Larger:
    state_.radius = std::min(2.f, state_.radius * 1.25f);
    return;
  case SpatialCommand::Smaller:
    state_.radius = std::max(.005f, state_.radius * .8f);
    return;
  default:
    break;
  }
  Scene before = scene_;
  auto &o = scene_.active();
  switch (command) {
  case SpatialCommand::NewSphere:
  case SpatialCommand::NewEmpty: {
    Volume next(volume().voxel_size());
    if (command == SpatialCommand::NewSphere) {
      next.begin_stroke();
      Brush b;
      b.radius = .25f;
      b.blend = 0;
      next.stamp(b);
      next.end_stroke();
    }
    auto transform = o.transform;
    transform.position.x = std::min(100.f, transform.position.x + .6f);
    scene_.add(command == SpatialCommand::NewSphere ? "Clay sphere"
                                                    : "Empty sculpt",
               std::move(next), transform);
    break;
  }
  case SpatialCommand::Duplicate: {
    auto id = scene_.duplicate(o.id);
    auto &copy = scene_.object(id);
    copy.transform.position.x =
        std::min(100.f, copy.transform.position.x + .6f);
    break;
  }
  case SpatialCommand::Delete:
    scene_.erase(o.id);
    break;
  case SpatialCommand::Next:
  case SpatialCommand::Previous: {
    auto &objects = scene_.objects();
    auto it = std::find_if(objects.begin(), objects.end(),
                           [&](auto &v) { return v.id == o.id; });
    int index = int(it - objects.begin()), count = int(objects.size());
    scene_.select(
        objects[(index + count + (command == SpatialCommand::Next ? 1 : -1)) %
                count]
            .id);
    bind_locked();
    publish_locked();
    return;
  }
  case SpatialCommand::Visibility:
    o.visible = !o.visible;
    break;
  case SpatialCommand::Lock:
    o.locked = !o.locked;
    break;
  case SpatialCommand::Left:
    o.transform.position.x = std::max(-100.f, o.transform.position.x - .1f);
    break;
  case SpatialCommand::Right:
    o.transform.position.x = std::min(100.f, o.transform.position.x + .1f);
    break;
  case SpatialCommand::Center:
    o.transform.position = {};
    break;
  case SpatialCommand::Warm:
    o.material.color = {.58f, .30f, .15f, 1};
    break;
  case SpatialCommand::Blue:
    o.material.color = {.04f, .12f, .8f, 1};
    break;
  case SpatialCommand::White:
    o.material.color = {.8f, .8f, .8f, 1};
    break;
  case SpatialCommand::Dark:
    o.material.color = {.04f, .04f, .04f, 1};
    break;
  case SpatialCommand::MetalUp:
    o.material.metallic = std::min(1.f, o.material.metallic + .1f);
    break;
  case SpatialCommand::MetalDown:
    o.material.metallic = std::max(0.f, o.material.metallic - .1f);
    break;
  case SpatialCommand::RoughUp:
    o.material.roughness = std::min(1.f, o.material.roughness + .1f);
    break;
  case SpatialCommand::RoughDown:
    o.material.roughness = std::max(0.f, o.material.roughness - .1f);
    break;
  case SpatialCommand::Toon:
    o.material.toon = true;
    break;
  case SpatialCommand::Pbr:
    o.material.toon = false;
    break;
  case SpatialCommand::Opacity:
    o.material.color[3] =
        o.material.color[3] > .26f ? o.material.color[3] - .25f : 1.f;
    break;
  default:
    return;
  }
  scene_changed_locked(std::move(before));
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
        export_glb(job->scene.surfaces(true), job->path);
      } else
        job->scene.save(job->path);
      std::lock_guard lock(model_mutex_);
      if (job->generation == state_.generation) {
        if (!job->export_mesh) {
          saved_content_ = job->content;
          if (job->adopt_path)
            path_ = job->path;
        }
        state_.error.clear();
        state_.message =
            std::string(job->export_mesh ? "Exported " : "Saved ") +
            job->path.string();
        publish_locked();
      }
      if (job->completion)
        job->completion->set_value();
    } catch (const std::exception &e) {
      std::lock_guard lock(model_mutex_);
      if (job->generation == state_.generation) {
        state_.error = e.what();
        publish_locked();
      }
      if (job->completion)
        job->completion->set_exception(std::current_exception());
    }
  }
}
} // namespace dome
