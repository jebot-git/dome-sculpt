// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bone-source-shadow.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/scene.hpp"
#include "volume-topology.hpp"
#include <map>
#include <nlohmann/json.hpp>
#include <numeric>
namespace dome::detail {
struct MaterialVisibilitySeams {
  std::vector<uint32_t> groups;
  size_t joined_edges = 0, rejected_edges = 0, ambiguous_edges = 0,
         planned_bytes = 0;
};
// Visibility incidence ONLY. Matrix rows/edges and source geometry remain
// separate. The raw source owner is the SAME canonical node/mesh/POSITION and
// skin accessor set, not a coordinate bucket spanning retained source assets.
// Each union additionally needs an actual oppositely oriented boundary edge,
// exact current skin/morph continuity and no additional triangle contact.
template <class Part, class Budget>
MaterialVisibilitySeams material_visibility_seams(
    std::span<const Part> parts, std::span<const bone_shadow::Point> points,
    std::span<const uint32_t> indices, std::span<const uint32_t> row_owner,
    size_t maximum_bytes, Budget &w) {
  using P = bone_shadow::Point;
  struct Source {
    int node = -1, mesh = -1, position = -1;
    std::map<std::string, uint32_t> skin;
    bool operator<(const Source &b) const {
      if (node != b.node)
        return node < b.node;
      if (mesh != b.mesh)
        return mesh < b.mesh;
      if (position != b.position)
        return position < b.position;
      return skin < b.skin;
    }
    bool operator==(const Source &b) const {
      return node == b.node && mesh == b.mesh && position == b.position &&
             skin == b.skin;
    }
  };
  if (parts.empty() || points.empty() || row_owner.size() != points.size() ||
      indices.size() % 3)
    throw std::invalid_argument(
        "Invalid captured material incidence dimensions");
  MaterialVisibilitySeams out;
  out.planned_bytes =
      points.size() * 32 + indices.size() * 192 + parts.size() * 8192 + 65536;
  if (out.planned_bytes > maximum_bytes)
    throw std::length_error("Material incidence bytes before copies");
  for (const auto &part : parts)
    if (!part.object || !part.object->painted || !part.object->appearance ||
        !part.object->deformation || !part.object->deformation->skeleton ||
        !part.object->deformation->skin ||
        part.raw_to_row.size() != part.object->painted->mesh().positions.size())
      throw std::invalid_argument("Material incidence missing captured owner");
  const auto &first = *parts[0].object->appearance;
  if (!first.original_bytes || first.original_bytes->size() < 20)
    throw std::invalid_argument(
        "Material incidence lacks canonical GLB authority");
  const auto &bytes = *first.original_bytes;
  auto u32 = [&](size_t at) {
    return uint32_t(bytes[at]) | (uint32_t(bytes[at + 1]) << 8) |
           (uint32_t(bytes[at + 2]) << 16) | (uint32_t(bytes[at + 3]) << 24);
  };
  const size_t json_bytes = u32(12);
  if (u32(0) != 0x46546c67 || u32(4) != 2 || u32(16) != 0x4e4f534a ||
      json_bytes > 8u * 1024u * 1024u || json_bytes > bytes.size() - 20 ||
      json_bytes > (maximum_bytes - out.planned_bytes) / 32)
    throw std::invalid_argument("Material incidence invalid canonical closure");
  out.planned_bytes += json_bytes * 32;
  w.step(json_bytes * 16);
  auto json = nlohmann::json::parse(bytes.begin() + 20,
                                    bytes.begin() + 20 + json_bytes);
  std::vector<Source> sources;
  const auto &first_object = *parts[0].object;
  if (!first_object.deformation || !first_object.deformation->skeleton)
    throw std::invalid_argument(
        "Material incidence lacks current bind authority");
  auto rig = first_object.deformation->skeleton->encode();
  for (const auto &part : parts) {
    const auto &o = *part.object;
    const auto &a = *o.appearance;
    w.step(64 + o.deformation->skeleton->bones.size() *
                    o.deformation->skeleton->bones.size());
    if (o.deformation->skeleton->encode() != rig)
      throw std::invalid_argument("Material incidence current bind differs");
    auto same = [](const auto &x, const auto &y) {
      return x.get() == y.get() && !x.owner_before(y) && !y.owner_before(x);
    };
    if (!same(a.asset, first.asset) ||
        !same(a.original_bytes, first.original_bytes) || a.source_mesh < 0 ||
        a.source_primitive < 0 || a.source_node < 0)
      throw std::invalid_argument("Material incidence foreign raw owner");
    const auto &primitive = json.at("meshes")
                                .at(a.source_mesh)
                                .at("primitives")
                                .at(a.source_primitive);
    Source source;
    source.node = a.source_node;
    source.mesh = a.source_mesh;
    const auto &attributes = primitive.at("attributes");
    source.position = attributes.at("POSITION").template get<int>();
    if (json.at("nodes").at(a.source_node).at("mesh").template get<int>() !=
            a.source_mesh ||
        a.asset->nodes.at(a.source_node).mesh != a.source_mesh ||
        a.source_joint_nodes.size() != o.deformation->skeleton->bones.size())
      throw std::invalid_argument(
          "Material incidence raw node/current joint identities differ");
    const auto &accessor = json.at("accessors").at(source.position);
    const auto &raw = a.asset->primitives.at(
        a.asset->meshes.at(a.source_mesh).primitives.at(a.source_primitive));
    if (accessor.at("type") != "VEC3" || accessor.at("componentType") != 5126 ||
        accessor.at("count").template get<size_t>() !=
            raw.mesh.positions.size())
      throw std::invalid_argument("Material incidence raw POSITION mismatch");
    for (auto at = attributes.begin(); at != attributes.end(); ++at) {
      w.step();
      const auto &key = at.key();
      if (key.starts_with("JOINTS_") || key.starts_with("WEIGHTS_"))
        source.skin.emplace(key, at.value().template get<uint32_t>());
    }
    if (source.skin.empty())
      throw std::invalid_argument(
          "Material incidence lacks original skin identities");
    sources.push_back(std::move(source));
  }
  size_t key_bytes = 512;
  for (const auto &source : sources) {
    size_t b = sizeof(Source);
    for (const auto &entry : source.skin)
      b += 256 + entry.first.size();
    key_bytes = std::max(key_bytes, b * 2 + 512);
  }
  if (indices.size() > (maximum_bytes - out.planned_bytes) / key_bytes)
    throw std::length_error(
        "Material incidence boundary-key quota before maps");
  out.planned_bytes += indices.size() * key_bytes;
  out.groups.resize(points.size());
  std::iota(out.groups.begin(), out.groups.end(), 0);
  std::vector<uint32_t> raw_row(points.size(), UINT32_MAX);
  for (size_t owner = 0; owner < parts.size(); ++owner)
    for (uint32_t raw = 0; raw < parts[owner].raw_to_row.size(); ++raw) {
      w.step();
      auto row = parts[owner].raw_to_row[raw];
      if (row >= points.size() || row_owner[row] != owner)
        throw std::invalid_argument(
            "Material incidence row ownership mismatch");
      const auto p = parts[owner].object->painted->mesh().positions[raw];
      if (points[row] != P{p.x, p.y, p.z})
        throw std::invalid_argument(
            "Material incidence current REST correspondence differs");
      if (raw_row[row] == UINT32_MAX)
        raw_row[row] = raw;
    }
  auto root = [&](uint32_t i) {
    while (i != out.groups[i]) {
      w.step();
      out.groups[i] = out.groups[out.groups[i]];
      i = out.groups[i];
    }
    return i;
  };
  struct Edge {
    uint32_t a = 0, b = 0, face = 0, count = 0;
  };
  struct Compare {
    Budget *w;
    bool operator()(const std::array<uint32_t, 2> &a,
                    const std::array<uint32_t, 2> &b) const {
      w->step();
      return a < b;
    }
  };
  std::map<std::array<uint32_t, 2>, Edge, Compare> raw_edges(Compare{&w});
  for (size_t c = 0; c < indices.size(); c += 3)
    for (size_t k = 0; k < 3; ++k) {
      w.step(4);
      auto a = indices[c + k], b = indices[c + (k + 1) % 3];
      if (a >= points.size() || b >= points.size() ||
          row_owner[a] != row_owner[b])
        throw std::invalid_argument(
            "Material incidence fabricated cross-owner face");
      auto &edge = raw_edges[{std::min(a, b), std::max(a, b)}];
      if (!edge.count)
        edge = {a, b, uint32_t(c / 3), 0};
      ++edge.count;
    }
  struct Boundary {
    Source source;
    std::array<P, 2> points;
  };
  struct BoundaryLess {
    Budget *w;
    bool operator()(const Boundary &a, const Boundary &b) const {
      w->step(64 + a.source.skin.size() + b.source.skin.size());
      if (a.source < b.source)
        return true;
      if (b.source < a.source)
        return false;
      return a.points < b.points;
    }
  };
  struct Pair {
    Edge first{}, second{};
    size_t count = 0;
  };
  std::map<Boundary, Pair, BoundaryLess> boundaries(BoundaryLess{&w});
  for (const auto &[key, edge] : raw_edges)
    if (edge.count == 1) {
      w.step();
      (void)key;
      auto a = points[edge.a], b = points[edge.b];
      auto &pair = boundaries[{sources[row_owner[edge.a]],
                               {std::min(a, b), std::max(a, b)}}];
      if (!pair.count)
        pair.first = edge;
      else if (pair.count == 1)
        pair.second = edge;
      ++pair.count;
    }
  auto equivalent = [&](uint32_t a, uint32_t b) {
    w.step(16);
    const auto &x = *parts[row_owner[a]].object,
               &y = *parts[row_owner[b]].object;
    if (points[a] != points[b] ||
        !(sources[row_owner[a]] == sources[row_owner[b]]) ||
        x.appearance->source_joint_nodes != y.appearance->source_joint_nodes ||
        x.appearance->source_morph_targets !=
            y.appearance->source_morph_targets)
      return false;
    const auto ra = raw_row[a], rb = raw_row[b];
    const auto &dx = *x.deformation, &dy = *y.deformation;
    const auto &sx = dx.skin->vertices.at(ra), &sy = dy.skin->vertices.at(rb);
    w.step(sx.size() * sy.size());
    if (sx.size() != sy.size())
      return false;
    for (auto p : sx)
      if (std::find(sy.begin(), sy.end(), p) == sy.end())
        return false;
    if (bool(dx.morphs) != bool(dy.morphs))
      return false;
    if (dx.morphs) {
      if (dx.morphs->targets.size() != dy.morphs->targets.size() ||
          dx.morphs->targets.size() !=
              x.appearance->source_morph_targets.size())
        return false;
      for (size_t t = 0; t < dx.morphs->targets.size(); ++t) {
        w.step(4);
        auto p = dx.morphs->targets[t].delta.at(ra),
             q = dy.morphs->targets[t].delta.at(rb);
        if (p.x != q.x || p.y != q.y || p.z != q.z)
          return false;
      }
    }
    return true;
  };
  for (const auto &[key, pair] : boundaries) {
    w.step();
    (void)key;
    if (pair.count != 2) {
      out.ambiguous_edges += pair.count > 2;
      continue;
    }
    const auto &a = pair.first, &b = pair.second;
    if (row_owner[a.a] == row_owner[b.a])
      continue;
    auto triangle = [&](uint32_t face) {
      return volume_topology::Triangle{points[indices[size_t(face) * 3]],
                                       points[indices[size_t(face) * 3 + 1]],
                                       points[indices[size_t(face) * 3 + 2]]};
    };
    if (points[a.a] != points[b.b] || points[a.b] != points[b.a] ||
        !equivalent(a.a, b.b) || !equivalent(a.b, b.a) ||
        volume_topology::unexpected_contact(triangle(a.face), triangle(b.face),
                                            w)) {
      ++out.rejected_edges;
      continue;
    }
    out.groups[root(a.a)] = root(b.b);
    out.groups[root(a.b)] = root(b.a);
    ++out.joined_edges;
  }
  for (uint32_t i = 0; i < out.groups.size(); ++i) {
    w.step();
    out.groups[i] = root(i);
  }
  return out;
}
} // namespace dome::detail
