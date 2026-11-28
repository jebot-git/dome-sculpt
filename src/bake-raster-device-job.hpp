// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake-device-buffer.hpp"
#include "bake-raster-kernel.hpp"
#include "bake-ray-device-query.hpp"
#include <stdexcept>
#include <vector>

namespace dome::detail::bake_raster {
template<class Api> class DeviceRaster final:public Raster {
  template<class T> using Buffer=BakeDeviceBuffer<Api,T>;
  static constexpr size_t block_capacity=candidate_capacity/candidates_per_block;
  bake_rays::DeviceQuery<Api> *projection_;
  ProjectionSettings projection_settings_;
  std::unique_ptr<Api> owned_api_;
  Api &api_;
  Buffer<float> axis_;
  Buffer<uint8_t> writable_;
  Buffer<uint32_t> owners_;
  Buffer<Span> spans_;
  Buffer<Sample> candidates_,output_;
  Buffer<Block> ranges_;
  Buffer<Totals> totals_;
  Buffer<ProjectionFace> faces_;
  Buffer<bake_rays::ProjectionHit> hits_;
  std::vector<Block> ranges_host_;
  std::vector<Sample> output_host_;
  std::vector<bake_rays::ProjectionHit> hits_host_;
  View view_;
  uint32_t first_candidate_=0;
  bool failed_=false;

  static void poll(const std::function<bool()> &cancelled){
    if(cancelled&&cancelled())throw std::runtime_error("Mesh baking cancelled");
  }

public:
  DeviceRaster(std::span<const float> axis,std::span<const uint8_t> writable,
               bake_rays::DeviceQuery<Api> *projection=nullptr,ProjectionSettings settings={})
      :projection_(projection),projection_settings_(settings),owned_api_(projection?nullptr:std::make_unique<Api>()),
       api_(projection?projection->api_:*owned_api_),axis_(api_,axis.size()),writable_(api_,writable.size()),
       owners_(api_,axis.size()*axis.size()),
       spans_(api_,span_capacity),candidates_(api_,candidate_capacity),output_(api_,candidate_capacity),
       ranges_(api_,block_capacity),totals_(api_,1),faces_(api_,projection?span_capacity:0),
       hits_(api_,projection?candidate_capacity:0),ranges_host_(block_capacity),output_host_(candidate_capacity),
       hits_host_(projection?candidate_capacity:0),
       view_{axis_.data,writable_.data,owners_.data,uint32_t(axis.size())} {
    try {
      api_.upload(axis_.data,axis.data(),axis.size_bytes());
      if(!writable.empty())api_.upload(writable_.data,writable.data(),writable.size_bytes());
      api_.clear_owners(owners_.data,axis.size()*axis.size());
      api_.wait();
    }catch(...){api_.drain();throw;}
  }

  Batch rasterize(std::span<const Span> spans,uint32_t count,const std::function<bool()> &cancelled) override {
    return run(spans,{},count,0,0,cancelled);
  }

  Batch rasterize_and_project(std::span<const Span> spans,std::span<const ProjectionFace> faces,
      uint32_t count,uint64_t ray_budget,uint32_t triangle_budget,const std::function<bool()> &cancelled) override {
    if(!projection_||faces.empty()||faces.size()!=spans.size()||!triangle_budget)
      throw std::logic_error("Invalid Bake raster projection batch");
    return run(spans,faces,count,ray_budget,triangle_budget,cancelled);
  }

private:
  Batch run(std::span<const Span> spans,std::span<const ProjectionFace> faces,uint32_t count,
            uint64_t ray_budget,uint32_t triangle_budget,const std::function<bool()> &cancelled){
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
      if(!faces.empty()&&totals.samples){
        if(totals.samples>ray_budget)throw std::length_error("Bake ray budget exceeded");
        api_.upload(faces_.data,faces.data(),faces.size_bytes());
        for(size_t first=0;first<totals.samples;){
          const auto size=std::min(size_t(totals.samples)-first,bake_rays::projection_batch_capacity);
          api_.reset(projection_->remaining_.data);
          api_.prepare_cage_rays(faces_.data,projection_settings_,output_.data+first,size,
                                 projection_->rays_.data,projection_->remaining_.data);
          projection_->trace_resident(size,hits_.data+first,triangle_budget,cancelled,true);
          first+=size;
        }
        api_.read(hits_host_.data(),hits_.data,totals.samples*sizeof(bake_rays::ProjectionHit));
      }
      if(totals.samples){
        api_.read(ranges_host_.data(),ranges_.data,blocks*sizeof(Block));
        api_.read(output_host_.data(),output_.data,totals.samples*sizeof(Sample));
        poll(cancelled);
      }
    }catch(...){failed_=true;api_.drain();throw;}
    first_candidate_+=count;
    return {std::span<const Block>(ranges_host_).first(totals.samples?blocks:0),
            std::span<const Sample>(output_host_).first(totals.samples),
            std::span<const bake_rays::ProjectionHit>(hits_host_).first(faces.empty()?0:totals.samples),totals.overlaps};
  }
};

inline void validate_axes(std::span<const float> axis,std::span<const uint8_t> writable){
  if(axis.size()<8||axis.size()>4096||(!writable.empty()&&writable.size()!=axis.size()*axis.size()))
    throw std::logic_error("Invalid Bake raster device input");
}

template<class Api>
std::unique_ptr<Raster> create_device(std::span<const float> axis,std::span<const uint8_t> writable){
  validate_axes(axis,writable);
  return std::make_unique<DeviceRaster<Api>>(axis,writable);
}
} // namespace dome::detail::bake_raster

namespace dome::detail::bake_rays {
template<class Api>
std::unique_ptr<bake_raster::Raster> DeviceQuery<Api>::create_projection_target(std::span<const float> axis,
    std::span<const uint8_t> writable,const bake_raster::ProjectionSettings &settings){
  bake_raster::validate_axes(axis,writable);
  if(!rays_.data)throw std::logic_error("Bake projection buffers are unavailable");
  return std::make_unique<bake_raster::DeviceRaster<Api>>(axis,writable,this,settings);
}
} // namespace dome::detail::bake_rays
