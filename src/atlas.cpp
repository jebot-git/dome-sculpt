// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/mesh.hpp"
#include "xatlas.h"
#include <memory>
namespace dome {
AtlasInfo unwrap(Mesh &mesh, uint32_t resolution, uint32_t padding) {
  mesh.validate();
  if (resolution < 64 || resolution > 8192 || padding > resolution / 8)
    throw std::invalid_argument("Invalid atlas resolution or padding");
  std::unique_ptr<xatlas::Atlas, decltype(&xatlas::Destroy)> atlas(
      xatlas::Create(), xatlas::Destroy);
  if (!atlas)
    throw std::bad_alloc();
  // xatlas rejects triangles using an absolute area epsilon. Authoring units
  // are metres, and valid small sculpt features can fall below that epsilon.
  // Parameterize normalized positions, then keep original metre-space geometry
  // through xref. Do not discard tiny faces and leave holes in the export.
  Vec3 low = mesh.positions.front(), high = low;
  for (auto p : mesh.positions) {
    low = {std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z)};
    high = {std::max(high.x, p.x), std::max(high.y, p.y),
            std::max(high.z, p.z)};
  }
  float minimum_area = INFINITY;
  for (size_t i = 0; i < mesh.indices.size(); i += 3) {
    auto a = mesh.positions[mesh.indices[i]],
         b = mesh.positions[mesh.indices[i + 1]],
         c = mesh.positions[mesh.indices[i + 2]];
    minimum_area = std::min(minimum_area, length(cross(b - a, c - a)) * .5f);
  }
  float extent = std::max({high.x - low.x, high.y - low.y, high.z - low.z});
  float scale = std::max(1024.f / extent, std::sqrt(.0001f / minimum_area));
  Vec3 center = (low + high) * .5f;
  std::vector<Vec3> normalized_positions;
  normalized_positions.reserve(mesh.positions.size());
  for (auto p : mesh.positions)
    normalized_positions.push_back((p - center) * scale);
  xatlas::MeshDecl input{};
  input.vertexCount = uint32_t(mesh.positions.size());
  input.vertexPositionData = normalized_positions.data();
  input.vertexPositionStride = sizeof(Vec3);
  input.vertexNormalData = mesh.normals.data();
  input.vertexNormalStride = sizeof(Vec3);
  input.indexCount = uint32_t(mesh.indices.size());
  input.indexData = mesh.indices.data();
  input.indexFormat = xatlas::IndexFormat::UInt32;
  auto error = xatlas::AddMesh(atlas.get(), input);
  if (error != xatlas::AddMeshError::Success)
    throw std::runtime_error(std::string("Atlas input: ") +
                             xatlas::StringForEnum(error));
  xatlas::ChartOptions chart{};
  chart.maxIterations = 4;
  xatlas::PackOptions pack{};
  pack.resolution = resolution;
  pack.padding = padding;
  pack.bilinear = true;
  pack.rotateCharts = true;
  pack.bruteForce = true;
  xatlas::Generate(atlas.get(), chart, pack);
  if (atlas->meshCount != 1 || atlas->atlasCount != 1 || !atlas->width ||
      !atlas->height)
    throw std::runtime_error(
        "The mesh needs multiple atlases; increase the texture resolution");
  Mesh output;
  const auto &generated = atlas->meshes[0];
  output.positions.reserve(generated.vertexCount);
  output.normals.reserve(generated.vertexCount);
  output.uv.reserve(generated.vertexCount);
  for (uint32_t i = 0; i < generated.vertexCount; ++i) {
    const auto &v = generated.vertexArray[i];
    if (v.xref >= mesh.positions.size() || v.atlasIndex != 0)
      throw std::runtime_error("Invalid generated chart mapping");
    output.positions.push_back(mesh.positions[v.xref]);
    output.normals.push_back(mesh.normals[v.xref]);
    output.uv.push_back({v.uv[0] / atlas->width, v.uv[1] / atlas->height});
  }
  output.indices.assign(generated.indexArray,
                        generated.indexArray + generated.indexCount);
  output.validate();
  AtlasInfo info{atlas->width, atlas->height, atlas->chartCount,
                 atlas->utilization[0]};
  mesh = std::move(output);
  return info;
}
} // namespace dome
