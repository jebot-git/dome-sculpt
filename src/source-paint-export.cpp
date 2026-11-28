// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-paint-export.hpp"
#include "dome/brush.hpp"
#include "dome/material-visibility.hpp"
#include "current-colors.hpp"
#include "emission-bake-source.hpp"
#include "emission-paint-access.hpp"
#include "emission-raster-device.hpp"
#include "normal-bake-encoding.hpp"
#include "normal-bake-frames.hpp"
#include "paint-overlay-sample.hpp"
#include "source-extra-uv-authority.hpp"
#include "source-paint-pairing.hpp"
#include "source-bake-normal.hpp"
#include <limits>
#include <numeric>

namespace dome::detail {
namespace {
[[noreturn]] void portable_required() {
  throw std::invalid_argument("Painted independent source sampling requires an explicit portable material bake before standard export; the complete native project is retained");
}

struct ProofWork {
  const std::function<bool()> &cancelled;
  size_t remaining=128u*1024u*1024u;

  void stop() const {
    if(cancelled&&cancelled())throw std::runtime_error("Source paint factor proof cancelled");
  }
  void charge(size_t amount) {
    if(amount>remaining)throw std::length_error("Source paint factor proof exceeds sampling work budget");
    remaining-=amount;
  }
};

template<class Visit>
void visit_triangle_footprint(const Mesh &mesh,size_t face,int r,ProofWork &work,Visit visit) {
  // The bilinear footprint of one texel is a two-texel-wide rectangle.
  // Include a conservative margin for the viewport's FLOAT UV interpolation.
  const double radius=1+double(r)*32*std::numeric_limits<float>::epsilon();
  struct Axis { double x,y,limit; };
  work.stop();
  std::array<std::array<double,2>,3> q;
  for(size_t k=0;k<3;++k){
    const auto uv=mesh.uv[mesh.indices[face+k]];
    q[k]={double(uv[0])*r-.5,double(uv[1])*r-.5};
  }
  const auto &a=q[0],&b=q[1],&c=q[2];
  const double area=(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);
  if(!area)portable_required();
  const double sign=area>0?1:-1;
  std::array<Axis,3> axes;
  for(size_t k=0;k<3;++k){
    const auto &p=q[k],&next=q[(k+1)%3];
    const double x=sign*(next[1]-p[1]),y=sign*(p[0]-next[0]);
    axes[k]={x,y,x*p[0]+y*p[1]+radius*(std::abs(x)+std::abs(y))};
  }
  const int left=std::max(0,int(std::ceil(std::min({a[0],b[0],c[0]})-radius)));
  const int right=std::min(r-1,int(std::floor(std::max({a[0],b[0],c[0]})+radius)));
  const int bottom=std::max(0,int(std::ceil(std::min({a[1],b[1],c[1]})-radius)));
  const int top=std::min(r-1,int(std::floor(std::max({a[1],b[1],c[1]})+radius)));
  if(right<left)return;
  for(int y=bottom;y<=top;++y){
    work.stop();
    work.charge(size_t(right-left+1)*3);
    for(int x=left;x<=right;++x){
      if(std::ranges::any_of(axes,[&](const auto &axis){return axis.x*x+axis.y*y>axis.limit;}))continue;
      if(!visit(size_t(y)*r+x))return;
    }
  }
}

template<class Visit>
void visit_footprint(const Mesh &mesh,const PaintOverlayPacket &packet,ProofWork &work,Visit visit) {
  const auto &padding=*packet.padding();
  std::vector<bool> examined(padding.size()+1);
  bool visited=false;
  for(size_t face=0;face<mesh.indices.size();face+=3) {
    bool complete=true;
    visit_triangle_footprint(mesh,face,int(packet.resolution()),work,[&](size_t at){
      const int32_t owner=padding.at(at);
      const size_t key=size_t(owner+1);
      if(examined.at(key))return true;
      examined[key]=true;
      const auto sample=owner<0?PaintOverlayTexel{}:sample_overlay_texel_owner(packet,size_t(owner),true);
      work.charge(sample.work+1);
      visited=true;
      return complete=visit(sample.values);
    });
    if(!complete)return;
  }
  if(!visited)portable_required();
}

std::optional<float> constant_source_channel(const Image &image,unsigned channel,bool srgb,ProofWork &work) {
  const auto first=image.rgba[channel];
  constexpr size_t block_bytes=16384;
  for(size_t at=channel;at<image.rgba.size();at+=4){
    if(at%block_bytes==channel){
      work.charge(std::min(block_bytes,image.rgba.size()-(at-channel))/4);
      work.stop();
    }
    if(image.rgba[at]!=first)return {};
  }
  return srgb?overlay_linear_byte(first):first/255.f;
}

float constant_channel(const Image &image,unsigned channel,bool srgb,ProofWork &work) {
  const auto value=constant_source_channel(image,channel,srgb,work);
  if(!value)portable_required();
  return *value;
}

struct UniformChannel {
  float *factor=nullptr;
  float coverage=0,addition=0;
};

struct UniformMask {
  const ObjectSurface &object;
  Material &material;
};

void transport_mask_cutoff(const UniformMask &mask,const Image &image,float coverage,float addition,ProofWork &work) {
  auto &material=mask.material;
  auto &factor=material.color[3];
  auto &cutoff=material.alpha_cutoff;
  if(cutoff==0){factor=1;return;}

  // Use the same REST color stream as the glTF writer. Only indexed
  // vertices can contribute alpha to this material's triangles.
  const auto colors=current_vertex_colors(mask.object,work.cancelled);
  float minimum_vertex=1,maximum_vertex=colors.empty()?1:0;
  if(!colors.empty()){
    work.charge(mask.object.mesh.indices.size());
    size_t i=0;
    for(const auto vertex:mask.object.mesh.indices){
      if((i++&1023)==0)work.stop();
      const auto alpha=colors.at(vertex)[3];
      if(!std::isfinite(alpha)||alpha<0||alpha>1)portable_required();
      minimum_vertex=std::min(minimum_vertex,alpha);
      maximum_vertex=std::max(maximum_vertex,alpha);
    }
  }
  if(maximum_vertex==0){factor=0;return;}
  const double scale=double(factor)*(1.f-coverage);
  if(minimum_vertex==maximum_vertex){
    // Native visibility compares (scale * sourceAlpha + addition) * vertexAlpha
    // with the cutoff. Move the threshold into sourceAlpha * vertexAlpha.
    const double transported=(double(cutoff)-double(addition)*minimum_vertex)/scale;
    if(transported<=minimum_vertex&&float(transported)>0){
      cutoff=float(transported);
      factor=1;
      return;
    }
  }

  // When vertex alpha varies, a single translated crossing is insufficient.
  // Every wrapped, filtered or mipmapped source alpha remains in the
  // original texel envelope. Preserve the result if that full envelope is
  // on one side of the cutoff, with room for FLOAT compositing roundoff.
  uint8_t low=255,high=0;
  work.charge(image.rgba.size()/4);
  for(size_t at=3;at<image.rgba.size();at+=4){
    if((at&16383)==3)work.stop();
    low=std::min(low,image.rgba[at]);
    high=std::max(high,image.rgba[at]);
  }
  constexpr double roundoff=16*std::numeric_limits<float>::epsilon();
  const double lower=(scale*(double(low)/255)+addition)*minimum_vertex;
  const double upper=(scale*(double(high)/255)+addition)*maximum_vertex;
  if(lower>=double(cutoff)+roundoff){factor=1;cutoff=0;return;}
  if(upper<double(cutoff)-roundoff){factor=0;return;}
  portable_required();
}

void rewrite_uniform_image(PaintSourceTexture &texture,const SourceUvSampler::TexelAffine &affine,
    bool srgb,ProofWork &work) {
  const auto &original=*texture.image;
  const auto pixels=original.rgba.size()/4;
  work.charge(pixels*std::popcount(affine.channels));
  const auto capacity=std::min(pixels,SourceUvSampler::max_batch);
  work.stop();
  auto sampler=upload_source_uv_sampler(texture,srgb,SourceUvSampler::Output::EncodedRgba,capacity);
  auto image=std::make_shared<Image>(Image{original.width,original.height,std::vector<uint8_t>(original.rgba.size())});
  std::vector<uint32_t> encoded(capacity);
  for(size_t first=0;first<pixels;first+=capacity){
    work.stop();
    const auto count=std::min(capacity,pixels-first);
    auto batch=std::span(encoded).first(count);
    sampler->rewrite(first,affine,batch);
    for(size_t i=0;i<count;++i)for(unsigned c=0;c<4;++c)
      image->rgba[4*(first+i)+c]=uint8_t(batch[i]>>(8*c));
  }
  work.stop();
  texture.image=std::move(image);
}

void fold_uniform_image(PaintSourceTexture &texture,const std::array<UniformChannel,4> &channels,
    bool srgb,const UniformMask *mask,ProofWork &work) {
  SourceUvSampler::TexelAffine affine;
  bool keep_image=false;
  for(unsigned c=0;c<channels.size();++c){
    const auto &channel=channels[c];
    if(!channel.factor)continue;
    auto &factor=*channel.factor;
    const float scale=factor*(1-channel.coverage);
    const float offset=channel.addition;
    if(!texture.image){factor=scale+offset;continue;}
    if(offset==0&&(scale!=0||channel.coverage==0)){
      // Use the old factor and texels' exact encoding for scaling alone.
      factor=scale;
      keep_image=true;
      continue;
    }
    auto original=scale==0?std::optional<float>(0):
        constant_source_channel(*texture.image,c,srgb&&c<3,work);
    if(original){
      affine.channels|=1u<<c;
      factor=factor*(*original)*(1-channel.coverage)+offset;
      affine.offset[c]=1;
      continue;
    }
    if(mask&&c==3){
      transport_mask_cutoff(*mask,*texture.image,channel.coverage,offset,work);
      keep_image=true;
      continue;
    }
    affine.channels|=1u<<c;
    factor=scale+offset;
    if(factor>1)portable_required();
    affine.scale[c]=scale/factor;
    affine.offset[c]=offset/factor;
    keep_image=true;
  }
  if(!keep_image)texture.image.reset();
  else if(affine.channels)rewrite_uniform_image(texture,affine,srgb,work);
}

struct OverlayRange {
  PaintOverlayValues low,high;

