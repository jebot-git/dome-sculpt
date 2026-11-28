// SPDX-License-Identifier: GPL-3.0-or-later
#include "texture-set-portable-field.hpp"
#include "polygon-surface-preview.hpp"
#include "polygon-surface-paint.hpp"
#include "affine-surface.hpp"
#include "baked-normal-field.hpp"
#include "emission-authority.hpp"
#include "texture-set-native-grid.hpp"
#include "portable-normal-raster.hpp"
#include "dome/brush.hpp"
#include "dome/source-maps.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace dome::detail {
namespace {
using Uv = std::array<float,2>;
using Bary = std::array<double,3>;
constexpr double normal_error_limit=.02;
constexpr double gutter=2;
constexpr std::array<double,2> probe_offsets{.5,1.};
struct PendingNormalTexel {
 size_t index,work;
 Bary weights;
};
struct PendingNormalProbe { size_t work; Bary weights; };
size_t certificate_scratch(uint32_t size) noexcept{
 if(compute_backend()=="CPU")return 0;
 const auto batch=std::min(size_t(size)*size,portable_normal::batch_pixels);
 return batch*(sizeof(PendingNormalProbe)+sizeof(portable_normal::Probe)+sizeof(portable_normal::Verdict))+
     std::min(batch,portable_normal::batch_charts)*(sizeof(portable_normal::Chart)+sizeof(uint32_t));
}

void stop(const TextureSetLimits &limits){
 if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Retained field export cancelled");
}
Vec3 transport(const PolygonGeometryFrame &before,const PolygonGeometryFrame &current,Vec3 v){
 const auto &a=before.basis,&b=current.basis;
 return normal_bake_math::unit(b.t*dot(a.t,v)+b.b*dot(a.b,v)+b.n*dot(a.n,v));
}
Vec3 perpendicular(Vec3 n){
 const auto axis=std::abs(n.x)<.8f?Vec3{1,0,0}:Vec3{0,1,0};
 return normal_bake_math::unit(axis-n*dot(axis,n));
}
Vec3 direction(const AffineTransform &w,Vec3 v){
 return affine_unit(w.direction({v.x,v.y,v.z}));
}
Vec3 normal(const AffineTransform &w,Vec3 v){
 return affine_unit(w.normal({v.x,v.y,v.z}));
}

struct UvTriangle {
 std::array<Uv,3> corners;
 double resolution,det;
 UvTriangle(std::array<Uv,3> points,uint32_t size):corners(points),resolution(size){
  const auto &a=corners[0],&b=corners[1],&c=corners[2];
  for(const auto &uv:corners)
   if(!std::isfinite(uv[0])||!std::isfinite(uv[1])||
      uv[0]<0||uv[0]>1||uv[1]<0||uv[1]>1)
    throw std::invalid_argument("Retained normal coordinates exceed their unit grid");
  det=(double(b[0])-a[0])*(double(c[1])-a[1])-(double(b[1])-a[1])*(double(c[0])-a[0]);
 }
 bool usable()const noexcept{return std::abs(det)>=1e-14;}
 Bary barycentric(double u,double v)const{
  const auto &a=corners[0],&b=corners[1],&c=corners[2];
  const double p=((u-a[0])*(double(c[1])-a[1])-(v-a[1])*(double(c[0])-a[0]))/det;
  const double q=((double(b[0])-a[0])*(v-a[1])-(double(b[1])-a[1])*(u-a[0]))/det;
  return {1-p-q,p,q};
 }
 std::pair<Bary,double> closest(double u,double v)const{
  const auto inside=barycentric(u,v);
  if(*std::min_element(inside.begin(),inside.end())>=0)return {inside,0};
  Bary best{};
  double distance=std::numeric_limits<double>::infinity();
  for(size_t k=0;k<3;++k){
   const size_t l=(k+1)%3;
   const auto &a=corners[k],&b=corners[l];
   const double dx=double(b[0])-a[0],dy=double(b[1])-a[1];
   const double t=std::clamp(((u-a[0])*dx+(v-a[1])*dy)/(dx*dx+dy*dy),0.,1.);
   const double d=std::hypot(u-a[0]-t*dx,v-a[1]-t*dy)*resolution;
   if(d<distance){distance=d;best={};best[k]=1-t;best[l]=t;}
  }
  return {best,distance};
 }
 std::array<int,4> bounds()const{
  const auto &a=corners[0],&b=corners[1],&c=corners[2];
  const auto start=[&](size_t k){return std::max(0,int(std::floor(std::min({a[k],b[k],c[k]})*resolution-gutter)));};
  const auto end=[&](size_t k){return std::min(int(resolution)-1,int(std::ceil(std::max({a[k],b[k],c[k]})*resolution+gutter)));};
  return {start(0),end(0),start(1),end(1)};
 }
};
Uv interpolate(const std::array<Uv,3> &uv,const Bary &weights){
 std::array<double,2> value{};
 for(size_t i=0;i<3;++i)for(size_t j=0;j<2;++j)value[j]+=weights[i]*uv[i][j];
 return {float(value[0]),float(value[1])};
}
double angle(Vec3 expected,Vec3 actual){
 return std::atan2(double(length(cross(expected,actual))),std::clamp(double(dot(expected,actual)),-1.,1.));
}

template<class Operation>
bool visit_normal_chart(const PolygonSurfacePreview &preview,uint32_t triangle,
    std::span<const PortableTextureSetCorner> corners,const AffineTransform &world,
    uint32_t size,const Operation &operation){
 const auto &original=preview.native_bake()?preview.paint_coordinates():preview.normal_coordinates();
 const float hand=world.orientation()==AffineOrientation::Reversing?-1.f:1.f;
 std::array<Uv,3> source,destination;
 std::array<NormalBakeVertex,3> frame;
 for(size_t k=0;k<3;++k){
  const auto &vertex=corners[k];
  source[k]=original.at(size_t(triangle)*3+k);
  destination[k]=vertex.role;
  const auto t=vertex.tangent;
  frame[k]=normal_bake_vertex(normal(world,vertex.normal),direction(world,{t[0],t[1],t[2]}),
      t[3]*hand,NormalVertexBitangent::UnitCross);
 }
 const UvTriangle chart(destination,size);
 return chart.usable()&&operation(triangle,source,chart,frame);
}

bool normal_map_matches(const PolygonSurfacePreview &preview,std::span<const uint32_t> domain,
    std::span<const PortableTextureSetCorner> corners,const AffineTransform &world,
    const std::shared_ptr<const Image> &image,uint32_t source_size,bool independent,
    size_t &work,const TextureSetLimits &limits,portable_normal::Raster *device=nullptr){
 image->validate();
 const auto size=image->width;
 const bool gpu=compute_backend()!="CPU";
 if(certificate_scratch(size)>limits.maximum_bytes)
  throw std::length_error("Portable normal validation exceeds its byte budget");
 const auto capacity=gpu?std::min(size_t(size)*size,portable_normal::batch_pixels):0;
 const auto chart_capacity=std::min(capacity,portable_normal::batch_charts);
 std::vector<portable_normal::Chart> charts;
 std::vector<uint32_t> chart_triangles;
 std::vector<PendingNormalProbe> pending;
 std::vector<portable_normal::Probe> probes;
 std::vector<portable_normal::Verdict> results(capacity);
 charts.reserve(chart_capacity);
 chart_triangles.reserve(chart_capacity);
 probes.reserve(capacity);
 pending.reserve(capacity);
 std::unique_ptr<portable_normal::Raster> owned_device;
 bool uploaded=false;
 PaintSourceTexture lookup;
 lookup.image=image;
 lookup.wrap_s=lookup.wrap_t=33071;
 lookup.min_filter=lookup.mag_filter=9729;
 const auto reference=[&](uint32_t triangle,const std::array<NormalBakeVertex,3> &frame,
                          const Bary &b,Uv original_uv,Uv emitted_uv){
  const auto wanted=preview.sample_baked_normal(triangle,b,world,original_uv);
  const auto e=sample_source_texture(lookup,emitted_uv[0],emitted_uv[1],false);
  const auto basis=normal_bake_basis(frame,b,NormalFragmentBasis::UnitColumns);
  const auto q=basis.t*(2*e[0]-1)+basis.b*(2*e[1]-1)+basis.n*(2*e[2]-1);
  return length(q)>1e-8f&&angle(wanted,normal_bake_math::unit(q))<=normal_error_limit;
 };
 const auto flush=[&]{
  if(probes.empty())return true;
  if(!uploaded){
   if(!device){
    const auto original=preview.native_bake()?preview.native_bake():preview.baked()->normal_field->pbr();
    owned_device=portable_normal::upload(*original,limits.cancelled);
    device=owned_device.get();
   }
   device->set_emitted_image(*image,limits.cancelled);
   uploaded=true;
  }
  device->certify(charts,probes,std::span(results).first(probes.size()),limits.cancelled);
  bool valid=true;
  for(size_t i=0;i<probes.size();++i){
   if((i&1023)==0)stop(limits);
   if(results[i]==portable_normal::Verdict::Pass)continue;
   const auto &p=probes[i];
   const auto index=p.source.chart;
   const auto &f=charts[index].destination;
   if(results[i]==portable_normal::Verdict::Fail||
      !reference(chart_triangles[index],{f[0],f[1],f[2]},pending[i].weights,
         {p.source.uv[0],p.source.uv[1]},{p.emitted_uv[0],p.emitted_uv[1]})){
    work=pending[i].work;
    valid=false;
    break;
   }
  }
  charts.clear();
  chart_triangles.clear();
  probes.clear();
  pending.clear();
  return valid;
 };
 const auto charge=[&]{
  if(work>=limits.maximum_work){
   if(!flush())return false;
   throw std::length_error("Portable normal validation exceeds its sample budget");
  }
  if((work++&1023)==0)stop(limits);
  return true;
 };
 for(size_t face=0;face<domain.size();++face){
  stop(limits);
  const bool accepted=visit_normal_chart(preview,domain[face],corners.subspan(face*3,3),world,size,[&](uint32_t triangle,const auto &source,const auto &chart,const auto &frame){
   uint32_t slot=UINT32_MAX;
   const auto verify=[&](const Bary &b,Uv uv){
    if(!charge())return false;
    const auto original=interpolate(source,b);
    if(!gpu)return reference(triangle,frame,b,original,uv);
    if(probes.size()==capacity||(slot==UINT32_MAX&&charts.size()==chart_capacity)){
     if(!flush())return false;
     slot=UINT32_MAX;
    }
    if(slot==UINT32_MAX){
     slot=uint32_t(charts.size());
     charts.push_back({portable_normal::source_frame(preview,triangle,world),{frame[0],frame[1],frame[2]}});
     chart_triangles.push_back(triangle);
    }
    probes.push_back({{{float(b[0]),float(b[1]),float(b[2])},{original[0],original[1]},slot},
                      {uv[0],uv[1]}});
    pending.push_back({work,b});
    return true;
   };
   const auto verify_grid=[&](const UvTriangle &grid,bool destination){
    const auto [x0,x1,y0,y1]=grid.bounds();
    for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x)
     for(double dy:probe_offsets)for(double dx:probe_offsets){
      if(!charge())return false;
      const double u=(x+dx)/grid.resolution,v=(y+dy)/grid.resolution;
      const auto b=grid.barycentric(u,v);
      if(*std::min_element(b.begin(),b.end())<0)continue;
      const auto uv=destination?Uv{float(u),float(v)}:interpolate(chart.corners,b);
      if(!verify(b,uv))return false;
     }
    return true;
   };
   if(!verify_grid(chart,true))return false;
   const UvTriangle old_chart(source,source_size);
   if((independent||size!=source_size)&&old_chart.usable()&&!verify_grid(old_chart,false))return false;
   for(size_t k=0;k<3;++k){
    const auto next=(k+1)%3;
    const auto edge=[](const Uv &a,const Uv &b,double pixels){
     return std::hypot(double(a[0])-b[0],double(a[1])-b[1])*pixels;
    };
    const auto length=std::max(edge(chart.corners[k],chart.corners[next],size),
                               edge(source[k],source[next],source_size));
    const auto count=std::max(size_t(1),size_t(std::ceil(length*2)));
    for(size_t i=0;i<=count;++i){
     Bary q{};
     q[k]=1-double(i)/count;q[next]=1-q[k];
     if(!verify(q,interpolate(chart.corners,q)))return false;
    }
   }
   const Bary center{1./3,1./3,1./3};
   return verify(center,interpolate(chart.corners,center));
  });
  if(!accepted){(void)flush();return false;}
 }
 return flush();
}

