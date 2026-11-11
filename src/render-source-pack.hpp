// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/render.hpp"
#include "render-paint-overlay.hpp"
#include <map>
#include <set>
#include <tuple>
#include <climits>
namespace dome {
// Seven bounded views share one fragment texture unit. Legacy uses the first
// four; native emission adds three independent floating contribution views. Mips
// stay GPU-resident: texture -> pixel-pack buffer -> texture-buffer attachment. Linear overlay
// views promote the shared destination to RGBA32F; legacy source-only packs
// retain their exact RGBA8 path and byte budget.
// glGetTexImage writes only GPU-buffer byte offsets: no host download, mapping,
// fence wait or CPU resampling is involved.
class MToonSourcePacks {
public:
  using Ledger=detail::ContextStoreLedger;
  struct Input {
    std::shared_ptr<const Image> owner;GLuint texture=0;bool mips=false,srgb=true;
    // Actual floating GPU plane authority; no fabricated byte Image is needed.
    std::shared_ptr<const detail::PaintOverlayPacket> overlay;uint32_t plane=0;bool overlay_pbr=false;
    static Input linear(const detail::PaintOverlayCompiler::Resource &resource,uint32_t plane,bool mips=false){if(plane>=5)throw std::invalid_argument("Invalid GPU overlay plane");Input in;in.texture=resource.planes[plane];in.mips=mips;in.srgb=false;in.overlay=resource.owner;in.plane=plane;in.overlay_pbr=resource.pbr;return in;}
  };
  static constexpr size_t view_count=7;
  using Inputs=std::array<Input,view_count>;
  using Key=std::array<std::tuple<uintptr_t,bool,bool,uint32_t>,view_count>;
  struct Resource {
    Ledger::Store texture_store,buffer_store;
    GLuint texture=0,buffer=0;size_t bytes=0;bool floating=false;
    Inputs owners;
    // offset (RGBA texels), width, height, reserved, for view_count*14 levels.
    std::array<std::array<GLint,4>,view_count*14> levels{};
    std::array<std::array<GLint,2>,view_count> info{}; // count,sRGB.
  };
private:
  std::map<Key,Resource> cache_;
  std::shared_ptr<Ledger> ledger_;Ledger::Group group_=0;bool own_ledger_=false;
  uint64_t builds_=0;
  static Resource specification(const Inputs &inputs) {
    Resource output;output.owners=inputs;
    std::map<std::tuple<uintptr_t,bool,bool,uint32_t>,size_t> shared;
    output.floating=std::any_of(inputs.begin(),inputs.end(),[](const auto &in){return bool(in.overlay);});
    size_t texels=0;
    for(size_t view=0;view<view_count;++view) {
      const auto &input=inputs[view];if(!input.owner&&!input.overlay)continue;
      if(input.overlay){if(input.owner||input.overlay->empty()||input.srgb||input.plane>=5)throw std::invalid_argument("Invalid linear overlay source-pack authority");}
      else input.owner->validate();
      auto key=std::tuple{reinterpret_cast<uintptr_t>(input.overlay?static_cast<const void*>(input.overlay.get()):static_cast<const void*>(input.owner.get())),input.mips,input.srgb,input.overlay?input.plane+1+(input.overlay_pbr?5u:0u):0u};
      if(auto found=shared.find(key);found!=shared.end()) {
        auto previous=found->second;output.info[view]=output.info[previous];
        std::copy_n(output.levels.begin()+previous*14,14,output.levels.begin()+view*14);continue;
      }
      shared.emplace(key,view);
      uint32_t width=input.overlay?input.overlay->resolution():input.owner->width,height=input.overlay?input.overlay->resolution():input.owner->height;
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
    output.bytes=texels*(output.floating?16:4);return output;
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
  void allocate(Resource &resource) {
    if(!resource.bytes)return;
    PackState state;
    try {
      resource.buffer_store=ledger_->create(Ledger::Kind::Buffer,resource.bytes,Ledger::Domain::Material,group_);resource.buffer=resource.buffer_store.name();glBindBuffer(GL_PIXEL_PACK_BUFFER,resource.buffer);
      glBufferData(GL_PIXEL_PACK_BUFFER,GLsizeiptr(resource.bytes),nullptr,GL_STATIC_COPY);
      if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("GPU source pack allocation failed");
      std::set<GLint> copied;
      for(size_t view=0;view<view_count;++view)for(GLint level=0;level<resource.info[view][0];++level) {
        auto offset=resource.levels[view*14+size_t(level)][0];
        if(!copied.insert(offset).second)continue;
        if(!resource.owners[view].texture)throw std::invalid_argument("GPU source pack has no uploaded texture");
        glBindTexture(GL_TEXTURE_2D,resource.owners[view].texture);
        glGetTexImage(GL_TEXTURE_2D,level,GL_RGBA,resource.floating?GL_FLOAT:GL_UNSIGNED_BYTE,
                      reinterpret_cast<void*>(uintptr_t(offset)*(resource.floating?16:4)));
        if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("GPU source mip copy into buffer failed");
      }
      resource.texture_store=ledger_->create(Ledger::Kind::Texture,0,Ledger::Domain::Material,group_,std::span(&resource.buffer_store,1));resource.texture=resource.texture_store.name();glBindTexture(GL_TEXTURE_BUFFER,resource.texture);
      glTexBuffer(GL_TEXTURE_BUFFER,resource.floating?GL_RGBA32F:GL_RGBA8,resource.buffer);
      if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("GPU packed source view allocation failed");
    } catch(...) {
      resource.texture_store.reset();resource.buffer_store.reset();
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
      if(spec.bytes/(spec.floating?16:4)>size_t(std::max(0,maximum)))throw std::length_error("GPU packed source exceeds texture-buffer limit");
      if(spec.bytes>limits.max_bytes-bytes)throw std::length_error("GPU packed source byte budget exceeded");
      bytes+=spec.bytes;wanted.emplace(id,std::move(spec));
    }
    if(base_images>limits.max_images || wanted.size()>limits.max_images-base_images)
      throw std::length_error("GPU packed source image budget exceeded");
    return wanted;
  }
  void sweep(const std::map<Key,Resource> &wanted) {
    for(auto it=cache_.begin();it!=cache_.end();)if(!wanted.contains(it->first)) {
      it=cache_.erase(it);
    }else ++it;
  }
public:
  explicit MToonSourcePacks(std::shared_ptr<Ledger> ledger={}):ledger_(ledger?ledger:std::make_shared<Ledger>(Ledger::Limits{2ull*1024*1024*1024,2048,256})),own_ledger_(!ledger){group_=ledger_->create_group();}
  Ledger::Group group()const noexcept{return group_;}
  struct Demand{size_t bytes=0,images=0;};
  Demand additional(const std::vector<Inputs>&inputs,const SurfaceTextureLimits&limits)const{auto wanted=planning(inputs,0,0,limits);Demand d;for(const auto&[key,s]:wanted)if(!cache_.contains(key)){if(s.bytes>SIZE_MAX-d.bytes)throw std::length_error("Source pack forecast overflow");d.bytes+=s.bytes;++d.images;}return d;}
  static Key key(const Inputs &inputs) {
    Key result;for(size_t n=0;n<view_count;++n)result[n]={reinterpret_cast<uintptr_t>(inputs[n].overlay?static_cast<const void*>(inputs[n].overlay.get()):static_cast<const void*>(inputs[n].owner.get())),inputs[n].mips,inputs[n].srgb,inputs[n].overlay?inputs[n].plane+1+(inputs[n].overlay_pbr?5u:0u):0u};return result;
  }
  using Selection=std::set<Key>;
  static Selection selection(const std::vector<Inputs>&inputs){Selection result;for(const auto&input:inputs)if(std::any_of(input.begin(),input.end(),[](const auto&i){return bool(i.owner)||bool(i.overlay);}))result.insert(key(input));return result;}
  void retain(const Selection&wanted)noexcept {for(auto it=cache_.begin();it!=cache_.end();)if(!wanted.contains(it->first)){it=cache_.erase(it);}else ++it;}
  void preflight(const std::vector<Inputs> &inputs,size_t base_bytes,size_t base_images,const SurfaceTextureLimits &limits) {
    (void)planning(inputs,base_bytes,base_images,limits);
  }
  void prepare(const std::vector<Inputs> &inputs,size_t base_bytes,size_t base_images,const SurfaceTextureLimits &limits,bool preserve_existing=false) {
    if(own_ledger_)ledger_->collect();
    auto wanted=planning(inputs,base_bytes,base_images,limits);const auto delta=additional(inputs,limits);const auto used=ledger_->usage(group_);
    if(base_bytes>limits.max_bytes||used.bytes>limits.max_bytes-base_bytes||delta.bytes>limits.max_bytes-base_bytes-used.bytes||base_images>limits.max_images||used.images>limits.max_images-base_images||delta.images>limits.max_images-base_images-used.images)throw std::length_error("Source pack old/new/debt quota exceeded");ledger_->preflight(Ledger::Domain::Material,delta.bytes,delta.images);
    std::vector<Key> added;added.reserve(wanted.size());
    try {
      for(auto &[id,spec]:wanted)if(!cache_.contains(id)) {
        allocate(spec);
        try {cache_.emplace(id,std::move(spec));added.push_back(id);++builds_;}
        catch(...) {throw;}
      }
    } catch(...) {
      for(const auto &id:added)cache_.erase(id);if(own_ledger_)ledger_->seal_noexcept();throw;
    }
    if(own_ledger_&&delta.bytes)ledger_->seal(true);if(!preserve_existing)sweep(wanted);
  }
  // Reacquire before each submission, or mark_used() before submitting a
  // retained raw ref. Pruning prepare/retain/clear invalidates such refs.
  void mark_used()noexcept{ledger_->mark_used();}
  const Resource &at(const Inputs &input) const {
    ledger_->mark_used();
    if(std::none_of(input.begin(),input.end(),[](const auto &v){return bool(v.owner)||bool(v.overlay);})) {static const Resource empty;return empty;}
    return cache_.at(key(input));
  }
  size_t bytes() const {return ledger_->usage(group_).bytes;}
  size_t size() const {return cache_.size();}
  uint64_t builds() const {return builds_;}
  void clear(bool release) {
    cache_.clear();if(own_ledger_)ledger_->clear(release);
  }
  ~MToonSourcePacks(){clear(true);}
};
} // namespace dome
