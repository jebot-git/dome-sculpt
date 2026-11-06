// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "polygon-surface-range.hpp"
#include <utility>
namespace dome::detail {
// Per-renderer immutable primitive frames. Unit14 already carries a sampler-
// buffer for MToon. Reuse that same sampler declaration as well as its unit:
// two active aliases would consume17 fragment samplers on a minimum16 device.
// Polygon packets reject source/typed roles on their own primitive; the scoped
// binding restores a mixed legacy range's actual source pack after each draw.
// No vertex attribute or GPU readback is added.
class PolygonSurfaceGpu {
 struct Chunk {
  GLuint buffer=0,texture=0;size_t first=0,count=0;
  Chunk()=default;Chunk(const Chunk&)=delete;Chunk&operator=(const Chunk&)=delete;
  Chunk(Chunk&&o)noexcept:buffer(std::exchange(o.buffer,0)),texture(std::exchange(o.texture,0)),first(o.first),count(o.count){}
  ~Chunk(){if(texture)glDeleteTextures(1,&texture);if(buffer)glDeleteBuffers(1,&buffer);}
 };
 struct Entry {std::shared_ptr<const PolygonSurfaceOwner>owner;AffineTransform world;std::vector<Chunk>chunks;size_t bytes=0;};
 std::vector<Entry>entries_;
 static bool same(const Entry&e,const PolygonSurfacePreview&p,const AffineTransform&w){const auto&o=p.field_owner();return e.owner==o&&!e.owner.owner_before(o)&&!o.owner_before(e.owner)&&e.world.matrix()==w.matrix();}
 struct Bind {
  GLint active=0,texture=0,buffer=0;
  Bind(){glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE14);glGetIntegerv(GL_TEXTURE_BINDING_BUFFER,&texture);glGetIntegerv(GL_TEXTURE_BUFFER_BINDING,&buffer);}
  ~Bind(){glBindBuffer(GL_TEXTURE_BUFFER,GLuint(buffer));glBindTexture(GL_TEXTURE_BUFFER,GLuint(texture));glActiveTexture(GLenum(active));}
 };
public:
 size_t bytes()const{size_t n=0;for(const auto&e:entries_)n+=e.bytes;return n;}
 size_t images()const{size_t n=0;for(const auto&e:entries_)n+=e.chunks.size();return n;}
 void clear(bool release){if(!release)for(auto&e:entries_)for(auto&c:e.chunks)c.texture=c.buffer=0;entries_.clear();}
 void prepare(const MeshSnapshot&snapshot,size_t other_bytes,size_t other_images,const SurfaceTextureLimits&limits){
  struct Wanted {std::shared_ptr<const PolygonSurfacePreview>packet;AffineTransform world;};std::vector<Wanted>wanted;
  if(std::none_of(snapshot.ranges.begin(),snapshot.ranges.end(),[](const auto&r){return r.textures&&bool(r.textures->polygon_surface);})){clear(true);return;}
  GLint texels=0;glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE,&texels);if(texels<4)throw std::runtime_error("Polygon primitive frames require an actual texture-buffer capacity");const size_t chunk_triangles=size_t(texels)/4;
  size_t all_bytes=0,all_images=0;
  for(const auto&r:snapshot.ranges)if(r.textures&&r.textures->polygon_surface){
   const auto&p=r.textures->polygon_surface;const auto world=polygon_range_world(r);
   if(std::any_of(wanted.begin(),wanted.end(),[&](const auto&w){return w.packet->field_owner()==p->field_owner()&&!w.packet->field_owner().owner_before(p->field_owner())&&!p->field_owner().owner_before(w.packet->field_owner())&&w.world.matrix()==world.matrix();}))continue;
   if(wanted.size()>=256)throw std::length_error("Polygon primitive frame owner count exceeds its renderer bound");
   const size_t triangles=p->geometric_frames().size(),bytes=triangles*64,images=(triangles+chunk_triangles-1)/chunk_triangles;
   if(all_bytes>limits.max_bytes||other_bytes>limits.max_bytes-all_bytes||bytes>limits.max_bytes-all_bytes-other_bytes||all_images>limits.max_images||other_images>limits.max_images-all_images||images>limits.max_images-all_images-other_images)throw std::length_error("Polygon primitive frames exceed aggregate image/buffer quota before upload");
   all_bytes+=bytes;all_images+=images;wanted.push_back({p,world});
  }
  std::erase_if(entries_,[&](const Entry&e){return !std::any_of(wanted.begin(),wanted.end(),[&](const auto&w){return same(e,*w.packet,w.world);});});
  Bind restore;
  for(const auto&w:wanted){if(std::any_of(entries_.begin(),entries_.end(),[&](const Entry&e){return same(e,*w.packet,w.world);}))continue;
   const auto frames=w.packet->placed_frames(w.world);Entry entry;entry.owner=w.packet->field_owner();entry.world=w.world;entry.bytes=frames.size()*64;
   for(size_t first=0;first<frames.size();first+=chunk_triangles){Chunk c;c.first=first;c.count=std::min(chunk_triangles,frames.size()-first);glGenBuffers(1,&c.buffer);glGenTextures(1,&c.texture);if(!c.buffer||!c.texture)throw std::runtime_error("Polygon primitive frame allocation failed");glBindBuffer(GL_TEXTURE_BUFFER,c.buffer);glBufferData(GL_TEXTURE_BUFFER,GLsizeiptr(c.count*64),frames.data()+first,GL_STATIC_DRAW);glBindTexture(GL_TEXTURE_BUFFER,c.texture);glTexBuffer(GL_TEXTURE_BUFFER,GL_RGBA32F,c.buffer);if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Polygon primitive frame upload failed");entry.chunks.push_back(std::move(c));}
   entries_.push_back(std::move(entry));
  }
 }
 template<class Draw>void draw(const PolygonSurfacePreview&p,const AffineTransform&world,size_t first,size_t count,Draw&&draw){
  auto found=std::find_if(entries_.begin(),entries_.end(),[&](const Entry&e){return same(e,p,world);});if(found==entries_.end()||count!=p.geometric_frames().size()*3)throw std::invalid_argument("Polygon draw has no matching immutable primitive-frame upload");
  Bind restore;for(const auto&chunk:found->chunks){glBindTexture(GL_TEXTURE_BUFFER,chunk.texture);draw(first+chunk.first*3,chunk.count*3);}
 }
};
inline std::string polygon_surface_glsl(){return R"GLSL(
uniform int polygon_mode;
uniform mat4 model;uniform mat3 normal_model;uniform int affine_motion;uniform float model_hand;
mat3 polygon_basis(int first){vec4 a=texelFetch(packedSources,gl_PrimitiveID*4+first);vec3 t=texelFetch(packedSources,gl_PrimitiveID*4+first+1).xyz;
mat3 normal_transform=affine_motion==1?normal_model:mat3(model);vec3 N=unit(normal_transform*a.xyz);vec3 T=mat3(model)*t;T=unit(T-N*dot(N,T));return mat3(T,unit(cross(N,T))*a.w*model_hand,N);}
vec3 polygon_transport(vec3 before){if(polygon_mode==0)return before;mat3 a=polygon_basis(0),b=polygon_basis(2);return unit(b*transpose(a)*before);}
)GLSL";}
} // namespace dome::detail
