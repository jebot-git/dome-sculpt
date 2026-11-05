// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/retopo-authoring.hpp"
#include "dome/affine.hpp"
#include "text-utf8.hpp"
#include <limits>
#include <type_traits>
namespace dome {
static_assert(std::is_nothrow_move_assignable_v<RetopoView>);
namespace {
ob::Vec3 obv(Vec3 p) { return {p.x, p.y, p.z}; }
Vec3 vec(ob::Vec3 p) { return {p.x, p.y, p.z}; }
void stop(const std::function<bool()> &cancelled) {
  if (cancelled && cancelled()) throw std::runtime_error("Retopology cancelled");
}
TopologyID selected_edge(const RetopoMesh &mesh, std::span<const TopologyID> selected,
                         const std::function<bool()> &cancelled, bool required = true, bool *incident = nullptr) {
  if (selected.size() != 2) { if (!required) return 0; throw std::runtime_error("Select two boundary-edge endpoints before extruding"); }
  TopologyID edge = 0;
  for (const auto &[id, e] : mesh.edges()) {
    stop(cancelled);
    if ((e.a == selected[0] && e.b == selected[1]) || (e.b == selected[0] && e.a == selected[1])) edge = id;
  }
  if (!edge) { if (!required) return 0; throw std::runtime_error("Connect the selected endpoints before extruding"); }
  unsigned uses = 0;
  for (const auto &[id, f] : mesh.faces()) {
    (void)id; stop(cancelled);
    for (size_t i = 0; i < f.vertices.size(); ++i) {
      auto a = f.vertices[i], b = f.vertices[(i + 1) % f.vertices.size()];
      uses += (a == selected[0] && b == selected[1]) || (b == selected[0] && a == selected[1]);
    }
  }
  if (uses > 1) { if (!required) return 0; throw std::runtime_error("Choose a wire or boundary edge"); }
  if (incident) *incident = uses != 0;
  return edge;
}
Vec3 face_normal(const RetopoMesh &mesh, const RetopoFace &face) {
  Vec3 normal{};
  for (size_t i = 0; i < face.vertices.size(); ++i)
    normal += cross(mesh.vertices().at(face.vertices[i]).position,
                    mesh.vertices().at(face.vertices[(i + 1) % face.vertices.size()]).position);
  return normal;
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
  const bool affine = scene.has_affine_placement(active.id) ||
                      scene.has_affine_placement(source.id);
  // An affine workspace uses actual world metres, so its existing similarity
  // view can remain identity. No approximate TRS factorization is permitted.
  view_.transform = affine ? CanvasTransform{} : scene.world_transform(active.id);
  auto mesh = source.mesh_active ? (source.deformation ? source.deformation->preview(source.painted->mesh())
                                                      : source.painted->mesh())
                                : surface_mesh(*source.volume, cancelled);
  if (mesh.indices.empty()) throw std::runtime_error("The retopology source has no surface");
  // Both projection and editable points use the active object's local space,
  // including differently grouped/transformed or currently posed sources.
  if (affine) {
    const auto matrix = scene.world_matrix(source.id);
    for (auto &p : mesh.positions) {
      stop(cancelled);
      const auto q = matrix.point({p.x, p.y, p.z});
      p = {float(q[0]), float(q[1]), float(q[2])};
      if (!finite(p) || (q[0] != 0 && p.x == 0) ||
          (q[1] != 0 && p.y == 0) || (q[2] != 0 && p.z == 0))
        throw std::invalid_argument("Affine retopology source exceeds float geometry");
    }
    for (auto &n : mesh.normals) {
      stop(cancelled);
      const auto q = matrix.normal({n.x, n.y, n.z});
      const double magnitude = std::hypot(q[0], q[1], q[2]);
      if (!std::isfinite(magnitude) || magnitude == 0)
        throw std::invalid_argument("Affine retopology source has an invalid world normal");
      n = {float(q[0] / magnitude), float(q[1] / magnitude), float(q[2] / magnitude)};
    }
    if (matrix.orientation() == AffineOrientation::Reversing)
      for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        stop(cancelled);
        std::swap(mesh.indices[i + 1], mesh.indices[i + 2]);
        if (!mesh.uv_seams.empty()) std::swap(mesh.uv_seams[i], mesh.uv_seams[i + 2]);
      }
  } else {
    const auto relative = ob::compose(ob::inverse(view_.transform), scene.world_transform(source.id));
    for (auto &p : mesh.positions) p = vec(ob::transform(relative, obv(p)));
    for (auto &n : mesh.normals) n = vec(ob::rotate(relative.orientation, obv(n)));
  }
  view_.source = std::make_shared<const SurfaceQuery>(mesh, cancelled);
  if (active.retopology) {
    auto topology = active.retopology->topology;
    if (affine) topology.transform(scene.world_matrix(active.id), cancelled);
    view_.topology = std::make_shared<const RetopoMesh>(std::move(topology));
  } else view_.topology = std::make_shared<const RetopoMesh>();
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
  if (cancel) view_ = std::move(*gesture_);
  else if (gesture_->topology != view_.topology) remember(*gesture_);
  gesture_.reset(); moving_vertex_ = 0; sampled_ = false;
}
void RetopoWorkspace::edit(RetopoAction action, Vec3 point, float snap_mm,
                            const std::function<bool()> &cancelled) {
  stop(cancelled);
  if (!std::isfinite(snap_mm) || snap_mm < 1 || snap_mm > 200)
    throw std::invalid_argument("Retopology projection distance must be 1 to 200 mm");
  view_.snap_mm = snap_mm;
  if (action == RetopoAction::BeginGesture) {
    finish_gesture(false); gesture_ = view_; sampled_ = false;
  } else if (action == RetopoAction::EndGesture || action == RetopoAction::CancelGesture) {
    finish_gesture(action == RetopoAction::CancelGesture);
  } else if (action == RetopoAction::Undo || action == RetopoAction::Redo) {
    if (gesture_) finish_gesture(true);
    else {
      auto &from = action == RetopoAction::Undo ? undo_ : redo_;
      auto &to = action == RetopoAction::Undo ? redo_ : undo_;
      if (!from.empty()) { auto next = from.back(); to.push_back(view_); view_ = std::move(next); from.pop_back(); }
    }
  } else if (action == RetopoAction::Mode || action == RetopoAction::PlaceMode || action == RetopoAction::MoveMode || action == RetopoAction::ExtrudeMode) {
    bool moving = action == RetopoAction::MoveMode, extruding = action == RetopoAction::ExtrudeMode;
    if (action == RetopoAction::Mode) {
      moving = !view_.moving && !view_.extruding;
      extruding = view_.moving && selected_edge(*view_.topology, view_.selected, cancelled, false) != 0;
    }
    if (extruding) selected_edge(*view_.topology, view_.selected, cancelled);
    if (moving == view_.moving && extruding == view_.extruding && !gesture_) return;
    finish_gesture(false); view_.moving = moving; view_.extruding = extruding;
  } else if (action == RetopoAction::Clear) {
    finish_gesture(false); view_.selected.clear(); view_.extruding = false;
  } else {
    auto before = view_;
    if (action != RetopoAction::Sample) finish_gesture(false);
    const bool extrusion_sample = action == RetopoAction::Sample && view_.extruding;
    RetopoMesh draft = *(extrusion_sample && gesture_ ? gesture_->topology : view_.topology);
    auto selected = extrusion_sample && gesture_ ? gesture_->selected : view_.selected;
    float maximum = snap_mm * .001f / view_.transform.scale;
    SurfaceQueryOptions options; options.cancelled = cancelled;
    auto project = [&](Vec3 p) {
      if (!finite(p)) throw std::invalid_argument("Invalid retopology point");
      auto hit = view_.source->closest(p, options);
      if (hit.distance > maximum) throw std::runtime_error("Point is outside the source projection distance");
      return hit.position;
    };
    auto outward = [&](const RetopoMesh &mesh, TopologyID face) {
      const auto &f = mesh.faces().at(face); Vec3 center{};
      for (auto id : f.vertices) center += mesh.vertices().at(id).position;
      auto hit = view_.source->closest(center / float(f.vertices.size()), options);
      return hit.distance <= maximum && dot(face_normal(mesh, f), hit.normal) > 0;
    };
    if (action == RetopoAction::Sample) {
      if (!gesture_) throw std::logic_error("Begin a retopology gesture first");
      if (sampled_ && !view_.moving && !view_.extruding) return;
      point = project(point);
      if (view_.extruding) {
        bool incident = false;
        auto edge_id = selected_edge(draft, selected, cancelled, true, &incident);
        auto e = draft.edges().at(edge_id);
        auto a = draft.vertices().at(e.a).position, b = draft.vertices().at(e.b).position;
        if (!sampled_) {
          auto ab = b - a; const float square = dot(ab, ab);
          if (!(square > 0)) throw std::runtime_error("Choose a nonzero boundary edge");
          auto nearest = a + ab * std::clamp(dot(point - a, ab) / square, 0.f, 1.f);
          if (length(point - nearest) > std::min(.01f, snap_mm * .00025f) / view_.transform.scale)
            throw std::runtime_error("Touch the selected edge before dragging an extrusion");
        } else {
          const auto offset = point - extrude_anchor_;
          if (dot(offset, offset) > 0) {
            auto result = draft.extrude_edge(edge_id, {project(a + offset), project(b + offset)}, cancelled);
            bool valid = outward(draft, result.face);
            if (!incident && !valid) {
              // A wire has no adjacent face to fix its winding. Orient the
              // candidate to the reference without changing existing IDs.
              auto face = draft.faces().at(result.face);
              std::reverse(face.vertices.begin(), face.vertices.end());
              draft.remove_face(result.face);
              result.face = draft.add_face(face.vertices, face.material);
              valid = outward(draft, result.face);
            }
            if (!valid) throw std::runtime_error("Extruded face is outside the source bound or faces inward");
            selected.assign(result.outer.begin(), result.outer.end());
          }
        }
      } else if (!sampled_) {
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
            if (selected.size() == 4) throw std::runtime_error("Make a face, bridge edges or clear the four selected points");
            if (!nearest) nearest = draft.add_vertex({point});
            selected.push_back(nearest);
          }
        }
      } else if (view_.moving && moving_vertex_) draft.move_vertex(moving_vertex_, point);
    } else if (action == RetopoAction::BridgeEdges) {
      if (selected.size() != 4) throw std::runtime_error("Select two boundary edges as four endpoints");
      const auto a = selected[0], b = selected[1], c = selected[2], d = selected[3];
      std::optional<RetopoMesh> best; float cost = std::numeric_limits<float>::infinity();
      for (auto ids : {std::array<TopologyID,4>{a,b,c,d}, {b,a,d,c}, {a,b,d,c}, {b,a,c,d}}) {
        stop(cancelled); auto candidate = draft; TopologyID face;
        // Candidate errors are geometry-only: cancellation/source queries are
        // evaluated outside this catch and can never become another candidate.
        try { face = candidate.bridge_edges(ids); } catch (const std::runtime_error &) { continue; }
        stop(cancelled);
        if (!outward(candidate, face)) continue;
        const auto delta0 = candidate.vertices().at(ids[1]).position - candidate.vertices().at(ids[2]).position;
        const auto delta1 = candidate.vertices().at(ids[3]).position - candidate.vertices().at(ids[0]).position;
        const float distance = dot(delta0,delta0) + dot(delta1,delta1);
        if (distance < cost) { best = std::move(candidate); cost = distance; }
      }
      if (!best) throw std::runtime_error("Boundary edges cannot be bridged outward within the source bound");
      draft = std::move(*best); selected.clear();
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
    auto next_topology = changed ? std::make_shared<const RetopoMesh>(std::move(draft)) : view_.topology;
    if (extrusion_sample && sampled_ && dot(point - extrude_anchor_, point - extrude_anchor_) == 0)
      next_topology = gesture_->topology;
    if (action != RetopoAction::Sample && changed) remember(std::move(before));
    view_.topology = std::move(next_topology);
    view_.selected = std::move(selected);
    if (view_.selected.size() != 2) view_.extruding = false;
    if (action == RetopoAction::Sample) {
      if (view_.extruding && !sampled_) extrude_anchor_ = point;
      sampled_ = true;
    }
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
