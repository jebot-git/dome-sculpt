// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-paint-export.hpp"
#include "dome/brush.hpp"
#include "dome/material-visibility.hpp"
#include "emission-raster-device.hpp"
#include "paint-overlay-sample.hpp"
#include "source-extra-uv-authority.hpp"
#include "source-paint-pairing.hpp"
#include <limits>

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
void visit_footprint(const Mesh &mesh,const PaintOverlayPacket &packet,ProofWork &work,Visit visit) {
  const int r=int(packet.resolution());
  // The bilinear footprint of one texel is a two-texel-wide rectangle.
  // Include a conservative margin for the viewport's FLOAT UV interpolation.
  const double radius=1+double(r)*32*std::numeric_limits<float>::epsilon();
  std::vector<bool> examined(packet.padding()->size()+1);
  bool visited=false;
  struct Axis { double x,y,limit; };
  for(size_t face=0;face<mesh.indices.size();face+=3) {
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
    for(int y=bottom;y<=top;++y){
      work.stop();
      work.charge(size_t(right-left+1)*3);
      for(int x=left;x<=right;++x){
        if(std::ranges::any_of(axes,[&](const auto &axis){return axis.x*x+axis.y*y>axis.limit;}))continue;
        const int32_t owner=packet.padding()->at(size_t(y)*r+x);
        const size_t key=size_t(owner+1);
        if(examined.at(key))continue;
        examined[key]=true;
        const auto sample=owner<0?PaintOverlayTexel{}:sample_overlay_texel_owner(packet,size_t(owner),true);
        work.charge(sample.work+1);
        visited=true;
        if(!visit(sample.values))return;
      }
    }
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
    bool srgb,bool masked,ProofWork &work) {
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
    affine.channels|=1u<<c;
    if(original){
      factor=factor*(*original)*(1-channel.coverage)+offset;
      affine.offset[c]=1;
      continue;
    }
    // The new alpha texture must preserve cutout decisions. A varying affine
    // alpha can move a crossing after PNG quantization or mip regeneration.
    if(masked&&c==3)portable_required();
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
                ProofWork &,bool rewrite=false,bool masked=false);
void fold_orm(PaintSourceTexture &,Material &,const PaintOverlayValues &,ProofWork &,bool rewrite=false);

// Preserve masked coverage exactly. A constant alpha can be carried outside
// the texture even when its value falls between two PNG alpha bytes.
struct AtlasAlpha { std::optional<float> constant;bool transparent=false; };
AtlasAlpha atlas_alpha(const PaintedMesh &paint,const PaintOverlayPacket &packet,
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
  if(masked&&!uniform&&!unorm)portable_required();
  return {uniform?constant:std::nullopt,transparent};
}

// The source contribution in each rewritten channel is either constant or
// fully covered across the triangle footprint. Composition therefore
// commutes with the atlas' linear filter. Quantizing the individual taps
// bounds the resulting channel error by the largest tap error.
struct PaintAtlas {
  std::shared_ptr<Image> color,orm,emission;
  bool transparent=false;
};

PaintAtlas compile_atlas(const PaintedMesh &paint,const PaintOverlayPacket &packet,
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
      alpha=atlas_alpha(paint,packet,background.color[3],material.alpha==Material::AlphaMode::Mask,work);
    }
    material.color={1,1,1,alpha.constant.value_or(1)};
  }else if(range.high[3][0]!=0){
    fold_color(source.color,material.color,value[0],value[3][0],work,true,material.alpha==Material::AlphaMode::Mask);
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
    const std::array<float,4> &addition,float coverage,ProofWork &work,bool rewrite,bool masked) {
  if(std::ranges::all_of(std::span(addition).first(N),[](float value){return value==0;})){
    for(auto &value:factor)value*=1-coverage;
    return;
  }
  if(rewrite){
    std::array<UniformChannel,4> channels{};
    for(unsigned c=0;c<N;++c)channels[c]={&factor[c],coverage,addition[c]};
    fold_uniform_image(texture,channels,true,masked,work);
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
    fold_uniform_image(texture,channels,false,false,work);
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
    return compile_atlas(paint,packet,range,sources,material,work);
  }
  const bool rewrite=allow_atlas&&!object.material.toon;
  fold_color(sources.color,material.color,value[0],value[3][0],work,rewrite,
             material.alpha==Material::AlphaMode::Mask);
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
    const std::vector<std::array<float,2>> &uv0,size_t maximum_bytes) {
  // Unbound authored accessors still belong to CreatorAppearance. Keep only
  // coordinate owners actually used by the detached material bindings here.
  if(atlas.color)source.color={};
  if(atlas.orm)source.metal_rough={};
  if(atlas.emission)source.emission={};
  if(!source.uses_uv1())source.uv1.reset();
  std::erase_if(source.extra_uv_sets,[&](const auto &entry){return !uses_coordinate(source,entry.first);});
  if(!atlas.color&&!atlas.orm&&!atlas.emission)return;
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
}
} // namespace

void source_paint_export(const PaintedMesh &paint,ObjectSurface &out,
    const std::function<bool()> &cancelled,bool allow_paint_atlas) {
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
  if(!marker->overlay->empty())
    atlas=compile_source_paint(paint,out,*marker->overlay,*source,material,cancelled,allow_paint_atlas);
  if(original.uv1)source->uv1=std::make_shared<const std::vector<std::array<float,2>>>(remap(*original.uv1));
  for(auto &[set,values]:source->extra_uv_sets)
    values=std::make_shared<const std::vector<std::array<float,2>>>(remap(*values));
  auto uv0=remap(before->mesh().uv);
  const bool transparent=atlas.transparent;
  install_atlas(*source,std::move(atlas),out,uv0,bytes_limit);
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
} // namespace dome::detail
