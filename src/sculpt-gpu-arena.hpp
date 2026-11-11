// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "sculpt-gpu-arena-layout.hpp"
#include "sculpt-placed-chunks.hpp"
#include "sculpt-chunk-masks.hpp"
#include "render-buffer-size.hpp"
#include <array>
#include <unordered_map>
namespace dome::render_detail {
// Context-local plain sculpt geometry only. No Paint/Material/normal-field or
// packed-primitive authority is manufactured by GPU placement.
class SculptGpuArena {
  struct Ledger {size_t bytes=0,peak=0,limit=gpu_limit;bool context=true;};
  struct Buffer {
    std::shared_ptr<Ledger> ledger;GLuint id=0;size_t bytes=0;
    Buffer(std::shared_ptr<Ledger> l,size_t b):ledger(std::move(l)),bytes(b) {
      sculpt_arena_peak(ledger->bytes,b,ledger->limit);ledger->bytes+=b;
      ledger->peak=std::max(ledger->peak,ledger->bytes);
    }
    ~Buffer(){if(ledger->context)glDeleteBuffers(1,&id);ledger->bytes-=bytes;}
  };
  using BufferPtr=std::shared_ptr<Buffer>;
  struct Key {uint64_t object;const Mesh *mesh;bool operator==(const Key &) const=default;};
  struct Hash {size_t operator()(const Key &key)const{return std::hash<const Mesh*>{}(key.mesh)^(std::hash<uint64_t>{}(key.object)+0x9e3779b9u);}};
  struct Entry {
    std::shared_ptr<const Mesh> geometry;
    detail::SculptChunkMasks::Values mask;
    SculptArenaSlots::Span vertices,indices;
  };
  struct Commands {
    detail::SculptSurfaceChunks::Ptr geometry;
    std::vector<GLsizei> counts;
    std::vector<const void*> offsets;
    std::vector<GLint> bases;
  };
public:
  // Explicit native validation bounds, independent of driver opaque overhead.
  static constexpr size_t gpu_limit=512*1024*1024,metadata_limit=128*1024*1024;
  using Cancel=LocalSurfaceCache::Cancel;
  struct Snapshot {
    detail::SculptSceneChunks::Ptr source;
    BufferPtr positions,normals,indices,masks;
    SculptArenaSlots vertex_slots,index_slots;
    std::unordered_map<Key,Entry,Hash> entries;
    std::unordered_map<uint64_t,std::shared_ptr<const Commands>> commands;
    std::shared_ptr<Ledger> ledger;
    GLuint vao=0;
    size_t metadata_bytes=0,geometry_transfers=0,scalar_transfers=0,copy_bytes=0;
    ~Snapshot(){if(ledger&&ledger->context)glDeleteVertexArrays(1,&vao);}
  };
  using Pending=std::shared_ptr<Snapshot>;
private:
  std::shared_ptr<Ledger> ledger_=std::make_shared<Ledger>();
  Pending accepted_;
  size_t cpu_limit_=metadata_limit;
  static void check(const char *what) {
    auto error=glGetError();if(error==GL_NO_ERROR)return;
    while(glGetError()!=GL_NO_ERROR){}
    throw std::runtime_error(std::string(what)+" (GL error "+std::to_string(error)+")");
  }
  static void poll(const Cancel &cancel){if(cancel&&cancel())throw std::runtime_error("Sculpt GPU arena preparation cancelled");}
  template<class T>static bool same_owner(const std::shared_ptr<T> &a,const std::shared_ptr<T> &b) {
    return a==b&&!a.owner_before(b)&&!b.owner_before(a);
  }
  BufferPtr buffer(size_t bytes,GLenum usage) {
    if(!bytes)return {};
    auto result=std::make_shared<Buffer>(ledger_,bytes);
    glGenBuffers(1,&result->id);check("Sculpt arena buffer name allocation failed");
    if(!result->id)throw std::runtime_error("Sculpt arena buffer name missing");
    glBindBuffer(GL_ARRAY_BUFFER,result->id);
    glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(bytes),nullptr,usage);
    check("Sculpt arena buffer allocation failed");return result;
  }
  static void copy(const BufferPtr &from,const BufferPtr &to,size_t start,size_t target,size_t bytes,Snapshot &next) {
    if(!bytes)return;
    if(!from||!to||start>from->bytes||bytes>from->bytes-start||target>to->bytes||bytes>to->bytes-target)
      throw std::logic_error("Sculpt arena copy outside actual buffer stores");
    glBindBuffer(GL_COPY_READ_BUFFER,from->id);glBindBuffer(GL_COPY_WRITE_BUFFER,to->id);
    glCopyBufferSubData(GL_COPY_READ_BUFFER,GL_COPY_WRITE_BUFFER,GLintptr(start),GLintptr(target),GLsizeiptr(bytes));
    check("Sculpt arena retained GPU copy failed");next.copy_bytes=sculpt_arena_add(next.copy_bytes,bytes);
  }
  static void upload(const BufferPtr &buffer,size_t offset,size_t bytes,const void *data) {
    if(!bytes)return;
    if(!buffer||offset>buffer->bytes||bytes>buffer->bytes-offset)throw std::logic_error("Sculpt arena upload outside buffer store");
    glBindBuffer(GL_ARRAY_BUFFER,buffer->id);
    glBufferSubData(GL_ARRAY_BUFFER,GLintptr(offset),GLsizeiptr(bytes),data);
    check("Sculpt arena immutable array upload failed");
  }
  static void scalar(const Entry &entry,Snapshot &next,const Cancel &cancel) {
    const size_t offset=buffer_bytes(entry.vertices.first,sizeof(float));
    const size_t size=buffer_bytes(entry.vertices.count,sizeof(float));
    if(entry.mask)upload(next.masks,offset,size,entry.mask->values.data());
    else {
      // Attribute6 is ALWAYS enabled in arena VAOs. Disabled-array current
      // values become unspecified after an enabled draw. Use real -1 scalars,
      // uploaded from this bounded constant block, never a mesh-sized CPU fill.
      static const auto defaults=[] {std::array<float,1024>a{};a.fill(-1);return a;}();
      for(size_t done=0;done<entry.vertices.count;){poll(cancel);const auto n=std::min(defaults.size(),entry.vertices.count-done);
        upload(next.masks,offset+done*sizeof(float),n*sizeof(float),defaults.data());done+=n;}
    }
    ++next.scalar_transfers;
  }
  static size_t cpu_forecast(size_t entries,size_t objects,size_t runs) {
    // Conservatively covers map nodes/buckets +both span free lists. Command
    // vectors reserve their exact counts; no geometry/vertex CPU copy exists.
    return sculpt_arena_add(sculpt_arena_add(sculpt_arena_multiply(entries,512),sculpt_arena_multiply(objects,256)),
      sculpt_arena_multiply(runs,sizeof(GLsizei)+sizeof(void*)+sizeof(GLint)));
  }
public:
  explicit SculptGpuArena(size_t gpu_bytes=gpu_limit,size_t cpu_bytes=metadata_limit):cpu_limit_(cpu_bytes) {
    if(gpu_bytes>gpu_limit||cpu_bytes>metadata_limit)throw std::invalid_argument("Sculpt arena limits exceed hard maximum");
    ledger_->limit=gpu_bytes;
  }
  uint64_t uploads=0,scalar_uploads=0,gpu_copy_bytes=0;
  size_t live_bytes()const{return ledger_->bytes;}
  size_t peak_bytes()const{return ledger_->peak;}
  size_t metadata_bytes()const{return accepted_?accepted_->metadata_bytes:0;}
  size_t vertex_bytes()const{return accepted_&&accepted_->positions?accepted_->positions->bytes+accepted_->normals->bytes:0;}
  size_t index_bytes()const{return accepted_&&accepted_->indices?accepted_->indices->bytes:0;}
  size_t mask_bytes()const{return accepted_&&accepted_->masks?accepted_->masks->bytes:0;}
  void clear(bool context) {
    if(!context)ledger_->context=false;
    accepted_.reset();
  }
  // No accepted range, scalar store, VAO, command owner or cache entry changes
  // before commit(). Partial uploads may only touch currently FREE holes.
  Pending stage(const detail::SculptSceneChunks::Ptr &scene,Cancel cancel={}) {
    poll(cancel);
    if(accepted_&&same_owner(accepted_->source,scene))return accepted_;
    size_t count=0,objects=0,runs=0,vertices=0,indices=0;
    if(scene)for(const auto &object:scene->objects()){
      ++objects;object.placed->for_each([&](const auto &chunk){poll(cancel);
        if(++count>65536)throw std::length_error("Sculpt GPU arena packet count limit exceeded");
        const auto &mesh=*chunk->geometry_owner();
        if(mesh.positions.empty()||mesh.normals.size()!=mesh.positions.size()||mesh.indices.size()>size_t(std::numeric_limits<GLsizei>::max()))
          throw std::length_error("Invalid sculpt GPU arena geometry attributes/count");
        vertices=sculpt_arena_add(vertices,mesh.positions.size());indices=sculpt_arena_add(indices,mesh.indices.size());
      });
      const auto prior=accepted_?accepted_->commands.find(object.object):decltype(accepted_->commands)::const_iterator{};
      if(accepted_&&prior!=accepted_->commands.end()&&same_owner(prior->second->geometry,object.geometry))runs=sculpt_arena_add(runs,prior->second->counts.size());
      else object.geometry->for_each_run([&](const auto &){poll(cancel);if(++runs>size_t(std::numeric_limits<GLsizei>::max()))throw std::length_error("Sculpt arena draw count overflow");});
    }
    const auto cpu=cpu_forecast(count,objects,runs);
    sculpt_arena_peak(accepted_?accepted_->metadata_bytes:0,cpu,cpu_limit_);
    auto next=std::make_shared<Snapshot>();next->ledger=ledger_;next->source=scene;next->metadata_bytes=cpu;
    if(!count)return next;
    const bool generation=accepted_&&accepted_->source&&scene->context().generation==accepted_->source->context().generation;
    if(generation){next->vertex_slots=accepted_->vertex_slots;next->index_slots=accepted_->index_slots;}
    bool rebuild=!generation;
    bool mask_change=false;
    for(const auto &object:scene->objects())object.placed->for_each([&](const auto &chunk){poll(cancel);
      const auto &owner=chunk->geometry_owner();Key key{object.object,owner.get()};Entry entry;entry.geometry=owner;
      entry.mask=object.masks?object.masks->find(chunk->source()):nullptr;
      if(entry.mask&&entry.mask->values.size()!=owner->positions.size())throw std::invalid_argument("Sculpt arena mask differs from actual geometry");
      auto old=generation?accepted_->entries.find(key):next->entries.end();
      if(generation&&old!=accepted_->entries.end()){
        if(!same_owner(old->second.geometry,owner))throw std::logic_error("Sculpt arena geometry shared-owner mismatch");
        entry.vertices=old->second.vertices;entry.indices=old->second.indices;
        mask_change=mask_change||!same_owner(entry.mask,old->second.mask);
      }else if(!rebuild){
        const auto v=next->vertex_slots.reserve(owner->positions.size()),i=next->index_slots.reserve(owner->indices.size());
        if(!v||!i)rebuild=true;else{entry.vertices=*v;entry.indices=*i;}
      }
      if(!next->entries.emplace(key,std::move(entry)).second)throw std::logic_error("Duplicate sculpt GPU geometry authority");
    });
    if(rebuild){
      const size_t vc=sculpt_arena_capacity(vertices),ic=sculpt_arena_capacity(indices);
      if(vc>size_t(std::numeric_limits<GLint>::max()))throw std::length_error("Sculpt arena basevertex exceeds signed index range");
      const auto storage=sculpt_arena_storage(vc,ic);sculpt_arena_peak(ledger_->bytes,storage,ledger_->limit);
      next->vertex_slots=SculptArenaSlots(vc);next->index_slots=SculptArenaSlots(ic);
      for(auto &[_,entry]:next->entries){entry.vertices=*next->vertex_slots.reserve(entry.geometry->positions.size());entry.indices=*next->index_slots.reserve(entry.geometry->indices.size());}
      // Reserve/forecast all old+new requested GPU stores before the first GL
      // allocation. Fragmentation and unused25% growth reserve remain charged.
      next->positions=buffer(buffer_bytes(vc,sizeof(Vec3)),GL_STATIC_DRAW);
      next->normals=buffer(buffer_bytes(vc,sizeof(Vec3)),GL_STATIC_DRAW);
      next->indices=buffer(buffer_bytes(ic,sizeof(uint32_t)),GL_STATIC_DRAW);
      next->masks=buffer(buffer_bytes(vc,sizeof(float)),GL_DYNAMIC_DRAW);
    }else {
      next->positions=accepted_->positions;next->normals=accepted_->normals;next->indices=accepted_->indices;
      if(mask_change){
        sculpt_arena_peak(ledger_->bytes,accepted_->masks->bytes,ledger_->limit);
        next->masks=buffer(accepted_->masks->bytes,GL_DYNAMIC_DRAW);
        copy(accepted_->masks,next->masks,0,0,accepted_->masks->bytes,*next);
      }else next->masks=accepted_->masks;
    }
    for(auto &[key,entry]:next->entries){poll(cancel);
      auto old=generation?accepted_->entries.find(key):next->entries.end();
      const bool retained=generation&&old!=accepted_->entries.end();
      const auto pbytes=buffer_bytes(entry.vertices.count,sizeof(Vec3)),ibytes=buffer_bytes(entry.indices.count,sizeof(uint32_t));
      if(rebuild&&retained){
        copy(accepted_->positions,next->positions,old->second.vertices.first*sizeof(Vec3),entry.vertices.first*sizeof(Vec3),pbytes,*next);
        copy(accepted_->normals,next->normals,old->second.vertices.first*sizeof(Vec3),entry.vertices.first*sizeof(Vec3),pbytes,*next);
        copy(accepted_->indices,next->indices,old->second.indices.first*sizeof(uint32_t),entry.indices.first*sizeof(uint32_t),ibytes,*next);
      }else if(!retained){
        for(auto index:entry.geometry->indices)if(index>=entry.vertices.count)throw std::invalid_argument("Sculpt arena index escapes immutable geometry");
        upload(next->positions,entry.vertices.first*sizeof(Vec3),pbytes,entry.geometry->positions.data());
        upload(next->normals,entry.vertices.first*sizeof(Vec3),pbytes,entry.geometry->normals.data());
        upload(next->indices,entry.indices.first*sizeof(uint32_t),ibytes,entry.geometry->indices.data());++next->geometry_transfers;
      }
      if(!retained||!same_owner(entry.mask,old->second.mask))scalar(entry,*next,cancel);
      else if(rebuild)copy(accepted_->masks,next->masks,old->second.vertices.first*sizeof(float),entry.vertices.first*sizeof(float),entry.vertices.count*sizeof(float),*next);
    }
    // Removed owners are released in the NEXT allocator only, after every
    // possibly failing upload. Old snapshots retain occupied accepted ranges.
    if(!rebuild)for(const auto &[key,entry]:accepted_->entries)if(!next->entries.contains(key)){
      next->vertex_slots.release(entry.vertices);next->index_slots.release(entry.indices);
    }
    for(const auto &object:scene->objects()){
      poll(cancel);
      const auto previous=generation?accepted_->commands.find(object.object):next->commands.end();
      if(!rebuild&&generation&&previous!=accepted_->commands.end()&&same_owner(previous->second->geometry,object.geometry)){
        next->commands.emplace(object.object,previous->second);continue;
      }
      auto commands=std::make_shared<Commands>();commands->geometry=object.geometry;
      size_t n=0;object.geometry->for_each_run([&](const auto &){poll(cancel);++n;});
      commands->counts.reserve(n);commands->offsets.reserve(n);commands->bases.reserve(n);
      object.geometry->for_each_run([&](const detail::SculptSurfaceRun &run){poll(cancel);
        const auto placed=object.placed->find(run.chunk);if(!placed)throw std::logic_error("Sculpt arena run lost placed owner");
        const auto at=next->entries.find({object.object,placed->geometry_owner().get()});
        if(at==next->entries.end()||!same_owner(at->second.geometry,placed->geometry_owner()))throw std::logic_error("Sculpt arena run lost actual geometry owner");
        const auto &entry=at->second;
        if(run.first>entry.indices.count||run.count>entry.indices.count-run.first||run.count>size_t(std::numeric_limits<GLsizei>::max()))throw std::length_error("Sculpt arena run outside actual index owner");
        commands->counts.push_back(GLsizei(run.count));commands->offsets.push_back(reinterpret_cast<const void*>((entry.indices.first+run.first)*sizeof(uint32_t)));commands->bases.push_back(GLint(entry.vertices.first));
      });
      next->commands.emplace(object.object,std::move(commands));
    }
    poll(cancel);glGenVertexArrays(1,&next->vao);check("Sculpt arena VAO allocation failed");if(!next->vao)throw std::runtime_error("Missing sculpt arena VAO");
    glBindVertexArray(next->vao);
    auto attribute=[&](GLuint slot,const BufferPtr &buffer,GLint components,GLsizei stride){glBindBuffer(GL_ARRAY_BUFFER,buffer->id);glVertexAttribPointer(slot,components,GL_FLOAT,GL_FALSE,stride,nullptr);glEnableVertexAttribArray(slot);};
    attribute(0,next->positions,3,sizeof(Vec3));attribute(1,next->normals,3,sizeof(Vec3));attribute(6,next->masks,1,sizeof(float));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,next->indices->id);check("Sculpt arena VAO staging failed");
    return next;
  }
  void commit(Pending next) noexcept {
    if(next==accepted_)return;
    uploads+=next->geometry_transfers;scalar_uploads+=next->scalar_transfers;gpu_copy_bytes+=next->copy_bytes;
    accepted_.swap(next);
  }
  bool has_commands(uint64_t object,const Pending &pending={})const {
    const auto &state=pending?pending:accepted_;
    if(!state)return false;auto i=state->commands.find(object);return i!=state->commands.end()&&!i->second->counts.empty();
  }
  void draw(uint64_t object,const Pending &pending={})const {
    if(!has_commands(object,pending))return;
    const auto &state=pending?pending:accepted_;
    const auto &commands=*state->commands.at(object);glBindVertexArray(state->vao);
    glVertexAttrib2f(2,0,0);glVertexAttrib2f(3,0,0);glVertexAttrib1f(4,-1);glVertexAttrib4f(5,1,0,0,1);
    glVertexAttrib3f(7,0,0,1);glVertexAttrib4f(8,1,0,0,1);glVertexAttrib3f(9,0,0,1);
    glVertexAttrib4f(10,1,1,1,1);glVertexAttrib4f(11,0,0,0,0);glVertexAttrib2f(12,0,0);
    for(GLuint slot=13;slot<16;++slot)glVertexAttrib4f(slot,0,0,0,0);
    glMultiDrawElementsBaseVertex(GL_TRIANGLES,commands.counts.data(),GL_UNSIGNED_INT,commands.offsets.data(),GLsizei(commands.counts.size()),commands.bases.data());
  }
};
} // namespace dome::render_detail
