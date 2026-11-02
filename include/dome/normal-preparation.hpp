// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "color-preparation.hpp"
namespace dome {
class NormalSamplingArchive;
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
struct SourceNormalUvReprojection;
struct SourceNormalSampling;
class PaintSamplingDomain;
struct SourceNormalPreparation {
  NormalPreparationReceipt receipt;
  std::shared_ptr<const PaintNormalProjection> projection,initial_projection;
  std::shared_ptr<const NormalFrameSet> target_frames;
  std::shared_ptr<const PaintedMesh> original_paint;
  std::shared_ptr<const CreatorAppearance> original_appearance;
  std::shared_ptr<const DeformationState> original_deformation;
  std::shared_ptr<const SourceMaterialBindings> original_material;
  // Scene19/normal recipe2 only: current atlas planes are authenticated by an
  // owning before-state and an exact ordered-corner sampling layout.
  std::shared_ptr<const SourceNormalUvReprojection> uv_reprojection;
  // Scene21/recipe3. Exact source sampling correspondence is independent of
  // the writable atlas. This is mutually exclusive with the latest raster UV
  // operation; older recipes remain in the immutable before-graph.
  std::shared_ptr<const SourceNormalSampling> sampling_domain;
  void validate(bool deep=true) const;
  void validate_current(const Mesh &,const CreatorAppearance *,
                        const std::function<bool()> &cancelled={}) const;
};
struct NormalUvReprojectionReceipt {
  uint32_t version=1,resolution=0;
  double accepted_sample_error=.02,observed_sample_error=0;
  uint64_t sampled_texels=0,source_constraints=0,raster_samples=0;
  uint64_t raster_budget=64'000'000,max_working_bytes=256*1024*1024;
  void validate() const;
  nlohmann::json encode() const;
  static NormalUvReprojectionReceipt decode(const nlohmann::json &);
};
struct SourceNormalUvLayout {
  std::vector<uint32_t> source_vertices,indices;
  std::vector<std::array<float,2>> uv;
  std::vector<uint8_t> pins,seams;
  size_t storage_bytes() const;
  Mesh target_mesh(const Mesh &,const std::function<bool()> &cancelled={}) const;
  nlohmann::json encode() const;
  static size_t encoded_storage_bytes(const nlohmann::json &);
  static std::shared_ptr<const SourceNormalUvLayout> decode(const nlohmann::json &);
};
struct SourceNormalUvReprojection {
  NormalUvReprojectionReceipt receipt;
  std::shared_ptr<const SourceNormalUvLayout> layout;
  std::shared_ptr<const PaintNormalProjection> projection;
  std::shared_ptr<const PaintedMesh> before_paint;
  std::shared_ptr<const CreatorAppearance> before_appearance;
  std::shared_ptr<const DeformationState> before_deformation;
  std::shared_ptr<const SourceMaterialBindings> before_material;
  void validate(bool deep=true,const std::function<bool()> &cancelled={}) const;
  void validate_current(const Mesh &,const CreatorAppearance *,const std::function<bool()> &cancelled={}) const;
};
struct NormalSamplingReceipt {
  uint32_t version = 1, resolution = 0;
  uint64_t vertices = 0, indices = 0, correspondence_bytes = 0;
  uint64_t max_working_bytes = 256 * 1024 * 1024;
  void validate() const;
  nlohmann::json encode() const;
  static NormalSamplingReceipt decode(const nlohmann::json &);
};
struct SourceNormalSampling {
  NormalSamplingReceipt receipt;
  std::shared_ptr<const SourceNormalUvLayout> layout;
  std::shared_ptr<const PaintNormalProjection> projection;
  std::shared_ptr<const PaintedMesh> before_paint;
  std::shared_ptr<const CreatorAppearance> before_appearance;
  std::shared_ptr<const DeformationState> before_deformation;
  std::shared_ptr<const SourceMaterialBindings> before_material;
  // Captured versioned sampler source; repeated atlas operations retain this
  // exact ancestor rather than resetting its normal sampling semantics.
  std::shared_ptr<const SourceMaterialBindings> sampling_material;
  std::optional<MToonMaterial> current_sampling_reference;
  // Scene24 only: independently proved basis for normal bake metadata.
  std::shared_ptr<const PaintSamplingDomain> metadata_sampling_domain;
  void validate(bool deep = true,const std::function<bool()> &cancelled = {}) const;
  void validate_current(const Mesh &,const CreatorAppearance *,
                        const std::function<bool()> &cancelled = {}) const;
  void validate_paint(const PaintedMesh &,const SourceMaterialBindings &,bool deep = true,
                      const std::function<bool()> &cancelled = {}) const;
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
nlohmann::json encode_normal_projection(const PaintNormalProjection &,PaintImageArchive &,NormalSamplingArchive *);
std::shared_ptr<const PaintNormalProjection> decode_normal_projection(const nlohmann::json &,const PaintImageArchive &,const NormalSamplingArchive *);
} // namespace dome
