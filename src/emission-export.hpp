// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "emission-package.hpp"
#include "emission-exr-owned.hpp"
namespace dome::detail {
// The immutable SculptObject owns the writable mesh for this raster's lifetime.
struct EmissionExportRaster {
 uint32_t resolution=0, version=0;
 size_t working_bytes=0;
 EmissionExrSource read;
};
EmissionExportRaster export_emission_raster(const SculptObject &,
    const std::function<bool()> &, bool authored_mtoon,bool prepared_source=false);
EmissionInterchangeImage canonical_paint_emission_field(const SculptObject &,const std::function<bool()> &);
EmissionInterchangeImage native_paint_emission_field(const SculptObject &,const std::function<bool()> &);
EmissionInterchangeImage baked_emission_field(const SculptObject &,const std::function<bool()> &,bool authored_mtoon);
bool requires_native_emission_package(const Scene &,const std::function<bool()> & = {});
class EmissionExportProof;
struct EmissionExportPrepared {
 std::vector<ObjectSurface> surfaces;
 std::vector<EmissionExportImage> images;
 std::vector<uint64_t> ids;
 bool authored_mtoon=false;
 bool streaming=false;
 std::shared_ptr<const EmissionExportProof> proof;
};
void validate_native_emission_surfaces(const EmissionExportPrepared &,const std::function<bool()> &);
bool matches_texture_set_emission_surface(const EmissionExportPrepared &,const ObjectSurface &);
bool matches_native_emission_surface(const EmissionExportPrepared &,const ObjectSurface &,const std::function<bool()> &);
EmissionExportPrepared prepare_emission_export(std::span<const ObjectSurface>,
 const NativeEmissionExport &,const std::function<bool()> &,bool authored_mtoon=false);
void finish_emission_export(GltfDocument &,EmissionExportPrepared &,
 const NativeEmissionExport &,size_t,const std::function<bool()> &);
}
