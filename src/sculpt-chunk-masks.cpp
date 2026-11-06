// SPDX-License-Identifier: GPL-3.0-or-later
#include "sculpt-chunk-masks.hpp"
#include "sculpt-chunk-index.hpp"
namespace dome::detail {
namespace {
struct Traits {
  static SculptChunkBounds bounds(const SculptChunkMasks::Values &v){return v->geometry->bounds;}
  static size_t bytes(const SculptChunkMasks::Values &v){return v->retained_bytes();}
};
using Index=PersistentSculptIndex<Int3,SculptChunkMasks::Values,Traits>;
struct Work {
  size_t limit,bytes=0,steps=0;
  LocalSurfaceCache::Cancel cancel;
  void stop(){if(cancel&&cancel())throw std::runtime_error("Sculpt scalar capture cancelled");}
  void tick(){if(++steps>16'000'000)throw std::length_error("Sculpt scalar metadata work limit exceeded");if((steps&63)==0)stop();}
  void charge(size_t n){if(bytes>limit||n>limit-bytes)throw std::length_error("Sculpt scalar working byte limit exceeded");bytes+=n;}
};
}
struct SculptChunkMasks::State {
  SculptSurfaceChunks::Ptr geometry;
  std::shared_ptr<const SculptMaskSnapshot> mask;
  Index chunks;
  size_t bytes=0;
};
SculptChunkMasks::SculptChunkMasks(std::shared_ptr<const State> s):state_(std::move(s)){}
size_t SculptChunkMasks::retained_bytes() const {return state_->bytes;}
SculptChunkMasks::Values SculptChunkMasks::find(const SculptSurfaceChunks::Chunk &source) const {
  auto found=state_->chunks.find(source->coordinate);
  if(!found||(*found)->geometry!=source||(*found)->geometry.owner_before(source)||source.owner_before((*found)->geometry))
    throw std::invalid_argument("Sculpt scalar lookup differs from actual immutable geometry owner");
  return *found;
}
SculptChunkMasks::Ptr SculptChunkMasks::capture(SculptSurfaceChunks::Ptr geometry,
    std::shared_ptr<const SculptMaskSnapshot> mask,const Ptr &before,size_t retained,size_t working,
    LocalSurfaceCache::Cancel cancel) {
  Work work{working,0,0,std::move(cancel)};work.stop();
  if(!geometry||!mask||!retained||!working)throw std::invalid_argument("Invalid sculpt scalar capture");
  const bool same=before&&before->state_->mask==mask&&!before->state_->mask.owner_before(mask)&&!mask.owner_before(before->state_->mask);
  if(same&&before->state_->geometry==geometry) {
    if(before->retained_bytes()>retained||before->retained_bytes()>working)throw std::length_error("Retained sculpt scalars exceed current budget");
    return before;
  }
  work.charge(sizeof(State)+sizeof(SculptChunkMasks)+256);
  if(before)work.charge(before->retained_bytes());
  auto out=std::make_shared<State>();out->geometry=std::move(geometry);out->mask=std::move(mask);
  if(same)out->chunks=before->state_->chunks;
  bool delta=false;
  if(same)delta=out->geometry->for_each_change(before->state_->geometry,[&](Int3 key,const auto &){out->chunks.erase(key,work);},work.cancel);
  if(!delta)out->chunks={};
  auto usage=[&](){
    const auto nodes=Index::n(out->chunks.root);
    if(nodes>65536||nodes>(SIZE_MAX-sizeof(State)-sizeof(SculptChunkMasks)-256)/(sizeof(Index::Node)+128))
      throw std::length_error("Sculpt scalar directory quota exceeded");
    return sculpt_index_sum(sizeof(State)+sizeof(SculptChunkMasks)+256+nodes*(sizeof(Index::Node)+128),out->chunks.root?out->chunks.root->payload_bytes:0);
  };
  auto sample=[&](const SculptSurfaceChunks::Chunk &chunk){
    work.tick();const auto count=chunk->mesh.positions.size();
    if(count>(SIZE_MAX-sizeof(SculptChunkMask)-128)/sizeof(float))throw std::length_error("Sculpt scalar byte overflow");
    const auto bytes=sizeof(SculptChunkMask)+128+count*sizeof(float);
    const auto used=usage();
    const auto prospective_nodes=3*(size_t(Index::h(out->chunks.root))+1)*(sizeof(Index::Node)+128);
    if(used>retained||bytes>retained-used||sizeof(Index::Node)+128>retained-used-bytes)
      throw std::length_error("Sculpt scalar retained byte limit exceeded");
    if(prospective_nodes>working-work.bytes||bytes>working-work.bytes-prospective_nodes)
      throw std::length_error("Sculpt scalar prospective insertion exceeds working limit");
    work.charge(bytes);auto values=std::make_shared<SculptChunkMask>();values->geometry=chunk;values->mask=out->mask;
    values->values.reserve(count);
    for(size_t v=0;v<count;++v){if((v&255)==0)work.stop();values->values.push_back(out->mask->sample(chunk->mesh.positions[v]));}
    if(values->retained_bytes()>bytes)throw std::length_error("Sculpt scalar actual capacity exceeded prospective limit");
    out->chunks.put(chunk->coordinate,std::move(values),work);
    if(usage()>retained)throw std::length_error("Sculpt scalar directory exceeds retained limit");
  };
  if(delta)out->geometry->for_each_change(before->state_->geometry,[&](Int3,const auto &chunk){if(chunk)sample(chunk);},work.cancel);
  else out->geometry->for_each_chunk(sample,work.cancel);
  out->bytes=usage();work.stop();return Ptr(new SculptChunkMasks(std::move(out)));
}
} // namespace dome::detail
