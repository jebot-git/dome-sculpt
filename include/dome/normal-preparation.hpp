// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "color-preparation.hpp"
namespace dome {
// A named consumer contract, separate from attribute transport. This does not
// assert that every glTF/Unity renderer uses identical normalization order.
enum class PreparedPbrNormalEvaluation { SampleRenderer2026 = 1 };
enum class PreparedMToonNormalEvaluation { Absent, LegacyRawInterpolated, MToon1 };
struct NormalPreparationReceipt {
  uint32_t version=1,resolution=0;
  PreparedPbrNormalEvaluation pbr=PreparedPbrNormalEvaluation::SampleRenderer2026;
  PreparedMToonNormalEvaluation mtoon=PreparedMToonNormalEvaluation::Absent;
  bool authored_native_normals=false,authored_native_tangents=false;
  // The observed initial conversion/quantization samples are not an all-pose
  // bound. Adoption must separately satisfy the declared route's authority and
  // quality contract; dynamic native/export parity has independent fixtures.
  double accepted_angular_error=.02,observed_initial_error=0;
  uint64_t sampled_texels=0;
  void validate() const;
  nlohmann::json encode() const;
  static NormalPreparationReceipt decode(const nlohmann::json &);
};
struct SourceNormalPreparation {
  NormalPreparationReceipt receipt;
  std::shared_ptr<const PaintNormalProjection> projection,initial_projection;
  std::shared_ptr<const NormalFrameSet> target_frames;
  std::shared_ptr<const PaintedMesh> original_paint;
  std::shared_ptr<const CreatorAppearance> original_appearance;
  std::shared_ptr<const DeformationState> original_deformation;
  std::shared_ptr<const SourceMaterialBindings> original_material;
  void validate(bool deep=true) const;
  void validate_current(const Mesh &,const CreatorAppearance *,
                        const std::function<bool()> &cancelled={}) const;
};
struct NormalPreparationOptions {
  double max_angular_error=.02;
  uint64_t max_samples=64'000'000;
  size_t max_working_bytes=256*1024*1024;
  std::function<bool()> cancelled;
};
struct NormalPreparationResult {
  std::shared_ptr<PaintedMesh> paint;
  std::shared_ptr<const SourceMaterialBindings> material_bindings;
  bool converted=false;
};
NormalPreparationResult prepare_source_normals(const ColorPreparationInput &,
                                               const NormalPreparationOptions & = {});
nlohmann::json encode_normal_projection(const PaintNormalProjection &,PaintImageArchive &);
std::shared_ptr<const PaintNormalProjection> decode_normal_projection(const nlohmann::json &,const PaintImageArchive &);
} // namespace dome
