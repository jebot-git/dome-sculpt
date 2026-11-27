// SPDX-License-Identifier: GPL-3.0-or-later
#include "texture-set-export.hpp"
#include "texture-set-paint-access.hpp"
#include "texture-set-portable-field.hpp"
#include "polygon-surface-preview.hpp"
#include "baked-normal-field.hpp"
#include "normal-frame-runtime.hpp"
#include "emission-authority.hpp"
#include "emission-export.hpp"
#include "paint-emission-sample.hpp"
#include "texture-set-native-grid.hpp"
#include "texture-set-surface.hpp"
#include "mtoon-role-sampling.hpp"
#include "deformation-active.hpp"
#include "source-frame-skin.hpp"
#include "affine-interaction.hpp"
#include "normal-bake-encoding.hpp"
#include "dome/material-bindings.hpp"
#include "dome/material-visibility.hpp"
#include "dome/paint-sampling.hpp"
#include <bit>
#include <cstring>
#include <limits>
#include <map>
#include <numeric>
namespace dome::detail {
namespace {
class ExportBudgetExceeded : public std::length_error {
public:
 using std::length_error::length_error;
};
class MToonFloatUnavailable : public std::invalid_argument {
public:
 using std::invalid_argument::invalid_argument;
};

void stop(const TextureSetLimits&l){if(l.cancelled&&l.cancelled())throw std::runtime_error("Texture-set export cancelled");}
void charge(size_t&v,size_t n,size_t u,size_t cap){
 if(v>cap||(u&&n>(cap-v)/u))
  throw ExportBudgetExceeded("Texture-set export exceeds its working budget ("+std::to_string(v)+
      " already charged, "+std::to_string(n)+" entries at "+std::to_string(u)+
      ", limit "+std::to_string(cap)+")");
 v+=n*u;
}
template<class T>bool owner(const std::shared_ptr<const T>&a,const std::shared_ptr<const T>&b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);}
bool material(const Material&a,const Material&b){return a.name==b.name&&a.color==b.color&&a.metallic==b.metallic&&a.roughness==b.roughness&&a.emissive==b.emissive&&a.toon==b.toon&&a.double_sided==b.double_sided&&a.alpha==b.alpha&&a.alpha_cutoff==b.alpha_cutoff&&a.unlit==b.unlit;}

const std::array<double,256> &mtoon_linear_bytes(){
 static const auto values=[]{
  std::array<double,256> table{};
  for(size_t i=0;i<table.size();++i)table[i]=mtoon_srgb_to_linear(float(i)/255.f);
  return table;
 }();
 return values;
}

uint8_t mtoon_srgb_byte(double linear){
 return uint8_t(std::lround(mtoon_linear_to_srgb(float(std::clamp(linear,0.,1.)))*255.));
}

bool clamp_linear_mtoon_source(const PaintSourceTexture &role,const MToonTexture &texture){
 return role.image&&role.wrap_s==33071&&role.wrap_t==33071&&
     (!role.min_filter||role.min_filter==9729)&&(!role.mag_filter||role.mag_filter==9729)&&
     mtoon_coordinate(texture)==0&&texture.offset==std::array<float,2>{}&&
     texture.scale==std::array<float,2>{1,1}&&texture.rotation==0;
}

bool aligned_mtoon_source(const PaintSourceTexture &role,const MToonTexture &texture,
                          uint32_t resolution){
 return clamp_linear_mtoon_source(role,texture)&&role.image->width==resolution&&role.image->height==resolution;
}

struct MToonPaintSource {
 std::shared_ptr<const Image> aligned;
 std::array<double,4> constant{};
 std::array<double,4> sample(size_t offset)const{
  if(!aligned)return constant;
  const auto *pixel=aligned->rgba.data()+offset;
  const auto &linear=mtoon_linear_bytes();
  return {linear[pixel[0]],linear[pixel[1]],linear[pixel[2]],pixel[3]/255.};
 }
};

std::optional<MToonPaintSource> mtoon_paint_source(const SourceMaterialBindings &binding,
    const MToonMaterial &effective,MToonSlot role,uint32_t resolution,const TextureSetLimits &limits){
 const auto &view=binding.roles[size_t(role)];
 const auto &image=view.image;
 MToonPaintSource source;
 if(!image){
  const auto fallback=mtoon_default_texel(role);
  std::copy(fallback.begin(),fallback.end(),source.constant.begin());
  return source;
 }
 image->validate();
 const size_t channels=role==MToonSlot::Emission?3:4;
 for(size_t at=4;at<image->rgba.size();at+=4){
  if((at&16383)==0)stop(limits);
  if(!std::equal(image->rgba.begin(),image->rgba.begin()+channels,image->rgba.begin()+at)){
   if(!aligned_mtoon_source(view,effective.textures[size_t(role)],resolution))return {};
   source.aligned=image;
   return source;
  }
 }
 for(size_t k=0;k<4;++k)
  source.constant[k]=k<3?mtoon_linear_bytes()[image->rgba[k]]:image->rgba[k]/255.;
 return source;
}

struct MToonSourceTap {
 std::array<double,4> background{},error{};
 double uncovered=0;
};

double mtoon_source_filter_error(const std::array<const MToonSourceTap *,4> &taps,
                                size_t channel){
 double min_source=taps[0]->background[channel],max_source=min_source;
 double min_uncovered=taps[0]->uncovered,max_uncovered=min_uncovered;
 for(const auto *tap:taps){
  min_source=std::min(min_source,tap->background[channel]);
  max_source=std::max(max_source,tap->background[channel]);
  min_uncovered=std::min(min_uncovered,tap->uncovered);
  max_uncovered=std::max(max_uncovered,tap->uncovered);
 }
 // The difference between filtering source*uncovered and multiplying
 // their filtered values is a weighted covariance. Cauchy-Schwarz and
 // the maximum variance of a bounded scalar apply to all four tap weights.
 return (max_source-min_source)*(max_uncovered-min_uncovered)*.25;
}

struct MToonShadeTap {
 std::array<double,3> correction{};
 MToonSourceTap sampling;
};

struct MToonNormalTap {
 Vec3 original, baked;
 float coverage=0, quantization=0;
};

Vec3 normal_rgb(const uint8_t *p){
 return {2.f*p[0]/255.f-1,2.f*p[1]/255.f-1,2.f*p[2]/255.f-1};
}

bool portable_mtoon_bake_frame(const Mesh &mesh,const TextureSetView &field,
    std::span<const Tangent> tangents,const AffineTransform &world,const TextureSetLimits &limits){
 const auto &m=world.matrix();
 const auto product=[&](size_t a,size_t b){
  return m[4*a]*m[4*b]+m[4*a+1]*m[4*b+1]+m[4*a+2]*m[4*b+2];
 };
 const double scale=product(0,0), tolerance=scale*1e-6;
 if(world.orientation()!=AffineOrientation::Preserving||scale<=0||
    std::abs(product(1,1)-scale)>tolerance||std::abs(product(2,2)-scale)>tolerance||
    std::abs(product(0,1))>tolerance||std::abs(product(0,2))>tolerance||std::abs(product(1,2))>tolerance)
  return false;
 constexpr float error=1e-5f;
 size_t processed=0;
 for(auto face:field.domain->triangles()){
  if((processed++&1023)==0)stop(limits);
  const size_t first=size_t(face)*3;
  const auto a=mesh.indices[first],b=mesh.indices[first+1],c=mesh.indices[first+2];
  const auto n=mesh.normals[a];
  const auto &t=tangents[first];
  const Vec3 axis{t[0],t[1],t[2]};
  if(length(mesh.normals[b]-n)>error||length(mesh.normals[c]-n)>error||
     tangents[first+1]!=t||tangents[first+2]!=t||std::abs(length(n)-1)>error||
     std::abs(length(axis)-1)>error||std::abs(dot(n,axis))>error)return false;
  // An original UV-derivative map and the exported Mikk frame must use
  // the same axes. Check the actual triangle derivatives and handedness.
  const auto p=mesh.positions[b]-mesh.positions[a],q=mesh.positions[c]-mesh.positions[a];
  const auto u=mesh.uv[a],v=mesh.uv[b],w=mesh.uv[c];
  const float du=v[0]-u[0],dv=v[1]-u[1],eu=w[0]-u[0],ev=w[1]-u[1];
  const float det=du*ev-dv*eu;
  if(std::abs(det)<1e-9f)return false;
  const auto derivative=(p*ev-q*dv)/det, bitangent=(q*du-p*eu)/det;
  const auto projected=derivative-n*dot(derivative,n);
  if(length(projected)<1e-8f||length(normalized(projected)-axis)>error||
     dot(cross(n,axis),bitangent)*t[3]<=0)return false;
 }
 return true;
}

struct MToonNormalCorner {
 Tangent tangent;
 std::array<float,2> uv;
};
struct PortableMToonFrames {
 std::vector<std::array<MToonNormalCorner,3>> triangles;
 bool normal_coordinates=false;
};

std::optional<PortableMToonFrames> portable_mtoon_derivative_frames(const Mesh &mesh,
    const TextureSetView &field,const AffineTransform &world,const TextureSetLimits &limits) {
 const auto &binding=field.material_bindings;
 constexpr size_t normal=size_t(MToonSlot::Normal);
 if(!native_texture_set_mtoon(field.painted.get(),field.appearance.get(),binding.get()) ||
    binding->material.version()!=MToonVersion::V1 || !binding->roles[normal].image ||
    world.orientation()!=AffineOrientation::Preserving)
  return {};
 const auto &p=binding->material.parameters;
 const auto &texture=binding->material.textures[normal];
 const bool transformed=texture.offset!=std::array<float,2>{} ||
     texture.scale!=std::array<float,2>{1,1} || texture.rotation!=0;
 const bool scrolling=p.scroll!=std::array<float,2>{};
 if(p.rotation_speed!=0 ||
    (scrolling && (transformed || binding->roles[size_t(MToonSlot::UvMask)].image)))
  return {};
 const auto &image=*binding->roles[normal].image;
 const double cosine=std::cos(double(texture.rotation)), sine=std::sin(double(texture.rotation));
 const auto transform_direction=[&](double u,double v){
  u*=texture.scale[0];
  v*=texture.scale[1];
  return std::array{cosine*u-sine*v,sine*u+cosine*v};
 };
 using namespace normal_bake_math;
 const AffineInteractionFrame placement(world);
 PortableMToonFrames result;
 result.normal_coordinates=transformed||texture.transform_texcoord>=0;
 auto &frames=result.triangles;
 const size_t faces=field.domain?field.domain->triangles().size():mesh.indices.size()/3;
 frames.reserve(faces);
 constexpr double axis_limit=2e-4, hand_margin=1e-3;
 const auto variance_bound=[](const std::array<D,3> &corners){
  double diameter=0;
  for(size_t a=0;a<3;++a)for(size_t b=0;b<a;++b){
   const D delta{corners[a][0]-corners[b][0],corners[a][1]-corners[b][1],corners[a][2]-corners[b][2]};
   diameter=std::max(diameter,dot(delta,delta));
  }
  return diameter/3;
 };
 for(size_t i=0;i<faces;++i){
  const auto face=field.domain?field.domain->triangles()[i]:i;
  if((frames.size()&1023)==0)stop(limits);
  const size_t first=size_t(face)*3;
  const auto a=mesh.indices[first], b=mesh.indices[first+1], c=mesh.indices[first+2];
  const auto &u=mesh.uv[a], &v=mesh.uv[b], &w=mesh.uv[c];
  const auto [du,dv]=transform_direction(double(v[0])-u[0],double(v[1])-u[1]);
  const auto [eu,ev]=transform_direction(double(w[0])-u[0],double(w[1])-u[1]);
  const double determinant=du*ev-dv*eu;
  if(std::abs(determinant)<=1e-8*std::max(std::hypot(du,dv)*std::hypot(eu,ev),1e-20))
   return {};
  const auto dp=world.direction(vec(mesh.positions[b]-mesh.positions[a]));
  const auto dq=world.direction(vec(mesh.positions[c]-mesh.positions[a]));
  const double sign=std::copysign(1.,determinant);
  D raw_t{},raw_b{};
  for(size_t axis=0;axis<3;++axis){
   raw_t[axis]=(dp[axis]*ev-dq[axis]*dv)*sign;
   raw_b[axis]=(dq[axis]*du-dp[axis]*eu)*sign;
  }
  if(std::hypot(raw_t[0],raw_t[1],raw_t[2])<1e-20 ||
     std::hypot(raw_b[0],raw_b[1],raw_b[2])<1e-20)
   return {};
  raw_t=unit(raw_t);raw_b=unit(raw_b);
  const std::array n{unit(placement.normal(vec(mesh.normals[a]))),
                     unit(placement.normal(vec(mesh.normals[b]))),
                     unit(placement.normal(vec(mesh.normals[c])))};
  std::array<D,3> tangents{};
  double cone=1,tangent_cone=1,max_dot=0,min_projection=1,max_projection=0;
  for(size_t corner=0;corner<3;++corner){
   cone=std::min(cone,dot(n[0],n[corner]));
   const double projection=dot(n[corner],raw_t);
   max_dot=std::max(max_dot,std::abs(projection));
   D t{};
   for(size_t axis=0;axis<3;++axis)t[axis]=raw_t[axis]-n[corner][axis]*projection;
   const double length=std::hypot(t[0],t[1],t[2]);
   if(length<1e-5)return {};
   min_projection=std::min(min_projection,length);
   max_projection=std::max(max_projection,length);
   tangents[corner]=unit(t);
   tangent_cone=std::min(tangent_cone,dot(tangents[0],tangents[corner]));
  }
  if(cone<.5||tangent_cone<.5||max_dot>=cone)return {};
  const double normal_variance=variance_bound(n), tangent_variance=variance_bound(tangents);
  // With barycentric weights w, A=sum(w*N) has |A| >= cone. The difference
  // between averaging the vertex projections and projecting along A/|A|
  // has two terms: covariance of N, and the normalization of A. Each is
  // bounded by 1-|A|^2. Normalizing the three projections before averaging
  // adds the covariance of the projection lengths with the unit tangents.
  const double component=max_dot/cone;
  const double projected_floor=std::sqrt(1-component*component);
  const double projection_error=std::min(2*normal_variance,max_dot+component)+
      .5*(max_projection-min_projection)*std::sqrt(tangent_variance);
  if(projection_error>=projected_floor)return {};
  const double tangent_error=2*projection_error/(projected_floor-projection_error);
  // Interpolating cross products can differ from crossing the interpolated
  // columns by their covariance. The positive cone bounds prevent a zero
  // tangent or bitangent anywhere inside the triangle.
  const double cross_error=tangent_error+std::sqrt(normal_variance*tangent_variance);
  if(cross_error>=cone*tangent_cone||
     2*cross_error/(cone*tangent_cone-cross_error)>axis_limit*.9)
   return {};
  const double side=std::copysign(1.,dot(cross(n[0],tangents[0]),raw_b));
  std::array<MToonNormalCorner,3> stored{};
  for(size_t corner=0;corner<3;++corner){
   const auto &t=tangents[corner];
   if(dot(cross(n[corner],t),raw_b)*side<hand_margin)return {};
   const auto local=unit(placement.inverse().direction(t));
   const auto vertex=mesh.indices[first+corner];
   auto &value=stored[corner].tangent;
   value={float(local[0]),float(local[1]),float(local[2]),float(side)};
   const D written{value[0],value[1],value[2]};
   const auto placed=unit(world.direction(written));
   if(std::abs(dot(written,unit(vec(mesh.normals[vertex]))))>1e-5 ||
      std::hypot(placed[0]-t[0],placed[1]-t[1],placed[2]-t[2])>2e-6)
    return {};
   auto &uv=stored[corner].uv;
   uv=mesh.uv[vertex];
   if(transformed){
    // Bound binary32 interpolation and affine arithmetic in texel units.
    // Large canceling offsets can otherwise lose the source sampling phase.
    const double scaled_u=std::abs(double(uv[0])*texture.scale[0]);
    const double scaled_v=std::abs(double(uv[1])*texture.scale[1]);
    constexpr double slack=8*std::numeric_limits<float>::epsilon();
    const double span_u=std::abs(cosine)*scaled_u+std::abs(sine)*scaled_v+std::abs(double(texture.offset[0]));
    const double span_v=std::abs(sine)*scaled_u+std::abs(cosine)*scaled_v+std::abs(double(texture.offset[1]));
    if(slack*std::max(span_u*image.width,span_v*image.height)>1./64)
     return {};
    const auto mapped=transform_direction(uv[0],uv[1]);
    uv={float(mapped[0]+texture.offset[0]),float(mapped[1]+texture.offset[1])};
   }
  }
  if(transformed){
   const auto &x=stored[0].uv,&y=stored[1].uv,&z=stored[2].uv;
   const double u0=double(y[0])-x[0],v0=double(y[1])-x[1],u1=double(z[0])-x[0],v1=double(z[1])-x[1];
   const double stored_determinant=u0*v1-v0*u1;
   if(stored_determinant*determinant<=0 ||
      std::abs(stored_determinant-determinant)>std::abs(determinant)*1e-3)
    return {};
  }
  frames.push_back(stored);
 }
 return result;
}

// A cell's filtered vectors are convex combinations of its four taps.
// Project them onto one unit tap to bound length from below and use the
// longest tap as the upper bound. Keep a separate bound on positive Z.
struct NormalCone {
 Vec3 reference{};
 float low=1,high=0,min_z=1;
 void add(Vec3 v){
  const float norm=length(v);
  if(high==0&&norm>0)reference=v/norm;
  low=std::min(low,dot(reference,v));
  high=std::max(high,norm);
  min_z=std::min(min_z,v.z);
 }
 float normalization_error() const { return std::max(1-low,high-1); }
};

double mtoon_normal_cell_error(const std::array<const MToonNormalTap *,4> &taps){
 constexpr double frame_and_sampling_slack=1e-4;
 float min_coverage=1,max_coverage=0,quantization=0;
 for(const auto *tap:taps){
  min_coverage=std::min(min_coverage,tap->coverage);
  max_coverage=std::max(max_coverage,tap->coverage);
  quantization=std::max(quantization,tap->quantization);
 }
 // With full coverage both texture reads normalize the same filtered RGB.
 if(min_coverage==1)return frame_and_sampling_slack;
 NormalCone original,baked;
 Vec3 maximum=taps[0]->original,minimum=maximum;
 for(const auto *tap:taps){
  original.add(tap->original);
  if(tap->coverage>0)baked.add(tap->baked);
  const auto v=tap->original;
  maximum={std::max(maximum.x,v.x),std::max(maximum.y,v.y),std::max(maximum.z,v.z)};
  minimum={std::min(minimum.x,v.x),std::min(minimum.y,v.y),std::min(minimum.z,v.z)};
 }
 // The original part has an additional product of filtered direction and
 // uncovered fraction. Bound its covariance componentwise, then take its norm.
 const double covariance=.25*(max_coverage-min_coverage)*length(maximum-minimum);
 const double normal_length_low=baked.high==0?original.min_z/original.high:
     std::min(original.min_z/original.high,baked.min_z/baked.high);
 // Leave room for frame and floating-point normalization differences.
 const double delta=quantization+covariance+(1-min_coverage)*original.normalization_error()+
     max_coverage*baked.normalization_error()+frame_and_sampling_slack;
 return normal_length_low>delta?std::asin(delta/normal_length_low):std::acos(-1.);
}

std::shared_ptr<const Image> compose_mtoon_baked_normal(const TextureSetView &field,
    const TextureMaps &maps,const MToonMaterial &effective,double &bound,const TextureSetLimits &limits){
 const auto &normal=maps.baked_normal,&coverage=maps.baked_coverage;
 const auto r=field.painted->resolution();
 normal.validate();coverage.validate();
 if(normal.width!=r||normal.height!=r||coverage.width!=r||coverage.height!=r)
  throw std::logic_error("MToon Bake lost its normal and coverage grids");
 const auto &role=field.material_bindings->roles[size_t(MToonSlot::Normal)];
 const auto &texture=effective.textures[size_t(MToonSlot::Normal)];
 const Image *aligned=nullptr;
 Vec3 original{0,0,1};
 if(role.image&&effective.parameters.normal_enabled){
  role.image->validate();
  bool uniform=true;
  for(size_t at=4;at<role.image->rgba.size();at+=4){
   if((at&16383)==0)stop(limits);
   if(!std::equal(role.image->rgba.begin(),role.image->rgba.begin()+3,role.image->rgba.begin()+at)){
    uniform=false;break;
   }
  }
  if(uniform&&texture.rotation==0&&texture.scale[0]>0&&texture.scale[1]>0){
   const auto *p=role.image->rgba.data();
   original=mtoon_normal_map({p[0]/255.f,p[1]/255.f,p[2]/255.f},effective.parameters.normal_scale);
  }else if(aligned_mtoon_source(role,texture,r))aligned=role.image.get();
  else for(size_t at=0;at<coverage.rgba.size();at+=4){
   if((at&16383)==0)stop(limits);
   if(coverage.rgba[at]!=255)return {};
  }
 }
 constexpr double limit=.02;
 auto output=std::make_shared<Image>(normal);
 std::vector<MToonNormalTap> previous(r),row(r);
 for(size_t y=0;y<r;++y){
  stop(limits);
  for(size_t x=0;x<r;++x){
   const auto i=(y*r+x)*4;
   auto &tap=row[x];
   tap={original,normal_rgb(normal.rgba.data()+i),coverage.rgba[i]/255.f,0};
   if(aligned){
    tap.original=normal_rgb(aligned->rgba.data()+i);
    tap.original.x*=effective.parameters.normal_scale;
    tap.original.y*=effective.parameters.normal_scale;
   }
   if((tap.coverage<1&&tap.original.z<=0)||(tap.coverage>0&&tap.baked.z<=0))return {};
   if(tap.coverage<1){
    const auto value=tap.original*(1-tap.coverage)+tap.baked*tap.coverage;
    const std::array channels{value.x,value.y,value.z};
    for(size_t k=0;k<3;++k)output->rgba[i+k]=uint8_t(std::lround(std::clamp(channels[k]*.5f+.5f,0.f,1.f)*255));
    tap.quantization=length(normal_rgb(output->rgba.data()+i)-value);
   }
   const size_t left=x?x-1:x;
   const auto &above=y?previous:row;
   bound=std::max(bound,mtoon_normal_cell_error({&above[left],&above[x],&row[left],&tap}));
   if(bound>limit)return {};
  }
  previous.swap(row);
 }
 return output;
}

std::shared_ptr<const Image> compose_shared_mtoon_shade(
    const Image &resolved,const Image &coverage,const std::array<float,4> &native,
    const MToonPaintSource &source,Vec3 factor,PortableMToonSetPaint &proof,
    const TextureSetLimits &limits){
 const auto width=resolved.width,height=resolved.height;
 auto image=std::make_shared<Image>(Image{width,height,std::vector<uint8_t>(resolved.rgba.size(),255)});
 const auto &linear=mtoon_linear_bytes();
 const std::array<double,3> strength{factor.x,factor.y,factor.z};
 std::vector<MToonShadeTap> previous(width),current(width);
 for(uint32_t y=0;y<height;++y){
  stop(limits);
  for(uint32_t x=0;x<width;++x){
   const size_t at=(size_t(y)*width+x)*4;
   const auto replacement=coverage.rgba[at];
   const double uncovered=1-replacement/255.;
   auto &tap=current[x];
   const auto original=source.sample(at);
   tap.sampling.uncovered=uncovered;
   for(size_t k=0;k<3;++k){
    const double background=strength[k]*original[k];
    const double divisor=native[k]!=0?native[k]:1;
    const double paint=(linear[resolved.rgba[at+k]]-native[k]*uncovered)/divisor;
    if(paint>std::numeric_limits<float>::max())return {};
    const double correction=strength[k]*paint;
    // The viewport adds just the positive paint contribution to the source
    // shade. At zero coverage it uses the original shade exactly.
    const double continuous=background*uncovered+std::max(0.,correction);
    const auto encoded=mtoon_srgb_byte(replacement?continuous:background);
    image->rgba[at+k]=encoded;
    const auto error=std::abs(linear[encoded]-continuous);
    tap.correction[k]=correction;
    tap.sampling.error[k]=error;
    tap.sampling.background[k]=background;
    proof.shade_error=std::max(proof.shade_error,error);
    if(!replacement)
     proof.shade_error=std::max(proof.shade_error,std::abs(linear[encoded]-background));
   }
   if(!x||!y)continue;
   const std::array taps{&previous[x-1],&previous[x],&current[x-1],&tap};
   for(size_t k=0;k<3;++k){
    double low=0,high=0,quantization=0;
    for(const auto *corner:taps){
     low=std::min(low,corner->correction[k]);
     high=std::max(high,corner->correction[k]);
     quantization=std::max(quantization,corner->sampling.error[k]);
    }
    // Interpolating max(0,t) at the taps can overestimate max(0,t) in
    // the shader. For t in [low,high], the worst gap is the secant at 0.
    const double kink=low<0&&high>0?-low*high/(high-low):0;
    const double product=source.aligned?mtoon_source_filter_error(
        {&taps[0]->sampling,&taps[1]->sampling,&taps[2]->sampling,&taps[3]->sampling},k):0;
    proof.shade_interpolation_error=std::max(proof.shade_interpolation_error,kink);
    proof.source_filter_error=std::max(proof.source_filter_error,product);
    proof.shade_error=std::max(proof.shade_error,quantization+kink+product);
   }
  }
  previous.swap(current);
 }
 return image;
}

enum class MToonAlphaGrid { None, Paint, Source };

std::optional<std::array<uint32_t,2>> independent_mtoon_dimensions(const SourceMaterialBindings &binding,
    MToonSlot role,uint32_t paint_size,MToonAlphaGrid alpha) {
 const auto slot=size_t(role);
 const auto &view=binding.roles[slot];
 if(!clamp_linear_mtoon_source(view,binding.material.textures[slot]))return {};
 if(alpha==MToonAlphaGrid::Source)return std::array{view.image->width,view.image->height};
 if(view.image->width==paint_size&&view.image->height==paint_size)return {};
 if(alpha==MToonAlphaGrid::Paint)return std::array{paint_size,paint_size};
 const auto axis=[&](uint32_t size){
  // An odd multiple keeps every native paint texel center as an output
  // filter knot, while the new raster has at least the source density.
  const auto multiple=(size+paint_size-1)/paint_size;
  return (multiple|1u)*paint_size;
 };
 const std::array result{axis(view.image->width),axis(view.image->height)};
 if(result[0]>8192||result[1]>8192)return {};
 return result;
}

struct MToonFilterAxis {
 uint32_t low,high;
 double fraction;
};
MToonFilterAxis mtoon_filter_axis(double uv,uint32_t count) {
 const auto p=std::clamp(uv*count-.5,0.,double(count-1));
 const auto low=uint32_t(p);
 return {low,std::min(low+1,count-1),p-low};
}

std::array<double,4> mtoon_filter(const Image &image,MToonFilterAxis u,MToonFilterAxis v,
                                  size_t channels,bool srgb) {
 const auto &linear=mtoon_linear_bytes();
 const auto pixel=[&](uint32_t x,uint32_t y) {
  return image.rgba.data()+(size_t(y)*image.width+x)*4;
 };
 const auto *a=pixel(u.low,v.low),*b=pixel(u.high,v.low),*c=pixel(u.low,v.high),*d=pixel(u.high,v.high);
 std::array<double,4> result{};
 for(size_t k=0;k<channels;++k){
  const auto value=[&](uint8_t byte){return srgb&&k<3?linear[byte]:byte/255.;};
  result[k]=std::lerp(std::lerp(value(a[k]),value(b[k]),u.fraction),
                      std::lerp(value(c[k]),value(d[k]),u.fraction),v.fraction);
 }
 return result;
}

struct MToonIndependentTap {
 std::array<double,4> background{}, correction{}, output{};
 double uncovered=0;
};

bool mtoon_alpha_matches_native(const Image &image,float factor,float native,
                              const TextureSetLimits &limits) {
 for(size_t at=3;at<image.rgba.size();at+=4){
  if((at&16383)==3)stop(limits);
  if((image.rgba[at]/255.f)*factor!=native)return false;
 }
 return true;
}

MToonAlphaGrid mtoon_base_alpha_grid(const SourceMaterialBindings &binding,bool opaque,
                                    float factor,float native,const TextureSetLimits &limits) {
 if(opaque)return MToonAlphaGrid::None;
 constexpr auto base=size_t(MToonSlot::Base);
 const auto &role=binding.roles[base];
 if(clamp_linear_mtoon_source(role,binding.material.textures[base])&&
    !mtoon_alpha_matches_native(*role.image,factor,native,limits))return MToonAlphaGrid::Source;
 return MToonAlphaGrid::Paint;
}

bool identical_mtoon_alpha_taps(const Image &a,const Image &b,double u,double v) {
 if(a.width!=b.width||a.height!=b.height)return false;
 const auto x=mtoon_filter_axis(u,a.width),y=mtoon_filter_axis(v,a.height);
 for(const auto j:{y.low,y.high})for(const auto i:{x.low,x.high}){
  const auto index=(size_t(j)*a.width+i)*4+3;
  if(a.rgba[index]!=b.rgba[index])return false;
 }
 return true;
}

bool certify_mtoon_alpha_cell(const std::array<const MToonIndependentTap*,4> &corners,
    const Image &source,const Image &output,double u,double v,float factor,
    const MToonParameters &params,PortableMToonSetPaint &proof) {
 const bool unpainted=std::ranges::all_of(corners,[](const auto *tap){return tap->uncovered==1;});
 // With identical filter taps, a cutout crossing is preserved at every
 // point in this rectangle, including points exactly on the threshold.
 if(unpainted&&factor==proof.alpha_factor&&identical_mtoon_alpha_taps(source,output,u,v))return true;
 double bg_min=1,bg_max=0,u_min=1,u_max=0;
 double wanted_min=1,wanted_max=0,emitted_min=1,emitted_max=0,error=0;
 for(const auto *tap:corners){
  const double background=tap->background[3];
  const double emitted=tap->output[3];
  const double wanted=unpainted?background:background*tap->uncovered+tap->correction[3];
  bg_min=std::min(bg_min,background);bg_max=std::max(bg_max,background);
  u_min=std::min(u_min,tap->uncovered);u_max=std::max(u_max,tap->uncovered);
  emitted_min=std::min(emitted_min,emitted);emitted_max=std::max(emitted_max,emitted);
  wanted_min=std::min(wanted_min,wanted);wanted_max=std::max(wanted_max,wanted);
  error=std::max(error,std::abs(emitted-wanted));
  if(tap->uncovered==1){
   // The zero-coverage branch can also lie on an edge of a mixed cell.
   wanted_min=std::min(wanted_min,background);wanted_max=std::max(wanted_max,background);
   error=std::max(error,std::abs(emitted-background));
  }
 }
 const double covariance=(bg_max-bg_min)*(u_max-u_min)*.25;
 proof.alpha_error=std::max(proof.alpha_error,error+covariance);
 proof.source_filter_error=std::max(proof.source_filter_error,covariance);
 if(proof.alpha_error>1./255.)return false;
 constexpr double sampling_slack=1e-6;
 const double low=std::min(wanted_min-covariance,emitted_min)-sampling_slack;
 const double high=std::max(wanted_max+covariance,emitted_max)+sampling_slack;
 // The native fragment clamps alpha. For any other MASK threshold the
 // complete native and portable ranges must stay on the same side.
 if(params.alpha==MToonAlpha::Mask)
  return params.alpha_cutoff==0||low>=params.alpha_cutoff||high<params.alpha_cutoff;
 return low>0;
}

std::vector<double> mtoon_filter_knots(uint32_t paint,uint32_t original,uint32_t output) {
 std::vector<double> knots;
 knots.reserve(size_t(paint)+original+output+2);
 knots.push_back(0);
 knots.push_back(1);
 for(const auto count:{paint,original,output})
  for(uint32_t k=0;k<count;++k)knots.push_back((k+.5)/count);
 std::ranges::sort(knots);
 knots.erase(std::unique(knots.begin(),knots.end()),knots.end());
 return knots;
}

std::shared_ptr<const Image> compose_independent_mtoon(MToonSlot role,const Image &source,
    const Image &resolved,const Image &coverage,std::array<uint32_t,2> output,
    const std::array<float,4> &factor,const std::array<float,4> &native,
    const MToonParameters &params,MToonAlphaGrid alpha,PortableMToonSetPaint &proof,
    const TextureSetLimits &limits) {
 source.validate();resolved.validate();coverage.validate();
 if(resolved.width!=coverage.width||resolved.height!=coverage.height)
  throw std::logic_error("MToon paint and coverage dimensions differ");
 const bool shade=role==MToonSlot::Shade, base=role==MToonSlot::Base;
 const size_t channels=alpha==MToonAlphaGrid::None?3:4;
 const auto [width,height]=output;
 if(alpha==MToonAlphaGrid::Paint){
  // Preserve the existing filtered alpha curve exactly on its paint grid.
  // Covered taps can then interpolate across a mask threshold as before.
  if(width!=resolved.width||height!=resolved.height)return {};
 }
 auto image=std::make_shared<Image>(Image{width,height,std::vector<uint8_t>(size_t(width)*height*4,255)});
 const auto sample=[&](double u,double v) {
  MToonIndependentTap tap;
  const auto pu=mtoon_filter_axis(u,resolved.width),pv=mtoon_filter_axis(v,resolved.height);
  const auto su=mtoon_filter_axis(u,source.width),sv=mtoon_filter_axis(v,source.height);
  const auto paint=mtoon_filter(resolved,pu,pv,channels,true);
  tap.uncovered=1-mtoon_filter(coverage,pu,pv,1,false)[0];
  tap.background=mtoon_filter(source,su,sv,channels,true);
  for(size_t k=0;k<channels;++k){
   tap.background[k]*=factor[k];
   tap.correction[k]=shade?factor[k]*(paint[k]-native[k]*tap.uncovered)/(native[k]!=0?native[k]:1):
                           paint[k]-native[k]*tap.uncovered;
  }
  return tap;
 };
 const auto value=[&](const MToonIndependentTap &tap,size_t k){
  return tap.background[k]*tap.uncovered+
     (shade?std::max(0.,tap.correction[k]):tap.correction[k]);
 };
 for(uint32_t y=0;y<height;++y){
  stop(limits);
  for(uint32_t x=0;x<width;++x){
   const auto tap=sample((x+.5)/width,(y+.5)/height);
   const size_t at=(size_t(y)*width+x)*4;
   for(size_t k=0;k<3;++k)
    image->rgba[at+k]=mtoon_srgb_byte(tap.uncovered==1?tap.background[k]:value(tap,k));
   if(alpha==MToonAlphaGrid::Paint)image->rgba[at+3]=resolved.rgba[at+3];
   if(alpha==MToonAlphaGrid::Source){
    const auto sampled=tap.uncovered==1?tap.background[3]:value(tap,3);
    image->rgba[at+3]=uint8_t(std::lround(255*std::clamp(sampled/proof.alpha_factor,0.,1.)));
   }
  }
 }
 const auto xs=mtoon_filter_knots(resolved.width,source.width,width);
 const auto ys=mtoon_filter_knots(resolved.height,source.height,height);
 std::vector<MToonIndependentTap> previous(xs.size()),row(xs.size());
 double &rgb_error=shade?proof.shade_error:base?proof.color_error:proof.emission_error;
 const auto alpha_pass=[&](double value){
  return params.alpha==MToonAlpha::Mask?value>=params.alpha_cutoff:value>0;
 };
 for(size_t y=0;y<ys.size();++y){
  stop(limits);
  for(size_t x=0;x<xs.size();++x){
   auto &tap=row[x];
   tap=sample(xs[x],ys[y]);
   tap.output=mtoon_filter(*image,mtoon_filter_axis(xs[x],width),
                           mtoon_filter_axis(ys[y],height),channels,true);
   if(alpha==MToonAlphaGrid::Source)tap.output[3]*=proof.alpha_factor;
   if(!x||!y)continue;
   const std::array<const MToonIndependentTap*,4> corners{&previous[x-1],&previous[x],&row[x-1],&tap};
   if(alpha==MToonAlphaGrid::Source&&
      !certify_mtoon_alpha_cell(corners,source,*image,std::midpoint(xs[x-1],xs[x]),
                               std::midpoint(ys[y-1],ys[y]),factor[3],params,proof))
    return {};
   // Break at every filter knot of all three grids. Inside each resulting
   // rectangle both input filters and the output filter are bilinear. The
   // product adds covariance and shared shade adds a possible max(0,x) kink.
   for(size_t k=0;k<(alpha==MToonAlphaGrid::Paint?4:3);++k){
    double bg_low=1,bg_high=0,u_low=1,u_high=0,correction_low=0,correction_high=0,error=0;
    for(const auto *corner:corners){
     bg_low=std::min(bg_low,corner->background[k]);bg_high=std::max(bg_high,corner->background[k]);
     u_low=std::min(u_low,corner->uncovered);u_high=std::max(u_high,corner->uncovered);
     correction_low=std::min(correction_low,corner->correction[k]);
     correction_high=std::max(correction_high,corner->correction[k]);
     error=std::max(error,std::abs(corner->output[k]-value(*corner,k)));
     if(corner->uncovered==1){
      error=std::max(error,std::abs(corner->output[k]-corner->background[k]));
      if(k==3&&alpha_pass(corner->output[k])!=alpha_pass(corner->background[k]))return {};
     }
    }
    const double covariance=(bg_high-bg_low)*(u_high-u_low)*.25;
    const double kink=shade&&correction_low<0&&correction_high>0?
        -correction_low*correction_high/(correction_high-correction_low):0;
    proof.source_filter_error=std::max(proof.source_filter_error,covariance);
    proof.shade_interpolation_error=std::max(proof.shade_interpolation_error,kink);
    if(k==3)proof.alpha_error=std::max(proof.alpha_error,error+covariance);
    else rgb_error=std::max(rgb_error,error+covariance+kink);
   }
   if(rgb_error>.005||proof.alpha_error>1./255.)return {};
  }
  previous.swap(row);
 }
 return image;
}

bool native_material_grid(const TextureSetView &field) {
 return !field.source&&!field.appearance&&!field.material_bindings&&native_hdr_grid(*field.painted);
}

bool baked_mtoon_pbr_fallback(const TextureSetView &field) {
 return !field.source&&native_texture_set_mtoon(field.painted.get(),field.appearance.get(),field.material_bindings.get())&&
     !field.material_bindings->physical_emission&&field.painted->baked()&&
     field.painted->baked()->emission_field&&native_hdr_grid(*field.painted);
}

bool native_mtoon_float_bake(const TextureSetView &field) {
 return baked_mtoon_pbr_fallback(field)&&!field.material.unlit&&
     field.material_bindings->material.version()==MToonVersion::V1&&
     !field.painted->baked()->maps.normal.rgba.empty();
}

// Padding refers to the actual writable owner texel, which can be in a
// different tile from the display texel. Preserve native layer order there.
class SparseExportGrid {
public:
 SparseExportGrid(const PaintedMesh &paint,size_t &work,const TextureSetLimits &limits)
     :padding(EmissionExportAccess::writable_padding(paint)),resolution(paint.resolution()),
      columns(resolution/PaintTile::size),tile_layers(size_t(columns)*columns) {
  const auto &layers=paint.layers();
  charge(work,tile_layers.size(),layers.size()+1,limits.maximum_work);
  for(size_t i=0;i<layers.size();++i){
   stop(limits);
   const auto &layer=layers[i];
   if(!layer.visible||layer.opacity<=0)continue;
   for(size_t tile=0;tile<tile_layers.size();++tile)
    if(layer.tiles[tile])tile_layers[tile]|=uint16_t(1u<<i);
  }
 }
 size_t tile(size_t owner) const {
  return (owner/resolution/PaintTile::size)*columns+owner%resolution/PaintTile::size;
 }
 size_t within(size_t owner) const {
  return (owner/resolution%PaintTile::size)*PaintTile::size+owner%PaintTile::size;
 }
 uint16_t mask(size_t at) const {
  const auto owner=padding[at];
  return owner<0?0:tile_layers[tile(size_t(owner))];
 }
 std::span<const int32_t> padding;
 uint32_t resolution,columns;
 std::vector<uint16_t> tile_layers;
};

void charge_native_grid(const PaintedMesh &paint,const SparseExportGrid &grid,size_t &work,
    const TextureSetLimits &limits,TextureSetGridMode mode) {
 const auto count=grid.padding.size();
 const auto &baked=paint.baked();
 const bool normal=mode==TextureSetGridMode::NativeHdr&&baked&&!baked->maps.normal.rgba.empty();
 const bool typed=mode==TextureSetGridMode::NativeMToonHdr;
 const bool legacy_emission=mode==TextureSetGridMode::RetainedLdr;
 charge(work,count,legacy_emission||typed?7:normal?4:3,limits.maximum_work);
 for(size_t at=0;at<count;++at){
  if((at&4095)==0)stop(limits);
  const auto owner=grid.padding[at];
  if(baked&&owner>=0&&baked->coverage[size_t(owner)])
   charge(work,1,legacy_emission||typed?14:normal?16:10,limits.maximum_work);
  charge(work,std::popcount(grid.mask(at)),legacy_emission?18:typed?14:12,limits.maximum_work);
 }
}
std::array<bool,2> legacy_coverage(const PaintedMesh &paint,size_t &work,const TextureSetLimits &limits) {
 if(const auto &bake=paint.baked()){
  for(size_t at=0;at<bake->coverage.size();++at){
   if((at&4095)==0)stop(limits);
   charge(work,1,1,limits.maximum_work);
   if(bake->coverage[at])return {true,true};
  }
 }
 std::array<bool,2> covered{};
 for(const auto &layer:paint.layers()){
  if(!layer.visible||layer.opacity<=0)continue;
  for(const auto &tile:layer.tiles){
   stop(limits);
   if(!tile)continue;
   for(const auto &pixel:tile->pixels){
    charge(work,1,1,limits.maximum_work);
    if(pixel.data[13]){
     covered[0]|=pixel.data[9]!=0;
     covered[1]|=pixel.data[12]!=0;
     if(covered[0]&&covered[1])return covered;
    }
   }
  }
 }
 return covered;
}
bool plain_color_roles(const PaintedMesh &paint) noexcept {
 const auto &baked=paint.baked();
 return !paint.source_maps()&&!paint.color_projection()&&!paint.normal_projection()&&
     !paint.source_sampling_prepared()&&
     (!baked||baked->color_interpretation==BakedColorInterpretation::VertexComponent);
}
bool retained_color_grid(const PaintedMesh &paint) noexcept {
 return plain_color_roles(paint)&&paint.paint_sampling_domain()&&
     paint.paint_sampling_domain()->version()==3&&bool(paint.paint_sampling_attachment());
}
} // namespace