// Sample in the original field and encode into the explicit glTF tangent
// columns. The bake atlas is transient and can be given its own coordinates.
std::optional<Image> reproject_normal(const PolygonSurfacePreview &preview,
    std::span<const uint32_t> domain,std::span<PortableTextureSetCorner> corners,
    const AffineTransform &world,uint32_t source_size,uint32_t size,bool independent,size_t &work,
    const TextureSetLimits &limits,std::unique_ptr<portable_normal::Raster> &device){
 const size_t pixels=size_t(size)*size;
 if(portable_normal_raster_scratch_bytes(size)>limits.maximum_bytes)
  throw std::length_error("Portable normal raster exceeds its byte budget");
 const auto charge=[&]{
  if(work>=limits.maximum_work)throw std::length_error("Portable normal raster exceeds its sample budget");
  if((work++&1023)==0)stop(limits);
 };
 auto raster=std::make_shared<Image>(Image{size,size,std::vector<uint8_t>(4*pixels)});
 auto &image=*raster;
 for(size_t i=0;i<pixels;++i){if((i&1023)==0)stop(limits);image.rgba[i*4]=image.rgba[i*4+1]=128;image.rgba[i*4+2]=image.rgba[i*4+3]=255;}
 std::vector<float> distance(pixels,std::numeric_limits<float>::infinity());
 const bool gpu=compute_backend()!="CPU";
 const auto capacity=gpu?std::min(pixels,portable_normal::batch_pixels):0;
 const auto chart_capacity=std::min(capacity,portable_normal::batch_charts);
 std::vector<PendingNormalTexel> pending;
 std::vector<portable_normal::Sample> samples;
 std::vector<uint32_t> encoded(capacity);
 std::vector<portable_normal::Chart> charts;
 std::vector<uint32_t> chart_triangles;
 pending.reserve(capacity);
 samples.reserve(capacity);
 charts.reserve(chart_capacity);
 chart_triangles.reserve(chart_capacity);
 const auto reference=[&](uint32_t triangle,const std::array<NormalBakeVertex,3> &frame,
                          const Bary &b,Uv uv)->std::optional<std::array<uint8_t,4>>{
  const auto wanted=preview.sample_baked_normal(triangle,b,world,uv);
  const auto basis=normal_bake_basis(frame,b,NormalFragmentBasis::UnitColumns);
  const auto result=encode_normal_bake(basis,wanted);
  if(!result||result->rgba[2]<128||result->quantized_angle_radians>normal_error_limit)return {};
  return result->rgba;
 };
 const auto write=[&](size_t i,const std::array<uint8_t,4> &rgba){
  std::copy(rgba.begin(),rgba.end(),image.rgba.begin()+i*4);
 };
 const auto flush=[&]{
  if(pending.empty())return true;
  if(!device){
   const auto &original_image=preview.native_bake()?preview.native_bake():preview.baked()->normal_field->pbr();
   device=portable_normal::upload(*original_image,limits.cancelled);
  }
  device->encode(charts,samples,std::span(encoded).first(samples.size()),limits.cancelled);
  bool valid=true;
  for(size_t i=0;i<pending.size();++i){
   const auto &job=pending[i];
   const auto word=encoded[i];
   if(word==portable_normal::needs_reference){
    const auto &sample=samples[i];
    const auto &f=charts[sample.chart].destination;
    const auto result=reference(chart_triangles[sample.chart],{f[0],f[1],f[2]},
        job.weights,{sample.uv[0],sample.uv[1]});
    if(!result){
     // A failed sample historically ends this atlas attempt at this
     // position. Deferred dispatch must preserve its cumulative budget.
     work=job.work;
     valid=false;
     break;
    }
    write(job.index,*result);
   }else write(job.index,{uint8_t(word),uint8_t(word>>8),uint8_t(word>>16),255});
  }
  pending.clear();
  samples.clear();
  charts.clear();
  chart_triangles.clear();
  return valid;
 };
 const size_t columns=size_t(std::ceil(std::sqrt(double(domain.size())))),cell=columns?size/columns:0;
 if(independent&&cell<8)return {};
 const auto &original=preview.native_bake()?preview.paint_coordinates():preview.normal_coordinates();
 for(size_t face=0;independent&&face<domain.size();++face){
  stop(limits);
  const auto i=size_t(domain[face])*3;
  const auto &a=original.at(i),&b=original.at(i+1),&c=original.at(i+2);
  const std::array<Uv,3> source{a,b,c};
  const std::array<Uv,3> reference{{{0,0},{1,0},{0,1}}};
  const bool preserve_layout=UvTriangle(source,size).usable();
  const auto &layout=preserve_layout?source:reference;
  for(size_t axis=0;axis<2;++axis){
   const auto lo=std::min({layout[0][axis],layout[1][axis],layout[2][axis]});
   const auto hi=std::max({layout[0][axis],layout[1][axis],layout[2][axis]});
   const auto tile=axis==0?face%columns:face/columns;
   const auto origin=tile*cell+gutter+1;
   const auto span=cell-2*(gutter+1);
   const auto first_pixel=std::floor(double(lo)*source_size);
   const auto last_pixel=std::ceil(double(hi)*source_size);
   // Keep the source texel centers aligned when the original span fits.
   // Shifting the filtering lattice changes sharp partial-Bake edges.
   const bool aligned=preserve_layout&&last_pixel-first_pixel<=span;
   for(size_t k=0;k<3;++k){
    const double value=layout[k][axis];
    const auto offset=aligned?value*source_size-first_pixel:
                         span*(value-lo)/(double(hi)-lo);
    corners[face*3+k].role[axis]=float((origin+offset)/size);
   }
  }
 }
 for(size_t face=0;face<domain.size();++face){
  stop(limits);
  const bool accepted=visit_normal_chart(preview,domain[face],corners.subspan(face*3,3),world,size,[&](uint32_t triangle,const auto &source,const auto &chart,const auto &frame){
   std::optional<portable_normal::Chart> prepared;
   uint32_t slot=UINT32_MAX;
   const auto [x0,x1,y0,y1]=chart.bounds();
   for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){
    if(work==limits.maximum_work&&!flush())return false;
    charge();
    const double u=(x+.5)/size,v=(y+.5)/size;
    const auto [b,d]=chart.closest(u,v);
    const size_t i=size_t(y)*size+x;
    if(d>gutter||d>=distance[i])continue;
    // Border texels are still part of the consumer's bilinear footprint.
    // Use the source image beyond the chart while holding the frame at
    // the nearest valid barycentrics.
    const auto uv=independent?interpolate(source,chart.barycentric(u,v)):Uv{float(u),float(v)};
    if(gpu){
     if(pending.size()==capacity||(slot==UINT32_MAX&&charts.size()==chart_capacity)){
      if(!flush())return false;
      slot=UINT32_MAX;
     }
     if(slot==UINT32_MAX){
      if(!prepared)prepared=portable_normal::Chart{portable_normal::source_frame(preview,triangle,world),
          {frame[0],frame[1],frame[2]}};
      slot=uint32_t(charts.size());
      charts.push_back(*prepared);
      chart_triangles.push_back(triangle);
     }
     pending.push_back({i,work,b});
     samples.push_back({{float(b[0]),float(b[1]),float(b[2])},{uv[0],uv[1]},slot});
    }else{
     const auto result=reference(triangle,frame,b,uv);
     if(!result)return false;
     write(i,*result);
    }
    // Reserve the accepted distance in triangle order. If its deferred
    // encoding fails, the whole atlas attempt is discarded.
    distance[i]=float(d);
   }
   return true;
  });
  if(!accepted){(void)flush();return {};}
 }
 if(!flush())return {};
 if(!normal_map_matches(preview,domain,corners,world,raster,source_size,independent,work,limits,device.get()))
  return {};
 return std::move(image);
}
std::vector<PortableTextureSetCorner> portable_corners(const PolygonSurfacePreview &preview,
    std::span<const uint32_t> domain,const AffineTransform &world,bool &reproject,
    const TextureSetLimits &limits){
 const bool native=bool(preview.native_bake());
 if(native)preview.validate_bake_world(domain,world,limits.cancelled);
 else preview.validate_world(world,1e-6,limits.cancelled);
 const auto inverse=world.inverse();
 const float hand=world.orientation()==AffineOrientation::Reversing?-1.f:1.f;
 const auto &source_normals=preview.normals();
 std::vector<PortableTextureSetCorner> corners;
 corners.reserve(domain.size()*3);
 const bool normal_role=native||preview.model()==PolygonSurfaceNormalModel::PreparedCorePbr;
 for(auto triangle:domain){
  stop(limits);
  const auto first=size_t(triangle)*3;
  const auto [before,current]=preview.triangle_frames(triangle,world);
  for(size_t k=0;k<3;++k){
   const auto index=first+k;
   const auto n=source_normals.at(index);
   const auto desired_n=transport(before,current,normal(world,n));
   const auto local_n=normal(inverse,desired_n);
   if(length(normal(world,local_n)-desired_n)>5e-6f)
    throw std::invalid_argument("Portable retained N exceeds floating-point transport bounds");
   auto local_t=perpendicular(local_n);
   float sign=1;
   if(normal_role){
    Bary corner{};corner[k]=1;
    const auto old=preview.before_role_basis(triangle,corner,world);
    const auto desired_t=transport(before,current,old.t);
    const auto desired_b=transport(before,current,old.b);
    sign=(dot(cross(desired_n,desired_t),desired_b)<0?-1.f:1.f)*hand;
    const auto raw=direction(inverse,desired_t);
    local_t=normal_bake_math::unit(raw-local_n*dot(raw,local_n));
    const auto t=direction(world,local_t);
    const auto b=normalized(cross(desired_n,t))*(sign*hand);
    // Prepared maps and the portable model both interpolate the three
    // columns. Native Bake instead constructs T and B from the smooth
    // fragment normal, so matching its endpoints alone is insufficient.
    reproject|=(native&&length(n-source_normals[first])>0.f)||
       length(t-desired_t)>5e-6f||length(b-desired_b)>5e-6f;
   }
   const auto paint=preview.paint_coordinates().at(index);
   const auto role=normal_role&&!native?preview.normal_coordinates().at(index):paint;
   corners.push_back({local_n,{local_t.x,local_t.y,local_t.z,sign},paint,role});
  }
 }
 return corners;
}
} // namespace

