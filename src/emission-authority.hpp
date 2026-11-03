// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/emission.hpp"
#include "dome/material-bindings.hpp"
#include <algorithm>
#include <nlohmann/json.hpp>
namespace dome::detail {
inline bool hdr_emission(const std::array<float,3> &value){return std::any_of(value.begin(),value.end(),[](float v){return v>1;});}
inline bool physical_emission(const SourceMaterialBindings &binding){return bool(binding.physical_emission)||hdr_emission(binding.native_reference.emission);}
inline void require_material_emission_version(const nlohmann::json &material,unsigned version){
 if(material.is_null())return;
 const auto value=material.at("emissive").get<std::array<float,3>>();
 if(version<27&&hdr_emission(value))throw std::invalid_argument("Physical native material requires Scene27");
}
}
