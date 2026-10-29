// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/creation.hpp"
#include "dome/rig-edit.hpp"
#include "dome/vrm.hpp"
#include "dome/gltf-export.hpp"
#include "dome/mesh-processing.hpp"
#include <unistd.h>
#include <utility>
namespace dome {
namespace {
ob::Vec3 obv(Vec3 p) { return {p.x, p.y, p.z}; }
Vec3 vec(ob::Vec3 p) { return {p.x, p.y, p.z}; }
uint64_t avatar_group(const Scene &scene, uint64_t object) {
  auto group = scene.object(object).group;
  auto path = scene.group_path(group);
  for (auto it = path.rbegin(); it != path.rend(); ++it)
    if (it->humanoid) return it->id;
  return group;
}
bool in_group(const Scene &scene, const SculptObject &object, uint64_t group) {
  auto path = scene.group_path(object.group);
  return group && std::any_of(path.begin(), path.end(), [group](const auto &g) { return g.id == group; });
}
} // namespace
Document::Document() : input_(volume()) {
  input_.radius = state_.radius;
  input_.allowed = [this](SpatialCommand command) {
    return command_enabled_locked(command);
  };
  input_.parameter_allowed = [this](VrParameter parameter) {
    return parameter_enabled_locked(parameter);
  };
  input_.control_accept = [this] {
    return !uv_ || (!uv_solving_ && !uv_accept_pending_ &&
                    (uv_->view().valid || uv_->view().validating));
  };
  input_.committed = [this] { record_stroke_locked(); };
  input_.backend = {[this] { return action_active_locked(); },
                    [this] { begin_action_locked(); },
                    [this] { cancel_action_locked(); },
                    [this](const Brush &b) { stamp_action_locked(b); },
                    [this] { return end_action_locked(); }};
  published_.store(std::make_shared<EditorState>(state_));
  mesh_.store(std::make_shared<MeshSnapshot>());
  try {
    mesh_thread_ = std::thread([this] { mesh_loop(); });
    input_thread_ = std::thread([this] { input_loop(); });
    file_thread_ = std::thread([this] { file_loop(); });
    uv_thread_ = std::thread([this] { uv_loop(); });
  } catch (...) {
    stopping_ = true;
    {
      std::lock_guard lock(file_mutex_);
      files_stopping_ = true;
    }
    mesh_changed_.notify_all();
    input_changed_.notify_all();
    file_changed_.notify_all();
    uv_changed_.notify_all();
    if (input_thread_.joinable())
      input_thread_.join();
    if (mesh_thread_.joinable())
      mesh_thread_.join();
    if (file_thread_.joinable())
      file_thread_.join();
    if (uv_thread_.joinable())
      uv_thread_.join();
    throw;
  }
}
Document::~Document() {
  stopping_ = true;
  mesh_changed_.notify_all();
  input_changed_.notify_all();
  uv_changed_.notify_all();
  input_thread_.join();
  {
    std::lock_guard lock(file_mutex_);
    files_stopping_ = true;
  }
  file_changed_.notify_all();
  mesh_thread_.join();
  file_thread_.join();
  uv_thread_.join();
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
bool Document::action_active_locked() const {
  if (weight_stroking_) return true;
  auto &o = scene_.active();
  return o.mesh_active ? o.painted->in_stroke() : volume().in_stroke();
}
bool Document::pending_locked() const {
  if (weight_stroking_) return !weight_samples_.empty();
  auto &o = scene_.active();
  return o.mesh_active ? o.painted->pending_changes()
                       : volume().pending_changes();
}
void Document::begin_action_locked() {
  state_.message.clear();
  if (state_.weights.enabled) {
    if (!state_.can_paint_weights || state_.asset_busy)
      throw std::logic_error("Skin weight painting is unavailable");
    auto mesh = mesh_.load();
    if (mesh->generation != state_.generation || mesh->revision != state_.revision)
      throw std::logic_error("Wait for the current weight view before painting");
    state_.weights.validate(scene_.active().deformation->skeleton->bones.size());
    weight_stroking_ = true; weight_samples_.clear();
    weight_source_ = {state_.generation, state_.revision, scene_.active().id};
    weight_stroke_settings_ = state_.weights;
    weight_joint_ = uint32_t(state_.rig_joint);
    return;
  }
  if (scene_.active().mesh_active)
    scene_.active().painted->begin_stroke();
  else
    volume().begin_stroke();
}
void Document::stamp_action_locked(const Brush &b) {
  if (weight_stroking_) {
    if (!finite(b.from) || !finite(b.to) || !std::isfinite(b.radius) || b.radius <= 0 ||
        !std::isfinite(b.pressure) || b.pressure < 0 || b.pressure > 1)
      throw std::invalid_argument("Invalid weight stroke geometry or pressure");
    if (!b.pressure) return;
    if (weight_samples_.size() >= 4096) throw std::length_error("Weight gesture sample budget exceeded");
    WeightBrush sample; sample.from = b.from; sample.to = b.to;
    sample.radius = b.radius; sample.pressure = b.pressure; sample.strength = 1;
    sample.joint = weight_joint_; weight_samples_.push_back(sample);
    return;
  }
  if (scene_.active().mesh_active)
    scene_.active().painted->stamp(b, state_.paint);
  else {
    size_t used = 0;
    for (auto &o : scene_.objects())
      if (o.id != scene_.active().id)
        used += std::max(o.volume->stored_samples(), o.volume->voxels().size());
    if (used > 4'000'000)
      throw std::length_error("Scene sculpt sample budget exceeded");
    volume().stamp(b, 4'000'000 - used);
  }
}
bool Document::end_action_locked() {
  if (weight_stroking_) {
    weight_stroking_ = false;
    auto samples = std::move(weight_samples_); weight_samples_.clear();
    if (samples.empty()) return false;
    auto settings = weight_stroke_settings_;
    auto object = weight_source_.object;
    auto report = std::make_shared<std::string>(); auto changed = std::make_shared<bool>(false);
    queue_scene_operation_locked("Paint skin weights",
      [samples = std::move(samples), settings, object, report, changed](Scene &scene, const auto &cancelled) {
        auto result = paint_scene_weights(scene, object, samples, settings, cancelled);
        *changed = result.changed != 0;
        *report = std::to_string(result.changed) + " vertices changed, " +
                  std::to_string(result.constrained) + " constrained";
        for (const auto &message : result.diagnostics) *report += "; " + message;
      }, weight_source_, report, changed);
    return false;
  }
  return scene_.active().mesh_active ? scene_.active().painted->end_stroke()
                                     : volume().end_stroke();
}
void Document::cancel_action_locked() {
  if (weight_stroking_) { weight_stroking_ = false; weight_samples_.clear(); return; }
  if (scene_.active().mesh_active)
    scene_.active().painted->cancel_stroke();
  else
    volume().cancel_stroke();
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
  History h;
  if (scene_.active().mesh_active)
    h.paint = scene_.active().painted;
  else
    h.sculpt = scene_.active().volume;
  auto volume_target = h.sculpt;
  auto paint_target = h.paint;
  record_locked(std::move(h));
  auto matches = [&](const History &entry) {
    return paint_target ? entry.paint == paint_target
                        : entry.sculpt == volume_target;
  };
  size_t available =
      paint_target ? paint_target->undo_count() : volume_target->undo_count();
  size_t count = std::count_if(undo_.begin(), undo_.end(), matches);
  for (auto it = undo_.begin(); count > available && it != undo_.end();) {
    if (matches(*it)) {
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
  last_paint_ =
      scene_.active().mesh_active ? scene_.active().painted->revision() : 0;
  ++state_.input_epoch;
  std::lock_guard lock(input_mutex_);
  inputs_.clear();
  reset_input_ = false;
  input_changed_.notify_all();
}
void Document::scene_changed_locked(Scene before) {
  try {
    // Structural commands change only native properties/ownership of already
    // validated source assets. Bulk source JSON checks stay on asset/file workers.
    scene_.validate(false);
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
  if (h.paint) {
    if (!(redo ? h.paint->redo() : h.paint->undo()))
      return false;
    for (auto &o : scene_.objects())
      if (o.painted == h.paint) {
        scene_.select(o.id);
        scene_.active().mesh_active = true;
        break;
      }
  } else if (h.sculpt) {
    if (!(redo ? h.sculpt->redo() : h.sculpt->undo()))
      return false;
    for (auto &o : scene_.objects())
      if (o.volume == h.sculpt) {
        scene_.select(o.id);
        scene_.active().mesh_active = false;
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
  CanvasTransform transform = scene_.world_transform(scene_.active().id);
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
  auto &active = scene_.active();
  uint64_t paint_revision = active.mesh_active ? active.painted->revision() : 0;
  bool changed = force || last_volume_ != volume().revision() ||
                 last_paint_ != paint_revision;
  if (changed) {
    last_volume_ = volume().revision();
    last_paint_ = paint_revision;
    ++state_.revision;
  }
  state_.active_object = scene_.active().id;
  state_.objects.clear();
  for (auto &o : scene_.objects())
    state_.objects.push_back({o.id, o.name, o.visible, o.locked, o.transform,
                              o.material, o.mesh_active, bool(o.painted)});
  if (uv_)
    control_changed_ = uv_changed(
        *uv_->view().mesh, scene_.object(control_object_).painted->mesh());
  schedule_uv_locked();
  if (uv_) {
    auto view = std::make_shared<UvView>(uv_->view());
    view->committing = uv_accept_pending_;
    view->solving = uv_solving_;
    if (view->solving) { view->can_undo = view->can_redo = false; view->valid = false; }
    if (view->committing) {
      view->valid = false;
      view->can_undo = view->can_redo = false;
      view->error = "Applying UVs...";
    }
    state_.uv = std::move(view);
  } else
    state_.uv.reset();
  state_.deformation = active.deformation;
  if (rig_object_ != active.id) {
    rig_object_ = active.id;
    state_.rig_joint = state_.morph_target = 0;
    state_.weights.locks.clear();
  }
  if (active.deformation) {
    auto &d = *active.deformation;
    if (d.skeleton) state_.rig_joint = std::min(state_.rig_joint, d.skeleton->bones.size() - 1);
    if (d.morphs && !d.morphs->targets.empty())
      state_.morph_target = std::min(state_.morph_target, d.morphs->targets.size() - 1);
  }
  state_.mesh_active = active.mesh_active;
  state_.can_paint_weights = active.mesh_active && active.painted && active.visible && !active.locked &&
      active.deformation && active.deformation->skeleton && active.deformation->skin;
  if (!state_.can_paint_weights) state_.weights.enabled = false;
  state_.can_voxel_remesh = parameter_enabled_locked(VrParameter::VoxelSize);
  auto avatar = avatar_group(scene_, active.id);
  state_.can_export_avatar = avatar && std::any_of(scene_.objects().begin(), scene_.objects().end(),
      [&](const auto &o) { return o.appearance && o.appearance->vrm_json && in_group(scene_, o, avatar); });
  state_.active_sculpt_layer = volume().active_layer();
  state_.sculpt_layers.clear();
  state_.sculpt_editable = volume().editable_layer();
  for (auto &l : volume().layers()) {
    state_.sculpt_layers.push_back(l.name);
    if (l.id == state_.active_sculpt_layer) {
      state_.sculpt_layer_name = l.name;
      state_.sculpt_layer_visible = l.visible;
      state_.sculpt_layer_locked = l.locked;
      state_.sculpt_layer_opacity = l.opacity;
    }
  }
  state_.active_layer = active.mesh_active ? active.painted->active_layer() : 0;
  state_.paint_layers.clear();
  state_.layer_visible = false;
  if (active.mesh_active)
    for (auto &l : active.painted->layers()) {
      state_.paint_layers.push_back(l.name);
      if (l.id == state_.active_layer)
        state_.layer_visible = l.visible;
    }
  state_.stroking = action_active_locked();
  state_.dirty = content_ != saved_content_ || pending_locked() ||
                 control_changed_ || state_.asset_busy;
  state_.tool = input_.tool();
  state_.menu = input_.menu_open();
  state_.menu_index = input_.menu_index();
  state_.menu_page = input_.menu_page();
  state_.can_undo = command_enabled_locked(SpatialCommand::Undo);
  state_.can_redo = command_enabled_locked(SpatialCommand::Redo);
  for (int i = 0; i < 12; ++i) {
    auto command = spatial_action(state_.menu_page, i);
    auto parameter = spatial_parameter(state_.menu_page, i);
    state_.menu_enabled[i] = parameter ? parameter_enabled_locked(*parameter)
                             : command ? command_enabled_locked(*command)
                                       : true;
  }
  checkpoint_locked();
  published_.store(std::make_shared<EditorState>(state_));
  {
    std::lock_guard lock(uv_mutex_);
    uv_changed_.notify_all();
  }
  if (changed) {
    std::lock_guard mesh_lock(mesh_mutex_);
    pending_mesh_.emplace(
        MeshJob{state_.revision, state_.generation, scene_.snapshot(), state_.weights.enabled,
                state_.weights.linked, uint32_t(state_.rig_joint)});
    mesh_changed_.notify_one();
  }
}
void Document::finish_locked() {
  // Starting another command may accept scalar property previews, but must
  // not implicitly execute an action such as creating a remeshed object.
  finish_control_locked(state_.controls.parameter != VrParameter::VoxelSize &&
                        state_.controls.parameter != VrParameter::RigRotation &&
                        state_.controls.parameter != VrParameter::MorphWeight);
  if (uv_accept_pending_)
    throw std::logic_error("Wait for the accepted UV edit to finish");
  if (action_active_locked() && end_action_locked())
    record_stroke_locked();
  desktop_ = false;
  input_.cancel();
  navigation_.reset();
}
void Document::replace(Scene next, bool clean) {
  next.validate();
  std::lock_guard lock(model_mutex_);
  replace_locked(std::move(next), clean);
}
void Document::replace_locked(Scene next, bool clean) {
  finish_control_locked(false);
  finish_locked();
  scene_ = std::move(next);
  bind_locked();
  undo_.clear();
  redo_.clear();
  path_.clear();
  ++state_.generation;
  if (recovery_) {
    recovery_source_ = {new_recovery_id(), state_.generation, {}};
    recovery_->set_source(recovery_source_);
    recovery_content_ = 0;
  }
  ++state_.input_epoch;
  {
    std::lock_guard queue_lock(input_mutex_);
    inputs_.clear();
    reset_input_ = true;
  }
  input_changed_.notify_all();
  state_.error.clear();
  anchored_ = false;
  state_.asset_busy = false;
  state_.recovery_busy = false;
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
  if (control_before_ || uv_ || state_.asset_busy || action_active_locked() ||
      input_.menu_open() || !scene_.active().visible ||
      scene_.active().locked ||
      (!scene_.active().mesh_active && !volume().editable_layer()))
    return false;
  input_.cancel();
  begin_action_locked();
  desktop_ = true;
  publish_locked();
  return true;
}
void Document::stamp_desktop(Brush b) {
  std::lock_guard lock(model_mutex_);
  if (!desktop_)
    throw std::logic_error("Desktop does not own the active stroke");
  auto t = scene_.world_transform(scene_.active().id);
  b.from = world_to_model(t, b.from);
  b.to = world_to_model(t, b.to);
  b.radius /= t.scale;
  b.blend /= t.scale;
  auto n = ob::rotate(ob::conjugate(t.orientation), obv(b.normal));
  b.normal = vec(n);
  stamp_action_locked(b);
  publish_locked();
}
void Document::end_desktop() {
  std::lock_guard lock(model_mutex_);
  if (desktop_) {
    if (end_action_locked())
      record_stroke_locked();
    desktop_ = false;
    input_.cancel();
    publish_locked();
  }
}
void Document::cancel_desktop() {
  std::lock_guard lock(model_mutex_);
  if (desktop_) {
    cancel_action_locked();
    desktop_ = false;
    input_.cancel();
    publish_locked();
  }
}
std::optional<Vec3> Document::raycast(Vec3 p, Vec3 d) const {
  std::lock_guard lock(model_mutex_);
  if (state_.weights.enabled) {
    auto mesh = mesh_.load();
    if (mesh->generation != state_.generation || mesh->revision != state_.revision || !mesh->picking) return {};
    auto hit = mesh->picking->raycast(p, d, 20);
    if (!hit) return {};
    auto vertex = mesh->mesh.indices.at(size_t(hit->triangle) * 3);
    return mesh->selected_weights.at(vertex) >= 0 ? std::optional<Vec3>(hit->position) : std::nullopt;
  }
  auto t = scene_.world_transform(scene_.active().id);
  if (!scene_.active().visible || scene_.active().locked)
    return {};
  auto direction = vec(ob::rotate(ob::conjugate(t.orientation), obv(d)));
  auto hit =
      scene_.active().mesh_active
          ? scene_.active().painted->raycast(world_to_model(t, p), direction,
                                             10 / t.scale)
          : volume().raycast(world_to_model(t, p), direction, 10 / t.scale);
  return hit ? std::optional<Vec3>(vec(ob::transform(t, obv(*hit))))
             : std::nullopt;
}
Vec3 Document::gradient(Vec3 p) const {
  std::lock_guard lock(model_mutex_);
  auto t = scene_.world_transform(scene_.active().id);
  return vec(
      ob::rotate(t.orientation,
                 obv(scene_.active().mesh_active
                         ? scene_.active().painted->normal(world_to_model(t, p))
                         : volume().gradient(world_to_model(t, p)))));
}
bool Document::undo() {
  std::lock_guard lock(model_mutex_);
  if (uv_accept_pending_ || uv_solving_)
    return false;
  if (uv_) {
    bool available = uv_->view().can_undo;
    uv_->edit(UvAction::Undo);
    publish_locked();
    return available;
  }
  if (state_.asset_busy)
    return false;
  finish_locked();
  bool changed = undo_locked(false);
  publish_locked();
  return changed;
}
bool Document::redo() {
  std::lock_guard lock(model_mutex_);
  if (uv_accept_pending_ || uv_solving_)
    return false;
  if (uv_) {
    bool available = uv_->view().can_redo;
    uv_->edit(UvAction::Redo);
    publish_locked();
    return available;
  }
  if (state_.asset_busy)
    return false;
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
  submit(std::span<const SpatialFrame>(&frame, 1));
}
void Document::submit(std::span<const SpatialFrame> frames) {
  std::lock_guard lock(input_mutex_);
  if (!spatial_accepting_)
    return;
  for (auto &frame : frames) {
    // A focus boundary cancels the unconfirmed action after preceding events
    // have been processed. Clearing here loses queued release/Apply actions.
    if (inputs_.size() >= 64) {
      inputs_.clear();
      reset_input_ = true;
    }
    inputs_.emplace_back(published_.load()->input_epoch, frame);
  }
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
  bool weight_view = std::exchange(state_.weights.enabled, false);
  publish_locked(weight_view);
}
void Document::paint_settings(PaintSettings settings) {
  settings.validate();
  std::lock_guard lock(model_mutex_);
  finish_locked();
  bool weights = std::exchange(state_.weights.enabled, false);
  state_.paint = settings;
  publish_locked(weights);
}
void Document::weight_settings(WeightToolSettings settings, std::optional<uint32_t> joint, AssetSource expected) {
  std::lock_guard lock(model_mutex_);
  finish_locked();
  if ((expected.generation && expected.generation != state_.generation) ||
      (expected.revision && expected.revision != state_.revision) ||
      (expected.object && expected.object != scene_.active().id))
    throw std::logic_error("The weight editor source changed; reopen the editor");
  if (state_.asset_busy || state_.recovery_busy)
    throw std::logic_error("Wait for pending authoring work before changing the weight tool");
  const auto &o = scene_.active();
  if (settings.enabled && !state_.can_paint_weights)
    throw std::logic_error("Select a visible, unlocked skinned mesh");
  size_t bones = o.deformation && o.deformation->skeleton ? o.deformation->skeleton->bones.size() : 1;
  if (!o.deformation) settings.locks.clear();
  settings.validate(bones);
  if (joint && *joint >= bones) throw std::out_of_range("Weight joint identity is out of range");
  if (settings.symmetry && (!o.deformation || !o.deformation->humanoid))
    throw std::logic_error("Symmetry requires explicit humanoid joint mapping");
  state_.weights = std::move(settings);
  if (joint) state_.rig_joint = *joint;
  publish_locked(true);
}
void Document::paint_command(SpatialCommand command) {
  std::lock_guard lock(model_mutex_);
  finish_locked();
  bool normal_paint = command == SpatialCommand::MakeMesh || command == SpatialCommand::EditSource ||
      (command >= SpatialCommand::PaintColor && command <= SpatialCommand::LayerOpacity);
  bool weight_view = normal_paint && std::exchange(state_.weights.enabled, false);
  command_locked(command);
  publish_locked(weight_view);
}
void Document::begin_uv() {
  std::lock_guard lock(model_mutex_);
  finish_locked();
  control_event_locked({VrControlEventType::Begin, VrParameter::UvEdit});
  desktop_ = true;
  publish_locked();
}
void Document::begin_voxel_remesh() {
  std::lock_guard lock(model_mutex_);
  finish_locked();
  control_event_locked({VrControlEventType::Begin, VrParameter::VoxelSize});
  desktop_ = true;
  state_.error.clear();
  publish_locked();
}
void Document::edit_voxel_size(float mm) {
  std::lock_guard lock(model_mutex_);
  if (!control_before_ || state_.controls.parameter != VrParameter::VoxelSize)
    throw std::logic_error("Open the voxel remesh controls first");
  auto &c = state_.controls;
  if (!std::isfinite(mm) || mm < c.minimum || mm > c.maximum)
    throw std::invalid_argument("Voxel size is outside the object's supported range");
  c.value[0] = state_.voxel_size_mm = mm;
  state_.error.clear();
  publish_locked();
}
void Document::end_voxel_remesh(bool accept) {
  std::lock_guard lock(model_mutex_);
  if (control_before_ && state_.controls.parameter != VrParameter::VoxelSize)
    throw std::logic_error("Another control owns the document");
  finish_control_locked(accept);
  desktop_ = false;
  publish_locked();
}
void Document::end_uv(bool accept) {
  std::lock_guard lock(model_mutex_);
  if (accept && uv_ && !uv_->view().valid && !uv_->view().validating)
    throw std::runtime_error(uv_->view().error);
  finish_control_locked(accept);
  if (!uv_accept_pending_)
    desktop_ = false;
  publish_locked();
}
void Document::queue_uv_locked(std::shared_ptr<const Mesh> mesh) {
  auto &o = scene_.active();
  FileJob job{scene_.snapshot(), {},       state_.revision,
              state_.generation, content_, false};
  job.uv_mesh = std::move(mesh);
  job.uv_source_vertices = uv_->view().source_vertices;
  job.paint_identity = o.painted;
  job.object = o.id;
  job.source_revision = o.painted->revision();
  job.uv_session = uv_session_;
  job.uv_desktop = desktop_;
  std::lock_guard lock(file_mutex_);
  if (files_.size() >= 8)
    throw std::runtime_error("File and asset work queue is full");
  files_.push_back(std::move(job));
  state_.asset_busy = true;
  state_.message = "Transferring paint to edited UVs...";
  state_.error.clear();
  file_changed_.notify_one();
}
void Document::use_source() { paint_command(SpatialCommand::EditSource); }
void Document::use_mesh() { paint_command(SpatialCommand::PaintColor); }
void Document::prepare_mesh(uint32_t resolution) {
  std::lock_guard lock(model_mutex_);
  finish_locked();
  prepare_locked(resolution);
  publish_locked();
}
bool Document::wait_asset(std::chrono::milliseconds timeout) const {
  std::unique_lock lock(file_mutex_);
  return file_changed_.wait_for(lock, timeout,
                                [&] { return !published_.load()->asset_busy; });
}
void Document::prepare_locked(uint32_t resolution) {
  if (resolution < 64 || resolution > 2048 || (resolution & (resolution - 1)))
    throw std::invalid_argument("Invalid paint resolution");
  if (state_.asset_busy)
    throw std::logic_error("A painting mesh is already being prepared");
  auto &o = scene_.active();
  if (o.painted && o.painted->has_paint_data())
    throw std::logic_error("The existing mesh has paint data; use the UV editor or transfer paint before rebuilding its geometry");
  if (o.deformation)
    throw std::logic_error("Explicitly remove or transfer rig data before replacing its rest mesh");
  if (o.locked || !o.visible)
    throw std::logic_error(
        "Show and unlock the object before preparing a painting mesh");
  FileJob job{scene_.snapshot(), {},       state_.revision,
              state_.generation, content_, false};
  job.prepare_mesh = true;
  job.object = o.id;
  job.source_revision = o.volume->revision();
  job.source_identity = o.volume;
  job.resolution = resolution;
  std::lock_guard lock(file_mutex_);
  if (files_.size() >= 8)
    throw std::runtime_error("File and asset work queue is full");
  files_.push_back(std::move(job));
  state_.asset_busy = true;
  state_.error.clear();
  state_.message = "Preparing painting mesh and UVs...";
  file_changed_.notify_one();
}
void Document::begin_spatial() {
  rebase_space({});
  {
    std::lock_guard lock(input_mutex_);
    spatial_accepting_ = true;
  }
  report_error({});
}
void Document::rebase_space(std::optional<CanvasTransform> old_from_new) {
  std::lock_guard lock(model_mutex_);
  if (!desktop_) {
    bool accepted_uv = uv_accept_pending_;
    if (!accepted_uv) finish_control_locked(false);
    input_.cancel();
    if (accepted_uv) input_.take_controls();
  }
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
    spatial_accepting_ = false;
  }
  // The producer has stopped. Drain its last transitions before cancelling an
  // unfinished action; in particular preserve an Apply just before shutdown.
  if (!wait_input())
    throw std::runtime_error("Spatial edit worker did not finish");
  std::lock_guard lock(model_mutex_);
  if (!desktop_) {
    bool accepted_uv = uv_accept_pending_;
    if (!accepted_uv) finish_control_locked(false);
    input_.cancel();
    if (accepted_uv) input_.take_controls();
    navigation_.reset();
    publish_locked();
  }
}
void Document::process_locked(const SpatialFrame &f) {
  if (desktop_)
    return;
  if (!f.focused || !f.head_valid || !ob::finite(f.head) ||
      !std::isfinite(f.time) || f.time < 0) {
    bool accepted_uv = uv_accept_pending_;
    if (!accepted_uv) finish_control_locked(false);
    input_.cancel();
    if (accepted_uv) input_.take_controls();
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
      if (action_active_locked())
        if (end_action_locked())
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
  auto object_canvas = ob::compose(state_.canvas, scene_.world_transform(scene_.active().id));
  input_.radius = state_.radius / object_canvas.scale;
  input_.edit_allowed =
      !state_.asset_busy && scene_.active().visible &&
      !scene_.active().locked &&
      (scene_.active().mesh_active || volume().editable_layer()) &&
      (!state_.weights.enabled || (mesh_.load()->generation == state_.generation && mesh_.load()->revision == state_.revision));
  input_.painting = scene_.active().mesh_active;
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
    if (state_.controls.parameter != VrParameter::None && h.tracked.aimValid) {
      if (auto hit = panel_ray(state_.panel, h.tracked.aim)) {
        s.panel_hit = true;
        s.panel_u = hit->u;
        s.panel_v = hit->v;
      }
    }
    input_.update(s);
  }
  for (auto event : input_.take_controls())
    control_event_locked(event);
  if (control_before_ && !uv_accept_pending_ && !input_.menu_open())
    finish_control_locked(false);
  if (!menu_was && input_.menu_open()) {
    state_.panel = {ob::transform(object_canvas, obv(input_.menu_anchor())),
                    f.head.orientation, 1};
    state_.panel.position = state_.panel.position +
                            ob::rotate(f.head.orientation, {0, .08f, -.18f});
  }
  if (auto c = input_.take_command()) {
    if (uv_ && !uv_->view().valid) {
      input_.resume_control(VrParameter::UvEdit);
      publish_locked();
      return;
    }
    finish_locked();
    command_locked(*c);
  }

  if (f.hands[0].undo || f.hands[1].undo) {
    if (uv_) {
      if (!uv_accept_pending_ && !uv_solving_) apply_uv_action_locked(UvAction::Undo);
    } else if (!state_.asset_busy) {
      finish_locked();
      undo_locked(false);
    }
  }
  if (f.hands[0].redo || f.hands[1].redo) {
    if (uv_) {
      if (!uv_accept_pending_ && !uv_solving_) apply_uv_action_locked(UvAction::Redo);
    } else if (!state_.asset_busy) {
      finish_locked();
      undo_locked(true);
    }
  }
  state_.hands = f.hands;
  publish_locked();
}
void Document::cancel_spatial_menu() {
  std::lock_guard lock(model_mutex_);
  finish_control_locked(false);
  if (!desktop_)
    input_.cancel();
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
        finish_control_locked(false);
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
          job->scene.preview_surfaces([&] { return stopping_.load(); });
      auto snapshot = std::make_shared<MeshSnapshot>();
      snapshot->revision = job->revision;
      snapshot->generation = job->generation;
      auto weighted = job->weight_view ? weight_objects(job->scene, job->scene.active().id, job->linked_weights)
                                      : std::vector<uint64_t>{};
      for (auto &o : objects) {
        if (o.deformation) o.mesh = o.preview_mesh();
        auto world = o.world_transform();
        size_t offset = snapshot->mesh.positions.size(),
               first = snapshot->mesh.indices.size();
        for (auto p : o.mesh.positions)
          snapshot->mesh.positions.push_back(
              vec(ob::transform(world, obv(p))));
        if (job->weight_view) {
          bool selected = std::find(weighted.begin(), weighted.end(), o.id) != weighted.end();
          for (size_t vertex = 0; vertex < o.mesh.positions.size(); ++vertex) {
            float value = selected ? 0.f : -1.f;
            if (selected) for (auto influence : o.deformation->skin->vertices.at(vertex))
              if (influence.joint == job->weight_joint) value = influence.weight;
            snapshot->selected_weights.push_back(value);
          }
        }
        for (auto n : o.mesh.normals)
          snapshot->mesh.normals.push_back(
              vec(ob::rotate(world.orientation, obv(n))));
        if (o.mesh.uv.empty())
          snapshot->mesh.uv.resize(snapshot->mesh.positions.size(), {0, 0});
        else
          snapshot->mesh.uv.insert(snapshot->mesh.uv.end(), o.mesh.uv.begin(),
                                   o.mesh.uv.end());
        for (auto index : o.mesh.indices)
          snapshot->mesh.indices.push_back(uint32_t(offset + index));
        snapshot->ranges.push_back(
            {o.id, first, o.mesh.indices.size(), o.material, o.textures, o.appearance, offset});
      }
      if (job->weight_view && !snapshot->mesh.indices.empty())
        snapshot->picking = std::make_shared<SurfaceQuery>(snapshot->mesh, [&] { return stopping_.load(); });
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
  if (recovery_) {
    recovery_source_.source_path = path;
    recovery_->set_source(recovery_source_);
  }
}
void Document::save(const std::filesystem::path &path) {
  auto completion = std::make_shared<std::promise<void>>();
  auto result = completion->get_future();
  {
    std::lock_guard lock(model_mutex_);
    finish_locked();
    if (state_.asset_busy)
      throw std::logic_error("Wait for mesh/UV preparation before saving");
    publish_locked();
    std::lock_guard file_lock(file_mutex_);
    if (files_.size() >= 8)
      throw std::runtime_error("File work queue is full");
    files_.push_back({scene_.snapshot(), path, state_.revision,
                      state_.generation, content_, false, true, completion});
    files_.back().recovery_source = recovery_source_;
  }
  file_changed_.notify_one();
  result.get();
}

std::future<void> Document::export_asset(const std::filesystem::path &path,
                                       AssetExportFormat format) {
  if (path.empty() || (format != AssetExportFormat::Glb && format != AssetExportFormat::Avatar))
    throw std::invalid_argument("Choose an export filename and supported format");
  auto completion = std::make_shared<std::promise<void>>();
  auto result = completion->get_future();
  std::lock_guard lock(model_mutex_);
  finish_locked();
  if (state_.asset_busy || state_.recovery_busy)
    throw std::logic_error("Wait for pending authoring work before exporting");
  if (format == AssetExportFormat::Avatar && !state_.can_export_avatar)
    throw std::logic_error("Select a source avatar group to export");
  FileJob job{scene_.snapshot(), path, state_.revision, state_.generation, content_, true};
  job.object = scene_.active().id; job.export_format = format; job.completion = completion;
  {
    std::lock_guard file_lock(file_mutex_);
    if (files_.size() >= 8) throw std::runtime_error("File work queue is full");
    files_.push_back(std::move(job));
  }
  state_.error.clear(); state_.message = "Exporting " + path.string();
  publish_locked();
  file_changed_.notify_one();
  return result;
}
void Document::file_locked(bool export_mesh, AssetExportFormat format) {
  if (state_.asset_busy)
    throw std::logic_error(
        "Wait for mesh/UV preparation before saving or exporting");
  auto path = path_;
  bool create_parent = path.empty();
  if (path.empty()) {
    const char *state = getenv("XDG_STATE_HOME"), *home = getenv("HOME");
    auto dir =
        state ? std::filesystem::path(state)
              : std::filesystem::path(home ? home : "/tmp") / ".local/state";
    dir /= "dome-sculpt";
    path = dir / ("Recovery-" + std::to_string(getpid()) + ".dsculpt");
  }
  if (export_mesh)
    path.replace_extension(format == AssetExportFormat::Avatar ? ".vrm" : ".glb");
  std::lock_guard lock(file_mutex_);
  if (files_.size() >= 8)
    throw std::runtime_error("File work queue is full");
  files_.push_back({scene_.snapshot(), path, state_.revision, state_.generation,
                    content_, export_mesh});
  files_.back().recovery_source = recovery_source_;
  files_.back().create_parent = create_parent;
  files_.back().export_format = format;
  files_.back().object = scene_.active().id;
  file_changed_.notify_one();
}
void Document::request_save(bool export_mesh) {
  std::lock_guard lock(model_mutex_);
  finish_locked();
  publish_locked();
  file_locked(export_mesh);
}
bool Document::parameter_enabled_locked(VrParameter parameter) const {
  if (parameter == VrParameter::WeightStrength || parameter == VrParameter::WeightValue)
    return !state_.asset_busy && !state_.recovery_busy && state_.can_paint_weights;
  if (parameter == VrParameter::RigRotation || parameter == VrParameter::MorphWeight) {
    const auto &o = scene_.active();
    if (state_.asset_busy || state_.recovery_busy || !o.visible || o.locked ||
        !o.mesh_active || !o.painted || !o.deformation) return false;
    const auto &d = *o.deformation;
    return parameter == VrParameter::RigRotation
        ? d.skeleton && d.skin && !d.skeleton->bones.empty()
        : d.morphs && !d.morphs->targets.empty();
  }
  if (parameter == VrParameter::VoxelSize) {
    const auto &o = scene_.active();
    return !state_.asset_busy && !state_.recovery_busy && o.visible && !o.locked &&
           scene_.objects().size() < 256 &&
           (o.mesh_active ? bool(o.painted) : o.volume->stored_samples() != 0);
  }
  if (parameter == VrParameter::SculptOpacity)
    return !state_.asset_busy && !scene_.active().mesh_active &&
           scene_.active().visible && !scene_.active().locked;
  if (parameter == VrParameter::UvEdit) {
    auto &o = scene_.active();
    return !state_.asset_busy && o.mesh_active && o.painted && o.visible &&
           !o.locked;
  }
  if (state_.asset_busy || parameter == VrParameter::None)
    return false;
  auto &o = scene_.active();
  if (parameter == VrParameter::LayerOpacity ||
      parameter == VrParameter::LayerOrder)
    return o.mesh_active && o.painted &&
           (parameter != VrParameter::LayerOrder ||
            o.painted->layers().size() > 1);
  return true;
}
void Document::finish_control_locked(bool accept) {
  if (!control_before_)
    return;
  if (state_.controls.parameter == VrParameter::RigRotation ||
      state_.controls.parameter == VrParameter::MorphWeight) {
    auto controls = state_.controls;
    if (accept && control_changed_) {
      RigEdit edit;
      if (controls.parameter == VrParameter::RigRotation)
        edit.joint = JointAdjustment{uint32_t(control_rig_index_),
                                     {controls.value[0], controls.value[1], controls.value[2]}, {}};
      else edit.morph = MorphWeightEdit{uint32_t(control_rig_index_), controls.value[0]};
      auto before = std::move(control_before_);
      control_before_.reset();
      try {
        auto object = control_object_;
        queue_scene_operation_locked("Edit rig and shapekeys",
          [object, edit](Scene &scene, const auto &cancelled) { edit_rig(scene, object, edit, cancelled); },
          control_source_, {});
      } catch (...) {
        control_before_ = std::move(before);
        throw;
      }
    } else control_before_.reset();
    control_changed_ = false;
    state_.controls = {};
    return;
  }
  if (state_.controls.parameter == VrParameter::VoxelSize) {
    if (accept) {
      auto before = std::move(control_before_);
      control_before_.reset();
      try {
        queue_voxel_sculpt_locked(state_.controls.value[0], control_source_);
      } catch (...) {
        control_before_ = std::move(before);
        throw;
      }
    } else {
      control_before_.reset();
      state_.voxel_size_mm = control_voxel_mm_;
    }
    control_changed_ = false;
    state_.controls = {};
    return;
  }
  if (uv_) {
    if (accept && uv_solving_) throw std::logic_error("Wait for the UV operation before applying");
    if (accept && uv_accept_pending_)
      return;
    if (accept && !uv_->view().valid && !uv_->view().validating)
      throw std::runtime_error(uv_->view().error);
    if (accept) {
      auto draft = uv_->view().mesh;
      auto &old = scene_.active().painted->mesh();
      bool changed = uv_changed(*draft, old);
      if (changed) {
        uv_->edit(UvAction::EndGesture);
        queue_uv_locked(draft);
        uv_accept_pending_ = true;
        invalidate_uv_locked();
        if (!desktop_)
          input_.resume_control(VrParameter::UvEdit);
        return;
      }
    }
    invalidate_uv_locked();
    ++uv_session_;
    uv_accept_pending_ = false;
    uv_.reset();
    state_.uv.reset();
    control_before_.reset();
    control_changed_ = false;
    state_.controls = {};
    return;
  }
  bool sculpt_control = state_.controls.parameter == VrParameter::SculptOpacity;
  bool changed = control_changed_;
  if (accept && changed)
    record_locked({{}, *control_before_, scene_});
  else
    scene_ = *control_before_;
  if (!accept) {
    state_.paint = control_settings_;
    state_.weights = control_weights_;
    state_.radius = control_radius_;
  }
  control_before_.reset();
  control_changed_ = false;
  state_.controls = {};
  if (sculpt_control) {
    input_.refresh_volume(volume());
    bound_volume_ = scene_.active().volume;
  }
  if (changed)
    publish_locked(true);
}
void Document::control_event_locked(VrControlEvent event) {
  if (event.type == VrControlEventType::Begin) {
    finish_control_locked(false);
    if (!parameter_enabled_locked(event.parameter))
      throw std::logic_error("This XR control is unavailable");
    control_before_ = scene_;
    control_settings_ = state_.paint;
    control_weights_ = state_.weights;
    control_radius_ = state_.radius;
    control_object_ = scene_.active().id;
    control_source_ = {state_.generation, state_.revision, control_object_};
    control_changed_ = false;
    auto &o = scene_.active();
    auto &c = state_.controls;
    c = {};
    c.parameter = event.parameter;
    if (event.parameter == VrParameter::UvEdit) {
      try {
        ++uv_session_;
        uv_.emplace(o.painted->mesh(), o.painted->resolution(), true);
        c.title = "UV editor";
      } catch (...) {
        control_before_.reset();
        state_.controls = {};
        throw;
      }
      state_.message.clear();
      return;
    }
    switch (event.parameter) {
    case VrParameter::WeightStrength:
      c.title = "Weight stroke strength"; c.value[0] = state_.weights.strength;
      break;
    case VrParameter::WeightValue:
      c.title = "Target weight"; c.value[0] = state_.weights.value;
      break;
    case VrParameter::RigRotation:
      control_rig_index_ = state_.rig_joint;
      c.title = o.deformation->skeleton->bones.at(control_rig_index_).name + " rotation delta";
      c.components = 3; c.minimum = -180; c.maximum = 180;
      break;
    case VrParameter::MorphWeight:
      control_rig_index_ = state_.morph_target;
      c.title = o.deformation->morphs->targets.at(control_rig_index_).name;
      c.minimum = -1; c.maximum = 1;
      c.value[0] = o.deformation->morph_weights.empty() ? 0 : o.deformation->morph_weights.at(control_rig_index_);
      break;
    case VrParameter::VoxelSize:
      control_voxel_mm_ = state_.voxel_size_mm;
      c.title = "Voxel remesh size (mm)";
      c.minimum = std::max(.01f, .01f * o.transform.scale);
      c.maximum = std::min(1000.f, 1000.f * o.transform.scale);
      c.logarithmic = true;
      c.value[0] = state_.voxel_size_mm =
          std::clamp(state_.voxel_size_mm, c.minimum, c.maximum);
      break;
    case VrParameter::BrushRadius:
      c.title = "Brush size";
      c.minimum = .005f;
      c.maximum = 2;
      c.logarithmic = true;
      c.value[0] = state_.radius;
      break;
    case VrParameter::Flow:
      c.title = "Paint flow";
      c.value[0] = state_.paint.flow;
      break;
    case VrParameter::Metallic:
      c.title = "Material metalness";
      c.value[0] = o.material.metallic;
      break;
    case VrParameter::Roughness:
      c.title = "Material roughness";
      c.value[0] = o.material.roughness;
      break;
    case VrParameter::Opacity:
      c.title = "Material opacity";
      c.value[0] = o.material.color[3];
      break;
    case VrParameter::BaseColor:
      c.title = "Material color";
      c.components = 4;
      c.value = o.material.color;
      break;
    case VrParameter::InkColor:
      c.title = "Brush color";
      c.components = 4;
      c.value = state_.paint.color;
      break;
    case VrParameter::InkEmission:
      c.title = "Emission color";
      c.components = 3;
      for (int i = 0; i < 3; ++i)
        c.value[i] = state_.paint.emission[i];
      break;
    case VrParameter::InkMetallic:
      c.title = "Paint metalness";
      c.value[0] = state_.paint.metallic;
      break;
    case VrParameter::InkRoughness:
      c.title = "Paint roughness";
      c.value[0] = state_.paint.roughness;
      break;
    case VrParameter::Mask:
      c.title = "Mask value";
      c.value[0] = state_.paint.mask;
      break;
    case VrParameter::SculptOpacity: {
      c.title = "Sculpt layer strength";
      c.value[0] = state_.sculpt_layer_opacity;
      o.volume = std::make_shared<Volume>(o.volume->geometry_snapshot());
      break;
    }
    case VrParameter::LayerOpacity:
    case VrParameter::LayerOrder: {
      auto &layers = o.painted->layers();
      auto it = std::find_if(layers.begin(), layers.end(), [&](auto &l) {
        return l.id == o.painted->active_layer();
      });
      c.title = event.parameter == VrParameter::LayerOpacity ? "Layer opacity"
                                                             : "Layer order";
      c.value[0] = event.parameter == VrParameter::LayerOpacity
                       ? it->opacity
                       : float(it - layers.begin());
      if (event.parameter == VrParameter::LayerOrder)
        c.maximum = float(layers.size() - 1);
      o.painted = o.painted->snapshot();
      break;
    }
    default:
      break;
    }
    state_.message.clear();
    return;
  }
  if (!control_before_ || event.parameter != state_.controls.parameter)
    return;
  if ((uv_accept_pending_ || uv_solving_) && event.type != VrControlEventType::Cancel)
    return;
  auto &c = state_.controls;
  if (uv_ && event.type != VrControlEventType::Confirm &&
      event.type != VrControlEventType::Cancel) {
    try {
      if (event.type == VrControlEventType::UvPick)
        apply_uv_action_locked(UvAction::Pick, event.value, event.second);
      else if (event.type == VrControlEventType::UvMove)
        apply_uv_action_locked(UvAction::Move, event.value, event.second);
      else if (event.type == VrControlEventType::UvCommand &&
               event.component >= 0 &&
               event.component <= int(UvAction::Stitch))
        apply_uv_action_locked(UvAction(event.component), event.value, event.second);
      state_.error.clear();
    } catch (const std::exception &e) {
      state_.error = e.what();
    }
    return;
  }
  if (event.type == VrControlEventType::Confirm ||
      event.type == VrControlEventType::Cancel) {
    if (event.type == VrControlEventType::Confirm && uv_ &&
        !uv_->view().valid && !uv_->view().validating) {
      state_.error = uv_->view().error;
      input_.resume_control(VrParameter::UvEdit);
      return;
    }
    finish_control_locked(event.type == VrControlEventType::Confirm);
    return;
  }
  if (event.component < 0 || event.component >= c.components ||
      !std::isfinite(event.value))
    return;
  c.selected = event.component;
  if (event.type == VrControlEventType::Select)
    return;
  float value = c.value[event.component];
  if (event.type == VrControlEventType::Absolute) {
    float t = std::clamp(event.value, 0.f, 1.f);
    value = c.logarithmic ? std::exp(std::log(c.minimum) +
                                     t * std::log(c.maximum / c.minimum))
                          : c.minimum + t * (c.maximum - c.minimum);
  } else
    value = c.logarithmic ? value * std::exp(event.value * .69314718056f)
                          : value + event.value * .5f * (c.maximum - c.minimum);
  value = std::clamp(value, c.minimum, c.maximum);
  if (value == c.value[event.component])
    return;
  c.value[event.component] = value;
  auto &o = scene_.object(control_object_);
  auto &before = control_before_->object(control_object_);
  bool scene_changed = false;
  switch (c.parameter) {
  case VrParameter::WeightStrength:
    state_.weights.strength = value;
    break;
  case VrParameter::WeightValue:
    state_.weights.value = value;
    break;
  case VrParameter::RigRotation:
    control_changed_ = c.value[0] != 0 || c.value[1] != 0 || c.value[2] != 0;
    break;
  case VrParameter::MorphWeight:
    control_changed_ = c.value[0] != (before.deformation->morph_weights.empty() ? 0 : before.deformation->morph_weights.at(control_rig_index_));
    break;
  case VrParameter::VoxelSize:
    state_.voxel_size_mm = value;
    break;
  case VrParameter::SculptOpacity: {
    auto &v = *o.volume;
    auto &ls = v.layers();
    auto l = *std::find_if(ls.begin(), ls.end(),
                           [&](auto &l) { return l.id == v.active_layer(); });
    v.layer_properties(l.id, l.name, l.visible, l.locked, value);
    control_changed_ =
        value != std::find_if(before.volume->layers().begin(),
                              before.volume->layers().end(),
                              [&](auto &l) {
                                return l.id == before.volume->active_layer();
                              })
                     ->opacity;
    scene_changed = true;
    break;
  }
  case VrParameter::BrushRadius:
    state_.radius = value;
    break;
  case VrParameter::Flow:
    state_.paint.flow = value;
    break;
  case VrParameter::InkColor:
    state_.paint.color[event.component] = value;
    break;
  case VrParameter::InkEmission:
    state_.paint.emission[event.component] = value;
    break;
  case VrParameter::InkMetallic:
    state_.paint.metallic = value;
    break;
  case VrParameter::InkRoughness:
    state_.paint.roughness = value;
    break;
  case VrParameter::Mask:
    state_.paint.mask = value;
    break;
  case VrParameter::BaseColor:
    o.material.color[event.component] = value;
    control_changed_ = o.material.color != before.material.color;
    scene_changed = true;
    break;
  case VrParameter::Metallic:
    o.material.metallic = value;
    control_changed_ = value != before.material.metallic;
    scene_changed = true;
    break;
  case VrParameter::Roughness:
    o.material.roughness = value;
    control_changed_ = value != before.material.roughness;
    scene_changed = true;
    break;
  case VrParameter::Opacity:
    o.material.color[3] = value;
    control_changed_ = value != before.material.color[3];
    scene_changed = true;
    break;
  case VrParameter::LayerOpacity: {
    auto &layers = o.painted->layers();
    auto it = std::find_if(layers.begin(), layers.end(), [&](auto &l) {
      return l.id == o.painted->active_layer();
    });
    auto &old = before.painted->layers();
    auto original = std::find_if(old.begin(), old.end(),
                                 [&](auto &l) { return l.id == it->id; });
    o.painted->layer_properties(it->name, it->visible, value);
    control_changed_ = value != original->opacity;
    scene_changed = true;
    break;
  }
  case VrParameter::LayerOrder: {
    auto &layers = o.painted->layers();
    auto it = std::find_if(layers.begin(), layers.end(), [&](auto &l) {
      return l.id == o.painted->active_layer();
    });
    int index = int(it - layers.begin()), target = int(std::lround(value));
    while (index != target) {
      int direction = target > index ? 1 : -1;
      o.painted->move_layer(direction);
      index += direction;
    }
    auto &old = before.painted->layers();
    control_changed_ = false;
    for (size_t i = 0; i < old.size(); ++i)
      control_changed_ |= old[i].id != o.painted->layers()[i].id;
    scene_changed = true;
    break;
  }
  default:
    break;
  }
  if (scene_changed)
    publish_locked(true);
}

bool Document::command_enabled_locked(SpatialCommand command) const {
  if (uv_solving_ && (command == SpatialCommand::Undo || command == SpatialCommand::Redo)) return false;
  if (command >= SpatialCommand::WeightToggle && command <= SpatialCommand::WeightLinked)
    return !state_.asset_busy && !state_.recovery_busy && !control_before_ && !action_active_locked() &&
           state_.can_paint_weights && (command != SpatialCommand::WeightSymmetry || bool(scene_.active().deformation->humanoid));
  if (command == SpatialCommand::ExportAvatar)
    return !state_.asset_busy && !state_.recovery_busy && !control_before_ && state_.can_export_avatar;
  if (command >= SpatialCommand::RigNextJoint && command <= SpatialCommand::RigResetPose) {
    auto parameter = command == SpatialCommand::RigNextMorph || command == SpatialCommand::RigPreviousMorph
        ? VrParameter::MorphWeight : VrParameter::RigRotation;
    return !control_before_ && !action_active_locked() && parameter_enabled_locked(parameter);
  }
  if (command >= SpatialCommand::CreateFemale && command <= SpatialCommand::Ungroup) {
    if (state_.asset_busy || state_.recovery_busy || control_before_ || action_active_locked()) return false;
    if (command <= SpatialCommand::CreateMakeHuman) return scene_.objects().size() <= 237 && scene_.groups().size() < 128;
    auto group = scene_.active().group;
    if (!group) return false;
    for (auto &o : scene_.objects())
      for (auto &g : scene_.group_path(o.group)) if (g.id == group && o.locked) return false;
    return true;
  }
  if (command >= SpatialCommand::RecoveryRefresh && command <= SpatialCommand::RecoveryRestore) {
    if (!recovery_ || state_.recovery_busy) return false;
    if (command == SpatialCommand::RecoveryRefresh) return true;
    if (command == SpatialCommand::RecoveryRestore)
      return !state_.dirty && !state_.asset_busy && !recovery_catalog_.empty();
    return recovery_catalog_.size() > 1;
  }
  auto &o = scene_.active();
  if (command >= SpatialCommand::SculptAdd &&
      command <= SpatialCommand::SculptOpacity) {
    if (state_.asset_busy || o.mesh_active || !o.visible || o.locked)
      return false;
    auto n = volume().layers().size();
    return command == SpatialCommand::SculptAdd ? n < 16
           : command == SpatialCommand::SculptDelete ||
                   command == SpatialCommand::SculptNext ||
                   command == SpatialCommand::SculptPrevious
               ? n > 1
               : true;
  }
  if (state_.asset_busy) {
    switch (command) {
    case SpatialCommand::Undo:
    case SpatialCommand::Redo:
    case SpatialCommand::Save:
    case SpatialCommand::Export:
    case SpatialCommand::MakeMesh:
    case SpatialCommand::EditSource:
    case SpatialCommand::PaintColor:
    case SpatialCommand::PaintMetal:
    case SpatialCommand::PaintRough:
    case SpatialCommand::PaintGlow:
    case SpatialCommand::PaintMask:
    case SpatialCommand::AddLayer:
    case SpatialCommand::DeleteLayer:
    case SpatialCommand::NextLayer:
    case SpatialCommand::LayerVisible:
    case SpatialCommand::LayerOpacity:
      return false;
    default:
      break;
    }
  }
  switch (command) {
  case SpatialCommand::Undo:
    return !undo_.empty() || pending_locked() || control_changed_;
  case SpatialCommand::Redo:
    return !redo_.empty() && !pending_locked() && !control_changed_;
  case SpatialCommand::Delete:
    return scene_.objects().size() > 1;
  case SpatialCommand::Next:
  case SpatialCommand::Previous:
    return scene_.objects().size() > 1;
  case SpatialCommand::NewSphere:
  case SpatialCommand::NewEmpty:
  case SpatialCommand::Duplicate:
    return scene_.objects().size() < 256;
  case SpatialCommand::MakeMesh:
    return o.visible && !o.locked && !o.deformation;
  case SpatialCommand::EditSource:
    return o.mesh_active;
  case SpatialCommand::PaintColor:
  case SpatialCommand::PaintMetal:
  case SpatialCommand::PaintRough:
  case SpatialCommand::PaintGlow:
  case SpatialCommand::PaintMask:
    return o.visible && !o.locked;
  case SpatialCommand::AddLayer:
    return o.mesh_active && o.painted && o.painted->layers().size() < 16;
  case SpatialCommand::DeleteLayer:
  case SpatialCommand::NextLayer:
    return o.mesh_active && o.painted && o.painted->layers().size() > 1;
  case SpatialCommand::LayerVisible:
  case SpatialCommand::LayerOpacity:
    return o.mesh_active && bool(o.painted);
  case SpatialCommand::Larger:
    return state_.radius < 2;
  case SpatialCommand::Smaller:
    return state_.radius > .005f;
  default:
    return true;
  }
}
void Document::sculpt_layer_properties(uint64_t object, uint64_t id,
                                       std::string name, bool visible,
                                       bool locked, float opacity) {
  std::lock_guard lock(model_mutex_);
  finish_locked();
  auto &o = scene_.object(object);
  if (state_.asset_busy || o.mesh_active || o.locked || !o.visible)
    throw std::logic_error("Sculpt layer properties unavailable");
  auto next = std::make_shared<Volume>(o.volume->geometry_snapshot());
  next->layer_properties(id, std::move(name), visible, locked, opacity);
  Scene before = scene_;
  o.volume = std::move(next);
  scene_changed_locked(std::move(before));
}
void Document::sculpt_command(SpatialCommand command) {
  std::lock_guard lock(model_mutex_);
  finish_locked();
  if (command < SpatialCommand::SculptAdd ||
      command > SpatialCommand::SculptOpacity ||
      !command_enabled_locked(command))
    throw std::logic_error("Sculpt layer command unavailable");
  command_locked(command);
  publish_locked();
}
void Document::command_locked(SpatialCommand command) {
  if (command >= SpatialCommand::WeightToggle && command <= SpatialCommand::WeightLinked) {
    if (!command_enabled_locked(command)) return;
    auto &settings = state_.weights;
    if (command == SpatialCommand::WeightToggle) settings.enabled = !settings.enabled;
    if (command == SpatialCommand::WeightLinked) settings.linked = !settings.linked;
    if (command == SpatialCommand::WeightSymmetry) settings.symmetry = !settings.symmetry;
    if (command == SpatialCommand::WeightMode) settings.mode = WeightBrushMode((int(settings.mode) + 1) % 4);
    if (command == SpatialCommand::WeightLock) {
      settings.locks.resize(scene_.active().deformation->skeleton->bones.size());
      settings.locks.at(state_.rig_joint) = !settings.locks.at(state_.rig_joint);
    }
    publish_locked(command == SpatialCommand::WeightToggle || command == SpatialCommand::WeightLinked);
    return;
  }
  if (command == SpatialCommand::ExportAvatar) {
    if (command_enabled_locked(command)) file_locked(true, AssetExportFormat::Avatar);
    return;
  }
  if (command >= SpatialCommand::RigNextJoint && command <= SpatialCommand::RigResetPose) {
    if (!command_enabled_locked(command)) return;
    const auto &d = *scene_.active().deformation;
    if (command == SpatialCommand::RigResetPose) {
      auto object = scene_.active().id;
      queue_scene_operation_locked("Reset grouped rig pose", [object](Scene &scene, const auto &cancelled) {
        RigEdit edit; edit.reset_pose = true; edit_rig(scene, object, edit, cancelled);
      }, {}, {});
    } else {
      bool morph = command == SpatialCommand::RigNextMorph || command == SpatialCommand::RigPreviousMorph;
      auto &index = morph ? state_.morph_target : state_.rig_joint;
      auto count = morph ? d.morphs->targets.size() : d.skeleton->bones.size();
      bool next = command == SpatialCommand::RigNextJoint || command == SpatialCommand::RigNextMorph;
      index = (index + count + (next ? 1 : count - 1)) % count;
      if (state_.weights.enabled && !morph) publish_locked(true);
    }
    return;
  }
  if (command >= SpatialCommand::CreateFemale && command <= SpatialCommand::Ungroup) {
    if (!command_enabled_locked(command)) return;
    if (command <= SpatialCommand::CreateMakeHuman) {
      std::string baseline = command == SpatialCommand::CreateFemale ? "vroid-beta-female"
          : command == SpatialCommand::CreateMale ? "vroid-beta-male" : "makehuman-hm08";
      queue_scene_operation_locked("Create humanoid", [baseline](Scene &scene, const auto &cancelled) {
        auto controls = humanoid_baseline_defaults(baseline);
        CanvasTransform transform{}; transform.position.y = -controls.height * .5f;
        append_humanoid(scene, controls, "Humanoid", transform, 256, cancelled);
      }, {}, {});
    } else {
      auto group = scene_.active().group;
      bool erase = command == SpatialCommand::DeleteGroup;
      queue_scene_operation_locked(erase ? "Delete avatar group" : "Ungroup avatar",
        [group, erase](Scene &scene, const auto &) { scene.erase_group(group, erase); }, {}, {});
    }
    return;
  }
  if (command >= SpatialCommand::RecoveryRefresh && command <= SpatialCommand::RecoveryRestore) {
    if (!command_enabled_locked(command)) return;
    if (command == SpatialCommand::RecoveryRefresh) refresh_recovery_locked();
    else if (command == SpatialCommand::RecoveryRestore)
      restore_recovery_locked(state_.recovery_selected);
    else {
      auto n = recovery_catalog_.size();
      state_.recovery_selected = (state_.recovery_selected + n +
          (command == SpatialCommand::RecoveryNext ? 1 : -1)) % n;
    }
    return;
  }
  if (command >= SpatialCommand::SculptAdd &&
      command <= SpatialCommand::SculptOpacity) {
    if (!command_enabled_locked(command))
      return;
    Scene before = scene_;
    auto next = std::make_shared<Volume>(volume().geometry_snapshot());
    auto ls = next->layers();
    auto i = std::find_if(ls.begin(), ls.end(), [&](auto &l) {
      return l.id == next->active_layer();
    });
    auto l = *i;
    switch (command) {
    case SpatialCommand::SculptAdd:
      next->add_layer("Sculpt " + std::to_string(ls.size() + 1));
      break;
    case SpatialCommand::SculptNext:
    case SpatialCommand::SculptPrevious:
      next->select_layer(ls[(size_t(i - ls.begin()) + ls.size() +
                             (command == SpatialCommand::SculptNext ? 1 : -1)) %
                            ls.size()]
                             .id);
      break;
    case SpatialCommand::SculptDelete:
      next->erase_layer(l.id);
      break;
    default:
      next->layer_properties(
          l.id, l.name,
          command == SpatialCommand::SculptVisible ? !l.visible : l.visible,
          command == SpatialCommand::SculptLock ? !l.locked : l.locked,
          command == SpatialCommand::SculptOpacity
              ? (l.opacity >= 1 ? .25f : std::min(1.f, l.opacity + .25f))
              : l.opacity);
      break;
    }
    scene_.active().volume = std::move(next);
    scene_changed_locked(std::move(before));
    return;
  }

  if (command == SpatialCommand::MakeMesh) {
    prepare_locked(512);
    return;
  }
  if (command == SpatialCommand::EditSource) {
    if (scene_.active().mesh_active) {
      Scene before = scene_;
      scene_.active().mesh_active = false;
      scene_changed_locked(std::move(before));
    }
    return;
  }
  if (command >= SpatialCommand::PaintColor &&
      command <= SpatialCommand::PaintMask) {
    state_.message.clear();
    state_.paint.channel =
        PaintChannel(int(command) - int(SpatialCommand::PaintColor));
    auto &o = scene_.active();
    if (!o.painted)
      prepare_locked(512);
    else if (!o.mesh_active) {
      Scene before = scene_;
      o.mesh_active = true;
      scene_changed_locked(std::move(before));
    }
    return;
  }
  switch (command) {
  case SpatialCommand::PaintRed:
    state_.paint.color = {.9f, .02f, .01f, 1};
    return;
  case SpatialCommand::PaintGreen:
    state_.paint.color = {.02f, .8f, .03f, 1};
    return;
  case SpatialCommand::PaintBlue:
    state_.paint.color = {.02f, .04f, .9f, 1};
    return;
  case SpatialCommand::PaintWhite:
    state_.paint.mask = 1;
    state_.paint.color = {1, 1, 1, 1};
    return;
  case SpatialCommand::PaintBlack:
    state_.paint.mask = 0;
    state_.paint.color = {0, 0, 0, 1};
    return;
  case SpatialCommand::PaintWarm:
    state_.paint.color = {.8f, .25f, .05f, 1};
    return;
  case SpatialCommand::FlowUp:
    state_.paint.flow = std::min(1.f, state_.paint.flow + .1f);
    return;
  case SpatialCommand::FlowDown:
    state_.paint.flow = std::max(.05f, state_.paint.flow - .1f);
    return;
  case SpatialCommand::ErasePaint:
    state_.paint.erase = !state_.paint.erase;
    return;
  default:
    break;
  }
  if (command == SpatialCommand::NextLayer ||
      command == SpatialCommand::AddLayer ||
      command == SpatialCommand::DeleteLayer ||
      command == SpatialCommand::LayerVisible ||
      command == SpatialCommand::LayerOpacity) {
    auto &o = scene_.active();
    if (!o.painted || !o.mesh_active)
      throw std::logic_error("Prepare or select a painting mesh first");
    if (command == SpatialCommand::NextLayer) {
      auto &layers = o.painted->layers();
      auto i = std::find_if(layers.begin(), layers.end(), [&](auto &l) {
        return l.id == o.painted->active_layer();
      });
      o.painted->select_layer(
          layers[(size_t(i - layers.begin()) + 1) % layers.size()].id);
      return;
    }
    Scene before = scene_;
    auto next = o.painted->snapshot();
    if (command == SpatialCommand::AddLayer)
      next->add_layer("Paint " +
                      std::to_string(o.painted->layers().size() + 1));
    else if (command == SpatialCommand::DeleteLayer)
      next->delete_layer();
    else {
      auto &layers = next->layers();
      auto &l = *std::find_if(layers.begin(), layers.end(), [&](auto &v) {
        return v.id == next->active_layer();
      });
      next->layer_properties(
          l.name,
          command == SpatialCommand::LayerVisible ? !l.visible : l.visible,
          command == SpatialCommand::LayerOpacity
              ? (l.opacity > .26f ? l.opacity - .25f : 1.f)
              : l.opacity);
    }
    o.painted = std::move(next);
    scene_changed_locked(std::move(before));
    return;
  }
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
    auto transform = scene_.world_transform(o.id);
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
      if (job->scene_operation) {
        auto cancelled = [&] {
          auto state = published_.load();
          return stopping_.load() || state->generation != job->generation ||
                 state->revision != job->revision || state->active_object != job->object;
        };
        job->scene_operation(job->scene, cancelled);
        if (cancelled()) throw std::runtime_error("Scene changed during " + job->operation_label);
        auto prepared = job->scene.snapshot();
        prepared.validate();
        std::lock_guard lock(model_mutex_);
        if (job->generation == state_.generation) {
          state_.asset_busy = false;
          if (job->content != content_ || job->revision != state_.revision ||
              job->object != scene_.active().id || action_active_locked() || control_before_)
            throw std::runtime_error("Scene changed during " + job->operation_label);
          bool changed = !job->operation_changed || *job->operation_changed;
          if (changed) {
            Scene before = scene_;
            scene_ = std::move(prepared);
            record_locked({{}, std::move(before), scene_});
            bind_locked();
          }
          state_.error.clear();
          state_.message = job->operation_label + " complete";
          if (job->operation_report && !job->operation_report->empty())
            state_.message += " | " + *job->operation_report;
          publish_locked(changed);
        }
        {
          std::lock_guard file_lock(file_mutex_);
          file_changed_.notify_all();
        }
        continue;
      }
      if (job->discover_recovery || job->restore_recovery) {
        if (job->discover_recovery) {
          auto found = RecoveryService::discover(job->path);
          std::lock_guard lock(model_mutex_);
          recovery_catalog_ = std::move(found.candidates);
          state_.recovery_candidates.clear();
          state_.recovery_selected = 0;
          for (auto &c : recovery_catalog_)
            state_.recovery_candidates.push_back(c.source.source_path.empty()
                ? "Untitled (" + c.source.id.substr(0, 8) + ")"
                : c.source.source_path.string());
          recovery_discovery_error_.clear();
          for (auto &error : found.errors) recovery_discovery_error_ += error + "\n";
          if (found.truncated) recovery_discovery_error_ += "Recovery scan limit reached";
          state_.recovery_busy = false;
          publish_locked();
        } else {
          auto recovered = RecoveryService::load(*job->restore_recovery);
          std::lock_guard lock(model_mutex_);
          if (job->generation != state_.generation || job->content != content_)
            throw std::runtime_error("Document changed while recovery was loading");
          replace_locked(std::move(recovered), false);
          state_.message = "Recovered as an unsaved project; save to a new path";
          state_.recovery_busy = false;
          publish_locked();
        }
        {
          std::lock_guard file_lock(file_mutex_);
          file_changed_.notify_all();
        }
        continue;
      }
      if (job->uv_mesh) {
        process_uv_transfer(*job);
        continue;
      }
      if (job->prepare_mesh) {
        auto &source = job->scene.object(job->object);
        auto mesh =
            surface_mesh(*source.volume, [&] { return stopping_.load(); });
        unwrap(mesh, job->resolution);
        auto painted =
            std::make_shared<PaintedMesh>(std::move(mesh), job->resolution);
        std::lock_guard lock(model_mutex_);
        if (job->generation == state_.generation) {
          auto it =
              std::find_if(scene_.objects().begin(), scene_.objects().end(),
                           [&](auto &o) { return o.id == job->object; });
          state_.asset_busy = false;
          if (it == scene_.objects().end() ||
              it->volume != job->source_identity ||
              it->volume->revision() != job->source_revision ||
              action_active_locked())
            throw std::runtime_error(
                "The source changed while its painting mesh was being "
                "prepared; retry on the current object");
          Scene before = scene_;
          auto &o = scene_.object(job->object);
          o.painted = std::move(painted);
          o.mesh_active = true;
          state_.message = "Painting mesh ready";
          state_.error.clear();
          scene_changed_locked(std::move(before));
        }
        {
          std::lock_guard file_lock(file_mutex_);
          file_changed_.notify_all();
        }
        continue;
      }
      if (job->create_parent) {
        std::filesystem::create_directories(job->path.parent_path());
        std::filesystem::permissions(job->path.parent_path(), std::filesystem::perms::owner_all);
      }
      std::string export_diagnostics;
      if (job->export_mesh) {
        if (job->export_format == AssetExportFormat::Avatar) {
          auto group = avatar_group(job->scene, job->object);
          if (!group) throw std::runtime_error("Select a grouped avatar to export");
          for (const auto &object : job->scene.objects())
            if (!in_group(job->scene, object, group)) job->scene.object(object.id).visible = false;
          auto report = export_vrm(job->scene.preview_surfaces(), job->path);
          for (size_t i = 0; i < std::min<size_t>(8, report.diagnostics.size()); ++i)
            export_diagnostics += " | " + report.diagnostics[i].substr(0, 200);
        } else {
          auto surfaces = job->scene.preview_surfaces();
          for (auto &surface : surfaces)
            if (surface.mesh.uv.empty()) unwrap(surface.mesh);
          GltfExportOptions options; options.recompute_changed_normals = true;
          auto report = export_glb(surfaces, job->path, options);
          for (size_t i = 0; i < std::min<size_t>(8, report.diagnostics.size()); ++i)
            export_diagnostics += " | " + report.diagnostics[i].substr(0, 200);
        }
      } else
        job->scene.save(job->path);
      std::lock_guard lock(model_mutex_);
      if (job->generation == state_.generation) {
        if (!job->export_mesh) {
          saved_content_ = job->content;
          if (recovery_ && job->recovery_source == recovery_source_)
            recovery_->discard(job->recovery_source, job->revision);
          if (job->adopt_path)
            path_ = job->path;
        }
        state_.error.clear();
        state_.message =
            std::string(job->export_mesh ? "Exported " : "Saved ") +
            job->path.string() + export_diagnostics;
        publish_locked();
      }
      if (job->completion)
        job->completion->set_value();
    } catch (const std::exception &e) {
      std::lock_guard lock(model_mutex_);
      if (job->discover_recovery) {
        state_.recovery_busy = false;
        recovery_discovery_error_ = e.what();
        publish_locked();
      }
      if (job->generation == state_.generation) {
        if (job->prepare_mesh || job->uv_mesh || job->restore_recovery || job->scene_operation)
          state_.asset_busy = false;
        if (job->discover_recovery || job->restore_recovery)
          state_.recovery_busy = false;
        if (job->uv_mesh && uv_ && uv_accept_pending_ &&
            uv_session_ == job->uv_session) {
          uv_accept_pending_ = false;
          uv_->validated(job->uv_mesh, e.what());
          invalidate_uv_locked();
          if (!job->uv_desktop)
            input_.resume_control(VrParameter::UvEdit);
        }
        state_.error = e.what();
        publish_locked();
      }
      {
        std::lock_guard file_lock(file_mutex_);
        file_changed_.notify_all();
      }
      if (job->completion)
        job->completion->set_exception(std::current_exception());
    }
  }
}
} // namespace dome
