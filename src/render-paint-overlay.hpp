// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/render.hpp"
#include "paint-overlay-packet.hpp"
#include <map>
#include <tuple>
#include <cstring>
#include <algorithm>
#include <cstdint>
namespace dome::detail {
// Five GPU-only premultiplied contribution planes. No source texture/material
// is flattened into them: original sampling and absolute background stay live.
class PaintOverlayCompiler {
public:
 struct Resource {
  std::array<GLuint,5> planes{};size_t bytes=0;uint32_t resolution=0;
  std::shared_ptr<const PaintOverlayPacket> owner;bool pbr=false;
 };
private:
 using Key=std::pair<const PaintOverlayPacket*,bool>;
 static std::shared_ptr<const PaintOverlayPacket> contribution(const std::shared_ptr<const PaintOverlayPacket> &packet){
  if(!packet)throw std::invalid_argument("Missing immutable overlay input");
  return packet->contribution_owner()?packet->contribution_owner():packet;
 }
 std::map<Key,Resource> cache_;GLuint program_=0,vao_=0;
 static GLuint shader(GLenum type,const char *source){
  GLuint id=glCreateShader(type);if(!id)throw std::runtime_error("Cannot allocate overlay shader");glShaderSource(id,1,&source,nullptr);glCompileShader(id);GLint ok=0;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);
  if(!ok){char message[4096]{};glGetShaderInfoLog(id,sizeof(message),nullptr,message);glDeleteShader(id);throw std::runtime_error(message);}return id;
 }
 void initialise(){if(program_)return;GLuint v=0,f=0,p=0;
  try{v=shader(GL_VERTEX_SHADER,R"GLSL(#version 330 core
void main(){vec2 p=vec2(gl_VertexID==1?3:-1,gl_VertexID==2?3:-1);gl_Position=vec4(p,0,1);})GLSL");
   f=shader(GL_FRAGMENT_SHADER,R"GLSL(#version 330 core
uniform usamplerBuffer common_pixels;uniform samplerBuffer physical_pixels;uniform isamplerBuffer tile_offsets,padding;
uniform sampler2D baked_color,baked_orm,baked_emission,baked_coverage;
uniform int resolution,layer_count,has_baked;uniform float layer_opacity[16];
layout(location=0)out vec4 colour;layout(location=1)out vec4 orm;layout(location=2)out vec4 legacy;layout(location=3)out vec4 coverage;layout(location=4)out vec4 physical;
float byte_value(uvec4 p,int i){return float((p[i/4]>>uint((i%4)*8))&255u)/255.0;}
vec3 decode_rgb(vec3 c){return mix(c/12.92,pow((c+.055)/1.055,vec3(2.4)),greaterThan(c,vec3(.04045)));}
float ph(int offset,int component){return texelFetch(physical_pixels,offset+component).r;}
void main(){
 colour=orm=legacy=coverage=physical=vec4(0);ivec2 out_pixel=ivec2(gl_FragCoord.xy);int at=out_pixel.y*resolution+out_pixel.x;int owner=texelFetch(padding,at).r;if(owner<0)return;
 ivec2 p=ivec2(owner%resolution,owner/resolution);
 if(has_baked==1){float a=texelFetch(baked_coverage,p,0).r;vec4 c=texelFetch(baked_color,p,0);c.rgb=decode_rgb(c.rgb);colour=c*a;coverage=vec4(a);orm=vec4(texelFetch(baked_orm,p,0).rgb*a,a);legacy=vec4(decode_rgb(texelFetch(baked_emission,p,0).rgb)*a,a);physical.a=a;}
 int columns=resolution/32,tile=(p.y/32)*columns+p.x/32,within=(p.y%32)*32+p.x%32;
 for(int layer=0;layer<layer_count;++layer){ivec2 offsets=texelFetch(tile_offsets,layer*columns*columns+tile).rg;if(offsets.x<0)continue;uvec4 q=texelFetch(common_pixels,offsets.x+within);
  float mask=byte_value(q,13)*layer_opacity[layer];vec4 a=vec4(byte_value(q,9),byte_value(q,10),byte_value(q,11),byte_value(q,12))*mask;
  vec4 c=vec4(decode_rgb(vec3(byte_value(q,0),byte_value(q,1),byte_value(q,2))),byte_value(q,3));colour=colour*(1-a.x)+c*a.x;
  orm.g=orm.g*(1-a.z)+byte_value(q,5)*a.z;orm.b=orm.b*(1-a.y)+byte_value(q,4)*a.y;
  vec3 l=decode_rgb(vec3(byte_value(q,6),byte_value(q,7),byte_value(q,8))),h=vec3(0);
  if(offsets.y>=0){int i=offsets.y+within*7;l=vec3(ph(i,0),ph(i,1),ph(i,2));h=vec3(ph(i,3),ph(i,4),ph(i,5));a.w=mask*ph(i,6);}
  legacy.rgb=legacy.rgb*(1-a.w)+l*a.w;physical.rgb=physical.rgb*(1-a.w)+h*a.w;coverage=coverage*(1-a)+a;
 }
 legacy.a=physical.a=coverage.a;
})GLSL");
   p=glCreateProgram();if(!p)throw std::runtime_error("Cannot allocate overlay program");glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok){char message[4096]{};glGetProgramInfoLog(p,sizeof(message),nullptr,message);throw std::runtime_error(message);}glGenVertexArrays(1,&vao_);if(!vao_)throw std::runtime_error("Cannot allocate overlay vertex array");program_=p;p=0;
  }catch(...){if(p)glDeleteProgram(p);if(v)glDeleteShader(v);if(f)glDeleteShader(f);if(vao_)glDeleteVertexArrays(1,&vao_);vao_=0;throw;}
  glDeleteShader(v);glDeleteShader(f);
 }
 struct State {
  GLint fbo=0,viewport[4]{},program=0,vao=0,active=0,unpack=0,buffer=0;GLint polygon[2]{};std::vector<GLboolean> clips,blending;std::vector<std::array<GLboolean,4>> colours;
  static constexpr std::array<GLenum,10> switches{GL_DEPTH_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_FRAMEBUFFER_SRGB,GL_RASTERIZER_DISCARD,GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE,GL_DITHER,GL_SAMPLE_MASK};
  std::array<GLboolean,switches.size()> enabled{};std::array<GLint,8> two_d{},tbo{},sampler{};
  static constexpr std::array<GLenum,6> stores{GL_UNPACK_ALIGNMENT,GL_UNPACK_ROW_LENGTH,GL_UNPACK_IMAGE_HEIGHT,GL_UNPACK_SKIP_PIXELS,GL_UNPACK_SKIP_ROWS,GL_UNPACK_SKIP_IMAGES};std::array<GLint,stores.size()> values{};
  State(){GLint draw=0,clip=0;glGetIntegerv(GL_MAX_DRAW_BUFFERS,&draw);glGetIntegerv(GL_MAX_CLIP_DISTANCES,&clip);colours.resize(size_t(std::max(0,draw)));blending.resize(colours.size());clips.resize(size_t(std::max(0,clip)));glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&fbo);glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpack);glGetIntegerv(GL_TEXTURE_BUFFER_BINDING,&buffer);for(size_t i=0;i<colours.size();++i){glGetBooleani_v(GL_COLOR_WRITEMASK,GLuint(i),colours[i].data());blending[i]=glIsEnabledi(GL_BLEND,GLuint(i));}glDisable(GL_BLEND);glGetIntegerv(GL_POLYGON_MODE,polygon);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);for(size_t i=0;i<clips.size();++i){clips[i]=glIsEnabled(GL_CLIP_DISTANCE0+i);glDisable(GL_CLIP_DISTANCE0+i);}
   for(size_t i=0;i<switches.size();++i){enabled[i]=glIsEnabled(switches[i]);glDisable(switches[i]);}glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
   for(size_t i=0;i<stores.size();++i){glGetIntegerv(stores[i],&values[i]);glPixelStorei(stores[i],i==0?1:0);}
   for(size_t i=0;i<8;++i){glActiveTexture(GL_TEXTURE0+i);glGetIntegerv(GL_TEXTURE_BINDING_2D,&two_d[i]);glGetIntegerv(GL_TEXTURE_BINDING_BUFFER,&tbo[i]);glGetIntegeri_v(GL_SAMPLER_BINDING,GLuint(i),&sampler[i]);glBindSampler(GLuint(i),0);}
  }
  ~State(){glPolygonMode(GL_FRONT_AND_BACK,GLenum(polygon[0]));for(size_t i=0;i<clips.size();++i)if(clips[i])glEnable(GL_CLIP_DISTANCE0+i);else glDisable(GL_CLIP_DISTANCE0+i);for(size_t i=0;i<8;++i){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,GLuint(two_d[i]));glBindTexture(GL_TEXTURE_BUFFER,GLuint(tbo[i]));glBindSampler(GLuint(i),GLuint(sampler[i]));}glActiveTexture(GLenum(active));glBindBuffer(GL_TEXTURE_BUFFER,GLuint(buffer));glBindBuffer(GL_PIXEL_UNPACK_BUFFER,GLuint(unpack));for(size_t i=0;i<stores.size();++i)glPixelStorei(stores[i],values[i]);for(size_t i=0;i<switches.size();++i)if(enabled[i])glEnable(switches[i]);else glDisable(switches[i]);for(size_t i=0;i<colours.size();++i){glColorMaski(GLuint(i),colours[i][0],colours[i][1],colours[i][2],colours[i][3]);if(blending[i])glEnablei(GL_BLEND,GLuint(i));else glDisablei(GL_BLEND,GLuint(i));}glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(fbo));glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glUseProgram(GLuint(program));glBindVertexArray(GLuint(vao));}
 };
 struct Temporary {
  std::array<GLuint,4> buffers{},views{},baked{};GLuint fbo=0;
  ~Temporary(){glDeleteBuffers(4,buffers.data());glDeleteTextures(4,views.data());glDeleteTextures(4,baked.data());if(fbo)glDeleteFramebuffers(1,&fbo);}
 };
 static void image(GLuint name,uint32_t width,uint32_t height,GLenum internal,GLenum format,const void *pixels){glBindTexture(GL_TEXTURE_2D,name);glTexImage2D(GL_TEXTURE_2D,0,GLint(internal),GLsizei(width),GLsizei(height),0,format,GL_UNSIGNED_BYTE,pixels);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);}
 Resource build(std::shared_ptr<const PaintOverlayPacket> packet,bool pbr,size_t budget,size_t working_budget){
  Resource result;result.owner=std::move(packet);result.pbr=pbr;result.resolution=result.owner->resolution();const auto &input=*result.owner;
  if(input.empty())return result;
  const size_t grid=size_t(input.resolution()/32)*(input.resolution()/32),pixels=size_t(input.resolution())*input.resolution();
  size_t mip_pixels=0;for(size_t width=input.resolution();;width=std::max(size_t(1),width/2)){mip_pixels+=width*width;if(width==1)break;}result.bytes=mip_pixels*16*5;
  if(result.bytes>budget)throw std::length_error("GPU overlay byte budget exceeded");
  struct CommonRef{const PaintTile*tile;const PaintColorTile*colour;size_t destination;};
  struct PhysicalRef{const PaintEmissionTile*tile;size_t destination;};
  size_t common_refs=0,physical_refs=0;
  for(const auto &layer:input.layers())for(size_t tile=0;tile<grid;++tile)if(layer.tiles[tile]){++common_refs;if(!layer.emission_tiles.empty()&&layer.emission_tiles[tile])++physical_refs;}
  auto add=[](size_t &total,size_t amount,size_t limit,const char*error){if(total>limit||amount>limit-total)throw std::length_error(error);total+=amount;};
  // Allocate only small, prospectively bounded pointer records to discover
  // actual aliasing. Sorting is in-place; no map-node allocation or pixel
  // packing occurs until both CPU scratch and GPU limits have passed.
  size_t key_bytes=0;add(key_bytes,common_refs*sizeof(CommonRef),working_budget,"GPU overlay planning exceeds CPU working budget");add(key_bytes,physical_refs*sizeof(PhysicalRef),working_budget,"GPU overlay planning exceeds CPU working budget");
  std::vector<CommonRef> common_keys;std::vector<PhysicalRef> physical_keys;common_keys.reserve(common_refs);physical_keys.reserve(physical_refs);
  for(size_t layer=0;layer<input.layers().size();++layer)for(size_t tile=0;tile<grid;++tile){const auto &l=input.layers()[layer];if(!l.tiles[tile])continue;const auto *colour=pbr&&!l.pbr_tiles.empty()?l.pbr_tiles[tile].get():nullptr;common_keys.push_back({l.tiles[tile].get(),colour,layer*grid+tile});if(!l.emission_tiles.empty()&&l.emission_tiles[tile])physical_keys.push_back({l.emission_tiles[tile].get(),layer*grid+tile});}
  auto common_key=[](const CommonRef &v){return std::pair{reinterpret_cast<uintptr_t>(v.tile),reinterpret_cast<uintptr_t>(v.colour)};};
  std::sort(common_keys.begin(),common_keys.end(),[&](const auto&a,const auto&b){return common_key(a)<common_key(b);});std::sort(physical_keys.begin(),physical_keys.end(),[](const auto&a,const auto&b){return reinterpret_cast<uintptr_t>(a.tile)<reinterpret_cast<uintptr_t>(b.tile);});
  size_t common_count=0,physical_count=0;for(size_t i=0;i<common_keys.size();++i)if(!i||common_key(common_keys[i])!=common_key(common_keys[i-1]))++common_count;for(size_t i=0;i<physical_keys.size();++i)if(!i||physical_keys[i].tile!=physical_keys[i-1].tile)++physical_count;
  const size_t common_pixels=std::max(size_t(1),common_count*1024),physical_values=std::max(size_t(1),physical_count*1024*7),offset_count=std::max(size_t(1),input.layers().size()*grid);
  size_t upload=0;for(auto size:{common_pixels*16,physical_values*4,offset_count*8,pixels*4,input.baked()?pixels*13:16})add(upload,size,budget-result.bytes,"GPU overlay byte budget exceeded");
  size_t working=key_bytes;for(auto size:{common_pixels*16,physical_values*4,offset_count*8})add(working,size,working_budget,"GPU overlay sparse packing exceeds CPU working budget");
  // A known byte refusal stays entirely in owned CPU code. Query the driver
  // only after that forecast, still before any large pixel-vector allocation.
  GLint max_buffer=0,max_size=0,max_draw=0;glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE,&max_buffer);glGetIntegerv(GL_MAX_TEXTURE_SIZE,&max_size);glGetIntegerv(GL_MAX_DRAW_BUFFERS,&max_draw);
  if(pixels>size_t(std::max(0,max_buffer))||common_pixels>size_t(std::max(0,max_buffer))||physical_values>size_t(std::max(0,max_buffer))||offset_count>size_t(std::max(0,max_buffer))||input.resolution()>uint32_t(std::max(0,max_size))||max_draw<5)throw std::length_error("GPU overlay exceeds device limits");
  std::vector<std::array<uint32_t,4>> common;std::vector<float> physical;std::vector<std::array<int32_t,2>> offsets(offset_count,{-1,-1});common.reserve(common_pixels);physical.reserve(physical_values);
  int32_t common_offset=-1,physical_offset=-1;
  for(size_t i=0;i<common_keys.size();++i){const auto &key=common_keys[i];if(!i||common_key(key)!=common_key(common_keys[i-1])){common_offset=int32_t(common.size());for(size_t px=0;px<1024;++px){std::array<uint8_t,16> bytes{};std::copy(key.tile->pixels[px].data.begin(),key.tile->pixels[px].data.end(),bytes.begin());if(key.colour)std::copy(key.colour->pixels[px].begin(),key.colour->pixels[px].end(),bytes.begin());std::array<uint32_t,4> words{};for(size_t b=0;b<16;++b)words[b/4]|=uint32_t(bytes[b])<<((b%4)*8);common.push_back(words);}}offsets[key.destination][0]=common_offset;}
  for(size_t i=0;i<physical_keys.size();++i){const auto &key=physical_keys[i];if(!i||key.tile!=physical_keys[i-1].tile){physical_offset=int32_t(physical.size());for(const auto &value:key.tile->pixels){physical.insert(physical.end(),value.legacy.begin(),value.legacy.end());physical.insert(physical.end(),value.physical.begin(),value.physical.end());physical.push_back(value.coverage);}}offsets[key.destination][1]=physical_offset;}
  if(common.empty())common.push_back({});if(physical.empty())physical.push_back(0);
  State state;initialise();Temporary temp;
  try{
   glGenBuffers(4,temp.buffers.data());glGenTextures(4,temp.views.data());glGenTextures(4,temp.baked.data());glGenTextures(5,result.planes.data());glGenFramebuffers(1,&temp.fbo);
   auto buffer=[&](size_t n,GLenum format,size_t bytes,const void *data){glActiveTexture(GL_TEXTURE0+n);glBindBuffer(GL_TEXTURE_BUFFER,temp.buffers[n]);glBufferData(GL_TEXTURE_BUFFER,GLsizeiptr(bytes),data,GL_STATIC_DRAW);glBindTexture(GL_TEXTURE_BUFFER,temp.views[n]);glTexBuffer(GL_TEXTURE_BUFFER,format,temp.buffers[n]);};
   buffer(0,GL_RGBA32UI,common.size()*16,common.data());buffer(1,GL_R32F,physical.size()*4,physical.data());buffer(2,GL_RG32I,offsets.size()*8,offsets.data());buffer(3,GL_R32I,pixels*4,input.padding()->data());
   std::array<uint8_t,4> zero{};for(size_t n=0;n<4;++n){glActiveTexture(GL_TEXTURE4+n);const auto baked=input.baked();const Image *im=nullptr;if(baked&&n<3)im=n==0?(pbr&&baked->color_interpretation==BakedColorInterpretation::ResolvedPbr?&baked->maps.pbr_color:&baked->maps.color):n==1?&baked->maps.metal_rough:&baked->maps.emission;
    image(temp.baked[n],baked?input.resolution():1,baked?input.resolution():1,n==3?GL_R8:GL_RGBA8,n==3?GL_RED:GL_RGBA,baked?(n==3?static_cast<const void*>(baked->coverage.data()):static_cast<const void*>(im->rgba.data())):zero.data());}
   glBindFramebuffer(GL_DRAW_FRAMEBUFFER,temp.fbo);std::array<GLenum,5> attachments{};for(size_t n=0;n<5;++n){glBindTexture(GL_TEXTURE_2D,result.planes[n]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,GLsizei(input.resolution()),GLsizei(input.resolution()),0,GL_RGBA,GL_FLOAT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);attachments[n]=GL_COLOR_ATTACHMENT0+n;glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,attachments[n],GL_TEXTURE_2D,result.planes[n],0);}glDrawBuffers(5,attachments.data());
   if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("GPU overlay framebuffer incomplete");
   for(size_t n=0;n<4;++n){glActiveTexture(GL_TEXTURE4+n);glBindTexture(GL_TEXTURE_2D,temp.baked[n]);}
   glViewport(0,0,GLsizei(input.resolution()),GLsizei(input.resolution()));glUseProgram(program_);glBindVertexArray(vao_);
   const char *names[]{"common_pixels","physical_pixels","tile_offsets","padding","baked_color","baked_orm","baked_emission","baked_coverage"};for(int i=0;i<8;++i)glUniform1i(glGetUniformLocation(program_,names[i]),i);glUniform1i(glGetUniformLocation(program_,"resolution"),GLint(input.resolution()));glUniform1i(glGetUniformLocation(program_,"layer_count"),GLint(input.layers().size()));glUniform1i(glGetUniformLocation(program_,"has_baked"),bool(input.baked()));std::array<float,16> opacity{};for(size_t i=0;i<input.layers().size();++i)opacity[i]=input.layers()[i].opacity;glUniform1fv(glGetUniformLocation(program_,"layer_opacity"),16,opacity.data());glDrawArrays(GL_TRIANGLES,0,3);for(auto plane:result.planes){glBindTexture(GL_TEXTURE_2D,plane);glGenerateMipmap(GL_TEXTURE_2D);}
   if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("GPU sparse overlay compilation failed");
  }catch(...){glDeleteTextures(5,result.planes.data());throw;}
  return result;
 }
