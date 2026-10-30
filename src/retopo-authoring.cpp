// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/retopo-authoring.hpp"
#include "text-utf8.hpp"
namespace dome {
namespace {
ob::Vec3 obv(Vec3 p) { return {p.x, p.y, p.z}; }
Vec3 vec(ob::Vec3 p) { return {p.x, p.y, p.z}; }
void stop(const std::function<bool()> &cancelled) {
  if (cancelled && cancelled()) throw std::runtime_error("Retopology cancelled");
}
}
RetopoWorkspace::RetopoWorkspace(const Scene &scene, const std::function<bool()> &cancelled) {
  const auto &active = scene.active();
  if (!active.visible || active.locked) throw std::logic_error("Show and unlock the object before retopology");
  source_object_ = active.retopology ? active.retopology->source : active.id;
  auto source_it = std::find_if(scene.objects().begin(), scene.objects().end(),
                                [&](const auto &object) { return object.id == source_object_; });
  if (source_it == scene.objects().end())
    throw std::logic_error("Retopology reference deleted or unavailable; restore or choose a source");
  const auto &source = *source_it;
  view_.transform = scene.world_transform(active.id);
  auto source_transform = scene.world_transform(source.id);
  auto mesh = source.mesh_active ? (source.deformation ? source.deformation->preview(source.painted->mesh())
                                                      : source.painted->mesh())
                                : surface_mesh(*source.volume, cancelled);
  if (mesh.indices.empty()) throw std::runtime_error("The retopology source has no surface");
  // Both projection and editable points use the active object's local space,
  // including differently grouped/transformed or currently posed sources.
  auto relative = ob::compose(ob::inverse(view_.transform), source_transform);
  for (auto &p : mesh.positions) p = vec(ob::transform(relative, obv(p)));
  for (auto &n : mesh.normals) n = vec(ob::rotate(relative.orientation, obv(n)));
  view_.source = std::make_shared<const SurfaceQuery>(mesh, cancelled);
  view_.topology = active.retopology
      ? std::make_shared<const RetopoMesh>(active.retopology->topology)
      : std::make_shared<const RetopoMesh>();
  view_.topology->validate(cancelled);
  original_ = view_.topology;
}
void RetopoWorkspace::remember(RetopoView before) {
  undo_.push_back(std::move(before));
  size_t entries = 0;
  auto retained = undo_.end();
  while (retained != undo_.begin()) {
    auto previous = std::prev(retained);
    const auto &t = *previous->topology;
    auto size = t.vertices().size() + t.edges().size() + t.faces().size();
    if (entries + size > 1'000'000 || size_t(undo_.end() - previous) > 128) break;
    entries += size; retained = previous;
  }
  undo_.erase(undo_.begin(), retained);
  redo_.clear();
}
void RetopoWorkspace::finish_gesture(bool cancel) {
  if (!gesture_) return;
  if (cancel) view_ = *gesture_;
  else if (gesture_->topology != view_.topology) remember(*gesture_);
  gesture_.reset(); moving_vertex_ = 0; sampled_ = false;
}
void RetopoWorkspace::edit(RetopoAction action, Vec3 point, float snap_mm,
                            const std::function<bool()> &cancelled) {
  stop(cancelled);
  if (!std::isfinite(snap_mm) || snap_mm < 1 || snap_mm > 200)
    throw std::invalid_argument("Retopology projection distance must be 1 to 200 mm");
  view_.snap_mm = snap_mm;
  auto before = view_;
  if (action == RetopoAction::BeginGesture) {
    finish_gesture(false); gesture_ = view_; sampled_ = false;
  } else if (action == RetopoAction::EndGesture || action == RetopoAction::CancelGesture) {
    finish_gesture(action == RetopoAction::CancelGesture);
  } else if (action == RetopoAction::Undo || action == RetopoAction::Redo) {
    if (gesture_) finish_gesture(true);
    else {
      auto &from = action == RetopoAction::Undo ? undo_ : redo_;
      auto &to = action == RetopoAction::Undo ? redo_ : undo_;
      if (!from.empty()) { to.push_back(view_); view_ = from.back(); from.pop_back(); }
    }
  } else if (action == RetopoAction::Mode || action == RetopoAction::PlaceMode || action == RetopoAction::MoveMode) {
    const bool moving = action == RetopoAction::Mode ? !view_.moving : action == RetopoAction::MoveMode;
    if (moving == view_.moving && !gesture_) return;
    finish_gesture(false); view_.moving = moving;
  } else if (action == RetopoAction::Clear) {
    finish_gesture(false); view_.selected.clear();
  } else {
    if (action != RetopoAction::Sample) finish_gesture(false);
    RetopoMesh draft = *view_.topology;
    auto selected = view_.selected;
    float maximum = snap_mm * .001f / view_.transform.scale;
    SurfaceQueryOptions options; options.cancelled = cancelled;
    auto project = [&](Vec3 p) {
      if (!finite(p)) throw std::invalid_argument("Invalid retopology point");
      auto hit = view_.source->closest(p, options);
      if (hit.distance > maximum) throw std::runtime_error("Point is outside the source projection distance");
      return hit.position;
    };
    if (action == RetopoAction::Sample) {
      if (!gesture_) throw std::logic_error("Begin a retopology gesture first");
      if (sampled_ && !view_.moving) return;
      point = project(point);
      if (!sampled_) {
        TopologyID nearest = 0;
        float distance = std::min(.01f, snap_mm * .00025f) / view_.transform.scale;
        for (const auto &[id, v] : draft.vertices()) {
          stop(cancelled);
          auto d = length(v.position - point);
          if (d <= distance) { nearest = id; distance = d; }
        }
        if (view_.moving) {
          if (!nearest) throw std::runtime_error("Touch an existing point to move it");
          if (draft.vertices().at(nearest).pinned) throw std::runtime_error("Unpin the point before moving it");
          selected = {nearest}; moving_vertex_ = nearest;
        } else {
          auto found = std::find(selected.begin(), selected.end(), nearest);
          if (nearest && found != selected.end()) selected.erase(found);
          else {
            if (selected.size() == 4) throw std::runtime_error("Make a face or clear the four selected points");
            if (!nearest) nearest = draft.add_vertex({point});
            selected.push_back(nearest);
          }
        }
      } else if (view_.moving && moving_vertex_) draft.move_vertex(moving_vertex_, point);
    } else if (action == RetopoAction::Face) {
      if (selected.size() < 3) throw std::runtime_error("Select three or four points in boundary order");
      Vec3 normal = cross(draft.vertices().at(selected[1]).position - draft.vertices().at(selected[0]).position,
                          draft.vertices().at(selected[2]).position - draft.vertices().at(selected[0]).position);
      Vec3 center{}; for (auto id : selected) center = center + draft.vertices().at(id).position;
      if (dot(normal, view_.source->closest(center / float(selected.size()), options).normal) < 0)
        std::reverse(selected.begin(), selected.end());
      draft.add_face(selected); selected.clear();
    } else if (action == RetopoAction::Wire || action == RetopoAction::Seam) {
      if (selected.size() != 2) throw std::runtime_error("Select two edge endpoints");
      if (action == RetopoAction::Wire) draft.add_edge(selected[0], selected[1]);
      else {
        TopologyID edge = 0; bool seam = false;
        for (auto &[id, e] : draft.edges())
          if ((e.a == selected[0] && e.b == selected[1]) || (e.b == selected[0] && e.a == selected[1]))
            { edge = id; seam = e.seam; break; }
        if (!edge) throw std::runtime_error("Connect the endpoints before marking a seam");
        draft.set_seam(edge, !seam);
      }
    } else if (action == RetopoAction::Pin) {
      if (selected.empty()) throw std::runtime_error("Select points to pin or unpin");
      bool pin = std::any_of(selected.begin(), selected.end(), [&](auto id) { return !draft.vertices().at(id).pinned; });
      for (auto id : selected) draft.set_pinned(id, pin);
    } else if (action == RetopoAction::Delete) {
      if (selected.empty()) throw std::runtime_error("Select points to delete with their incident faces and edges");
      draft.erase_vertices(selected);
      selected.clear();
    } else if (action == RetopoAction::Relax) {
      RetopoRelaxOptions relax; relax.maximum_snap_distance = maximum; relax.cancelled = cancelled;
      draft.relax(*view_.source, relax);
    } else if (action == RetopoAction::Snap) {
      if (selected.empty()) throw std::runtime_error("Select points to project");
      draft.project(selected, *view_.source, maximum, cancelled);
    }
    stop(cancelled);
    bool changed = draft.encode() != view_.topology->encode();
    if (changed) view_.topology = std::make_shared<const RetopoMesh>(std::move(draft));
    view_.selected = std::move(selected);
    if (action == RetopoAction::Sample) sampled_ = true;
    else if (changed) remember(std::move(before));
  }
  view_.can_undo = !undo_.empty() || (gesture_ && gesture_->topology != view_.topology);
  view_.can_redo = !redo_.empty() && !gesture_;
  view_.changed = view_.topology != original_;
  ++view_.revision;
}
uint64_t publish_retopology(Scene &scene, const RetopoMesh &topology, uint64_t source,
                            CanvasTransform transform, const std::function<bool()> &cancelled) {
  stop(cancelled);
  auto previous = scene.active();
  if (!text::valid_utf8(previous.name)) throw std::invalid_argument("Object names must be valid UTF-8");
  if (!previous.visible || previous.locked) throw std::logic_error("Retopology target changed");
  scene.object(source);
  auto triangles = topology.triangulate(cancelled);
  stop(cancelled);
  unwrap(triangles.mesh, 512, 4, cancelled);
  stop(cancelled);
  auto painted = std::make_shared<PaintedMesh>(std::move(triangles.mesh), 512);
  auto patch = std::make_shared<const RetopoPatch>(RetopoPatch{source, topology});
  // Construct all heavy/throwing data before modifying the detached scene.
  auto id = scene.add(text::utf8_prefix(previous.name,240) + " retopo", Volume{}, transform);
  auto &object = scene.object(id);
  object.painted = std::move(painted); object.mesh_active = true;
  object.material = previous.material; object.retopology = std::move(patch);
  scene.object(previous.id).visible = false;
  if (source != previous.id) scene.object(source).visible = false;
  return id;
}
} // namespace dome
