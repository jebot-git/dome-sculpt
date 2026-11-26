// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "import-texture-transform.hpp"
#include "source-tangent-frames.hpp"

namespace dome::detail {
inline bool source_needs_generated_tangents(const ImportedPrimitive &source) {
  return !source.normals_authored || source.tangents.empty();
}

inline SourceTangentFrames core_source_normal_sampler_frame(
    const ImportedPrimitive &source, const ImportedMaterial &material,
    const std::function<bool()> &cancelled) {
  const auto set = imported_texture_coordinate(material.normal_texcoord,
                                               material.normal_transform);
  const auto &uv = set == 0 ? source.mesh.uv
                 : set == 1 ? source.uv1
                            : source.extra_uv_sets.at(uint32_t(set));
  const auto matrix = imported_texture_matrix(material.normal_transform);
  if (!source_needs_generated_tangents(source) ||
      (matrix[0] == 1 && matrix[1] == 0 && matrix[2] == 0 && matrix[3] == 1))
    return source_tangent_frames(source, uv, cancelled);
  const double determinant = double(matrix[0]) * matrix[3] - double(matrix[1]) * matrix[2];
  if (determinant == 0 || uv.size() != source.mesh.positions.size() || uv.size() > 2'000'000)
    throw std::invalid_argument("Generated core normal tangents require nonsingular source sampling coordinates");
  std::vector<std::array<float,2>> effective;
  effective.reserve(uv.size());
  for (size_t index = 0; index < uv.size(); ++index) {
    if ((index & 1023) == 0 && cancelled && cancelled())
      throw std::runtime_error("Source normal sampler frame generation cancelled");
    const auto [u, v] = uv[index];
    // Translation affects the samples but not the derivatives. Omitting it
    // also avoids losing small UV differences next to a large offset.
    effective.push_back({float(double(matrix[0]) * u + double(matrix[1]) * v),
                         float(double(matrix[2]) * u + double(matrix[3]) * v)});
  }
  return source_tangent_frames(source, effective, cancelled);
}
} // namespace dome::detail
