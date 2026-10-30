// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <nlohmann/json_fwd.hpp>
namespace dome {
enum class ImportSourceFormat { Automatic, Glb, Gltf, Obj, Fbx };
enum class ImportUnits { FromFile, Metres, Centimetres, Millimetres, Custom };
enum class ImportAxes { FromFile, YUp, ZUp };
enum class ImportNormals { Preserve, GenerateFlat, GenerateSmooth };
// Format-independent authoring choices, shared by the native and XR workflows.
// FromFile for unitless OBJ means one metre per unit, with a diagnostic.
struct ImportOptions {
  ImportSourceFormat format = ImportSourceFormat::Automatic;
  ImportUnits units = ImportUnits::FromFile;
  ImportAxes axes = ImportAxes::FromFile;
  ImportNormals normals = ImportNormals::Preserve;
  double custom_metres_per_unit = 1;
  bool hierarchy = true, instances = true, materials = true, skins = true,
       morphs = true;
  void validate() const;
  nlohmann::json encode() const;
  static ImportOptions decode(const nlohmann::json &);
  bool operator==(const ImportOptions &) const = default;
};
} // namespace dome
