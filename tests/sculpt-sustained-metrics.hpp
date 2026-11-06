// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "sculpt-chunk-masks.hpp"
#include "sculpt-placed-chunks.hpp"
#include <atomic>
#include <ctime>
#include <new>
#include <cstdlib>
#include <thread>

// Test executable interposition only. No producer layout, cancellation guard,
// allocator failure semantics or runtime product instrumentation is changed.
namespace sustained {
enum Stage { Capture, ExtractionBuild, ExtractionUpdate, Placement, Scalars,
             Nearest, Ray, StageCount };
struct Count {uint64_t calls=0,wall_ns=0,cpu_ns=0;};
struct AtomicCount {std::atomic_uint64_t calls{0},wall_ns{0},cpu_ns{0};};
inline std::atomic_bool measuring{false},allocations{false};
// Correctness-only seam after the actual complete factory returns. A stale
// candidate is held before Document acceptance, never fabricated or flattened.
inline std::atomic_bool stop_at_capture{false},capture_waiting{false},capture_release{false};
inline std::atomic_uint64_t blocked_generation{0};
inline std::atomic_uint64_t news{0},new_bytes{0},buffer_calls{0},buffer_allocation_bytes{0},buffer_payload_bytes{0},scalar_payload_bytes{0},scalar_calls{0};
inline std::array<AtomicCount,StageCount> stages;
inline uint64_t clock_ns(clockid_t clock){timespec t{};if(clock_gettime(clock,&t))std::abort();return uint64_t(t.tv_sec)*1000000000u+uint64_t(t.tv_nsec);}
struct StageScope {
  Stage stage;bool active;uint64_t wall=0,cpu=0;
  explicit StageScope(Stage s):stage(s),active(measuring.load(std::memory_order_relaxed)){
    if(active){wall=clock_ns(CLOCK_MONOTONIC);cpu=clock_ns(CLOCK_THREAD_CPUTIME_ID);}
  }
  ~StageScope(){if(active){auto &s=stages[stage];s.wall_ns.fetch_add(clock_ns(CLOCK_MONOTONIC)-wall,std::memory_order_relaxed);s.cpu_ns.fetch_add(clock_ns(CLOCK_THREAD_CPUTIME_ID)-cpu,std::memory_order_relaxed);s.calls.fetch_add(1,std::memory_order_relaxed);}}
};
inline void allocation(size_t n){if(allocations.load(std::memory_order_relaxed)&&measuring.load(std::memory_order_relaxed)){news.fetch_add(1,std::memory_order_relaxed);new_bytes.fetch_add(n,std::memory_order_relaxed);}}
struct Counters {
  uint64_t news,new_bytes,buffer_calls,buffer_allocation_bytes,buffer_payload_bytes,scalar_payload_bytes,scalar_calls;
  std::array<Count,StageCount> stages;
  static Counters read(){Counters r{::sustained::news.load(),::sustained::new_bytes.load(),::sustained::buffer_calls.load(),::sustained::buffer_allocation_bytes.load(),::sustained::buffer_payload_bytes.load(),::sustained::scalar_payload_bytes.load(),::sustained::scalar_calls.load(),{}};for(size_t i=0;i<StageCount;++i)r.stages[i]={::sustained::stages[i].calls.load(),::sustained::stages[i].wall_ns.load(),::sustained::stages[i].cpu_ns.load()};return r;}
};
}
// Count requested bytes for ordinary/aligned C++ new only, across all workers.
// This does not claim to count malloc, driver memory, live bytes or peak RSS.
void *operator new(size_t n){if(void *p=std::malloc(n?n:1)){sustained::allocation(n);return p;}throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void *p)noexcept{std::free(p);}
void operator delete[](void *p)noexcept{std::free(p);}
void operator delete(void *p,size_t)noexcept{std::free(p);}
void operator delete[](void *p,size_t)noexcept{std::free(p);}
void *operator new(size_t n,std::align_val_t a){void *p=nullptr;const auto alignment=std::max(size_t(a),alignof(void*));if(posix_memalign(&p,alignment,n?n:1))throw std::bad_alloc();sustained::allocation(n);return p;}
void *operator new[](size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void *p,std::align_val_t)noexcept{std::free(p);}
void operator delete[](void *p,std::align_val_t)noexcept{std::free(p);}
void operator delete(void *p,size_t,std::align_val_t)noexcept{std::free(p);}
void operator delete[](void *p,size_t,std::align_val_t)noexcept{std::free(p);}
void *operator new(size_t n,const std::nothrow_t&)noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
void *operator new[](size_t n,const std::nothrow_t&)noexcept{try{return ::operator new[](n);}catch(...){return nullptr;}}
void operator delete(void *p,const std::nothrow_t&)noexcept{std::free(p);}
void operator delete[](void *p,const std::nothrow_t&)noexcept{std::free(p);}
void *operator new(size_t n,std::align_val_t a,const std::nothrow_t&)noexcept{try{return ::operator new(n,a);}catch(...){return nullptr;}}
void *operator new[](size_t n,std::align_val_t a,const std::nothrow_t&)noexcept{try{return ::operator new[](n,a);}catch(...){return nullptr;}}
void operator delete(void *p,std::align_val_t,const std::nothrow_t&)noexcept{std::free(p);}
void operator delete[](void *p,std::align_val_t,const std::nothrow_t&)noexcept{std::free(p);}

