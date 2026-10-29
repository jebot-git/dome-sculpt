// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/tangents.hpp"
#include "mikktspace.h"
namespace dome {
std::vector<Tangent> tangent_corners(const Mesh &mesh,
                                     const std::function<bool()> &cancelled) {
  auto check = [&] {
    if (cancelled && cancelled())
      throw std::runtime_error("Tangent generation cancelled");
  };
  check();
  if (mesh.positions.size() > 2'000'000 || mesh.indices.size() > 6'000'000)
    throw std::length_error("Tangent mesh exceeds surface limits");
  mesh.validate();
  if (mesh.uv.size() != mesh.positions.size())
    throw std::invalid_argument("Tangent generation requires UV coordinates");
  for (auto n : mesh.normals)
    if (length(n) < 1e-8f)
      throw std::invalid_argument("Tangent generation requires nonzero normals");
  struct Data {
    const Mesh &mesh;
    std::vector<Tangent> result;
  } data{mesh, std::vector<Tangent>(mesh.indices.size())};
  SMikkTSpaceInterface api{};
  api.m_getNumFaces = [](const SMikkTSpaceContext *c) {
    return int(static_cast<Data *>(c->m_pUserData)->mesh.indices.size() / 3);
  };
  api.m_getNumVerticesOfFace = [](const SMikkTSpaceContext *, int) { return 3; };
  api.m_getPosition = [](const SMikkTSpaceContext *c, float *out, int f, int v) {
    auto &m = static_cast<Data *>(c->m_pUserData)->mesh;
    auto p = m.positions[m.indices[size_t(f) * 3 + v]];
    out[0] = p.x; out[1] = p.y; out[2] = p.z;
  };
  api.m_getNormal = [](const SMikkTSpaceContext *c, float *out, int f, int v) {
    auto &m = static_cast<Data *>(c->m_pUserData)->mesh;
    auto n = normalized(m.normals[m.indices[size_t(f) * 3 + v]]);
    out[0] = n.x; out[1] = n.y; out[2] = n.z;
  };
  api.m_getTexCoord = [](const SMikkTSpaceContext *c, float *out, int f, int v) {
    auto &m = static_cast<Data *>(c->m_pUserData)->mesh;
    auto uv = m.uv[m.indices[size_t(f) * 3 + v]];
    out[0] = uv[0]; out[1] = uv[1];
  };
  api.m_setTSpaceBasic = [](const SMikkTSpaceContext *c, const float *t,
                            float sign, int f, int v) {
    static_cast<Data *>(c->m_pUserData)->result[size_t(f) * 3 + v] =
        {t[0], t[1], t[2], sign};
  };
  SMikkTSpaceContext context{&api, &data};
  check();
  // Upstream has no cancellation hook inside its sorting/grouping phases.
  // Keep it bounded, on workers, and reject obsolete results immediately after.
  if (!genTangSpaceDefault(&context))
    throw std::runtime_error("MikkTSpace tangent generation failed");
  check();
  for (auto t : data.result)
    if (!finite(Vec3{t[0], t[1], t[2]}) ||
        std::abs(length(Vec3{t[0], t[1], t[2]}) - 1) > .001f ||
        std::abs(std::abs(t[3]) - 1) > 1e-5f)
      throw std::runtime_error("MikkTSpace produced an invalid tangent frame");
  return std::move(data.result);
}
TangentFrame tangent_frame(const Mesh &mesh, std::span<const Tangent> corners,
                           size_t triangle, std::array<float, 3> barycentric) {
  if (corners.size() != mesh.indices.size() || triangle >= mesh.indices.size()/3)
    throw std::invalid_argument("Tangent corner identities differ from the mesh");
  TangentFrame result;
  float hand = corners[triangle * 3][3];
  for (int i = 0; i < 3; ++i) {
    auto v = mesh.indices[triangle * 3 + i];
    auto t = corners[triangle * 3 + i];
    if (t[3] * hand < 0)
      throw std::runtime_error("Tangent handedness changes inside a triangle");
    auto n = normalized(mesh.normals[v]);
    auto tangent = Vec3{t[0], t[1], t[2]};
    result.normal += n * barycentric[i];
    result.tangent += tangent * barycentric[i];
    result.bitangent += cross(n, tangent) * (t[3] * barycentric[i]);
  }
  return result;
}
Vec3 TangentFrame::to_world(Vec3 value) const {
  auto result = tangent * value.x + bitangent * value.y + normal * value.z;
  if (!finite(result) || length(result) < 1e-10f)
    throw std::runtime_error("Invalid tangent-space normal");
  return normalized(result);
}
Vec3 TangentFrame::to_tangent(Vec3 value) const {
  auto x = cross(bitangent, normal), y = cross(normal, tangent),
       z = cross(tangent, bitangent);
  float determinant = dot(tangent, x);
  if (!std::isfinite(determinant) || std::abs(determinant) < 1e-8f)
    throw std::runtime_error("Singular interpolated tangent frame");
  auto result = Vec3{dot(value, x), dot(value, y), dot(value, z)} / determinant;
  if (!finite(result) || length(result) < 1e-10f)
    throw std::runtime_error("Invalid object-space normal");
  return normalized(result);
}
} // namespace dome