bool native_hdr_grid(const PaintedMesh &paint) noexcept {
 const auto &baked=paint.baked();
 return plain_color_roles(paint)&&!paint.paint_sampling_domain()&&!paint.paint_sampling_attachment()&&
     paint.surface_diagnostics().policy==PaintUvPolicy::Strict&&
     (!baked||(!baked->normal_field&&!baked->normal_authority&&
       baked->normal_interpretation==BakedNormalInterpretation::CanonicalMikk));
}

bool retained_material_grid(const TextureSetView &field) noexcept {
 const auto &binding=field.material_bindings;
 return field.source&&field.source->source_preview()&&binding&&binding->polygon_surface_transfer&&
     !binding->has_mtoon&&!field.material.toon&&!field.appearance&&field.painted&&retained_color_grid(*field.painted);
}

TextureSetGridMode retained_grid_mode(const TextureSetView &field) noexcept {
 const auto &binding=field.material_bindings;
 const auto &bake=field.painted->baked();
 if((bake&&bake->emission_field)||binding->physical_emission||
    hdr_emission(binding->native_reference.emission)||hdr_emission(field.material.emissive))
  return TextureSetGridMode::RetainedHdr;
 for(const auto &layer:field.painted->layers())
  if(layer.visible&&layer.opacity>0&&std::any_of(layer.emission_tiles.begin(),layer.emission_tiles.end(),
        [](const auto &tile){return bool(tile);}))return TextureSetGridMode::RetainedHdr;
 return TextureSetGridMode::RetainedLdr;
}

