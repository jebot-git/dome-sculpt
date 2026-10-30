// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/polygon-color.hpp"
#include <map>
#include <set>
namespace dome {
namespace {
struct Budget {
  const PolygonLimits &limits;
  size_t work = 0;
  void step() {
    if (work == limits.maximum_work)
      throw std::length_error("Polygon color work budget exceeded");
    ++work;
    if (limits.cancelled && limits.cancelled())
      throw std::runtime_error("Polygon color operation cancelled");
  }
};
template <class T> const T &lookup(const std::vector<T> &rows, PolygonId id) {
  auto i = std::lower_bound(
      rows.begin(), rows.end(), id,
      [](const T &row, PolygonId key) { return row.corner < key; });
  if (i == rows.end() || i->corner != id)
    throw std::invalid_argument("Polygon color corner witness is missing");
  return *i;
}
bool ordered(PolygonId &previous, PolygonId id) {
  if (!id || id <= previous)
    return false;
  previous = id;
  return true;
}
PolygonRgba sample_value(const PolygonColorSample &corner,
                         const std::vector<PolygonCornerRgba> &sources,
                         Budget &budget) {
  std::array<double, 4> value{};
  for (auto [id, weight] : corner.provenance) {
    budget.step();
    const auto &source = lookup(sources, id);
    for (size_t c = 0; c < 4; ++c)
      value[c] += double(source.value[c]) * weight;
  }
  PolygonRgba result;
  for (size_t c = 0; c < 4; ++c)
    result[c] = float(value[c]);
  return result;
}
} // namespace
PolygonColorField
PolygonColorField::from_corners(const PolygonMesh &mesh,
                                std::span<const PolygonCornerRgba> values,
                                const PolygonLimits &limits) {
  mesh.validate(limits);
  Budget budget{limits};
  PolygonColorField result;
  if (values.size() > limits.maximum_corners)
    throw std::length_error("Polygon color sample budget exceeded");
  result.sources_.assign(values.begin(), values.end());
  std::sort(result.sources_.begin(), result.sources_.end(),
            [](const auto &a, const auto &b) { return a.corner < b.corner; });
  for (const auto &face : mesh.faces())
    for (const auto &corner : face.corners) {
      budget.step();
      result.corners_.push_back({corner.id, {{corner.id, 1}}});
    }
  std::sort(result.corners_.begin(), result.corners_.end(),
            [](const auto &a, const auto &b) { return a.corner < b.corner; });
  if (result.sources_.size() != result.corners_.size())
    throw std::invalid_argument(
        "Initial polygon color source must cover each exact corner once");
  result.validate(mesh, limits);
  return result;
}
void PolygonColorField::validate(const PolygonMesh &mesh,
                                 const PolygonLimits &limits) const {
  Budget budget{limits};
  if (sources_.size() > limits.maximum_corners ||
      corners_.size() > limits.maximum_corners)
    throw std::length_error("Polygon color sample budget exceeded");
  PolygonId previous = 0;
  for (const auto &source : sources_) {
    budget.step();
    if (!ordered(previous, source.corner))
      throw std::invalid_argument(
          "Polygon color source identities must be unique and ordered");
    for (float c : source.value)
      if (!std::isfinite(c) || c < 0 || c > 1)
        throw std::invalid_argument(
            "Polygon colors must be finite linear unit RGBA");
  }
  previous = 0;
  size_t entries = 0;
  for (const auto &corner : corners_) {
    budget.step();
    if (!ordered(previous, corner.corner) || corner.provenance.empty() ||
        corner.provenance.size() > limits.maximum_provenance)
      throw std::invalid_argument("Invalid polygon color corner provenance");
    if (corner.provenance.size() > 8'000'000 - entries)
      throw std::length_error("Polygon color provenance storage exceeded");
    entries += corner.provenance.size();
    PolygonId old = 0;
    double sum = 0;
    for (auto [id, weight] : corner.provenance) {
      budget.step();
      if (!ordered(old, id) || !std::isfinite(weight) || weight <= 0)
        throw std::invalid_argument("Invalid polygon color source weight");
      lookup(sources_, id);
      sum += weight;
    }
    if (std::abs(sum - 1) > 1e-12)
      throw std::invalid_argument(
          "Polygon color weights must form a convex partition");
  }
  size_t count = 0;
  for (const auto &face : mesh.faces())
    for (const auto &corner : face.corners) {
      budget.step();
      lookup(corners_, corner.id);
      ++count;
    }
  if (count != corners_.size())
    throw std::invalid_argument("Polygon color field has orphan corners");
}
PolygonRgba PolygonColorField::sample(PolygonId corner,
                                      const PolygonLimits &limits) const {
  Budget budget{limits};
  budget.step();
  return sample_value(lookup(corners_, corner), sources_, budget);
}
std::vector<PolygonRgba>
PolygonColorField::packed(const PolygonMesh &mesh,
                          const PolygonLimits &limits) const {
  validate(mesh, limits);
  auto render = mesh.triangulate(limits);
  Budget budget{limits};
  std::vector<PolygonRgba> result;
  result.reserve(render.corners.size());
  for (auto corner : render.corners) {
    budget.step();
    result.push_back(sample_value(lookup(corners_, corner), sources_, budget));
  }
  return result;
}
PolygonColoredEdit edit_colored_polygon(const PolygonMesh &mesh,
                                        const PolygonColorField &field,
                                        const PolygonEdit &edit,
                                        const PolygonLimits &limits) {
  field.validate(mesh, limits);
  Budget budget{limits};
  // A dissolve may remove a color seam even where positions/UVs agree. Until
  // field-equivalence analysis is present, only constant affected regions are
  // admitted; the original remains exact on refusal.
  if (edit.operation == PolygonOperation::Dissolve) {
    for (auto edge_id : edit.selection.ids) {
      budget.step();
      auto edge = std::find_if(mesh.edges().begin(), mesh.edges().end(),
                               [&](const auto &e) { return e.id == edge_id; });
      if (edge == mesh.edges().end())
        throw std::invalid_argument("Unknown colored dissolve edge");
      std::optional<PolygonRgba> color;
      for (const auto &face : mesh.faces()) {
        bool incident = false;
        for (size_t i = 0; i < face.corners.size(); ++i) {
          budget.step();
          auto a = face.corners[i].vertex,
               b = face.corners[(i + 1) % face.corners.size()].vertex;
          if (std::minmax(a, b) == std::minmax(edge->a, edge->b))
            incident = true;
        }
        if (incident)
          for (const auto &c : face.corners) {
            auto value = sample_value(lookup(field.corners_, c.id),
                                      field.sources_, budget);
            if (color && *color != value)
              throw std::logic_error("Dissolve requires a proven color-field "
                                     "equivalence for this nonconstant region");
            color = value;
          }
      }
    }
  }
  auto result = edit_polygon_with_corner_sources(mesh, edit, limits);
  if (!result.geometry.replacement)
    return {};
  std::map<PolygonId, const PolygonCornerSource *> transfers;
  for (const auto &transfer : result.corner_sources) {
    budget.step();
    if (!transfers.emplace(transfer.corner, &transfer).second)
      throw std::logic_error("Duplicate polygon color transfer");
  }
  PolygonColorField output;
  output.sources_ = field.sources_;
  size_t entries = 0;
  for (const auto &face : result.geometry.replacement->faces())
    for (const auto &corner : face.corners) {
      budget.step();
      PolygonColorSample row;
      row.corner = corner.id;
      if (auto transfer = transfers.find(corner.id);
          transfer != transfers.end()) {
        std::map<PolygonId, double> weights;
        for (auto [id, amount] : transfer->second->weights) {
          const auto &input = lookup(field.corners_, id);
          for (auto [source, weight] : input.provenance) {
            budget.step();
            weights[source] += amount * weight;
            if (weights.size() > limits.maximum_provenance)
              throw std::length_error(
                  "Polygon color contributor budget exceeded");
          }
        }
        double sum = 0;
        for (auto [id, weight] : weights)
          sum += weight;
        for (auto [id, weight] : weights)
          if (weight > 0)
            row.provenance.emplace_back(id, weight / sum);
      } else
        row.provenance = lookup(field.corners_, corner.id).provenance;
      if (row.provenance.size() > 8'000'000 - entries)
        throw std::length_error("Polygon color provenance storage exceeded");
      entries += row.provenance.size();
      output.corners_.push_back(std::move(row));
    }
  std::sort(output.corners_.begin(), output.corners_.end(),
            [](const auto &a, const auto &b) { return a.corner < b.corner; });
  output.validate(*result.geometry.replacement, limits);
  return {std::move(result.geometry), std::move(output)};
}
} // namespace dome
