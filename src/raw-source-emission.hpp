// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native-authoring-capture.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/material-bindings.hpp"
#include "dome/source-maps.hpp"
#include "paint-overlay-packet.hpp"
#include "import-texture-transform.hpp"
#include "raw-source-canonical.hpp"
#include <algorithm>
#include <bit>
#include <optional>
#include <cstring>
#include <tuple>
#include <cmath>
namespace dome::detail {
struct RawSourceEmission {
  std::array<float,3> factor{};
  float strength=1;
  double product_error=0;
  bool original_factors=false;
};
// Conservative temporary JSON allocation forecast, obtained without allocating
// or trusting parsed DTO values. Charge one peak, not every aliased material.
inline size_t raw_emission_proof_bytes(const CreatorAppearance &a,
    const std::function<bool()> &cancelled={}) {
  return raw_source_proof_plan(a,cancelled).bytes();
}
// Per-call replay, not a persistent seal or an arbitrary-surface permission.
// Only the sealed capture authenticates geometry, placement and current owners;
// the retained parsed source authenticates the exact role/sampler/UV mapping.
inline std::optional<RawSourceEmission> raw_source_emission(
    const NativeAuthoringCapture &capture,const ObjectSurface &surface,
    const std::function<bool()> &cancelled={}) {
  auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Raw source emission validation cancelled");};
  stop();
  const auto &o=capture.scene().object(surface.id);
  if(!o.mesh_active||!o.painted||o.painted->source_sampling_prepared()||
     o.painted->paint_sampling_domain()||o.painted->baked()||
     o.painted->color_projection()||o.painted->normal_projection()||
     o.painted->surface_diagnostics().policy!=PaintUvPolicy::SourceAuthored||
     !o.appearance||!o.material_bindings||o.material_bindings->physical_emission||
     o.material_bindings->color_preparation||o.material_bindings->normal_preparation)
    return {};
  for(const auto &layer:o.painted->layers())
    if(std::any_of(layer.tiles.begin(),layer.tiles.end(),[](const auto &p){return bool(p);})||
       std::any_of(layer.pbr_tiles.begin(),layer.pbr_tiles.end(),[](const auto &p){return bool(p);})||
       std::any_of(layer.emission_tiles.begin(),layer.emission_tiles.end(),[](const auto &p){return bool(p);}))return {};
  const auto original=std::find_if(capture.originals().begin(),capture.originals().end(),
      [&](const auto &s){return s.id==surface.id;});
  auto fail=[] {throw std::invalid_argument("Raw emission view differs from its exact captured source authority");};
  if(original==capture.originals().end())fail();
  auto material=[](const Material &m){return std::tie(m.name,m.color,m.emissive,m.metallic,m.roughness,m.toon,m.double_sided,m.alpha,m.alpha_cutoff,m.unlit);};
  if(surface.name!=original->name||material(surface.material)!=material(o.material)||
     surface.deformation!=o.deformation||surface.appearance!=o.appearance||
     surface.material_bindings!=o.material_bindings||surface.polygon_colors!=original->polygon_colors||
     surface.rig_source!=original->rig_source||surface.affine_edit!=original->affine_edit||
     surface.source_dormant||surface.paint_sampling_attachment||!surface.paint_corner_coordinates.empty()||
     surface.world_matrix().float_matrix()!=original->world_matrix().float_matrix())fail();
  auto same=[](const auto &a,const auto &b){return a.size()==b.size()&&(a.empty()||!std::memcmp(a.data(),b.data(),a.size()*sizeof(a.front())));};
  const auto &mesh=surface.mesh,&expected=original->mesh;
  if(!same(mesh.positions,expected.positions)||!same(mesh.normals,expected.normals)||
     !same(mesh.uv,expected.uv)||mesh.indices!=expected.indices||mesh.uv_pins!=expected.uv_pins||mesh.uv_seams!=expected.uv_seams)fail();
  const auto maps=o.painted->source_maps();
  if(!maps||!surface.textures||!surface.textures->source_only||surface.textures->source_maps!=maps)fail();
  const auto &texture=*surface.textures;
  auto present=[](const Image &i){return i.width||i.height||!i.rgba.empty();};
  for(const auto *i:{&texture.color,&texture.metal_rough,&texture.emission,&texture.color_coverage,
      &texture.normal,&texture.baked_normal,&texture.baked_coverage,&texture.emission_coverage,
      &texture.pbr_color,&texture.mtoon_normal})if(present(*i))fail();
  if(texture.baked_base||texture.color_projection||texture.normal_projection||
     (texture.overlay&&(!texture.overlay->empty()||texture.overlay->source_maps()!=maps)))fail();
  maps->validate_surface(mesh);
  const auto &a=*o.appearance;
  if(!a.asset||!a.decoded_images||a.material<0||size_t(a.material)>=a.asset->materials.size()||
     a.source_vertices.size()!=mesh.positions.size())fail();
  const auto &m=a.asset->materials.at(size_t(a.material));
  RawSourceCanonical canonical(a,cancelled);
  const cgltf_material empty{};const auto *source=canonical.material();const auto &source_material=source?*source:empty;
  std::array<float,3> factor{};std::copy_n(source_material.emissive_factor,3,factor.begin());
  const float strength=source_material.has_emissive_strength?source_material.emissive_strength.emissive_strength:1;
  if(std::bit_cast<std::array<uint32_t,3>>(factor)!=std::bit_cast<std::array<uint32_t,3>>(m.emissive)||
     std::bit_cast<uint32_t>(strength)!=std::bit_cast<uint32_t>(m.emissive_strength))
    throw std::invalid_argument("Raw emission factor and strength differ from canonical source material");
  canonical.role(source_material.pbr_metallic_roughness.base_color_texture,m.color_texture,m.color_texcoord,m.color_transform);
  canonical.role(source_material.pbr_metallic_roughness.metallic_roughness_texture,m.metal_rough_texture,m.metal_rough_texcoord,m.metal_rough_transform);
  canonical.role(source_material.normal_texture,m.normal_texture,m.normal_texcoord,m.normal_transform);
  canonical.role(source_material.emissive_texture,m.emissive_texture,m.emissive_texcoord,m.emissive_transform);
  canonical.role(source_material.occlusion_texture,m.occlusion_texture,m.occlusion_texcoord,m.occlusion_transform);
  if(m.normal_scale!=(source_material.normal_texture.texture?source_material.normal_texture.scale:1)||
     m.occlusion_strength!=(source_material.occlusion_texture.texture?source_material.occlusion_texture.scale:1))
    throw std::invalid_argument("Raw normal/occlusion strength differs from canonical source authority");
  const ImportedPrimitive *primitive=nullptr;
  for(const auto &p:a.asset->primitives)if(p.source_mesh==a.source_mesh&&p.source_primitive==a.source_primitive){if(primitive)fail();primitive=&p;}
  if(!primitive||primitive->material!=uint32_t(a.material))fail();
  size_t work=0;
  auto pair=[&](const auto &current,const auto &raw,int set){
    if(current.size()!=a.source_vertices.size())fail();
    for(size_t v=0;v<current.size();++v){if((++work&1023)==0)stop();if(a.source_vertices[v]>=raw.size()||
        std::bit_cast<std::array<uint32_t,2>>(current[v])!=std::bit_cast<std::array<uint32_t,2>>(raw[a.source_vertices[v]]))fail();}
    canonical.coordinates(set,raw);
  };
  auto role=[&](const PaintSourceTexture &slot,int index,int coordinate,const ImportedTextureTransform &transform){
    if(index<0){if(slot.image)fail();return;}
    const auto &t=a.asset->textures.at(size_t(index));
    const auto &image=a.decoded_images->at(size_t(t.image));
    if(slot.image!=image||slot.image.owner_before(image)||image.owner_before(slot.image)||
       slot.wrap_s!=t.wrap_s||slot.wrap_t!=t.wrap_t||slot.min_filter!=t.min_filter||slot.mag_filter!=t.mag_filter||
       slot.texcoord!=imported_texture_coordinate(coordinate,transform)||
       slot.uv_transform!=imported_texture_matrix(transform))fail();
    if(slot.texcoord==0)pair(mesh.uv,primitive->mesh.uv,0);
    else if(slot.texcoord==1){if(!maps->uv1)fail();pair(*maps->uv1,primitive->uv1,1);}
    else {const auto set=uint32_t(slot.texcoord);if(!maps->extra_uv_sets.contains(set)||!primitive->extra_uv_sets.contains(set))fail();pair(*maps->extra_uv_sets.at(set),primitive->extra_uv_sets.at(set),int(set));}
  };
  role(maps->color,m.color_texture,m.color_texcoord,m.color_transform);
  role(maps->metal_rough,m.metal_rough_texture,m.metal_rough_texcoord,m.metal_rough_transform);
  role(maps->normal,m.normal_texture,m.normal_texcoord,m.normal_transform);
  role(maps->emission,m.emissive_texture,m.emissive_texcoord,m.emissive_transform);
  role(maps->occlusion,m.occlusion_texture,m.occlusion_texcoord,m.occlusion_transform);
  if(maps->normal_scale!=m.normal_scale||maps->occlusion_strength!=m.occlusion_strength)fail();
  if(maps->uv1)pair(*maps->uv1,primitive->uv1,1);
  for(const auto &[set,values]:maps->extra_uv_sets){if(!primitive->extra_uv_sets.contains(set))fail();pair(*values,primitive->extra_uv_sets.at(set),int(set));}
  RawSourceEmission result;
  auto native=m.emissive;for(auto &c:native)c*=m.emissive_strength;
  if(native!=o.material_bindings->native_reference.emission)fail();
  if(o.material.emissive==native){result.factor=m.emissive;result.strength=m.emissive_strength;result.original_factors=true;}
  else {result.strength=std::max({1.f,o.material.emissive[0],o.material.emissive[1],o.material.emissive[2]});
    for(size_t c=0;c<3;++c)result.factor[c]=o.material.emissive[c]/result.strength;}
  for(size_t c=0;c<3;++c)result.product_error=std::max(result.product_error,
      std::abs(double(result.factor[c])*result.strength-o.material.emissive[c]));
  stop();return result;
}
}
