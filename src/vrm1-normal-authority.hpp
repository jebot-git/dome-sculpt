// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "normal-frame-runtime.hpp"
namespace dome::detail {
// Changing a glTF normal texture's UV parameters does not reauthor an explicit
// imported TANGENT. This adapter admits only the authentic fixed N/T domain,
// shared by the PBR fallback and MToon1; generated/prepared domains stay guarded.
inline void require_vrm1_authored_normal_frame(const SculptObject &object,
 const std::function<bool()> &cancelled={}) {
 auto fail=[](const char *reason){throw std::invalid_argument(std::string("VRM1 normal-texture authored frame: ")+reason);};
 if(!object.painted||!object.appearance||!object.material_bindings)fail("owning source is absent");
 const auto &binding=*object.material_bindings;
 if(!object.painted->source_sampling_prepared()||!object.painted->valid_source_sampling_attachment()||
    binding.normal_frame!=SourceMaterialNormalFrame::OriginalAttributes||!binding.normal_frames||
    !binding.uv_origin||!binding.uv_origin->appearance||!binding.uv_origin->paint||
    binding.normal_preparation||binding.color_preparation||binding.key_derivation||binding.physical_emission)
  fail("an independent source and unchanged imported attributes are required");
 const auto &appearance=*object.appearance,&origin=*binding.uv_origin->appearance;
 if(!appearance.asset||appearance.asset!=origin.asset||appearance.original_bytes!=origin.original_bytes||
    appearance.source_mesh!=origin.source_mesh||appearance.source_primitive!=origin.source_primitive)
  fail("retained original primitive identity differs");
 const ImportedPrimitive *primitive=nullptr;
 for(const auto &candidate:appearance.asset->primitives)if(candidate.source_mesh==appearance.source_mesh&&candidate.source_primitive==appearance.source_primitive){if(primitive)fail("source primitive is ambiguous");primitive=&candidate;}
 if(!primitive||primitive->tangents.size()!=primitive->mesh.positions.size()||primitive->tangents.empty())fail("genuine authored source TANGENT is required");
 normal_frame_current_authority(binding,object.painted->mesh(),object.deformation.get(),cancelled);
 const auto &pbr=binding.normal_frames->pbr,&toon=binding.normal_frames->mtoon;
 if(!pbr||!toon||pbr->generated_tangents||toon->generated_tangents||
    pbr->transport!=NormalFrameTransport::FixedAttributes||toon->transport!=NormalFrameTransport::FixedAttributes||
    pbr->tangents.empty()||pbr->tangents!=toon->tangents||pbr->normals.size()!=toon->normals.size()||pbr->targets.size()!=toon->targets.size())
  fail("PBR and MToon require compatible fixed authored N/T");
 auto equal=[&](const std::vector<Vec3>&a,const std::vector<Vec3>&b){if(a.size()!=b.size())return false;for(size_t i=0;i<a.size();++i){if((i&1023)==0&&cancelled&&cancelled())throw std::runtime_error("VRM1 normal frame validation cancelled");if(a[i].x!=b[i].x||a[i].y!=b[i].y||a[i].z!=b[i].z)return false;}return true;};
 if(!equal(pbr->normals,toon->normals))fail("original normal roles differ");
 for(size_t i=0;i<pbr->targets.size();++i)if(pbr->targets[i].id!=toon->targets[i].id||!equal(pbr->targets[i].normals,toon->targets[i].normals)||!equal(pbr->targets[i].tangents,toon->targets[i].tangents))fail("original morph direction roles differ");
 const auto maps=object.painted->source_sampling_original()->source_maps();const auto &role=binding.roles[size_t(MToonSlot::Normal)];
 if(!maps||!role.image||role.image!=maps->normal.image||role.wrap_s!=maps->normal.wrap_s||role.wrap_t!=maps->normal.wrap_t||role.min_filter!=maps->normal.min_filter||role.mag_filter!=maps->normal.mag_filter||binding.material.parameters.normal_scale!=maps->normal_scale)
  fail("typed and original core normal sampler/image authority differs");
}
} // namespace dome::detail
