// SPDX-License-Identifier: GPL-3.0-or-later
#include "polygon-surface-preview.hpp"
#include "dome/paint-sampling.hpp"
#include "normal-frame-runtime.hpp"
#include "baked-normal-field.hpp"
#include "paint-overlay-packet.hpp"
#include <limits>
namespace dome::detail {namespace {
bool own(const auto&a,const auto&b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);}
void stop(const std::function<bool()>&f){if(f&&f())throw std::runtime_error("Polygon surface preview cancelled");}
Vec3 transform_normal(const AffineTransform&w,Vec3 n){return normal_bake_math::unit(normal_bake_math::vec(w.normal(normal_bake_math::vec(n))));}
Vec3 transform_tangent(const AffineTransform&w,const std::array<float,4>&t){return normal_bake_math::unit(normal_bake_math::vec(w.direction({t[0],t[1],t[2]})));}
}
std::shared_ptr<const PolygonSurfacePreview> PolygonSurfacePreview::create(const PaintedMesh&paint,
 std::shared_ptr<const SourceMaterialBindings>binding,const PolygonSurfaceTransferLimits&l){
 stop(l.geometry.cancelled);if(!binding||!binding->polygon_surface_transfer)throw std::invalid_argument("Polygon preview needs an actual current field binding");
 auto owner=binding->polygon_surface_transfer;owner->validate_binding(*binding,l);owner->validate_current(paint,l);
 const auto &mesh=paint.mesh();const size_t corners=mesh.indices.size();
 if(corners>2'000'000||corners%3)throw std::length_error("Polygon preview corner packing exceeds native renderer capacity");
 // Conservative simultaneous raw role/layout, triangle maps, derivatives,
 // packed streams and geometric frame buffers (not just final output bytes).
 const size_t per_corner=512,per_source_vertex=128;
 const auto &before=*owner->before_;const auto &source=before.paint->mesh();
 if(source.positions.size()>l.maximum_working_bytes/per_source_vertex)throw std::length_error("Polygon preview exceeds prospective source-preview quota");
 const size_t source_bytes=source.positions.size()*per_source_vertex;
 if(corners>(l.maximum_working_bytes-source_bytes)/per_corner)throw std::length_error("Polygon preview exceeds prospective packed input quota");
 const auto mapping=owner->trace_.triangle_permutation(l);const auto old_mesh=owner->trace_.original()->triangulate(l.geometry).mesh;
 auto result=std::shared_ptr<PolygonSurfacePreview>(new PolygonSurfacePreview);result->binding_=std::move(binding);result->owner_=owner;result->attachment_=paint.paint_sampling_attachment();result->reference_=result->binding_->native_reference;result->physical_=result->binding_->physical_emission;result->origin_=result->binding_->uv_origin;
 PosedNormalFrames raw;Mesh native;
 if(owner->has_normal_field()){
  result->model_=PolygonSurfaceNormalModel::PreparedCorePbr;NormalFrameLayout layout;layout.vertices=source.indices;layout.corners.resize(source.indices.size());layout.indices.resize(source.indices.size());for(size_t c=0;c<source.indices.size();++c)layout.corners[c]=layout.indices[c]=uint32_t(c);raw=normal_frame_preview(*before.material_bindings->normal_frames,layout,before.deformation.get(),l.geometry.cancelled,1);if(raw.tangents[0].size()!=source.indices.size())throw std::invalid_argument("Polygon preview cannot synthesize missing before artist tangents");result->tangents_.reserve(corners);result->normal_.reserve(corners);
 }else native=before.deformation?before.deformation->preview(source):source;
 result->normals_.reserve(corners);result->paint_.reserve(corners);result->frames_.reserve(corners/3);
 for(size_t t=0;t<corners/3;++t){
  stop(l.geometry.cancelled);const auto &map=mapping.at(t);std::array<Vec3,3>p{},q{};std::array<std::array<float,2>,3>uv{},current_uv{};
  const std::array<std::array<float,2>,3>chart{{{0,0},{1,0},{0,1}}};
  for(size_t k=0;k<3;++k){p[k]=old_mesh.positions.at(old_mesh.indices.at(map.source_triangle*3+k));uv[k]=owner->has_normal_field()?owner->normal_corners_.at(map.source_triangle*3+k):chart[k];}
  for(size_t c=0;c<3;++c){size_t k=0;while(k<3&&map.source_barycentrics[c][k]!=1)++k;if(k==3)throw std::invalid_argument("Polygon preview needs its genuine barycentric refinement adapter");const size_t before_corner=map.source_triangle*3+k;const auto original=owner->native_corners_.at(before_corner);q[c]=mesh.positions.at(mesh.indices.at(t*3+c));current_uv[c]=uv[k];result->paint_.push_back(owner->paint_corners_.at(before_corner));if(owner->has_normal_field()){result->normal_.push_back(uv[k]);result->normals_.push_back(raw.normals[0].at(original));result->tangents_.push_back(raw.tangents[0].at(original));}else result->normals_.push_back(native.normals.at(native.indices.at(original)));}
  const auto old=polygon_geometry_frame(p,uv,AffineTransform{}),current=polygon_geometry_frame(q,current_uv,AffineTransform{});result->frames_.push_back({std::array<float,4>{old.basis.n.x,old.basis.n.y,old.basis.n.z,old.handedness},std::array<float,4>{old.basis.t.x,old.basis.t.y,old.basis.t.z,0},std::array<float,4>{current.basis.n.x,current.basis.n.y,current.basis.n.z,current.handedness},std::array<float,4>{current.basis.t.x,current.basis.t.y,current.basis.t.z,0}});
 }
 result->baked_=paint.baked();result->self_=result;stop(l.geometry.cancelled);return result;
}
bool PolygonSurfacePreview::matches(const std::shared_ptr<const SourceMaterialBindings>&binding,
 const std::shared_ptr<const PaintSamplingAttachment>&attachment)const noexcept{
 if(!binding||binding->has_mtoon||binding->normal_frames||binding->canonical_attributes||binding->canonical_uv||binding->canonical_paint_sampling||binding->native_paint_sampling||binding->native_paint_frames||binding->normal_preparation||binding->color_preparation||binding->key_derivation||binding->normal_frame!=SourceMaterialNormalFrame::UvDerivative||binding->sampling_reparameterized||!own(binding->uv_origin,origin_)||!own(binding,binding_)||!own(attachment,attachment_)||!own(binding->polygon_surface_transfer,owner_)||binding->physical_emission!=physical_)return false;
 for(const auto &role:binding->roles)if(role.image)return false;
 const auto &r=binding->native_reference;return r.color==reference_.color&&r.emission==reference_.emission&&r.alpha==reference_.alpha&&r.alpha_cutoff==reference_.alpha_cutoff&&r.double_sided==reference_.double_sided;
}
bool PolygonSurfacePreview::matches_binding(const SourceMaterialBindings&binding)const noexcept{
 return binding_.get()==&binding&&matches(binding_,attachment_);
}
void PolygonSurfacePreview::validate_maps(const TextureMaps&maps)const{
 if(!own(maps.polygon_surface,self_.lock())||!matches(binding_,attachment_)||maps.source_only||maps.source_maps||maps.color_projection||maps.normal_projection||!own(maps.baked_base,baked_))throw std::invalid_argument("Polygon surface maps lost their actual separate current field owner");
 if(!maps.overlay||!attachment_->matches(maps.overlay->native_geometry(),maps.overlay->native_domain()))throw std::invalid_argument("Polygon surface float/paint packet lost its actual current attachment");
 if(owner_->has_normal_field()){
  const auto&field=*owner_->before()->paint->baked()->normal_field;
  const auto equal=[](const Image&a,const Image&b){return a.width==b.width&&a.height==b.height&&a.rgba==b.rgba;};
  if(!equal(maps.normal,*field.pbr())||!equal(maps.mtoon_normal,*field.mtoon()))throw std::invalid_argument("Polygon surface changed its independently owned before normal role");
 }else if(maps.normal.width||maps.normal.height||!maps.normal.rgba.empty()||maps.mtoon_normal.width||maps.mtoon_normal.height||!maps.mtoon_normal.rgba.empty())throw std::invalid_argument("Polygon N-only preview cannot invent a normal texture");
}
Vec3 PolygonSurfacePreview::normal_at(size_t triangle,std::array<double,3>b,const AffineTransform&world,const std::function<bool()>&cancel)const{
 stop(cancel);if(!matches(binding_,attachment_))throw std::invalid_argument("Polygon CPU field query lost its actual current binding");
 // The same immutable before inputs and actual full world basis are used by
 // the CPU evaluator; no GPU readback or edited-UV tangent inference exists.
 PolygonSurfaceTransferLimits l;l.geometry.cancelled=cancel;return owner_->has_normal_field()?owner_->pbr_normal(triangle,b,world,l):owner_->native_normal(triangle,b,world,l);
}
void PolygonSurfacePreview::validate_world(const AffineTransform&world,double minimum,const std::function<bool()>&cancel)const{
 if(!std::isfinite(minimum)||minimum<=0||minimum>1)throw std::invalid_argument("Invalid complete polygon field conditioning bound");
 stop(cancel);(void)world.inverse();const float hand=world.orientation()==AffineOrientation::Reversing?-1.f:1.f;
 for(size_t t=0;t<normals_.size()/3;++t){
  stop(cancel);std::array<NormalBakeVertex,3>v{};
  for(size_t k=0;k<3;++k){const auto c=t*3+k;const auto n=transform_normal(world,normals_[c]);if(model_==PolygonSurfaceNormalModel::NativeNOnly)v[k].n=n;else{const auto raw=tangents_.at(c);v[k]=normal_bake_vertex(n,transform_tangent(world,raw),raw[3]*hand,NormalVertexBitangent::UnitCross);}}
  if(model_==PolygonSurfaceNormalModel::NativeNOnly){for(size_t i=0;i<3;++i)for(size_t j=i+1;j<3;++j)if(dot(v[i].n,v[j].n)<=minimum)throw std::invalid_argument("Polygon N-only interpolation needs a finer whole-triangle nonzero proof");continue;}
  if(v[0].w!=v[1].w||v[0].w!=v[2].w)throw std::invalid_argument("Polygon before artist W changes inside its triangle");
  // det(T(b),B(b),N(b)) is a homogeneous cubic in barycentrics. Its10
  // Bernstein coefficients are the averages of like-monomial determinants.
  // Unit input columns bound all interpolated lengths by1, so positive signed
  // coefficients also bound the UnitColumns relative determinant from below.
  std::array<double,27>sum{};std::array<unsigned,27>count{};
  for(size_t a=0;a<3;++a)for(size_t b=0;b<3;++b)for(size_t c=0;c<3;++c){std::array<size_t,3>ids{a,b,c};std::sort(ids.begin(),ids.end());const auto key=ids[0]*9+ids[1]*3+ids[2];sum[key]+=normal_bake_math::dot(normal_bake_math::vec(v[a].t),normal_bake_math::cross(normal_bake_math::vec(v[b].b),normal_bake_math::vec(v[c].n)))*v[0].w;++count[key];}
  const double upper=1+8*std::numeric_limits<float>::epsilon(),bound=minimum*upper*upper*upper+64*std::numeric_limits<double>::epsilon();
  for(size_t k=0;k<sum.size();++k)if(count[k]&&sum[k]/count[k]<=bound)throw std::invalid_argument("Polygon before role needs a finer complete-triangle conditioning proof");
 }
 stop(cancel);
}
std::vector<std::array<std::array<float,4>,4>>PolygonSurfacePreview::placed_frames(const AffineTransform&world,const std::function<bool()>&cancel)const{
 validate_world(world,1e-6,cancel);if(frames_.size()>128*1024*1024/sizeof(frames_[0]))throw std::length_error("Polygon primitive frames exceed their bounded upload workspace");
 auto result=frames_;const float hand=world.orientation()==AffineOrientation::Reversing?-1.f:1.f;
 for(auto&frame:result){stop(cancel);for(size_t first:{size_t(0),size_t(2)}){
  const auto raw_n=frame[first],raw_t=frame[first+1];const auto n=transform_normal(world,{raw_n[0],raw_n[1],raw_n[2]});
  auto t=normal_bake_math::vec(world.direction({raw_t[0],raw_t[1],raw_t[2]}));t=normal_bake_math::unit(t-n*dot(n,t));
  frame[first]={n.x,n.y,n.z,raw_n[3]*hand};frame[first+1]={t.x,t.y,t.z,0};
 }}stop(cancel);return result;
}
} // namespace dome::detail
