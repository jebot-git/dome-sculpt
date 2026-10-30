// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/deformation.hpp"
#include "dome/import.hpp"
#include "source-frame-skin.hpp"
#include "text-utf8.hpp"
#include <map>
#include <set>
namespace dome::detail {
inline RigMatrix imported_inverse(const RigMatrix &m) {
  auto inverse = source_frame_inverse_transpose(m);
  std::swap(inverse[1], inverse[4]);
  std::swap(inverse[2], inverse[8]);
  std::swap(inverse[6], inverse[9]);
  Vec3 t{m[12], m[13], m[14]};
  auto p = source_frame_direction(inverse, t);
  inverse[12] = -p.x;
  inverse[13] = -p.y;
  inverse[14] = -p.z;
  return inverse;
}
inline JointTransform imported_node_reference(const ImportedNode &node) {
  if (!node.has_matrix)
    return {{node.translation[0], node.translation[1], node.translation[2]},
            {node.rotation[3], node.rotation[0], node.rotation[1],
             node.rotation[2]},
            {node.scale[0], node.scale[1], node.scale[2]}};
  const auto &m = node.local;
  Vec3 x{m[0], m[1], m[2]}, y{m[4], m[5], m[6]}, z{m[8], m[9], m[10]};
  float sx = length(x), sy = length(y), sz = length(z);
  if (std::min({sx, sy, sz}) < 1e-4f)
    throw std::runtime_error(
        "Imported joint matrix is singular or below native scale bounds");
  if (dot(x, cross(y, z)) < 0)
    sx = -sx;
  x = x / sx;
  y = y / sy;
  z = z / sz;
  if (std::max({std::abs(dot(x, y)), std::abs(dot(y, z)),
                std::abs(dot(z, x))}) > 2e-6f)
    throw std::runtime_error(
        "Imported local joint matrix is not glTF TRS-decomposable");
  float a = x.x, b = y.x, c = z.x, d = x.y, e = y.y, f = z.y, g = x.z, h = y.z,
        i = z.z;
  Quat q;
  float trace = a + e + i;
  if (trace > 0) {
    float s = std::sqrt(trace + 1) * 2;
    q = {s * .25f, (h - f) / s, (c - g) / s, (d - b) / s};
  } else if (a > e && a > i) {
    float s = std::sqrt(1 + a - e - i) * 2;
    q = {(h - f) / s, s * .25f, (b + d) / s, (c + g) / s};
  } else if (e > i) {
    float s = std::sqrt(1 + e - a - i) * 2;
    q = {(c - g) / s, (b + d) / s, s * .25f, (f + h) / s};
  } else {
    float s = std::sqrt(1 + i - a - e) * 2;
    q = {(d - b) / s, (c + g) / s, (f + h) / s, s * .25f};
  }
  q = q.unit();
  return {{m[12], m[13], m[14]}, q, {sx, sy, sz}};
}
struct ImportedRig {
  Skeleton skeleton;
  std::vector<uint32_t> source_nodes, slots;
};
inline ImportedRig imported_rig(const ImportedAsset &asset, uint32_t mesh_node,
                                const std::function<bool()> &cancelled) {
  auto stop = [&] {
    if (cancelled && cancelled())
      throw std::runtime_error("Imported rig adoption cancelled");
  };
  stop();
  const auto &node = asset.nodes.at(mesh_node);
  if (node.skin < 0)
    throw std::invalid_argument("Imported skin instance has no skin identity");
  const auto &skin = asset.skins.at(size_t(node.skin));
  std::set<uint32_t> nodes, joints(skin.joints.begin(), skin.joints.end());
  for (auto n : skin.joints)
    for (int p = int(n); p >= 0; p = asset.nodes.at(size_t(p)).parent) {
      stop();
      nodes.insert(uint32_t(p));
      if (nodes.size() > 256)
        throw std::length_error(
            "Imported joint ancestor hierarchy exceeds native256-bone limit");
    }
  ImportedRig out;
  out.source_nodes.assign(nodes.begin(), nodes.end());
  std::map<uint32_t, uint32_t> indices;
  for (size_t i = 0; i < out.source_nodes.size(); ++i)
    indices.emplace(out.source_nodes[i], uint32_t(i));
  for (auto source : out.source_nodes) {
    stop();
    const auto &n = asset.nodes.at(source);
    Bone b;
    if (!text::valid_utf8(n.name) || n.name.find('\0') != std::string::npos)
      throw std::runtime_error("Imported bone name is not valid UTF-8");
    b.name = text::utf8_prefix(n.name.empty() ? "Source node" : n.name, 210) +
             " [" + std::to_string(source) + "]";
    b.parent = n.parent < 0 ? -1 : int(indices.at(uint32_t(n.parent)));
    b.rest = imported_node_reference(n);
    b.rest_matrix = n.local;
    b.deform = joints.contains(source);
    out.skeleton.bones.push_back(std::move(b));
  }
  constexpr RigMatrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  out.skeleton.inverse_bind_override =
      std::vector<RigMatrix>(nodes.size(), identity);
  for (size_t slot = 0; slot < skin.joints.size(); ++slot) {
    stop();
    auto bone = indices.at(skin.joints[slot]);
    out.slots.push_back(bone);
    if (skin.inverse_bind_matrices_authored)
      (*out.skeleton.inverse_bind_override)[bone] =
          skin.inverse_bind_matrices.at(slot);
  }
  // Positions remain in original mesh coordinates. The node's original world
  // cancels in skinning; retain a separate exact space instead of a fake joint.
  out.skeleton.space_matrix = imported_inverse(node.world);
  out.skeleton.validate();
  return out;
}
inline SkinWeights imported_weights(const ImportedPrimitive &primitive,
                                    const ImportedRig &rig,
                                    std::span<const uint32_t> remap,
                                    const std::function<bool()> &cancelled) {
  SkinWeights out;
  out.vertices.reserve(remap.size());
  size_t entries = 0;
  for (size_t v = 0; v < remap.size(); ++v) {
    if ((v & 511) == 0 && cancelled && cancelled())
      throw std::runtime_error("Imported skin weights cancelled");
    VertexWeights weights;
    for (const auto &set : primitive.influences)
      for (size_t k = 0; k < 4; ++k) {
        float value = set.weights.at(remap[v])[k];
        if (value != 0) {
          if (++entries > 8'000'000)
            throw std::length_error(
                "Imported skin weights exceed native entry budget");
          weights.push_back({rig.slots.at(set.joints.at(remap[v])[k]), value});
        }
      }
    out.vertices.push_back(std::move(weights));
  }
  // Do not prune, normalize, sort, or invent zero-weight joint influences.
  out.validate(remap.size(), rig.skeleton.bones.size());
  return out;
}
} // namespace dome::detail
