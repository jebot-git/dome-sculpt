// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake-device-buffer.hpp"
#include "bake-raster-kernel.hpp"
#include <stdexcept>
#include <vector>

namespace dome::detail::bake_raster {
template<class Api> class DeviceRaster final:public Raster {
  template<class T> using Buffer=BakeDeviceBuffer<Api,T>;
  static constexpr size_t block_capacity=candidate_capacity/candidates_per_block;
  Api api_;
  Buffer<float> axis_;
  Buffer<uint8_t> writable_;
  Buffer<uint32_t> owners_;
  Buffer<Span> spans_;
  Buffer<Sample> candidates_,output_;
  Buffer<Block> ranges_;
  Buffer<Totals> totals_;
  std::vector<Block> ranges_host_;
  std::vector<Sample> output_host_;
  View view_;
  uint32_t first_candidate_=0;
  bool failed_=false;

  static void poll(const std::function<bool()> &cancelled){
    if(cancelled&&cancelled())throw std::runtime_error("Mesh baking cancelled");
  }

public:
  DeviceRaster(std::span<const float> axis,std::span<const uint8_t> writable)
      :axis_(api_,axis.size()),writable_(api_,writable.size()),owners_(api_,axis.size()*axis.size()),
       spans_(api_,span_capacity),candidates_(api_,candidate_capacity),output_(api_,candidate_capacity),
       ranges_(api_,block_capacity),totals_(api_,1),ranges_host_(block_capacity),output_host_(candidate_capacity),
       view_{axis_.data,writable_.data,owners_.data,uint32_t(axis.size())} {
    try {
      api_.upload(axis_.data,axis.data(),axis.size_bytes());
      if(!writable.empty())api_.upload(writable_.data,writable.data(),writable.size_bytes());
      api_.clear_owners(owners_.data,axis.size()*axis.size());
      api_.wait();
    }catch(...){api_.drain();throw;}
  }

  Batch rasterize(std::span<const Span> spans,uint32_t count,const std::function<bool()> &cancelled) override {
    if(failed_||spans.empty()||spans.size()>span_capacity||!count||count>candidate_capacity||
       first_candidate_>UINT32_MAX-count)
      throw std::logic_error("Invalid Bake raster device batch");
    poll(cancelled);
    const uint32_t blocks=(count+candidates_per_block-1)/candidates_per_block;
    Totals totals;
    try {
      api_.upload(spans_.data,spans.data(),spans.size_bytes());
      api_.raster(view_,spans_.data,uint32_t(spans.size()),count,first_candidate_,candidates_.data);
      api_.clear_totals(totals_.data);
      api_.compact(view_,candidates_.data,count,first_candidate_,blocks,ranges_.data,output_.data,totals_.data);
      api_.read(&totals,totals_.data,sizeof(totals));
      poll(cancelled);
      if(totals.samples>count||totals.overlaps>count-totals.samples)
        throw std::runtime_error("Invalid Bake raster device results");
      if(totals.samples){
        api_.read(ranges_host_.data(),ranges_.data,blocks*sizeof(Block));
        api_.read(output_host_.data(),output_.data,totals.samples*sizeof(Sample));
        poll(cancelled);
      }
    }catch(...){failed_=true;api_.drain();throw;}
    first_candidate_+=count;
    return {std::span<const Block>(ranges_host_).first(totals.samples?blocks:0),
            std::span<const Sample>(output_host_).first(totals.samples),totals.overlaps};
  }
};

template<class Api>
std::unique_ptr<Raster> create_device(std::span<const float> axis,std::span<const uint8_t> writable){
  if(axis.size()<8||axis.size()>4096||(!writable.empty()&&writable.size()!=axis.size()*axis.size()))
    throw std::logic_error("Invalid Bake raster device input");
  return std::make_unique<DeviceRaster<Api>>(axis,writable);
}
} // namespace dome::detail::bake_raster
