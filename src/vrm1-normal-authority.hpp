// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core-source-normal-export.hpp"
namespace dome::detail {
// Texture expressions edit the material. Retain each primitive's initial
// vertex frame, including Core PBR frames generated from the original sampler.
inline const ImportedPrimitive &vrm1_normal_source(const SculptObject &object,
 const std::function<bool()> &cancelled) {
 auto fail=[](const char *reason){throw std::invalid_argument(std::string("VRM1 normal-texture authored frame: ")+reason);};
 if(!object.painted||!object.appearance||!object.material_bindings)fail("owning source is absent");
 const auto &binding=*object.material_bindings;
 if(!object.painted->valid_source_sampling_attachment()||
    binding.normal_frame!=SourceMaterialNormalFrame::OriginalAttributes||!binding.normal_frames||
    !binding.uv_origin||!binding.uv_origin->appearance||!binding.uv_origin->paint||
    binding.normal_preparation||binding.color_preparation||binding.key_derivation||binding.physical_emission)
  fail("an independent source and unchanged imported attributes are required");
 const auto &primitive=original_normal_primitive(binding,*object.appearance);
 normal_frame_current_authority(binding,object.painted->mesh(),object.deformation.get(),cancelled);
 const auto &pbr=binding.normal_frames->pbr;
 if(!pbr||pbr->transport!=NormalFrameTransport::FixedAttributes||pbr->tangents.empty())
  fail("core PBR requires fixed source N/T");
 return primitive;
}

inline const ImportedPrimitive &require_vrm1_core_normal_frame(const SculptObject &object,
 const std::function<bool()> &cancelled) {
 const auto &primitive=vrm1_normal_source(object,cancelled);
 const auto &binding=*object.material_bindings;
 const auto &frame=*binding.normal_frames->pbr;
 const auto &raw=object.appearance->asset->materials.at(primitive.material);
 if(binding.has_mtoon||raw.normal_texture<0||
    frame.generated_tangents!=source_needs_generated_tangents(primitive))
  throw std::invalid_argument("VRM1 core normal frame needs its original sampler and attribute provenance");
 if(frame.generated_tangents){
  const auto &maps=binding.uv_origin->paint->source_maps();
  const auto set=imported_texture_coordinate(raw.normal_texcoord,raw.normal_transform);
  if(frame.generated_uv!=uint32_t(set)||!maps||!maps->normal.image||maps->normal.texcoord!=set||
     maps->normal.uv_transform!=imported_texture_matrix(raw.normal_transform))
   throw std::invalid_argument("VRM1 generated normal frame lost its initial normal sampler");
 }
 return primitive;
}

// The sealed exporter certifies derived directions against the actual
// original primitive. Preview uses the immutable imported attribute owner.
inline void verify_vrm1_generated_normal_frame(const SculptObject &object,
 const ImportedPrimitive &primitive,const std::function<bool()> &cancelled) {
 const auto &set=*object.material_bindings->normal_frames;
 if(set.pbr->generated_tangents)
  verify_core_source_normal_frame(set,primitive,object.appearance->asset->materials.at(primitive.material),cancelled);
}
inline void require_vrm1_authored_normal_frame(const SculptObject &object,
 const std::function<bool()> &cancelled={}) {
 const auto &primitive=vrm1_normal_source(object,cancelled);
 const auto &binding=*object.material_bindings;
 const auto &frames=*binding.normal_frames;
 auto fail=[](const char *reason){throw std::invalid_argument(std::string("VRM1 normal-texture authored frame: ")+reason);};
 if(!binding.has_mtoon||binding.material.version()!=MToonVersion::V1||
    !object.painted->source_sampling_prepared())fail("actual typed MToon1 and independent writable owner are required");
 const auto &pbr=frames.pbr,&toon=frames.mtoon;
 if(source_needs_generated_tangents(primitive)||pbr->generated_tangents)
  fail("genuine authored source TANGENT is required");
 if(!toon||toon->generated_tangents||toon->transport!=NormalFrameTransport::FixedAttributes||
    pbr->tangents!=toon->tangents||pbr->normals.size()!=toon->normals.size()||pbr->targets.size()!=toon->targets.size())
  fail("PBR and MToon require compatible fixed authored N/T");
 auto equal=[&](const std::vector<Vec3>&a,const std::vector<Vec3>&b){if(a.size()!=b.size())return false;for(size_t i=0;i<a.size();++i){if((i&1023)==0&&cancelled&&cancelled())throw std::runtime_error("VRM1 normal frame validation cancelled");if(a[i].x!=b[i].x||a[i].y!=b[i].y||a[i].z!=b[i].z)return false;}return true;};
 if(!equal(pbr->normals,toon->normals))fail("original normal roles differ");
 for(size_t i=0;i<pbr->targets.size();++i)if(pbr->targets[i].id!=toon->targets[i].id||!equal(pbr->targets[i].normals,toon->targets[i].normals)||!equal(pbr->targets[i].tangents,toon->targets[i].tangents))fail("original morph direction roles differ");
 const auto maps=object.painted->source_sampling_original()->source_maps();const auto &role=binding.roles[size_t(MToonSlot::Normal)];
 if(!maps||!role.image||role.image!=maps->normal.image||role.wrap_s!=maps->normal.wrap_s||role.wrap_t!=maps->normal.wrap_t||role.min_filter!=maps->normal.min_filter||role.mag_filter!=maps->normal.mag_filter||binding.material.parameters.normal_scale!=maps->normal_scale)
  fail("typed and original core normal sampler/image authority differs");
}
struct NormalFrameExport;
class Vrm1AuthoringCompilation;
NormalFrameExport normal_frame_export_vrm1(const Vrm1AuthoringCompilation &,
 const ObjectSurface &,const std::function<bool()> &);
// A caller cannot construct this from arbitrary changed geometry or a bool.
// Its only friend authenticates the exact sealed compiler surface and captured
// Source4 owner, and consumes the token synchronously without exposing it.
class Vrm1NormalFrameExportProof final {
 const ObjectSurface *surface_;
 std::shared_ptr<const SourceMaterialBindings> binding_;
 std::shared_ptr<const TextureMaps> textures_;
 Vrm1NormalFrameExportProof(const ObjectSurface &surface):surface_(&surface),binding_(surface.material_bindings),textures_(surface.textures){}
 friend NormalFrameExport normal_frame_export_vrm1(const Vrm1AuthoringCompilation &,const ObjectSurface &,const std::function<bool()> &);
public:
 Vrm1NormalFrameExportProof(const Vrm1NormalFrameExportProof&)=delete;
 bool matches(const ObjectSurface &surface)const{return &surface==surface_&&surface.material_bindings==binding_&&surface.textures==textures_;}
};
} // namespace dome::detail