// Whole, actual exported factory calls: inclusive durations deliberately nest.
// The output must not sum them as a decomposition of one worker duration.
using namespace dome;
using namespace dome::detail;
#define SUSTAINED_CAPTURE "_ZN4dome6detail17SculptSceneChunks7captureERKNS_5SceneENS0_23SculptSceneChunkContextERKSt10shared_ptrIKS1_ENS0_22SculptSceneChunkLimitsESt8functionIFbvEE"
#define SUSTAINED_BUILD "_ZN4dome6detail19SculptSurfaceChunks5buildERKNS_6VolumeENS0_17SculptChunkLimitsESt8functionIFbvEE"
#define SUSTAINED_UPDATE "_ZNK4dome6detail19SculptSurfaceChunks6updateERKNS_6VolumeENS0_17SculptChunkLimitsESt8functionIFbvEE"
#define SUSTAINED_PLACEMENT "_ZN4dome6detail18SculptPlacedChunks7captureERKSt10shared_ptrIKNS0_17SculptSceneChunksEEmRKS2_IKS1_EbNS0_24SculptPlacedChunksLimitsESt8functionIFbvEE"
#define SUSTAINED_SCALARS "_ZN4dome6detail16SculptChunkMasks7captureESt10shared_ptrIKNS0_19SculptSurfaceChunksEES2_IKNS0_18SculptMaskSnapshotEERKS2_IKS1_EmmSt8functionIFbvEE"
#define SUSTAINED_NEAREST "_ZNK4dome12SurfaceQuery14nearest_resultENS_4Vec3ERKNS_15AffineTransformERKNS_19SurfaceQueryOptionsE"
#define SUSTAINED_RAY "_ZNK4dome12SurfaceQuery14raycast_resultENS_4Vec3ES1_fRKNS_15AffineTransformERKNS_19SurfaceQueryOptionsE"
#define DECLARE_WRAP(ret,name,symbol,args) extern "C" ret real_##name args asm("__real_" symbol); extern "C" ret wrap_##name args asm("__wrap_" symbol)
DECLARE_WRAP(SculptScenePreview,capture,SUSTAINED_CAPTURE,(const Scene &s,SculptSceneChunkContext c,const SculptSceneChunks::Ptr &b,SculptSceneChunkLimits l,LocalSurfaceCache::Cancel x));
extern "C" SculptScenePreview wrap_capture(const Scene &s,SculptSceneChunkContext c,const SculptSceneChunks::Ptr &b,SculptSceneChunkLimits l,LocalSurfaceCache::Cancel x){sustained::StageScope t(sustained::Capture);auto result=real_capture(s,c,b,l,std::move(x));if(sustained::stop_at_capture.exchange(false)){sustained::blocked_generation=result.chunks->context().generation;sustained::capture_waiting=true;while(!sustained::capture_release.load())std::this_thread::sleep_for(std::chrono::milliseconds(1));}return result;}
DECLARE_WRAP(SculptSurfaceChunks::Ptr,build,SUSTAINED_BUILD,(const Volume &v,SculptChunkLimits l,LocalSurfaceCache::Cancel x));
extern "C" SculptSurfaceChunks::Ptr wrap_build(const Volume &v,SculptChunkLimits l,LocalSurfaceCache::Cancel x){sustained::StageScope t(sustained::ExtractionBuild);return real_build(v,l,std::move(x));}
DECLARE_WRAP(SculptSurfaceChunks::Ptr,update,SUSTAINED_UPDATE,(const SculptSurfaceChunks *self,const Volume &v,SculptChunkLimits l,LocalSurfaceCache::Cancel x));
extern "C" SculptSurfaceChunks::Ptr wrap_update(const SculptSurfaceChunks *self,const Volume &v,SculptChunkLimits l,LocalSurfaceCache::Cancel x){sustained::StageScope t(sustained::ExtractionUpdate);return real_update(self,v,l,std::move(x));}
DECLARE_WRAP(SculptPlacedChunks::Ptr,placement,SUSTAINED_PLACEMENT,(const SculptSceneChunks::Ptr &s,uint64_t id,const SculptPlacedChunks::Ptr &b,bool q,SculptPlacedChunksLimits l,LocalSurfaceCache::Cancel x));
extern "C" SculptPlacedChunks::Ptr wrap_placement(const SculptSceneChunks::Ptr &s,uint64_t id,const SculptPlacedChunks::Ptr &b,bool q,SculptPlacedChunksLimits l,LocalSurfaceCache::Cancel x){sustained::StageScope t(sustained::Placement);return real_placement(s,id,b,q,l,std::move(x));}
DECLARE_WRAP(SculptChunkMasks::Ptr,scalars,SUSTAINED_SCALARS,(SculptSurfaceChunks::Ptr s,std::shared_ptr<const SculptMaskSnapshot> m,const SculptChunkMasks::Ptr &b,size_t r,size_t w,LocalSurfaceCache::Cancel x));
extern "C" SculptChunkMasks::Ptr wrap_scalars(SculptSurfaceChunks::Ptr s,std::shared_ptr<const SculptMaskSnapshot> m,const SculptChunkMasks::Ptr &b,size_t r,size_t w,LocalSurfaceCache::Cancel x){sustained::StageScope t(sustained::Scalars);return real_scalars(std::move(s),std::move(m),b,r,w,std::move(x));}
DECLARE_WRAP(std::optional<SurfaceQueryResult>,nearest,SUSTAINED_NEAREST,(const SurfaceQuery *s,Vec3 p,const AffineTransform &a,const SurfaceQueryOptions &o));
extern "C" std::optional<SurfaceQueryResult> wrap_nearest(const SurfaceQuery *s,Vec3 p,const AffineTransform &a,const SurfaceQueryOptions &o){sustained::StageScope t(sustained::Nearest);return real_nearest(s,p,a,o);}
DECLARE_WRAP(std::optional<SurfaceQueryResult>,ray,SUSTAINED_RAY,(const SurfaceQuery *s,Vec3 p,Vec3 d,float m,const AffineTransform &a,const SurfaceQueryOptions &o));
extern "C" std::optional<SurfaceQueryResult> wrap_ray(const SurfaceQuery *s,Vec3 p,Vec3 d,float m,const AffineTransform &a,const SurfaceQueryOptions &o){sustained::StageScope t(sustained::Ray);return real_ray(s,p,d,m,a,o);}
#undef DECLARE_WRAP