bool retained_hdr_grid(const TextureSetView &field) noexcept {
 return retained_material_grid(field)&&retained_grid_mode(field)==TextureSetGridMode::RetainedHdr;
}

size_t retained_hdr_normal_bytes(const TextureSetView &field) noexcept {
 const auto &preview=field.source->source_preview();
 if(preview->native_bake())return preview->native_bake()->rgba.size();
 const auto &baked=field.painted->baked();
 return baked&&baked->normal_field?
     baked->normal_field->pbr()->rgba.size()+baked->normal_field->mtoon()->rgba.size():0;
}

size_t native_material_grid_bytes(const PaintedMesh &paint,TextureSetGridMode mode) noexcept {
 const auto r=paint.resolution();
 const auto &baked=paint.baked();
 const bool normal=mode==TextureSetGridMode::NativeHdr&&baked&&!baked->maps.normal.rgba.empty();
 const auto channels=mode==TextureSetGridMode::NativeMToonHdr?20:
     mode==TextureSetGridMode::RetainedLdr?20:normal?12:8;
 return sizeof(TextureMaps)+size_t(r)*r*channels+size_t(r/PaintTile::size)*(r/PaintTile::size)*sizeof(uint16_t);
}

void charge_native_material_grid_work(const PaintedMesh &paint,size_t &work,const TextureSetLimits &limits,
    TextureSetGridMode mode) {
 const SparseExportGrid grid(paint,work,limits);
 charge_native_grid(paint,grid,work,limits,mode);
 if(mode==TextureSetGridMode::RetainedLdr)(void)legacy_coverage(paint,work,limits);
}

