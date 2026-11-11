// SPDX-License-Identifier: GPL-3.0-or-later
#include "texture-set-portable-field.hpp"
#include "polygon-surface-preview.hpp"
#include "polygon-surface-paint.hpp"
#include "affine-surface.hpp"
#include "emission-authority.hpp"
#include "dome/material-bindings.hpp"
namespace dome::detail {namespace {
void stop(const TextureSetLimits&l){if(l.cancelled&&l.cancelled())throw std::runtime_error("Retained field export cancelled");}
bool same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
Vec3 unit(AffineVector v){return affine_unit(v);}
struct Basis {Vec3 n,t,b;};
Basis basis(const std::array<float,4>&n,const std::array<float,4>&t){const auto N=normalized(Vec3{n[0],n[1],n[2]}),T=normalized(Vec3{t[0],t[1],t[2]});return {N,T,normalized(cross(N,T))*n[3]};}
Vec3 transport(const Basis&a,const Basis&b,Vec3 v){return b.t*dot(a.t,v)+b.b*dot(a.b,v)+b.n*dot(a.n,v);}
bool exact_similarity(const AffineTransform&w){const auto a=w.direction({1,0,0}),b=w.direction({0,1,0}),c=w.direction({0,0,1});const auto dot=[](const auto&x,const auto&y){return x[0]*y[0]+x[1]*y[1]+x[2]*y[2];};return dot(a,b)==0&&dot(a,c)==0&&dot(b,c)==0&&dot(a,a)>0&&dot(a,a)==dot(b,b)&&dot(a,a)==dot(c,c);}
Tangent perpendicular(Vec3 n){const auto axis=std::abs(n.x)<.8f?Vec3{1,0,0}:Vec3{0,1,0};const auto t=normalized(axis-n*dot(axis,n));return {t.x,t.y,t.z,1};}
}
PortableTextureSetField portable_texture_set_field(const TextureSetView&field,const AffineTransform&world,const TextureSetLimits&limits){
 limits.validate();stop(limits);if(!field.source||!field.domain||!field.material_bindings)throw std::invalid_argument("Portable retained field requires its actual member/source/material owners");field.source->authenticate_owner(field.source);field.source->validate_field(field,limits);
 if(field.material.toon)throw std::invalid_argument("Native toon source export requires its actual typed material compiler");
 const auto&p=field.source->source_preview();if(!p)throw std::invalid_argument("Portable retained field lacks genuine before role inputs");
 if(field.painted->has_physical_emission()||hdr_emission(field.material.emissive)||field.material_bindings->physical_emission||hdr_emission(field.material_bindings->native_reference.emission))throw std::invalid_argument("Retained texture-set FLOAT/HDR export needs its precision-qualified per-material emission compiler");
 const size_t count=field.domain->triangles().size()*3,full=p->geometric_frames().size();
 if(count>limits.maximum_corners||full>limits.maximum_bytes/64||count>(limits.maximum_bytes-full*64)/sizeof(PortableTextureSetCorner)||full>limits.maximum_work/96)throw std::length_error("Portable retained field exceeds prospective frame/attribute budget");
 p->validate_world(world,1e-6,limits.cancelled);const auto frames=p->placed_frames(world,limits.cancelled);const auto inverse=world.inverse();const float hand=world.orientation()==AffineOrientation::Reversing?-1.f:1.f;
 PortableTextureSetField out;out.corners.reserve(count);out.normal_role=p->model()==PolygonSurfaceNormalModel::PreparedCorePbr;const bool conformal=exact_similarity(world);
 for(auto triangle:field.domain->triangles()){
  stop(limits);const auto first=size_t(triangle)*3;const auto rawN=p->normals().at(first);
  for(size_t k=1;k<3;++k)if(!same(rawN,p->normals().at(first+k))&&(out.normal_role||!conformal))throw std::invalid_argument("Varying retained before normals require a certified whole-triangle standard-export basis adapter");
  const auto&a=frames.at(triangle);const auto before=basis(a[0],a[1]),current=basis(a[2],a[3]);
  const auto N=unit(world.normal({rawN.x,rawN.y,rawN.z}));const auto desiredN=normalized(transport(before,current,N));const auto localN=unit(inverse.normal({desiredN.x,desiredN.y,desiredN.z}));
  auto localT=perpendicular(localN);
  if(out.normal_role){
   const auto rawT=p->tangents().at(first);for(size_t k=1;k<3;++k)if(rawT!=p->tangents().at(first+k))throw std::invalid_argument("Varying retained artist tangent/W requires its whole-triangle standard-export basis adapter");
   const auto T=unit(world.direction({rawT[0],rawT[1],rawT[2]}));if(std::abs(dot(N,T))>2e-7f)throw std::invalid_argument("Oblique retained before N/T cannot be silently orthogonalized for glTF");
   const auto B=normalized(cross(N,T))*(rawT[3]*hand);const auto desiredT=normalized(transport(before,current,T)),desiredB=normalized(transport(before,current,B));
   const auto t=unit(inverse.direction({desiredT.x,desiredT.y,desiredT.z}));const float w=(dot(cross(desiredN,desiredT),desiredB)<0?-1.f:1.f)*hand;localT={t.x,t.y,t.z,w};
   if(std::abs(dot(localN,t))>2e-6f)throw std::invalid_argument("Portable retained local N/T lost orthogonality under the actual world transform");
   const auto actualN=unit(world.normal({localN.x,localN.y,localN.z})),actualT=unit(world.direction({t.x,t.y,t.z}));const auto actualB=normalized(cross(actualN,actualT))*(w*hand);
   if(length(actualN-desiredN)>5e-6f||length(actualT-desiredT)>5e-6f||length(actualB-desiredB)>5e-6f)throw std::invalid_argument("Portable retained frame exceeds floating-point transport conditioning bounds");
  }else if(length(unit(world.normal({localN.x,localN.y,localN.z}))-desiredN)>5e-6f)throw std::invalid_argument("Portable retained N exceeds floating-point transport bounds");
  for(size_t k=0;k<3;++k){const auto paint=p->paint_coordinates().at(first+k);const auto role=out.normal_role?p->normal_coordinates().at(first+k):paint;
   auto n=localN;auto t=localT;
   if(!out.normal_role&&!same(rawN,p->normals().at(first+k))){const auto raw=p->normals().at(first+k);const auto original=unit(world.normal({raw.x,raw.y,raw.z})),desired=normalized(transport(before,current,original));n=unit(inverse.normal({desired.x,desired.y,desired.z}));t=perpendicular(n);if(length(unit(world.normal({n.x,n.y,n.z}))-desired)>5e-6f)throw std::invalid_argument("Portable smooth retained N exceeds floating-point transport bounds");}
   out.corners.push_back({n,t,paint,role});}
 }
 const auto read=field.source->read_view(field,limits);const auto maps=PolygonSurfacePaintCodec::preview(*read,field.material_bindings,field.material,limits.cancelled,p);p->validate_maps(*maps);
 if(!maps->pbr_color.rgba.empty())throw std::invalid_argument("Retained resolved colour export needs its actual per-field colour interpretation");
 auto portable=std::make_shared<TextureMaps>(*maps);portable->overlay.reset();portable->polygon_surface.reset();portable->baked_base.reset();portable->mtoon_normal={};portable->baked_normal={};portable->baked_coverage={};
 // Full original image bytes and actual current composite stay intact. Source
 // ownership remains in the authoring Scene; these are emitted glTF resources.
 out.maps=std::move(portable);stop(limits);return out;
}
}
