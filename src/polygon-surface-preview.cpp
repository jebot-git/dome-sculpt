// SPDX-License-Identifier: GPL-3.0-or-later
#include "polygon-surface-preview.hpp"
#include "dome/paint-sampling.hpp"
#include "normal-frame-runtime.hpp"
#include "baked-normal-field.hpp"
#include "paint-overlay-packet.hpp"
#include "portable-normal-raster.hpp"
#include "dome/source-maps.hpp"
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
 const auto mapping=owner->trace().triangle_permutation(l);const auto old_mesh=owner->trace().original()->triangulate(l.geometry).mesh;
 auto result=std::shared_ptr<PolygonSurfacePreview>(new PolygonSurfacePreview);result->binding_=std::move(binding);result->owner_=owner;result->attachment_=paint.paint_sampling_attachment();result->reference_=result->binding_->native_reference;result->physical_=result->binding_->physical_emission;result->origin_=result->binding_->uv_origin;
 auto streams=std::make_shared<Streams>();
 PosedNormalFrames raw;Mesh native;
 if(owner->has_normal_field()){
  result->model_=PolygonSurfaceNormalModel::PreparedCorePbr;NormalFrameLayout layout;layout.vertices=source.indices;layout.corners.resize(source.indices.size());layout.indices.resize(source.indices.size());for(size_t c=0;c<source.indices.size();++c)layout.corners[c]=layout.indices[c]=uint32_t(c);raw=normal_frame_preview(*before.material_bindings->normal_frames,layout,before.deformation.get(),l.geometry.cancelled,1);if(raw.tangents[0].size()!=source.indices.size())throw std::invalid_argument("Polygon preview cannot synthesize missing before artist tangents");streams->tangents.reserve(corners);streams->normal.reserve(corners);
 }else{
  native=before.deformation?before.deformation->preview(source):source;
  streams->native_frame_charts.reserve(corners/3);
 }
 streams->normals.reserve(corners);streams->paint.reserve(corners);streams->frames.reserve(corners/3);
 for(size_t t=0;t<corners/3;++t){
  stop(l.geometry.cancelled);const auto &map=mapping.at(t);std::array<Vec3,3>p{},q{};std::array<std::array<float,2>,3>uv{},current_uv{};
  const std::array<std::array<float,2>,3>chart{{{0,0},{1,0},{0,1}}};
  for(size_t k=0;k<3;++k){p[k]=old_mesh.positions.at(old_mesh.indices.at(map.source_triangle*3+k));uv[k]=owner->has_normal_field()?owner->corners_->normal.at(map.source_triangle*3+k):chart[k];}
  std::array<uint8_t,3> native_chart{};
  for(size_t c=0;c<3;++c){
   size_t k=0;while(k<3&&map.source_barycentrics[c][k]!=1)++k;
   if(k==3)throw std::invalid_argument("Polygon preview needs its genuine barycentric refinement adapter");
   const size_t before_corner=map.source_triangle*3+k;
   const auto original=owner->corners_->native.at(before_corner);
   q[c]=mesh.positions.at(mesh.indices.at(t*3+c));current_uv[c]=uv[k];
   streams->paint.push_back(owner->corners_->paint.at(before_corner));
   if(owner->has_normal_field()){
    streams->normal.push_back(uv[k]);streams->normals.push_back(raw.normals[0].at(original));
    streams->tangents.push_back(raw.tangents[0].at(original));
   }else{
    native_chart[c]=uint8_t(k);
    streams->normals.push_back(native.normals.at(native.indices.at(original)));
   }
  }
  if(!owner->has_normal_field())streams->native_frame_charts.push_back(native_chart);
  const auto old=polygon_geometry_frame(p,uv,AffineTransform{}),current=polygon_geometry_frame(q,current_uv,AffineTransform{});streams->frames.push_back({std::array<float,4>{old.basis.n.x,old.basis.n.y,old.basis.n.z,old.handedness},std::array<float,4>{old.basis.t.x,old.basis.t.y,old.basis.t.z,0},std::array<float,4>{current.basis.n.x,current.basis.n.y,current.basis.n.z,current.handedness},std::array<float,4>{current.basis.t.x,current.basis.t.y,current.basis.t.z,0}});
 }
 result->streams_=std::move(streams);result->baked_=paint.baked();result->self_=result;stop(l.geometry.cancelled);return result;
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
 const auto equal=[](const Image&a,const Image&b){return a.width==b.width&&a.height==b.height&&a.rgba==b.rgba;};
 if(owner_->has_normal_field()){
  const auto&field=*baked_->normal_field;
  if(!equal(maps.normal,*field.pbr())||!equal(maps.mtoon_normal,*field.mtoon()))
   throw std::invalid_argument("Polygon surface changed its independently owned before normal role");
 }else if(native_bake_){
  if(!equal(maps.normal,*native_bake_)||!maps.mtoon_normal.rgba.empty())
   throw std::invalid_argument("Polygon surface changed its own native normal bake");
 }else if(maps.normal.width||maps.normal.height||!maps.normal.rgba.empty()||
          maps.mtoon_normal.width||maps.mtoon_normal.height||!maps.mtoon_normal.rgba.empty())
  throw std::invalid_argument("Polygon N-only preview cannot invent a normal texture");
}
Vec3 PolygonSurfacePreview::normal_at(size_t triangle,std::array<double,3>b,const AffineTransform&world,const std::function<bool()>&cancel)const{
 stop(cancel);if(!matches(binding_,attachment_))throw std::invalid_argument("Polygon CPU field query lost its actual current binding");
 PolygonSurfaceTransferLimits limits;limits.geometry.cancelled=cancel;
 if(model_==PolygonSurfaceNormalModel::NativeNOnly){
  if(native_bake_)return sample_native_bake(triangle,b,world);
  if(!posed_frames_)return owner_->native_normal(triangle,b,world,limits);
  Vec3 direction{};
  for(size_t k=0;k<3;++k)
   direction+=transform_normal(world,streams_->normals.at(triangle*3+k))*float(b[k]);
  const auto [before,current]=triangle_frames(triangle,world);
  return transport_polygon_direction(normal_bake_math::unit(direction),before,current);
 }
 if(!posed_frames_&&own(baked_->normal_field,owner_->before()->paint->baked()->normal_field))
  return owner_->pbr_normal(triangle,b,world,limits);
 std::array<double,2> uv{};
 for(size_t k=0;k<3;++k){
  const auto at=streams_->normal.at(triangle*3+k);
  for(size_t axis=0;axis<2;++axis)uv[axis]+=at[axis]*b[k];
 }
 return sample_baked_normal(triangle,b,world,{float(uv[0]),float(uv[1])});
}
Vec3 PolygonSurfacePreview::sample_native_bake(
    size_t triangle,std::array<double,3> b,const AffineTransform &world)const{
 if(!native_bake_)throw std::invalid_argument("Polygon has no authored native normal bake");
 std::array<double,2> uv{};
 for(size_t k=0;k<3;++k){
  const auto at=streams_->paint.at(triangle*3+k);
  for(size_t axis=0;axis<2;++axis)uv[axis]+=at[axis]*b[k];
 }
 return sample_baked_normal(triangle,b,world,{float(uv[0]),float(uv[1])});
}
Vec3 PolygonSurfacePreview::sample_baked_normal(size_t triangle,std::array<double,3> b,
    const AffineTransform &world,std::array<float,2> uv)const{
 const auto image=native_bake_?native_bake_:baked_&&baked_->normal_field?baked_->normal_field->pbr():nullptr;
 if(!image)throw std::invalid_argument("Polygon has no authored normal image");
 PaintSourceTexture texture;
 texture.image=image;
 texture.wrap_s=texture.wrap_t=33071;
 texture.min_filter=texture.mag_filter=9729;
 return transport_baked_normal(triangle,b,world,sample_source_texture(texture,uv[0],uv[1],false));
}
Vec3 PolygonSurfacePreview::transport_baked_normal(size_t triangle,std::array<double,3> b,
    const AffineTransform &world,std::array<float,4> e)const{
 const auto [before,current]=triangle_frames(triangle,world);
 const auto basis=before_role_basis(triangle,b,world);
 const Vec3 detail{e[0]*2-1,e[1]*2-1,e[2]*2-1};
 const bool available=native_bake_?e[3]>0&&length(detail)>1e-8f:
     std::max({std::abs(detail.x),std::abs(detail.y),std::abs(detail.z)})>1e-20f;
 const auto normal=available?basis.t*detail.x+basis.b*detail.y+basis.n*detail.z:basis.n;
 return transport_polygon_direction(normal,before,current);
}
std::shared_ptr<const PolygonSurfacePreview> PolygonSurfacePreview::with_bake(const PaintedMesh &paint)const{
 return with_bake(paint,native_bake_);
}
std::shared_ptr<const PolygonSurfacePreview> PolygonSurfacePreview::with_bake(
    const PaintedMesh &paint,std::shared_ptr<const Image> native_bake)const{
 if(!matches(binding_,paint.paint_sampling_attachment()))
  throw std::invalid_argument("Polygon preview Bake lost its actual sampling attachment");
 if(native_bake&&(model_!=PolygonSurfaceNormalModel::NativeNOnly||!paint.baked()))
  throw std::invalid_argument("Native normal bake requires its original polygon model and material bake");
 if(own(baked_,paint.baked())&&own(native_bake_,native_bake))return self_.lock();
 auto result=std::shared_ptr<PolygonSurfacePreview>(new PolygonSurfacePreview(*this));
 result->baked_=paint.baked();
 result->native_bake_=std::move(native_bake);
 result->self_=result;
 return result;
}
std::shared_ptr<const PolygonSurfacePreview> PolygonSurfacePreview::with_geometry(
    const std::shared_ptr<const Mesh> &rest,const Mesh &posed,std::span<const uint32_t> triangles,
    const std::function<bool()> &cancelled)const{
 stop(cancelled);
 if(!attachment_||!own(rest,attachment_->authored_geometry())||
    posed.positions.size()!=rest->positions.size()||posed.indices!=rest->indices||posed.uv!=rest->uv||
    streams_->frames.size()!=rest->indices.size()/3)
  throw std::invalid_argument("Posed source field must retain its original topology and UV domain");
 std::shared_ptr<std::vector<std::array<std::array<float,4>,4>>> frames;
 for(const auto triangle:triangles){
  stop(cancelled);
  const auto &original=streams_->frames.at(triangle);
  std::array<Vec3,3> positions;
  constexpr std::array<std::array<float,2>,3> unit_chart{{{0,0},{1,0},{0,1}}};
  std::array<std::array<float,2>,3> chart;
  bool changed=false;
  for(size_t k=0;k<3;++k){
   const auto corner=size_t(triangle)*3+k;
   const auto vertex=rest->indices[corner];
   const auto before=rest->positions[vertex];
   positions[k]=posed.positions[vertex];
   changed|=positions[k].x!=before.x||positions[k].y!=before.y||positions[k].z!=before.z;
   chart[k]=streams_->normal.empty()?unit_chart[streams_->native_frame_charts.at(triangle)[k]]:
                                     streams_->normal.at(corner);
  }
  if(!changed)continue;
  const auto current=polygon_geometry_frame(positions,chart,AffineTransform{});
  if(!frames)frames=std::make_shared<std::vector<std::array<std::array<float,4>,4>>>(streams_->frames);
  const auto &n=current.basis.n,&t=current.basis.t;
  frames->at(triangle)={original[0],original[1],
      std::array<float,4>{n.x,n.y,n.z,current.handedness},std::array<float,4>{t.x,t.y,t.z,0}};
 }
 if(!frames&&!posed_frames_)return self_.lock();
 auto result=std::shared_ptr<PolygonSurfacePreview>(new PolygonSurfacePreview(*this));
 result->posed_frames_=std::move(frames);
 result->self_=result;
 stop(cancelled);
 return result;
}
std::pair<PolygonGeometryFrame,PolygonGeometryFrame> PolygonSurfacePreview::triangle_frames(
    size_t triangle,const AffineTransform &world)const{
 const auto &frame=geometric_frames().at(triangle);
 const float hand=world.orientation()==AffineOrientation::Reversing?-1.f:1.f;
 const auto place=[&](size_t first){
  const auto raw_n=frame[first],raw_t=frame[first+1];
  const auto n=transform_normal(world,{raw_n[0],raw_n[1],raw_n[2]});
  const auto raw=normal_bake_math::vec(world.direction({raw_t[0],raw_t[1],raw_t[2]}));
  const auto t=normal_bake_math::unit(raw-n*dot(n,raw));
  const auto w=raw_n[3]*hand;
  return PolygonGeometryFrame{{t,cross(n,t)*w,n},w};
 };
 return {place(0),place(2)};
}
NormalBakeBasis PolygonSurfacePreview::before_role_basis(
    size_t triangle,std::array<double,3> weights,const AffineTransform &world)const{
 if(model_==PolygonSurfaceNormalModel::NativeNOnly){
  Vec3 n{};
  for(size_t k=0;k<3;++k)n+=transform_normal(world,streams_->normals.at(triangle*3+k))*float(weights[k]);
  n=normal_bake_math::unit(n);
  const auto [before,current]=triangle_frames(triangle,world);
  const auto axis=before.basis.t;
  const auto t=normal_bake_math::unit(axis-n*dot(axis,n));
  return {t,cross(n,t)*before.handedness,n};
 }
 const float hand=world.orientation()==AffineOrientation::Reversing?-1.f:1.f;
 std::array<NormalBakeVertex,3> vertices;
 for(size_t k=0;k<3;++k){
  const auto c=triangle*3+k;
  const auto n=transform_normal(world,streams_->normals.at(c));
  const auto t=streams_->tangents.at(c);
  vertices[k]=normal_bake_vertex(n,transform_tangent(world,t),t[3]*hand,NormalVertexBitangent::UnitCross);
 }
 return normal_bake_basis(vertices,weights,NormalFragmentBasis::UnitColumns);
}
portable_normal::SourceFrame portable_normal::source_frame(const PolygonSurfacePreview &preview,
    size_t triangle,const AffineTransform &world){
 const auto [before,current]=preview.triangle_frames(triangle,world);
 // Authenticate both placed frames once before device probes use them.
 (void)transport_polygon_direction(before.basis.n,before,current);
 SourceFrame out;
 out.before=before.basis;
 out.current=current.basis;
 out.handedness=before.handedness;
 out.native=preview.model()==PolygonSurfaceNormalModel::NativeNOnly;
 const float hand=world.orientation()==AffineOrientation::Reversing?-1.f:1.f;
 for(size_t k=0;k<3;++k){
  const auto corner=triangle*3+k;
  const auto n=transform_normal(world,preview.normals().at(corner));
  if(out.native)out.vertices[k].n=n;
  else {
   const auto &t=preview.tangents().at(corner);
   out.vertices[k]=normal_bake_vertex(n,transform_tangent(world,t),t[3]*hand,NormalVertexBitangent::UnitCross);
  }
 }
 return out;
}
void PolygonSurfacePreview::validate_world(size_t triangle,const AffineTransform &world,double minimum,
    const std::function<bool()> &cancel)const{
 stop(cancel);
 const auto [before,current]=triangle_frames(triangle,world);
 const auto axis=before.basis.n;
 const auto first=transform_normal(world,streams_->normals.at(triangle*3));
 const float sign=dot(first,axis)<0?-1.f:1.f;
 for(size_t k=0;k<3;++k){
  const auto n=transform_normal(world,streams_->normals.at(triangle*3+k));
  if(sign*dot(n,axis)<=minimum)
   throw std::invalid_argument("Native normal bake needs a conditioned original tangent across the triangle");
 }
}
void PolygonSurfacePreview::validate_bake_world(std::span<const uint32_t> triangles,
    const AffineTransform &world,const std::function<bool()> &cancel)const{
 validate_world(world,1e-6,cancel);
 if(model_==PolygonSurfaceNormalModel::NativeNOnly)
  for(const auto triangle:triangles)validate_world(triangle,world,1e-6,cancel);
}
void PolygonSurfacePreview::validate_world(const AffineTransform&world,double minimum,const std::function<bool()>&cancel)const{
 if(!std::isfinite(minimum)||minimum<=0||minimum>1)throw std::invalid_argument("Invalid complete polygon field conditioning bound");
 stop(cancel);(void)world.inverse();const float hand=world.orientation()==AffineOrientation::Reversing?-1.f:1.f;
 for(size_t t=0;t<streams_->normals.size()/3;++t){
  stop(cancel);std::array<NormalBakeVertex,3>v{};
  for(size_t k=0;k<3;++k){const auto c=t*3+k;const auto n=transform_normal(world,streams_->normals[c]);if(model_==PolygonSurfaceNormalModel::NativeNOnly)v[k].n=n;else{const auto raw=streams_->tangents.at(c);v[k]=normal_bake_vertex(n,transform_tangent(world,raw),raw[3]*hand,NormalVertexBitangent::UnitCross);}}
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
 validate_world(world,1e-6,cancel);if(streams_->frames.size()>128*1024*1024/sizeof(streams_->frames[0]))throw std::length_error("Polygon primitive frames exceed their bounded upload workspace");
 auto result=geometric_frames();const float hand=world.orientation()==AffineOrientation::Reversing?-1.f:1.f;
 for(auto&frame:result){stop(cancel);for(size_t first:{size_t(0),size_t(2)}){
  const auto raw_n=frame[first],raw_t=frame[first+1];const auto n=transform_normal(world,{raw_n[0],raw_n[1],raw_n[2]});
  auto t=normal_bake_math::vec(world.direction({raw_t[0],raw_t[1],raw_t[2]}));t=normal_bake_math::unit(t-n*dot(n,t));
  frame[first]={n.x,n.y,n.z,raw_n[3]*hand};frame[first+1]={t.x,t.y,t.z,0};
 }}stop(cancel);return result;
}
} // namespace dome::detail