public:
 PaintOverlayCompiler()=default;PaintOverlayCompiler(const PaintOverlayCompiler&)=delete;PaintOverlayCompiler&operator=(const PaintOverlayCompiler&)=delete;
 struct Input {std::shared_ptr<const PaintOverlayPacket> owner;bool pbr=false;};
 static size_t texture_bytes(const std::shared_ptr<const PaintOverlayPacket> &owner){
  if(!owner)throw std::invalid_argument("Missing immutable overlay input");if(owner->empty())return 0;
  size_t pixels=0;for(size_t width=owner->resolution();;width=std::max(size_t(1),width/2)){pixels+=width*width;if(width==1)break;}return pixels*16*5;
 }
 static size_t texture_images(const std::shared_ptr<const PaintOverlayPacket> &owner){if(!owner)throw std::invalid_argument("Missing immutable overlay input");return owner->empty()?0:5;}

 void prepare(const std::vector<Input> &inputs,size_t budget=1024ull*1024*1024,size_t max_images=1024,size_t working_budget=256ull*1024*1024){
  if(inputs.size()>512)throw std::length_error("GPU overlay descriptor count exceeds two interpretations per native object");
  std::map<Key,Input> wanted;size_t bytes=0,images=0;
  for(const auto &input:inputs){auto owner=contribution(input.owner);Key key{owner.get(),input.pbr};if(wanted.contains(key))continue;
    const auto size=texture_bytes(owner);images+=texture_images(owner);if(size>budget-bytes)throw std::length_error("GPU overlay cache byte budget exceeded");
    bytes+=size;wanted.emplace(key,Input{std::move(owner),input.pbr});}
  if(images>max_images)throw std::length_error("GPU overlay image count exceeded");
  for(auto it=cache_.begin();it!=cache_.end();)if(!wanted.contains(it->first)){glDeleteTextures(5,it->second.planes.data());it=cache_.erase(it);}else ++it;
  std::vector<Key> added;added.reserve(wanted.size());
  try{for(const auto &[key,input]:wanted)if(!cache_.contains(key)){auto value=build(input.owner,input.pbr,budget-(bytes-(texture_bytes(input.owner))),working_budget);try{cache_.emplace(key,std::move(value));added.push_back(key);}catch(...){glDeleteTextures(5,value.planes.data());throw;}}}
  catch(...){for(const auto &key:added){auto it=cache_.find(key);glDeleteTextures(5,it->second.planes.data());cache_.erase(it);}throw;}
 }
 const Resource &at(const std::shared_ptr<const PaintOverlayPacket> &packet,bool pbr)const{return cache_.at({contribution(packet).get(),pbr});}
 size_t bytes()const{size_t count=0;for(const auto &[key,value]:cache_)count+=value.bytes;return count;}
 size_t size()const noexcept{return cache_.size();}
 size_t image_count()const noexcept{size_t n=0;for(const auto &[key,value]:cache_)if(value.bytes)n+=5;return n;}
 void clear(bool release){if(release)for(const auto &[key,value]:cache_)glDeleteTextures(5,value.planes.data());cache_.clear();if(release){if(program_)glDeleteProgram(program_);if(vao_)glDeleteVertexArrays(1,&vao_);}program_=vao_=0;}
 ~PaintOverlayCompiler(){clear(true);}
};
}
