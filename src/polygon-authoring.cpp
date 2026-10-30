// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/polygon-authoring.hpp"
#include <map>
#include <set>
namespace dome {
namespace {
void stop(const PolygonLimits &limits) {
  if (limits.cancelled && limits.cancelled())
    throw std::runtime_error("Polygon workspace cancelled");
}
bool present(const PolygonMesh &mesh, PolygonSelectionMode mode, PolygonId id) {
  if (mode == PolygonSelectionMode::Vertex)
    return std::any_of(mesh.vertices().begin(), mesh.vertices().end(),
                       [&](auto &v) { return v.id == id; });
  if (mode == PolygonSelectionMode::Edge)
    return std::any_of(mesh.edges().begin(), mesh.edges().end(),
                       [&](auto &e) { return e.id == id; });
  if (mode == PolygonSelectionMode::Face)
    return std::any_of(mesh.faces().begin(), mesh.faces().end(),
                       [&](auto &f) { return f.id == id; });
  throw std::invalid_argument("Unknown polygon selection mode");
}
std::vector<PolygonId> ids(const PolygonMesh &m, PolygonSelectionMode mode) {
  std::vector<PolygonId> out;
  if (mode == PolygonSelectionMode::Vertex)
    for (auto &v : m.vertices())
      out.push_back(v.id);
  else if (mode == PolygonSelectionMode::Edge)
    for (auto &e : m.edges())
      out.push_back(e.id);
  else if (mode == PolygonSelectionMode::Face)
    for (auto &f : m.faces())
      out.push_back(f.id);
  else
    throw std::invalid_argument("Unknown polygon selection mode");
  return out;
}
double distance(Vec3 a, Vec3 b) {
  return std::hypot(double(a.x) - b.x, double(a.y) - b.y, double(a.z) - b.z);
}
Vec3 segment(Vec3 p, Vec3 a, Vec3 b) {
  double x = double(b.x) - a.x, y = double(b.y) - a.y, z = double(b.z) - a.z,
         den = x * x + y * y + z * z;
  double t =
      den ? std::clamp(((double(p.x) - a.x) * x + (double(p.y) - a.y) * y +
                        (double(p.z) - a.z) * z) /
                           den,
                       0., 1.)
          : 0;
  return {float(a.x + x * t), float(a.y + y * t), float(a.z + z * t)};
}
PolygonDrawData display_data(const PolygonMesh &mesh,
                             const PolygonRender &surface,
                             const PolygonSelection &selection,
                             const PolygonLimits &limits) {
  PolygonDrawData out;
  std::set<PolygonId> selected(selection.ids.begin(), selection.ids.end()),
      vertices;
  std::set<std::pair<PolygonId, PolygonId>> selected_edges;
  size_t work = 0;
  auto visit = [&] {
    stop(limits);
    if (++work > limits.maximum_work)
      throw std::length_error("Polygon display exceeds its work budget");
  };
  if (selection.mode == PolygonSelectionMode::Vertex)
    vertices = selected;
  else if (selection.mode == PolygonSelectionMode::Edge) {
    for (auto &e : mesh.edges()) {
      visit();
      if (selected.contains(e.id)) {
        vertices.insert(e.a);
        vertices.insert(e.b);
        selected_edges.insert(std::minmax(e.a, e.b));
      }
    }
  } else
    for (auto &f : mesh.faces()) {
      visit();
      if (selected.contains(f.id))
        for (size_t i = 0; i < f.corners.size(); ++i) {
          visit();
          auto a = f.corners[i].vertex,
               b = f.corners[(i + 1) % f.corners.size()].vertex;
          vertices.insert(a);
          selected_edges.insert(std::minmax(a, b));
        }
    }
  std::map<PolygonId, Vec3> positions;
  for (auto &v : mesh.vertices()) {
    visit();
    positions.emplace(v.id, v.position);
  }
  std::map<PolygonId, Vec3> coefficients;
  for (auto &face : mesh.faces()) {
    std::array<double, 3> n{}, k{};
    auto origin = positions.at(face.corners[0].vertex);
    for (size_t i = 0; i < face.corners.size(); ++i) {
      visit();
      auto a = face.corners[i].vertex,
           b = face.corners[(i + 1) % face.corners.size()].vertex;
      auto pa = positions.at(a), pb = positions.at(b);
      std::array<double, 3> x{double(pa.x) - origin.x, double(pa.y) - origin.y,
                              double(pa.z) - origin.z},
          y{double(pb.x) - origin.x, double(pb.y) - origin.y,
            double(pb.z) - origin.z};
      n[0] += x[1] * y[2] - x[2] * y[1];
      n[1] += x[2] * y[0] - x[0] * y[2];
      n[2] += x[0] * y[1] - x[1] * y[0];
      for (size_t axis = 0; axis < 3; ++axis)
        k[axis] += (vertices.contains(a) ? y[axis] : 0) -
                   (vertices.contains(b) ? x[axis] : 0);
    }
    double length = std::hypot(n[0], n[1], n[2]);
    if (!(length > 0))
      throw std::invalid_argument("Polygon preview face has no area normal");
    Vec3 c{float(k[0] / length), float(k[1] / length), float(k[2] / length)};
    if (!finite(c))
      throw std::length_error(
          "Polygon normal preview exceeds its representable scale");
    for (auto &corner : face.corners) {
      visit();
      coefficients.emplace(corner.id, c);
    }
  }
  std::map<PolygonId, uint32_t> first;
  for (size_t i = 0; i < surface.vertices.size(); ++i) {
    visit();
    first.emplace(surface.vertices[i], uint32_t(i));
    out.normal_coefficients.push_back(coefficients.at(surface.corners.at(i)));
    out.move_weights.push_back(vertices.contains(surface.vertices[i]) ? 1.f
                                                                      : 0.f);
  }
  for (auto &e : mesh.edges()) {
    visit();
    auto a = first.find(e.a), b = first.find(e.b);
    if (a == first.end() || b == first.end())
      continue;
    out.wire_indices.push_back(a->second);
    out.wire_indices.push_back(b->second);
    if (selected_edges.contains(std::minmax(e.a, e.b))) {
      out.selected_edges.push_back(a->second);
      out.selected_edges.push_back(b->second);
    }
  }
  double x = 0, y = 0, z = 0;
  size_t count = 0;
  for (auto &v : mesh.vertices()) {
    visit();
    if (vertices.contains(v.id)) {
      x += v.position.x;
      y += v.position.y;
      z += v.position.z;
      ++count;
      if (selection.mode == PolygonSelectionMode::Vertex)
        if (auto i = first.find(v.id); i != first.end())
          out.selected_points.push_back(i->second);
    }
  }
  if (count) {
    out.selection_center = {float(x / count), float(y / count),
                            float(z / count)};
    out.has_selection = true;
  }
  return out;
}
} // namespace
PolygonWorkspace::Snapshot PolygonWorkspace::prepare(
    PolygonMesh mesh, PolygonSelection selected, const PolygonLimits &limits,
    std::shared_ptr<const PolygonColorField> colors) const {
  stop(limits);
  if (colors)
    colors->validate(mesh, limits);
  auto topology = std::make_shared<const PolygonMesh>(std::move(mesh));
  auto render =
      std::make_shared<const PolygonRender>(topology->triangulate(limits));
  std::shared_ptr<const SurfaceQuery> query;
  if (!render->mesh.indices.empty())
    query =
        std::make_shared<const SurfaceQuery>(render->mesh, limits.cancelled);
  const auto valid_ids = ids(*topology, selected.mode);
  std::set<PolygonId> valid;
  size_t visited = 0;
  for (auto id : valid_ids) {
    stop(limits);
    if (++visited > limits.maximum_work)
      throw std::length_error("Polygon selection exceeds its work budget");
    valid.insert(id);
  }
  std::erase_if(selected.ids, [&](auto id) {
    stop(limits);
    if (++visited > limits.maximum_work)
      throw std::length_error("Polygon selection exceeds its work budget");
    return !valid.contains(id);
  });
  auto view = std::make_shared<PolygonView>();
  view->topology = std::move(topology);
  view->colors = std::move(colors);
  view->surface = std::move(render);
  view->selection =
      std::make_shared<const PolygonSelection>(std::move(selected));
  view->display = std::make_shared<const PolygonDrawData>(
      display_data(*view->topology, *view->surface, *view->selection, limits));
  stop(limits);
  return {std::move(view), std::move(query)};
}
PolygonWorkspace::PolygonWorkspace(PolygonMesh mesh,
                                   const PolygonLimits &limits)
    : PolygonWorkspace(std::move(mesh), {}, limits) {}
PolygonWorkspace::PolygonWorkspace(
    PolygonMesh mesh, std::shared_ptr<const PolygonColorField> colors,
    const PolygonLimits &limits) {
  current_ = prepare(std::move(mesh), {}, limits, std::move(colors));
  original_ = current_.view->topology;
  high_water_ = original_->next_id();
}
PolygonWorkspace::Snapshot PolygonWorkspace::decorate(Snapshot next,
                                                      bool can_undo,
                                                      bool can_redo,
                                                      bool moving) const {
  if (current_.view->revision == UINT64_MAX)
    throw std::overflow_error("Polygon workspace revisions exhausted");
  auto view = std::make_shared<PolygonView>(*next.view);
  view->revision = current_.view->revision + 1;
  view->changed = view->topology != original_;
  view->can_undo = can_undo;
  view->can_redo = can_redo;
  view->moving = moving;
  next.view = std::move(view);
  return next;
}
void PolygonWorkspace::selection(PolygonSelection selected,
                                 const PolygonLimits &limits) {
  if (gesture_)
    throw std::logic_error(
        "Finish the component move before changing selection");
  auto view = std::make_shared<PolygonView>(*current_.view);
  view->selection =
      std::make_shared<const PolygonSelection>(std::move(selected));
  view->display = std::make_shared<const PolygonDrawData>(
      display_data(*view->topology, *view->surface, *view->selection, limits));
  auto next = decorate({std::move(view), current_.query}, !undo_.empty(),
                       !redo_.empty(), false);
  current_ = std::move(next);
}
void PolygonWorkspace::mode(PolygonSelectionMode mode,
                            const PolygonLimits &limits) {
  if (mode != PolygonSelectionMode::Vertex &&
      mode != PolygonSelectionMode::Edge && mode != PolygonSelectionMode::Face)
    throw std::invalid_argument("Unknown polygon selection mode");
  if (mode == current_.view->selection->mode)
    return;
  selection({mode, {}}, limits);
}
void PolygonWorkspace::select(PolygonId id, bool additive, bool toggle,
                              const PolygonLimits &limits) {
  auto selected = *current_.view->selection;
  if (!present(*current_.view->topology, selected.mode, id))
    throw std::invalid_argument("Selected polygon component no longer exists");
  if (!additive)
    selected.ids.clear();
  auto i = std::find(selected.ids.begin(), selected.ids.end(), id);
  if (i != selected.ids.end()) {
    if (toggle)
      selected.ids.erase(i);
  } else
    selected.ids.push_back(id);
  selection(std::move(selected), limits);
}
void PolygonWorkspace::select_all(const PolygonLimits &limits) {
  selection({current_.view->selection->mode,
             ids(*current_.view->topology, current_.view->selection->mode)},
            limits);
}
void PolygonWorkspace::clear_selection(const PolygonLimits &limits) {
  selection({current_.view->selection->mode, {}}, limits);
}
bool PolygonWorkspace::select_ray(Vec3 origin, Vec3 direction, float maximum,
                                  float radius, bool additive, bool toggle,
                                  const PolygonLimits &limits,
                                  bool preserve_selected_group) {
  if (gesture_)
    throw std::logic_error("Finish the component move before picking");
  if (!finite(origin) || !finite(direction) || !std::isfinite(maximum) ||
      maximum <= 0 || !std::isfinite(radius) || radius <= 0)
    throw std::invalid_argument("Invalid polygon component pick");
  stop(limits);
  if (!current_.query)
    return false;
  SurfaceQueryOptions options;
  options.cancelled = limits.cancelled;
  options.max_triangle_tests = limits.maximum_work;
  auto hit = current_.query->raycast(origin, direction, maximum, options);
  if (!hit)
    return false;
  const auto &mesh = *current_.view->topology;
  auto fid = current_.view->surface->triangle_faces.at(hit->triangle);
  auto pick = [&](PolygonId id) {
    const auto &selection = *current_.view->selection;
    if (preserve_selected_group &&
        std::find(selection.ids.begin(), selection.ids.end(), id) !=
            selection.ids.end())
      return;
    select(id, additive, toggle, limits);
  };
  if (current_.view->selection->mode == PolygonSelectionMode::Face) {
    pick(fid);
    return true;
  }
  const auto &face = *std::find_if(mesh.faces().begin(), mesh.faces().end(),
                                   [&](auto &f) { return f.id == fid; });
  std::map<PolygonId, Vec3> positions;
  for (auto &v : mesh.vertices()) {
    stop(limits);
    positions.emplace(v.id, v.position);
  }
  PolygonId best = 0;
  double nearest = radius;
  if (current_.view->selection->mode == PolygonSelectionMode::Vertex) {
    for (auto &c : face.corners) {
      stop(limits);
      double d = distance(hit->position, positions.at(c.vertex));
      if (d < nearest || (d == nearest && (!best || c.vertex < best))) {
        nearest = d;
        best = c.vertex;
      }
    }
  } else {
    std::map<std::pair<PolygonId, PolygonId>, PolygonId> edges;
    for (auto &e : mesh.edges()) {
      stop(limits);
      edges.emplace(std::minmax(e.a, e.b), e.id);
    }
    for (size_t i = 0; i < face.corners.size(); ++i) {
      stop(limits);
      auto a = face.corners[i].vertex,
           b = face.corners[(i + 1) % face.corners.size()].vertex;
      double d = distance(hit->position, segment(hit->position, positions.at(a),
                                                 positions.at(b)));
      auto id = edges.at(std::minmax(a, b));
      if (d < nearest || (d == nearest && (!best || id < best))) {
        nearest = d;
        best = id;
      }
    }
  }
  if (!best)
    return false;
  stop(limits);
  pick(best);
  return true;
}
void PolygonWorkspace::trim_history() {
  size_t components = 0;
  for (auto i = undo_.rbegin(); i != undo_.rend(); ++i) {
    auto &m = *i->view->topology;
    size_t n = m.vertices().size() + m.edges().size();
    for (auto &f : m.faces())
      n += f.corners.size() + 1;
    if (auto colors = i->view->colors) {
      n += colors->corners().size();
      for (const auto &c : colors->corners())
        n += c.provenance.size();
    }
    if (n > maximum_history_components_ - components) {
      auto keep = std::distance(undo_.rbegin(), i);
      while (undo_.size() > size_t(keep))
        undo_.pop_front();
      break;
    }
    components += n;
  }
  while (undo_.size() > 128)
    undo_.pop_front();
}
void PolygonWorkspace::commit(Snapshot next, Snapshot before) {
  size_t components = before.view->topology->vertices().size() +
                      before.view->topology->edges().size();
  for (auto &face : before.view->topology->faces()) {
    if (face.corners.size() + 1 >
        maximum_history_components_ -
            std::min(components, maximum_history_components_))
      throw std::length_error("Polygon edit exceeds its Undo resource budget");
    components += face.corners.size() + 1;
  }
  if (auto colors = before.view->colors) {
    const auto add = [&](size_t n) {
      if (n > maximum_history_components_ -
                  std::min(components, maximum_history_components_))
        throw std::length_error(
            "Polygon corner colors exceed their Undo resource budget");
      components += n;
    };
    add(colors->corners().size());
    for (const auto &c : colors->corners())
      add(c.provenance.size());
  }
  if (components > maximum_history_components_)
    throw std::length_error("Polygon edit exceeds its Undo resource budget");
  next = decorate(std::move(next), true, false, false);
  undo_.push_back(
      std::move(before)); // Last allocation before authority changes.
  static_assert(std::is_nothrow_move_assignable_v<Snapshot>);
  redo_.clear();
  current_ = std::move(next);
  high_water_ = std::max(high_water_, current_.view->topology->next_id());
  trim_history();
}
void PolygonWorkspace::apply(PolygonOperation operation, Vec3 delta,
                             float amount, unsigned cuts,
                             const PolygonLimits &limits) {
  if (gesture_)
    throw std::logic_error(
        "Finish the component move before another operation");
  PolygonEdit edit{operation,  *current_.view->selection, delta, amount, cuts,
                   high_water_};
  PolygonEditResult result;
  std::shared_ptr<const PolygonColorField> colors;
  if (current_.view->colors) {
    auto colored = edit_colored_polygon(*current_.view->topology,
                                        *current_.view->colors, edit, limits);
    result = std::move(colored.geometry);
    if (colored.colors)
      colors =
          std::make_shared<const PolygonColorField>(std::move(*colored.colors));
  } else
    result = edit_polygon(*current_.view->topology, edit, limits);
  if (!result.replacement)
    return;
  auto next = prepare(std::move(*result.replacement), *current_.view->selection,
                      limits, std::move(colors));
  commit(std::move(next), current_);
}
void PolygonWorkspace::begin_move() {
  if (gesture_)
    throw std::logic_error("A polygon component move is already active");
  if (current_.view->selection->ids.empty())
    throw std::logic_error("Select polygon components before moving");
  auto next = decorate(current_, !undo_.empty(), !redo_.empty(), true);
  gesture_ = current_;
  current_ = std::move(next);
}
void PolygonWorkspace::move_to(Vec3 delta, const PolygonLimits &limits) {
  if (!gesture_)
    throw std::logic_error("Begin the component move first");
  if (!finite(delta))
    throw std::invalid_argument("Invalid polygon move delta");
  stop(limits);
  // Only a uniform changes during motion. Immutable triangles, BVH, UVs,
  // normals and the worker-prepared selection mask retain their owners.
  auto view = std::make_shared<PolygonView>(*gesture_->view);
  view->move_delta = delta;
  auto next = decorate({std::move(view), gesture_->query}, true, false, true);
  current_ = std::move(next);
}
void PolygonWorkspace::end_move(bool accept, const PolygonLimits &limits) {
  if (!gesture_)
    return;
  auto before = *gesture_;
  if (accept) {
    PolygonEdit edit{PolygonOperation::Move,
                     *before.view->selection,
                     current_.view->move_delta,
                     0,
                     1,
                     high_water_};
    PolygonEditResult result;
    std::shared_ptr<const PolygonColorField> colors;
    if (before.view->colors) {
      auto colored = edit_colored_polygon(*before.view->topology,
                                          *before.view->colors, edit, limits);
      result = std::move(colored.geometry);
      if (colored.colors)
        colors = std::make_shared<const PolygonColorField>(
            std::move(*colored.colors));
    } else
      result = edit_polygon(*before.view->topology, edit, limits);
    if (result.replacement) {
      auto next = prepare(std::move(*result.replacement),
                          *before.view->selection, limits, std::move(colors));
      commit(std::move(next), before);
      gesture_.reset();
      return;
    }
  }
  auto next = decorate(before, !undo_.empty(), !redo_.empty(), false);
  current_ = std::move(next);
  gesture_.reset();
}
bool PolygonWorkspace::undo() {
  if (gesture_) {
    end_move(false);
    return true;
  }
  if (undo_.empty())
    return false;
  auto next = decorate(undo_.back(), undo_.size() > 1, true, false);
  redo_.push_back(current_);
  undo_.pop_back();
  current_ = std::move(next);
  return true;
}
bool PolygonWorkspace::redo() {
  if (gesture_)
    throw std::logic_error("Finish the component move before Redo");
  if (redo_.empty())
    return false;
  auto next = decorate(redo_.back(), true, redo_.size() > 1, false);
  undo_.push_back(current_);
  redo_.pop_back();
  current_ = std::move(next);
  trim_history();
  return true;
}
} // namespace dome