std::shared_ptr<TextureMaps> compile_native_material_grid(const PaintedMesh &paint,const Material &material,
    size_t &work,const TextureSetLimits &limits,TextureSetGridMode mode) {
 const bool typed=mode==TextureSetGridMode::NativeMToonHdr;
 const auto &baked=paint.baked();
 if(!((mode==TextureSetGridMode::NativeHdr||typed)?native_hdr_grid(paint):retained_color_grid(paint))||
    (typed&&(!baked||baked->maps.normal.rgba.empty())))
  throw std::invalid_argument("Material grid compilation needs actual colour and normal owners");
 if(native_material_grid_bytes(paint,mode)>limits.maximum_bytes)
  throw std::length_error("Native material fields exceed their working budget");
 const SparseExportGrid grid(paint,work,limits);
 charge_native_grid(paint,grid,work,limits,mode);
 const auto count=grid.padding.size();
 const bool normal=mode==TextureSetGridMode::NativeHdr&&baked&&!baked->maps.normal.rgba.empty();
 const bool legacy_emission=mode==TextureSetGridMode::RetainedLdr;
 auto maps=std::make_shared<TextureMaps>();
 auto &albedo=maps->color,&orm=maps->metal_rough;
 for(auto *image:{&albedo,&orm}){
  image->width=image->height=grid.resolution;
  image->rgba.resize(count*4);
 }
 if(normal)maps->normal={grid.resolution,grid.resolution,std::vector<uint8_t>(count*4)};
 if(typed){
  maps->baked_base=baked;
  for(auto *image:{&maps->color_coverage,&maps->baked_normal,&maps->baked_coverage})
   *image={grid.resolution,grid.resolution,std::vector<uint8_t>(count*4)};
 }
 const auto covered=legacy_emission?legacy_coverage(paint,work,limits):std::array<bool,2>{};
 if(legacy_emission){
  maps->emission={grid.resolution,grid.resolution,std::vector<uint8_t>(count*4)};
  const auto make_coverage=[&](bool active){
   if(active)return Image{grid.resolution,grid.resolution,std::vector<uint8_t>(count*4)};
   return Image{1,1,{0,0,0,255}};
  };
  maps->color_coverage=make_coverage(covered[0]);
  maps->emission_coverage=make_coverage(covered[1]);
 }
 const auto unit=[](uint8_t value){return float(value)/255;};
 const auto byte=[](float value){return uint8_t(std::lround(std::clamp(value,0.f,1.f)*255));};
 static const auto linear_byte=[]{
  std::array<float,256> table{};
  for(size_t i=0;i<table.size();++i)table[i]=srgb_to_linear(float(i)/255);
  return table;
 }();
 const auto encode_color=[&](const std::array<float,4> &color){
  return std::array{byte(linear_to_srgb(color[0])),byte(linear_to_srgb(color[1])),
      byte(linear_to_srgb(color[2])),byte(color[3])};
 };
 const auto base_color=encode_color(material.color);
 const std::array<uint8_t,4> base_orm{255,byte(material.roughness),byte(material.metallic),255};
 const auto &layers=paint.layers();
 for(size_t at=0;at<count;++at){
  if((at&4095)==0)stop(limits);
  auto mask=grid.mask(at);
  auto color=base_color,surface=base_orm;
  std::array<uint8_t,4> normal_sample{128,128,255,255};
  float alpha=material.color[3];
  const auto owner=grid.padding[at];
  const float baked_coverage=baked&&owner>=0?unit(baked->coverage[size_t(owner)]):0.f;
  auto emission=material.emissive;
  float color_coverage=baked_coverage,emission_coverage=baked_coverage;
  if(mask||baked_coverage>0){
   auto linear=material.color;
   std::array<float,3> material_values{1,material.roughness,material.metallic};
   const auto bake_mix=[&](float base,float value){return base*(1-baked_coverage)+value*baked_coverage;};
   if(baked_coverage>0){
    const size_t offset=size_t(owner)*4;
    const auto *baked_color=baked->maps.color.rgba.data()+offset;
    const auto *baked_orm=baked->maps.metal_rough.rgba.data()+offset;
    for(size_t i=0;i<4;++i)linear[i]=bake_mix(linear[i],i<3?linear_byte[baked_color[i]]:unit(baked_color[i]));
    for(size_t i=0;i<3;++i)material_values[i]=bake_mix(material_values[i],unit(baked_orm[i]));
    if(legacy_emission){
     const auto *baked_emission=baked->maps.emission.rgba.data()+offset;
     for(size_t i=0;i<3;++i)emission[i]=bake_mix(emission[i],linear_byte[baked_emission[i]]);
    }
    if(typed)std::copy_n(baked->maps.normal.rgba.data()+offset,4,maps->baked_normal.rgba.data()+at*4);
    if(normal){
     const auto *detail=baked->maps.normal.rgba.data()+offset;
     Vec3 direction{unit(detail[0])*2-1,unit(detail[1])*2-1,unit(detail[2])*2-1};
     direction=direction*baked_coverage+Vec3{0,0,1}*(1-baked_coverage);
     direction=length(direction)<1e-8f?Vec3{0,0,1}:normalized(direction);
     normal_sample={byte(direction.x*.5f+.5f),byte(direction.y*.5f+.5f),byte(direction.z*.5f+.5f),255};
    }
   }
   const size_t tile=grid.tile(size_t(owner)),within=grid.within(size_t(owner));
   for(;mask;mask&=mask-1){
    const auto &layer=layers[std::countr_zero(mask)];
    const auto &data=layer.tiles[tile]->pixels[within].data;
    const float weight=unit(data[13])*layer.opacity;
    const auto blend=[&](float base,float paint,uint8_t coverage){
     const float a=weight*unit(coverage);
     return base*(1-a)+paint*a;
    };
    for(size_t i=0;i<4;++i)linear[i]=blend(linear[i],i<3?linear_byte[data[i]]:unit(data[i]),data[9]);
    material_values[1]=blend(material_values[1],unit(data[5]),data[11]);
    material_values[2]=blend(material_values[2],unit(data[4]),data[10]);
    if(typed||legacy_emission){
     const float color_weight=weight*unit(data[9]);
     color_coverage=color_coverage*(1-color_weight)+color_weight;
    }
    if(legacy_emission){
     const float emission_weight=weight*unit(data[12]);
     emission_coverage=emission_coverage*(1-emission_weight)+emission_weight;
     for(size_t i=0;i<3;++i)emission[i]=blend(emission[i],linear_byte[data[6+i]],data[12]);
    }
   }
   color=encode_color(linear);
   alpha=linear[3];
   for(size_t i=0;i<3;++i)surface[i]=byte(material_values[i]);
  }
  maps->transparent|=alpha<.999f;
  std::copy(color.begin(),color.end(),albedo.rgba.data()+at*4);
  std::copy(surface.begin(),surface.end(),orm.rgba.data()+at*4);
  if(normal)std::copy(normal_sample.begin(),normal_sample.end(),maps->normal.rgba.data()+at*4);
  if(typed){
   maps->color_coverage.rgba[at*4]=byte(color_coverage);
   maps->color_coverage.rgba[at*4+3]=255;
   maps->baked_coverage.rgba[at*4]=byte(baked_coverage);
   maps->baked_coverage.rgba[at*4+3]=255;
  }
  if(legacy_emission){
   for(size_t i=0;i<3;++i)maps->emission.rgba[at*4+i]=byte(linear_to_srgb(emission[i]));
   maps->emission.rgba[at*4+3]=255;
   if(covered[0]){
    maps->color_coverage.rgba[at*4]=byte(color_coverage);
    maps->color_coverage.rgba[at*4+3]=255;
   }
   if(covered[1]){
    maps->emission_coverage.rgba[at*4]=byte(emission_coverage);
    maps->emission_coverage.rgba[at*4+3]=255;
   }
  }
 }
 return maps;
}

