// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <new>
#include <sycl/sycl.hpp>

namespace dome::detail::weight_transfer {
class SyclSurfaceMemory {
protected:
  sycl::queue queue_{sycl::gpu_selector_v,[](sycl::exception_list errors){
    for(auto error:errors)std::rethrow_exception(error);
  },sycl::property_list{sycl::property::queue::in_order{}}};
public:
  void *allocate(size_t bytes){
    try{
      auto *pointer=sycl::malloc_device(bytes,queue_);
      if(!pointer)throw std::bad_alloc();
      return pointer;
    }catch(const sycl::exception &error){
      if(error.code()==sycl::make_error_code(sycl::errc::memory_allocation))throw std::bad_alloc();
      throw;
    }
  }
  void release(void *pointer) noexcept {try{sycl::free(pointer,queue_);}catch(...){}}
  void wait(){queue_.wait_and_throw();}
  void drain() noexcept {try{wait();}catch(...){}}
  void upload(void *to,const void *from,size_t bytes){queue_.memcpy(to,from,bytes);}
  void read(void *to,const void *from,size_t bytes){queue_.memcpy(to,from,bytes).wait_and_throw();}
};
} // namespace dome::detail::weight_transfer