  bool varies(unsigned plane,unsigned component) const {
    return low[plane][component]!=high[plane][component];
  }
};

OverlayRange overlay_range(const Mesh &mesh,const PaintOverlayPacket &packet,ProofWork &work,
                           bool allow_atlas) {
  std::optional<OverlayRange> range;
  visit_footprint(mesh,packet,work,[&](const auto &sample){
    if(!range)range=OverlayRange{sample,sample};
    else{
      if(!allow_atlas&&sample!=range->low)portable_required();
      for(unsigned p=0;p<sample.size();++p)for(unsigned c=0;c<4;++c){
        range->low[p][c]=std::min(range->low[p][c],sample[p][c]);
        range->high[p][c]=std::max(range->high[p][c],sample[p][c]);
      }
    }
    return true;
  });
  return *range;
}

float atlas_background(const PaintSourceTexture &view,unsigned component,bool srgb,float factor,
                       float minimum_coverage,ProofWork &work) {
  if(factor==0||minimum_coverage==1)return 0;
  return view.image?factor*constant_channel(*view.image,component,srgb,work):factor;
}

uint8_t unorm8(float value) {
  return uint8_t(std::lround(std::clamp(value,0.f,1.f)*255));
}

template<size_t N>
void fold_color(PaintSourceTexture &,std::array<float,N> &,const std::array<float,4> &,float,
                ProofWork &,bool rewrite=false,const UniformMask *mask=nullptr);
void fold_orm(PaintSourceTexture &,Material &,const PaintOverlayValues &,ProofWork &,bool rewrite=false);

// Preserve masked coverage exactly. A constant alpha can be carried outside
// the texture even when its value falls between two PNG alpha bytes.
struct AtlasAlpha { std::optional<float> constant;bool transparent=false; };
std::optional<AtlasAlpha> atlas_alpha(const PaintedMesh &paint,const PaintOverlayPacket &packet,
    float background,bool masked,ProofWork &work) {
  std::optional<float> constant;
  bool uniform=true,unorm=true,transparent=false;
  visit_footprint(paint.mesh(),packet,work,[&](const auto &sample){
    const float alpha=background*(1-sample[3][0])+sample[0][3];
    if(!constant)constant=alpha;
    else uniform&=alpha==*constant;
    unorm&=alpha==unorm8(alpha)/255.f;
    transparent|=alpha<1;
    return masked?(uniform||unorm):(uniform||!transparent);
  });
  if(masked&&!uniform&&!unorm)return std::nullopt;
  return AtlasAlpha{uniform?constant:std::nullopt,transparent};
}

// A clamp sampler without mip levels can narrow the source alpha interval
// to the original texture taps reachable by this triangle's affine UVs.
std::optional<std::array<uint8_t,2>> clamped_source_alpha(const PaintSourceTexture &view,
    std::span<const std::array<float,2>> uv,std::span<const uint32_t> vertices,ProofWork &work) {
  const auto &image=*view.image;
  const auto &t=view.uv_transform;
  std::array<int,2> first,last;
  for(size_t axis=0;axis<2;++axis){
    double lo=1,hi=0;
    const int size=axis==0?image.width:image.height;
    for(const auto vertex:vertices){
      const auto &q=uv[vertex];
      const double a=double(t[axis*2])*q[0],b=double(t[axis*2+1])*q[1],d=t[4+axis];
      const double error=64*std::numeric_limits<float>::epsilon()*(1+std::abs(a)+std::abs(b)+std::abs(d));
      lo=std::min(lo,std::clamp(a+b+d-error,0.,1.));
      hi=std::max(hi,std::clamp(a+b+d+error,0.,1.));
    }
    // Enclose both nearest and linear taps, including sampler subtexel rounding.
    first[axis]=std::max(0,int(std::floor(lo*size-1.5)));
    last[axis]=std::min(size-1,int(std::floor(hi*size+1.5)));
  }
  const size_t count=size_t(last[0]-first[0]+1)*(last[1]-first[1]+1);
  if(count>4096)return std::nullopt;
  work.charge(count+6);
  std::array<uint8_t,2> result{255,0};
  for(int y=first[1];y<=last[1];++y)
    for(int x=first[0];x<=last[0];++x){
      const auto alpha=image.rgba[(size_t(y)*image.width+x)*4+3];
      result[0]=std::min(result[0],alpha);
      result[1]=std::max(result[1],alpha);
    }
  return result;
}

// The native shader filters source alpha, coverage and premultiplied paint
// separately. Use the source interval and the extrema of all writable taps
// touching each triangle to bound both native and emitted alpha.
bool certify_baked_mask(const ObjectSurface &out,const PaintOverlayPacket &packet,
    const PaintSourceMaps &maps,const Image &encoded,Material &material,ProofWork &work) {
  if(material.alpha_cutoff==0)return true;
  const auto &view=maps.color;
  std::array<uint8_t,2> all{255,255};
  const bool textured=view.image&&material.color[3]!=0;
  if(textured){
    const auto &rgba=view.image->rgba;
    all[1]=0;
    work.charge(rgba.size()/4);
    for(size_t at=3;at<rgba.size();at+=4){
      if((at&16383)==3)work.stop();
      all[0]=std::min(all[0],rgba[at]);
      all[1]=std::max(all[1],rgba[at]);
    }
  }
  const bool local=textured&&all[0]!=all[1]&&view.wrap_s==33071&&view.wrap_t==33071&&
      (view.min_filter==9728||view.min_filter==9729);
  const auto source_uv=local?source_texture_coordinates(out.mesh,maps,view.texcoord):
      std::span<const std::array<float,2>>{};
  const auto colors=current_vertex_colors(out,work.cancelled);
  const auto &padding=*packet.padding();
  constexpr double slack=64*std::numeric_limits<float>::epsilon();
  std::optional<double> visible_floor,hidden_ceiling;
  for(size_t face=0;face<out.mesh.indices.size();face+=3){
    work.stop();
    const auto vertices=std::span(out.mesh.indices).subspan(face,3);
    const auto alpha_range=local?clamped_source_alpha(view,source_uv,vertices,work).value_or(all):all;
    const double source_low=double(alpha_range[0])/255*material.color[3];
    const double source_high=double(alpha_range[1])/255*material.color[3];
    double vertex_low=1,vertex_high=colors.empty()?1:0;
    if(!colors.empty())for(size_t k=0;k<3;++k){
      const double alpha=colors.at(out.mesh.indices[face+k])[3];
      if(!std::isfinite(alpha)||alpha<0||alpha>1)portable_required();
      vertex_low=std::min(vertex_low,alpha);
      vertex_high=std::max(vertex_high,alpha);
    }
    double native_low=1,native_high=0,encoded_low=1,encoded_high=0;
    bool reached=false;
    visit_triangle_footprint(out.mesh,face,int(packet.resolution()),work,[&](size_t at){
      const auto owner=padding.at(at);
      const auto sample=owner<0?PaintOverlayTexel{}:sample_overlay_texel_owner(packet,size_t(owner),true);
      work.charge(sample.work+1);
      const double uncovered=1.-sample.values[3][0],addition=sample.values[0][3];
      native_low=std::min(native_low,source_low*uncovered+addition);
      native_high=std::max(native_high,source_high*uncovered+addition);
      const double alpha=double(encoded.rgba.at(at*4+3))/255;
      encoded_low=std::min(encoded_low,alpha);
      encoded_high=std::max(encoded_high,alpha);
      reached=true;
      return true;
    });
    if(!reached)portable_required();
    if(native_low*vertex_low>=material.alpha_cutoff+slack){
      const double limit=encoded_low*vertex_low-slack;
      visible_floor=std::min(visible_floor.value_or(limit),limit);
    }else if(native_high*vertex_high<material.alpha_cutoff-slack){
      const double limit=encoded_high*vertex_high+slack;
      hidden_ceiling=std::max(hidden_ceiling.value_or(limit),limit);
    }else portable_required();
  }
  if(!visible_floor)return hidden_ceiling&&*hidden_ceiling<material.alpha_cutoff;
  if(!hidden_ceiling){
    if(*visible_floor<material.alpha_cutoff)material.alpha_cutoff=0;
    return true;
  }
  const auto separates=[&](float cutoff){return cutoff>*hidden_ceiling&&cutoff<=*visible_floor;};
  if(!separates(material.alpha_cutoff)){
    const auto cutoff=float(std::midpoint(*hidden_ceiling,*visible_floor));
    if(!separates(cutoff))portable_required();
    material.alpha_cutoff=cutoff;
  }
  return true;
}

// The source contribution in each rewritten channel is either constant or
// fully covered across the triangle footprint. Composition therefore
// commutes with the atlas' linear filter. Quantizing the individual taps
// bounds the resulting channel error by the largest tap error.
struct PaintAtlas {
  std::shared_ptr<Image> color,orm,emission;
  bool transparent=false;
  std::shared_ptr<Image> normal{};
  bool includes_occlusion=false;
};

void prepare_baked_export_frame(const SculptObject &owner,ObjectSurface &out,
    const std::function<bool()> &cancelled) {
  if(owner.painted->baked()->maps.normal.rgba.empty())return;
  const auto binding=owner.material_bindings;
  const auto frames=binding?binding->normal_frames:nullptr;
  if(frames&&frames->pbr&&!frames->pbr->tangents.empty())return;
  if(!frames&&owner.appearance)
    throw std::invalid_argument("Prepared HDR Bake lost its original normal attribute owner");
  // Generate only the outbound frame, before remapping the surface UVs.
  // The existing emitter splits its actual per-corner tangents and key deltas.
  NormalBakeFrameOptions options;
  options.cancelled=cancelled;
  auto candidate=normal_bake_frame_candidate(owner.painted->mesh(),owner.deformation.get(),binding.get(),options);
  auto staged=binding?std::make_shared<SourceMaterialBindings>(*binding):std::make_shared<SourceMaterialBindings>();
  staged->has_mtoon=false;
  staged->normal_frame=SourceMaterialNormalFrame::OriginalAttributes;
  staged->normal_frames=std::move(candidate.frames);
  if(!frames){
    auto origin=std::make_shared<SourceUvOrigin>();
    origin->paint=owner.painted;
    origin->deformation=owner.deformation;
    staged->uv_origin=std::move(origin);
  }
  staged->validate();
  out.material_bindings=std::move(staged);
}

std::shared_ptr<Image> baked_normal_atlas(const SculptObject &owner,const ObjectSurface &out,
    std::span<const int32_t> padding,ProofWork &work) {
  const auto &paint=*owner.painted;
  if(paint.baked()->maps.normal.rgba.empty())return {};
  const auto frames=out.material_bindings?out.material_bindings->normal_frames:nullptr;
  if(!frames||!frames->pbr||frames->pbr->tangents.empty())
    throw std::invalid_argument("Prepared HDR Bake normals require the source's emitted tangent frame");
  // Encode the finite baked normal into the same frame glTF emits. The source
  // normal may use a different UV derivative and the native Bake may use Mikk
  // on the writable atlas. Neither input vector can be copied verbatim.
  auto rest=owner;
  if(!rest.material_bindings||!rest.material_bindings->normal_frames)rest.material_bindings=out.material_bindings;
  if(rest.deformation){
    auto deformation=std::make_shared<DeformationState>(*rest.deformation);
    deformation->morph_weights.clear();
    deformation->pose.clear();
    rest.deformation=std::move(deformation);
  }
  const auto sample=source_bake_normals(rest,{},SourceBakeNormalOptions{128u*1024u*1024u,work.cancelled});
  if(!sample)throw std::invalid_argument("Prepared HDR Bake lost its actual native normal resolver");
  const auto &frame=*frames->pbr;
  const auto r=paint.resolution();
  auto normal=std::make_shared<Image>(Image{r,r,std::vector<uint8_t>(padding.size()*4)});
  uint32_t prior=UINT32_MAX;
  std::array<NormalBakeVertex,3> columns;
  for(size_t at=0;at<padding.size();++at){
    work.charge(1);
    if((at&1023)==0)work.stop();
    if(padding[at]!=int32_t(at))continue;
    const auto [face,bary]=EmissionExportAccess::writable_sample(paint,at);
    if(face!=prior){
      for(size_t c=0;c<3;++c){
        const size_t corner=size_t(face)*3+c;
        const auto &t=frame.tangents.at(corner);
        columns[c]=normal_bake_vertex(frame.normals.at(corner),{t[0],t[1],t[2]},t[3],NormalVertexBitangent::UnitCross);
      }
      prior=face;
    }
    const auto basis=normal_bake_basis(columns,{bary[0],bary[1],bary[2]},NormalFragmentBasis::UnitColumns);
    const auto encoded=encode_normal_bake(basis,sample(face,bary)[0]);
    if(!encoded||encoded->quantized_angle_radians>.02)
      throw std::invalid_argument("Prepared HDR Bake cannot encode its normal in the emitted tangent frame");
    std::copy(encoded->rgba.begin(),encoded->rgba.end(),normal->rgba.begin()+at*4);
  }
  return normal;
}

PaintAtlas compile_baked_hdr_atlas(const SculptObject &owner,const ObjectSurface &out,
    const PaintOverlayPacket &packet,Material &material,const std::function<bool()> &cancelled) {
  const auto &paint=*owner.painted;
  const auto &maps=*paint.source_maps();
  const auto padding=EmissionExportAccess::writable_padding(paint);
  const auto r=paint.resolution();
  ProofWork work{cancelled};
  work.charge(padding.size()*6);
  AtlasAlpha alpha;
  bool prove_mask=false;
  if(material.alpha==Material::AlphaMode::Mask){
    const auto original=maps.color.image&&material.color[3]!=0?
        constant_source_channel(*maps.color.image,3,false,work):std::optional(1.f);
    const auto exact=original?atlas_alpha(paint,packet,material.color[3]*(*original),true,work):std::nullopt;
    if(exact)alpha=*exact;
    else prove_mask=true;
  }
  const auto image=[&]{return std::make_shared<Image>(Image{r,r,std::vector<uint8_t>(padding.size()*4)});};
  PaintAtlas atlas{image(),image(),{},alpha.transparent,baked_normal_atlas(owner,out,padding,work),true};
  const auto store=[&](size_t at,const paint_compute::Output &pixel){
    auto *rgba=atlas.color->rgba.data()+at*4;
    std::copy_n(pixel.color,4,rgba);
    if(alpha.constant)rgba[3]=255;
    std::copy_n(pixel.orm,4,atlas.orm->rgba.data()+at*4);
    atlas.transparent|=!alpha.constant&&pixel.transparent;
  };
  const SourceMaterialRaster::Roles roles{maps.color.image?&maps.color:nullptr,
    maps.metal_rough.image?&maps.metal_rough:nullptr,nullptr,maps.occlusion.image?&maps.occlusion:nullptr};
  std::array<std::span<const std::array<float,2>>,paint_compute::SourceRoleCount> uv;
  for(size_t i=0;i<roles.size();++i)if(roles[i])uv[i]=source_texture_coordinates(paint.mesh(),maps,roles[i]->texcoord);
  if(compute_backend()!="CPU"){
    constexpr size_t working_bytes=256u*1024u*1024u;
    auto device=create_source_material_raster(paint,material,roles,maps.occlusion_strength,r,
        working_bytes-padding.size()*(atlas.normal?12:8),cancelled,false);
    std::vector<uint32_t> texels;
    texels.reserve(device->capacity());
    std::vector<paint_compute::Output> output(device->capacity());
    const auto flush=[&]{
      if(texels.empty())return;
      device->read_material(texels,[&](unsigned role,std::span<SourceUvSampler::Coordinate> coordinates){
        for(size_t i=0;i<texels.size();++i){
          if((i&1023)==0)work.stop();
          const auto q=EmissionExportAccess::writable_source_uv(paint,uv[role],texels[i]);
          coordinates[i]={q[0],q[1]};
        }
      },std::span(output).first(texels.size()),cancelled);
      for(size_t i=0;i<texels.size();++i)store(texels[i],output[i]);
      texels.clear();
    };
    for(size_t at=0;at<padding.size();++at){
      if((at&1023)==0)work.stop();
      if(padding[at]!=int32_t(at))continue;
      texels.push_back(uint32_t(at));
      if(texels.size()==device->capacity())flush();
    }
    flush();
  }else{
    // Independent headless reference. Production builds use the GPU above.
    auto resolved=std::array{maps.color,maps.metal_rough,maps.emission,maps.occlusion};
    for(auto &role:resolved)role.texcoord=0;
    for(size_t at=0;at<padding.size();++at){
      if((at&1023)==0)work.stop();
      if(padding[at]!=int32_t(at))continue;
      auto source=[&](unsigned role,bool srgb){
        if(!roles[role])return std::array{1.f,1.f,1.f,1.f};
        const auto q=EmissionExportAccess::writable_source_uv(paint,uv[role],at);
        return sample_source_texture(resolved[role],q[0],q[1],srgb);
      };
      const auto [value,operations]=sample_overlay_texel_owner(packet,at,true);
      work.charge(operations);
      const auto color=source(0,true),orm=source(1,false),ao=source(3,false);
      paint_compute::Output pixel;
      for(size_t k=0;k<4;++k){
        const float c=material.color[k]*color[k]*(1-value[3][0])+value[0][k];
        pixel.color[k]=unorm8(k<3?linear_to_srgb(c):c);
        if(k==3)pixel.transparent=c<.999f;
      }
      pixel.orm[0]=unorm8((1-maps.occlusion_strength*(1-ao[0]))*(1-value[1][3])+value[1][0]);
      pixel.orm[1]=unorm8(material.roughness*orm[1]*(1-value[3][2])+value[1][1]);
      pixel.orm[2]=unorm8(material.metallic*orm[2]*(1-value[3][1])+value[1][2]);
      pixel.orm[3]=255;
      store(at,pixel);
    }
  }
  for(size_t at=0;at<padding.size();++at){
    if((at&1023)==0)work.stop();
    const auto from=padding[at];
    if(from<0||size_t(from)==at)continue;
    for(auto *image:{atlas.color.get(),atlas.orm.get(),atlas.normal.get()})if(image)
      std::copy_n(image->rgba.data()+size_t(from)*4,4,image->rgba.data()+at*4);
  }
  if(prove_mask&&!certify_baked_mask(out,packet,maps,*atlas.color,material,work))
    alpha.constant=0;
  material.color={1,1,1,alpha.constant.value_or(1)};
  material.roughness=material.metallic=1;
  return atlas;
}

PaintAtlas compile_atlas(const PaintedMesh &paint,const ObjectSurface &object,const PaintOverlayPacket &packet,
    const OverlayRange &range,PaintSourceMaps &source,Material &material,ProofWork &work) {
  const auto &value=range.low;
  const bool color=range.high[3][0]!=0&&(range.varies(3,0)||range.low[0]!=range.high[0]);
  const bool orm=(range.high[3][1]!=0||range.high[3][2]!=0)&&
      (range.varies(3,1)||range.varies(3,2)||range.low[1]!=range.high[1]);
  const bool emission=range.high[3][3]!=0&&(range.varies(3,3)||range.low[2]!=range.high[2]);
  Material background;
  background.color={};
  background.roughness=background.metallic=0;
  AtlasAlpha alpha;
  if(color){
    for(unsigned c=0;c<3;++c)
      background.color[c]=atlas_background(source.color,c,true,material.color[c],value[3][0],work);
    if(material.alpha==Material::AlphaMode::Opaque)alpha.constant=1;
    else{
      background.color[3]=atlas_background(source.color,3,false,material.color[3],value[3][0],work);
      const auto exact=atlas_alpha(paint,packet,background.color[3],material.alpha==Material::AlphaMode::Mask,work);
      if(!exact)portable_required();
      alpha=*exact;
    }
    material.color={1,1,1,alpha.constant.value_or(1)};
  }else if(range.high[3][0]!=0){
    const UniformMask mask{object,material};
    fold_color(source.color,material.color,value[0],value[3][0],work,true,
               material.alpha==Material::AlphaMode::Mask?&mask:nullptr);
  }
  if(orm){
    background.roughness=atlas_background(source.metal_rough,1,false,material.roughness,value[3][2],work);
    background.metallic=atlas_background(source.metal_rough,2,false,material.metallic,value[3][1],work);
    material.roughness=material.metallic=1;
  }else if(range.high[3][1]!=0||range.high[3][2]!=0){
    fold_orm(source.metal_rough,material,value,work,true);
  }
  if(emission){
    for(unsigned c=0;c<3;++c)
      background.emissive[c]=atlas_background(source.emission,c,true,material.emissive[c],value[3][3],work);
    material.emissive={1,1,1};
  }else if(range.high[3][3]!=0){
    fold_color(source.emission,material.emissive,value[2],value[3][3],work,true);
  }
  if(std::ranges::any_of(material.emissive,[](float v){return v>1;}))portable_required();
  // Only emitted images need final byte storage. The device also produces
  // the auxiliary coverage fields for the same existing compositor kernel.
  const auto r=packet.resolution();
  const size_t pixels=size_t(r)*r;
  const size_t images=size_t(color)+size_t(orm)+size_t(emission);
  constexpr size_t maximum_bytes=256*1024*1024;
  work.charge(pixels*images);
  auto image=[&](bool enabled){
    return enabled?std::make_shared<Image>(Image{r,r,std::vector<uint8_t>(pixels*4)}):nullptr;
  };
  PaintAtlas result{image(color),image(orm),image(emission),alpha.transparent};
  const auto store=[&](size_t at,const paint_compute::Output &pixel){
    if(result.color){
      auto *rgba=result.color->rgba.data()+4*at;
      std::copy_n(pixel.color,4,rgba);
      if(alpha.constant)rgba[3]=255;
    }
    if(result.orm)std::copy_n(pixel.orm,4,result.orm->rgba.data()+4*at);
    if(result.emission)std::copy_n(pixel.emission,4,result.emission->rgba.data()+4*at);
  };
  if(compute_backend()!="CPU"){
    auto device=create_source_material_raster(paint,background,{},1,r,maximum_bytes-pixels*images*4,work.cancelled);
    std::vector<uint32_t> texels(device->capacity());
    std::vector<paint_compute::Output> output(device->capacity());
    for(size_t first=0;first<pixels;first+=device->capacity()){
      work.stop();
      const auto count=std::min(device->capacity(),pixels-first);
      for(size_t i=0;i<count;++i)texels[i]=uint32_t(first+i);
      device->read_material(std::span(texels).first(count),{},std::span(output).first(count),work.cancelled);
      for(size_t i=0;i<count;++i)store(first+i,output[i]);
    }
  }else{
    // The opt-in headless reference uses the independent scalar sampler.
    for(size_t at=0;at<pixels;++at){
      if((at&1023)==0)work.stop();
      const auto sample=sample_overlay_texel_values(packet,int(at%r),int(at/r),true);
      paint_compute::Output pixel;
      for(unsigned c=0;c<4;++c){
        const float v=background.color[c]*(1-sample[3][0])+sample[0][c];
        pixel.color[c]=unorm8(c==3?v:linear_to_srgb(v));
      }
      pixel.orm[0]=pixel.orm[3]=pixel.emission[3]=255;
      pixel.orm[1]=unorm8(background.roughness*(1-sample[3][2])+sample[1][1]);
      pixel.orm[2]=unorm8(background.metallic*(1-sample[3][1])+sample[1][2]);
      for(unsigned c=0;c<3;++c){
        const float v=background.emissive[c]*(1-sample[3][3])+sample[2][c];
        if(v>1)portable_required();
        pixel.emission[c]=unorm8(linear_to_srgb(v));
      }
      store(at,pixel);
    }
  }
  return result;
}

template<size_t N>
void fold_color(PaintSourceTexture &texture,std::array<float,N> &factor,
    const std::array<float,4> &addition,float coverage,ProofWork &work,bool rewrite,const UniformMask *mask) {
  if(std::ranges::all_of(std::span(addition).first(N),[](float value){return value==0;})){
    for(auto &value:factor)value*=1-coverage;
    return;
  }
  if(rewrite){
    std::array<UniformChannel,4> channels{};
    for(unsigned c=0;c<N;++c)channels[c]={&factor[c],coverage,addition[c]};
    fold_uniform_image(texture,channels,true,mask,work);
    return;
  }
  for(unsigned channel=0;channel<N;++channel){
    const float original=texture.image&&coverage!=1&&factor[channel]!=0
      ?constant_channel(*texture.image,channel,channel<3,work):1;
    factor[channel]=factor[channel]*original*(1-coverage)+addition[channel];
  }
  texture.image.reset();
}

void fold_orm(PaintSourceTexture &texture,Material &material,const PaintOverlayValues &value,ProofWork &work,
    bool rewrite) {
  if(rewrite&&(value[1][1]!=0||value[1][2]!=0)){
    std::array<UniformChannel,4> channels{};
    channels[1]={&material.roughness,value[3][2],value[1][1]};
    channels[2]={&material.metallic,value[3][1],value[1][2]};
    fold_uniform_image(texture,channels,false,nullptr,work);
    return;
  }
  std::array<bool,4> neutral{};
  const auto channel=[&](unsigned index,float &factor,float coverage,float addition){
    if(addition==0){factor*=1-coverage;return;}
    const float original=texture.image&&coverage!=1&&factor!=0
      ?constant_channel(*texture.image,index,false,work):1;
    factor=factor*original*(1-coverage)+addition;
    neutral[index]=true;
  };
  channel(1,material.roughness,value[3][2],value[1][1]);
  channel(2,material.metallic,value[3][1],value[1][2]);
  if(!texture.image)return;
  if(neutral[1]&&neutral[2])texture.image.reset();
  else if(neutral[1]||neutral[2]){
    work.charge(texture.image->rgba.size()/4);
    auto copy=std::make_shared<Image>(*texture.image);
    const size_t component=neutral[1]?1:2;
    for(size_t at=component;at<copy->rgba.size();at+=4){
      if((at&16383)==component)work.stop();
      copy->rgba[at]=255;
    }
    texture.image=std::move(copy);
  }
}
PaintAtlas compile_source_paint(const PaintedMesh &paint,const ObjectSurface &object,
    const PaintOverlayPacket &packet,PaintSourceMaps &sources,Material &material,
    const std::function<bool()> &cancelled,bool allow_atlas) {
  if(packet.baked()||object.mesh.uv!=paint.mesh().uv||object.mesh.indices!=paint.mesh().indices)
    portable_required();
  ProofWork work{cancelled};
  const auto range=overlay_range(paint.mesh(),packet,work,allow_atlas);
  const auto &value=range.low;
  if(range.high==PaintOverlayValues{})return {};
  if((object.material_bindings&&object.material_bindings->has_mtoon)||
      retained_mtoon(object.appearance.get())||paint.color_projection()||paint.normal_projection()||
      overlay_uses_float_emission(packet)||range.high[1][3]!=0)portable_required();
  if(value!=range.high){
    if(object.material.toon)portable_required();
    return compile_atlas(paint,object,packet,range,sources,material,work);
  }
  const bool rewrite=allow_atlas&&!object.material.toon;
  const UniformMask mask{object,material};
  fold_color(sources.color,material.color,value[0],value[3][0],work,rewrite,
             material.alpha==Material::AlphaMode::Mask?&mask:nullptr);
  fold_color(sources.emission,material.emissive,value[2],value[3][3],work,rewrite);
  if(std::ranges::any_of(material.emissive,[](float channel){return channel>1;}))portable_required();
  fold_orm(sources.metal_rough,material,value,work,rewrite);
  material.validate();
  work.stop();
  return {};
}

bool uses_coordinate(const PaintSourceMaps &source,unsigned set) {
  for(const auto *role:{&source.color,&source.metal_rough,&source.emission,&source.normal,&source.occlusion})
    if(role->image&&role->texcoord==int(set))return true;
  return false;
}

unsigned adopt_atlas_coordinates(PaintSourceMaps &source,const ObjectSurface &out,
    const std::vector<std::array<float,2>> &original_uv0,size_t maximum_bytes) {
  if(out.mesh.uv==original_uv0)return 0;
  if(source.uv1&&*source.uv1==out.mesh.uv)return 1;
  for(const auto &[set,uv]:source.extra_uv_sets)if(*uv==out.mesh.uv)return set;
  std::array<bool,8> present{};
  present[0]=true;
  present[1]=bool(source.uv1);
  for(const auto &[set,uv]:source.extra_uv_sets)present.at(set)=true;
  if(const auto &appearance=out.appearance){
    if(!appearance->asset)portable_required();
    for(const auto &primitive:appearance->asset->primitives)
      if(primitive.source_mesh==appearance->source_mesh&&primitive.source_primitive==appearance->source_primitive){
        present[1]|=!primitive.uv1.empty();
        for(const auto &[set,uv]:primitive.extra_uv_sets)present.at(set)=true;
        break;
      }
  }
  const auto first=std::find(present.begin(),present.end(),false);
  if(first==present.end()||std::find(first,present.end(),true)!=present.end())portable_required();
  const auto set=unsigned(first-present.begin());
  if(out.mesh.uv.size()>maximum_bytes/((set+1)*sizeof(std::array<float,2>)))
    throw std::length_error("Source paint atlas coordinate staging exceeds its bounded resource policy");
  auto uv=std::make_shared<const std::vector<std::array<float,2>>>(out.mesh.uv);
  if(set==1)source.uv1=std::move(uv);
  else source.extra_uv_sets.emplace(set,std::move(uv));
  return set;
}

void install_atlas(PaintSourceMaps &source,PaintAtlas atlas,const ObjectSurface &out,
    const std::vector<std::array<float,2>> &uv0,size_t maximum_bytes,bool hdr) {
  // Unbound authored accessors still belong to CreatorAppearance. Keep only
  // coordinate owners actually used by the detached material bindings here.
  if(atlas.color)source.color={};
  if(atlas.orm)source.metal_rough={};
  if(atlas.emission||hdr)source.emission={};
  if(atlas.normal)source.normal={};
  if(atlas.includes_occlusion)source.occlusion={};
  if(!source.uses_uv1())source.uv1.reset();
  std::erase_if(source.extra_uv_sets,[&](const auto &entry){return !uses_coordinate(source,entry.first);});
  if(!atlas.color&&!atlas.orm&&!atlas.emission&&!atlas.normal)return;
  const auto set=adopt_atlas_coordinates(source,out,uv0,maximum_bytes);
  const auto enable=[&](PaintSourceTexture &slot,std::shared_ptr<Image> image){
    if(!image)return;
    slot.image=std::move(image);
    slot.texcoord=int(set);
    slot.wrap_s=slot.wrap_t=33071;
    slot.min_filter=slot.mag_filter=9729;
  };
  enable(source.color,std::move(atlas.color));
  enable(source.metal_rough,std::move(atlas.orm));
  enable(source.emission,std::move(atlas.emission));
  if(atlas.normal){
    enable(source.normal,std::move(atlas.normal));
    source.normal_scale=1;
  }
  if(atlas.includes_occlusion){
    source.occlusion=source.metal_rough;
    source.occlusion_strength=1;
  }
}

void compile_source_surface(const PaintedMesh &paint,ObjectSurface &out,
    const std::function<bool()> &cancelled,SourcePaintExport mode,const SculptObject *baked_owner) {
  auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Source paint export cancelled");};
  stop();
  if(!paint.source_sampling_prepared()||!paint.valid_source_sampling_attachment())
    throw std::invalid_argument("Source paint export lost its immutable original sampling owner");
  source_paint_pairing(&paint,out.material_bindings.get(),out.appearance.get(),false);
  if(out.appearance)extra_source_uv_pair(paint,*out.appearance,cancelled);
  const auto marker=out.textures;
  if(!marker||!marker->source_only||!marker->overlay||
      marker->overlay->source_maps()!=paint.source_maps())
    throw std::invalid_argument("Source paint export requires its actual captured contribution packet");
  const auto before=paint.source_sampling_original();
  const auto vertices=paint.source_sampling_vertices();
  if(!before||vertices.size()!=out.mesh.positions.size()||out.mesh.indices!=paint.mesh().indices)
    throw std::invalid_argument("Source paint export vertex domain differs from its captured owner");
  const PaintSourceMaps empty;
  const auto &original=before->source_maps()?*before->source_maps():empty;
  const size_t arrays=1+bool(original.uv1)+original.extra_uv_sets.size();
  constexpr size_t bytes_limit=128*1024*1024;
  if(vertices.size()>2'000'000||arrays>8||vertices.size()>bytes_limit/(arrays*sizeof(std::array<float,2>)))
    throw std::length_error("Source paint export coordinate staging exceeds its bounded resource policy");
  auto remap=[&](const auto &values){
    if(values.size()!=before->mesh().positions.size())
      throw std::invalid_argument("Source paint export original coordinate domain is incomplete");
    std::vector<std::array<float,2>> result;result.reserve(vertices.size());
    for(size_t v=0;v<vertices.size();++v){if((v&1023)==0)stop();result.push_back(values.at(vertices[v]));}
    return result;
  };
  auto source=std::make_shared<PaintSourceMaps>(original);
  auto material=out.material;
  PaintAtlas atlas;
  if(baked_owner)atlas=compile_baked_hdr_atlas(*baked_owner,out,*marker->overlay,material,cancelled);
  else if(!marker->overlay->empty())
    atlas=compile_source_paint(paint,out,*marker->overlay,*source,material,cancelled,
                               mode!=SourcePaintExport::Factors);
  if(original.uv1)source->uv1=std::make_shared<const std::vector<std::array<float,2>>>(remap(*original.uv1));
  for(auto &[set,values]:source->extra_uv_sets)
    values=std::make_shared<const std::vector<std::array<float,2>>>(remap(*values));
  auto uv0=remap(before->mesh().uv);
  const bool transparent=atlas.transparent;
  install_atlas(*source,std::move(atlas),out,uv0,bytes_limit,mode==SourcePaintExport::StandardHdr);
  out.mesh.uv=std::move(uv0);
  // The pins and seams describe the native atlas editor.
  out.mesh.uv_pins.clear();
  out.mesh.uv_seams.clear();
  source->validate_surface(out.mesh);
  material.validate();
  auto textures=std::make_shared<TextureMaps>();
  textures->source_only=true;
  textures->source_maps=std::move(source);
  textures->transparent=marker->transparent||transparent||
      (material.color[3]!=out.material.color[3]&&out.material.color[3]<1);
  out.material=std::move(material);
  out.textures=std::move(textures);
  stop();
}
} // namespace

void source_paint_export(const PaintedMesh &paint,ObjectSurface &out,
    const std::function<bool()> &cancelled,SourcePaintExport mode) {
  compile_source_surface(paint,out,cancelled,mode,nullptr);
}

void source_baked_hdr_export(const SculptObject &owner,ObjectSurface &out,
    const std::function<bool()> &cancelled) {
  const auto &paint=owner.painted;
  auto marker=source_baked_preview(owner,cancelled);
  if(!paint||!marker||!source_baked_preview_metadata(*marker)||
     !paint->source_maps()||marker->overlay->source_baked_geometry()!=paint->mesh_owner()||
     out.material.emissive!=std::array<float,3>{})
    throw std::invalid_argument("Prepared HDR Bake requires its actual non-emission material view");
  out.textures=std::move(marker);
  prepare_baked_export_frame(owner,out,cancelled);
  compile_source_surface(*paint,out,cancelled,SourcePaintExport::StandardHdr,&owner);
}
} // namespace dome::detail