size_t portable_normal_raster_scratch_bytes(uint32_t size) noexcept{
 const auto pixels=size_t(size)*size;
 const auto batch=compute_backend()=="CPU"?0:std::min(pixels,portable_normal::batch_pixels);
 return pixels*(sizeof(float)+4)+batch*(sizeof(PendingNormalTexel)+sizeof(portable_normal::Sample)+sizeof(uint32_t))+
     std::min(batch,portable_normal::batch_charts)*(sizeof(portable_normal::Chart)+sizeof(uint32_t))+
     certificate_scratch(size);
}

PortableTextureSetField portable_texture_set_field(const TextureSetView &field,
    const AffineTransform &world,size_t &work,const TextureSetLimits &limits){
 limits.validate();stop(limits);
 if(!field.source||!field.domain||!field.material_bindings)
  throw std::invalid_argument("Portable retained field requires its actual member/source/material owners");
 field.source->authenticate_owner(field.source);
 field.source->validate_field(field,limits);
 if(field.material.toon)throw std::invalid_argument("Native toon source export requires its actual typed material compiler");
 const auto &preview=field.source->source_preview();
 if(!preview)throw std::invalid_argument("Portable retained field lacks genuine before role inputs");
 const auto domain=field.domain->triangles();
 const auto count=domain.size()*3,full=preview->geometric_frames().size();
 if(count>limits.maximum_corners||full>limits.maximum_bytes/64||
    count>(limits.maximum_bytes-full*64)/sizeof(PortableTextureSetCorner)||full>limits.maximum_work/96)
  throw std::length_error("Portable retained field exceeds prospective frame/attribute budget");
 PortableTextureSetField out;
 out.normal_role=preview->native_bake()||preview->model()==PolygonSurfaceNormalModel::PreparedCorePbr;
 out.corners=portable_corners(*preview,domain,world,out.reprojected,limits);
 std::shared_ptr<TextureMaps> portable;
 if(retained_material_grid(field))portable=PolygonSurfacePaintCodec::preview_retained_material(field,work,limits);
 else{
  const auto read=field.source->read_view(field,limits);
  const auto maps=PolygonSurfacePaintCodec::preview(*read,field.material_bindings,field.material,limits.cancelled,preview);
  portable=std::make_shared<TextureMaps>(*maps);
 }
 preview->validate_maps(*portable);
 if(!portable->pbr_color.rgba.empty())
  throw std::invalid_argument("Retained resolved colour export needs its actual per-field colour interpretation");
 portable->overlay.reset();
 portable->polygon_surface.reset();
 portable->baked_base.reset();
 portable->mtoon_normal={};
 portable->baked_normal={};
 portable->baked_coverage={};
 if(out.reprojected){
  const uint32_t original=portable->normal.width;
  const auto maximum=portable_normal_export_max_resolution(original);
  std::unique_ptr<portable_normal::Raster> device;
  auto image=reproject_normal(*preview,domain,out.corners,world,original,original,false,work,limits,device);
  if(!image&&maximum>original)
   image=reproject_normal(*preview,domain,out.corners,world,original,maximum,false,work,limits,device);
  if(!image)
   image=reproject_normal(*preview,domain,out.corners,world,original,maximum,true,work,limits,device);
  if(!image)throw std::invalid_argument("Portable retained normals exceed the sampled angular limit after reprojection");
  portable->normal=std::move(*image);
 }
 out.maps=std::move(portable);
 stop(limits);
 return out;
}

