// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_material_reference_main
#include "mtoon-render.cpp"
#undef main
#define main existing_native_chunk_main
#include "sculpt-chunk-pipeline.cpp"
#undef main
#include "sculpt-gpu-arena.hpp"
#include <cstdlib>
#include <new>
namespace {
thread_local int64_t deny=-1;
thread_local bool measure=false;
thread_local size_t allocations=0;
size_t gpu_allocations=0,gpu_submits=0,gpu_copies=0;
int64_t deny_allocate=-1,deny_upload=-1,deny_copy=-1;
bool hit=false,hold_retirement=false;
int64_t deny_fence=-1,deny_wait=-1;
size_t retirement_queries=0;
void drain(dome::render_detail::SculptGpuArena &arena){
  // Test-only completion boundary. Production collection is timeout0 and never
  // glFinish-es an unchanged, held, canvas or ordinary dirty render view.
  for(unsigned n=0;n<8;++n){glFinish();if(arena.collect_retired())return;}
  throw std::runtime_error("Actual GPU retirement did not drain after completion");
}
void count_allocation(){if(measure)++allocations;if(deny==0){hit=true;throw std::bad_alloc();}if(deny>0)--deny;}
bool reject_at(int64_t &n){if(n==0){hit=true;n=-1;return true;}if(n>0)--n;return false;}
}
void *operator new(size_t n){count_allocation();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
void *operator new(size_t n,std::align_val_t a){count_allocation();void *p=nullptr;if(posix_memalign(&p,std::max(size_t(a),sizeof(void*)),n?n:1))throw std::bad_alloc();return p;}
void *operator new[](size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void*p,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,std::align_val_t)noexcept{std::free(p);}
void operator delete(void*p,size_t,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,size_t,std::align_val_t)noexcept{std::free(p);}
// Complete replacement family: sanitizer/LLVM may call nothrow forms even
// before this test arms a fault. Mixing ASan's default nothrow new with our
// malloc/free replacement is an allocator mismatch, never a suppression case.
void *operator new(size_t n,const std::nothrow_t&)noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
void *operator new[](size_t n,const std::nothrow_t&)noexcept{try{return ::operator new[](n);}catch(...){return nullptr;}}
void operator delete(void*p,const std::nothrow_t&)noexcept{std::free(p);}void operator delete[](void*p,const std::nothrow_t&)noexcept{std::free(p);}
void *operator new(size_t n,std::align_val_t a,const std::nothrow_t&)noexcept{try{return ::operator new(n,a);}catch(...){return nullptr;}}
void *operator new[](size_t n,std::align_val_t a,const std::nothrow_t&)noexcept{try{return ::operator new[](n,a);}catch(...){return nullptr;}}
void operator delete(void*p,std::align_val_t,const std::nothrow_t&)noexcept{std::free(p);}void operator delete[](void*p,std::align_val_t,const std::nothrow_t&)noexcept{std::free(p);}
extern "C" void __real_glBufferData(GLenum,GLsizeiptr,const void*,GLenum);
extern "C" void __wrap_glBufferData(GLenum t,GLsizeiptr n,const void *p,GLenum u){++gpu_allocations;__real_glBufferData(t,reject_at(deny_allocate)?-1:n,p,u);}
extern "C" void __real_glBufferSubData(GLenum,GLintptr,GLsizeiptr,const void*);
extern "C" void __wrap_glBufferSubData(GLenum t,GLintptr o,GLsizeiptr n,const void *p){++gpu_submits;__real_glBufferSubData(t,o,reject_at(deny_upload)?-1:n,p);}
extern "C" void __real_glCopyBufferSubData(GLenum,GLenum,GLintptr,GLintptr,GLsizeiptr);
extern "C" void __wrap_glCopyBufferSubData(GLenum a,GLenum b,GLintptr i,GLintptr j,GLsizeiptr n){++gpu_copies;__real_glCopyBufferSubData(a,b,i,j,reject_at(deny_copy)?-1:n);}
extern "C" GLsync __real_glFenceSync(GLenum,GLbitfield);
extern "C" GLsync __wrap_glFenceSync(GLenum condition,GLbitfield flags){return __real_glFenceSync(reject_at(deny_fence)?GL_NONE:condition,flags);}
extern "C" GLenum __real_glClientWaitSync(GLsync,GLbitfield,GLuint64);
extern "C" GLenum __wrap_glClientWaitSync(GLsync sync,GLbitfield flags,GLuint64 timeout){
  ++retirement_queries;
  if(hold_retirement)return GL_TIMEOUT_EXPIRED; // Explicit conservative API-query shim.
  return __real_glClientWaitSync(sync,reject_at(deny_wait)?GLbitfield(-1):flags,timeout);
}
int main()try{
  allocations=0;measure=true;
  void *p=::operator new(17,std::nothrow);::operator delete(p,std::nothrow);
  p=::operator new[](17,std::nothrow);::operator delete[](p,std::nothrow);
  p=::operator new(17,std::align_val_t(32),std::nothrow);::operator delete(p,std::align_val_t(32),std::nothrow);
  p=::operator new[](17,std::align_val_t(32),std::nothrow);::operator delete[](p,std::align_val_t(32),std::nothrow);
  measure=false;check(allocations==4,"All nothrow and aligned replacement forms share the same actual allocation hook");
  NativeGraphics context;std::cout<<"GPU "<<glGetString(GL_RENDERER)<<'\n';
  auto scene=fixture();const auto native=scene.encode();auto base=SculptSceneChunks::capture(scene,{7,1,1,true,true}).chunks;
  auto changed=scene.snapshot();changed.object(1).volume=std::make_shared<Volume>(*scene.object(1).volume);changed.object(1).volume->fill_mask(.71f);
  auto masks=SculptSceneChunks::capture(changed,{7,2,2,true,true},base).chunks;
  render_detail::SculptGpuArena arena;auto first=arena.stage(base);const auto commands=first->commands.at(1);
  size_t raw_runs=0;base->find(1)->geometry->for_each_run([&](const auto &){++raw_runs;});
  check(commands->source_runs==raw_runs&&commands->counts.size()<raw_runs,"Cached raw command capacity stays independent of adjacent-only coalescing");const auto positions=first->positions.get();arena.commit(std::move(first));
  const auto baseline_bytes=arena.live_bytes(),geometry_uploads=arena.uploads,base_scalars=arena.scalar_uploads;
  auto next=arena.stage(masks);check(next->commands.at(1)==commands&&next->positions.get()==positions,"Mask-only transaction preserves actual immutable geometry command owner and GPU positions");
  check(next->copy_bytes>0&&next->geometry_transfers==0&&next->scalar_transfers>0,"Mask transaction stages a new scalar buffer with GPU copy, no geometry upload");next.reset();drain(arena);
  check(arena.live_bytes()==baseline_bytes&&arena.uploads==geometry_uploads&&arena.scalar_uploads==base_scalars,"Abandoned valid transaction preserves all accepted buffer reservations and counters");
  const auto before_gpu=gpu_allocations;
  render_detail::SculptGpuArena limited(baseline_bytes);limited.commit(limited.stage(base));
  const auto after_gpu=gpu_allocations;check(rejects([&]{limited.stage(masks);})&&gpu_allocations==after_gpu,"Exact old-plus-new resource guard refuses BEFORE scalar allocation; final size alone is insufficient");
  check(limited.live_bytes()==baseline_bytes&&before_gpu<after_gpu,"Failed staging retains complete bounded accepted GPU stores");limited.clear(true);drain(limited);
  // Real GL calls reject invalid sizes injected at EACH actual allocation,
  // transfer and GPU-copy boundary. This is a vendor-API error-control shim,
  // not a claim of physical device loss or allocator-driver exhaustion.
  auto faults=[&](int64_t &control,size_t observed,const char *text){
    for(size_t point=0;point<observed;++point){hit=false;control=int64_t(point);bool failed=rejects([&]{auto candidate=arena.stage(masks);});control=-1;drain(arena);
      check(hit&&failed&&arena.live_bytes()==baseline_bytes&&arena.uploads==geometry_uploads,text);
      auto retry=arena.stage(masks);check(retry->geometry_transfers==0&&retry->scalar_transfers>0,"Actual retry after GL staging failure prepares complete current scalar state");retry.reset();drain(arena);
    }
  };
  auto a0=gpu_allocations,u0=gpu_submits,c0=gpu_copies;auto measured=arena.stage(masks);const auto ac=gpu_allocations-a0,uc=gpu_submits-u0,cc=gpu_copies-c0;measured.reset();drain(arena);
  faults(deny_allocate,ac,"Every real GL allocation failure leaves accepted GPU buffers exact");
  faults(deny_upload,uc,"Every real partial upload failure leaves accepted scalar owner exact");
  faults(deny_copy,cc,"Every real GPU-copy failure leaves accepted scalar owner exact");
  std::cout<<"GPU_BOUNDARIES allocation="<<ac<<" upload="<<uc<<" copy="<<cc<<'\n';
  size_t polls=0;auto cancel_measure=arena.stage(masks,[&]{++polls;return false;});cancel_measure.reset();drain(arena);
  for(size_t point=0;point<polls;++point){size_t at=0;const auto cancelled=rejects([&]{arena.stage(masks,[&]{return at++==point;});});drain(arena);check(cancelled&&arena.live_bytes()==baseline_bytes,"Every actual preparation cancellation boundary preserves accepted GPU state");}
  allocations=0;measure=true;auto allocation_measure=arena.stage(masks);measure=false;const auto total=allocations;allocation_measure.reset();drain(arena);
  for(size_t point=0;point<total;++point){hit=false;deny=int64_t(point);bool failed=rejects([&]{arena.stage(masks);});deny=-1;drain(arena);
    check(hit&&failed&&arena.live_bytes()==baseline_bytes,"Every actual persistent C++ allocation prefix rolls back GPU staging");
  }
  // Known deleted stores stay charged until the actual sync is observed.
  // Withholding completion below is an explicit query shim, not evidence the
  // physical GPU happened to remain busy for this whole host-side interval.
  {
    render_detail::SculptGpuArena debt(baseline_bytes+arena.mask_bytes());debt.commit(debt.stage(base));
    auto queued=debt.stage(masks);const auto scalar_store=queued->masks->bytes;
    hold_retirement=true;queued.reset();
    check(debt.retired_bytes()==scalar_store&&debt.live_bytes()==baseline_bytes+scalar_store,"Deleted but unconfirmed GPU stores retain their complete requested-byte charge");
    const auto old_allocations=gpu_allocations;
    check(rejects([&]{debt.stage(masks);})&&gpu_allocations==old_allocations&&debt.retired_bytes()==scalar_store,"Pending retirement debt refuses another store before allocation even though accepted plus new alone fits");
    const auto queries=retirement_queries;auto unchanged=debt.stage(base);unchanged.reset();
    check(retirement_queries==queries,"Unchanged accepted scene performs no retirement query or wait");
    hold_retirement=false;drain(debt);
    check(debt.retired_bytes()==0&&debt.live_bytes()==baseline_bytes,"Actual signaled GPU fence drains exactly the charged retirement debt");
    auto retry=debt.stage(masks);retry.reset();drain(debt);debt.clear(true);drain(debt);
  }
  {
    auto queued=arena.stage(masks);hit=false;deny_fence=0;queued.reset();deny_fence=-1;
    check(hit&&arena.retired_bytes()>0&&rejects([&]{arena.collect_retired();}),"Actual fence-creation API rejection preserves debt and is explicitly reported");
    check(arena.retired_bytes()>0,"Fence-creation failure cannot falsely release requested GPU storage");drain(arena);
    auto again=arena.stage(masks);again.reset();hit=false;deny_wait=0;
    check(rejects([&]{arena.collect_retired();})&&hit&&arena.retired_bytes()>0,"Actual fence-query API rejection keeps prior storage charged and reports failure");
    deny_wait=-1;drain(arena);
    check(arena.live_bytes()==baseline_bytes,"Actual retry after fence creation/query failures recovers old accepted GPU state");
  }
  // Genuine geometry growth, with retained chunks copied only on the GPU.
  // The new object is real native Volume geometry, never a fabricated packet.
  auto expanded=scene.snapshot();expanded.add("Arena growth",shells(),{});
  const auto expanded_native=expanded.encode();auto geometry=SculptSceneChunks::capture(expanded,{7,3,3,true,true},base).chunks;
  const auto ga0=gpu_allocations,gu0=gpu_submits,gc0=gpu_copies;
  auto growth=arena.stage(geometry);const auto ga=gpu_allocations-ga0,gu=gpu_submits-gu0,gc=gpu_copies-gc0;
  check(growth->geometry_transfers>0&&growth->geometry_transfers<growth->entries.size()&&growth->copy_bytes>0,"Real growth uploads only new geometry owners; retained local arrays move by GPU copy");
  check(growth->positions.get()!=positions,"Real growth stages a distinct reserved GPU arena before publication");growth.reset();drain(arena);
  for(auto pair:{std::pair{&deny_allocate,ga},std::pair{&deny_upload,gu},std::pair{&deny_copy,gc}}){
    for(size_t point=0;point<pair.second;++point){hit=false;*pair.first=int64_t(point);bool failed=rejects([&]{arena.stage(geometry);});*pair.first=-1;drain(arena);
      check(hit&&failed&&arena.live_bytes()==baseline_bytes&&arena.uploads==geometry_uploads,"Every real growth allocation/upload/copy boundary retains old accepted geometry exactly");
    }
  }
  std::cout<<"GROWTH_GPU_BOUNDARIES allocation="<<ga<<" upload="<<gu<<" copy="<<gc<<'\n';
  auto retry_growth=arena.stage(geometry);check(retry_growth->geometry_transfers>0,"Actual geometry retry succeeds after every growth error");
  // Retained test snapshot deliberately remains live here: its bytes must also
  // remain in the SAME unique-buffer ledger until its last owner is dropped.
  check(arena.live_bytes()>baseline_bytes&&arena.live_bytes()<=arena.gpu_limit,"Outstanding staged lease is charged while caller retains it");
  render_detail::SculptGpuArena metadata_limited(arena.gpu_limit,arena.metadata_bytes());metadata_limited.commit(metadata_limited.stage(base));
  const auto before_metadata=gpu_allocations;check(rejects([&]{metadata_limited.stage(masks);})&&gpu_allocations==before_metadata,"CPU metadata old-plus-new guard refuses before GPU staging");metadata_limited.clear(true);drain(metadata_limited);
  check(expanded.encode()==expanded_native,"Geometry growth/copy failures leave real expanded native input unchanged");
  // Release the test-held candidate before the final scalar publication.
  retry_growth.reset();drain(arena);
  auto final=arena.stage(masks);const auto bytes=final->masks->bytes;arena.commit(std::move(final));drain(arena);
  check(arena.live_bytes()==baseline_bytes&&arena.uploads==geometry_uploads&&arena.scalar_uploads>base_scalars,"Successful atomic publication releases prior scalar storage with geometry unchanged");
  check(arena.peak_bytes()>=baseline_bytes+bytes&&arena.peak_bytes()<=arena.gpu_limit,"Actual live ledger charges previous plus staged scalar store and all arena reserves");
  auto empty=arena.stage({});arena.commit(std::move(empty));drain(arena);check(arena.live_bytes()==0,"Empty accepted snapshot prunes all obsolete arena resources");
  check(scene.encode()==native,"All native source state stays byte-identical across GPU prepare/fail/retry/cancel/publication");
  check(glGetError()==GL_NO_ERROR,"No latent GL error survives checked fault controls");
  std::cout<<"CPP_BOUNDARIES full="<<total<<" CANCEL_BOUNDARIES full="<<polls<<" accepted_bytes="<<baseline_bytes<<" peak_bytes="<<arena.peak_bytes()<<'\n';
  std::cout<<"PASS total "<<checks<<'\n';return 0;
}catch(const std::exception&e){deny=-1;measure=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
