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
#include "dome/paint-sampling.hpp"
#include <bit>
#include <map>
namespace dome::detail {
namespace {
void stop(const TextureSetLimits&l){if(l.cancelled&&l.cancelled())throw std::runtime_error("Texture-set export cancelled");}
void charge(size_t&v,size_t n,size_t u,size_t cap){
 if(v>cap||(u&&n>(cap-v)/u))
  throw std::length_error("Texture-set export exceeds its working budget ("+std::to_string(v)+
      " already charged, "+std::to_string(n)+" entries at "+std::to_string(u)+
      ", limit "+std::to_string(cap)+")");
 v+=n*u;
}
template<class T>bool owner(const std::shared_ptr<const T>&a,const std::shared_ptr<const T>&b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);}
bool material(const Material&a,const Material&b){return a.name==b.name&&a.color==b.color&&a.metallic==b.metallic&&a.roughness==b.roughness&&a.emissive==b.emissive&&a.toon==b.toon&&a.double_sided==b.double_sided&&a.alpha==b.alpha&&a.alpha_cutoff==b.alpha_cutoff&&a.unlit==b.unlit;}

bool native_material_grid(const TextureSetView &field) {
 return !field.source&&!field.appearance&&!field.material_bindings&&native_hdr_grid(*field.painted);
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
    const TextureSetLimits &limits,bool keep_retained_normal) {
 const auto count=grid.padding.size();
 const auto &baked=paint.baked();
 const bool normal=!keep_retained_normal&&baked&&!baked->maps.normal.rgba.empty();
 charge(work,count,normal?4:3,limits.maximum_work);
 for(size_t at=0;at<count;++at){
  if((at&4095)==0)stop(limits);
  const auto owner=grid.padding[at];
  if(baked&&owner>=0&&baked->coverage[size_t(owner)])
   charge(work,1,normal?16:10,limits.maximum_work);
  charge(work,std::popcount(grid.mask(at)),12,limits.maximum_work);
 }
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

bool retained_hdr_grid(const TextureSetView &field) noexcept {
 const auto &binding=field.material_bindings;
 if(!field.source||!field.source->source_preview()||!binding||!binding->polygon_surface_transfer||binding->has_mtoon||
    field.material.toon||field.appearance||!field.painted||!retained_color_grid(*field.painted))
  return false;
 const auto &bake=field.painted->baked();
 if((bake&&bake->emission_field)||binding->physical_emission||
    hdr_emission(binding->native_reference.emission)||hdr_emission(field.material.emissive))return true;
 for(const auto &layer:field.painted->layers())
  if(layer.visible&&layer.opacity>0&&std::any_of(layer.emission_tiles.begin(),layer.emission_tiles.end(),
        [](const auto &tile){return bool(tile);}))return true;
 return false;
}

size_t retained_hdr_normal_bytes(const TextureSetView &field) noexcept {
 const auto &preview=field.source->source_preview();
 if(preview->native_bake())return preview->native_bake()->rgba.size();
 const auto &baked=field.painted->baked();
 return baked&&baked->normal_field?
     baked->normal_field->pbr()->rgba.size()+baked->normal_field->mtoon()->rgba.size():0;
}

size_t native_hdr_grid_bytes(const PaintedMesh &paint,bool keep_retained_normal) noexcept {
 const auto r=paint.resolution();
 const auto &baked=paint.baked();
 const bool normal=!keep_retained_normal&&baked&&!baked->maps.normal.rgba.empty();
 return sizeof(TextureMaps)+size_t(r)*r*(normal?12:8)+size_t(r/PaintTile::size)*(r/PaintTile::size)*sizeof(uint16_t);
}

void charge_native_hdr_grid_work(const PaintedMesh &paint,size_t &work,const TextureSetLimits &limits,
    bool keep_retained_normal) {
 const SparseExportGrid grid(paint,work,limits);
 charge_native_grid(paint,grid,work,limits,keep_retained_normal);
}

std::shared_ptr<TextureMaps> compile_native_hdr_grid(const PaintedMesh &paint,const Material &material,
    size_t &work,const TextureSetLimits &limits,bool keep_retained_normal) {
 if(!(keep_retained_normal?retained_color_grid(paint):native_hdr_grid(paint)))
  throw std::invalid_argument("HDR grid compilation needs actual colour and normal owners");
 if(native_hdr_grid_bytes(paint,keep_retained_normal)>limits.maximum_bytes)
  throw std::length_error("Native HDR material fields exceed their working budget");
 const SparseExportGrid grid(paint,work,limits);
 charge_native_grid(paint,grid,work,limits,keep_retained_normal);
 const auto count=grid.padding.size();
 const auto &baked=paint.baked();
 const bool normal=!keep_retained_normal&&baked&&!baked->maps.normal.rgba.empty();
 auto maps=std::make_shared<TextureMaps>();
 auto &albedo=maps->color,&orm=maps->metal_rough;
 for(auto *image:{&albedo,&orm}){
  image->width=image->height=grid.resolution;
  image->rgba.resize(count*4);
 }
 if(normal)maps->normal={grid.resolution,grid.resolution,std::vector<uint8_t>(count*4)};
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
   }
   color=encode_color(linear);
   alpha=linear[3];
   for(size_t i=0;i<3;++i)surface[i]=byte(material_values[i]);
  }
  maps->transparent|=alpha<.999f;
  std::copy(color.begin(),color.end(),albedo.rgba.data()+at*4);
  std::copy(surface.begin(),surface.end(),orm.rgba.data()+at*4);
  if(normal)std::copy(normal_sample.begin(),normal_sample.end(),maps->normal.rgba.data()+at*4);
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
     const TextureSetLimits &limits)
     :paint_(texture_set_source_read_view(field,limits)),
      sampler_(*paint_,limits.cancelled),resolution(paint_->resolution()) {
  const auto &binding=field.material_bindings;
  background_=binding&&binding->physical_emission?*binding->physical_emission:field.material.emissive;
  if(const auto maps=paint_->source_maps();maps&&(!binding||!binding->physical_emission))
   source_=maps->emission;
  if(source_.image&&source_.texcoord!=0)
   throw std::invalid_argument("Texture-set HDR requires its actual alternate emission coordinates");
  charge(work,sampler_.index_work(),1,limits.maximum_work);
  const size_t count=size_t(resolution)*resolution;
  size_t variable_count=count;
  if(native_material_grid(field)||retained_hdr_grid(field)){
   grid_.emplace(*paint_,work,limits);
   constants_[0]=resolve({});
   if(const auto &baked=paint_->baked())
    if(const auto sample=shared_baked_emission(*baked,work,limits)){
     constants_[1]=resolve(*sample);
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
  charge(work,variable_count,passes*(sampler_.work_per_texel()+12+(source_.image?16:0)),
      limits.maximum_work);
 }
 SampleKind sample_kind(size_t at) const {
  if(!grid_||grid_->mask(at))return SampleKind::Varying;
  if(paint_->baked()&&grid_->padding[at]>=0)
   return shared_bake_?SampleKind::SharedBake:SampleKind::Varying;
  return SampleKind::Background;
 }
 const auto &constant_samples() const { return constants_; }
 const auto &constant_counts() const { return constant_counts_; }
 size_t working_bytes() const {
  return sizeof(*this)+(grid_?grid_->tile_layers.capacity()*sizeof(uint16_t):0);
 }
 EmissionExrPixel sample_variable(uint32_t x,uint32_t y) const {
  auto background=background_;
  if(source_.image){
   const auto sample=sample_source_texture(source_,(float(x)+.5f)/resolution,(float(y)+.5f)/resolution,true);
   for(size_t k=0;k<3;++k)background[k]*=sample[k];
  }
  const size_t at=size_t(y)*resolution+x;
  const auto contribution=grid_&&paint_->baked()&&!grid_->mask(at)?
      baked_emission_texel(*paint_->baked(),size_t(grid_->padding[at])):sampler_.sample_texel(x,y);
  return coefficients(contribution,background);
 }
 EmissionExrPixel pixel(uint32_t x,uint32_t y)const{
  const auto kind=sample_kind(size_t(y)*resolution+x);
  return kind==SampleKind::Varying?sample_variable(x,y):constants_[size_t(kind)];
 }
private:
 static EmissionExrPixel coefficients(const BakedEmissionSample &q,std::array<float,3> background) {
  const auto rgb=q.resolve(background,1.f);
  return {rgb[0],rgb[1],rgb[2],q.legacy[0],q.legacy[1],q.legacy[2],
      q.absolute[0],q.absolute[1],q.absolute[2],q.coverage};
 }
 EmissionExrPixel resolve(const BakedEmissionSample &q)const{return coefficients(q,background_);}
 std::shared_ptr<const PaintedMesh> paint_;
 PaintEmissionSampler sampler_;
 std::array<float,3> background_{};
 PaintSourceTexture source_;
 std::optional<SparseExportGrid> grid_;
 std::array<EmissionExrPixel,2> constants_{};
 std::array<size_t,2> constant_counts_{};
 bool shared_bake_=false;
public:
 uint32_t resolution;
};
TextureSetExportEmission compile_emission(const TextureSetView &field,size_t &work,const TextureSetLimits &limits) {
 const TextureSetEmissionSamples samples(field,work,2,limits);
 const auto resolution=samples.resolution;
 const auto count=size_t(resolution)*resolution;
 std::array<float,3> peak{};
 const auto &constant_counts=samples.constant_counts();
 const auto &constants=samples.constant_samples();
 using Kind=TextureSetEmissionSamples::SampleKind;
 for(size_t value=0;value<constants.size();++value)if(constant_counts[value])
  for(size_t k=0;k<3;++k)peak[k]=std::max(peak[k],constants[value][k]);
 for(uint32_t y=0;y<resolution;++y){
  stop(limits);
  for(uint32_t x=0;x<resolution;++x){
   if(samples.sample_kind(size_t(y)*resolution+x)!=Kind::Varying)continue;
   const auto rgb=samples.sample_variable(x,y);
   for(size_t k=0;k<3;++k)peak[k]=std::max(peak[k],rgb[k]);
  }
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
 return out;
}

struct ExportBound { size_t bytes, work; };
ExportBound export_bound(const ObjectSurface&input,const TextureSetLimits&limits,bool measured_physical){
 limits.validate();stop(limits);const auto&plan=input.texture_set_surface;
 if(!plan)throw std::invalid_argument("Texture-set export requires its owning surface plan");plan->authenticate_owner(plan);
 if(input.id!=plan->object()||!owner(input.deformation,plan->deformation())||plan->fields().empty()||!material(input.material,plan->fields()[0].material)||
    !input.mesh.positions.empty()||!input.mesh.normals.empty()||!input.mesh.indices.empty()||!input.mesh.uv.empty()||!input.mesh.uv_pins.empty()||!input.mesh.uv_seams.empty()||input.textures||input.appearance||input.material_bindings||input.source_dormant||input.polygon_colors||input.paint_sampling_attachment||!input.paint_corner_coordinates.empty()||input.normal_preview_proof)
  throw std::invalid_argument("Texture-set export bundle differs from its genuine geometry/material/pose owner");
 const auto&rest=*plan->geometry();plan->authenticate_rest(rest,input.deformation,limits);rest.validate();
 size_t bytes=sizeof(TextureSetExport)+plan->preparation_bytes(),work=plan->preparation_work();const auto count=plan->indices().size();
 charge(bytes,count,2*sizeof(Vec3)+sizeof(std::array<float,2>)+sizeof(Tangent)+3*sizeof(uint32_t)+1,limits.maximum_bytes);
 charge(bytes,count,10*sizeof(void*)+12*sizeof(uint32_t),limits.maximum_bytes);
 charge(work,count,64,limits.maximum_work);size_t scratch=0;
 for(size_t f=0;f<plan->fields().size();++f){
  stop(limits);const auto&field=plan->fields()[f];const auto&draw=plan->draws()[f];
  if(!draw.count)continue;
  charge(bytes,1,sizeof(TextureSetExportField)+field.name.size()+field.material.name.size()+258,limits.maximum_bytes);
  const auto resolution=field.painted->resolution();
  const bool hdr=texture_set_physical_emission(field);
  const bool sparse_retained=hdr&&retained_hdr_grid(field);
  if(hdr&&(!measured_physical||field.material.toon||field.appearance||
      (field.material_bindings&&field.material_bindings->has_mtoon)))
   throw std::invalid_argument("Texture-set HDR emission needs its per-material precision-qualified export compiler");
  if(hdr&&native_material_grid(field)){
   const size_t pixels=size_t(resolution)*resolution;
   charge(bytes,1,native_hdr_grid_bytes(*field.painted),limits.maximum_bytes);
   charge(bytes,pixels,4,limits.maximum_bytes);
   continue;
  }
  if(sparse_retained){
   charge(bytes,1,native_hdr_grid_bytes(*field.painted,true),limits.maximum_bytes);
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
 if(input.deformation){const auto entries=input.deformation->resource_entries();charge(bytes,entries,32,limits.maximum_bytes);charge(work,entries,16,limits.maximum_work);}
 stop(limits);return {bytes,work};
}
} // namespace

bool texture_set_physical_emission(const TextureSetView &field) {
 return hdr_emission(field.material.emissive)||(field.painted&&field.painted->has_physical_emission())||
     (field.material_bindings&&physical_emission(*field.material_bindings));
}

EmissionExportRaster texture_set_emission_raster(const TextureSetView &field,const TextureSetLimits &limits){
 if(!texture_set_physical_emission(field)||!field.domain||field.domain->triangles().empty()||
    field.material.toon||field.appearance||(field.material_bindings&&field.material_bindings->has_mtoon))
  throw std::invalid_argument("Texture-set HDR requires an actual physical PBR material member");
 size_t work=0;
 TextureSetEmissionSamples samples(field,work,1,limits);
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

size_t texture_set_export_working_bound(const ObjectSurface&input,const TextureSetLimits&limits,bool measured_physical){
 return export_bound(input,limits,measured_physical).bytes;
}
TextureSetExport compile_texture_set_export(const ObjectSurface&input,const TextureSetLimits&limits,bool measured_physical){
 auto [bytes,work]=export_bound(input,limits,measured_physical);
 TextureSetExport out;out.working_bound=bytes;
 const auto&plan=input.texture_set_surface;const auto&rest=*plan->geometry();const auto count=plan->indices().size();
 const bool source=std::any_of(plan->fields().begin(),plan->fields().end(),[](const auto&f){return bool(f.source);});
 bool normal=false;
 // Tangent basis is evaluated in REST, never the current pose. Include the
 // actual vertex and role-specific frame bits in every necessary split.
 const bool canonical_frames=std::any_of(plan->fields().begin(),plan->fields().end(),[](const auto&field){return !field.source&&field.painted->baked()&&!field.painted->baked()->maps.normal.rgba.empty();});
 const auto frames=canonical_frames?tangent_corners(rest,limits.cancelled):std::vector<Tangent>{};
 using Signature=std::array<uint32_t,12>;std::map<Signature,uint32_t>vertices;std::vector<uint32_t>rest_vertices;
 auto&mesh=out.surface.mesh;mesh.indices.reserve(count);rest_vertices.reserve(std::min(count,rest.positions.size()));
 for(size_t f=0;f<plan->fields().size();++f){
  stop(limits);const auto&field=plan->fields()[f];const auto&draw=plan->draws()[f];if(!draw.count)continue;
  const bool canonical=!field.source&&field.painted->baked()&&!field.painted->baked()->maps.normal.rgba.empty();
  const auto portable=field.source?std::optional<PortableTextureSetField>(portable_texture_set_field(field,input.world_matrix(),work,limits)):std::nullopt;
  normal|=canonical||(portable&&portable->normal_role);
  TextureSetExportField output;
  output.set=field.id;
  output.name=field.name;
  output.first=mesh.indices.size();
  output.count=draw.count;
  output.material=field.material;
  output.normal_role=portable&&portable->normal_role;
  if(texture_set_physical_emission(field)){
   output.emission=compile_emission(field,work,limits);
   output.material.emissive={};
   if(portable)output.textures=portable->maps;
   else if(native_material_grid(field))output.textures=compile_native_hdr_grid(*field.painted,field.material,work,limits);
   else output.textures=EmissionExportAccess::other_channels(
       *texture_set_source_read_view(field,limits),output.material,limits.cancelled);
  }else output.textures=portable?portable->maps:field.painted->composite(field.material,limits.cancelled);
  for(size_t c=0;c<draw.count;++c){
   if((c&1023)==0)stop(limits);const auto logical=plan->logical_corners()[draw.first+c];const auto vertex=rest.indices.at(logical);
   const auto n=portable?portable->corners[c].normal:rest.normals.at(vertex);const auto paint=portable?portable->corners[c].paint:rest.uv.at(vertex);const auto role=portable?portable->corners[c].role:rest.uv.at(vertex);
   const Tangent tangent=portable?portable->corners[c].tangent:canonical?frames.at(logical):Tangent{1,0,0,1};Signature key{};key[0]=vertex;size_t k=1;for(float v:tangent)key[k++]=std::bit_cast<uint32_t>(v);for(float v:{n.x,n.y,n.z})key[k++]=std::bit_cast<uint32_t>(v);for(float v:paint)key[k++]=std::bit_cast<uint32_t>(v);for(float v:role)key[k++]=std::bit_cast<uint32_t>(v);
   auto found=vertices.find(key);uint32_t id;
   if(found!=vertices.end())id=found->second;
   else{
    if(mesh.positions.size()>=2'000'000)throw std::length_error("Texture-set exported vertex splits exceed the native geometry limit");
    id=uint32_t(mesh.positions.size());vertices.emplace(key,id);rest_vertices.push_back(vertex);mesh.positions.push_back(rest.positions.at(vertex));mesh.normals.push_back(n);if(!rest.uv.empty())mesh.uv.push_back(rest.uv.at(vertex));if(!rest.uv_pins.empty())mesh.uv_pins.push_back(rest.uv_pins.at(vertex));out.tangents.push_back(tangent);if(source){out.paint_coordinates.push_back(paint);out.normal_coordinates.push_back(role);}
   }
   mesh.indices.push_back(id);if(!rest.uv_seams.empty())mesh.uv_seams.push_back(rest.uv_seams.at(logical));
  }
  out.fields.push_back(std::move(output));
 }
 if(out.fields.empty()||mesh.indices.size()!=count)throw std::invalid_argument("Texture-set export omitted a nonempty material range");
 mesh.validate();out.surface.id=input.id;out.surface.name=input.name;out.surface.transform=input.transform;out.surface.groups=input.groups;out.surface.affine_edit=input.affine_edit;out.surface.rig_source=input.rig_source;
 out.surface.material=out.fields[0].material;out.surface.textures=out.fields[0].textures;
 out.surface.deformation=normal_frame_remap_deformation(input.deformation,rest,mesh,rest_vertices,limits.cancelled);
 if(!normal){out.tangents.clear();out.normal_coordinates.clear();}stop(limits);return out;
}
} // namespace dome::detail
