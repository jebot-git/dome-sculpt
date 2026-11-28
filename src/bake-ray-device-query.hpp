// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake-device-buffer.hpp"
#include "bake-ray-kernel.hpp"

namespace dome::detail::bake_rays {
template<class Api>
class DeviceQuery final:public Query {
  using Progress=ProjectionProgress<typename Api::Precision::Scalar>;
  template<class T> using Buffer=BakeDeviceBuffer<Api,T>;
  Api api_;
  size_t samples_,capacity_;
  float maximum_;
  Buffer<Node> nodes_;
  Buffer<uint32_t> order_,indices_,states_,remaining_;
  Buffer<Vec3> vertices_,pattern_;
  Buffer<Frame> frames_;
  Buffer<uint16_t> counts_;
  Buffer<ProjectionRay> rays_;
  Buffer<ProjectionHit> hits_;
  Buffer<Progress> progress_;
  SurfaceView view_;
  static void poll(const std::function<bool()> &cancelled){
    if(cancelled&&cancelled())throw std::runtime_error("Mesh baking cancelled");
  }
public:
  DeviceQuery(const Surface &surface,std::span<const Vec3> pattern,float maximum,bool projection)
      :samples_(pattern.size()),capacity_(samples_?std::min(max_texels_per_batch,max_rays_per_batch/samples_):0),
       maximum_(maximum),nodes_(api_,surface.nodes.size()),order_(api_,surface.order.size()),
       indices_(api_,surface.indices.size()),states_(api_,capacity_*samples_),remaining_(api_,1),
       vertices_(api_,surface.vertices.size()),pattern_(api_,samples_),frames_(api_,capacity_),counts_(api_,capacity_),
       rays_(api_,projection?projection_batch_capacity:0),hits_(api_,projection?projection_batch_capacity:0),
       progress_(api_,projection?projection_batch_capacity:0),
       view_{nodes_.data,order_.data,indices_.data,vertices_.data,uint32_t(surface.nodes.size())} {
    try {
      api_.upload(nodes_.data,surface.nodes.data(),surface.nodes.size_bytes());
      api_.upload(order_.data,surface.order.data(),surface.order.size_bytes());
      api_.upload(indices_.data,surface.indices.data(),surface.indices.size_bytes());
      api_.upload(vertices_.data,surface.vertices.data(),surface.vertices.size_bytes());
      if(samples_)api_.upload(pattern_.data,pattern.data(),pattern.size_bytes());
      api_.wait();
    }catch(...){api_.drain();throw;}
  }

  void count_blocked(std::span<const Frame> frames,std::span<uint16_t> results,
                     const std::function<bool()> &cancelled) override {
    if(frames.empty()||frames.size()>capacity_||frames.size()!=results.size())
      throw std::logic_error("Invalid ambient occlusion device batch");
    poll(cancelled);
    try {
      api_.upload(frames_.data,frames.data(),frames.size_bytes());
      bool first=true;
      uint32_t remaining;
      do {
        api_.reset(remaining_.data);
        api_.trace(view_,frames_.data,pattern_.data,samples_,maximum_,states_.data,
                   frames.size()*samples_,remaining_.data,first);
        api_.read(&remaining,remaining_.data,sizeof(remaining));
        poll(cancelled);
        first=false;
      }while(remaining);
      api_.reduce(states_.data,samples_,counts_.data,frames.size());
      api_.read(results.data(),counts_.data,results.size_bytes());
      poll(cancelled);
    }catch(...){api_.drain();throw;}
  }

  void project(std::span<const ProjectionRay> rays,std::span<ProjectionHit> results,
               uint32_t budget,const std::function<bool()> &cancelled) override {
    if(!rays_.data||rays.empty()||rays.size()>projection_batch_capacity||rays.size()!=results.size()||!budget)
      throw std::logic_error("Invalid Bake projection device batch");
    poll(cancelled);
    auto run_pass=[&](bool boundary){
      bool first=true;
      uint32_t status,summary=0;
      do {
        api_.reset(remaining_.data);
        api_.project(view_,rays_.data,hits_.data,progress_.data,rays.size(),budget,remaining_.data,first,boundary);
        api_.read(&status,remaining_.data,sizeof(status));
        poll(cancelled);
        if(status&triangle_limit_exceeded)throw std::length_error("Surface ray triangle budget exceeded");
        summary|=status;
        first=false;
      }while(status&more_nodes);
      return summary;
    };
    try {
      api_.upload(rays_.data,rays.data(),rays.size_bytes());
      if(run_pass(false)&needs_boundary)run_pass(true);
      api_.read(results.data(),hits_.data,results.size_bytes());
      poll(cancelled);
    }catch(...){api_.drain();throw;}
  }
};

template<class Api>
std::unique_ptr<Query> upload_device(const Surface &surface,std::span<const Vec3> pattern,float maximum,bool projection) {
  if(surface.nodes.empty()||surface.order.empty()||surface.indices.empty()||surface.vertices.empty()||
     (pattern.empty()&&!projection)||pattern.size()>256||(!pattern.empty()&&(!std::isfinite(maximum)||maximum<=0)))
    throw std::logic_error("Invalid Bake ray device input");
  return std::make_unique<DeviceQuery<Api>>(surface,pattern,maximum,projection);
}
} // namespace dome::detail::bake_rays
