// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/humanoid-creator.hpp"
#include "dome/import-image.hpp"
#include "cgltf.h"
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstring>
#include <exception>
#include <memory>
#include <new>
#include <span>
#include <vector>
namespace dome::detail {
struct RawSourceProofPlan {
  size_t metadata=0,uv=0,image=0;
  size_t bytes()const{return metadata+uv+image;}
};
inline RawSourceProofPlan raw_source_proof_plan(const CreatorAppearance &a,
    const std::function<bool()> &cancelled={}) {
  if(!a.original_bytes||!a.asset||!a.decoded_images)
    throw std::invalid_argument("Raw source proof has no canonical owner");
  const auto &b=*a.original_bytes;
  auto word=[&](size_t at){if(at>b.size()||b.size()-at<4)throw std::invalid_argument("Truncated raw canonical source");return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;};
  if(b.size()<28||word(0)!=0x46546c67||word(4)!=2||word(8)!=b.size()||word(16)!=0x4e4f534a)
    throw std::invalid_argument("Raw source proof requires its actual canonical GLB");
  const size_t length=word(12);
  if(length>16*1024*1024||length>b.size()-20)
    throw std::length_error("Raw canonical JSON exceeds source limits");
  size_t structural=1;bool quoted=false,escaped=false;
  for(size_t n=0;n<length;++n){
    if((n&4095)==0&&cancelled&&cancelled())throw std::runtime_error("Raw material proof forecast cancelled");
    const char c=char(b[20+n]);
    if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='\"')quoted=false;}
    else if(c=='\"')quoted=true;else if(c=='{'||c=='['||c==':'||c==',')++structural;
  }
  // Includes parser tokens, source tables, names and allocator headers. The
  // allocator below independently enforces this forecast before every request.
  const size_t unit=512+std::max({sizeof(cgltf_material),sizeof(cgltf_node),sizeof(cgltf_accessor),sizeof(cgltf_primitive)});
  RawSourceProofPlan plan;plan.metadata=10*length+unit*structural;
  size_t visited=0;
  for(const auto &p:a.asset->primitives){
    if((++visited&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Raw canonical primitive proof cancelled");
    if(p.source_mesh!=a.source_mesh||p.source_primitive!=a.source_primitive)continue;
    auto count=[&](size_t n){if(n>2'000'000)throw std::length_error("Raw canonical UV proof exceeds vertex limits");plan.uv=std::max(plan.uv,n*2*sizeof(float));};
    count(p.mesh.uv.size());count(p.uv1.size());for(const auto &[id,v]:p.extra_uv_sets)count(v.size());
  }
  for(const auto &i:*a.decoded_images){
    if(cancelled&&cancelled())throw std::runtime_error("Raw canonical image proof cancelled");
    if(!i||!i->width||!i->height||i->height>16*1024*1024/size_t(i->width))throw std::length_error("Raw canonical image proof exceeds pixel limits");
  }
  if(a.material<0||size_t(a.material)>=a.asset->materials.size())throw std::invalid_argument("Raw canonical material is unavailable");
  const auto &material=a.asset->materials[size_t(a.material)];
  for(const int id:{material.color_texture,material.metal_rough_texture,material.normal_texture,material.occlusion_texture,material.emissive_texture}){
    if(id<0)continue;
    if(size_t(id)>=a.asset->textures.size())throw std::invalid_argument("Raw canonical texture is unavailable");
    const auto image=a.asset->textures[size_t(id)].image;
    if(image<0||size_t(image)>=a.decoded_images->size())throw std::invalid_argument("Raw canonical image is unavailable");
    const auto &i=*a.decoded_images->at(size_t(image));
    // RGBA result, codec output/rows and JPEG coefficient storage coexist.
    plan.image=std::max(plan.image,size_t(i.width)*i.height*16+4096);
  }
  if(plan.metadata>512*1024*1024||plan.uv>512*1024*1024-plan.metadata||
     plan.image>512*1024*1024-plan.metadata-plan.uv)
    throw std::length_error("Raw material proof exceeds bounded working storage");
  return plan;
}
// A per-call replay of the actual canonical bytes. Parsed DTOs may have legal
// mutable aliases, so equal current DTO/map values alone are not provenance.
class RawSourceCanonical {
  struct Budget {
    struct alignas(std::max_align_t) Header{size_t bytes;Header *previous,*next;};
    size_t used=0,limit=0;Header *head=nullptr;
    const std::function<bool()> *cancelled=nullptr;
    ~Budget()noexcept{while(head){auto *next=head->next;::operator delete(head);head=next;}used=0;}
    static void *allocate(void *user,cgltf_size n){
      auto &b=*static_cast<Budget*>(user);
      if(b.cancelled&&*b.cancelled&&(*b.cancelled)())throw std::runtime_error("Raw canonical source proof cancelled");
      if(b.used>b.limit||sizeof(Header)>b.limit-b.used||n>b.limit-b.used-sizeof(Header))
        throw std::length_error("Raw canonical parser exceeds prospective working budget");
      // cgltf is compiled as C++ in our owned import.cpp. Never hand its
      // partial-table cleanup a null allocation: unwind to this arena instead.
      // Nothrow new preserves both C++ fault coverage and malloc-null handling.
      auto *h=static_cast<Header*>(::operator new(n+sizeof(Header),std::nothrow));
      if(!h)throw std::bad_alloc();
      h->bytes=n+sizeof(Header);h->previous=nullptr;h->next=b.head;
      if(b.head)b.head->previous=h;b.head=h;b.used+=h->bytes;return h+1;
    }
    static void release(void *user,void *p)noexcept{
      if(!p)return;auto &b=*static_cast<Budget*>(user);auto *h=static_cast<Header*>(p)-1;
      if(h->previous)h->previous->next=h->next;else b.head=h->next;
      if(h->next)h->next->previous=h->previous;b.used-=h->bytes;::operator delete(h);
    }

  } budget_;
  std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data_{nullptr,cgltf_free};
  const CreatorAppearance &a_;
  const std::function<bool()> &cancelled_;
  std::vector<int> images_,coordinates_;
  [[noreturn]] static void fail(){throw std::invalid_argument("Raw sampling differs from canonical source authority");}
  void stop()const{if(cancelled_&&cancelled_())throw std::runtime_error("Raw canonical source proof cancelled");}
  void result(cgltf_result result){
    if(result!=cgltf_result_success)fail();
  }
public:
  RawSourceCanonical(const CreatorAppearance &a,const std::function<bool()> &cancelled)
      :a_(a),cancelled_(cancelled){
    const auto plan=raw_source_proof_plan(a,cancelled);budget_.limit=plan.metadata;budget_.cancelled=&cancelled_;
    cgltf_options options{};options.memory={Budget::allocate,Budget::release,&budget_};
    cgltf_data *raw=nullptr;const auto parsed=cgltf_parse(&options,a.original_bytes->data(),a.original_bytes->size(),&raw);
    data_.reset(raw);result(parsed);stop();
    // No filesystem/network loader: canonical captures are self-contained.
    result(cgltf_load_buffers(&options,data_.get(),nullptr));result(cgltf_validate(data_.get()));stop();
    if(data_->textures_count!=a.asset->textures.size()||data_->images_count!=a.asset->images.size()||
       data_->materials_count+1!=a.asset->materials.size())fail();
  }
  const cgltf_material *material()const{
    if(a_.source_material<0)return nullptr;
    if(size_t(a_.source_material)>=data_->materials_count)fail();
    return &data_->materials[size_t(a_.source_material)];
  }
  void role(const cgltf_texture_view &view,int index,int coordinate,const ImportedTextureTransform &transform){
    stop();if(!view.texture){if(index!=-1)fail();return;}
    if(index<0||size_t(index)>=data_->textures_count||view.texture!=&data_->textures[size_t(index)]||view.texcoord!=coordinate)fail();
    ImportedTextureTransform actual;
    if(view.has_transform){actual.authored=true;std::copy_n(view.transform.offset,2,actual.offset.begin());std::copy_n(view.transform.scale,2,actual.scale.begin());actual.rotation=view.transform.rotation;actual.texcoord=view.transform.has_texcoord?view.transform.texcoord:-1;}
    if(actual!=transform)fail();
    const auto &texture=*view.texture;const auto &expected=a_.asset->textures.at(size_t(index));
    if(!texture.image||expected.image<0||size_t(expected.image)>=data_->images_count||texture.image!=&data_->images[size_t(expected.image)])fail();
    const int wrap_s=texture.sampler?int(texture.sampler->wrap_s):10497,wrap_t=texture.sampler?int(texture.sampler->wrap_t):10497;
    const int min=texture.sampler?int(texture.sampler->min_filter):0,mag=texture.sampler?int(texture.sampler->mag_filter):0;
    if(expected.wrap_s!=wrap_s||expected.wrap_t!=wrap_t||expected.min_filter!=min||expected.mag_filter!=mag)fail();
    if(std::find(images_.begin(),images_.end(),expected.image)!=images_.end())return;
    const auto &source=*texture.image;const auto &encoded=a_.asset->images.at(size_t(expected.image));
    if(!source.buffer_view||!source.mime_type||encoded.mime_type!=source.mime_type)fail();
    const auto *bytes=cgltf_buffer_view_data(source.buffer_view);
    if(!bytes||source.buffer_view->size!=encoded.bytes.size()||
       (!encoded.bytes.empty()&&std::memcmp(bytes,encoded.bytes.data(),encoded.bytes.size())))fail();
    const auto decoded=decode_import_image(encoded,16*1024*1024,cancelled_);
    const auto &owned=*a_.decoded_images->at(size_t(expected.image));
    if(decoded.width!=owned.width||decoded.height!=owned.height||decoded.rgba!=owned.rgba)fail();
    images_.push_back(expected.image);stop();
  }
  void coordinates(int set,const std::vector<std::array<float,2>> &values){
    stop();if(std::find(coordinates_.begin(),coordinates_.end(),set)!=coordinates_.end())return;
    if(a_.source_mesh<0||size_t(a_.source_mesh)>=data_->meshes_count||a_.source_primitive<0||
       size_t(a_.source_primitive)>=data_->meshes[size_t(a_.source_mesh)].primitives_count)fail();
    const auto &primitive=data_->meshes[size_t(a_.source_mesh)].primitives[size_t(a_.source_primitive)];
    const cgltf_accessor *accessor=nullptr;
    for(size_t n=0;n<primitive.attributes_count;++n){const auto &v=primitive.attributes[n];if(v.type==cgltf_attribute_type_texcoord&&v.index==set){if(accessor)fail();accessor=v.data;}}
    if(!accessor||accessor->type!=cgltf_type_vec2||accessor->count!=values.size()||values.size()>2'000'000)fail();
    std::vector<float> sparse;if(accessor->is_sparse){sparse.resize(values.size()*2);if(cgltf_accessor_unpack_floats(accessor,sparse.data(),sparse.size())!=sparse.size())fail();}
    for(size_t n=0;n<values.size();++n){if((n&1023)==0)stop();std::array<float,2> actual{};
      if(accessor->is_sparse)std::copy_n(sparse.data()+n*2,2,actual.begin());
      else if(!cgltf_accessor_read_float(accessor,n,actual.data(),2))fail();
      if(std::bit_cast<std::array<uint32_t,2>>(actual)!=std::bit_cast<std::array<uint32_t,2>>(values[n]))fail();
    }
    coordinates_.push_back(set);stop();
  }
};
}
