// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/humanoid-creator.hpp"
#include "dome/scene.hpp"
namespace dome::detail {
enum class SourceMorphSpace { Native, Original };
struct SourceMorphChannels {
  std::vector<Vec3> normals, tangents;
  bool normals_recomputed = false;
};
// Exact source identities are required before any retained attribute is
// emitted. Uniform positive scale/translation and explicit orientation preserve
// directions.
class SourceMorphTransport {
  const ObjectSurface &object_;
  const ImportedPrimitive *source_ = nullptr;
  Quat basis_;
  float scale_ = 1;
  bool frame_valid_ = false, has_source_morph_channels_ = false;
  std::function<bool()> cancelled_;
  mutable size_t work_ = 0;
  size_t max_work_;
  void visit() const;
  void check_cancel() const;
  void check_tangent_frame() const;
  mutable bool tangents_checked_ = false;
  [[noreturn]] void fail(const char *) const;

public:
  SourceMorphTransport(const ObjectSurface &,
                       SourceMorphSpace = SourceMorphSpace::Native,
                       std::function<bool()> cancelled = {},
                       size_t max_work = 64 * 1024 * 1024);
  const ImportedPrimitive *source() const noexcept { return source_; }
  bool frame_valid() const noexcept { return frame_valid_; }
  std::vector<std::array<float, 4>> base_tangents() const;
  SourceMorphChannels target(size_t native_target,
                             bool recompute_changed_normals = false) const;
};
} // namespace dome::detail
