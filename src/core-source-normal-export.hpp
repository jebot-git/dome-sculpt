// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "normal-frame-runtime.hpp"
#include "source-normal-sampler-frame.hpp"

namespace dome::detail {
inline const ImportedPrimitive &original_normal_primitive(
    const SourceMaterialBindings &binding, const CreatorAppearance &appearance) {
  const auto origin = binding.uv_origin ? binding.uv_origin->appearance : nullptr;
  if (!origin || !appearance.asset || !appearance.original_bytes ||
      appearance.asset != origin->asset || appearance.original_bytes != origin->original_bytes ||
      appearance.source_mesh != origin->source_mesh ||
      appearance.source_primitive != origin->source_primitive)
    throw std::invalid_argument("Normal frame lost its retained original primitive identity");
  const ImportedPrimitive *primitive = nullptr;
  for (const auto &candidate : appearance.asset->primitives)
    if (candidate.source_mesh == appearance.source_mesh &&
        candidate.source_primitive == appearance.source_primitive) {
      if (primitive)
        throw std::invalid_argument("Retained normal source primitive is ambiguous");
      primitive = &candidate;
    }
  if (!primitive)
    throw std::invalid_argument("Retained normal source primitive is absent");
  return *primitive;
}

inline void verify_core_source_normal_frame(
    const NormalFrameSet &set, const ImportedPrimitive &primitive,
    const ImportedMaterial &material, const std::function<bool()> &cancelled) {
  const auto &frame = *set.pbr;
  auto fail = [&] {
    throw std::invalid_argument(frame.generated_tangents
        ? "The generated normal frame differs from source replay"
        : "The authored normal frame differs from source replay");
  };
  if (frame.generated_tangents != source_needs_generated_tangents(primitive) ||
      frame.transport != NormalFrameTransport::FixedAttributes)
    fail();
  const auto expected = core_source_normal_sampler_frame(primitive, material, cancelled);
  if (frame.source_corners.size() != expected.base.size() ||
      frame.source_vertices.size() != expected.origin_vertices.size() ||
      set.targets.size() != expected.targets.size())
    fail();
  auto compare = [&](const auto &before, const auto &after, auto equal) {
    if (before.size() != after.size()) fail();
    for (size_t corner = 0; corner < before.size(); ++corner) {
      if ((corner & 1023) == 0 && cancelled && cancelled())
        throw std::runtime_error("Original normal frame verification cancelled");
      const auto original = frame.source_corners[corner];
      if (original >= after.size() || !equal(before[corner], after[original]))
        fail();
    }
  };
  const auto vector_equal = [](Vec3 a, Vec3 b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
  };
  compare(frame.source_vertices, expected.origin_vertices, std::equal_to{});
  compare(frame.tangents, expected.base, std::equal_to{});
  compare(frame.normals, expected.normals, vector_equal);
  for (size_t i = 0; i < set.targets.size(); ++i) {
    const auto &id = set.targets[i];
    if (id.source_target != i || !id.positions.empty())
      fail();
    compare(frame.targets[i].tangents, expected.targets[i], vector_equal);
    compare(frame.targets[i].normals, expected.normal_targets[i], vector_equal);
  }
}

// A portable core glTF consumer has explicit vertex tangents. KHR sampling
// transforms can therefore change independently of the fixed vertex frame.
// Prove both the real source directions and the coordinate stream being emitted.
inline void require_core_normal_export(const ObjectSurface &input,
    const std::function<bool()> &cancelled) {
  const auto &binding = *input.material_bindings;
  const auto &view = *input.textures;
  const auto fail = [] {
    throw std::invalid_argument("Normal texture transform needs a proven PBR consumer frame convention before export");
  };
  if (binding.has_mtoon || !binding.normal_frames->pbr ||
      binding.normal_frame != SourceMaterialNormalFrame::OriginalAttributes ||
      binding.sampling_reparameterized || binding.normal_preparation || binding.color_preparation ||
      binding.key_derivation || binding.canonical_attributes || binding.canonical_uv ||
      binding.native_paint_sampling || input.source_dormant || !input.appearance ||
      !view.source_only || view.overlay || !binding.uv_origin || !binding.uv_origin->paint)
    fail();
  const auto &appearance = *input.appearance;
  const auto &primitive = original_normal_primitive(binding, appearance);
  const auto &asset = *appearance.asset;
  const auto &raw = asset.materials.at(primitive.material);
  if (raw.normal_texture < 0 || !appearance.decoded_images)
    fail();
  const auto &texture = asset.textures.at(size_t(raw.normal_texture));
  const auto &maps = *view.source_maps;
  const auto &normal = maps.normal;
  const auto coord = imported_texture_coordinate(raw.normal_texcoord, raw.normal_transform);
  const auto &frame = *binding.normal_frames->pbr;
  if (texture.image < 0 || normal.image != appearance.decoded_images->at(size_t(texture.image)) ||
      normal.texcoord != coord || normal.wrap_s != texture.wrap_s || normal.wrap_t != texture.wrap_t ||
      normal.min_filter != texture.min_filter || normal.mag_filter != texture.mag_filter ||
      maps.normal_scale != raw.normal_scale ||
      (frame.generated_tangents && frame.generated_uv != uint32_t(coord)))
    fail();
  const auto &original = coord == 0 ? primitive.mesh.uv : coord == 1 ? primitive.uv1 :
                        primitive.extra_uv_sets.at(uint32_t(coord));
  maps.validate_surface(input.mesh);
  const auto &current = coord == 0 ? input.mesh.uv : coord == 1 ? *maps.uv1 :
                       *maps.extra_uv_sets.at(uint32_t(coord));
  if (frame.source_vertices.size() != input.mesh.indices.size() ||
      appearance.source_vertices.size() != input.mesh.positions.size())
    fail();
  for (size_t corner = 0; corner < input.mesh.indices.size(); ++corner) {
    if ((corner & 1023) == 0 && cancelled && cancelled())
      throw std::runtime_error("Core normal sampler export cancelled");
    const auto vertex = input.mesh.indices[corner];
    const auto source = frame.source_vertices[corner];
    if (appearance.source_vertices.at(vertex) != source)
      throw std::invalid_argument("Export normal sampler lost its source corner identity");
    const auto a = input.mesh.positions.at(vertex), b = primitive.mesh.positions.at(source);
    if (a.x != b.x || a.y != b.y || a.z != b.z)
      throw std::invalid_argument("Export normal frame rest position differs from the original source");
    if (current.at(vertex) != original.at(source))
      throw std::invalid_argument("Export normal sampler coordinates differ from the original source");
  }
  verify_core_source_normal_frame(*binding.normal_frames, primitive, raw, cancelled);
}
} // namespace dome::detail