namespace {
std::optional<BakedEmissionSample> shared_baked_emission(const BakedTextures &bake,size_t &work,
    const TextureSetLimits &limits) {
 if(bake.emission_field){
  const auto storage=bake.emission_field->storage();
  if(storage.dense.empty()&&!storage.varying_mask&&storage.binary_coverage.empty())
   return bake.emission_field->texel(0);
  return {};
 }
 const auto *rgba=bake.maps.emission.rgba.data();
 const auto coverage=bake.coverage.front();
 charge(work,bake.coverage.size(),1,limits.maximum_work);
 for(size_t at=0;at<bake.coverage.size();++at){
  if((at&4095)==0)stop(limits);
  if(bake.coverage[at]!=coverage||!std::equal(rgba,rgba+3,rgba+at*4))return {};
 }
 return baked_emission_texel(bake,0);
}

// The per-field exporter uses the same paint-grid coordinate as the fragment
// shader. Contributions are composed before sampling. The original field and
// its exact FLOAT values remain in the author's native Scene payload.
class TextureSetEmissionSamples {
public:
 enum class SampleKind : size_t { Background, SharedBake, Varying };
 TextureSetEmissionSamples(const TextureSetView &field,size_t &work,size_t passes,
     const TextureSetLimits &limits,bool authored_mtoon)
     :paint_(texture_set_source_read_view(field,limits)),
      sampler_(*paint_,limits.cancelled),resolution(paint_->resolution()) {
  const auto &binding=field.material_bindings;
  background_=binding&&binding->physical_emission?*binding->physical_emission:field.material.emissive;
  if(const auto maps=paint_->source_maps();maps&&(!binding||!binding->physical_emission))
   source_=maps->emission;
  if(authored_mtoon){
   if(!native_mtoon_float_bake(field))
    throw std::invalid_argument("Typed FLOAT Bake requires its native MToon 1.0 paint grid");
   const auto &typed=binding->material;
   const auto &p=typed.parameters;
   if(p.scroll!=std::array<float,2>{}||p.rotation_speed!=0)
    throw MToonFloatUnavailable("Typed FLOAT Bake needs static texture coordinates");
   if(const auto &image=binding->roles[size_t(MToonSlot::Emission)].image)
    charge(work,size_t(image->width)*image->height,1,limits.maximum_work);
   typed_source_=mtoon_paint_source(*binding,typed,MToonSlot::Emission,resolution,limits);
   if(!typed_source_)
    throw MToonFloatUnavailable("Typed FLOAT Bake needs a constant or aligned clamp-linear emission source");
   legacy_strength_=p.emission_strength;
   const std::array factor{p.emission.x,p.emission.y,p.emission.z};
   for(size_t k=0;k<3;++k){
    background_[k]=mtoon_factor(factor[k],binding->native_reference.emission[k],field.material.emissive[k])*legacy_strength_;
    if(!std::isfinite(background_[k]))
     throw std::overflow_error("Typed FLOAT Bake background exceeds the radiance range");
   }
  }
  if(source_.image&&source_.texcoord!=0)
   throw std::invalid_argument("Texture-set HDR requires its actual alternate emission coordinates");
  charge(work,sampler_.index_work(),1,limits.maximum_work);
  const size_t count=size_t(resolution)*resolution;
  size_t variable_count=count;
  if(native_material_grid(field)||retained_hdr_grid(field)||baked_mtoon_pbr_fallback(field))
   grid_.emplace(*paint_,work,limits);
  if(grid_&&!varying_typed_source()){
   constants_[0]=coefficients({},background(0,0));
   if(const auto &baked=paint_->baked())
    if(const auto sample=shared_baked_emission(*baked,work,limits)){
     constants_[1]=coefficients(*sample,background(0,0));
     shared_bake_=true;
    }
   charge(work,count,1+passes,limits.maximum_work);
   size_t baked_only=0;
   for(size_t at=0;at<count;++at){
    if((at&4095)==0)stop(limits);
    const auto kind=sample_kind(at);
    if(kind!=SampleKind::Varying)++constant_counts_[size_t(kind)];
    else if(!grid_->mask(at))++baked_only;
   }
   charge(work,baked_only,passes*14,limits.maximum_work);
   variable_count-=constant_counts_[0]+constant_counts_[1]+baked_only;
  }
  charge(work,variable_count,passes*(sampler_.work_per_texel()+12+(source_.image||varying_typed_source()?16:0)),
      limits.maximum_work);
 }
 SampleKind sample_kind(size_t at) const {
  if(!grid_||varying_typed_source()||grid_->mask(at))return SampleKind::Varying;
  if(paint_->baked()&&grid_->padding[at]>=0)
   return shared_bake_?SampleKind::SharedBake:SampleKind::Varying;
  return SampleKind::Background;
 }
 const auto &constant_samples() const { return constants_; }
 const auto &constant_counts() const { return constant_counts_; }
 size_t working_bytes() const {
  return sizeof(*this)+(grid_?grid_->tile_layers.capacity()*sizeof(uint16_t):0);
 }
 bool varying_typed_source() const { return typed_source_&&typed_source_->aligned; }
 std::array<float,3> background(uint32_t x,uint32_t y) const {
  auto value=background_;
  if(typed_source_){
   const auto sample=typed_source_->sample((size_t(y)*resolution+x)*4);
   for(size_t k=0;k<3;++k)value[k]*=static_cast<float>(sample[k]);
  }else if(source_.image){
   const auto sample=sample_source_texture(source_,(float(x)+.5f)/resolution,(float(y)+.5f)/resolution,true);
   for(size_t k=0;k<3;++k)value[k]*=sample[k];
  }
  return value;
 }
 EmissionExrPixel sample_variable(uint32_t x,uint32_t y) const {
  const size_t at=size_t(y)*resolution+x;
  const auto contribution=grid_&&grid_->padding[at]>=0&&paint_->baked()&&!grid_->mask(at)?
      baked_emission_texel(*paint_->baked(),size_t(grid_->padding[at])):sampler_.sample_texel(x,y);
  return coefficients(contribution,background(x,y));
 }
 EmissionExrPixel pixel(uint32_t x,uint32_t y)const{
  const auto kind=sample_kind(size_t(y)*resolution+x);
  return kind==SampleKind::Varying?sample_variable(x,y):constants_[size_t(kind)];
 }
private:
 EmissionExrPixel coefficients(const BakedEmissionSample &q,std::array<float,3> background) const {
  const auto rgb=q.resolve(background,legacy_strength_);
  return {rgb[0],rgb[1],rgb[2],q.legacy[0],q.legacy[1],q.legacy[2],
      q.absolute[0],q.absolute[1],q.absolute[2],q.coverage};
 }
 std::shared_ptr<const PaintedMesh> paint_;
 PaintEmissionSampler sampler_;
 std::array<float,3> background_{};
 PaintSourceTexture source_;
 std::optional<MToonPaintSource> typed_source_;
 float legacy_strength_=1;
 std::optional<SparseExportGrid> grid_;
 std::array<EmissionExrPixel,2> constants_{};
 std::array<size_t,2> constant_counts_{};
 bool shared_bake_=false;
public:
 uint32_t resolution;
};
TextureSetExportEmission compile_emission(const TextureSetView &field,size_t &work,
    const TextureSetLimits &limits,bool authored_mtoon) {
 const TextureSetEmissionSamples samples(field,work,2,limits,authored_mtoon);
 const auto resolution=samples.resolution;
 const auto count=size_t(resolution)*resolution;
 std::array<float,3> peak{};
 const auto &constant_counts=samples.constant_counts();
 const auto &constants=samples.constant_samples();
 using Kind=TextureSetEmissionSamples::SampleKind;
 const auto row_size=samples.varying_typed_source()?resolution:0;
 std::vector<MToonSourceTap> previous(row_size),row(row_size);
 double source_filter_error=0;
 charge(work,size_t(row_size)*resolution,18,limits.maximum_work);
 for(size_t value=0;value<constants.size();++value)if(constant_counts[value])
  for(size_t k=0;k<3;++k)peak[k]=std::max(peak[k],constants[value][k]);
 for(uint32_t y=0;y<resolution;++y){
  stop(limits);
  for(uint32_t x=0;x<resolution;++x){
   if(samples.sample_kind(size_t(y)*resolution+x)!=Kind::Varying)continue;
   const auto rgb=samples.sample_variable(x,y);
   for(size_t k=0;k<3;++k)peak[k]=std::max(peak[k],rgb[k]);
   if(!row_size)continue;
   const auto background=samples.background(x,y);
   auto &tap=row[x];
   for(size_t k=0;k<3;++k)tap.background[k]=background[k];
   tap.uncovered=1.-rgb[9];
   if(!x||!y)continue;
   const std::array<const MToonSourceTap *,4> taps{&previous[x-1],&previous[x],&row[x-1],&tap};
   for(size_t k=0;k<3;++k)source_filter_error=std::max(source_filter_error,mtoon_source_filter_error(taps,k));
  }
  previous.swap(row);
 }
 TextureSetExportEmission out;
 const float scale=std::max({1.f,peak[0],peak[1],peak[2]});
 out.strength=field.material.unlit?1.f:scale;
 for(size_t k=0;k<3;++k)out.factor[k]=peak[k]/scale;
 auto &image=out.image;
 image.width=image.height=resolution;
 image.rgba.resize(count*4);
 const auto encode=[&](const EmissionExrPixel &rgb){
   std::array<uint8_t,4> pixel{0,0,0,255};
   for(size_t k=0;k<3;++k){
    const float linear=peak[k]>0?rgb[k]/peak[k]:0.f;
    const float srgb=linear<=.0031308f?linear*12.92f:1.055f*std::pow(linear,1.f/2.4f)-.055f;
    const auto encoded=uint8_t(std::clamp(std::lround(srgb*255),0l,255l));
    pixel[k]=encoded;
    out.measured.sample(rgb[k],resolved_fallback_radiance(encoded,out.factor[k],out.strength,field.material.unlit));
   }
   return pixel;
 };
 std::array<std::array<uint8_t,4>,2> encoded_constants{};
 for(size_t value=0;value<constants.size();++value)if(constant_counts[value]){
  const auto prior=out.measured.erased_nonzero_samples;
  encoded_constants[value]=encode(constants[value]);
  const auto erased=out.measured.erased_nonzero_samples-prior;
  out.measured.erased_nonzero_samples=prior+erased*constant_counts[value];
 }
 for(uint32_t y=0;y<resolution;++y){
  stop(limits);
  for(uint32_t x=0;x<resolution;++x){
   const size_t at=size_t(y)*resolution+x;
   const auto kind=samples.sample_kind(at);
   const auto pixel=kind==Kind::Varying?encode(samples.sample_variable(x,y)):encoded_constants[size_t(kind)];
   std::copy(pixel.begin(),pixel.end(),image.rgba.data()+at*4);
  }
 }
 if(authored_mtoon){
  // The FLOAT planes filter linearly. Only the product of uncovered
  // fraction and original emission contributes a non-linear term.
  const double filter_error=out.measured.maximum_absolute_error+source_filter_error+1e-6*scale;
  if(filter_error>.005*scale)
   throw MToonFloatUnavailable("Typed FLOAT Bake emission exceeds its portable filter bound");
  out.filter_error=filter_error;
 }
 return out;
}

void charge_mtoon_paint(const TextureSetView &field,size_t &bytes,size_t &work,
    const TextureSetLimits &limits,bool typed_hdr){
 const auto resolution=field.painted->resolution();
 const size_t pixels=size_t(resolution)*resolution;
 charge(bytes,pixels,8,limits.maximum_bytes);
 charge(work,pixels,12,limits.maximum_work);
 const auto &typed=field.material_bindings->material;
 if(typed.version()==MToonVersion::V1&&typed.parameters.scroll==std::array<float,2>{}&&
    typed.parameters.rotation_speed==0&&!field.painted->source_maps()&&
    mtoon_shared_shade(*field.material_bindings)){
  charge(bytes,pixels,4,limits.maximum_bytes);
  charge(bytes,resolution,2*sizeof(MToonShadeTap),limits.maximum_bytes);
  charge(work,pixels,24,limits.maximum_work);
 }
 if(field.painted->baked()&&!field.painted->baked()->maps.normal.rgba.empty()){
  charge(bytes,pixels,4,limits.maximum_bytes);
  charge(bytes,resolution,2*sizeof(MToonNormalTap),limits.maximum_bytes);
  charge(work,pixels,42,limits.maximum_work);
  if(const auto &image=field.material_bindings->roles[size_t(MToonSlot::Normal)].image)
   charge(work,size_t(image->width)*image->height,1,limits.maximum_work);
 }
 for(auto role:{MToonSlot::Base,MToonSlot::Emission})
  if(const auto &image=field.material_bindings->roles[size_t(role)].image){
   const bool float_emission=typed_hdr&&role==MToonSlot::Emission;
   if(!float_emission)charge(work,size_t(image->width)*image->height,1,limits.maximum_work);
   if(aligned_mtoon_source(field.material_bindings->roles[size_t(role)],typed.textures[size_t(role)],resolution)){
    charge(bytes,resolution,2*sizeof(MToonSourceTap),limits.maximum_bytes);
    if(!float_emission)charge(work,pixels,18,limits.maximum_work);
   }
  }
}

struct ExportBound { size_t bytes, work; bool independent_mtoon, scaled_mtoon_alpha; };
ExportBound export_bound(const ObjectSurface&input,const TextureSetLimits&limits,bool measured_physical,bool editable_materials){
 limits.validate();stop(limits);const auto&plan=input.texture_set_surface;
 if(!plan)throw std::invalid_argument("Texture-set export requires its owning surface plan");plan->authenticate_owner(plan);
 if(input.id!=plan->object()||!owner(input.deformation,plan->deformation())||plan->fields().empty()||!material(input.material,plan->fields()[0].material)||
    !input.mesh.positions.empty()||!input.mesh.normals.empty()||!input.mesh.indices.empty()||!input.mesh.uv.empty()||!input.mesh.uv_pins.empty()||!input.mesh.uv_seams.empty()||input.textures||input.appearance||input.material_bindings||input.source_dormant||input.polygon_colors||input.paint_sampling_attachment||!input.paint_corner_coordinates.empty()||input.normal_preview_proof)
  throw std::invalid_argument("Texture-set export bundle differs from its genuine geometry/material/pose owner");
 const auto&rest=*plan->geometry();plan->authenticate_rest(rest,input.deformation,limits);rest.validate();
 size_t bytes=sizeof(TextureSetExport)+plan->preparation_bytes(),work=plan->preparation_work();const auto count=plan->indices().size();
 charge(bytes,count,2*sizeof(Vec3)+sizeof(std::array<float,2>)+sizeof(Tangent)+3*sizeof(uint32_t)+1,limits.maximum_bytes);
 charge(bytes,count,10*sizeof(void*)+12*sizeof(uint32_t),limits.maximum_bytes);
 if(std::ranges::any_of(plan->fields(),[](const auto &field){
      return field.material_bindings&&field.material_bindings->has_mtoon&&
          field.material_bindings->roles[size_t(MToonSlot::Normal)].image;
    }))
  charge(bytes,count,sizeof(std::array<float,2>),limits.maximum_bytes);
 charge(work,count,64,limits.maximum_work);size_t scratch=0;
 size_t independent_bytes=0,independent_work=0,scaled_alpha_work=0;
 for(size_t f=0;f<plan->fields().size();++f){
  stop(limits);const auto&field=plan->fields()[f];const auto&draw=plan->draws()[f];
  if(!draw.count)continue;
  charge(bytes,1,sizeof(TextureSetExportField)+field.name.size()+field.material.name.size()+258,limits.maximum_bytes);
  const auto resolution=field.painted->resolution();
  const bool hdr=texture_set_physical_emission(field);
  const bool mtoon=field.material_bindings&&field.material_bindings->has_mtoon;
  if(mtoon&&field.material_bindings->roles[size_t(MToonSlot::Normal)].image){
   charge(bytes,draw.count,sizeof(MToonNormalCorner),limits.maximum_bytes);
   charge(work,draw.count,64,limits.maximum_work);
  }
  const bool editable_pbr=editable_materials&&baked_mtoon_pbr_fallback(field);
  const bool typed_hdr=hdr&&!editable_materials&&native_mtoon_float_bake(field);
  const bool sparse_retained=retained_material_grid(field);
  if(mtoon&&!editable_pbr&&field.painted->has_paint_data()){
   charge_mtoon_paint(field,bytes,work,limits,typed_hdr);
   const auto &binding=*field.material_bindings;
   const auto &p=binding.material.parameters;
   if(binding.material.version()==MToonVersion::V1&&!field.source&&!field.appearance&&
      !field.painted->source_maps()&&!hdr&&
      p.scroll==std::array<float,2>{}&&p.rotation_speed==0){
    const bool opaque=field.material.alpha!=Material::AlphaMode::Automatic&&
        int(field.material.alpha)!=binding.native_reference.alpha?
        field.material.alpha==Material::AlphaMode::Opaque:p.alpha==MToonAlpha::Opaque;
    const auto native_alpha=field.material.color[3];
    const auto factor=mtoon_factor(p.base_color[3],binding.native_reference.color[3],native_alpha);
    const auto base_alpha=mtoon_base_alpha_grid(binding,opaque,factor,native_alpha,limits);
    for(const auto role:{MToonSlot::Base,MToonSlot::Shade,MToonSlot::Emission}){
     if(role==MToonSlot::Shade&&!mtoon_shared_shade(binding))continue;
     const auto alpha=role==MToonSlot::Base?base_alpha:MToonAlphaGrid::None;
     const auto grid=independent_mtoon_dimensions(binding,role,resolution,alpha);
     if(!grid)continue;
     const auto &source=*binding.roles[size_t(role)].image;
     const auto [w,h]=*grid;
     const auto columns=size_t(resolution)+source.width+w+2;
     const auto rows=size_t(resolution)+source.height+h+2;
     charge(independent_bytes,size_t(w)*h,4,SIZE_MAX);
     charge(independent_bytes,columns,2*sizeof(MToonIndependentTap)+sizeof(double),SIZE_MAX);
     charge(independent_bytes,rows,sizeof(double),SIZE_MAX);
     charge(independent_work,columns*rows,alpha==MToonAlphaGrid::Source?26:18,SIZE_MAX);
     charge(independent_work,size_t(w)*h,12,SIZE_MAX);
     if(alpha==MToonAlphaGrid::Source&&factor>0&&factor<1){
      // A failed unit-alpha candidate releases its image and scratch first.
      charge(scaled_alpha_work,columns*rows,26,SIZE_MAX);
      charge(scaled_alpha_work,size_t(w)*h,12,SIZE_MAX);
     }
    }
   }
   if(field.painted->baked()&&!field.painted->baked()->maps.normal.rgba.empty())
    charge(work,draw.count,8,limits.maximum_work);
  }
  if(hdr&&(!measured_physical||(mtoon?!(editable_pbr||typed_hdr):field.material.toon||field.appearance)))
   throw std::invalid_argument("Texture-set HDR emission needs its per-material precision-qualified export compiler");
  if(hdr&&(native_material_grid(field)||editable_pbr||typed_hdr)){
   const size_t pixels=size_t(resolution)*resolution;
   charge(bytes,1,native_material_grid_bytes(*field.painted,typed_hdr?TextureSetGridMode::NativeMToonHdr:
           TextureSetGridMode::NativeHdr),limits.maximum_bytes);
   charge(bytes,pixels,4,limits.maximum_bytes);
   continue;
  }
  if(sparse_retained){
   charge(bytes,1,native_material_grid_bytes(*field.painted,retained_grid_mode(field)),limits.maximum_bytes);
   charge(bytes,1,retained_hdr_normal_bytes(field),limits.maximum_bytes);
   // The sealed read raster and Bake are borrowed. Sparse directory capture
   // can copy layer pointers and hold ordered owner nodes for this one field.
   const auto metadata=TextureSetPaintAccess::snapshot_bytes(*field.painted);
   charge(bytes,metadata,1,limits.maximum_bytes);
   scratch=std::max(scratch,metadata*4);
  }else{
   charge(bytes,size_t(resolution)*resolution,64,limits.maximum_bytes);
   scratch=std::max(scratch,TextureSetPaintAccess::working_bytes(*field.painted,resolution));
  }
  if(field.source){
   field.source->authenticate_owner(field.source);field.source->validate_field(field,limits);
   const auto&preview=field.source->source_preview();
   if(!preview)throw std::invalid_argument("Retained export lacks actual before role data");
   charge(bytes,preview->geometric_frames().size(),64,limits.maximum_bytes);
   charge(bytes,draw.count,sizeof(PortableTextureSetCorner)+2*sizeof(std::array<float,2>)+3*sizeof(float),limits.maximum_bytes);
   charge(work,preview->normals().size(),128,limits.maximum_work);
   if(input.deformation&&evaluates_skin(*input.deformation)){
    charge(work,preview->geometric_frames().size(),8,limits.maximum_work);
    charge(work,draw.count,256+input.deformation->morph_weights.size()*4,limits.maximum_work);
   }
   if(preview->native_bake()||preview->model()==PolygonSurfaceNormalModel::PreparedCorePbr){
    const auto width=preview->native_bake()?preview->native_bake()->width:
        preview->baked()->normal_field->pbr()->width;
    const auto maximum=portable_normal_export_max_resolution(width);
    const auto pixels=size_t(maximum)*maximum;
    charge(bytes,pixels,4,limits.maximum_bytes);
    if(pixels>limits.maximum_bytes/8)throw std::length_error("Portable normal raster exceeds its byte budget");
    scratch=std::max(scratch,pixels*8);
   }
  }
  if(hdr)charge(bytes,size_t(resolution)*resolution,4,limits.maximum_bytes);
  if(!sparse_retained)charge(work,size_t(resolution)*resolution,16+8*field.painted->layers().size(),limits.maximum_work);
 }
 charge(bytes,scratch,1,limits.maximum_bytes);
 if(input.deformation){
  const auto entries=input.deformation->resource_entries();charge(bytes,entries,32,limits.maximum_bytes);charge(work,entries,16,limits.maximum_work);
  if(evaluates_skin(*input.deformation)&&std::ranges::any_of(plan->fields(),[](const auto &f){return bool(f.source);})){
   charge(bytes,rest.positions.size(),4*sizeof(Vec3)+2*sizeof(std::array<float,2>)+2,limits.maximum_bytes);
   charge(bytes,rest.indices.size(),2*sizeof(uint32_t)+2,limits.maximum_bytes);
   charge(bytes,input.deformation->skeleton->bones.size(),4*sizeof(RigMatrix),limits.maximum_bytes);
  }
  if(input.deformation->morphs&&!input.deformation->morphs->targets.empty()){
   const size_t targets=input.deformation->morphs->targets.size();
   const bool tangents=std::ranges::any_of(plan->fields(),[](const auto &field){
    if(!field.source)return field.painted->baked()&&!field.painted->baked()->maps.normal.rgba.empty();
    const auto &preview=field.source->source_preview();
    return preview->native_bake()||preview->model()==PolygonSurfaceNormalModel::PreparedCorePbr;
   });
   charge(bytes,targets,sizeof(SourceMorphChannels),limits.maximum_bytes);
   charge(bytes,count,targets*(tangents?2:1)*sizeof(Vec3)+sizeof(uint32_t),limits.maximum_bytes);
   charge(bytes,rest.positions.size(),2*sizeof(Vec3)+sizeof(std::array<float,2>)+1,limits.maximum_bytes);
   charge(bytes,rest.indices.size(),sizeof(uint32_t)+1+(tangents?2*sizeof(Tangent):0),limits.maximum_bytes);
   charge(work,rest.positions.size()+rest.indices.size(),targets*8,limits.maximum_work);
   if(tangents)charge(work,count,targets*32,limits.maximum_work);
   for(const auto &field:plan->fields())if(field.source&&!field.domain->triangles().empty()){
    const auto &preview=*field.source->source_preview();
    // Endpoint frames/corners are scratch for one field at a time. The
    // same bound above already accounts for these arrays during rest export.
    charge(work,preview.geometric_frames().size(),targets*8,limits.maximum_work);
    charge(work,preview.normals().size(),targets*192,limits.maximum_work);
   }
  }
 }
 const bool allow_independent=independent_bytes&&independent_bytes<=limits.maximum_bytes-bytes&&
     independent_work<=limits.maximum_work-work;
 if(allow_independent){bytes+=independent_bytes;work+=independent_work;}
 const bool allow_scaled=allow_independent&&scaled_alpha_work&&scaled_alpha_work<=limits.maximum_work-work;
 if(allow_scaled)work+=scaled_alpha_work;
 stop(limits);return {bytes,work,allow_independent,allow_scaled};
}
} // namespace

