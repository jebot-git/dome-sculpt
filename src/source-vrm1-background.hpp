// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/humanoid-creator.hpp"
#include "dome/material-bindings.hpp"
#include "dome/material-visibility.hpp"
#include "dome/paint.hpp"
#include "dome/scene.hpp"
#include "paint-overlay-packet.hpp"
namespace dome::detail {
struct Vrm1SourceBackground {
 std::array<float,4> color{};
 std::array<float,3> emission{};
 float relative_emission_strength=1;
};
// V1 shares these actual core factor properties with MToon. Resolve only its
// original source/background term; paint retains its native comparison basis.
// Derived/prepared and native-float packets require their own interpretation.
// This is preview data, never an export or source-authority admission shortcut.
inline std::optional<Vrm1SourceBackground> vrm1_source_background(
 const Material &current,const SourceMaterialBindings *binding,
 const CreatorAppearance *appearance,const TextureMaps *maps,bool pbr_view){
 if(!pbr_view||!binding||!binding->has_mtoon||binding->material.version()!=MToonVersion::V1||
    !appearance||!appearance->asset||!appearance->original_bytes||
    appearance->asset->mode!=ImportMode::PreserveGltf||
    appearance->asset->avatar_version!=ImportedAvatarVersion::Vrm1||
    appearance->source_material<0||appearance->material<1||
    uint64_t(appearance->material)!=uint64_t(appearance->source_material)+1||
    size_t(appearance->material)>=appearance->asset->materials.size()||
    binding->color_preparation||binding->normal_preparation||binding->key_derivation||
    (maps&&!maps->source_only))return {};
 // This base has only Source4 source-only packets. Future native emission
 // packet union must also exclude its explicit native-geometry interpretation.
 if(maps&&maps->overlay&&(!maps->source_maps||maps->overlay->source_maps()!=maps->source_maps||
    maps->overlay->source_maps().owner_before(maps->source_maps)||maps->source_maps.owner_before(maps->overlay->source_maps())||
    !maps->overlay->original_uv0()))return {};
 const auto &parameters=binding->material.parameters;Vrm1SourceBackground value;
 value.relative_emission_strength=parameters.emission_strength;
 for(size_t k=0;k<4;++k)value.color[k]=mtoon_factor(parameters.base_color[k],binding->native_reference.color[k],current.color[k]);
 const std::array<float,3> source{parameters.emission.x,parameters.emission.y,parameters.emission.z};
 for(size_t k=0;k<3;++k)value.emission[k]=binding->physical_emission?binding->physical_emission->at(k):
   mtoon_factor(source[k],binding->native_reference.emission[k],current.emissive[k])*parameters.emission_strength;
 for(float q:value.color)if(!std::isfinite(q)||q<0)throw std::overflow_error("Effective VRM1 source color exceeds finite preview range");
 for(float q:value.emission)if(!std::isfinite(q)||q<0)throw std::overflow_error("Effective VRM1 source emission exceeds finite preview range");
 if(!std::isfinite(value.relative_emission_strength)||value.relative_emission_strength<0)
  throw std::overflow_error("Effective VRM1 relative emission strength exceeds finite preview range");
 return value;
}
}
