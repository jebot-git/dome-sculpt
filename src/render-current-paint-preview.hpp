// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "current-paint-preview.hpp"
#include "paint-composite-backend.hpp"
#include "render-paint-state.hpp"
#include "render-store-ledger.hpp"
#include <map>
#include <set>
#include <cstring>
#include <limits>
namespace dome::detail {
// Context-owned rendering cache, never an editable field or serialization owner.
// Common inputs retain native bytes; only genuine emission uses FLOAT inputs.
class CurrentPaintPreviewCompiler {
public:
 struct Stats {size_t builds=0,common_tiles=0,emission_tiles=0,payload_bytes=0,directory_bytes=0,padding_bytes=0,quantization_bytes=0,requested_bytes=0,peak_bytes=0,retired_bytes=0;};
 struct Resource {
  std::shared_ptr<const CurrentGridPaintPreview> owner;
  std::array<GLuint,5> textures{};
  bool physical=false;
 };
private:
 using Ledger=ContextStoreLedger;
 struct Store {GLuint name=0;bool texture=false;size_t bytes=0;Ledger::Store lease;};
 template<class T>struct OwnerLess {
  bool operator()(const std::shared_ptr<const T>&a,const std::shared_ptr<const T>&b)const noexcept {
   if(a.get()!=b.get())return std::less<const T*>{}(a.get(),b.get());return a.owner_before(b);
  }
 };
 struct Entry {Resource resource;std::array<Store,5> images{};Store directory,view;};
 struct Padding {Store buffer,view;};
 template<class T>struct Pool {
  struct Slot {std::shared_ptr<const T> owner;uint64_t used=0;};
  std::map<std::shared_ptr<const T>,size_t,OwnerLess<T>> index;
  std::vector<Slot> slots;Store buffer,view;size_t capacity=0;
 };
 using Data=std::shared_ptr<const CurrentGridPaintPreview>;
 using PaddingOwner=std::shared_ptr<const std::vector<int32_t>>;
 std::map<Data,Entry,OwnerLess<CurrentGridPaintPreview>> entries_;
 std::map<PaddingOwner,Padding,OwnerLess<std::vector<int32_t>>> padding_;
 Pool<PaintTile> common_;Pool<PaintEmissionTile> emission_;
 std::shared_ptr<Ledger> ledger_;Ledger::Group group_=0;bool own_ledger_=false;GLsync fence_=nullptr;uint64_t epoch_=0,sealed_=0,completed_=0;
 bool dirty_=false;size_t requested_=0,images_=0,budget_=0,max_images_=0;Stats stats_;
 GLuint program_=0,vao_=0;
 static Data data(const Data&owner){
  if(!owner)throw std::invalid_argument("Missing actual current-grid preview owner");owner->authenticate_owner(owner);
  auto result=owner->contribution_owner()?owner->contribution_owner():owner;result->authenticate_owner(result);return result;
 }
 void sync_usage(){const auto u=ledger_->usage(group_);requested_=u.bytes;images_=u.images;stats_.retired_bytes=u.retired_bytes;stats_.peak_bytes=std::max(stats_.peak_bytes,u.peak_bytes);}
 void charge(size_t n){sync_usage();if(requested_>budget_||n>budget_-requested_)throw std::length_error("Current-grid requested GPU stores exceed old/new/retirement budget");}
 void check(){if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Current-grid GPU allocation/upload failed");}
 void retire(Store&s)noexcept{s.lease.reset();s={};}
 void retire(Entry&e)noexcept{for(auto&s:e.images)retire(s);retire(e.view);retire(e.directory);}
 void destroy(Store&s)noexcept{retire(s);}
 void seal(bool force=false){
  if(!dirty_&&!force)return;const auto next=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
  if(!next){(void)glGetError();throw std::runtime_error("Current-grid GPU retirement fence creation failed; debt retained");}
  if(fence_)glDeleteSync(fence_);fence_=next;sealed_=epoch_;dirty_=false;glFlush();
 }
 void collect(){
  if(own_ledger_)ledger_->collect();sync_usage();if(dirty_)seal();if(!fence_)return;const auto status=glClientWaitSync(fence_,0,0);
  if(status==GL_WAIT_FAILED){(void)glGetError();throw std::runtime_error("Current-grid GPU retirement fence query failed; debt retained");}
  if(status!=GL_ALREADY_SIGNALED&&status!=GL_CONDITION_SATISFIED)return;
  completed_=sealed_;glDeleteSync(fence_);fence_=nullptr;
  sync_usage();
 }
 Store buffer(size_t bytes){
  charge(bytes);Store out;out.bytes=bytes;out.lease=ledger_->create(Ledger::Kind::Buffer,bytes,Ledger::Domain::Material,group_);out.name=out.lease.name();
  glBindBuffer(GL_COPY_WRITE_BUFFER,out.name);glBufferData(GL_COPY_WRITE_BUFFER,GLsizeiptr(bytes),nullptr,GL_DYNAMIC_DRAW);
  sync_usage();dirty_=true;
  try{check();}catch(...){retire(out);throw;}return out;
 }
 Store view(const Store&storage,GLenum format){
  if(images_>=max_images_)throw std::length_error("Current-grid GPU texture/view count exceeded");
  Store out;out.texture=true;out.lease=ledger_->create(Ledger::Kind::Texture,0,Ledger::Domain::Material,group_,std::span(&storage.lease,1));out.name=out.lease.name();sync_usage();dirty_=true;
  try{glBindTexture(GL_TEXTURE_BUFFER,out.name);glTexBuffer(GL_TEXTURE_BUFFER,format,storage.name);check();}catch(...){retire(out);throw;}return out;
 }
 Store image(uint32_t r,GLenum format){
  const size_t bytes=size_t(r)*r*(format==GL_RGBA32F?16:4);charge(bytes);
  if(images_>=max_images_)throw std::length_error("Current-grid GPU texture/view count exceeded");
  Store out;out.texture=true;out.bytes=bytes;out.lease=ledger_->create(Ledger::Kind::Texture,bytes,Ledger::Domain::Material,group_);out.name=out.lease.name();sync_usage();dirty_=true;
  try{glBindTexture(GL_TEXTURE_2D,out.name);glTexImage2D(GL_TEXTURE_2D,0,GLint(format),GLsizei(r),GLsizei(r),0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
   for(auto parameter:{GL_TEXTURE_MIN_FILTER,GL_TEXTURE_MAG_FILTER})glTexParameteri(GL_TEXTURE_2D,parameter,GL_LINEAR);
   for(auto parameter:{GL_TEXTURE_WRAP_S,GL_TEXTURE_WRAP_T})glTexParameteri(GL_TEXTURE_2D,parameter,GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,0);check();
  }catch(...){retire(out);throw;}return out;
 }
 template<class T>static size_t wanted_capacity(const Pool<T>&pool,
     const std::set<std::shared_ptr<const T>,OwnerLess<T>>&wanted,uint64_t completed){
  size_t needed=0,free=pool.capacity-pool.slots.size();
  for(const auto&key:wanted)if(!pool.index.contains(key))++needed;
  for(const auto&s:pool.slots)if(s.used<=completed&&(!s.owner||!wanted.contains(s.owner)))++free;
  if(needed<=free)return pool.capacity;
  size_t next=std::max(size_t(4),pool.capacity);while(next-pool.capacity<needed-free){if(next>SIZE_MAX/2)throw std::length_error("Current-grid input pool overflow");next*=2;}return next;
 }
 template<class T>void reserve(Pool<T>&pool,size_t capacity,size_t prefix,size_t tile_bytes,GLenum format){
  if(capacity==pool.capacity&&pool.buffer.name){if(!pool.view.name)pool.view=view(pool.buffer,format);return;}
  if(capacity>(SIZE_MAX-prefix)/tile_bytes)throw std::length_error("Current-grid input store overflow");
  const size_t bytes=std::max(size_t(4),prefix+capacity*tile_bytes);pool.slots.reserve(capacity);
  Store next=buffer(bytes);
  try{
   if(pool.buffer.name){glBindBuffer(GL_COPY_READ_BUFFER,pool.buffer.name);glBindBuffer(GL_COPY_WRITE_BUFFER,next.name);glCopyBufferSubData(GL_COPY_READ_BUFFER,GL_COPY_WRITE_BUFFER,0,0,GLsizeiptr(pool.buffer.bytes));check();}
   else if(prefix){const auto&q=paint_compute::quantization();static_assert(sizeof(q)==3072);glBindBuffer(GL_COPY_WRITE_BUFFER,next.name);glBufferSubData(GL_COPY_WRITE_BUFFER,0,sizeof(q),&q);stats_.quantization_bytes+=sizeof(q);check();}
  }catch(...){retire(next);throw;}
  Store next_view;try{next_view=view(next,format);}catch(...){retire(next);throw;}
  retire(pool.view);retire(pool.buffer);pool.buffer=std::move(next);pool.view=std::move(next_view);pool.capacity=capacity;
 }
 template<class T,class Pack>size_t slot(Pool<T>&pool,const std::shared_ptr<const T>&owner,
     const std::set<std::shared_ptr<const T>,OwnerLess<T>>&wanted,size_t prefix,size_t tile_bytes,Pack pack){
  if(auto found=pool.index.find(owner);found!=pool.index.end()){pool.slots[found->second].used=epoch_;return found->second;}
  size_t index=pool.slots.size();
  for(size_t i=0;i<pool.slots.size();++i)if(pool.slots[i].used<=completed_&&(!pool.slots[i].owner||!wanted.contains(pool.slots[i].owner))){index=i;break;}
  if(index==pool.slots.size()){if(index>=pool.capacity)throw std::logic_error("Current-grid pool forecast lost a required slot");pool.slots.push_back({});}
  auto&selected=pool.slots[index];const auto previous=pool.index.find(selected.owner);
  auto inserted=pool.index.emplace(owner,index);if(previous!=pool.index.end()&&previous!=inserted.first)pool.index.erase(previous);selected.owner=owner;selected.used=epoch_;
  try{auto packed=pack(*owner);glBindBuffer(GL_COPY_WRITE_BUFFER,pool.buffer.name);glBufferSubData(GL_COPY_WRITE_BUFFER,GLintptr(prefix+index*tile_bytes),GLsizeiptr(tile_bytes),packed.data());dirty_=true;check();}
  catch(...){pool.index.erase(inserted.first);selected.owner.reset();throw;}
  stats_.payload_bytes+=tile_bytes;if constexpr(std::is_same_v<T,PaintTile>)++stats_.common_tiles;else ++stats_.emission_tiles;return index;
 }
 static GLuint shader(GLenum kind,const char*text){
  const auto id=glCreateShader(kind);if(!id)throw std::runtime_error("Cannot allocate current-grid shader");glShaderSource(id,1,&text,nullptr);glCompileShader(id);GLint okay=0;glGetShaderiv(id,GL_COMPILE_STATUS,&okay);
  if(!okay){char log[4096]{};glGetShaderInfoLog(id,sizeof(log),nullptr,log);glDeleteShader(id);throw std::runtime_error(log);}return id;
 }
 void initialise(){
  if(program_)return;bool precise=false;GLint count=0;glGetIntegerv(GL_NUM_EXTENSIONS,&count);
  for(GLint i=0;i<count;++i)if(std::strcmp(reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS,GLuint(i))),"GL_ARB_gpu_shader5")==0)precise=true;
  if(!precise)throw std::runtime_error("Canonical current-grid preview requires precise GPU arithmetic; no CPU composite fallback");
  const char*vertex="#version 330 core\nvoid main(){gl_Position=vec4(gl_VertexID==1?3:-1,gl_VertexID==2?3:-1,0,1);}";
  const char*fragment=R"GLSL(#version 330 core
#extension GL_ARB_gpu_shader5 : require
uniform usamplerBuffer common_pixels;uniform samplerBuffer physical_pixels;uniform isamplerBuffer directory,padding;
uniform int resolution,layer_count,physical_mode;uniform float layer_opacity[16];uniform vec4 background_color;uniform vec3 background_emission;uniform float background_metal,background_rough;
layout(location=0)out vec4 colour;layout(location=1)out vec4 orm;layout(location=2)out vec4 legacy;layout(location=3)out vec4 coverage;layout(location=4)out vec4 physical;
uint word(int at){return texelFetch(common_pixels,at/4)[at%4];}
float unit_value(uint at){return uintBitsToFloat(word(int(at)));}float srgb_value(uint at){return uintBitsToFloat(word(256+int(at)));}
uint byte_at(uvec4 p,int at){return (p[at/4]>>uint((at%4)*8))&255u;}
float mul(float a,float b){precise float v=a*b;return v;}float add(float a,float b){precise float v=a+b;return v;}float sub(float a,float b){precise float v=a-b;return v;}
float blend(float a,float b,float opacity){return add(mul(a,sub(1,opacity)),mul(b,opacity));}
float linear_byte(float v){if(v<=0)return 0.0;if(v>=1)return 1.0;float scaled=mul(v,255);uint integer=uint(scaled);return float(integer+uint(sub(scaled,float(integer))>=.5))/255;}
float srgb_byte(float v){if(v<=0)return 0.0;if(v>=1)return 1.0;uint bits=floatBitsToUint(v);int lo=0,hi=255;while(lo<hi){int mid=(lo+hi)/2;if(bits>=word(512+mid))lo=mid+1;else hi=mid;}return float(lo)/255;}
void main(){
 vec4 c=background_color;float rough=background_rough,metal=background_metal;vec3 l=physical_mode==1?vec3(0):background_emission,h=vec3(0);float ec=0;
 ivec2 xy=ivec2(gl_FragCoord.xy);int owner=texelFetch(padding,xy.y*resolution+xy.x).r;
 if(owner>=0){int x=owner%resolution,y=owner/resolution,columns=resolution/32,tile=(y/32)*columns+x/32,within=(y%32)*32+x%32;
  for(int layer=0;layer<layer_count;++layer){ivec2 offsets=texelFetch(directory,layer*columns*columns+tile).rg;if(offsets.x<0)continue;uvec4 p=texelFetch(common_pixels,192+offsets.x*1024+within);
   float mask=mul(unit_value(byte_at(p,13)),layer_opacity[layer]);float a=mul(mask,unit_value(byte_at(p,9)));
   for(int k=0;k<4;++k)c[k]=blend(c[k],k<3?srgb_value(byte_at(p,k)):unit_value(byte_at(p,k)),a);
   metal=blend(metal,unit_value(byte_at(p,4)),mul(mask,unit_value(byte_at(p,10))));rough=blend(rough,unit_value(byte_at(p,5)),mul(mask,unit_value(byte_at(p,11))));
   float ea=mul(mask,unit_value(byte_at(p,12)));vec3 el=vec3(srgb_value(byte_at(p,6)),srgb_value(byte_at(p,7)),srgb_value(byte_at(p,8))),eh=vec3(0);
   if(physical_mode==1&&offsets.y>=0){int q=offsets.y*7168+within*7;el=vec3(texelFetch(physical_pixels,q).r,texelFetch(physical_pixels,q+1).r,texelFetch(physical_pixels,q+2).r);eh=vec3(texelFetch(physical_pixels,q+3).r,texelFetch(physical_pixels,q+4).r,texelFetch(physical_pixels,q+5).r);ea=mul(mask,texelFetch(physical_pixels,q+6).r);}
   for(int k=0;k<3;++k){l[k]=blend(l[k],el[k],ea);h[k]=blend(h[k],eh[k],ea);}ec=blend(ec,1,ea);
  }
 }
 colour=vec4(srgb_byte(c.r),srgb_byte(c.g),srgb_byte(c.b),linear_byte(c.a));orm=vec4(1,linear_byte(rough),linear_byte(metal),1);
 legacy=physical_mode==1?vec4(l,ec):vec4(srgb_byte(l.r),srgb_byte(l.g),srgb_byte(l.b),1);coverage=vec4(0,0,0,ec);physical=vec4(h,ec);
})GLSL";
  GLuint v=0,f=0,p=0;try{v=shader(GL_VERTEX_SHADER,vertex);f=shader(GL_FRAGMENT_SHADER,fragment);p=glCreateProgram();if(!p)throw std::runtime_error("Cannot allocate current-grid program");glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);GLint okay=0;glGetProgramiv(p,GL_LINK_STATUS,&okay);if(!okay){char log[4096]{};glGetProgramInfoLog(p,sizeof(log),nullptr,log);throw std::runtime_error(std::string("Cannot link canonical current-grid program: ")+log);}glGenVertexArrays(1,&vao_);if(!vao_)throw std::runtime_error("Cannot allocate current-grid vertex array");program_=p;p=0;}
  catch(...){if(p)glDeleteProgram(p);if(v)glDeleteShader(v);if(f)glDeleteShader(f);throw;}glDeleteShader(v);glDeleteShader(f);
 }
 static size_t resolved_bytes(const Data&d){return size_t(d->layers()->resolution())*d->layers()->resolution()*(d->physical_emission()?56:12);}
 struct Plan {
  std::set<Data,OwnerLess<CurrentGridPaintPreview>> wanted;
  std::set<PaddingOwner,OwnerLess<std::vector<int32_t>>> padding;
  std::set<std::shared_ptr<const PaintTile>,OwnerLess<PaintTile>> common;
  std::set<std::shared_ptr<const PaintEmissionTile>,OwnerLess<PaintEmissionTile>> emission;
  size_t common_capacity=0,emission_capacity=0,peak=0,names=0;
 };
 Plan plan(const std::vector<Data>&inputs,size_t working){
  if(inputs.size()>512)throw std::length_error("Current-grid preview descriptor bound exceeded");Plan p;p.peak=requested_;p.names=images_;size_t scratch=0,directory=0;
  auto cpu=[&](size_t n){if(scratch>working||n>working-scratch)throw std::length_error("Current-grid planning exceeds declared CPU scratch budget");scratch+=n;};
  cpu(inputs.size()*1024+32768);
  auto bytes=[&](size_t n){if(n>SIZE_MAX-p.peak)throw std::length_error("Current-grid store forecast overflow");p.peak+=n;};
  for(const auto&input:inputs){auto d=data(input);if(!p.wanted.insert(d).second)continue;const auto&field=*d->layers();
   if(field.layers().size()>16)throw std::length_error("Current-grid visible layer bound exceeded");
   if(!entries_.contains(d)){bytes(resolved_bytes(d));const auto size=std::max(size_t(1),field.layers().size()*size_t(field.resolution()/32)*(field.resolution()/32))*8;bytes(size);directory=std::max(directory,size);p.names+=(d->physical_emission()?5:3)+1;}
   const auto&pad=field.padding();if(p.padding.insert(pad).second&&!padding_.contains(pad)){bytes(pad->size()*4);++p.names;}
   if(entries_.contains(d))continue;
   for(const auto&l:field.layers())for(size_t i=0;i<l.tiles.size();++i)if(l.tiles[i]){
    cpu(256);p.common.insert(l.tiles[i]);if(!l.emission_tiles.empty()&&l.emission_tiles[i])p.emission.insert(l.emission_tiles[i]);
   }
  }
  cpu(directory);
  p.common_capacity=wanted_capacity(common_,p.common,completed_);p.emission_capacity=wanted_capacity(emission_,p.emission,completed_);
  const bool builds=std::any_of(p.wanted.begin(),p.wanted.end(),[&](const auto&d){return !entries_.contains(d);});
  if(builds){
   if(!common_.buffer.name||p.common_capacity!=common_.capacity)bytes(3072+p.common_capacity*16384);
   if(!emission_.buffer.name||p.emission_capacity!=emission_.capacity)bytes(std::max(size_t(4),p.emission_capacity*28672));
   if(!common_.view.name||p.common_capacity!=common_.capacity)++p.names;if(!emission_.view.name||p.emission_capacity!=emission_.capacity)++p.names;
  }
  return p;
 }
 void build(Entry&entry,const Plan&p){
  const auto&d=entry.resource.owner;const auto&field=*d->layers();const auto r=field.resolution();const size_t grid=size_t(r/32)*(r/32);
  const size_t count=std::max(size_t(1),grid*field.layers().size());std::vector<std::array<int32_t,2>> offsets(count,{-1,-1});
  auto common_pack=[](const PaintTile&t){std::array<std::array<uint32_t,4>,1024> out{};for(size_t i=0;i<1024;++i)for(size_t k=0;k<14;++k)out[i][k/4]|=uint32_t(t.pixels[i].data[k])<<((k%4)*8);return out;};
  auto emission_pack=[](const PaintEmissionTile&t){std::array<float,7168> out{};for(size_t i=0;i<1024;++i){const auto&v=t.pixels[i];v.validate();for(size_t k=0;k<3;++k){out[i*7+k]=v.legacy[k];out[i*7+3+k]=v.physical[k];}out[i*7+6]=v.coverage;}return out;};
  for(size_t li=0;li<field.layers().size();++li){const auto&l=field.layers()[li];for(size_t t=0;t<grid;++t)if(l.tiles[t]){auto&o=offsets[li*grid+t];o[0]=int32_t(slot(common_,l.tiles[t],p.common,3072,16384,common_pack));if(!l.emission_tiles.empty()&&l.emission_tiles[t])o[1]=int32_t(slot(emission_,l.emission_tiles[t],p.emission,0,28672,emission_pack));}}
  entry.directory=buffer(offsets.size()*8);glBindBuffer(GL_COPY_WRITE_BUFFER,entry.directory.name);glBufferSubData(GL_COPY_WRITE_BUFFER,0,GLsizeiptr(offsets.size()*8),offsets.data());check();stats_.directory_bytes+=offsets.size()*8;entry.view=view(entry.directory,GL_RG32I);
  auto pad=padding_.find(field.padding());if(pad==padding_.end())throw std::logic_error("Current-grid padding forecast lost its owner");
  entry.resource.physical=d->physical_emission();const auto images=entry.resource.physical?5:3;
  for(int i=0;i<images;++i){const auto format=i==0?GL_SRGB8_ALPHA8:i==1?GL_RGBA8:i==2&&!entry.resource.physical?GL_SRGB8_ALPHA8:GL_RGBA32F;entry.images[size_t(i)]=image(r,GLenum(format));entry.resource.textures[size_t(i)]=entry.images[size_t(i)].name;}
  GLuint fbo=0;glGenFramebuffers(1,&fbo);if(!fbo)throw std::runtime_error("Cannot allocate current-grid resolve framebuffer");
  try{glBindFramebuffer(GL_DRAW_FRAMEBUFFER,fbo);std::array<GLenum,5> attachments{};for(int i=0;i<images;++i){attachments[size_t(i)]=GL_COLOR_ATTACHMENT0+GLenum(i);glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,attachments[size_t(i)],GL_TEXTURE_2D,entry.images[size_t(i)].name,0);}glDrawBuffers(images,attachments.data());if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Current-grid resolve framebuffer incomplete");
   const std::array<GLuint,4> views{common_.view.name,emission_.view.name,entry.view.name,pad->second.view.name};for(size_t i=0;i<views.size();++i){glActiveTexture(GL_TEXTURE0+GLenum(i));glBindTexture(GL_TEXTURE_BUFFER,views[i]);}
   glUseProgram(program_);glBindVertexArray(vao_);glViewport(0,0,GLsizei(r),GLsizei(r));const char*names[]{"common_pixels","physical_pixels","directory","padding"};for(int i=0;i<4;++i)glUniform1i(glGetUniformLocation(program_,names[i]),i);
   glUniform1i(glGetUniformLocation(program_,"resolution"),GLint(r));glUniform1i(glGetUniformLocation(program_,"layer_count"),GLint(field.layers().size()));glUniform1i(glGetUniformLocation(program_,"physical_mode"),entry.resource.physical);
   std::array<float,16> opacity{};for(size_t i=0;i<field.layers().size();++i)opacity[i]=field.layers()[i].opacity;glUniform1fv(glGetUniformLocation(program_,"layer_opacity"),16,opacity.data());const auto&f=d->factors();glUniform4fv(glGetUniformLocation(program_,"background_color"),1,f.data());glUniform3fv(glGetUniformLocation(program_,"background_emission"),1,f.data()+4);glUniform1f(glGetUniformLocation(program_,"background_metal"),f[7]);glUniform1f(glGetUniformLocation(program_,"background_rough"),f[8]);glDrawArrays(GL_TRIANGLES,0,3);dirty_=true;check();++stats_.builds;
  }catch(...){glDeleteFramebuffers(1,&fbo);throw;}glDeleteFramebuffers(1,&fbo);
 }
