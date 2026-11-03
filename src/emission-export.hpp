// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "emission-package.hpp"
namespace dome::detail {
struct EmissionExportPrepared {
 std::vector<ObjectSurface> surfaces;
 std::vector<EmissionExportImage> images;
 std::vector<uint64_t> ids;
};
EmissionExportPrepared prepare_emission_export(std::span<const ObjectSurface>,
 const NativeEmissionExport &,const std::function<bool()> &);
void finish_emission_export(GltfDocument &,EmissionExportPrepared &,
 const NativeEmissionExport &,size_t,const std::function<bool()> &);
}