std::optional<PortableMToonSetPaint> compile_mtoon_set_paint(
    const TextureSetView &field,const TextureSetExportField &compiled,
    const MToonMaterial &effective,const TextureSetLimits &limits){
 const auto &maps=compiled.textures;
 if(!maps||compiled.set!=field.id||!field.material_bindings||!field.material_bindings->has_mtoon||
    field.source||field.appearance||field.painted->source_maps()||maps->overlay||maps->source_maps||
    (texture_set_physical_emission(field)&&!(compiled.emission&&compiled.emission->filter_error)))
  return {};
 const auto &bake=field.painted->baked();
 const bool normal=bake&&!bake->maps.normal.rgba.empty();
 if(maps->baked_base!=bake||(!maps->baked_normal.rgba.empty())!=normal||
    (normal&&!compiled.portable_mtoon_bake_frame))return {};
 const auto &binding=*field.material_bindings;
 const auto &params=effective.parameters;
 const auto covered=[&](const Image &image){
  for(size_t at=0;at<image.rgba.size();at+=4){
   if((at&16383)==0)stop(limits);
   if(image.rgba[at])return true;
  }
  return false;
 };
 const bool color=covered(maps->color_coverage),emission=covered(maps->emission_coverage);
 if((color||emission||normal)&&(params.scroll!=std::array<float,2>{}||params.rotation_speed!=0))
  return {};

 PortableMToonSetPaint out;
 const auto independent=[&](MToonSlot role,const Image &resolved,const Image &coverage,
     const std::array<float,4> &factor,const std::array<float,4> &native,
     MToonAlphaGrid alpha=MToonAlphaGrid::None)->std::shared_ptr<const Image>{
  if(!compiled.independent_mtoon_paint)return {};
  const auto dimensions=independent_mtoon_dimensions(binding,role,field.painted->resolution(),alpha);
  if(!dimensions)return {};
  return compose_independent_mtoon(role,*binding.roles[size_t(role)].image,resolved,coverage,
                                    *dimensions,factor,native,params,alpha,out,limits);
 };
 const auto compose=[&](const Image &resolved,const Image &coverage,
     const MToonPaintSource &source,const std::array<float,4> &factor,const std::array<float,4> &native,
     bool use_alpha,double &rgb_error)->std::shared_ptr<const Image>{
  const auto resolution=field.painted->resolution();
  if(resolved.width!=resolution||resolved.height!=resolution||
     coverage.width!=resolution||coverage.height!=resolution)
   throw std::logic_error("MToon material lost the current paint grid");
  resolved.validate();coverage.validate();
  const bool visible_alpha=use_alpha&&params.alpha!=MToonAlpha::Opaque;
  const auto pass_alpha=[&](double alpha){
   return params.alpha==MToonAlpha::Mask?alpha>=params.alpha_cutoff:alpha>0;
  };
  auto result=std::make_shared<Image>(resolved);
  const auto &linear=mtoon_linear_bytes();
  const auto channels=use_alpha?4u:3u;
  std::vector<MToonSourceTap> previous(source.aligned?resolution:0),row(previous.size());
  for(size_t y=0;y<resolution;++y){
   stop(limits);
   for(size_t x=0;x<resolution;++x){
    const size_t at=(y*resolution+x)*4;
    const auto original=source.sample(at);
    MToonSourceTap tap;
    tap.uncovered=1-coverage.rgba[at]/255.;
    for(size_t k=0;k<channels;++k){
     const auto before=resolved.rgba[at+k];
     const double decoded=k<3?linear[before]:before/255.;
     const auto background=static_cast<float>(original[k])*factor[k];
     const double corrected=decoded+(double(background)-native[k])*tap.uncovered;
     const double clamped=std::clamp(corrected,0.,1.);
     const auto quantized=k<3?mtoon_srgb_byte(clamped):uint8_t(std::lround(clamped*255.));
     const double restored=k<3?linear[quantized]:quantized/255.;
     // The alpha curve under paint must stay byte-identical. At zero
     // filtered coverage, all participating taps use the original alpha.
     if(visible_alpha&&k==3&&(background!=native[k]||quantized!=before||
        (!coverage.rgba[at]&&pass_alpha(restored)!=pass_alpha(background))))return {};
     tap.background[k]=background;
     tap.error[k]=std::abs(restored-corrected);
     double &error=k<3?rgb_error:out.alpha_error;
     error=std::max(error,tap.error[k]);
     if(!coverage.rgba[at])error=std::max(error,std::abs(restored-background));
     result->rgba[at+k]=quantized;
    }
    if(!source.aligned)continue;
    row[x]=tap;
    if(!x||!y)continue;
    const std::array<const MToonSourceTap*,4> taps{&previous[x-1],&previous[x],&row[x-1],&row[x]};
    for(size_t k=0;k<channels;++k){
     const auto product=mtoon_source_filter_error(taps,k);
     const double quantization=std::max({taps[0]->error[k],taps[1]->error[k],taps[2]->error[k],taps[3]->error[k]});
     double &error=k<3?rgb_error:out.alpha_error;
     error=std::max(error,quantization+product);
     out.source_filter_error=std::max(out.source_filter_error,product);
    }
   }
   previous.swap(row);
  }
  return result;
 };
 if(color){
  const auto original=mtoon_paint_source(binding,effective,MToonSlot::Base,field.painted->resolution(),limits);
  const auto alpha=mtoon_base_alpha_grid(binding,params.alpha==MToonAlpha::Opaque,params.base_color[3],
                                      field.material.color[3],limits);
  out.color=original&&alpha!=MToonAlphaGrid::Source?
      compose(maps->color,maps->color_coverage,*original,params.base_color,field.material.color,true,out.color_error):
      independent(MToonSlot::Base,maps->color,maps->color_coverage,params.base_color,field.material.color,alpha);
  if(!out.color&&alpha==MToonAlphaGrid::Source&&compiled.scaled_mtoon_alpha&&
     params.base_color[3]>0&&params.base_color[3]<1){
   // Keep the authored multiplier when quantizing it into the PNG would
   // shift a visibility edge. The scaled result gets its own full proof.
   out={};
   out.alpha_factor=params.base_color[3];
   out.color=independent(MToonSlot::Base,maps->color,maps->color_coverage,params.base_color,field.material.color,alpha);
  }
  if(!out.color)return {};
  if(mtoon_shared_shade(binding)){
   const auto &factor=params.shade_color;
   out.shade=original?
       compose_shared_mtoon_shade(maps->color,maps->color_coverage,field.material.color,*original,factor,out,limits):
       independent(MToonSlot::Shade,maps->color,maps->color_coverage,{factor.x,factor.y,factor.z,1},field.material.color);
   if(!out.shade)return {};
  }
 }
 if(emission){
  const auto original=mtoon_paint_source(binding,effective,MToonSlot::Emission,field.painted->resolution(),limits);
  const std::array factor{params.emission.x,params.emission.y,params.emission.z,1.f};
  const auto &native=field.material.emissive;
  const std::array base_emission{native[0],native[1],native[2],1.f};
  out.emission=original?
      compose(maps->emission,maps->emission_coverage,*original,factor,base_emission,false,out.emission_error):
      independent(MToonSlot::Emission,maps->emission,maps->emission_coverage,factor,base_emission);
  if(!out.emission)return {};
 }
 if(normal){
  out.normal=compose_mtoon_baked_normal(field,*maps,effective,out.normal_error_radians,limits);
  if(!out.normal)return {};
 }
 // A typical nearest sRGB byte step has at most .0045 linear error.
 // Include arithmetic tolerance in the recorded bound, before acceptance.
 constexpr double sampling_slack=1e-6;
 if(out.color){out.color_error+=sampling_slack;out.alpha_error+=sampling_slack;}
 if(out.shade)out.shade_error+=sampling_slack;
 if(out.emission)out.emission_error+=sampling_slack;
 if(out.color_error>.005||out.shade_error>.005||out.emission_error>.005||out.alpha_error>1./255.)
  return {};
 return out;
}

