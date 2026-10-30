// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/import-options.hpp"
#include <cmath>
#include <nlohmann/json.hpp>
namespace dome {
void ImportOptions::validate() const {
  if (format < ImportSourceFormat::Automatic || format > ImportSourceFormat::Fbx ||
      units < ImportUnits::FromFile || units > ImportUnits::Custom ||
      axes < ImportAxes::FromFile || axes > ImportAxes::ZUp ||
      normals < ImportNormals::Preserve || normals > ImportNormals::GenerateSmooth ||
      !std::isfinite(custom_metres_per_unit) || custom_metres_per_unit <= 0 ||
      custom_metres_per_unit > 1e12)
    throw std::invalid_argument("Invalid model import options");
}
nlohmann::json ImportOptions::encode() const {
  validate();
  return {{"version",1},{"format",int(format)},{"units",int(units)},
    {"axes",int(axes)},{"normals",int(normals)},
    {"custom_metres_per_unit",custom_metres_per_unit},
    {"hierarchy",hierarchy},{"instances",instances},{"materials",materials},
    {"skins",skins},{"morphs",morphs}};
}
ImportOptions ImportOptions::decode(const nlohmann::json &j) {
  if (!j.is_object() || j.at("version") != 1)
    throw std::invalid_argument("Unsupported model import options version");
  auto field = [&](const char *name, int maximum) {
    const auto &v=j.at(name);
    if (!v.is_number_integer() || v.get<int64_t>() < 0 || v.get<uint64_t>() > uint64_t(maximum))
      throw std::invalid_argument("Invalid model import option enum");
    return v.get<int>();
  };
  ImportOptions out;
  out.format=ImportSourceFormat(field("format",4));
  out.units=ImportUnits(field("units",4));
  out.axes=ImportAxes(field("axes",2));
  out.normals=ImportNormals(field("normals",2));
  out.custom_metres_per_unit=j.at("custom_metres_per_unit").get<double>();
  for (auto [name, target] : {std::pair{"hierarchy",&out.hierarchy},
      {"instances",&out.instances},{"materials",&out.materials},
      {"skins",&out.skins},{"morphs",&out.morphs}}) {
    if(!j.at(name).is_boolean()) throw std::invalid_argument("Invalid model import option flag");
    *target=j.at(name).get<bool>();
  }
  out.validate(); return out;
}
} // namespace dome
