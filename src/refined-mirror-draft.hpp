// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "frame-attribute-sampling.hpp"
namespace dome::detail {
// Private connected draft. Neither this object nor its raw artist columns is a
// NormalFrameSet, writable Paint attachment, or permission to publish a Scene.
struct RefinedMirrorDraft {
 std::shared_ptr<const FrameAttributeSampling> anchor;
 uint64_t source_target=0,target=0;
 std::string name;
 std::vector<Vec3> positions;
 std::array<NormalFrameTarget,3> roles;
 size_t working_bytes=0;
};
RefinedMirrorDraft derive_refined_mirror(std::shared_ptr<const FrameAttributeSampling>,
 size_t current_target,std::string name,const PaintSamplingLimits & = {});
}
