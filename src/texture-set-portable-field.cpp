// SPDX-License-Identifier: GPL-3.0-or-later
#include "texture-set-portable-field.hpp"
#include "polygon-surface-preview.hpp"
#include "polygon-surface-paint.hpp"
#include "affine-surface.hpp"
#include "emission-authority.hpp"
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

// Sample in the original field and encode into the explicit glTF tangent
// columns. The bake atlas is transient and can be given its own coordinates.
std::optional<Image> reproject_normal(const PolygonSurfacePreview &preview,
    std::span<const uint32_t> domain,std::span<PortableTextureSetCorner> corners,
    const AffineTransform &world,uint32_t source_size,uint32_t size,bool independent,size_t &work,
    const TextureSetLimits &limits){
 const size_t pixels=size_t(size)*size;
 if(pixels>limits.maximum_bytes/8)throw std::length_error("Portable normal raster exceeds its byte budget");
 const auto charge=[&]{
  if(work>=limits.maximum_work)throw std::length_error("Portable normal raster exceeds its sample budget");
  if((work++&1023)==0)stop(limits);
 };
 auto raster=std::make_shared<Image>(Image{size,size,std::vector<uint8_t>(4*pixels)});
 auto &image=*raster;
 for(size_t i=0;i<pixels;++i){if((i&1023)==0)stop(limits);image.rgba[i*4]=image.rgba[i*4+1]=128;image.rgba[i*4+2]=image.rgba[i*4+3]=255;}
 std::vector<float> distance(pixels,std::numeric_limits<float>::infinity());
 const size_t columns=size_t(std::ceil(std::sqrt(double(domain.size())))),cell=columns?size/columns:0;
 if(independent&&cell<8)return {};
 const float hand=world.orientation()==AffineOrientation::Reversing?-1.f:1.f;
 const auto &original=preview.native_bake()?preview.paint_coordinates():preview.normal_coordinates();
 const auto visit=[&](size_t face,auto &&operation){
  const auto triangle=domain[face];
  std::array<Uv,3> source,destination;
  std::array<NormalBakeVertex,3> frame;
  for(size_t k=0;k<3;++k){
   auto &vertex=corners[face*3+k];
   source[k]=original.at(size_t(triangle)*3+k);
   destination[k]=vertex.role;
   const auto t=vertex.tangent;
   frame[k]=normal_bake_vertex(normal(world,vertex.normal),direction(world,{t[0],t[1],t[2]}),
       t[3]*hand,NormalVertexBitangent::UnitCross);
  }
  const UvTriangle chart(destination,size);
  return chart.usable()&&operation(triangle,source,chart,frame);
 };
 for(size_t face=0;independent&&face<domain.size();++face){
  stop(limits);
  const auto i=size_t(domain[face])*3;
  const auto &a=original.at(i),&b=original.at(i+1),&c=original.at(i+2);
  const std::array<Uv,3> source{a,b,c};
  const std::array<Uv,3> reference{{{0,0},{1,0},{0,1}}};
  const auto &layout=UvTriangle(source,size).usable()?source:reference;
  for(size_t axis=0;axis<2;++axis){
   const auto lo=std::min({layout[0][axis],layout[1][axis],layout[2][axis]});
   const auto hi=std::max({layout[0][axis],layout[1][axis],layout[2][axis]});
   const auto tile=axis==0?face%columns:face/columns;
   for(size_t k=0;k<3;++k)
    corners[face*3+k].role[axis]=float((tile*cell+gutter+1+(cell-2*(gutter+1))*
       ((double(layout[k][axis])-lo)/(double(hi)-lo)))/size);
  }
 }
 for(size_t face=0;face<domain.size();++face){
  stop(limits);
  const bool accepted=visit(face,[&](uint32_t triangle,const auto &source,const auto &chart,const auto &frame){
   const auto [x0,x1,y0,y1]=chart.bounds();
   for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){
    charge();
    const double u=(x+.5)/size,v=(y+.5)/size;
    const auto [b,d]=chart.closest(u,v);
    const size_t i=size_t(y)*size+x;
    if(d>gutter||d>=distance[i])continue;
    const auto uv=independent?interpolate(source,b):Uv{float(u),float(v)};
    const auto wanted=preview.sample_baked_normal(triangle,b,world,uv);
    const auto basis=normal_bake_basis(frame,b,NormalFragmentBasis::UnitColumns);
    const auto encoded=encode_normal_bake(basis,wanted);
    if(!encoded||encoded->rgba[2]<128||encoded->quantized_angle_radians>normal_error_limit)return false;
    distance[i]=float(d);
    std::copy(encoded->rgba.begin(),encoded->rgba.end(),image.rgba.begin()+i*4);
   }
   return true;
  });
  if(!accepted)return {};
 }
 PaintSourceTexture lookup;
 lookup.image=raster;
 lookup.wrap_s=lookup.wrap_t=33071;
 lookup.min_filter=lookup.mag_filter=9729;
 for(size_t face=0;face<domain.size();++face){
  stop(limits);
  const bool accepted=visit(face,[&](uint32_t triangle,const auto &source,const auto &chart,const auto &frame){
   const auto verify=[&](const Bary &b,Uv uv){
    charge();
    const auto wanted=preview.sample_baked_normal(triangle,b,world,interpolate(source,b));
    const auto e=sample_source_texture(lookup,uv[0],uv[1],false);
    const auto basis=normal_bake_basis(frame,b,NormalFragmentBasis::UnitColumns);
    const auto q=basis.t*(2*e[0]-1)+basis.b*(2*e[1]-1)+basis.n*(2*e[2]-1);
    return length(q)>1e-8f&&angle(wanted,normal_bake_math::unit(q))<=normal_error_limit;
   };
   const auto verify_grid=[&](const UvTriangle &grid,bool destination){
    const auto [x0,x1,y0,y1]=grid.bounds();
    for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x)
     for(double dy:probe_offsets)for(double dx:probe_offsets){
      charge();
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
  if(!accepted)return {};
 }
 return std::move(image);
}
} // namespace

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
 if(field.painted->has_physical_emission()||hdr_emission(field.material.emissive)||
    field.material_bindings->physical_emission||hdr_emission(field.material_bindings->native_reference.emission))
  throw std::invalid_argument("Retained texture-set FLOAT/HDR export needs its precision-qualified per-material emission compiler");
 const auto domain=field.domain->triangles();
 const auto count=domain.size()*3,full=preview->geometric_frames().size();
 if(count>limits.maximum_corners||full>limits.maximum_bytes/64||
    count>(limits.maximum_bytes-full*64)/sizeof(PortableTextureSetCorner)||full>limits.maximum_work/96)
  throw std::length_error("Portable retained field exceeds prospective frame/attribute budget");
 const bool native=bool(preview->native_bake());
 if(native)preview->validate_bake_world(domain,world,limits.cancelled);
 else preview->validate_world(world,1e-6,limits.cancelled);
 const auto inverse=world.inverse();
 const float hand=world.orientation()==AffineOrientation::Reversing?-1.f:1.f;
 const auto &source_normals=preview->normals();
 const auto &source_tangents=preview->tangents();
 PortableTextureSetField out;
 out.corners.reserve(count);
 out.normal_role=native||preview->model()==PolygonSurfaceNormalModel::PreparedCorePbr;
 bool reproject=false;
 for(auto triangle:domain){
  stop(limits);
  const auto first=size_t(triangle)*3;
  const auto [before,current]=preview->triangle_frames(triangle,world);
  for(size_t k=0;k<3;++k){
   const auto index=first+k;
   const auto n=source_normals.at(index);
   const auto desired_n=transport(before,current,normal(world,n));
   const auto local_n=normal(inverse,desired_n);
   if(length(normal(world,local_n)-desired_n)>5e-6f)
    throw std::invalid_argument("Portable retained N exceeds floating-point transport bounds");
   auto local_t=perpendicular(local_n);
   float sign=1;
   if(out.normal_role){
    Bary corner{};corner[k]=1;
    const auto old=preview->before_role_basis(triangle,corner,world);
    const auto desired_t=transport(before,current,old.t);
    const auto desired_b=transport(before,current,old.b);
    sign=(dot(cross(desired_n,desired_t),desired_b)<0?-1.f:1.f)*hand;
    const auto raw=direction(inverse,desired_t);
    local_t=normal_bake_math::unit(raw-local_n*dot(raw,local_n));
    const auto t=direction(world,local_t);
    const auto b=normalized(cross(desired_n,t))*(sign*hand);
    reproject|=length(n-source_normals[first])>0||
       (!native&&source_tangents.at(index)!=source_tangents.at(first))||
       length(t-desired_t)>5e-6f||length(b-desired_b)>5e-6f;
   }
   const auto paint=preview->paint_coordinates().at(index);
   const auto role=out.normal_role&&!native?preview->normal_coordinates().at(index):paint;
   out.corners.push_back({local_n,{local_t.x,local_t.y,local_t.z,sign},paint,role});
  }
 }
 const auto read=field.source->read_view(field,limits);
 const auto maps=PolygonSurfacePaintCodec::preview(*read,field.material_bindings,field.material,limits.cancelled,preview);
 preview->validate_maps(*maps);
 if(!maps->pbr_color.rgba.empty())
  throw std::invalid_argument("Retained resolved colour export needs its actual per-field colour interpretation");
 auto portable=std::make_shared<TextureMaps>(*maps);
 portable->overlay.reset();
 portable->polygon_surface.reset();
 portable->baked_base.reset();
 portable->mtoon_normal={};
 portable->baked_normal={};
 portable->baked_coverage={};
 if(reproject){
  const uint32_t original=maps->normal.width;
  const auto maximum=portable_normal_export_max_resolution(original);
  auto image=reproject_normal(*preview,domain,out.corners,world,original,original,false,work,limits);
  if(!image&&maximum>original)
   image=reproject_normal(*preview,domain,out.corners,world,original,maximum,false,work,limits);
  if(!image)
   image=reproject_normal(*preview,domain,out.corners,world,original,maximum,true,work,limits);
  if(!image)throw std::invalid_argument("Portable retained normals exceed the sampled angular limit after reprojection");
  portable->normal=std::move(*image);
 }
 out.maps=std::move(portable);
 stop(limits);
 return out;
}
} // namespace dome::detail
