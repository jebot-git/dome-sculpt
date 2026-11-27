// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "polygon-surface-range.hpp"
#include "render-store-ledger.hpp"
#include <utility>
namespace dome::detail {
// Per-renderer immutable primitive frames. Unit14 already carries a sampler-
// buffer for MToon. Reuse that same sampler declaration as well as its unit:
// two active aliases would consume17 fragment samplers on a minimum16 device.
// Polygon packets reject source/typed roles on their own primitive; the scoped
// binding restores a mixed legacy range's actual source pack after each draw.
// No vertex attribute or GPU readback is added.
class PolygonSurfaceGpu {
 using Ledger=ContextStoreLedger;
 std::shared_ptr<Ledger>ledger_;Ledger::Group group_=0;bool own_ledger_=false;
 struct Chunk {
  GLuint buffer=0,texture=0;Ledger::Store buffer_store,texture_store;size_t first=0,count=0;
  Chunk()=default;Chunk(const Chunk&)=delete;Chunk&operator=(const Chunk&)=delete;
  Chunk(Chunk&&o)noexcept:buffer(std::exchange(o.buffer,0)),texture(std::exchange(o.texture,0)),buffer_store(std::move(o.buffer_store)),texture_store(std::move(o.texture_store)),first(o.first),count(o.count){}
  ~Chunk()=default;
 };
 struct Key {
  std::shared_ptr<const PolygonSurfaceOwner> owner;
  std::shared_ptr<const void> pose;
  std::shared_ptr<const TextureSetFaceDomain> domain;
  AffineTransform world;
  size_t triangles;
  bool operator==(const Key &other)const{
   const auto same=[](const auto &a,const auto &b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);};
   return same(owner,other.owner)&&same(pose,other.pose)&&same(domain,other.domain)&&
       world.matrix()==other.world.matrix()&&triangles==other.triangles;
  }
 };
 struct Entry {Key key;std::vector<Chunk>chunks;};
 std::vector<Entry>entries_;
 static Key key(const PolygonSurfacePreview &p,const AffineTransform &world,
     const std::shared_ptr<const TextureSetFaceDomain> &domain){
  return {p.field_owner(),p.posed_frame_owner(),domain,world,
          domain?domain->triangles().size():p.geometric_frames().size()};
 }
 struct Bind {
  GLint active=0,texture=0,buffer=0;
  Bind(){glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE14);glGetIntegerv(GL_TEXTURE_BINDING_BUFFER,&texture);glGetIntegerv(GL_TEXTURE_BUFFER_BINDING,&buffer);}
  ~Bind(){glBindBuffer(GL_TEXTURE_BUFFER,GLuint(buffer));glBindTexture(GL_TEXTURE_BUFFER,GLuint(texture));glActiveTexture(GLenum(active));}
 };
