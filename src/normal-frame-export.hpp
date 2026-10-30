// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "normal-frame-runtime.hpp"
#include "source-morph.hpp"
#include "dome/humanoid-creator.hpp"
#include "normal-preparation-state.hpp"
#include <algorithm>
namespace dome::detail {
struct NormalFrameExport {
  ObjectSurface surface;
  // Exact original REST vertex for every expanded emitted vertex. Attribute
  // consumers validate against input authority before following this map.
  std::vector<uint32_t> rest_vertices;
  std::shared_ptr<const PolygonColorSurface> input_polygon_colors;
  std::vector<Tangent> tangents;
  std::vector<std::vector<Vec3>> normal_targets,tangent_targets;
  const ImportedPrimitive *source=nullptr;
  PaintSourceTexture canonical_normal,prepared_normal;
};
inline bool normal_frame_attribute_equal(const NormalFrames &a,const NormalFrames &b,bool compare_tangent,
    const std::function<bool()> &cancelled={}) {
  if(a.normals.size()!=b.normals.size() || a.origin_vertex_count!=b.origin_vertex_count || a.origin_vertices!=b.origin_vertices || a.targets.size()!=b.targets.size())return false;
  size_t work=0;auto vector_equal=[&](const std::vector<Vec3> &x,const std::vector<Vec3> &y) {
    if(x.empty() || y.empty()){const auto &present=x.empty()?y:x;for(auto v:present){if((++work&1023)==0 && cancelled && cancelled())throw std::runtime_error("Normal export comparison cancelled");if(v.x!=0 || v.y!=0 || v.z!=0)return false;}return true;}
    if(x.size()!=y.size())return false;
    for(size_t n=0;n<x.size();++n){if((++work&1023)==0 && cancelled && cancelled())throw std::runtime_error("Normal export comparison cancelled");if(x[n].x!=y[n].x || x[n].y!=y[n].y || x[n].z!=y[n].z)return false;}return true;
  };
  if(!vector_equal(a.normals,b.normals))return false;
  if(compare_tangent && a.tangents!=b.tangents)return false;
  for(size_t t=0;t<a.targets.size();++t)if(a.targets[t].id!=b.targets[t].id || !vector_equal(a.targets[t].normals,b.targets[t].normals) || (compare_tangent && !vector_equal(a.targets[t].tangents,b.targets[t].tangents)))return false;
  return true;
}
// Sufficient, deliberately conservative proof that the native raw interpolated
// canonical TBN equals the standard normalized-column TBN for every pose.
// Constant orthogonal per-triangle directions plus one shared rigid joint keep
// the columns orthogonal under inverse-transpose N / linear T transport.
inline PaintSourceTexture normal_frame_canonical_export(const ObjectSurface &input,
    const NormalFrames &selected,bool legacy,const std::function<bool()> &cancelled) {
  const auto &maps=*input.textures;const auto &set=*input.material_bindings->normal_frames;
  auto fail=[](const char *reason) {throw std::invalid_argument(std::string("Canonical normal export: ")+reason+"; retain the native project or explicitly convert to a standard representable target basis");};
  if(!set.canonical || set.canonical->tangents.empty() || !maps.baked_base || maps.baked_normal.rgba.empty() || maps.baked_coverage.rgba.empty())fail("retained canonical frame and raw bake payload are required");
  maps.baked_normal.validate();maps.baked_coverage.validate();
  if(maps.baked_normal.width!=maps.baked_coverage.width || maps.baked_normal.height!=maps.baked_coverage.height)fail("normal and coverage grids differ");
  if(!normal_frame_attribute_equal(selected,*set.canonical,true,cancelled))fail("selected and canonical complete direction channels differ");
  bool opaque=true;for(size_t p=0;p<maps.baked_coverage.rgba.size();p+=4){if((p&4095)==0 && cancelled && cancelled())throw std::runtime_error("Canonical normal export cancelled");opaque&=maps.baked_coverage.rgba[p]==255;}
  if(!opaque) {
    // Prove every bilinear footprint touched by the continuous triangle UV
    // domain. Merely checking covered texel centers misses filtered edges.
    // A texel has nonzero bilinear weight inside its center +/- one texel;
    // conservative closed rectangle/triangle SAT also checks boundary zeros.
    if(input.mesh.uv.size()!=input.mesh.positions.size())fail("canonical sampling UVs are unavailable");
    size_t work=0;const double width=maps.baked_coverage.width,height=maps.baked_coverage.height;
    for(size_t c=0;c<input.mesh.indices.size();c+=3) {
      std::array<std::array<double,2>,3> triangle;
      for(size_t k=0;k<3;++k){const auto uv=input.mesh.uv.at(input.mesh.indices[c+k]);if(uv[0]<0 || uv[0]>1 || uv[1]<0 || uv[1]>1)fail("full-coverage proof currently requires unit-atlas UVs");triangle[k]={uv[0]*width,uv[1]*height};}
      double lowx=triangle[0][0],highx=lowx,lowy=triangle[0][1],highy=lowy;for(auto p:triangle){lowx=std::min(lowx,p[0]);highx=std::max(highx,p[0]);lowy=std::min(lowy,p[1]);highy=std::max(highy,p[1]);}
      const int x0=std::max(0,int(std::ceil(lowx-1.5))),x1=std::min(int(width)-1,int(std::floor(highx+.5)));
      const int y0=std::max(0,int(std::ceil(lowy-1.5))),y1=std::min(int(height)-1,int(std::floor(highy+.5)));
      for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x) {
        if(++work>64*1024*1024)throw std::length_error("Canonical coverage proof exceeds its bounded sampling work quota");
        if((work&1023)==0 && cancelled && cancelled())throw std::runtime_error("Canonical coverage proof cancelled");
        if(maps.baked_coverage.rgba[(size_t(y)*size_t(width)+x)*4]==255)continue;
        const double left=std::max(0.,x-.5),right=std::min(width,x+1.5),bottom=std::max(0.,y-.5),top=std::min(height,y+1.5);
        bool overlap=!(highx<left || lowx>right || highy<bottom || lowy>top);
        for(size_t k=0;k<3 && overlap;++k){const auto a=triangle[k],b=triangle[(k+1)%3];const double ax=a[1]-b[1],ay=b[0]-a[0];double lo=ax*triangle[0][0]+ay*triangle[0][1],hi=lo;for(auto p:triangle){const double d=ax*p[0]+ay*p[1];lo=std::min(lo,d);hi=std::max(hi,d);}const double r0=ax*(ax>=0?left:right)+ay*(ay>=0?bottom:top),r1=ax*(ax>=0?right:left)+ay*(ay>=0?top:bottom);const double tolerance=1e-10*std::max({1.,std::abs(lo),std::abs(hi),std::abs(r0),std::abs(r1)});if(hi<r0-tolerance || lo>r1+tolerance)overlap=false;}
        if(overlap)fail("coverage is not fully opaque throughout a triangle's bilinear footprint");
      }
    }
  }
  auto zero=[](const auto &values){for(auto v:values)if(v.x!=0 || v.y!=0 || v.z!=0)return false;return true;};
  for(const auto &target:selected.targets)if(!zero(target.normals) || !zero(target.tangents))fail("direction targets are not constant");
  for(size_t c=0;c<input.mesh.indices.size();c+=3) {
    if((c&1023)==0 && cancelled && cancelled())throw std::runtime_error("Canonical normal export cancelled");
    const auto n=selected.normals[c];const auto t=selected.tangents[c];
    if(std::abs(dot(n,Vec3{t[0],t[1],t[2]}))>1e-6f)fail("base normal and tangent are not orthogonal");
    for(size_t k=1;k<3;++k){const auto v=selected.normals[c+k];if(v.x!=n.x || v.y!=n.y || v.z!=n.z || selected.tangents[c+k]!=t)fail("direction columns vary within a triangle");}
    if(input.deformation && input.deformation->skin) {
      std::optional<uint32_t> joint;
      for(size_t k=0;k<3;++k){const auto &w=input.deformation->skin->vertices.at(input.mesh.indices[c+k]);if(w.size()!=1 || w[0].weight!=1)fail("a vertex does not have one rigid joint");if(joint && *joint!=w[0].joint)fail("triangle vertices use different rigid joints");joint=w[0].joint;}
    }
  }
  if(legacy) {
    const auto &m=input.material_bindings->material;
    if(m.textures[0].offset!=std::array<float,2>{} || m.textures[0].scale!=std::array<float,2>{1,1} || m.parameters.scroll!=std::array<float,2>{} || m.parameters.rotation_speed!=0)fail("legacy shared sampling ST or animation is not identity");
  }
  PaintSourceTexture view;view.image=std::shared_ptr<const Image>(input.textures,&maps.baked_normal);
  view.wrap_s=view.wrap_t=33071;view.min_filter=view.mag_filter=9729;
  return view;
}
inline NormalFrameExport normal_frame_export(const ObjectSurface &input,SourceMorphSpace space,
    const std::function<bool()> &cancelled={}) {
  if(!input.material_bindings || !input.material_bindings->normal_frames)throw std::invalid_argument("Missing original frame export payload");
  const auto &binding=*input.material_bindings;binding.validate();const auto &set=*binding.normal_frames;
  const bool prepared=prepared_normal_state(&binding,input.textures.get(),false);
  const bool legacy=space==SourceMorphSpace::Original;const auto selected=legacy?set.mtoon:set.pbr;auto tangent_source=selected;
  if(!selected)throw std::invalid_argument("Selected export material has no retained normal frame");
  const bool pbr_normal=prepared||(input.textures && input.textures->source_maps && input.textures->source_maps->normal.image && input.textures->source_maps->normal_scale!=0);
  if(!prepared && pbr_normal && input.textures->source_maps->normal.uv_transform!=std::array<float,6>{1,0,0,1,0,0})throw std::invalid_argument("Normal texture transform needs a proven PBR consumer frame convention before export");
  const bool mtoon_normal=binding.has_mtoon && binding.material.parameters.normal_enabled && binding.material.parameters.normal_scale!=0 && binding.material.textures[size_t(MToonSlot::Normal)].index>=0;
  if(legacy && (!binding.has_mtoon || binding.material.version()!=MToonVersion::V0))throw std::invalid_argument("Original frame VRM0 export requires an explicit legacy MToon0 material");
  if((legacy?mtoon_normal:pbr_normal) && selected->tangents.empty())throw std::invalid_argument("Normal mapped export has no retained authoritative tangents");
  PaintSourceTexture canonical_normal;
  if(!prepared && input.textures && (!input.textures->baked_normal.rgba.empty() || !input.textures->normal.rgba.empty()))canonical_normal=normal_frame_canonical_export(input,*selected,legacy,cancelled);
  Quat to_native{};if(legacy){if(!input.appearance || !input.appearance->source_basis_quaternion)throw std::invalid_argument("Original frame export has no coordinate basis authority");to_native=input.appearance->source_basis_quaternion->unit();}
  if(!legacy)normal_frame_current_authority(binding,input.mesh,input.deformation.get(),cancelled);
  else {
    // VRM already converted the mesh and current POSITION targets to source
    // axes. Check against retained native authority through its explicit basis.
    const auto &origin=binding.uv_origin->paint->mesh();const size_t active=input.deformation && input.deformation->morphs?input.deformation->morphs->targets.size():0;
    if(input.mesh.indices.size()!=origin.indices.size() || set.current_targets.size()!=active)throw std::invalid_argument("Original frame export target/corner identity differs");
    for(size_t c=0;c<input.mesh.indices.size();++c){if((c&1023)==0 && cancelled && cancelled())throw std::runtime_error("Normal frame export cancelled");auto a=input.mesh.indices[c],b=origin.indices[c];if(length(to_native.rotate(input.mesh.positions.at(a))-origin.positions.at(b))>1e-5f)throw std::invalid_argument("Export rest positions differ from retained frame authority");for(size_t slot=0;slot<active;++slot){auto id=set.current_targets[slot];auto t=std::find_if(set.targets.begin(),set.targets.end(),[&](const auto &t){return t.id==id;});const auto &delta=t->positions.empty()?binding.uv_origin->deformation->morphs->targets.at(*t->origin_slot).delta:t->positions;if(length(to_native.rotate(input.deformation->morphs->targets[slot].delta.at(a))-delta.at(b))>1e-5f)throw std::invalid_argument("Export morph positions differ from retained frame authority");}}
    if(set.pbr) {
      const bool pbr_t=pbr_normal || (!set.pbr->generated_tangents && !set.pbr->tangents.empty());
      const bool mtoon_t=mtoon_normal || (!selected->generated_tangents && !selected->tangents.empty());
      if(pbr_normal && set.pbr->tangents.empty())throw std::invalid_argument("PBR fallback normal map has no retained tangent authority");
      if(!normal_frame_attribute_equal(*selected,*set.pbr,pbr_t&&mtoon_t,cancelled))throw std::invalid_argument("PBR fallback and MToon require different retained normal attributes; one glTF primitive cannot silently replace either frame");
      if(pbr_t && !mtoon_t)tangent_source=set.pbr;
    }
  }
  auto layout=normal_frame_layout(input.mesh,set,cancelled);NormalFrameExport out;out.canonical_normal=std::move(canonical_normal);out.rest_vertices=layout.vertices;out.input_polygon_colors=input.polygon_colors;
  if(prepared){out.prepared_normal.image=std::shared_ptr<const Image>(input.textures,legacy?&input.textures->mtoon_normal:&input.textures->normal);out.prepared_normal.wrap_s=out.prepared_normal.wrap_t=33071;out.prepared_normal.min_filter=out.prepared_normal.mag_filter=9729;}
  out.surface={input.id,input.name,input.transform,input.material,{},input.textures,input.deformation,input.groups,input.appearance,input.material_bindings,input.source_dormant};out.surface.affine_edit=input.affine_edit;auto &mesh=out.surface.mesh;
  Quat from_native=legacy?to_native.inverse():Quat{};
  std::vector<size_t> targets;for(auto id:set.current_targets){auto t=std::find_if(selected->targets.begin(),selected->targets.end(),[&](const auto &t){return t.id==id;});if(t==selected->targets.end())throw std::invalid_argument("Export target normal identity is missing");targets.push_back(size_t(t-selected->targets.begin()));}
  out.normal_targets.resize(targets.size());out.tangent_targets.resize(targets.size());
  for(size_t v=0;v<layout.vertices.size();++v) {
    if((v&1023)==0 && cancelled && cancelled())throw std::runtime_error("Normal frame export cancelled");
    auto vertex=layout.vertices[v],corner=layout.corners[v];
    mesh.positions.push_back(input.mesh.positions.at(vertex));mesh.normals.push_back(from_native.rotate(selected->normals.at(corner)));
    if(!input.mesh.uv.empty())mesh.uv.push_back(input.mesh.uv.at(vertex));
    if(!input.mesh.uv_pins.empty())mesh.uv_pins.push_back(input.mesh.uv_pins.at(vertex));
    if(!tangent_source->tangents.empty()){auto t=tangent_source->tangents[corner];auto T=from_native.rotate({t[0],t[1],t[2]});out.tangents.push_back({T.x,T.y,T.z,t[3]});}
    for(size_t slot=0;slot<targets.size();++slot){const auto &target=selected->targets[targets[slot]];if(!target.normals.empty())out.normal_targets[slot].push_back(from_native.rotate(target.normals[corner]));const auto &tangent_target=tangent_source->targets[targets[slot]];if(!tangent_target.tangents.empty())out.tangent_targets[slot].push_back(from_native.rotate(tangent_target.tangents[corner]));}
  }
  mesh.indices=layout.indices;mesh.uv_seams=input.mesh.uv_seams;mesh.validate();
  out.surface.deformation=normal_frame_remap_deformation(input.deformation,input.mesh,mesh,layout.vertices,cancelled);
  if(input.appearance) {
    if(!input.appearance->asset || input.appearance->source_vertices.size()!=input.mesh.positions.size())throw std::invalid_argument("Frame export source attribute mapping is incomplete");
    if(binding.uv_origin->appearance && (binding.uv_origin->appearance->asset!=input.appearance->asset || binding.uv_origin->appearance->source_mesh!=input.appearance->source_mesh || binding.uv_origin->appearance->source_primitive!=input.appearance->source_primitive))throw std::invalid_argument("Current primitive identity differs from retained normal source authority");
    auto appearance=std::make_shared<CreatorAppearance>(*input.appearance);appearance->source_vertices.clear();
    for(size_t v=0;v<layout.vertices.size();++v){auto corner=layout.corners[v];uint32_t id;
      if(!selected->source_vertices.empty())id=selected->source_vertices.at(corner);
      else if(binding.uv_origin->appearance)id=binding.uv_origin->appearance->source_vertices.at(selected->origin_vertices.at(corner));
      else id=input.appearance->source_vertices.at(layout.vertices[v]);
      appearance->source_vertices.push_back(id);}

    for(const auto &p:appearance->asset->primitives)if(p.source_mesh==appearance->source_mesh && p.source_primitive==appearance->source_primitive){if(out.source)throw std::invalid_argument("Frame export source primitive is ambiguous");out.source=&p;}
    if(!out.source)throw std::invalid_argument("Frame export source primitive is unavailable");
    out.surface.appearance=std::move(appearance);
  }
  return out;
}
} // namespace dome::detail
