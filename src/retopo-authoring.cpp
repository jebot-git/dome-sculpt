// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/retopo-authoring.hpp"
#include "dome/affine.hpp"
#include "dome/bake-recipe.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/polygon-scene.hpp"
#include "dome/source-placement.hpp"
#include "dome/tangents.hpp"
#include "normal-bake-frames.hpp"
#include "normal-frame-runtime.hpp"
#include "baked-normal-field.hpp"
#include "deformation-active.hpp"
#include "paint-normal-access.hpp"
#include "retopo-transfer.hpp"
#include "retopo-surface.hpp"
#include "skin-affine.hpp"
#include "source-paint-pairing.hpp"
#include "source-skin-link.hpp"
#include "text-utf8.hpp"
#include "texture-set-paint-access.hpp"
#include "texture-set-surface.hpp"
#include <algorithm>
#include <bit>
#include <limits>
#include <map>
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

bool same_position(Vec3 a, Vec3 b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

AffineVector coordinates(Vec3 value) { return {value.x, value.y, value.z}; }

Vec3 normal_direction(const AffineTransform &frame, Vec3 normal) {
  const auto value = frame.normal(coordinates(normal));
  const double magnitude = std::hypot(value[0], value[1], value[2]);
  if (!(magnitude > 0) || !std::isfinite(magnitude))
    throw std::invalid_argument("Retopology correspondence has an invalid normal");
  return {float(value[0] / magnitude), float(value[1] / magnitude), float(value[2] / magnitude)};
}
}

detail::RetopoTransfer::RetopoTransfer(const Mesh &previous_mesh,
    const AffineTransform &previous, const AffineTransform &replacement,
    RetopoTransferBudget &budget, std::span<const RetopoCorrespondence> retained, bool first_patch,
    const DeformationState *source_pose)
    : budget_(budget), retained_(retained), first_patch_(first_patch), posed_source_(evaluates_geometry(source_pose)),
      placement_change_(replacement.inverse() * previous) {
  // Subtract a common translation before narrowing the query point. Meshes
  // far from the world origin still use the authored affine metric.
  auto old = previous.matrix();
  auto next = replacement.matrix();
  for (size_t axis = 12; axis < 15; ++axis) {
    next[axis] -= old[axis];
    old[axis] = 0;
  }
  previous_metric_ = AffineTransform(old);
  replacement_metric_ = AffineTransform(next);
  const size_t faces = previous_mesh.indices.size() / 3;
  reserve(previous_mesh.positions.size(), 2 * sizeof(Vec3) + sizeof(std::array<float, 2>) + 1);
  // The detached BVH owns triangle order, bounds and nodes as well as the mesh.
  reserve(faces, 224);
  work(previous_mesh.positions.size() + previous_mesh.indices.size());
  std::optional<Mesh> posed;
  if (posed_source_) {
    // The evaluator can hold its working mesh and a new skin or morph result
    // at once. The BVH's copy is already included in the source budget above.
    reserve(previous_mesh.positions.size(), 2 * (2 * sizeof(Vec3) + sizeof(std::array<float, 2>)));
    reserve(previous_mesh.indices.size(), 2 * sizeof(uint32_t));
    reserve(previous_mesh.uv_pins.size() + previous_mesh.uv_seams.size(), 2);
    work(source_pose->resource_entries() * 128);
    stop(cancelled());
    posed = source_pose->preview(previous_mesh);
    stop(cancelled());
    if (posed->indices != previous_mesh.indices || posed->uv != previous_mesh.uv)
      throw std::invalid_argument("The posed retopology source lost its original material corners");
  }
  const auto &geometry = posed ? *posed : previous_mesh;
  old_surface_ = std::make_unique<SurfaceQuery>(geometry, cancelled());
  world_normals_.reserve(faces);
  for (size_t corner = 0; corner < geometry.indices.size(); corner += 3) {
    if ((corner & 1023) == 0) stop(cancelled());
    const auto a = geometry.positions[geometry.indices[corner]];
    const auto b = geometry.positions[geometry.indices[corner + 1]];
    const auto c = geometry.positions[geometry.indices[corner + 2]];
    const auto normal = cross(b - a, c - a);
    world_normals_.push_back(length(normal) > 0 ? normal_direction(previous_metric_, normal) : Vec3{});
  }
  options_.cancelled = cancelled();
  // This is the maximum projection span exposed in the retopology editor.
  options_.closest_maximum = .2f;
  options_.triangle_filter = [this](uint32_t face) {
    work();
    return (!current_domain_ || current_domain_->contains(face)) && dot(world_normals_[face], current_normal_) > 0;
  };
}

void detail::RetopoTransferBudget::work(size_t amount) {
  if (amount > work_left_) throw std::length_error("Retopology transfer exceeds the surface work budget");
  work_left_ -= amount;
}

void detail::RetopoTransferBudget::reserve(size_t count, size_t stride) {
  if (count > bytes_left_ / stride) throw std::length_error("Retopology transfer exceeds working storage");
  bytes_left_ -= count * stride;
}

std::optional<SurfaceHit> detail::RetopoTransfer::match(Vec3 position, Vec3 normal,
    const TextureSetFaceDomain *source_domain) {
  work();
  current_domain_ = source_domain;
  current_normal_ = normal_direction(replacement_metric_, normal);
  const auto point = skin_affine_point(replacement_metric_, position);
  if (previous_metric_.matrix() == AffineTransform{}.matrix())
    return old_surface_->nearest(point, options_);
  const auto result = old_surface_->nearest_result(point, previous_metric_, options_);
  return result ? std::optional(result->surface) : std::nullopt;
}

bool detail::RetopoTransfer::has_retained(uint32_t face) const noexcept {
  return face < retained_.size() && retained_[face].witnessed && retained_[face].triangle < world_normals_.size();
}

std::optional<SurfaceHit> detail::RetopoTransfer::match_retained(Vec3 position, Vec3 normal,
    uint32_t face, std::array<float,3> weights, const TextureSetFaceDomain *source_domain) {
  work();
  if (!has_retained(face)) return {};
  const auto &reference = retained_[face];
  if ((source_domain && !source_domain->contains(reference.triangle)) ||
      dot(world_normals_[reference.triangle],normal_direction(replacement_metric_,normal)) <= 0)
    return {};
  auto hit = SurfaceHit{.triangle=reference.triangle};
  for (size_t i=0;i<3;++i) for (size_t k=0;k<3;++k)
    hit.barycentric[k] += std::max(0.f,weights[i])*reference.corners[i][k];
  const auto total = hit.barycentric[0]+hit.barycentric[1]+hit.barycentric[2];
  if (!(total>0)) return {};
  const auto &mesh = old_surface_->mesh();
  for (size_t k=0;k<3;++k) {
    hit.barycentric[k] /= total;
    const auto vertex = mesh.indices[size_t(reference.triangle)*3+k];
    hit.position += mesh.positions[vertex]*hit.barycentric[k];
  }
  hit.normal = world_normals_[reference.triangle];
  hit.distance = length(skin_affine_point(previous_metric_,hit.position)-skin_affine_point(replacement_metric_,position));
  return hit;
}

