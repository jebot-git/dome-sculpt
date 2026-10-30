// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/humanoid-creator.hpp"
#include <new>
namespace dome {
namespace {
bool heavy_uv(UvAction action) {
  return action == UvAction::Conformal || action == UvAction::Stitch ||
         action == UvAction::Relax || action == UvAction::Pack || action == UvAction::Seam;
}
}
void Document::edit_uv(UvAction action, float x, float y) {
  std::lock_guard lock(model_mutex_);
  try { apply_uv_action_locked(action, x, y); }
  catch (...) { publish_locked(); throw; }
  publish_locked();
}
void Document::apply_uv_action_locked(UvAction action, float x, float y) {
  if (!uv_) throw std::logic_error("Open the UV editor first");
  if (uv_accept_pending_) throw std::logic_error("Wait for the accepted UV edit to finish");
  if (uv_solving_) throw std::logic_error("Wait for UV processing, or cancel the UV editor");
  try {
    if (heavy_uv(action)) queue_uv_operation_locked(action);
    else uv_->edit(action, x, y);
    state_.error.clear();
  } catch (const std::exception &error) {
    state_.error = error.what();
    throw;
  }
}
void Document::queue_uv_operation_locked(UvAction operation) {
  uv_->edit(UvAction::EndGesture);
  invalidate_uv_locked();
  auto mesh = uv_->view().mesh;
  UvValidationJob job{++uv_request_, uv_session_, state_.generation, mesh,
                      operation, std::make_shared<UvWorkspace>(*uv_)};
  uv_solving_ = true;
  uv_scheduled_ = mesh;
  state_.message = operation == UvAction::Conformal ? "Unwrapping selected UV chart..."
      : operation == UvAction::Stitch ? "Stitching selected UV seam..." : "Processing UV charts...";
  std::lock_guard lock(uv_mutex_);
  uv_validation_ = std::move(job);
  uv_changed_.notify_all();
}
bool Document::wait_uv(std::chrono::milliseconds timeout) const {
  std::unique_lock lock(uv_mutex_);
  return uv_changed_.wait_for(lock, timeout, [&] {
    auto state = published_.load();
    return !state->uv || (!state->uv->validating && !state->uv->committing && !state->uv->solving);
  });
}
void Document::invalidate_uv_locked() {
  ++uv_request_;
  uv_solving_ = false;
  uv_scheduled_.reset();
  std::lock_guard lock(uv_mutex_);
  uv_validation_.reset();
  uv_changed_.notify_all();
}
void Document::schedule_uv_locked() {
  if (!uv_ || uv_accept_pending_ || uv_solving_) return;
  if (!uv_->view().validating) {
    if (!uv_scheduled_.expired()) invalidate_uv_locked();
    return;
  }
  if (uv_scheduled_.lock() == uv_->view().mesh) return;
  auto mesh = uv_->view().mesh;
  uv_scheduled_ = mesh;
  const bool initializing = !uv_->view().topology;
  if (initializing) {
    // Source lease is captured after publish_locked has accounted for any
    // just-finished stroke, without copying its geometry on this thread.
    control_source_ = {state_.generation, state_.revision, control_object_};
    uv_solving_ = true;
    state_.message = "Preparing UV charts...";
  }
  UvValidationJob job{++uv_request_, uv_session_, state_.generation, mesh, {},
      initializing ? std::make_shared<UvWorkspace>(*uv_) : nullptr};
  std::lock_guard lock(uv_mutex_);
  uv_validation_ = std::move(job);
  uv_changed_.notify_all();
}
void Document::uv_loop() {
  for (;;) {
    UvValidationJob job;
    {
      std::unique_lock lock(uv_mutex_);
      uv_changed_.wait(lock, [&] { return stopping_ || uv_validation_; });
      if (stopping_) return;
      job = std::move(*uv_validation_);
      uv_validation_.reset();
    }
    auto cancelled = [&] { return stopping_ || uv_request_ != job.request; };
    std::string error;
    bool completed = false;
    const bool workspace_job = bool(job.workspace);
    const bool initializing = workspace_job && !job.operation;
    try {
      if (workspace_job) {
        if (job.operation) job.workspace->edit(*job.operation, 0, 0, cancelled);
        else job.workspace->initialize(cancelled);
        auto result = job.workspace->view().mesh;
        error = validate_uv(*result, cancelled);
        job.workspace->validated(result, error);
        completed = true;
      } else error = validate_uv(*job.mesh, cancelled);
    } catch (const std::bad_alloc &) {
      if (cancelled()) continue;
      error = "Not enough memory to prepare UV charts; cancel and reopen the editor";
    } catch (const std::exception &failure) {
      if (cancelled()) continue;
      error = failure.what();
    }
    std::lock_guard lock(model_mutex_);
    if (cancelled() || !uv_ || uv_session_ != job.session ||
        state_.generation != job.generation) continue;
    if (initializing) {
      const auto &source = scene_.active();
      bool same_owner = source.painted && !job.mesh.owner_before(source.painted) &&
                        !source.painted.owner_before(job.mesh);
      if (!same_owner || source.id != control_object_ ||
          job.mesh.get() != &source.painted->mesh() ||
          state_.revision != control_source_.revision) {
        finish_control_locked(false);
        state_.error = "The UV source changed while loading; reopen the editor";
        state_.message.clear();
        publish_locked();
        continue;
      }
    }
    if (uv_->view().mesh != job.mesh) {
      if (workspace_job) {
        uv_solving_ = false;
        uv_scheduled_.reset();
        state_.error = "The UV draft changed during processing; retry";
        publish_locked();
      }
      continue;
    }
    if (workspace_job) {
      uv_solving_ = false;
      uv_scheduled_.reset();
      if (completed) {
        uv_ = std::move(*job.workspace);
        state_.error.clear();
        state_.message = initializing ? "UV charts ready" :
            "UV operation complete; Apply publishes UVs and transferred paint";
      } else {
        if (initializing) uv_->validated(job.mesh, error);
        state_.error = std::move(error);
        state_.message.clear();
      }
      publish_locked();
    } else if (uv_->validated(job.mesh, std::move(error))) publish_locked();
  }
}
void Document::process_uv_transfer(FileJob &job) {
  uint64_t request;
  {
    std::lock_guard lock(model_mutex_);
    if (job.generation != state_.generation || job.content != content_ ||
        !uv_ || uv_session_ != job.uv_session || !uv_accept_pending_ ||
        scene_.active().id != job.object)
      throw std::runtime_error("The UV session changed before paint transfer");
    const auto &current = scene_.object(job.object);
    if (current.painted != job.paint_identity ||
        current.painted->revision() != job.source_revision)
      throw std::runtime_error("The source mesh changed before UV transfer");
    request = uv_request_.load();
  }
  auto cancelled = [&] { return stopping_.load() || uv_request_.load() != request; };
  const auto &source = job.scene.object(job.object);
  const auto &old = source.painted->mesh();
  const auto &target = *job.uv_mesh;
  const auto &map = job.uv_source_vertices;
  if (!map || map->size() != target.positions.size() ||
      old.indices.size() != target.indices.size())
    throw std::runtime_error("UV source vertex/corner identity is missing; reopen the UV editor");
  bool uv_modified = false;
  for (size_t vertex = 0; vertex < map->size(); ++vertex) {
    if (cancelled()) throw std::runtime_error("UV transfer cancelled");
    auto original = map->at(vertex);
    if (original >= old.positions.size())
      throw std::runtime_error("UV source vertex identity is stale");
    auto p = old.positions[original], q = target.positions[vertex],
         n = old.normals[original], r = target.normals[vertex];
    if (p.x != q.x || p.y != q.y || p.z != q.z ||
        n.x != r.x || n.y != r.y || n.z != r.z)
      throw std::runtime_error("UV remap changed source geometry or normals");
  }
  for (size_t corner = 0; corner < old.indices.size(); ++corner) {
    auto vertex = target.indices[corner];
    if (vertex >= map->size() || map->at(vertex) != old.indices[corner])
      throw std::runtime_error("UV remap changed original triangle/corner identity");
    uv_modified |= old.uv[old.indices[corner]] != target.uv[vertex];
  }
  if (uv_modified && (source.appearance || source.painted->source_maps()))
    throw std::runtime_error("Original source UV edits require an explicit all-map bake and tangent-frame update; source images and draft were retained");
  // Metadata-only edits of unchanged authored UVs retain the original explicit
  // source-authored policy (shared/zero-area charts are not silently repaired).
  if (uv_modified || !source.painted->source_maps()) {
    auto error = validate_uv(target, cancelled);
    if (!error.empty()) throw std::runtime_error(error);
  }
  std::shared_ptr<const DeformationState> deformation;
  if (source.deformation) {
    source.deformation->validate(old);
    auto remapped = *source.deformation;
    remapped.source = morph_topology_id(target);
    size_t entries = remapped.pose.size() + remapped.morph_weights.size();
    if (remapped.skin) {
      SkinWeights skin;
      skin.vertices.reserve(map->size());
      for (auto original : *map) {
        if (cancelled()) throw std::runtime_error("UV skin remap cancelled");
        const auto &row = source.deformation->skin->vertices.at(original);
        if (row.size() > 8'000'000 - std::min(entries, size_t(8'000'000)))
          throw std::runtime_error("UV remap exceeds deformation resource budget");
        entries += row.size(); skin.vertices.push_back(row);
      }
      remapped.skin = std::move(skin);
    }
    if (remapped.morphs) {
      if (map->size() && remapped.morphs->targets.size() >
          (8'000'000 - std::min(entries, size_t(8'000'000))) / map->size())
        throw std::runtime_error("UV morph remap exceeds deformation resource budget");
      MorphSet morphs(target);
      for (const auto &original : source.deformation->morphs->targets) {
        MorphTarget morph{original.name, {}};
        morph.delta.reserve(map->size());
        for (auto vertex : *map) {
          if (cancelled()) throw std::runtime_error("UV morph remap cancelled");
          morph.delta.push_back(original.delta.at(vertex));
        }
        morphs.targets.push_back(std::move(morph));
      }
      remapped.morphs = std::move(morphs);
    }
    remapped.validate(target);
    deformation = std::make_shared<const DeformationState>(std::move(remapped));
  }
  std::shared_ptr<const CreatorAppearance> appearance;
  if (source.appearance) {
    if (source.appearance->source_vertices.size() != old.positions.size())
      throw std::runtime_error("Original avatar source vertex provenance is incomplete");
    auto updated = std::make_shared<CreatorAppearance>(*source.appearance);
    updated->source_vertices.clear(); updated->source_vertices.reserve(map->size());
    for (auto original : *map)
      updated->source_vertices.push_back(source.appearance->source_vertices.at(original));
    appearance = std::move(updated);
  }
  bool changed = uv_changed(target, old) || morph_topology_id(target) != morph_topology_id(old);
  auto painted = changed ? source.painted->reparameterize(target, cancelled) : source.painted;
  if (cancelled()) throw std::runtime_error("UV transfer cancelled");
  Scene prepared = job.scene;
  auto &candidate = prepared.object(job.object);
  candidate.painted = std::move(painted);
  candidate.deformation = std::move(deformation);
  candidate.appearance = std::move(appearance);
  prepared.validate();
  if (cancelled()) throw std::runtime_error("UV transfer cancelled");
  {
    std::lock_guard lock(model_mutex_);
    if (cancelled() || job.generation != state_.generation || job.content != content_ ||
        !uv_ || uv_session_ != job.uv_session || !uv_accept_pending_ ||
        scene_.active().id != job.object)
      throw std::runtime_error("The UV session changed during paint transfer");
    const auto &current = scene_.object(job.object);
    if (current.painted != job.paint_identity ||
        current.painted->revision() != job.source_revision)
      throw std::runtime_error("The source mesh changed during UV transfer");
    input_.complete_control();
    finish_control_locked(false);
    if (job.uv_desktop) desktop_ = false;
    if (changed) {
      Scene before = scene_;
      scene_ = std::move(prepared);
      record_locked({{}, std::move(before), scene_});
      bind_locked();
    }
    state_.asset_busy = false;
    state_.message = "UVs, paint and deformation identities updated";
    state_.error.clear();
    publish_locked(changed);
  }
  std::lock_guard lock(file_mutex_);
  file_changed_.notify_all();
}
} // namespace dome
