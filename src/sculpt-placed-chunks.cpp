// SPDX-License-Identifier: GPL-3.0-or-later
#include "sculpt-placed-chunks.hpp"
#include "sculpt-chunk-index.hpp"
namespace dome::detail {
namespace {
struct Traits {
  static SculptChunkBounds bounds(const SculptPlacedChunk::Ptr &p){return p->bounds();}
  static size_t bytes(const SculptPlacedChunk::Ptr &p){return p->retained_bytes();}
};
using Index=PersistentSculptIndex<Int3,SculptPlacedChunk::Ptr,Traits>;
struct Work {
  const SculptPlacedChunksLimits &limits;
  LocalSurfaceCache::Cancel cancel;
  size_t bytes=0,steps=0;
  void stop() const {if(cancel&&cancel())throw std::runtime_error("Placed sculpt directory cancelled");}
  void tick(){if(steps>=limits.metadata_steps)throw std::length_error("Placed sculpt directory metadata work limit exceeded");++steps;if((steps&63)==0)stop();}
  void charge(size_t n,size_t width=1){
    if(bytes>limits.working_bytes||n>(limits.working_bytes-bytes)/width)
      throw std::length_error("Placed sculpt directory working byte limit exceeded");
    bytes+=n*width;
  }
};
}
struct SculptPlacedChunks::State {
  SculptSurfaceChunks::Ptr geometry;
  Index chunks;
  SculptPlacedChunksReport report;
  SculptPlacedChunksLimits limits;
  uint64_t object=0,generation=0;
  bool queries=false;
};
SculptPlacedChunks::SculptPlacedChunks(std::shared_ptr<const State> s):state_(std::move(s)){}
const SculptSurfaceChunks::Ptr &SculptPlacedChunks::geometry() const {return state_->geometry;}
const SculptPlacedChunksReport &SculptPlacedChunks::report() const {return state_->report;}
const SculptChunkBounds &SculptPlacedChunks::bounds() const {
  static const SculptChunkBounds empty;
  return state_->chunks.root?state_->chunks.root->bounds:empty;
}
bool SculptPlacedChunks::has_queries() const {return state_->queries;}
SculptPlacedChunks::Ptr SculptPlacedChunks::capture(const SculptSceneChunks::Ptr &scene,uint64_t object,
    const Ptr &before,bool queries,SculptPlacedChunksLimits limits,LocalSurfaceCache::Cancel cancel) {
  Work work{limits,cancel};work.stop();
  if(!limits.retained_bytes||!limits.working_bytes||!limits.nodes||!limits.metadata_steps)
    throw std::invalid_argument("Invalid placed sculpt directory limits");
  const auto *view=scene?scene->find(object):nullptr;
  if(!view)throw std::invalid_argument("Placed directory requires its actual current object");
  const bool context=before&&before->state_->object==object&&
      before->state_->generation==scene->context().generation;
  const auto prototype=context&&before->state_->chunks.root?before->state_->chunks.root->value:SculptPlacedChunk::Ptr{};
  const bool placement=prototype&&prototype->placement_matches(*view);
  if(context&&placement&&before->state_->geometry==view->geometry&&(!queries||before->state_->queries)) {
    if(before->report().retained_bytes>limits.retained_bytes||before->report().retained_bytes>limits.working_bytes)
      throw std::length_error("Current placed directory exceeds requested limits");
    return before;
  }
  if(before)work.charge(before->report().retained_bytes);
  work.charge(view->geometry->report().retained_bytes);work.charge(sizeof(State)+sizeof(SculptPlacedChunks)+256);
  auto out=std::make_shared<State>();out->geometry=view->geometry;out->object=object;
  out->generation=scene->context().generation;out->queries=queries;out->limits=limits;
  // Enabling picking needs a one-time acceleration pass even if geometry is
  // unchanged. A zero geometry delta cannot manufacture complete BVHs.
  const bool attempt_delta=context&&placement&&(!queries||before->state_->queries);
  if(attempt_delta)out->chunks=before->state_->chunks;
  bool delta=false;
  if(attempt_delta)delta=view->geometry->for_each_change(before->state_->geometry,[&](Int3 key,const auto &source){
    work.tick();if(out->chunks.find(key)){out->chunks.erase(key,work);if(!source)++out->report.removed;}
  },cancel);
  if(!delta){out->chunks={};out->report.removed=0;out->report.complete_reconcile=true;}
  auto retained=[&](){
    auto bytes=sculpt_index_sum(sizeof(State)+sizeof(SculptPlacedChunks)+256,out->geometry->report().retained_bytes);
    const auto count=Index::n(out->chunks.root);
    if(count>SIZE_MAX/(sizeof(Index::Node)+128))throw std::length_error("Placed directory retained byte overflow");
    bytes=sculpt_index_sum(bytes,count*(sizeof(Index::Node)+128));
    return sculpt_index_sum(bytes,out->chunks.root?out->chunks.root->payload_bytes:0);
  };
  auto compile=[&](const SculptSurfaceChunks::Chunk &source){
    work.tick();
    if(Index::n(out->chunks.root)>=limits.nodes)throw std::length_error("Placed directory chunk limit exceeded");
    const auto current=retained();
    if(current>limits.retained_bytes||sizeof(Index::Node)+128>limits.retained_bytes-current)
      throw std::length_error("Placed directory retained byte limit exceeded");
    auto chunk_limits=limits.chunk;
    chunk_limits.retained_bytes=std::min(chunk_limits.retained_bytes,limits.retained_bytes-current-sizeof(Index::Node)-128);
    const size_t node_copies=3*(size_t(Index::h(out->chunks.root))+1);
    if(node_copies>(limits.working_bytes-work.bytes)/(sizeof(Index::Node)+128))
      throw std::length_error("Placed directory insertion envelope exceeds working budget");
    const size_t index_work=node_copies*(sizeof(Index::Node)+128);
    chunk_limits.working_bytes=std::min(chunk_limits.working_bytes,limits.working_bytes-work.bytes-index_work);
    if(!chunk_limits.working_bytes||!chunk_limits.retained_bytes)
      throw std::length_error("No placed chunk capacity remains in the directory budget");
    SculptPlacedChunk::Ptr previous;
    if(context)if(const auto found=before->state_->chunks.find(source->coordinate))previous=*found;
    // The per-chunk operation includes any live previous owner. Its bounded
    // complete envelope must fit before the new geometry/BVH factory starts.
    const auto placed=SculptPlacedChunk::capture(scene,object,source,previous,queries,chunk_limits,cancel);
    if(placed==previous)++out->report.reused;else ++out->report.built;
    work.charge(placed->retained_bytes());
    out->chunks.put(source->coordinate,placed,work);
    if(retained()>limits.retained_bytes)throw std::length_error("Placed directory retained byte limit exceeded");
  };
  if(delta) {
    const bool again=view->geometry->for_each_change(before->state_->geometry,[&](Int3,const auto &source){if(source)compile(source);},cancel);
    if(!again)throw std::logic_error("An immutable placed delta changed its actual base");
  }else view->geometry->for_each_chunk(compile,cancel);
  out->report.retained_bytes=retained();out->report.working_bytes=work.bytes;work.stop();
  return Ptr(new SculptPlacedChunks(std::move(out)));
}
void SculptPlacedChunks::for_each(const std::function<void(const SculptPlacedChunk::Ptr &)> &fn,
                                  LocalSurfaceCache::Cancel cancel) const {
  Work work{state_->limits,std::move(cancel)};work.stop();
  state_->chunks.visit([&](Int3,const auto &chunk){fn(chunk);},work);work.stop();
}
void SculptPlacedChunks::visit_bounds(const std::function<bool(const SculptChunkBounds &)> &keep,
    const std::function<void(const SculptPlacedChunk::Ptr &)> &fn,
    LocalSurfaceCache::Cancel cancel) const {
  Work work{state_->limits,std::move(cancel)};work.stop();
  const auto visit=[&](auto &self,const Index::Ptr &node)->void {
    if(!node)return;work.tick();if(!keep(node->bounds))return;
    self(self,node->left);if(keep(node->value->bounds()))fn(node->value);self(self,node->right);
  };
  visit(visit,state_->chunks.root);work.stop();
}
} // namespace dome::detail
