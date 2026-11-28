// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "surface-heat-device-fields.hpp"

namespace dome::detail::surface_heat_device {
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
  template<class Operation> void launch(size_t count,Operation operation){
    queue_.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){operation(id[0]);});
  }
};
} // namespace
std::unique_ptr<Fields> create(const System &input){return std::make_unique<DeviceFields<SyclApi>>(input);}
} // namespace dome::detail::surface_heat_device
