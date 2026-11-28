// SPDX-License-Identifier: GPL-3.0-or-later

#include <sycl/sycl.hpp>
#include "emission-raster-upload.hpp"

namespace dome::detail::emission_raster {
void launch_bake(sycl::queue &,DeviceInput,size_t,size_t,uint32_t *);
void launch_typed(sycl::queue &,DeviceInput,size_t,size_t,uint32_t *);
void launch_finite(sycl::queue &,DeviceInput,FiniteGrid,size_t,size_t,uint32_t *);
void launch_texture_set(sycl::queue &,DeviceInput,size_t,size_t,uint32_t *,bool coefficients);
void launch_texture_set_source(sycl::queue &,DeviceInput,size_t,size_t,uint32_t *,bool coefficients);

namespace {
class Device {
public:
  Device():queue_(sycl::gpu_selector_v,[](sycl::exception_list errors){
    for(auto error:errors)std::rethrow_exception(error);
  },sycl::property_list{sycl::property::queue::in_order{}}){memory_.reserve(16);}
  ~Device(){
    try{queue_.wait_and_throw();}catch(...){}
    for(auto *p:memory_)try{sycl::free(p,queue_);}catch(...){}
  }
  void *allocate(size_t bytes){
    memory_.push_back(nullptr);
    auto *pointer=sycl::malloc_device(bytes,queue_);
    if(!pointer)throw std::bad_alloc();
    memory_.back()=pointer;
    return pointer;
  }
  void upload(void *device,const void *host,size_t bytes){queue_.memcpy(device,host,bytes).wait_and_throw();}
  void download(void *host,const void *device,size_t bytes){queue_.memcpy(host,device,bytes).wait_and_throw();}
  void render(DeviceInput input,size_t first,size_t count,uint32_t *out){
    if(input.source.typed_transform){
      launch_typed(queue_,input,first,count,out);
      return;
    }
    if(!input.layer_count&&!input.source.rgba){
      launch_bake(queue_,input,first,count,out);
      return;
    }
    queue_.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
      emission_raster::render(input,first+id[0],out+10*id[0]);
    });
  }
  void render_finite(DeviceInput input,FiniteGrid grid,size_t first,size_t count,uint32_t *out){
    launch_finite(queue_,input,grid,first,count,out);
  }
  void render_texture_set(DeviceInput input,size_t first,size_t count,uint32_t *out,bool coefficients){
    // Keep affine wrapping out of the first compilation for constant and
    // already aligned backdrops.
    if(input.source.coordinates.columns)
      launch_texture_set_source(queue_,input,first,count,out,coefficients);
    else launch_texture_set(queue_,input,first,count,out,coefficients);
  }
private:
  sycl::queue queue_;
  std::vector<void *> memory_;
};
}
std::unique_ptr<EmissionRasterDevice> upload(const Input &input,const std::function<bool()> &cancelled){
  return std::make_unique<Raster<Device>>(input,cancelled);
}
}