namespace {
struct RetopoPaintTap {
 MToonSourceTap product, core_shade;
 // Only shared Shade clips the isolated paint contribution before adding it.
 std::array<double,4> shade_ink{}, unpainted_error{};
 double encoded_alpha=1;
};

std::shared_ptr<const Image> compose_retopo_mtoon_role(const TextureSetView &field,
    const MToonMaterial &effective,MToonSlot role,const TextureMaps *maps,
    const PaintEmissionSampler *hdr,PortableMToonSetPaint &proof,const TextureSetLimits &limits) {
 const auto &p=effective.parameters;
 const auto &binding=*field.material_bindings;
 const auto r=field.painted->resolution();
 const bool color=role==MToonSlot::Base,shade=role==MToonSlot::Shade;
 const auto &core_maps=*field.painted->source_maps();
 const auto &generic=role==MToonSlot::Emission?core_maps.emission:core_maps.color;
 const auto typed=mtoon_paint_source(binding,effective,role,r,limits);
 if(!typed)return {};
 const auto aligned=[&](const PaintSourceTexture &view){
  return !view.image||(view.texcoord==0&&view.uv_transform==std::array<float,6>{1,0,0,1,0,0}&&
                       aligned_mtoon_source(view,MToonTexture{},r));
 };
 if(!hdr&&!aligned(generic))return {};
 const auto &linear=mtoon_linear_bytes();
 const auto &native=field.material;
 const auto factor=color?p.base_color:shade?std::array{p.shade_color.x,p.shade_color.y,p.shade_color.z,1.f}:
     std::array{p.emission.x,p.emission.y,p.emission.z,1.f};
 const auto *resolved=hdr?nullptr:role==MToonSlot::Emission?&maps->emission:&maps->color;
 const auto *coverage=hdr?nullptr:role==MToonSlot::Emission?&maps->emission_coverage:&maps->color_coverage;
 if(resolved&&(resolved->width!=r||resolved->height!=r||coverage->width!=r||coverage->height!=r))return {};
 auto image=std::make_shared<Image>(Image{r,r,std::vector<uint8_t>(size_t(r)*r*4,255)});
 std::vector<RetopoPaintTap> previous(r),row(r);
 auto &bound=color?proof.color_error:shade?proof.shade_error:proof.emission_error;
 constexpr double rgb_limit=.008, arithmetic_slack=2e-6;
 for(uint32_t y=0;y<r;++y){
  stop(limits);
  for(uint32_t x=0;x<r;++x){
   const size_t at=(size_t(y)*r+x)*4;
   auto &tap=row[x];
   const auto original=typed->sample(at);
   const auto emission=hdr?hdr->sample_texel(x,y):BakedEmissionSample{};
   auto &product=tap.product;
   product.uncovered=1-(hdr?double(emission.coverage):coverage->rgba[at]/255.);
   tap.core_shade.uncovered=product.uncovered;
   const bool uncovered=!hdr&&product.uncovered==1;
   bool paint_border=false;
   if(uncovered){
    for(uint32_t v=y?y-1:0;v<=std::min(y+1,r-1)&&!paint_border;++v)
     for(uint32_t u=x?x-1:0;u<=std::min(x+1,r-1);++u)
      paint_border|=coverage->rgba[(size_t(v)*r+u)*4]!=0;
   }
   for(size_t k=0;k<(color?4u:3u);++k){
    const bool alpha=k==3;
    const double typed_value=original[k]*factor[k];
    const auto decode=[&](uint8_t b){return alpha?b/255.:linear[b];};
    double legacy_background=0,composite=0;
    if(hdr)composite=emission.legacy[k];
    else {
     const double generic_value=generic.image?decode(generic.image->rgba[at+k]):1.;
     legacy_background=generic_value*(color||shade?native.color[k]:native.emissive[k]);
     composite=decode(resolved->rgba[at+k]);
    }
    const double divisor=shade&&native.color[k]>0?native.color[k]:1.;
    product.background[k]=shade?typed_value:typed_value-legacy_background;
    tap.core_shade.background[k]=shade?legacy_background*factor[k]/divisor:0;
    tap.shade_ink[k]=shade?(composite-legacy_background*product.uncovered)*factor[k]/divisor:0;
    const double raw=shade?typed_value*product.uncovered+std::max(0.,tap.shade_ink[k]):
        composite+product.background[k]*product.uncovered;
    // At zero coverage the native shader bypasses quantization in the Core
    // composite. On that contour, one portable texel must represent the
    // native tap and its limit from the painted side. Center their error.
    const double target=uncovered?(paint_border?std::midpoint(typed_value,raw):typed_value):raw;
    const auto byte=alpha?uint8_t(std::lround(std::clamp(target,0.,1.)*255)):mtoon_srgb_byte(target);
    image->rgba[at+k]=byte;
    const double encoded=decode(byte);
    product.error[k]=encoded-raw;
    tap.unpainted_error[k]=uncovered?std::abs(encoded-typed_value):0;
    if(alpha)tap.encoded_alpha=encoded;
   }
   const auto left=x?x-1:x;
   const auto &above=y?previous:row;
   const std::array<const RetopoPaintTap*,4> taps{&above[left],&above[x],&row[left],&tap};
   const std::array<const MToonSourceTap*,4> products{&taps[0]->product,&taps[1]->product,&taps[2]->product,&tap.product};
   const bool empty=std::ranges::all_of(products,[](const auto *entry){return entry->uncovered==1;});
   for(size_t k=0;k<(color?4u:3u);++k){
    double unpainted_error=0;
    double low_encoded=products[0]->error[k],high_encoded=low_encoded;
    for(const auto *entry:taps){
     unpainted_error=std::max(unpainted_error,entry->unpainted_error[k]);
     low_encoded=std::min(low_encoded,entry->product.error[k]);
     high_encoded=std::max(high_encoded,entry->product.error[k]);
    }
    double covariance=mtoon_source_filter_error(products,k),kink=0;
    if(shade&&!empty){
     double low=0,high=0;
     for(const auto *entry:taps){low=std::min(low,entry->shade_ink[k]);high=std::max(high,entry->shade_ink[k]);}
     kink=low<0&&high>0?-low*high/(high-low):0;
     proof.shade_interpolation_error=std::max(proof.shade_interpolation_error,kink);
     // In the shared-shade case the two source products vary independently.
     // The signed paint residual also loses a covariance of Core and coverage.
     covariance+=mtoon_source_filter_error({&taps[0]->core_shade,&taps[1]->core_shade,
                                           &taps[2]->core_shade,&tap.core_shade},k);
    }
    proof.source_filter_error=std::max(proof.source_filter_error,covariance);
    // Clamping the signed shade contribution is convex. Its interpolation
    // error only increases the portable value, so retain the sign of the
    // encoded error when combining its interval with the two covariances.
    double local_error=unpainted_error;
    if(!empty||hdr)
     local_error=std::max({local_error,std::abs(low_encoded-covariance),std::abs(high_encoded+covariance+kink)});
    local_error+=arithmetic_slack;
    if(k==3){
     proof.alpha_error=std::max(proof.alpha_error,local_error);
     if(p.alpha==MToonAlpha::Opaque)continue;
     const auto [lowest,highest]=std::ranges::minmax(taps,{},[](const auto *entry){return entry->encoded_alpha;});
     const double cutoff=p.alpha==MToonAlpha::Mask?p.alpha_cutoff:0;
     if(lowest->encoded_alpha-local_error<=cutoff&&highest->encoded_alpha+local_error>=cutoff)return {};
     if(proof.alpha_error>1./255.)return {};
    }else {
     bound=std::max(bound,local_error);
     if(bound>rgb_limit)return {};
    }
   }
  }
  previous.swap(row);
 }
 return image;
}
} // namespace

std::optional<PortableMToonSetPaint> compile_retopo_mtoon_paint(const TextureSetView &field,
    const MToonMaterial &effective,TextureSetLimits &remaining)try {
 const auto &paint=*field.painted;
 const auto &binding=*field.material_bindings;
 size_t work=0,bytes=0;
 const auto [color,emission]=legacy_coverage(paint,work,remaining);
 remaining.maximum_work-=work;
 if(!color&&!emission)return PortableMToonSetPaint{};
 work=0;
 const auto r=paint.resolution();
 const bool shade=color&&mtoon_shared_shade(binding);
 const bool hdr=emission&&EmissionExportAccess::requires_preview(paint,field.material,&binding);
 const size_t pixels=size_t(r)*r;
 // The current Core compositor and three bounded role passes run on the
 // file worker. Each proof uses two rows and one result image per role.
 charge(work,pixels,48+12*paint.layers().size(),remaining.maximum_work);
 charge(bytes,pixels,48,remaining.maximum_bytes);
 charge(bytes,r,2*sizeof(RetopoPaintTap),remaining.maximum_bytes);
 const size_t roles=size_t(color)+size_t(shade)+size_t(emission);
 charge(work,pixels,72*roles,remaining.maximum_work);
 std::optional<PaintEmissionSampler> coefficients;
 if(hdr){
  coefficients.emplace(paint,remaining.cancelled);
  charge(work,coefficients->index_work(),1,remaining.maximum_work);
  charge(work,pixels,coefficients->work_per_texel(),remaining.maximum_work);
 }
 // Colour always uses the native derived bytes, including the original Core
 // backdrop that the shader removes before inserting the typed role.
 const auto maps=EmissionExportAccess::requires_preview(paint,field.material,&binding)?
     EmissionExportAccess::preview(paint,field.material,remaining.cancelled):paint.composite(field.material,remaining.cancelled);
 remaining.maximum_work-=work;
 PortableMToonSetPaint out;
 if(color){
  out.color=compose_retopo_mtoon_role(field,effective,MToonSlot::Base,maps.get(),nullptr,out,remaining);
  if(!out.color)return {};
 }
 if(shade){
  out.shade=compose_retopo_mtoon_role(field,effective,MToonSlot::Shade,maps.get(),nullptr,out,remaining);
  if(!out.shade)return {};
 }
 if(emission&&effective.parameters.emission_strength!=0){
  out.emission=compose_retopo_mtoon_role(field,effective,MToonSlot::Emission,maps.get(),
      coefficients?&*coefficients:nullptr,out,remaining);
  if(!out.emission)return {};
 }
 remaining.maximum_bytes-=pixels*roles*4;
 return out;
} catch(const ExportBudgetExceeded &) {
 // The independent Core view and an Editable package can still be emitted.
 return {};
}

std::optional<MToonFloatPreview> compile_editable_mtoon_float_preview(
    const TextureSetView &field,const MToonMaterial &effective,TextureSetLimits &budget){
 stop(budget);
 if(!native_mtoon_float_bake(field)||!budget.maximum_bytes||!budget.maximum_work)
  return {};
 size_t work=0;
 size_t bytes=sizeof(MToonFloatPreview);
 std::optional<MToonFloatPreview> result;
 try {
  charge_mtoon_paint(field,bytes,work,budget,true);
  charge(bytes,1,native_material_grid_bytes(*field.painted,TextureSetGridMode::NativeMToonHdr),budget.maximum_bytes);
  const auto resolution=field.painted->resolution();
  charge(bytes,size_t(resolution)*resolution,4,budget.maximum_bytes);

  TextureSetExportField candidate;
  candidate.set=field.id;
  candidate.portable_mtoon_bake_frame=true;
  candidate.emission=compile_emission(field,work,budget,true);
  auto maps=compile_native_material_grid(*field.painted,field.material,work,budget,TextureSetGridMode::NativeMToonHdr);
  candidate.textures=maps;
  if(auto paint=compile_mtoon_set_paint(field,candidate,effective,budget))
   result.emplace(MToonFloatPreview{std::move(*candidate.emission),std::move(maps->metal_rough),std::move(*paint)});
 } catch(const MToonFloatUnavailable &) {
  // A legal authored material can lie outside the portable sampling domain.
 } catch(const std::overflow_error &) {
  // The typed resolved view has its own finite radiance range.
 } catch(const ExportBudgetExceeded &) {
  // The original Editable fallback has its own, already reserved budget.
 }
 budget.maximum_work-=work;
 if(result)budget.maximum_bytes-=bytes;
 return result;
}

bool texture_set_physical_emission(const TextureSetView &field) {
 return hdr_emission(field.material.emissive)||(field.painted&&field.painted->has_physical_emission())||
     (field.material_bindings&&physical_emission(*field.material_bindings));
}

std::shared_ptr<const RetopoMToonExport> RetopoMToonExport::capture(const SculptObject &object,
    const std::function<bool()> &cancelled) {
 const auto &paint=object.painted;
 const auto &binding=object.material_bindings;
 if(!object.retopology||object.texture_sets||!object.mesh_active||object.deformation||
    !native_texture_set_mtoon(paint.get(),object.appearance.get(),binding.get())||
    binding->material.version()!=MToonVersion::V1||!paint->source_maps()||paint->in_stroke()||
    paint->baked()||paint->has_physical_emission()||binding->physical_emission)
  return {};
 const auto &params=binding->material.parameters;
 if(params.rotation_speed!=0||params.scroll!=std::array<float,2>{})return {};
 const auto &core=paint->source_maps()->normal;
 const auto &normal=binding->material.textures[size_t(MToonSlot::Normal)];
 // Both materials read the same explicit tangent attribute in a portable
 // primitive. The independently projected maps must use the same UV basis.
 if(core.texcoord||core.uv_transform!=std::array<float,6>{1,0,0,1,0,0}||
    normal.texcoord||normal.transform_texcoord!=-1||normal.offset!=std::array<float,2>{}||
    normal.scale!=std::array<float,2>{1,1}||normal.rotation!=0)
  return {};
 if(cancelled&&cancelled())throw std::runtime_error("MToon export capture cancelled");
 auto result=std::shared_ptr<RetopoMToonExport>(new RetopoMToonExport);
 result->object_=object.id;
 result->field_=texture_set_view(object,0);
 result->field_.painted=paint->snapshot();
 return result;
}

void RetopoMToonExport::authenticate(const ObjectSurface &surface,const TextureSetLimits &limits) const {
 stop(limits);
 const auto equal=[](const auto &a,const auto &b){
  return a.size()==b.size()&&(a.empty()||!std::memcmp(a.data(),b.data(),a.size()*sizeof(a.front())));
 };
 const auto &source=field_.painted->mesh();
 const auto &mesh=surface.mesh;
 if(surface.id!=object_||surface.appearance||surface.deformation||surface.texture_set_surface||
    !equal(mesh.positions,source.positions)||!equal(mesh.normals,source.normals)||!equal(mesh.uv,source.uv)||
    mesh.indices!=source.indices||mesh.uv_pins!=source.uv_pins||mesh.uv_seams!=source.uv_seams)
  throw std::invalid_argument("Projected MToon export lost its exact current mesh owner");
}

std::optional<TextureSetExport> compile_retopo_mtoon_export(const ObjectSurface &object,const TextureSetLimits &limits) {
 limits.validate();
 const auto &owner=*object.retopo_mtoon_export;
 const auto &field=owner.field();
 const auto &mesh=object.mesh;
 const size_t count=mesh.indices.size();
 if(count>limits.maximum_corners||count/3>limits.maximum_triangles||mesh.positions.size()>limits.maximum_corners)
  throw std::length_error("MToon export exceeds geometry bounds");
 size_t bytes=sizeof(TextureSetExport),work=0;
 charge(work,count,64,limits.maximum_work);
 charge(bytes,count,sizeof(MToonNormalCorner)+2*sizeof(uint32_t)+sizeof(Tangent)+2*sizeof(Vec3)+
        sizeof(std::array<float,2>)+6*sizeof(void*),limits.maximum_bytes);
 charge(bytes,mesh.positions.size(),2*sizeof(Vec3)+sizeof(std::array<float,2>),limits.maximum_bytes);
 owner.authenticate(object,limits);
 mesh.validate();
 const auto frames=portable_mtoon_derivative_frames(mesh,field,object.world_matrix(),limits);
 if(!frames||frames->normal_coordinates)return {};

 TextureSetExport out;
 out.working_bound=bytes;
 out.surface=object;
 auto &emitted=out.surface.mesh;
 emitted=Mesh{};
 emitted.indices.reserve(count);
 std::map<std::pair<uint32_t,std::array<uint32_t,4>>,uint32_t> indices;
 for(size_t face=0;face<frames->triangles.size();++face){
  if((face&1023)==0)stop(limits);
  for(size_t c=0;c<3;++c){
   const auto vertex=mesh.indices[face*3+c];
   const auto &tangent=frames->triangles[face][c].tangent;
   const auto [entry,fresh]=indices.emplace(std::pair{vertex,std::bit_cast<std::array<uint32_t,4>>(tangent)},
                                            uint32_t(emitted.positions.size()));
   if(fresh){
    emitted.positions.push_back(mesh.positions[vertex]);
    emitted.normals.push_back(mesh.normals[vertex]);
    emitted.uv.push_back(mesh.uv[vertex]);
    out.tangents.push_back(tangent);
   }
   emitted.indices.push_back(entry->second);
  }
 }
 TextureSetExportField material;
 material.name=field.name;
 material.count=count;
 material.material=field.material;
 material.tangent_attribute=true;
 material.portable_mtoon_derivative_frame=true;
 out.fields.push_back(std::move(material));
 return out;
}