public:
 explicit PolygonSurfaceGpu(std::shared_ptr<Ledger>ledger={}):ledger_(ledger?ledger:std::make_shared<Ledger>(Ledger::Limits{2ull*1024*1024*1024,2048,256})),own_ledger_(!ledger){group_=ledger_->create_group();}
 Ledger::Group group()const noexcept{return group_;}
 using Selection=std::vector<Key>;
 static Selection selection(const MeshSnapshot&snapshot){
  Selection result;
  for(const auto &r:snapshot.ranges)if(r.textures&&r.textures->polygon_surface){
   auto k=key(*r.textures->polygon_surface,polygon_range_world(r),
              r.texture_set_proof?r.texture_set_proof->field().domain:nullptr);
   if(std::ranges::find(result,k)==result.end())result.push_back(std::move(k));
  }
  return result;
 }
 void retain(const Selection&wanted)noexcept {
  std::erase_if(entries_,[&](const Entry &entry){return std::ranges::find(wanted,entry.key)==wanted.end();});
 }
 size_t bytes()const{return ledger_->usage(group_).bytes;}
 size_t images()const{return ledger_->usage(group_).images;}
 void clear(bool release){entries_.clear();if(own_ledger_)ledger_->clear(release);}
 struct Demand{size_t bytes=0,images=0;};
 Demand additional(const MeshSnapshot&snapshot)const {
  Demand result;
  const auto keys=selection(snapshot);
  if(keys.empty())return result;
  GLint texels=0;glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE,&texels);
  if(texels<4)throw std::runtime_error("Polygon primitive frame capacity unavailable");
  const auto chunk=size_t(texels)/4;
  for(const auto &k:keys){
   if(std::ranges::find(entries_,k,&Entry::key)!=entries_.end())continue;
   const auto n=k.triangles;
   if(n>SIZE_MAX/64||n*64>SIZE_MAX-result.bytes)throw std::length_error("Polygon frame store forecast overflow");
   result.bytes+=n*64;result.images+=(n+chunk-1)/chunk;
  }
  return result;
 }
 void prepare(const MeshSnapshot&snapshot,size_t other_bytes,size_t other_images,const SurfaceTextureLimits&limits,bool preserve_existing=false){
  if(own_ledger_)ledger_->collect();const auto delta=additional(snapshot);const auto used=ledger_->usage(group_);if(other_bytes>limits.max_bytes||used.bytes>limits.max_bytes-other_bytes||delta.bytes>limits.max_bytes-other_bytes-used.bytes||other_images>limits.max_images||used.images>limits.max_images-other_images||delta.images>limits.max_images-other_images-used.images)throw std::length_error("Polygon old/new/debt quota exceeded");ledger_->preflight(Ledger::Domain::Material,delta.bytes,delta.images);
  struct Wanted {Key key;std::shared_ptr<const PolygonSurfacePreview>packet;};std::vector<Wanted>wanted;
  if(std::none_of(snapshot.ranges.begin(),snapshot.ranges.end(),[](const auto&r){return r.textures&&bool(r.textures->polygon_surface);})){if(!preserve_existing){entries_.clear();if(own_ledger_)ledger_->seal_noexcept();}return;}
  GLint texels=0;glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE,&texels);if(texels<4)throw std::runtime_error("Polygon primitive frames require an actual texture-buffer capacity");const size_t chunk_triangles=size_t(texels)/4;
  size_t all_bytes=0,all_images=0;
  for(const auto&r:snapshot.ranges)if(r.textures&&r.textures->polygon_surface){
   const auto&p=r.textures->polygon_surface;const auto world=polygon_range_world(r);const auto domain=r.texture_set_proof?r.texture_set_proof->field().domain:nullptr;
   if(r.texture_set_proof)r.texture_set_proof->authenticate(r);if(domain)domain->authenticate_owner(domain);
   auto k=key(*p,world,domain);
   if(std::ranges::find(wanted,k,&Wanted::key)!=wanted.end())continue;
   if(wanted.size()>=256)throw std::length_error("Polygon primitive frame owner count exceeds its renderer bound");
   const size_t triangles=k.triangles,bytes=triangles*64,images=(triangles+chunk_triangles-1)/chunk_triangles;
   if(all_bytes>limits.max_bytes||other_bytes>limits.max_bytes-all_bytes||bytes>limits.max_bytes-all_bytes-other_bytes||all_images>limits.max_images||other_images>limits.max_images-all_images||images>limits.max_images-all_images-other_images)throw std::length_error("Polygon primitive frames exceed aggregate image/buffer quota before upload");
   all_bytes+=bytes;all_images+=images;wanted.push_back({std::move(k),p});
  }
  if(!preserve_existing)std::erase_if(entries_,[&](const Entry&e){return std::ranges::find(wanted,e.key,&Wanted::key)==wanted.end();});
  Bind restore;
  for(const auto&w:wanted){if(std::ranges::find(entries_,w.key,&Entry::key)!=entries_.end())continue;
   // Reserve the full placement scratch plus the retained subset before allocation.
   const size_t full=w.packet->geometric_frames().size();if(full>limits.max_bytes/64||all_bytes>limits.max_bytes-full*64||other_bytes>limits.max_bytes-full*64-all_bytes)throw std::length_error("Polygon set frame placement scratch exceeds aggregate quota");
   auto frames=w.packet->placed_frames(w.key.world);
   if(w.key.domain){std::vector<std::array<std::array<float,4>,4>>subset;subset.reserve(w.key.triangles);for(auto triangle:w.key.domain->triangles())subset.push_back(frames.at(triangle));frames=std::move(subset);}
   Entry entry{w.key,{}};
   for(size_t first=0;first<frames.size();first+=chunk_triangles){Chunk c;c.first=first;c.count=std::min(chunk_triangles,frames.size()-first);c.buffer_store=ledger_->create(Ledger::Kind::Buffer,c.count*64,Ledger::Domain::Material,group_);c.buffer=c.buffer_store.name();c.texture_store=ledger_->create(Ledger::Kind::Texture,0,Ledger::Domain::Material,group_,std::span(&c.buffer_store,1));c.texture=c.texture_store.name();glBindBuffer(GL_TEXTURE_BUFFER,c.buffer);glBufferData(GL_TEXTURE_BUFFER,GLsizeiptr(c.count*64),frames.data()+first,GL_STATIC_DRAW);glBindTexture(GL_TEXTURE_BUFFER,c.texture);glTexBuffer(GL_TEXTURE_BUFFER,GL_RGBA32F,c.buffer);if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Polygon primitive frame upload failed");entry.chunks.push_back(std::move(c));}
   entries_.push_back(std::move(entry));
  }
  if(own_ledger_&&delta.bytes)ledger_->seal(true);
 }
 template<class Draw>void draw(const PolygonSurfacePreview&p,const AffineTransform&world,size_t first,size_t count,const std::shared_ptr<const TextureSetFaceDomain>&domain,Draw&&draw){
  const auto k=key(p,world,domain);
  auto found=std::ranges::find(entries_,k,&Entry::key);if(found==entries_.end()||count!=k.triangles*3)throw std::invalid_argument("Polygon draw has no matching immutable primitive-frame upload");
  ledger_->mark_used();Bind restore;for(const auto&chunk:found->chunks){glBindTexture(GL_TEXTURE_BUFFER,chunk.texture);draw(first+chunk.first*3,chunk.count*3);}
 }
};
inline std::string polygon_surface_glsl(){return R"GLSL(
uniform int polygon_mode;
uniform mat4 model;uniform mat3 normal_model;uniform int affine_motion;uniform float model_hand;
mat3 polygon_basis(int first){vec4 a=texelFetch(packedSources,gl_PrimitiveID*4+first);vec3 t=texelFetch(packedSources,gl_PrimitiveID*4+first+1).xyz;
mat3 normal_transform=affine_motion==1?normal_model:mat3(model);vec3 N=unit(normal_transform*a.xyz);vec3 T=mat3(model)*t;T=unit(T-N*dot(N,T));return mat3(T,unit(cross(N,T))*a.w*model_hand,N);}
vec3 polygon_transport(vec3 before){
 if(polygon_mode==0)return before;
 mat3 a=polygon_basis(0),b=polygon_basis(2);
 if(polygon_mode==3){
  vec4 stored=texture(tex3,paintcoord);
  vec3 detail=2.0*stored.xyz-1.0;
  if(stored.a>0.0&&dot(detail,detail)>1e-16){
   vec3 N=unit(before),T=unit(a[0]-N*dot(N,a[0]));
   float hand=dot(cross(a[2],a[0]),a[1])<0.0?-1.0:1.0;
   before=unit(T*detail.x+cross(N,T)*hand*detail.y+N*detail.z);
  }
 }
 return unit(b*transpose(a)*before);
}
)GLSL";}
} // namespace dome::detail