std::vector<PortableTextureSetCorner> portable_texture_set_shape(
    const PolygonSurfacePreview &preview,std::span<const uint32_t> domain,
    const AffineTransform &world,std::span<const Uv> coordinates,
    const std::shared_ptr<const TextureMaps> &maps,bool rest_reprojected,
    size_t &work,const TextureSetLimits &limits){
 bool reproject=false;
 auto corners=portable_corners(preview,domain,world,reproject,limits);
 if(rest_reprojected||reproject){
  const auto &source=preview.native_bake()?preview.native_bake():preview.baked()->normal_field->pbr();
  if(!source||!maps||maps->normal.rgba.empty()||coordinates.size()!=corners.size())
   throw std::invalid_argument("Portable posed normal needs its emitted rest map and coordinates");
  bool independent=false;
  for(size_t i=0;i<corners.size();++i){
   independent|=corners[i].role!=coordinates[i];
   corners[i].role=coordinates[i];
  }
  const std::shared_ptr<const Image> image(maps,&maps->normal);
  if(!normal_map_matches(preview,domain,corners,world,image,source->width,independent,work,limits))
   throw std::invalid_argument("The shaped surface cannot preserve this normal map within the portable angular limit");
 }
 return corners;
}

void certify_portable_texture_set_skin(const PolygonSurfacePreview &preview,
    std::span<const uint32_t> domain,std::span<const PortableTextureSetCorner> posed,
    const AffineTransform &world,const std::shared_ptr<const TextureMaps> &maps,
    size_t &work,const TextureSetLimits &limits){
 constexpr auto error="Retained material normals exceed the portable angular limit in the active skin pose";
 if(!maps||posed.size()!=domain.size()*3)
  throw std::invalid_argument("Portable skin lost its posed texture-set corners");
 if(!maps->normal.rgba.empty()){
  const auto &original=preview.native_bake()?preview.native_bake():preview.baked()->normal_field->pbr();
  const auto &source_uv=preview.native_bake()?preview.paint_coordinates():preview.normal_coordinates();
  bool independent=false;
  for(size_t f=0;f<domain.size();++f)
   for(size_t c=0;c<3;++c)independent|=posed[f*3+c].role!=source_uv.at(size_t(domain[f])*3+c);
  const std::shared_ptr<const Image> image(maps,&maps->normal);
  if(!normal_map_matches(preview,domain,posed,world,image,original->width,independent,work,limits))
   throw std::invalid_argument(error);
  return;
 }
 // Unit corner normals share one geometric transport and are smoothly
 // interpolated. Require closer agreement at their corners and check the
 // actual face edges and interior against the portable interpolation.
 for(size_t f=0;f<domain.size();++f){
  stop(limits);
  const auto triangle=domain[f];
  const auto [before,current]=preview.triangle_frames(triangle,world);
  std::array<Vec3,3> wanted{},actual{};
  for(size_t c=0;c<3;++c){
   wanted[c]=transport(before,current,normal(world,preview.normals().at(size_t(triangle)*3+c)));
   actual[c]=normal(world,posed[f*3+c].normal);
   if(angle(wanted[c],actual[c])>normal_error_limit/4)throw std::invalid_argument(error);
  }
  for(size_t c=0;c<3;++c){
   if(angle(normalized(wanted[c]+wanted[(c+1)%3]),normalized(actual[c]+actual[(c+1)%3]))>
      normal_error_limit)throw std::invalid_argument(error);
  }
  if(angle(normalized(wanted[0]+wanted[1]+wanted[2]),normalized(actual[0]+actual[1]+actual[2]))>
     normal_error_limit)throw std::invalid_argument(error);
 }
}
} // namespace dome::detail
