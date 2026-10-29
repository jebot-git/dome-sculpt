// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/render.hpp"
#include <map>
#include <set>
#include <tuple>
#include <climits>
namespace dome {
// Four arbitrary-sized sampled images share one fragment texture unit. Mips
// stay GPU-resident: texture -> pixel-pack buffer -> texture-buffer attachment.
// glGetTexImage writes only GPU-buffer byte offsets: no host download, mapping,
// fence wait or CPU resampling is involved.
class MToonSourcePacks {
public:
  struct Input {std::shared_ptr<const Image> owner;GLuint texture=0;bool mips=false,srgb=true;};
  using Inputs=std::array<Input,4>;
  using Key=std::array<std::tuple<uintptr_t,bool,bool>,4>;
  struct Resource {
    GLuint texture=0,buffer=0;size_t bytes=0;
    Inputs owners;
    // offset (RGBA texels), width, height, reserved, for4x14 mip levels.
    std::array<std::array<GLint,4>,56> levels{};
    std::array<std::array<GLint,2>,4> info{}; // count,sRGB.
  };
private:
  std::map<Key,Resource> cache_;
  uint64_t builds_=0;
  static Resource specification(const Inputs &inputs) {
    Resource output;output.owners=inputs;
    std::map<std::tuple<const Image*,bool,bool>,size_t> shared;
    size_t texels=0;
    for(size_t view=0;view<4;++view) {
      const auto &input=inputs[view];if(!input.owner)continue;
      input.owner->validate();
      auto key=std::tuple{input.owner.get(),input.mips,input.srgb};
      if(auto found=shared.find(key);found!=shared.end()) {
        auto previous=found->second;output.info[view]=output.info[previous];
        std::copy_n(output.levels.begin()+previous*14,14,output.levels.begin()+view*14);continue;
      }
      shared.emplace(key,view);
      uint32_t width=input.owner->width,height=input.owner->height;
      for(size_t level=0;;++level) {
        if(level>=14 || texels>size_t(INT_MAX)-size_t(width)*height)
          throw std::length_error("GPU source pack metadata exceeds index limits");
        output.levels[view*14+level]={GLint(texels),GLint(width),GLint(height),0};
        ++output.info[view][0];output.info[view][1]=input.srgb;
        texels+=size_t(width)*height;
        if(!input.mips || (width==1 && height==1))break;
        width=std::max(1u,width/2);height=std::max(1u,height/2);
      }
    }
    output.bytes=texels*4;return output;
  }
  struct PackState {
    GLint buffer=0,texture2d=0,texture_buffer=0;
    static constexpr std::array<GLenum,8> names{GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,GL_PACK_IMAGE_HEIGHT,
      GL_PACK_SKIP_PIXELS,GL_PACK_SKIP_ROWS,GL_PACK_SKIP_IMAGES,GL_PACK_SWAP_BYTES,GL_PACK_LSB_FIRST};
    std::array<GLint,8> values{};
    PackState() {
      glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&buffer);
      glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture2d);glGetIntegerv(GL_TEXTURE_BINDING_BUFFER,&texture_buffer);
      for(size_t n=0;n<names.size();++n){glGetIntegerv(names[n],&values[n]);glPixelStorei(names[n],n==0?1:0);}
    }
    ~PackState() {
      glBindBuffer(GL_PIXEL_PACK_BUFFER,GLuint(buffer));
      glBindTexture(GL_TEXTURE_2D,GLuint(texture2d));glBindTexture(GL_TEXTURE_BUFFER,GLuint(texture_buffer));
      for(size_t n=0;n<names.size();++n)glPixelStorei(names[n],values[n]);
    }
  };
  static void allocate(Resource &resource) {
    if(!resource.bytes)return;
    PackState state;
    try {
      glGenBuffers(1,&resource.buffer);glBindBuffer(GL_PIXEL_PACK_BUFFER,resource.buffer);
      glBufferData(GL_PIXEL_PACK_BUFFER,GLsizeiptr(resource.bytes),nullptr,GL_STATIC_COPY);
      if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("GPU source pack allocation failed");
      std::set<GLint> copied;
      for(size_t view=0;view<4;++view)for(GLint level=0;level<resource.info[view][0];++level) {
        auto offset=resource.levels[view*14+size_t(level)][0];
        if(!copied.insert(offset).second)continue;
        if(!resource.owners[view].texture)throw std::invalid_argument("GPU source pack has no uploaded texture");
        glBindTexture(GL_TEXTURE_2D,resource.owners[view].texture);
        glGetTexImage(GL_TEXTURE_2D,level,GL_RGBA,GL_UNSIGNED_BYTE,
                      reinterpret_cast<void*>(uintptr_t(offset)*4));
        if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("GPU source mip copy into buffer failed");
      }
      glGenTextures(1,&resource.texture);glBindTexture(GL_TEXTURE_BUFFER,resource.texture);
      glTexBuffer(GL_TEXTURE_BUFFER,GL_RGBA8,resource.buffer);
      if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("GPU packed source view allocation failed");
    } catch(...) {
      if(resource.texture)glDeleteTextures(1,&resource.texture);
      if(resource.buffer)glDeleteBuffers(1,&resource.buffer);
      resource.texture=resource.buffer=0;throw;
    }
  }
  static std::map<Key,Resource> planning(const std::vector<Inputs> &inputs,size_t base_bytes,size_t base_images,const SurfaceTextureLimits &limits) {
    if(base_bytes>limits.max_bytes)throw std::length_error("GPU source byte budget exceeded");
    std::map<Key,Resource> wanted;size_t bytes=base_bytes;
    GLint maximum=0;glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE,&maximum);
    for(const auto &input:inputs) {
      auto id=key(input);if(wanted.contains(id))continue;
      auto spec=specification(input);
      if(!spec.bytes)continue;
      if(spec.bytes/4>size_t(std::max(0,maximum)))throw std::length_error("GPU packed source exceeds texture-buffer limit");
      if(spec.bytes>limits.max_bytes-bytes)throw std::length_error("GPU packed source byte budget exceeded");
      bytes+=spec.bytes;wanted.emplace(id,std::move(spec));
    }
    if(base_images>limits.max_images || wanted.size()>limits.max_images-base_images)
      throw std::length_error("GPU packed source image budget exceeded");
    return wanted;
  }
  void sweep(const std::map<Key,Resource> &wanted) {
    for(auto it=cache_.begin();it!=cache_.end();)if(!wanted.contains(it->first)) {
      if(it->second.texture)glDeleteTextures(1,&it->second.texture);if(it->second.buffer)glDeleteBuffers(1,&it->second.buffer);it=cache_.erase(it);
    }else ++it;
  }
