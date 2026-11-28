// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "volume-binding-device-solver.hpp"

namespace dome::detail::volume_device {
namespace {
class SyclApi {
  sycl::queue queue_{sycl::gpu_selector_v,[](sycl::exception_list errors){
    for(auto error:errors)std::rethrow_exception(error);
  },sycl::property_list{sycl::property::queue::in_order{}}};
public:
  void *allocate(size_t bytes){
    auto *pointer=sycl::malloc_device(bytes,queue_);
    if(!pointer)throw std::bad_alloc();
    return pointer;
  }
  void release(void *pointer) noexcept {try{sycl::free(pointer,queue_);}catch(...){}}
  void wait(){queue_.wait_and_throw();}
  void drain() noexcept {try{wait();}catch(...){}}
  void upload(void *to,const void *from,size_t bytes){queue_.memcpy(to,from,bytes);}
  void read(void *to,const void *from,size_t bytes){queue_.memcpy(to,from,bytes).wait_and_throw();}
  void reset(uint32_t *flag){queue_.fill(flag,uint32_t(0),1);}
  void initialize(float *field,size_t count){queue_.fill(field,infinity,count);}
  void scatter(float *field,const Seed *seeds,size_t count){
    queue_.parallel_for(sycl::range<1>{count},[=](sycl::id<1> index){
      const auto seed=seeds[index[0]];
      field[seed.node]=seed.cells;
    });
  }
  void step(View graph,const float *current,float *next,size_t count,uint32_t *flag){
    if(!flag){
      queue_.parallel_for(sycl::range<1>{count},[=](sycl::id<1> index){
        next[index[0]]=relax(index[0],graph,current);
      });
      return;
    }
    const size_t rounded=(count+lanes-1)/lanes*lanes;
    queue_.parallel_for(sycl::nd_range<1>{{rounded},{lanes}},[=](sycl::nd_item<1> item){
      const size_t index=item.get_global_linear_id();
      bool changed=false;
      if(index<count){
        const auto value=relax(index,graph,current);
        changed=value<current[index];
        next[index]=value;
      }
      const bool group_changed=sycl::any_of_group(item.get_group(),changed);
      if(group_changed&&item.get_local_linear_id()==0){
        using Flag=sycl::atomic_ref<uint32_t,sycl::memory_order::relaxed,
          sycl::memory_scope::device,sycl::access::address_space::global_space>;
        Flag(*flag).fetch_or(1);
      }
    });
  }
};
} // namespace

std::unique_ptr<Solver> upload(const Graph &graph,size_t maximum_seeds){
  return std::make_unique<DeviceSolver<SyclApi>>(graph,maximum_seeds);
}
} // namespace dome::detail::volume_device
