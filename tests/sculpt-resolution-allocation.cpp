// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/sculpt-resolution.hpp"
#include "../src/ray-index.hpp"
#include <cstdlib>
#include <iostream>
namespace {
thread_local int64_t fail_after=-1;
thread_local size_t allocations=0;
thread_local bool observe=false;
}
void *operator new(size_t n) {
 if(fail_after==0)throw std::bad_alloc();if(fail_after>0)--fail_after;
 if(observe)++allocations;
 if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();
}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void *p)noexcept{std::free(p);}void operator delete[](void *p)noexcept{std::free(p);}
void operator delete(void *p,size_t)noexcept{std::free(p);}void operator delete[](void *p,size_t)noexcept{std::free(p);}
using namespace dome;
int main()try {
 auto require=[](bool value,const char *message){if(!value)throw std::runtime_error(message);};
 Scene source;auto &v=*source.active().volume;v.begin_stroke();Brush b;b.radius=.137f;b.from=b.to={.013f,.017f,-.011f};b.blend=0;v.stamp(b);v.end_stroke();v.fill_mask(.37f);
 auto owner=source.active().volume;auto cells=owner->voxels();auto mask=owner->mask_values();auto settings=owner->sculpt_settings();
 SculptResolutionOptions options;options.voxel_size=owner->voxel_size();
 fail_after=0;auto no_op=resample_sculpt(*owner,options);fail_after=-1;
 require(!no_op.replacement && !no_op.report.changed,"same resolution unexpectedly allocated or changed the source");
 // The indexed picker intentionally allocates on cold preparation. Propagate
 // genuine OOM without changing authority, then retain the warmed no-allocation
 // contract independently from resolution's allocation-free same-voxel path.
 sculpt_detail::clear_ray_index_cache();bool cold_failed=false;fail_after=0;
 try{owner->raycast({.013f,.017f,1},{0,0,-1},2);}catch(const std::bad_alloc&){cold_failed=true;}fail_after=-1;
 require(cold_failed&&owner->voxels()==cells&&owner->mask_values()==mask,"cold picker allocation failure changed original field/protection");
 auto warm=owner->raycast({.013f,.017f,1},{0,0,-1},2);require(bool(warm),"cold indexed ray missed known geometry");
 fail_after=0;auto hit=owner->raycast({.013f,.017f,1},{0,0,-1},2);fail_after=-1;require(bool(hit),"warmed bounded ray allocated or missed known geometry");
 options.voxel_size=owner->voxel_size()*.5f;
 Scene successful=source;allocations=0;observe=true;bool changed=resample_sculpt_object(successful,1,options);observe=false;
 require(changed && allocations>100,"allocation baseline failed");size_t total=allocations;
 std::vector<size_t> boundaries;for(size_t n=0;n<64;++n)boundaries.push_back(n);
 for(size_t n=64;n<total;n*=2)boundaries.push_back(n);
 for(size_t n=0;n<16 && n<total;++n)boundaries.push_back(total-1-n);
 size_t failures=0;
 for(auto boundary:boundaries) {
  Scene candidate=source;bool failed=false;fail_after=int64_t(boundary);
  try{resample_sculpt_object(candidate,1,options);}catch(const std::bad_alloc &){failed=true;}
  fail_after=-1;
  require(failed,"a measured allocation-fault boundary unexpectedly succeeded");
  require(candidate.active().volume==owner && source.active().volume==owner && owner->voxels()==cells && owner->mask_values()==mask && owner->mask_default()==.37f && owner->sculpt_settings().symmetry_axes==settings.symmetry_axes,"allocation failure changed original source/ownership/protection");
  ++failures;
 }
 std::cout<<"PASS allocation-free no-op/warm ray, exact cold-OOM preservation and "<<failures<<" transactional allocation boundaries through "<<total<<" measured allocations\n";
 return 0;
}catch(const std::exception &e){fail_after=-1;observe=false;std::cerr<<e.what()<<'\n';return 1;}