public:
  static Key key(const Inputs &inputs) {
    Key result;for(size_t n=0;n<4;++n)result[n]={reinterpret_cast<uintptr_t>(inputs[n].owner.get()),inputs[n].mips,inputs[n].srgb};return result;
  }
  void preflight(const std::vector<Inputs> &inputs,size_t base_bytes,size_t base_images,const SurfaceTextureLimits &limits) {
    auto wanted=planning(inputs,base_bytes,base_images,limits);sweep(wanted);
  }
  void prepare(const std::vector<Inputs> &inputs,size_t base_bytes,size_t base_images,const SurfaceTextureLimits &limits) {
    auto wanted=planning(inputs,base_bytes,base_images,limits);
    std::vector<Key> added;added.reserve(wanted.size());
    try {
      for(auto &[id,spec]:wanted)if(!cache_.contains(id)) {
        allocate(spec);
        try {cache_.emplace(id,std::move(spec));added.push_back(id);++builds_;}
        catch(...) {if(spec.texture)glDeleteTextures(1,&spec.texture);if(spec.buffer)glDeleteBuffers(1,&spec.buffer);throw;}
      }
    } catch(...) {
      for(const auto &id:added){auto &r=cache_.at(id);if(r.texture)glDeleteTextures(1,&r.texture);if(r.buffer)glDeleteBuffers(1,&r.buffer);cache_.erase(id);}throw;
    }
    sweep(wanted);
  }
  const Resource &at(const Inputs &input) const {
    if(std::none_of(input.begin(),input.end(),[](const auto &v){return bool(v.owner);})) {static const Resource empty;return empty;}
    return cache_.at(key(input));
  }
  size_t bytes() const {size_t result=0;for(const auto &[key,r]:cache_)result+=r.bytes;return result;}
  size_t size() const {return cache_.size();}
  uint64_t builds() const {return builds_;}
  void clear(bool release) {
    if(release)for(const auto &[key,r]:cache_){if(r.texture)glDeleteTextures(1,&r.texture);if(r.buffer)glDeleteBuffers(1,&r.buffer);}
    cache_.clear();
  }
  ~MToonSourcePacks(){clear(true);}
};
} // namespace dome
