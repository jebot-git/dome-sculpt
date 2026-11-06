// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scene.hpp"
namespace dome {
// Values2/3 remain reserved for earlier unadmitted source/handle experiments.
enum class SkinAuthoringMode { Automatic=0, Transfer=1, Anatomical=4, SurfaceHeat=5 };
struct AnatomicalSkinOptions {
  size_t maximum_work=500'000'000;
  size_t maximum_workspace=256u*1024u*1024u;
  // Owning request data plus workspace, not whole application/load/Save RSS.
  size_t maximum_total=512u*1024u*1024u;
  void validate() const;
};
struct SkinAuthoringOptions {
  uint64_t reference = 0;
  SkinAuthoringMode mode = SkinAuthoringMode::Automatic;
  WeightLimits limits{};
  // Distances are world metres, independent of object/group scale.
  float seed_distance = .3f, seed_margin = .01f, falloff = 2;
  float transfer_distance = .1f;
  // Legacy Automatic/Transfer bound. Anatomical/SurfaceHeat use the owning
  // anatomical resource block (same500M/256MiB/512MiB ceilings).
  size_t maximum_work = 50'000'000;
  // Locks refer to the target's existing exact joint identities. No names or
  // nearest bones are used to guess a correspondence between different rigs.
  std::vector<uint8_t> locks;
  AnatomicalSkinOptions anatomical;
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
// Positive screened surface heat with exact nearest/visibility seeds and
// prospective component handle support. It does not solve a volume PDE.
// Complete authenticated humanoid sources also use geometric head/ocular
// boundaries; generic native targets keep their own topology and source owners.
bool author_surface_heat_skin(Scene &,std::span<const uint64_t>,
    const SkinAuthoringOptions &,const std::function<bool()> &cancelled={});
// Source-informed anatomical surface/contact energy on a complete authenticated
// current unskinned source instance and an authored native humanoid reference.
// Fixed4 influences/prune1e-5; artist influence values are never solver inputs.
// Requested outputs alone change; other support parts remain read-only.
bool author_anatomical_skin(Scene &,std::span<const uint64_t>,
    const SkinAuthoringOptions &,const std::function<bool()> &cancelled={});
} // namespace dome
