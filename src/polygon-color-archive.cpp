// SPDX-License-Identifier: GPL-3.0-or-later
#include "polygon-color-archive.hpp"
#include <nlohmann/json.hpp>
#include <set>
namespace dome {
struct PolygonColorArchiveAccess {
  static std::shared_ptr<const PolygonColorField>
  restore(std::shared_ptr<const std::vector<PolygonCornerRgba>> sources,
          std::vector<PolygonColorSample> corners) {
    auto value = std::make_shared<PolygonColorField>();
    value->sources_ = std::move(sources);
    value->corners_ = std::move(corners);
    return value;
  }
};
namespace detail {
namespace {
constexpr size_t maximum_nodes = 512, maximum_entries = 8'000'000;
void count(size_t &entries, size_t amount) {
  if (entries > maximum_entries || amount > maximum_entries - entries)
    throw std::length_error("Scene polygon color/provenance quota exceeded");
  entries += amount;
}
uint64_t integer(const nlohmann::json &v) {
  if (!v.is_number_unsigned() &&
      (!v.is_number_integer() || v.get<int64_t>() < 0))
    throw std::invalid_argument("Invalid polygon color archive identity");
  return v.get<uint64_t>();
}
void ordered(uint64_t &previous, const nlohmann::json &j) {
  auto id = integer(j);
  if (!id || id <= previous)
    throw std::invalid_argument("Polygon color archive IDs must be ordered");
  previous = id;
}
template <class T>
size_t index(const std::vector<std::shared_ptr<const T>> &values, const T *p) {
  for (size_t i = 0; i < values.size(); ++i)
    if (values[i].get() == p)
      return i;
  return values.size();
}
} // namespace
uint32_t
PolygonColorArchive::add(std::shared_ptr<const PolygonColorField> field) {
  if (!field || !field->source_owner())
    throw std::invalid_argument("Incomplete polygon color source basis");
  auto found = index(fields_, field.get());
  if (found < fields_.size())
    return uint32_t(found);
  if (fields_.size() == maximum_nodes)
    throw std::length_error("Polygon color field count exceeded");
  size_t added = field->corners().size();
  for (const auto &corner : field->corners())
    count(added, corner.provenance.size());
  const auto basis = field->source_owner();
  if (index(bases_, basis.get()) == bases_.size()) {
    if (bases_.size() == maximum_nodes)
      throw std::length_error("Polygon color source basis count exceeded");
    count(added, basis->size());
    bases_.push_back(basis);
  }
  count(entries_, added);
  fields_.push_back(std::move(field));
  return uint32_t(found);
}
const std::shared_ptr<const PolygonColorField> &
PolygonColorArchive::at(uint32_t i) const {
  if (i >= fields_.size())
    throw std::invalid_argument("Invalid polygon color field reference");
  return fields_[i];
}
nlohmann::json PolygonColorArchive::encode() const {
  nlohmann::json bases = nlohmann::json::array(),
                 fields = nlohmann::json::array();
  for (const auto &basis : bases_) {
    nlohmann::json rows = nlohmann::json::array();
    for (const auto &row : *basis)
      rows.push_back({row.corner, row.value});
    bases.push_back(std::move(rows));
  }
  for (const auto &field : fields_) {
    nlohmann::json rows = nlohmann::json::array();
    for (const auto &corner : field->corners()) {
      nlohmann::json weights = nlohmann::json::array();
      for (auto [source, weight] : corner.provenance)
        weights.push_back({source, weight});
      rows.push_back({corner.corner, std::move(weights)});
    }
    fields.push_back({{"basis", index(bases_, field->source_owner().get())},
                      {"corners", std::move(rows)}});
  }
  size_t entries = 0;
  preflight(bases, fields, entries);
  return {{"color_bases", std::move(bases)},
          {"color_fields", std::move(fields)}};
}
void PolygonColorArchive::preflight(const nlohmann::json &bases,
                                    const nlohmann::json &fields,
                                    size_t &entries) {
  if (!bases.is_array() || !fields.is_array() || bases.size() > maximum_nodes ||
      fields.size() > maximum_nodes)
    throw std::length_error("Polygon color archive table count exceeded");
  std::set<size_t> used_bases;
  for (const auto &basis : bases) {
    if (!basis.is_array() || basis.size() > 2'000'000)
      throw std::length_error("Polygon color source sample count exceeded");
    count(entries, basis.size());
    uint64_t previous = 0;
    for (const auto &source : basis) {
      if (!source.is_array() || source.size() != 2 || !source[1].is_array() ||
          source[1].size() != 4)
        throw std::invalid_argument("Invalid polygon color source record");
      ordered(previous, source[0]);
      for (const auto &channel : source[1]) {
        if (!channel.is_number())
          throw std::invalid_argument("Invalid polygon source RGBA channel");
        auto value = channel.get<double>();
        if (!std::isfinite(value) || value < 0 || value > 1)
          throw std::invalid_argument(
              "Polygon source colors require unit RGBA");
      }
    }
  }
  for (const auto &field : fields) {
    if (!field.is_object() || field.size() != 2 ||
        !field.at("corners").is_array() ||
        field.at("corners").size() > 2'000'000)
      throw std::invalid_argument("Invalid polygon color field record");
    auto basis = integer(field.at("basis"));
    if (basis >= bases.size())
      throw std::invalid_argument("Polygon color basis reference is missing");
    used_bases.insert(size_t(basis));
    count(entries, field.at("corners").size());
    uint64_t previous = 0;
    for (const auto &row : field.at("corners")) {
      if (!row.is_array() || row.size() != 2 || !row[1].is_array() ||
          row[1].empty() || row[1].size() > 128)
        throw std::invalid_argument("Invalid polygon current corner record");
      ordered(previous, row[0]);
      count(entries, row[1].size());
      uint64_t source_previous = 0;
      double sum = 0;
      for (const auto &weight : row[1]) {
        if (!weight.is_array() || weight.size() != 2 || !weight[1].is_number())
          throw std::invalid_argument("Invalid polygon corner color weight");
        ordered(source_previous, weight[0]);
        const auto value = weight[1].get<double>();
        if (!std::isfinite(value) || value <= 0 || value > 1)
          throw std::invalid_argument("Polygon corner weights must be convex");
        const auto &sources = bases[size_t(basis)];
        auto found = std::lower_bound(
            sources.begin(), sources.end(), source_previous,
            [](const auto &row, uint64_t id) { return integer(row[0]) < id; });
        if (found == sources.end() || integer((*found)[0]) != source_previous)
          throw std::invalid_argument("Polygon corner source color is missing");
        sum += value;
      }
      if (std::abs(sum - 1) > 1e-12)
        throw std::invalid_argument("Polygon corner weights do not sum to one");
    }
  }
  if (used_bases.size() != bases.size())
    throw std::invalid_argument("Orphan polygon color source basis");
}
PolygonColorArchive PolygonColorArchive::decode(const nlohmann::json &bases,
                                                const nlohmann::json &fields) {
  size_t entries = 0;
  preflight(bases, fields, entries);
  PolygonColorArchive archive;
  archive.entries_ = entries;
  for (const auto &input : bases) {
    auto values = std::make_shared<Basis>();
    values->reserve(input.size());
    for (const auto &row : input)
      values->push_back({integer(row[0]), row[1].get<PolygonRgba>()});
    archive.bases_.push_back(std::move(values));
  }
  for (const auto &input : fields) {
    std::vector<PolygonColorSample> corners;
    corners.reserve(input.at("corners").size());
    for (const auto &row : input.at("corners")) {
      PolygonColorSample corner;
      corner.corner = integer(row[0]);
      corner.provenance.reserve(row[1].size());
      for (const auto &weight : row[1])
        corner.provenance.emplace_back(integer(weight[0]),
                                       weight[1].get<double>());
      corners.push_back(std::move(corner));
    }
    archive.fields_.push_back(PolygonColorArchiveAccess::restore(
        archive.bases_.at(size_t(integer(input.at("basis")))),
        std::move(corners)));
  }
  return archive;
}
} // namespace detail
} // namespace dome