EmissionExportRaster texture_set_emission_raster(const TextureSetView &field,const TextureSetLimits &limits,bool authored_mtoon){
 if(!texture_set_physical_emission(field)||!field.domain||field.domain->triangles().empty()||
    (authored_mtoon?!native_mtoon_float_bake(field):
        field.material.toon||field.appearance||(field.material_bindings&&field.material_bindings->has_mtoon)))
  throw std::invalid_argument("Texture-set HDR requires its actual physical material member");
 size_t work=0;
 TextureSetEmissionSamples samples(field,work,1,limits,authored_mtoon);
 const auto resolution=samples.resolution;
 const auto working_bytes=samples.working_bytes();
 return {resolution,2,working_bytes,
      [samples=std::move(samples),cancelled=limits.cancelled,resolution](uint32_t y,std::span<EmissionExrPixel> rows){
        if(size_t(y)>resolution||rows.size()>size_t(resolution-y)*resolution)
         throw std::logic_error("Texture-set HDR read exceeds its owning raster");
        for(size_t i=0;i<rows.size();++i){
         if((i&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Texture-set HDR raster cancelled");
         rows[i]=samples.pixel(uint32_t(i%resolution),y+uint32_t(i/resolution));
        }
      }};
}

size_t texture_set_export_working_bound(const ObjectSurface&input,const TextureSetLimits&limits,bool measured_physical,bool editable_materials){
 return export_bound(input,limits,measured_physical,editable_materials).bytes;
}
TextureSetExport compile_texture_set_export(const ObjectSurface&input,const TextureSetLimits&limits,bool measured_physical,bool editable_materials){
 auto [bytes,work,independent_mtoon,scaled_mtoon_alpha]=export_bound(input,limits,measured_physical,editable_materials);
 TextureSetExport out;out.working_bound=bytes;
 const auto&plan=input.texture_set_surface;const auto&rest=*plan->geometry();const auto count=plan->indices().size();
 const bool source=std::any_of(plan->fields().begin(),plan->fields().end(),[](const auto&f){return bool(f.source);});
 const auto *morphs=input.deformation&&input.deformation->morphs?&*input.deformation->morphs:nullptr;
 const bool has_morphs=morphs&&!morphs->targets.empty();
 const bool skin_check=source&&input.deformation&&evaluates_skin(*input.deformation);
 const bool static_geometry=!has_morphs&&(!input.deformation||!evaluates_skin(*input.deformation));
 const auto world=input.world_matrix();
 bool normal=false;
 bool normal_coordinates=source;
 // Tangent basis is evaluated in REST, never the current pose. Include the
 // actual vertex and role-specific frame bits in every necessary split.
 const bool canonical_frames=std::any_of(plan->fields().begin(),plan->fields().end(),[](const auto&field){return !field.source&&field.painted->baked()&&!field.painted->baked()->maps.normal.rgba.empty();});
 const auto frames=canonical_frames?tangent_corners(rest,limits.cancelled):std::vector<Tangent>{};
 using Signature=std::array<uint32_t,12>;std::map<Signature,uint32_t>vertices;std::vector<uint32_t>rest_vertices;
 std::vector<uint32_t>tangent_corners_by_vertex;
 struct RetainedShapeRange {size_t field,first_vertex,output;bool reprojected;};
 std::vector<RetainedShapeRange> retained_shapes;
 if(has_morphs&&canonical_frames)tangent_corners_by_vertex.reserve(count);
 auto&mesh=out.surface.mesh;mesh.indices.reserve(count);rest_vertices.reserve(std::min(count,rest.positions.size()));
 for(size_t f=0;f<plan->fields().size();++f){
  stop(limits);const auto&field=plan->fields()[f];const auto&draw=plan->draws()[f];if(!draw.count)continue;
  const bool canonical=!field.source&&field.painted->baked()&&!field.painted->baked()->maps.normal.rgba.empty();
  const auto portable=field.source?std::optional<PortableTextureSetField>(portable_texture_set_field(field,world,work,limits)):std::nullopt;
  const auto typed_frames=static_geometry&&!canonical&&!field.source?
      portable_mtoon_derivative_frames(rest,field,world,limits):std::nullopt;
  if(portable&&(has_morphs||skin_check))
   retained_shapes.push_back({f,mesh.positions.size(),out.fields.size(),portable->reprojected});
  const bool retained_normal=portable&&portable->normal_role;
  const bool tangent_attribute=canonical||retained_normal||bool(typed_frames);
  normal|=tangent_attribute;
  TextureSetExportField output;
  output.set=field.id;
  output.name=field.name;
  output.first=mesh.indices.size();
  output.count=draw.count;
  output.material=field.material;
  output.normal_role=retained_normal;
  output.tangent_attribute=tangent_attribute;
  output.portable_mtoon_derivative_frame=bool(typed_frames);
  output.portable_mtoon_normal_coordinates=typed_frames&&typed_frames->normal_coordinates;
  if(output.portable_mtoon_normal_coordinates&&!normal_coordinates){
   out.normal_coordinates.reserve(count);
   out.normal_coordinates=mesh.uv;
   normal_coordinates=true;
  }
  const bool physical=texture_set_physical_emission(field);
  output.independent_mtoon_paint=independent_mtoon&&!physical;
  output.scaled_mtoon_alpha=scaled_mtoon_alpha&&!physical;
  const bool typed_hdr=physical&&!editable_materials&&native_mtoon_float_bake(field);
  if(canonical&&!input.deformation&&field.material_bindings&&field.material_bindings->has_mtoon&&
     (!physical||typed_hdr||(editable_materials&&native_mtoon_float_bake(field))))
   output.portable_mtoon_bake_frame=portable_mtoon_bake_frame(rest,field,frames,world,limits);
  if(physical){
   if(typed_hdr&&!output.portable_mtoon_bake_frame)
    throw std::invalid_argument("Typed FLOAT Bake needs its certified canonical tangent frame");
   const bool editable_pbr=editable_materials&&baked_mtoon_pbr_fallback(field);
   output.emission=compile_emission(field,work,limits,typed_hdr);
   output.material.emissive={};
   if(editable_pbr)output.material.toon=false;
   if(portable)output.textures=portable->maps;
   else if(native_material_grid(field)||editable_pbr||typed_hdr)output.textures=compile_native_material_grid(
       *field.painted,field.material,work,limits,typed_hdr?TextureSetGridMode::NativeMToonHdr:
       TextureSetGridMode::NativeHdr);
   else output.textures=EmissionExportAccess::other_channels(
       *texture_set_source_read_view(field,limits),output.material,limits.cancelled);
  }else output.textures=portable?portable->maps:field.painted->composite(field.material,limits.cancelled);
  for(size_t c=0;c<draw.count;++c){
   if((c&1023)==0)stop(limits);const auto logical=plan->logical_corners()[draw.first+c];const auto vertex=rest.indices.at(logical);
   const auto n=portable?portable->corners[c].normal:rest.normals.at(vertex);
   const auto paint=portable?portable->corners[c].paint:rest.uv.at(vertex);
   if(typed_frames&&field.domain->triangles()[c/3]!=logical/3)
    throw std::logic_error("Typed tangent correspondence differs from the emitted material range");
   const auto *typed_corner=typed_frames?&typed_frames->triangles.at(c/3)[c%3]:nullptr;
   const auto role=portable?portable->corners[c].role:typed_corner?typed_corner->uv:rest.uv.at(vertex);
   const Tangent tangent=portable?portable->corners[c].tangent:canonical?frames.at(logical):
       typed_corner?typed_corner->tangent:Tangent{1,0,0,1};
   Signature key{};key[0]=vertex;size_t k=1;for(float v:tangent)key[k++]=std::bit_cast<uint32_t>(v);for(float v:{n.x,n.y,n.z})key[k++]=std::bit_cast<uint32_t>(v);for(float v:paint)key[k++]=std::bit_cast<uint32_t>(v);for(float v:role)key[k++]=std::bit_cast<uint32_t>(v);
   // Independent retained frames can diverge at a shared rest vertex.
   // Preserve all logical corners needed to represent endpoint directions.
   const bool distinct=(canonical||portable.has_value())&&has_morphs;
   auto found=distinct?vertices.end():vertices.find(key);uint32_t id;
   if(found!=vertices.end())id=found->second;
   else{
    if(mesh.positions.size()>=2'000'000)throw std::length_error("Texture-set exported vertex splits exceed the native geometry limit");
    id=uint32_t(mesh.positions.size());if(!distinct)vertices.emplace(key,id);rest_vertices.push_back(vertex);mesh.positions.push_back(rest.positions.at(vertex));mesh.normals.push_back(n);if(!rest.uv.empty())mesh.uv.push_back(rest.uv.at(vertex));if(!rest.uv_pins.empty())mesh.uv_pins.push_back(rest.uv_pins.at(vertex));out.tangents.push_back(tangent);
    if(source)out.paint_coordinates.push_back(paint);
    if(normal_coordinates)out.normal_coordinates.push_back(role);
    if(has_morphs&&canonical_frames)tangent_corners_by_vertex.push_back(canonical?logical:UINT32_MAX);
   }
   mesh.indices.push_back(id);if(!rest.uv_seams.empty())mesh.uv_seams.push_back(rest.uv_seams.at(logical));
  }
  out.fields.push_back(std::move(output));
 }
 if(out.fields.empty()||mesh.indices.size()!=count)throw std::invalid_argument("Texture-set export omitted a nonempty material range");
 if(has_morphs){
  out.morph_channels.reserve(morphs->targets.size());
  for(size_t target=0;target<morphs->targets.size();++target){
   stop(limits);
   const auto shaped=morphs->target_surface(rest,target,limits.cancelled);
   auto &channels=out.morph_channels.emplace_back();
   channels.normals.reserve(rest_vertices.size());
   const auto shaped_tangents=canonical_frames?tangent_corners(shaped,limits.cancelled):std::vector<Tangent>{};
   if(normal)channels.tangents.resize(rest_vertices.size());
   const auto write_tangent=[&](size_t vertex,const Tangent &to){
    const auto &from=out.tangents[vertex];
    if(from[3]!=to[3])
     throw std::invalid_argument("A shape changes a baked normal's tangent handedness; glTF morphs cannot encode that sign");
    channels.tangents[vertex]={to[0]-from[0],to[1]-from[1],to[2]-from[2]};
   };
   for(size_t v=0;v<rest_vertices.size();++v){
    if((v&1023)==0)stop(limits);
    channels.normals.push_back(shaped.normals[rest_vertices[v]]-mesh.normals[v]);
    if(!canonical_frames||tangent_corners_by_vertex[v]==UINT32_MAX)continue;
    write_tangent(v,shaped_tangents.at(tangent_corners_by_vertex[v]));
   }
   for(const auto &range:retained_shapes){
    const auto &field=plan->fields()[range.field];
    const auto &original=field.source->source_preview();
    const auto triangles=field.domain->triangles();
    const auto current=original->with_geometry(plan->geometry(),shaped,triangles,limits.cancelled);
    if(current==original){
     std::fill_n(channels.normals.begin()+range.first_vertex,triangles.size()*3,Vec3{});
     continue;
    }
    const auto &output=out.fields[range.output];
    const auto coordinates=std::span(out.normal_coordinates).subspan(range.first_vertex,output.count);
    const auto corners=portable_texture_set_shape(*current,triangles,world,
        coordinates,output.textures,range.reprojected,work,limits);
    for(size_t c=0;c<corners.size();++c){
     if((c&1023)==0)stop(limits);
     const auto v=range.first_vertex+c;
     channels.normals[v]=corners[c].normal-mesh.normals[v];
     if(output.normal_role)write_tangent(v,corners[c].tangent);
    }
   }
  }
 }
 if(skin_check){
  const auto &deformation=*input.deformation;
  const auto posed=deformation.preview(rest);
  const auto matrices=deformation.skeleton->skin_matrices(deformation.pose);
  std::vector<RigMatrix> normal_matrices;
  normal_matrices.reserve(matrices.size());
  for(const auto &matrix:matrices){
   stop(limits);
   normal_matrices.push_back(source_frame_inverse_transpose(matrix));
  }
  for(const auto &range:retained_shapes){
   stop(limits);
   const auto &field=plan->fields()[range.field];
   const auto triangles=field.domain->triangles();
   const auto posed_field=field.source->source_preview()->with_geometry(plan->geometry(),posed,triangles,limits.cancelled);
   const auto &output=out.fields[range.output];
   std::vector<PortableTextureSetCorner> corners;
   corners.reserve(output.count);
   for(size_t c=0;c<output.count;++c){
    if((c&1023)==0)stop(limits);
    const auto v=mesh.indices.at(output.first+c);
    const auto &base=out.tangents[v];
    Vec3 n=mesh.normals[v],t{base[0],base[1],base[2]};
    for(size_t key=0;key<deformation.morph_weights.size();++key){
     const auto weight=deformation.morph_weights[key];
     const auto &morph=out.morph_channels[key];
     n+=morph.normals[v]*weight;
     if(output.normal_role)t+=morph.tangents[v]*weight;
    }
    Vec3 skin_n{},skin_t{};
    for(const auto influence:deformation.skin->vertices.at(rest_vertices[v])){
     skin_n+=source_frame_direction(normal_matrices.at(influence.joint),n)*influence.weight;
     if(output.normal_role)skin_t+=source_frame_direction(matrices.at(influence.joint),t)*influence.weight;
    }
    corners.push_back({skin_n,{skin_t.x,skin_t.y,skin_t.z,base[3]},
                       out.paint_coordinates[v],out.normal_coordinates[v]});
   }
   certify_portable_texture_set_skin(*posed_field,triangles,corners,world,output.textures,work,limits);
  }
 }
 mesh.validate();out.surface.id=input.id;out.surface.name=input.name;out.surface.transform=input.transform;out.surface.groups=input.groups;out.surface.affine_edit=input.affine_edit;out.surface.rig_source=input.rig_source;
 out.surface.material=out.fields[0].material;out.surface.textures=out.fields[0].textures;
 out.surface.deformation=normal_frame_remap_deformation(input.deformation,rest,mesh,rest_vertices,limits.cancelled);
 if(!normal){out.tangents.clear();out.normal_coordinates.clear();}stop(limits);return out;
}
} // namespace dome::detail
