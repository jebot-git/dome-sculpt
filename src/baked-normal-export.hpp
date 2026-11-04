// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "normal-frame-export.hpp"
#include "dome/tangents.hpp"
namespace dome::detail {
struct GeneratedBakedNormalExport {
 NormalFrameExport frame;
 size_t recomputed_normals=0;
};
// This is transient export geometry, never a replacement for persistent source
// authority. Original retained frames take their existing authenticated path.
// The normal image here is the genuine native baker's canonical UV0 field.
inline std::optional<GeneratedBakedNormalExport> baked_normal_export(
 const ObjectSurface &input,SourceMorphSpace space,bool recompute_normals,
 const std::function<bool()> &cancelled={}){
 if(space!=SourceMorphSpace::Native||!input.textures||!input.textures->baked_base||
    input.textures->baked_base->normal_interpretation!=BakedNormalInterpretation::CanonicalMikk||
    input.textures->baked_base->maps.normal.rgba.empty()||input.textures->normal.rgba.empty()||
    (input.material_bindings&&input.material_bindings->normal_frames))return {};
 auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Baked normal tangent export cancelled");};stop();
 SourceMorphTransport source(input,space,cancelled);
 // Authored tangents keep the existing comparison/serialization route. Never
 // override their source identity merely because a canonical bake is present.
 if(!source.base_tangents().empty())return {};
 const auto &mesh=input.mesh;mesh.validate();
 const size_t count=input.deformation&&input.deformation->morphs?input.deformation->morphs->targets.size():0;
 constexpr size_t cap=128*1024*1024;
 if(count>128)throw std::length_error("Baked normal tangent target count exceeds native limits");
 size_t bytes=64*1024;
 auto reserve=[&](size_t n,size_t unit){if(bytes>cap||n>(cap-bytes)/unit)throw std::length_error("Baked normal tangent staging exceeds128MiB");bytes+=n*unit;};
 // Charge both corner-direction copies, endpoint scratch, worst-case split
 // geometry, hash/layout storage and all remapped morph channels before work.
 reserve(mesh.indices.size(),192+count*60);
 reserve(mesh.positions.size(),64);
 if(input.deformation){
  reserve(input.deformation->resource_entries(),sizeof(Vec3));
  if(input.deformation->skin){
   reserve(mesh.indices.size(),sizeof(decltype(input.deformation->skin->vertices)::value_type));
   for(size_t c=0;c<mesh.indices.size();++c){if((c&1023)==0)stop();reserve(input.deformation->skin->vertices.at(mesh.indices[c]).size(),sizeof(decltype(input.deformation->skin->vertices)::value_type::value_type));}
  }
 }
 auto directions=std::make_shared<NormalFrames>();directions->transport=NormalFrameTransport::CanonicalMikk;
 directions->generated_tangents=true;directions->generated_uv=0;directions->origin_vertex_count=uint32_t(mesh.positions.size());
 directions->origin_vertices=mesh.indices;directions->tangents=tangent_corners(mesh,cancelled);
 directions->normals.reserve(mesh.indices.size());
 for(size_t c=0;c<mesh.indices.size();++c){if((c&1023)==0)stop();directions->normals.push_back(mesh.normals.at(mesh.indices[c]));}
 NormalFrameSet layout_input;layout_input.pbr=directions;layout_input.next_target=count+1;
 GeneratedBakedNormalExport result;
 for(size_t slot=0;slot<count;++slot){
  stop();const auto channels=source.target(slot,recompute_normals);
  if(!channels.tangents.empty())throw std::invalid_argument("Generated base tangent cannot replace authored morph tangent authority");
  result.recomputed_normals+=channels.normals_recomputed;
  auto endpoint=mesh;const auto &delta=input.deformation->morphs->targets.at(slot).delta;
  for(size_t v=0;v<endpoint.positions.size();++v){if((v&1023)==0)stop();endpoint.positions[v]+=delta.at(v);if(!channels.normals.empty())endpoint.normals[v]+=channels.normals.at(v);}
  auto tangents=tangent_corners(endpoint,cancelled);NormalFrameTarget target;target.id=slot+1;
  if(!channels.normals.empty())target.normals.reserve(mesh.indices.size());target.tangents.reserve(mesh.indices.size());
  for(size_t c=0;c<mesh.indices.size();++c){
   if((c&1023)==0)stop();const auto base=directions->tangents[c],end=tangents[c];
   // glTF morph tangents carry XYZ deltas; handedness has no morph channel.
   if(base[3]!=end[3])throw std::invalid_argument("Baked normal morph changes tangent handedness; native source retained");
   target.tangents.push_back({end[0]-base[0],end[1]-base[1],end[2]-base[2]});
   if(!channels.normals.empty())target.normals.push_back(channels.normals.at(mesh.indices[c]));
  }
  directions->targets.push_back(std::move(target));layout_input.targets.push_back({slot+1,uint32_t(slot),{}, {}});layout_input.current_targets.push_back(slot+1);
 }
 directions->validate();auto layout=normal_frame_layout(mesh,layout_input,cancelled);auto &out=result.frame;
 if(input.paint_sampling_attachment||!input.paint_corner_coordinates.empty()){
  const auto node=input.material_bindings&&input.material_bindings->color_preparation?input.material_bindings->color_preparation->paint_sampling:nullptr;
  const auto owner=node?node->geometry_attachment():nullptr;
  if(!owner||owner!=input.paint_sampling_attachment||owner.owner_before(input.paint_sampling_attachment)||input.paint_sampling_attachment.owner_before(owner))
   throw std::invalid_argument("Baked normal export lost its own paint sampling attachment");
  node->validate_current(mesh,cancelled);PaintSamplingLimits limits;limits.cancelled=cancelled;
  auto coordinates=owner->domain()->corner_coordinates(mesh,limits);
  if(coordinates!=input.paint_corner_coordinates)throw std::invalid_argument("Baked normal export paint coordinates differ from their domain proof");
  auto split=split_frame_corner_coordinates(mesh,layout,coordinates,cancelled);layout=std::move(split.frames);out.paint_coordinates=std::move(split.coordinates);
 }
 // Copy document properties without copying and then discarding its mesh.
 // Geometry-bound preview/sampling proofs stay with the original input; only
 // their authenticated derived coordinates enter this transient frame.
 out.surface={input.id,input.name,input.transform,input.material,{},input.textures,input.deformation,input.groups,input.appearance,input.material_bindings,input.source_dormant};
 out.surface.affine_edit=input.affine_edit;out.surface.rig_source=input.rig_source;
 out.rest_vertices=layout.vertices;out.input_polygon_colors=input.polygon_colors;out.source=source.source();
 auto &emitted=out.surface.mesh;const auto vertices=layout.vertices.size();
 emitted.positions.reserve(vertices);emitted.normals.reserve(vertices);
 if(!mesh.uv.empty())emitted.uv.reserve(vertices);
 if(!mesh.uv_pins.empty())emitted.uv_pins.reserve(vertices);
 out.tangents.reserve(vertices);out.normal_targets.resize(count);out.tangent_targets.resize(count);
 for(size_t t=0;t<count;++t){if(!directions->targets[t].normals.empty())out.normal_targets[t].reserve(vertices);out.tangent_targets[t].reserve(vertices);}
 for(size_t v=0;v<layout.vertices.size();++v){
  if((v&1023)==0)stop();const auto original=layout.vertices[v],corner=layout.corners[v];emitted.positions.push_back(mesh.positions.at(original));emitted.normals.push_back(mesh.normals.at(original));
  if(!mesh.uv.empty())emitted.uv.push_back(mesh.uv.at(original));if(!mesh.uv_pins.empty())emitted.uv_pins.push_back(mesh.uv_pins.at(original));out.tangents.push_back(directions->tangents.at(corner));
  for(size_t t=0;t<count;++t){const auto &target=directions->targets[t];if(!target.normals.empty())out.normal_targets[t].push_back(target.normals.at(corner));out.tangent_targets[t].push_back(target.tangents.at(corner));}
 }
 emitted.indices=layout.indices;emitted.uv_seams=mesh.uv_seams;emitted.validate();out.surface.deformation=normal_frame_remap_deformation(input.deformation,mesh,emitted,layout.vertices,cancelled);
 if(input.appearance){
  if(input.appearance->source_vertices.size()!=mesh.positions.size())throw std::invalid_argument("Baked normal export source vertex domain is incomplete");
  auto appearance=std::make_shared<CreatorAppearance>(*input.appearance);appearance->source_vertices.clear();appearance->source_vertices.reserve(layout.vertices.size());
  for(size_t v=0;v<layout.vertices.size();++v){if((v&1023)==0)stop();appearance->source_vertices.push_back(input.appearance->source_vertices.at(layout.vertices[v]));}out.surface.appearance=std::move(appearance);
 }
 stop();return result;
}
} // namespace dome::detail