public:
 explicit CurrentPaintPreviewCompiler(std::shared_ptr<Ledger> ledger={}):ledger_(ledger?ledger:std::make_shared<Ledger>(Ledger::Limits{2ull*1024*1024*1024,2048,256})),own_ledger_(!ledger){group_=ledger_->create_group();}
 CurrentPaintPreviewCompiler(const CurrentPaintPreviewCompiler&)=delete;
 Ledger::Group group()const noexcept{return group_;}
 size_t forecast(const std::vector<Data>&inputs,size_t working=256ull*1024*1024){collect();return plan(inputs,working).peak;}
 size_t forecast_images(const std::vector<Data>&inputs,size_t working=256ull*1024*1024){collect();return plan(inputs,working).names;}
 using Selection=std::set<Data,OwnerLess<CurrentGridPaintPreview>>;
 static Selection selection(const std::vector<Data>&inputs){Selection result;for(const auto&i:inputs)result.insert(data(i));return result;}
 void retain(const Selection&wanted)noexcept {
  for(auto it=entries_.begin();it!=entries_.end();)if(!wanted.contains(it->first)){retire(it->second);it=entries_.erase(it);}else ++it;
  for(auto it=padding_.begin();it!=padding_.end();)if(!std::any_of(wanted.begin(),wanted.end(),[&](const auto&d){const auto&p=d->layers()->padding();return it->first==p&&!it->first.owner_before(p)&&!p.owner_before(it->first);})){retire(it->second.view);retire(it->second.buffer);it=padding_.erase(it);}else ++it;
  if(wanted.empty()){common_={};emission_={};}
 }
 void prepare(const std::vector<Data>&inputs,size_t budget=1024ull*1024*1024,size_t max_images=1024,size_t working=256ull*1024*1024,bool preserve_existing=false){
  collect();auto p=plan(inputs,working);if(p.peak>budget||p.names>max_images)throw std::length_error("Current-grid old/new/retirement GPU budget exceeded");budget_=budget;max_images_=max_images;
  if(epoch_==UINT64_MAX)throw std::overflow_error("Current-grid GPU operation epoch exhausted");++epoch_;
  ledger_->mark_used();
  std::map<Data,Entry,OwnerLess<CurrentGridPaintPreview>> added;
  for(const auto&d:p.wanted)if(!entries_.contains(d)){auto&entry=added[d];entry.resource.owner=d;}
  if(added.empty()&&p.wanted.size()==entries_.size()&&p.padding.size()==padding_.size())return;
  PaintCompilerState state;
  try{
   if(!added.empty()){
    GLint texels=0,dimension=0,draw=0,attachments=0;glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE,&texels);glGetIntegerv(GL_MAX_TEXTURE_SIZE,&dimension);glGetIntegerv(GL_MAX_DRAW_BUFFERS,&draw);glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS,&attachments);
    if(draw<5||attachments<5||192+p.common_capacity*1024>size_t(std::max(0,texels))||p.emission_capacity*7168>size_t(std::max(0,texels)))throw std::length_error("Current-grid preview exceeds actual GPU buffer/MRT capabilities");
    for(const auto&d:p.wanted){const auto r=d->layers()->resolution();if(r>uint32_t(std::max(0,dimension))||size_t(r)*r>size_t(std::max(0,texels))||d->layers()->layers().size()*size_t(r/32)*(r/32)>size_t(std::max(0,texels)))throw std::length_error("Current-grid preview exceeds actual GPU grid capabilities");}
    initialise();reserve(common_,p.common_capacity,3072,16384,GL_RGBA32UI);reserve(emission_,p.emission_capacity,0,28672,GL_R32F);
   }
   for(const auto&owner:p.padding)if(!padding_.contains(owner)){
    auto inserted=padding_.emplace(owner,Padding{});auto&pad=inserted.first->second;
    try{pad.buffer=buffer(owner->size()*4);glBindBuffer(GL_COPY_WRITE_BUFFER,pad.buffer.name);glBufferSubData(GL_COPY_WRITE_BUFFER,0,GLsizeiptr(owner->size()*4),owner->data());check();stats_.padding_bytes+=owner->size()*4;pad.view=view(pad.buffer,GL_R32I);}catch(...){retire(pad.buffer);retire(pad.view);padding_.erase(inserted.first);throw;}
   }
   for(auto&[key,entry]:added)build(entry,p);
   // Fence all new resolves and the prior frame's last uses BEFORE replacing
   // the old cache. A rejected fence cannot invalidate the still-live frame.
   seal(true);if(own_ledger_)ledger_->seal(true);
   if(!preserve_existing)for(auto it=entries_.begin();it!=entries_.end();)if(!p.wanted.contains(it->first)){retire(it->second);it=entries_.erase(it);}else ++it;
   entries_.merge(added);
   if(!preserve_existing)for(auto it=padding_.begin();it!=padding_.end();)if(!p.padding.contains(it->first)){retire(it->second.view);retire(it->second.buffer);it=padding_.erase(it);}else ++it;
   if(p.wanted.empty()&&!preserve_existing){retire(common_.view);retire(common_.buffer);retire(emission_.view);retire(emission_.buffer);common_={};emission_={};}
  }catch(...){for(auto&[key,entry]:added)retire(entry);try{seal();}catch(...){}if(own_ledger_)ledger_->seal_noexcept();throw;}
 }
 // Raw references are invalidated by pruning prepare/retain/clear. Reacquire
 // before each submission, or mark_used() before submitting retained refs.
 void mark_used()noexcept{ledger_->mark_used();}
 const Resource&at(const Data&owner)const{ledger_->mark_used();return entries_.at(owner->contribution_owner()?owner->contribution_owner():owner).resource;}
 Stats stats()const{auto out=stats_;const auto u=ledger_->usage(group_);out.requested_bytes=u.bytes;out.retired_bytes=u.retired_bytes;out.peak_bytes=std::max(out.peak_bytes,u.peak_bytes);return out;}
 size_t bytes()const noexcept{return ledger_->usage(group_).bytes;}size_t image_count()const noexcept{return ledger_->usage(group_).images;}
 void clear(bool release){
  if(release){for(auto&[key,e]:entries_){for(auto&s:e.images)destroy(s);destroy(e.directory);destroy(e.view);}for(auto&[key,p]:padding_){destroy(p.buffer);destroy(p.view);}destroy(common_.buffer);destroy(common_.view);destroy(emission_.buffer);destroy(emission_.view);if(fence_)glDeleteSync(fence_);if(program_)glDeleteProgram(program_);if(vao_)glDeleteVertexArrays(1,&vao_);}
  entries_.clear();padding_.clear();common_={};emission_={};fence_=nullptr;program_=vao_=0;requested_=images_=0;epoch_=sealed_=completed_=0;dirty_=false;stats_.retired_bytes=0;if(own_ledger_)ledger_->clear(release);
 }
 ~CurrentPaintPreviewCompiler(){clear(true);}
};
} // namespace dome::detail