std::optional<SurfaceHit> detail::RetopoTransfer::match_patch(Vec3 position, Vec3 normal,
    uint32_t face, std::array<float,3> weights, const TextureSetFaceDomain *source_domain) {
  if (first_patch_ && source_domain && std::ranges::min(weights) > 1e-6f) {
    // Without earlier patch corners, every interior texel must agree with
    // the material picked at the face centre. The same-slot lookup alone
    // could project sideways across a source material boundary.
    auto hit = match(position, normal);
    if (!hit)
      throw std::invalid_argument("A patch face is outside the source material surface");
    if (!source_domain->contains(hit->triangle))
      throw std::invalid_argument("Split the patch along its source material boundary before Apply");
    return hit;
  }
  if (auto spatial = match(position,normal,source_domain)) return spatial;
  if (first_patch_ && !source_domain)
    throw std::invalid_argument("A retopology texel has no matching source surface within the projection range");
  return match_retained(position,normal,face,weights,source_domain);
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
  projection_ = std::make_shared<detail::RetopoSurface>(*view_.source);
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
    auto projected_position = [&](const SurfaceHit &hit) {
      if (hit.distance > maximum) throw std::runtime_error("Point is outside the source projection distance");
      return hit.position;
    };
    auto project = [&](Vec3 p) {
      return projected_position(projection_->closest(p,cancelled));
    };
    auto outward = [&](const RetopoMesh &mesh, TopologyID face) {
      const auto &f = mesh.faces().at(face); Vec3 center{};
      for (auto id : f.vertices) center += mesh.vertices().at(id).position;
      auto hit = projection_->closest(center / float(f.vertices.size()),cancelled);
      return hit.distance <= maximum && dot(face_normal(mesh, f), hit.normal) > 0;
    };
    if (action == RetopoAction::Sample) {
      if (!gesture_) throw std::logic_error("Begin a retopology gesture first");
      if (sampled_ && !view_.moving && !view_.extruding) return;
      if (!finite(point)) throw std::invalid_argument("Invalid retopology point");
      if (view_.extruding) {
        const auto touched = point;
        point = project(point);
        bool incident = false;
        auto edge_id = selected_edge(draft, selected, cancelled, true, &incident);
        auto e = draft.edges().at(edge_id);
        auto a = draft.vertices().at(e.a).position, b = draft.vertices().at(e.b).position;
        if (!sampled_) {
          auto ab = b - a; const float square = dot(ab, ab);
          if (!(square > 0)) throw std::runtime_error("Choose a nonzero boundary edge");
          auto edge_distance = [&](Vec3 p) {
            return length(p - a - ab * std::clamp(dot(p - a, ab) / square, 0.f, 1.f));
          };
          if (std::min(edge_distance(touched),edge_distance(point)) >
              std::min(.01f, snap_mm * .00025f) / view_.transform.scale)
            throw std::runtime_error("Touch the selected edge before dragging an extrusion");
        } else {
          const auto offset = point - extrude_anchor_;
          if (dot(offset, offset) > 0) {
            const std::array points{a+offset,b+offset};
            std::array<SurfaceHit,2> hits;
            projection_->closest(points,hits,cancelled);
            auto result = draft.extrude_edge(edge_id,
                {projected_position(hits[0]),projected_position(hits[1])},cancelled);
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
        auto touched_vertex = [&](Vec3 p) {
          TopologyID nearest = 0;
          float distance = std::min(.01f, snap_mm * .00025f) / view_.transform.scale;
          for (const auto &[id, v] : draft.vertices()) {
            stop(cancelled);
            const auto d = length(v.position - p);
            if (d <= distance) { nearest = id; distance = d; }
          }
          return nearest;
        };
        // A reopened patch may have been offset for Bake. First pick its
        // visible vertices at their actual placement, then use projection for
        // contact close to the reference and for newly placed points.
        auto nearest = touched_vertex(point);
        if (!nearest) {
          point = project(point);
          nearest = touched_vertex(point);
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
      } else if (view_.moving && moving_vertex_) draft.move_vertex(moving_vertex_, project(point));
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
      if (dot(normal, projection_->closest(center / float(selected.size()),cancelled).normal) < 0)
        std::reverse(selected.begin(), selected.end());
      draft.add_face(selected); selected.clear();
    } else if (action == RetopoAction::Wire || action == RetopoAction::Seam) {
      if (selected.size() != 2) throw std::runtime_error("Select two edge endpoints");
      if (action == RetopoAction::Wire) draft.add_edge(selected[0], selected[1]);
      else draft.toggle_seam(selected[0],selected[1],cancelled);
    } else if (action == RetopoAction::SubdivideEdge) {
      if (selected.size() != 2) throw std::runtime_error("Select the two endpoints of an existing edge");
      const auto edge = draft.edge_between(selected[0],selected[1]);
      if (!edge) throw std::runtime_error("Connect the endpoints before subdividing an edge");
      selected = {draft.subdivide_edge(edge,cancelled)};
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
      draft.relax(*projection_, relax);
    } else if (action == RetopoAction::Snap) {
      if (selected.empty()) throw std::runtime_error("Select points to project");
      draft.project(selected, *projection_, maximum, cancelled);
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
  view_.selected_edge_id = view_.selected.size() == 2 ?
      view_.topology->edge_between(view_.selected[0],view_.selected[1]) : 0;
  view_.changed = view_.topology != original_;
  ++view_.revision;
}
namespace {

bool same_patch_surface(const RetopoSurface &a, const RetopoSurface &b) {
  return a.source_vertices == b.source_vertices && a.source_faces == b.source_faces &&
         a.materials == b.materials && a.mesh.indices == b.mesh.indices &&
         a.mesh.uv == b.mesh.uv && a.mesh.uv_pins == b.mesh.uv_pins &&
         a.mesh.uv_seams == b.mesh.uv_seams &&
         std::ranges::equal(a.mesh.positions, b.mesh.positions, same_position);
}

bool same_rest_corners(const Mesh &a,const Mesh &b,const std::function<bool()> &cancelled) {
  if (a.indices.size()!=b.indices.size()) return false;
  for (size_t c=0;c<a.indices.size();++c) {
    if ((c&1023)==0) stop(cancelled);
    if (!same_position(a.positions[a.indices[c]],b.positions[b.indices[c]])) return false;
  }
  return true;
}

using StableTriangle = std::array<TopologyID,3>;
struct CornerParent { TopologyID vertex = 0; float weight = 0; };
using CornerParents = std::array<CornerParent,3>;

CornerParents combine_corner_support(std::initializer_list<CornerParents> corners) {
  CornerParents result{};
  size_t used = 0;
  for (const auto &corner : corners) {
    if (!corner[0].vertex) return {};
    for (const auto &parent : corner) {
      if (!parent.vertex) continue;
      const auto existing = std::ranges::find(result,parent.vertex,&CornerParent::vertex);
      const auto weight = parent.weight / float(corners.size());
      if (existing != result.end()) existing->weight += weight;
      else {
        if (used == result.size()) return {};
        result[used++] = {parent.vertex,weight};
      }
    }
  }
  std::ranges::sort(std::span(result).first(used),{},&CornerParent::vertex);
  return result;
}

std::vector<detail::RetopoCorrespondence> match_patch_triangles(const RetopoSurface &before,
    const RetopoSurface &after, const RetopoMesh &previous, const RetopoMesh &replacement,
    bool witnessed_order,
    const std::function<bool()> &cancelled) {
  const auto corners = [](const RetopoSurface &surface, size_t face) {
    StableTriangle ids;
    for (size_t k = 0; k < ids.size(); ++k)
      ids[k] = surface.source_vertices[surface.mesh.indices[face * 3 + k]];
    return ids;
  };
  const auto sorted = [](StableTriangle ids) { std::ranges::sort(ids); return ids; };
  // Stop at vertices in the preceding topology, even if they were created
  // by an earlier subdivision. Material correspondence is to that generation.
  std::map<TopologyID,CornerParents> midpoint_support;
  const auto support = [&](TopologyID id) -> CornerParents {
    if (previous.vertices().contains(id)) return {{{id,1},{},{}}};
    const auto found = midpoint_support.find(id);
    return found == midpoint_support.end() ? CornerParents{} : found->second;
  };
  for (const auto &[id,vertex] : replacement.vertices()) {
    stop(cancelled);
    const auto [a,b] = vertex.split_edge;
    if (a && !previous.vertices().contains(id))
      midpoint_support.emplace(id,combine_corner_support({support(a),support(b)}));
  }
  std::map<StableTriangle,uint32_t> source_triangles;
  std::vector<detail::RetopoCorrespondence> result(after.source_faces.size());
  for (size_t i = 0; i < result.size(); ++i) {
    if ((i & 1023) == 0) stop(cancelled);
    const auto new_corners = corners(after,i);
    const auto ids = sorted(new_corners);
    const std::array parents{support(new_corners[0]),support(new_corners[1]),support(new_corners[2])};
    const auto combined = midpoint_support.empty() ? CornerParents{} :
        combine_corner_support({parents[0],parents[1],parents[2]});
    const StableTriangle original{combined[0].vertex,combined[1].vertex,combined[2].vertex};
    const auto face = after.source_faces[i];
    auto &reference = result[i];
    for (auto parent : {face, replacement.faces().at(face).split_from}) {
      if (!parent) break;
      const auto candidates = std::ranges::equal_range(before.source_faces, parent);
      for (auto it = candidates.begin(); it != candidates.end(); ++it) {
        const auto candidate = size_t(it - before.source_faces.begin());
        const auto old_corners = sorted(corners(before,candidate));
        if (old_corners == ids || old_corners == original) {
          reference.triangle = uint32_t(candidate);
          break;
        }
      }
      if (reference.triangle != UINT32_MAX) break;
    }
    if (reference.triangle == UINT32_MAX && original[2]) {
      if (source_triangles.empty()) {
        for (size_t triangle = 0; triangle < before.source_faces.size(); ++triangle) {
          if ((triangle & 1023) == 0) stop(cancelled);
          const auto [position, inserted] = source_triangles.try_emplace(sorted(corners(before,triangle)),uint32_t(triangle));
          if (!inserted) position->second = UINT32_MAX;
        }
      }
      if (const auto found = source_triangles.find(original); found != source_triangles.end()) {
        if (found->second == UINT32_MAX) throw std::invalid_argument("Retopology subdivision has ambiguous material ancestry");
        reference.triangle = found->second;
      }
    }
    if (reference.triangle == UINT32_MAX || !witnessed_order) continue;
    const auto old_corners = corners(before,reference.triangle);
    for (size_t c=0;c<3;++c) {
      for (size_t k=0;k<3;++k) {
        const auto ancestor = std::ranges::find(parents[c],old_corners[k],&CornerParent::vertex);
        if (ancestor != parents[c].end()) reference.corners[c][k] = ancestor->weight;
      }
    }
    reference.witnessed = true;
  }
  return result;
}

std::shared_ptr<DeformationState> transfer_patch_deformation(const DeformationState &before,
    const Mesh &old_mesh, const Mesh &new_mesh, detail::RetopoTransfer &transfer) {
  const auto vertices = new_mesh.positions.size();
  const size_t targets = before.morphs ? before.morphs->targets.size() : 0;
  if (targets && vertices > 8'000'000 / targets)
    throw std::length_error("Retopology shapekeys exceed native deformation storage");
  if (targets) transfer.reserve(vertices * targets, sizeof(Vec3));
  if (before.skin) transfer.reserve(vertices, sizeof(VertexWeights));
  auto result = std::make_shared<DeformationState>(new_mesh);
  result->skeleton = before.skeleton;
  result->humanoid = before.humanoid;
  result->landmarks = before.landmarks;
  result->expressions = before.expressions;
  result->pose = before.pose;
  result->morph_weights = before.morph_weights;
  result->source_skin_instance = before.source_skin_instance;
  result->source_skin_node = before.source_skin_node;
  result->source_skin_joints = before.source_skin_joints;
  const auto &change = transfer.placement_change();
  if (change.matrix() != AffineTransform{}.matrix()) {
    detail::SkinGeometryWork work(500'000'000, transfer.cancelled());
    if (before.skeleton) result->skeleton = detail::skin_reframe(*before.skeleton, change, work);
    if (before.landmarks) result->landmarks = detail::skin_reframe_landmarks(*before.landmarks, change, work);
  }
  if (before.morphs) {
    result->morphs.emplace(new_mesh);
    for (const auto &target : before.morphs->targets)
      result->morphs->targets.push_back({target.name, std::vector<Vec3>(vertices)});
  }
  if (before.skin) {
    result->skin.emplace();
    result->skin->vertices.reserve(vertices);
  }
  if (targets || before.skin) {
    size_t entries = result->resource_entries();
    std::vector<uint32_t> retained_corners;
    for (size_t vertex = 0; vertex < vertices; ++vertex) {
      if ((vertex & 1023) == 0) stop(transfer.cancelled());
      const auto position = new_mesh.positions[vertex], normal = new_mesh.normals[vertex];
      auto hit = transfer.match(position,normal);
      if (!hit) {
        if (retained_corners.empty()) {
          transfer.reserve(vertices,sizeof(uint32_t));
          transfer.work(new_mesh.indices.size());
          retained_corners.assign(vertices,UINT32_MAX);
          for (size_t corner=0;corner<new_mesh.indices.size();++corner) {
            if ((corner & 1023) == 0) stop(transfer.cancelled());
            auto &first = retained_corners[new_mesh.indices[corner]];
            if (first==UINT32_MAX && transfer.has_retained(uint32_t(corner/3))) first=uint32_t(corner);
          }
        }
        const auto corner=retained_corners[vertex];
        if (corner!=UINT32_MAX) {
          std::array<float,3> weights{};
          weights[corner%3]=1;
          hit=transfer.match_retained(position,normal,corner/3,weights);
        }
      }
      if (!hit)
        throw std::invalid_argument("Some retopology vertices have no matching rest surface for weights or shapekeys");
      std::array<uint32_t,3> old;
      std::copy_n(old_mesh.indices.begin() + size_t(hit->triangle) * 3, 3, old.begin());
      const auto &bary = hit->barycentric;
      if (before.skin) {
        const auto bones = before.skeleton->bones.size();
        std::array<double,256> by_joint{};
        for (size_t c = 0; c < 3; ++c) {
          if (bary[c] <= 0) continue;
          const auto &corner = before.skin->vertices[old[c]];
          transfer.work(corner.size());
          for (const auto &influence : corner)
            by_joint[influence.joint] += double(influence.weight) * bary[c];
        }
        // Two or three source vertices may name the same joint.
        VertexWeights weights;
        transfer.work(bones);
        for (uint32_t joint = 0; joint < bones; ++joint)
          if (by_joint[joint] > 0) weights.push_back({joint,float(std::min(by_joint[joint],1.0))});
        auto normalized = normalize_weights(std::move(weights), bones, {}, {32, 0});
        if (normalized.size() > 8'000'000 - entries)
          throw std::length_error("Retopology weights exceed native deformation storage");
        entries += normalized.size();
        transfer.reserve(normalized.capacity(), sizeof(Influence));
        result->skin->vertices.push_back(std::move(normalized));
      }
      transfer.work(targets * 3);
      for (size_t target = 0; target < targets; ++target) {
        const auto &delta = before.morphs->targets[target].delta;
        const Vec3 value = delta[old[0]] * bary[0] + delta[old[1]] * bary[1] + delta[old[2]] * bary[2];
        const auto direction = change.direction(coordinates(value));
        result->morphs->targets[target].delta[vertex] = {float(direction[0]),float(direction[1]),float(direction[2])};
      }
    }
  }
  result->validate(new_mesh);
  return result;
}

std::shared_ptr<const SourceMaterialBindings> transfer_patch_material(const PaintedMesh &before,
    const std::shared_ptr<const SourceMaterialBindings> &before_material,
    const PaintedMesh &paint, const std::shared_ptr<const DeformationState> &deformation,
    detail::RetopoTransfer &transfer) {
  if (!before_material) return {};
  const auto &binding = *before_material;
  if (binding.has_mtoon || binding.normal_preparation || binding.color_preparation ||
      binding.canonical_attributes || binding.canonical_uv || binding.native_paint_sampling ||
      binding.native_paint_frames || binding.canonical_paint_sampling ||
      binding.polygon_surface_transfer || binding.sampling_reparameterized || binding.key_derivation)
    throw std::invalid_argument("Retopology cannot reproject this source-retaining material yet");
  auto result = std::make_shared<SourceMaterialBindings>(binding);
  if (const auto &frames = binding.normal_frames) {
    const auto &old = before.mesh();
    if (!frames->pbr || frames->pbr != frames->mtoon || frames->position_domain || frames->canonical ||
        frames->pbr->source_vertex_count || frames->pbr->source_corner_count ||
        (!frames->pbr->tangents.empty() && !frames->pbr->generated_tangents) ||
        frames->pbr->normals.size() != old.indices.size())
      throw std::invalid_argument("Retopology cannot reproject independent artist normal frames yet");
    for (size_t i = 0; i < old.indices.size(); ++i) {
      if ((i & 1023) == 0) stop(transfer.cancelled());
      if (!same_position(frames->pbr->normals[i], old.normals[old.indices[i]]))
        throw std::invalid_argument("Retopology cannot reproject edited rest normals yet");
    }
    const auto &mesh = paint.mesh();
    const size_t targets = deformation && deformation->morphs ? deformation->morphs->targets.size() : 0;
    transfer.reserve(mesh.indices.size(), (2 + 2 * targets) * sizeof(Vec3) + sizeof(Tangent));
    result->normal_frames = detail::normal_bake_frame_candidate(mesh, deformation.get(), nullptr,
        {128 * 1024 * 1024, 128 * 1024 * 1024, transfer.cancelled()}).frames;
  }
  if (binding.uv_origin) {
    auto origin = std::make_shared<SourceUvOrigin>();
    origin->paint = paint.snapshot();
    origin->deformation = deformation;
    result->uv_origin = std::move(origin);
  }
  result->validate();
  return result;
}

class RetopoReplacement {
public:
  struct State {
    std::shared_ptr<const PaintedMesh> paint;
    std::shared_ptr<const DeformationState> deformation;
    std::shared_ptr<const SourceMaterialBindings> material;
    std::shared_ptr<const TextureSetCollection> texture_sets;
    std::optional<std::array<float,3>> emission = {};
  };

  RetopoReplacement(std::shared_ptr<const PaintedMesh> grid,const Mesh &original_rest,AffineTransform old_world,
                    AffineTransform new_world, std::span<const TopologyID> old_faces,
                    std::span<const TopologyID> new_faces, std::vector<detail::RetopoCorrespondence> correspondence,
                    const std::function<bool()> &cancelled, const DeformationState *first_source_pose=nullptr)
      : budget_(cancelled), old_world_(old_world), new_world_(new_world), first_grid_(std::move(grid)),
        original_rest_(original_rest), first_source_pose_(first_source_pose),
        old_faces_(old_faces), new_faces_(new_faces), correspondence_(std::move(correspondence)) {
    const auto resolution = first_grid_->resolution();
    budget_.reserve(correspondence_.size(), sizeof(detail::RetopoCorrespondence));
    budget_.reserve(1, detail::TextureSetPaintAccess::working_bytes(*first_grid_, resolution));
    charge_raster(*first_grid_);
    grids_.emplace(resolution, first_grid_);
  }

  State project(const std::shared_ptr<const PaintedMesh> &primary, const Material &material,
                const std::shared_ptr<const DeformationState> &old_deformation,
                const std::shared_ptr<const SourceMaterialBindings> &old_material,
                const std::shared_ptr<const TextureSetCollection> &old_sets,
                const std::shared_ptr<const ObjectBakeRecipe> &recipe) {
    stop(budget_.cancelled());
    const auto &old_paint = *primary;
    auto &projection = query(old_paint);
    std::shared_ptr<const DeformationState> deformation;
    if (old_deformation) {
      const auto match = [&](const Deformation &entry) {
        return entry.mesh == old_paint.mesh_owner().get() && entry.source == old_deformation.get();
      };
      const auto found = std::ranges::find_if(deformations_, match);
      if (found != deformations_.end()) deformation = found->projected;
      else {
        deformation = transfer_patch_deformation(*old_deformation, old_paint.mesh(), first_grid_->mesh(), projection);
        deformations_.push_back({old_paint.mesh_owner().get(), old_deformation.get(), deformation});
      }
    }
    if (old_material && old_material->canonical_attributes && !old_sets)
      return reproject_canonical(old_paint,material,old_deformation,*old_material,deformation,projection);
    if (old_material && old_material->color_preparation && !old_sets)
      return reproject_color(old_paint,material,*old_material,deformation,projection);
    if (!old_sets && old_material && detail::native_texture_set_mtoon(&old_paint,nullptr,old_material.get())) {
      auto paint = grid_for(old_paint.resolution())->snapshot();
      detail::TextureSetPaintAccess::reproject_retopology(old_paint,*paint,projection,false,nullptr,nullptr,true);
      auto binding = detail::TextureSetPaintAccess::reproject_retopology_mtoon(old_paint,*paint,material,old_material,projection);
      return {std::move(paint),std::move(deformation),std::move(binding),{}};
    }
    // Every field of a texture set shares the replacement REST geometry. Its
    // own raster bounds and same-slot source filter preserve material seams.
    const bool owned_set_material = old_sets && (old_sets->primary_source() || (old_material && old_material->has_mtoon));
    auto fields = old_sets ? reproject_sets(primary, material, recipe, owned_set_material ? old_material : nullptr,
                                            *old_sets, projection)
                           : detail::RetopoTextureSets{};
    std::shared_ptr<const PaintedMesh> paint = fields.primary ? std::move(fields.primary) : reproject(old_paint, projection);
    auto binding = owned_set_material ? std::move(fields.primary_binding)
                                      : transfer_patch_material(old_paint, old_material, *paint, deformation, projection);
    return {paint, deformation, std::move(binding),
            std::move(fields.membership), fields.primary_emission};
  }

private:
  detail::RetopoTransferBudget budget_;
  AffineTransform old_world_, new_world_;
  std::shared_ptr<const PaintedMesh> first_grid_;
  const Mesh &original_rest_;
  const DeformationState *first_source_pose_;
  std::span<const TopologyID> old_faces_, new_faces_;
  std::vector<detail::RetopoCorrespondence> correspondence_;
  std::map<uint32_t,std::shared_ptr<const PaintedMesh>> grids_;
  std::map<const Mesh *,std::unique_ptr<detail::RetopoTransfer>> surfaces_;
  struct Paint {const PaintedMesh *source; std::shared_ptr<const PaintedMesh> projected;};
  struct Deformation {
    const Mesh *mesh;
    const DeformationState *source;
    std::shared_ptr<const DeformationState> projected;
  };
  std::vector<Paint> paints_;
  std::vector<Deformation> deformations_;
  struct ProjectedSets {
    const PaintedMesh *primary;
    const TextureSetCollection *source;
    const SourceMaterialBindings *binding;
    detail::RetopoTextureSets projected;
  };
  std::vector<ProjectedSets> projected_sets_;

  State reproject_color(const PaintedMesh &before,const Material &material,
      const SourceMaterialBindings &binding,
      const std::shared_ptr<const DeformationState> &deformation,detail::RetopoTransfer &projection,
      bool prepared_normals=false) {
    const auto &recipe = *binding.color_preparation;
    if (recipe.receipt.source_field != SourceColorField::Identity || recipe.paint_sampling ||
        recipe.uv_reprojection || before.source_maps() || before.source_sampling_prepared())
      throw std::invalid_argument("Retopology needs native prepared color with its original identity field");
    detail::source_paint_pairing(&before,&binding,nullptr,false);
    budget_.work(recipe.projection->quality_cells);
    const auto domain=before.paint_sampling_domain();
    recipe.validate_current(domain?*domain->basis():before.mesh(),nullptr,budget_.cancelled());
    const auto &grid = grid_for(before.resolution());
    const auto pixels = size_t(grid->resolution()) * grid->resolution();
    budget_.reserve(pixels,4);
    budget_.reserve(grid->mesh().positions.size(),sizeof(std::array<float,4>));

    // Certify the new atlas's unpainted identity field. The separate paint and
    // Bake roles are subsequently transported as live edits on that field.
    std::shared_ptr<const SourceMaterialBindings> rebound;
    if (!prepared_normals) {
      auto raw_binding = std::make_shared<SourceMaterialBindings>(binding);
      raw_binding->color_preparation.reset();
      rebound = transfer_patch_material(before,raw_binding,*grid,deformation,projection);
    }
    ColorPreparationOptions options;
    options.resolve_identity = true;
    options.max_projection_error = recipe.receipt.accepted_projection_error;
    options.max_working_bytes = budget_.remaining_bytes();
    options.max_quality_cells = std::min(options.max_quality_cells,uint64_t(budget_.remaining_work()));
    options.max_probe_samples = std::min(options.max_probe_samples,uint64_t(budget_.remaining_work()));
    options.cancelled = budget_.cancelled();
    auto output = prepare_source_colors({grid,{},rebound,deformation,material,{}},options);
    const auto &receipt = output.material_bindings->color_preparation->receipt;
    budget_.work(receipt.tested_quality_cells + receipt.probe_samples);
    detail::TextureSetPaintAccess::reproject_retopology(before,*output.paint,projection,prepared_normals);
    return {std::move(output.paint),deformation,std::move(output.material_bindings),{}};
  }

  State reproject_canonical(const PaintedMesh &before,const Material &material,
      const std::shared_ptr<const DeformationState> &old_deformation,const SourceMaterialBindings &binding,
      const std::shared_ptr<const DeformationState> &deformation,detail::RetopoTransfer &projection) {
    if (projection.posed_source())
      throw std::invalid_argument("Use the rest pose before transferring prepared normal fields to a first patch");
    if (binding.has_mtoon || binding.normal_preparation ||
        binding.polygon_surface_transfer || binding.native_paint_sampling || binding.native_paint_frames ||
        !binding.normal_frames || !binding.normal_frames->pbr || binding.normal_frames->pbr->tangents.empty())
      throw std::invalid_argument("Retopology needs the prepared native normal material and its actual tangent frame");
    detail::source_paint_pairing(&before,&binding,nullptr,true);
    detail::normal_frame_current_authority(binding,before.mesh(),old_deformation.get(),budget_.cancelled());
    const auto bake=before.baked();
    if (bake && (!bake->normal_field || bake->normal_field->origin()!=binding.canonical_attributes))
      throw std::invalid_argument("The prepared Bake no longer owns its current normal field");
    auto color = binding.color_preparation ? reproject_color(before,material,binding,deformation,projection,true) : State{};
    auto raw = color.paint ? color.paint : reproject(before,projection,true);
    // Clear Bake keeps an original conversion lease. Transport its actual
    // previous raw input to establish the new frame while keeping the live
    // replacement unbaked.
    const auto &anchor=*binding.canonical_attributes->before_paint();
    auto conversion_input=bake?raw:reproject(anchor,query(anchor));
    NormalPreparationOptions options;
    options.max_angular_error=binding.canonical_attributes->receipt().accepted_angular_error;
    options.max_working_bytes=budget_.remaining_bytes();
    options.cancelled=budget_.cancelled();
    auto output=convert_canonical_normals({conversion_input,{},bake?color.material:nullptr,deformation,material,{}},options);
    budget_.reserve(1,output.material_bindings->normal_frames->storage_bytes());
    std::shared_ptr<const PaintedMesh> paint=raw;
    if (bake) {
      auto field=detail::TextureSetPaintAccess::reproject_retopology_normals(
          before,binding,*output.paint,*output.material_bindings,projection);
      paint=SourceNormalPreparationAccess::adopt_baked_field(*raw,std::move(field),budget_.cancelled());
    } else {
      const auto &origin=output.material_bindings->canonical_attributes;
      budget_.reserve(size_t(origin->before_paint()->resolution())*origin->before_paint()->resolution(),4);
    }
    auto material_binding=std::make_shared<SourceMaterialBindings>(*output.material_bindings);
    if (!bake && color.material) material_binding->color_preparation=color.material->color_preparation;
    material_binding->physical_emission=binding.physical_emission;
    return {std::move(paint),deformation,std::move(material_binding),{}};
  }

  detail::RetopoTextureSets reproject_sets(const std::shared_ptr<const PaintedMesh> &primary,
      const Material &primary_material, const std::shared_ptr<const ObjectBakeRecipe> &recipe,
      const std::shared_ptr<const SourceMaterialBindings> &primary_binding, const TextureSetCollection &sets,
                                           detail::RetopoTransfer &projection) {
    const auto cached = std::ranges::find_if(projected_sets_, [&](const ProjectedSets &entry) {
      return entry.source == &sets && entry.binding == primary_binding.get() && (entry.primary == primary.get() ||
             detail::TextureSetPaintAccess::same_current(*primary,*entry.primary));
    });
    if (cached != projected_sets_.end()) return cached->projected;
    const auto existing = sets.triangle_sets();
    const auto &mesh = first_grid_->mesh();
    const bool first_patch = old_faces_.empty();
    if (existing.size() != primary->mesh().indices.size() / 3 || (!first_patch && existing.size() != old_faces_.size()) ||
        new_faces_.size() != mesh.indices.size() / 3)
      throw std::invalid_argument("Retopology source material slots differ from its genuine mesh");
    budget_.reserve(mesh.indices.size() / 3, sizeof(TextureSetId));
    std::vector<TextureSetId> slots;
    slots.reserve(mesh.indices.size() / 3);
    for (size_t c = 0; c < mesh.indices.size(); c += 3) {
      if ((c & 1023) == 0) stop(budget_.cancelled());
      const auto old_triangle = first_patch ? UINT32_MAX : correspondence_[c / 3].triangle;
      if (old_triangle != UINT32_MAX) {
        // A quad can carry two material slots. Its stable corners survive
        // reflection, reordering and movement across the old material seam.
        slots.push_back(existing[old_triangle]);
        continue;
      }
      budget_.work(2 * std::bit_width(old_faces_.size()) + 2);
      const auto previous = std::ranges::equal_range(old_faces_, new_faces_[c / 3]);
      if (!previous.empty()) {
        const auto fields = existing.subspan(previous.begin() - old_faces_.begin(), previous.size());
        // A moved face keeps its authored material even when its centre now
        // overlaps another face. Newly created faces use the rest surface.
        if (std::ranges::all_of(fields, [&](auto slot) { return slot == fields.front(); })) {
          slots.push_back(fields.front());
          continue;
        }
        throw std::invalid_argument("Retopology changed a quad diagonal that carries a material boundary");
      }
      const auto a = mesh.positions[mesh.indices[c]], b = mesh.positions[mesh.indices[c + 1]],
                 d = mesh.positions[mesh.indices[c + 2]];
      const auto hit = projection.match((a + b + d) / 3.f, cross(b - a,d - a));
      if (first_patch && !hit)
        throw std::invalid_argument("A patch face is outside the source material surface");
      slots.push_back(hit ? existing[hit->triangle] : 0);
    }
    auto result = detail::project_retopology_texture_sets(primary, primary_material, recipe, primary_binding, sets,
        first_grid_->mesh_owner(), slots, budget_, [&](const TextureSetView &field,
            const std::shared_ptr<const TextureSetFaceDomain> &domain) {
          const auto &source = *field.painted;
          const auto &binding = field.material_bindings;
          if (field.appearance || (field.source && !first_patch) ||
              (!field.source && binding && !detail::native_texture_set_mtoon(&source,nullptr,binding.get())))
            throw std::invalid_argument("Retopology needs the original texture set's retained material adapter");
          TextureSetLimits limits;
          limits.cancelled = budget_.cancelled();
          limits.maximum_work = budget_.remaining_work();
          const auto size = source.resolution();
          budget_.reserve(1, detail::TextureSetPaintAccess::initial_raster_bytes(size));
          auto output = detail::TextureSetPaintAccess::empty(domain, size, limits);
          charge_raster(*output);
          if (field.source) {
            detail::TextureSetPaintAccess::reproject_retopology(source, *output, projection, false, &field);
            return detail::RetopoTextureField{std::move(output),{},binding->physical_emission};
          }
          detail::TextureSetPaintAccess::reproject_retopology(source,*output,projection,false,nullptr,nullptr,bool(binding));
          if (!binding) return detail::RetopoTextureField{std::move(output),{}, {}};
          return detail::RetopoTextureField{output,detail::TextureSetPaintAccess::reproject_retopology_mtoon(
              source,*output,field.material,binding,projection), {}};
        });
    projected_sets_.push_back({primary.get(), &sets, primary_binding.get(), result});
    return result;
  }

  void charge_raster(const PaintedMesh &paint) {
    const auto &raster = paint.surface_diagnostics();
    budget_.work(20 * size_t(paint.resolution()) * paint.resolution() +
                 raster.raster_samples + raster.candidate_checks);
  }

  detail::RetopoTransfer &query(const PaintedMesh &paint) {
    auto &surface = surfaces_[paint.mesh_owner().get()];
    if (!surface) {
      std::span<const detail::RetopoCorrespondence> retained;
      if (&paint.mesh()==&original_rest_ || same_rest_corners(paint.mesh(),original_rest_,budget_.cancelled()))
        retained=correspondence_;
      surface=std::make_unique<detail::RetopoTransfer>(paint.mesh(),old_world_,new_world_,budget_,retained,
          old_faces_.empty(),first_source_pose_);
    }
    return *surface;
  }

  std::shared_ptr<const PaintedMesh> reproject(const PaintedMesh &source, detail::RetopoTransfer &projection,
                                               bool prepared_normals=false) {
    const auto existing = std::ranges::find_if(paints_, [&](const Paint &entry) {
      return entry.source == &source || detail::TextureSetPaintAccess::same_current(*entry.source,source);
    });
    if (existing != paints_.end()) return existing->projected;
    auto output = grid_for(source.resolution())->snapshot();
    detail::TextureSetPaintAccess::reproject_retopology(source, *output, projection,prepared_normals);
    paints_.push_back({&source, output});
    return output;
  }

  const std::shared_ptr<const PaintedMesh> &grid_for(uint32_t resolution) {
    auto &grid = grids_[resolution];
    if (!grid) {
      budget_.reserve(1, detail::TextureSetPaintAccess::working_bytes(*first_grid_, resolution));
      TextureSetLimits limits;
      limits.cancelled = budget_.cancelled();
      limits.maximum_work = budget_.remaining_work();
      grid = detail::TextureSetPaintAccess::resize_unpainted(*first_grid_, resolution, limits);
      charge_raster(*grid);
    }
    return grid;
  }
};

class RetopoRestoreHistory {
public:
  RetopoRestoreHistory(std::shared_ptr<const RetopoPatch> patch, CanvasTransform world,
                       std::shared_ptr<const SourcePlacementCapsule> placement,
                       RetopoReplacement *replacement, const std::function<bool()> &cancelled)
      : patch_(std::move(patch)), world_(world), placement_(std::move(placement)),
        replacement_(replacement), cancelled_(cancelled) {}

  std::shared_ptr<const PolygonOrigin> rebase(const std::shared_ptr<const PolygonOrigin> &original, size_t depth = 1) {
    if (!original) return {};
    stop(cancelled_);
    // Same depth bound as the native PolygonOrigin archive.
    if (depth > 32) throw std::length_error("Retopology Restore history exceeds archive depth");
    const auto [entry, inserted] = origins_.try_emplace(original.get());
    if (!inserted) {
      if (!entry->second) throw std::invalid_argument("Retopology Restore history has a cycle");
      return entry->second;
    }
    if (!original->mesh_active || !original->paint || !original->retopology ||
        original->retopology->source != patch_->source || original->polygon || original->appearance)
      throw std::invalid_argument("Retopology cannot rebase a Restore checkpoint from a different surface model");
    auto mapped = std::make_shared<PolygonOrigin>(*original);
    mapped->retopology = patch_;
    if (replacement_) {
      // Restore uses the current object's placement. Its old checkpoints
      // belong to that same object-local space, even if the object was moved.
      const auto state = replacement_->project(original->paint,original->material,original->deformation,original->material_bindings,
                                               original->texture_sets,original->bake_recipe);
      mapped->paint = state.paint;
      mapped->deformation = state.deformation;
      mapped->material_bindings = state.material;
      mapped->texture_sets = state.texture_sets;
      if (state.emission) mapped->material.emissive = *state.emission;
      mapped->world_transform = placement_ ? CanvasTransform{} : world_;
      mapped->placement = placement_;
    }
    mapped->shape_source = rebase(original->shape_source, depth + 1);
    mapped->rig_source = rebase(original->rig_source, depth + 1);
    entry->second = mapped;
    return mapped;
  }

private:
  std::shared_ptr<const RetopoPatch> patch_;
  CanvasTransform world_;
  std::shared_ptr<const SourcePlacementCapsule> placement_;
  RetopoReplacement *replacement_;
  const std::function<bool()> &cancelled_;
  std::map<const PolygonOrigin *,std::shared_ptr<const PolygonOrigin>> origins_;
};

} // namespace

uint64_t publish_retopology(Scene &scene, const RetopoMesh &topology, uint64_t source,
                            CanvasTransform transform, const std::function<bool()> &cancelled) {
  stop(cancelled);
  auto previous = scene.active();
  if (!text::valid_utf8(previous.name)) throw std::invalid_argument("Object names must be valid UTF-8");
  if (!previous.visible || previous.locked) throw std::logic_error("Retopology target changed");
  const auto &guide = scene.object(source);
  auto triangles = topology.triangulate(cancelled);
  stop(cancelled);
  Scene staged = scene;
  const auto id = staged.add(text::utf8_prefix(previous.name,240) + " retopo", Volume{}, transform);
  auto &object = staged.object(id);
  std::shared_ptr<PaintedMesh> painted;
  std::shared_ptr<const DeformationState> deformation;
  std::shared_ptr<const SourceMaterialBindings> material;
  std::shared_ptr<const TextureSetCollection> texture_sets;
  std::shared_ptr<const ObjectBakeRecipe> recipe;
  std::optional<std::array<float,3>> emission;
  const auto patch = std::make_shared<RetopoPatch>(RetopoPatch{source, topology});
  std::shared_ptr<const PolygonOrigin> rig_source, shape_source;
  if (previous.retopology) {
    if (previous.retopology->source != source || !previous.mesh_active || !previous.painted ||
        previous.appearance || previous.polygon)
      throw std::invalid_argument("Retopology cannot replace this material or topology authority yet");
    recipe = previous.bake_recipe;
    detail::authenticate_native_source_skin(scene, previous, cancelled);
    const auto old_world = scene.world_matrix(previous.id);
    const auto desired_world = affine_similarity(transform);
    const bool same_placement = old_world.matrix() == desired_world.matrix();
    const auto &original = previous.retopology->topology;
    const auto old_surface = original.triangulate(cancelled);
    bool same_surface = same_placement && same_patch_surface(old_surface, triangles);
    if (!same_surface && desired_world.matrix() == AffineTransform{}.matrix() &&
        (scene.has_affine_placement(previous.id) || scene.has_affine_placement(guide.id))) {
      // The affine editor works in world metres. Compare in that same frame
      // and return wire edits to the original local geometry without rounding
      // its unchanged surface vertices through the inverse transform.
      auto original_view = original;
      original_view.transform(old_world, cancelled);
      same_surface = same_patch_surface(original_view.triangulate(cancelled), triangles);
      if (same_surface)
        patch->topology.transform(old_world.inverse(), original_view, original, cancelled);
    }
    // Preserve the actual avatar instance and other parent-local links. The
    // workspace presents a world similarity, so use the exact affine inverse
    // of its original parent to express the new mesh in that same hierarchy.
    object.group = previous.group;
    if (same_placement || same_surface) {
      object.transform = previous.transform;
      object.affine_edit = previous.affine_edit;
    } else if (object.group) {
      object.transform = {};
      object.affine_edit = std::make_shared<const NativeAffineEdit>(scene.parent_matrix(previous.id).inverse() * desired_world);
    }
    const auto new_world = staged.world_matrix(id);
    std::unique_ptr<RetopoReplacement> replacement;
    if (same_surface) {
      painted = previous.painted->snapshot();
      deformation = previous.deformation;
      material = previous.material_bindings;
      texture_sets = previous.texture_sets;
    } else {
      const auto size = previous.painted->resolution();
      const auto &old_rest = previous.painted->mesh();
      auto correspondence = match_patch_triangles(old_surface,triangles,original,patch->topology,
          same_rest_corners(old_surface.mesh,old_rest,cancelled),cancelled);
      unwrap(triangles.mesh, size, 4, cancelled);
      PaintSurfaceOptions options;
      options.cancelled = cancelled;
      auto grid = PaintedMesh::from_mesh(std::move(triangles.mesh), size, options);
      replacement = std::make_unique<RetopoReplacement>(std::move(grid),old_rest,old_world,new_world,
          old_surface.source_faces, triangles.source_faces, std::move(correspondence), cancelled);
      auto current = replacement->project(previous.painted,previous.material,previous.deformation,previous.material_bindings,
                                          previous.texture_sets,previous.bake_recipe);
      painted = current.paint->snapshot();
      deformation = std::move(current.deformation);
      material = std::move(current.material);
      texture_sets = std::move(current.texture_sets);
      emission = current.emission;
    }
    RetopoRestoreHistory history(patch, transform, capture_source_placement(staged,id), replacement.get(), cancelled);
    rig_source = history.rebase(previous.rig_source);
    shape_source = history.rebase(previous.shape_source);
  } else {
    const auto &sets = previous.texture_sets;
    const bool retained = sets && sets->topology()->polygon_ids() &&
        (sets->primary_source() || std::ranges::any_of(sets->additional(),[](const auto &field) { return bool(field.source); }));
    const bool project_materials = previous.id == source && sets && !previous.appearance &&
        (retained || (!sets->topology()->polygon_ids() && detail::texture_set_editable_fields(previous)));
    const bool imported = previous.id == source && !sets && previous.mesh_active && previous.painted &&
        previous.appearance && previous.painted->source_maps() && previous.material_bindings &&
        (!previous.material.toon || previous.material_bindings->has_mtoon);
    const bool native = previous.id == source && !sets && previous.mesh_active && previous.painted &&
        !previous.appearance && (detail::TextureSetPaintAccess::has_authored_layers(*previous.painted) ||
                                  previous.material_bindings || previous.bake_recipe);
    if (native && previous.polygon && previous.polygon->colors)
      throw std::invalid_argument("Retopology needs a source adapter for the guide's authored corner colors");
    const auto resolution = project_materials || imported || native ? previous.painted->resolution() : 512;
    unwrap(triangles.mesh, resolution, 4, cancelled);
    PaintSurfaceOptions options;
    options.cancelled = cancelled;
    painted = PaintedMesh::from_mesh(std::move(triangles.mesh), resolution, options);
    if (imported) {
      detail::RetopoTransferBudget budget(cancelled);
      budget.reserve(1,detail::TextureSetPaintAccess::working_bytes(*painted,resolution));
      detail::RetopoTransfer projection(previous.painted->mesh(),scene.world_matrix(source),staged.world_matrix(id),
          budget,{},true,previous.deformation.get());
      const auto &binding=previous.material_bindings;
      if (binding->has_mtoon)
        material=detail::TextureSetPaintAccess::reproject_retopology_mtoon(*previous.painted,*painted,previous.material,binding,projection,&previous);
      detail::TextureSetPaintAccess::reproject_retopology(*previous.painted,*painted,projection,false,nullptr,&previous,binding->has_mtoon);
      recipe = previous.bake_recipe;
    } else if (project_materials || native) {
      RetopoReplacement replacement(painted, previous.painted->mesh(), scene.world_matrix(source), staged.world_matrix(id),
          {}, triangles.source_faces, {}, cancelled,previous.deformation.get());
      auto current = replacement.project(previous.painted, previous.material, {}, previous.material_bindings,
                                          sets,previous.bake_recipe);
      painted = current.paint->snapshot();
      material = std::move(current.material);
      texture_sets = std::move(current.texture_sets);
      emission = current.emission;
      recipe = previous.bake_recipe;
    }
  }
  object.painted = std::move(painted); object.mesh_active = true;
  object.material = previous.material; object.retopology = patch;
  if (emission) object.material.emissive = *emission;
  object.material_bindings = std::move(material);
  object.bake_recipe = std::move(recipe);
  object.deformation = std::move(deformation);
  object.rig_source = std::move(rig_source);
  object.shape_source = std::move(shape_source);
  object.texture_sets = std::move(texture_sets);
  staged.object(previous.id).visible = false;
  if (source != previous.id) staged.object(source).visible = false;
  detail::authenticate_native_source_skin(staged, object, cancelled);
  staged.validate(false);
  stop(cancelled);
  scene = std::move(staged);
  return id;
}
} // namespace dome
