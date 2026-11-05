// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "visibility-heat-bind.hpp"
#include "skin-affine.hpp"
namespace dome::detail {
// Exact independent-component elimination. The existing heat graph/mass and
// ComponentHandleSupport predicates are component-local, and its owning ray
// query explicitly tests only the row's intrinsic component. Removing a fully
// constrained independent block therefore removes no generic edge or wall.
inline VisibilityGeodesicResult partial_current_surface_heat(
 const SculptObject&object,const Skeleton&rig,const AffineTransform&metric,
 BindOptions options,std::span<const HeatSourceSegment>sources,
 std::span<const uint32_t>fixed,const std::function<bool()>&cancelled,
 size_t maximum_bytes){
 if(!object.painted||maximum_bytes<65536||maximum_bytes>256u*1024u*1024u||!options.maximum_work||options.maximum_work>500'000'000)throw std::invalid_argument("Partial current heat owner/resource bounds");
 const auto&raw=object.painted->mesh();const size_t n=raw.positions.size(),corners=raw.indices.size(),targets=object.deformation&&object.deformation->morphs?object.deformation->morphs->targets.size():0;
 if(!n||n>2'000'000||!corners||corners>6'000'000||fixed.size()!=n||targets>128||!options.limits.max_influences||options.limits.max_influences>32)throw std::invalid_argument("Partial current heat geometry/label dimensions");
 weight_surface::Budget work{0,options.maximum_work,cancelled};work.stop();
 visibility_geodesic::Bytes preparation{65536,maximum_bytes};preparation.add(targets,sizeof(std::vector<Vec3>)+64);preparation.add(n,16+12*targets);preparation.add(corners,256+48*targets);
 visibility_geodesic::Bytes retained{65536,maximum_bytes};retained.add(n,6*sizeof(uint32_t)+4*sizeof(Vec3)+sizeof(VertexWeights)+options.limits.max_influences*sizeof(Influence)+128);retained.add(corners,2*sizeof(uint32_t));if(retained.used>maximum_bytes-65536)throw std::length_error("Partial current heat leaves no actual field workspace");
 GeometricBoundarySewing sewn;{std::vector<std::vector<Vec3>>deltas;deltas.reserve(targets);if(object.deformation&&object.deformation->morphs){work.step_items(n,targets*4);object.deformation->morphs->validate(raw);for(const auto&t:object.deformation->morphs->targets)deltas.push_back(t.delta);}sewn=geometric_boundary_sewing(raw,deltas,maximum_bytes-(65536+n*12*targets),work);}
 Mesh generic;generic.positions.reserve(n);generic.normals.reserve(n);generic.indices.reserve(corners);std::vector<uint32_t>root_to_generic(n,UINT32_MAX),raw_to_generic(n,UINT32_MAX);
 bool known=false,unknown=false;
 for(uint32_t i=0;i<n;++i){work.step(16);const auto root=sewn.groups.at(i);if(root>=n||sewn.groups[root]!=root||fixed[root]!=fixed[i])throw std::invalid_argument("Partial current labels disagree on an authenticated raw boundary");const auto label=fixed[i];if(label!=UINT32_MAX){if(label>=rig.bones.size()||!rig.bones[label].deform)throw std::invalid_argument("Partial current hard boundary emits an invalid/nondeform control");known=true;continue;}unknown=true;auto&index=root_to_generic[root];if(index==UINT32_MAX){index=uint32_t(generic.positions.size());generic.positions.push_back(raw.positions[root]);generic.normals.push_back(raw.normals[root]);}raw_to_generic[i]=index;}
 if(!known||!unknown)throw std::logic_error("Partial current heat needs real fixed and generic blocks");
 for(size_t c=0;c<corners;c+=3){work.step(12);const bool a=fixed[raw.indices[c]]==UINT32_MAX,b=fixed[raw.indices[c+1]]==UINT32_MAX,d=fixed[raw.indices[c+2]]==UINT32_MAX;if(a!=b||a!=d)throw std::invalid_argument("Partial current hard boundary crosses an actual triangle");if(a)for(size_t k=0;k<3;++k)generic.indices.push_back(raw_to_generic.at(raw.indices[c+k]));}
 SkinGeometryWork geometry(work.maximum-work.used,cancelled);const auto before=geometry.remaining();auto placed=skin_world_mesh(generic,metric,geometry);work.step(before-geometry.remaining());const size_t expansion=n*(options.limits.max_influences+2);if(expansion>=work.maximum-work.used)throw std::length_error("Partial current heat expansion work before solve");options.maximum_work=work.maximum-work.used-expansion;
 auto field=visibility_heat_bind(placed,rig,options,cancelled,maximum_bytes-retained.used,sources,HeatDomainPolicy::ComponentHandleSupport);work.step(field.work);SkinWeights output;output.vertices.reserve(n);for(size_t i=0;i<n;++i){work.step(options.limits.max_influences+2);if(fixed[i]!=UINT32_MAX)output.vertices.push_back({{fixed[i],1}});else output.vertices.push_back(field.weights.vertices.at(raw_to_generic[i]));}
 field.weights=std::move(output);field.work=work.used;field.workspace=std::max(preparation.used,retained.used+field.workspace);return field;
}
}
