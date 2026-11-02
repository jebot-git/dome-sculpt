// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/normal-preparation.hpp"
#include "dome/source-maps.hpp"

namespace dome::detail {

// Detached prototype. This is not yet admitted by Paint/Scene codecs or the
// worker. The sampling field and the editable atlas are separate authorities.
// It deliberately owns no new material/source identity and never resamples an
// image merely because the writable atlas has changed.
struct NormalSamplingLimits {
  size_t max_vertices = 2'000'000;
  size_t max_indices = 6'000'000;
  size_t max_bytes = 256 * 1024 * 1024;
  size_t max_depth = 32;
  std::function<bool()> cancelled;
};

struct NormalSamplingDomain {
  std::shared_ptr<const PaintedMesh> original_paint;
  std::shared_ptr<const PaintNormalProjection> intrinsic;
  // A real existing bake must retain the resolved quantize-before-filter
  // plane too. Sampling intrinsic/detail separately would change its model.
  std::shared_ptr<const Image> resolved_pbr, resolved_mtoon;
  std::shared_ptr<const BakedTextures> original_bake;
  std::shared_ptr<const NormalSamplingDomain> before;
  std::shared_ptr<const Mesh> rest;
  std::vector<uint32_t> source_vertices;
  std::vector<std::array<float, 2>> coordinates;
  size_t depth = 1;
  size_t owned_bytes = 0, aggregate_bytes = 0;

  // The caller must separately authenticate the current material, deformation
  // and Paint pairing before attaching this to a product model. This helper
  // proves only the exact immutable geometry/normal-field correspondence.
  static std::shared_ptr<const NormalSamplingDomain> capture(
      const PaintedMesh &paint, const Material &material, const Mesh &target,
      std::span<const uint32_t> source, const NormalSamplingLimits &limits = {}) {
    stop(limits);
    if (paint.in_stroke() || !paint.normal_projection() ||
        paint.normal_projection()->version != 1)
      throw std::invalid_argument("Normal sampling capture needs a finished, explicit version1 field");
    paint.normal_projection()->validate(paint.resolution());
    const size_t image_bytes = size_t(paint.resolution()) * paint.resolution() * 8;
    const auto bytes = bound(target, source, limits, image_bytes);
    auto out = std::make_shared<NormalSamplingDomain>();
    out->owned_bytes = out->aggregate_bytes = bytes;
    out->original_paint = paint.snapshot();
    out->intrinsic = out->original_paint->normal_projection();
    out->original_bake = out->original_paint->baked();
    auto maps = out->original_paint->preview(material);
    if (maps->normal.rgba.empty() || maps->mtoon_normal.rgba.empty())
      throw std::invalid_argument("Normal sampling capture lost its resolved role fields");
    out->resolved_pbr = std::shared_ptr<const Image>(maps, &maps->normal);
    out->resolved_mtoon = std::shared_ptr<const Image>(maps, &maps->mtoon_normal);
    map(*out, paint.mesh(), target, source, nullptr, limits);
    return out;
  }

  static std::shared_ptr<const NormalSamplingDomain> reparameterize(
      std::shared_ptr<const NormalSamplingDomain> input, const Mesh &target,
      std::span<const uint32_t> source, const NormalSamplingLimits &limits = {}) {
    stop(limits);
    if (!input || !input->rest || !input->original_paint ||
        !input->intrinsic || input->coordinates.size() != input->rest->positions.size())
      throw std::invalid_argument("Normal sampling source domain is incomplete");
    if (input->depth >= limits.max_depth)
      throw std::length_error("Normal sampling domain history exceeds its depth budget");
    const auto bytes = bound(target, source, limits, input->aggregate_bytes);
    auto out = std::make_shared<NormalSamplingDomain>();
    out->owned_bytes = bytes - input->aggregate_bytes;
    out->aggregate_bytes = bytes;
    out->original_paint = input->original_paint;
    out->intrinsic = input->intrinsic;
    out->resolved_pbr = input->resolved_pbr;
    out->resolved_mtoon = input->resolved_mtoon;
    out->original_bake = input->original_bake;
    out->before = input;
    out->depth = input->depth + 1;
    map(*out, *input->rest, target, source, &input->coordinates, limits);
    return out;
  }

private:
  static void stop(const NormalSamplingLimits &limits) {
    if (limits.cancelled && limits.cancelled())
      throw std::runtime_error("Normal sampling correspondence cancelled");
  }
  static size_t bound(const Mesh &target, std::span<const uint32_t> source,
                    const NormalSamplingLimits &limits, size_t extra) {
    if (!limits.max_vertices || limits.max_vertices > 2'000'000 ||
        !limits.max_indices || limits.max_indices > 6'000'000 ||
        !limits.max_bytes || limits.max_bytes > 512 * 1024 * 1024 ||
        !limits.max_depth || limits.max_depth > 64)
      throw std::invalid_argument("Invalid normal sampling resource policy");
    if (target.positions.size() > limits.max_vertices ||
        target.indices.size() > limits.max_indices || source.size() != target.positions.size() ||
        (!target.uv_pins.empty() && target.uv_pins.size() != target.positions.size()) ||
        (!target.uv_seams.empty() && target.uv_seams.size() != target.indices.size()))
      throw std::length_error("Normal sampling geometry exceeds its resource policy");
    // All counts are capped above before multiplication. Include both the
    // immutable copied mesh and explicit identity/sampling tables.
    const size_t bytes = target.positions.size() * (2 * sizeof(Vec3) +
        2 * sizeof(std::array<float, 2>) + sizeof(uint32_t)) +
        target.indices.size() * sizeof(uint32_t) +
        target.uv_pins.size() + target.uv_seams.size();
    if (extra > limits.max_bytes || bytes > limits.max_bytes - extra)
      throw std::length_error("Normal sampling correspondence exceeds its storage budget");
    target.validate();
    return bytes + extra;
  }
  static void map(NormalSamplingDomain &out, const Mesh &before,
      const Mesh &target, std::span<const uint32_t> source,
      const std::vector<std::array<float, 2>> *previous,
      const NormalSamplingLimits &limits) {
    if (target.indices.size() != before.indices.size() ||
        target.uv.size() != target.positions.size() ||
        before.uv.size() != before.positions.size())
      throw std::invalid_argument("Normal sampling needs exact ordered corner topology");
    out.coordinates.reserve(source.size());
    auto equal = [](Vec3 a, Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; };
    for (size_t v = 0; v < source.size(); ++v) {
      if ((v & 1023) == 0) stop(limits);
      const auto original = source[v];
      if (original >= before.positions.size() ||
          !equal(target.positions[v], before.positions[original]) ||
          !equal(target.normals[v], before.normals[original]))
        throw std::invalid_argument("Normal sampling cannot invent changed REST geometry or directions");
      const auto uv = previous ? previous->at(original) : before.uv[original];
      for (const auto q : uv)
        if (!std::isfinite(q) || q < 0 || q > 1)
          throw std::invalid_argument("Normal sampling source coordinate is outside its declared field");
      out.coordinates.push_back(uv);
    }
    for (size_t c = 0; c < target.indices.size(); ++c) {
      if ((c & 1023) == 0) stop(limits);
      if (source[target.indices[c]] != before.indices[c])
        throw std::invalid_argument("Normal sampling lost its exact ordered source corner");
    }
    out.rest = std::make_shared<const Mesh>(target);
    out.source_vertices.assign(source.begin(), source.end());
    stop(limits);
  }
};
} // namespace dome::detail
