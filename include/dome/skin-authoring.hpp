// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scene.hpp"
namespace dome {
enum class SkinAuthoringMode { Automatic, Transfer };
struct SkinAuthoringOptions {
  uint64_t reference = 0;
  SkinAuthoringMode mode = SkinAuthoringMode::Automatic;
  WeightLimits limits{};
  // Distances are world metres, independent of object/group scale.
  float seed_distance = .3f, seed_margin = .01f, falloff = 2;
  float transfer_distance = .1f;
  size_t maximum_work = 50'000'000;
  // Locks refer to the target's existing exact joint identities. No names or
  // nearest bones are used to guess a correspondence between different rigs.
  std::vector<uint8_t> locks;
  void validate() const;
};
// Zero-capable logarithmic slider over0..10000 world metres (displayed as mm).
float skin_distance_metres(float normalized);
float skin_distance_slider(float metres);
// Automatic binding accepts fitted skeletons without weights. Transfer requires
// an already skinned reference. Source and destination placements are converted
// explicitly; paint, original appearance, morphs and native source identities
// stay intact. An existing different rig is rejected rather than discarded.
bool author_skin(Scene &, uint64_t target, const SkinAuthoringOptions &,
                 const std::function<bool()> &cancelled = {});
} // namespace dome
