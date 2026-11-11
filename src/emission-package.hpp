// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/emission-native.hpp"
#include "dome/source-maps.hpp"
#include "gltf-document.hpp"
namespace dome::detail {
class CanonicalPaintSampling;
Scene emission_export_subset(const Scene &,std::span<const uint64_t>);
struct EmissionExportImage {
 uint64_t object=0;
 int node=-1,material=-1;
 EmissionInterchangeImage image;
 float fallback_strength=1,maximum_absolute_error=0,maximum_relative_error=0;
 std::array<float,3> fallback_factor{1,1,1};
 size_t erased_nonzero_samples=0;
 // Genuine native32 field; legacy images retain their implicit UV0 meaning.
 bool native_paint_domain=false;
 unsigned texcoord=0;
 // Package4 only: actual independently authenticated canonical writable owner.
 std::shared_ptr<const CanonicalPaintSampling> canonical_paint_domain;
};
void attach_native_emission(GltfDocument &,const NativeEmissionExport &,
                            std::span<const uint64_t> exported_objects,
                            std::span<const EmissionExportImage>,size_t max_bytes,
                            const std::function<bool()> &cancelled={});
}
