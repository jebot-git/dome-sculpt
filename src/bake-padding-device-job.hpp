// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake-device-buffer.hpp"
#include "bake-padding-kernel.hpp"
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace dome::detail::bake_padding {
template<class Api>
void resolve_device(uint32_t resolution,uint32_t radius,std::span<const uint8_t> coverage,
                    const Apply &apply,const std::function<bool()> &cancelled){
  constexpr uint32_t chunk=65536;
  if(resolution<8||resolution>4096||radius>64||coverage.size()!=size_t(resolution)*resolution||!apply)
    throw std::invalid_argument("Invalid Bake padding request");
  if(!radius)return;
  const auto poll=[&]{if(cancelled&&cancelled())throw std::runtime_error("Mesh baking cancelled");};
  poll();
  Api api;
  const auto pixels=uint32_t(coverage.size());
  BakeDeviceBuffer<Api,uint32_t> owners(api,pixels),queue(api,pixels),counter(api,1);
  const Frontier view{owners.data,queue.data,counter.data,resolution};
  const auto batches=[&](uint32_t start,uint32_t end,auto launch){
    while(start<end){
      poll();
      const auto count=std::min(chunk,end-start);
      launch(start,count);
      start+=count;
    }
  };
  const auto next_end=[&](uint32_t minimum){
    uint32_t end;
    api.read(&end,counter.data,sizeof(end));
    if(end<minimum||end>pixels)throw std::runtime_error("Invalid Bake padding frontier");
    return end;
  };
  try{
    uint32_t end;
    {
      BakeDeviceBuffer<Api,uint8_t> input(api,pixels);
      try{
        api.upload(input.data,coverage.data(),coverage.size_bytes());
        api.reset(counter.data);
        batches(0,pixels,[&](uint32_t first,uint32_t count){api.seed_padding(view,input.data,first,count);});
        end=next_end(0);
      }catch(...){api.drain();throw;}
    }
    uint32_t first=0;
    for(uint32_t distance=2;distance<=radius&&first<end;++distance){
      batches(first,end,[&](uint32_t offset,uint32_t count){api.expand_padding(view,offset,count,distance);});
      first=end;
      end=next_end(first);
    }
    poll();
    if(!end)return;
    BakeDeviceBuffer<Api,Pixel> pairs(api,std::min(end,chunk));
    std::vector<Pixel> host(std::min(end,chunk));
    try{
      batches(0,end,[&](uint32_t offset,uint32_t count){
        api.gather_padding(view,offset,count,pairs.data);
        api.read(host.data(),pairs.data,count*sizeof(Pixel));
        poll();
        apply(std::span<const Pixel>(host).first(count));
      });
    }catch(...){api.drain();throw;}
  }catch(...){api.drain();throw;}
}
}
