// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "key-mirror-kernel.hpp"
#include "weight-transfer-device-query.hpp"

namespace dome::detail::key_mirror {
template<class Api>
class DeviceQuery final:public Query {
  template<class T>using Buffer=weight_transfer::DeviceBuffer<Api,T>;
  Api api_;
  size_t vertices_,capacity_;
  Buffer<Bin> bins_;
  Buffer<uint32_t> order_;
  Buffer<Vec3> rest_,delta_;
  Buffer<Cell> cells_;
  Buffer<Progress> progress_;
  Buffer<uint32_t> donors_,status_;
  View view_;
public:
  DeviceQuery(std::span<const Bin> bins,std::span<const uint32_t> order,
              std::span<const Vec3> rest,std::span<const Vec3> delta):
      vertices_(rest.size()),capacity_(std::min(vertices_,batch_capacity)),
      bins_(api_,bins.size()),order_(api_,order.size()),rest_(api_,vertices_),delta_(api_,vertices_),
      cells_(api_,capacity_),progress_(api_,capacity_),donors_(api_,capacity_),status_(api_,1),
      view_{bins_.data,order_.data,rest_.data,delta_.data,uint32_t(bins.size())} {
    try{
      api_.upload(bins_.data,bins.data(),bins.size_bytes());
      api_.upload(order_.data,order.data(),order.size_bytes());
      api_.upload(rest_.data,rest.data(),rest.size_bytes());
      api_.upload(delta_.data,delta.data(),delta.size_bytes());
      api_.wait();
    }catch(...){api_.drain();throw;}
  }

  uint32_t search(size_t first,std::span<const Cell> cells,Metric metric,uint32_t work,
      std::span<uint32_t> donors,const std::function<bool()> &cancelled) override {
    if(cells.empty()||cells.size()>capacity_||donors.size()!=cells.size()||
       first>vertices_||cells.size()>vertices_-first)
      throw std::logic_error("Invalid vertex-position Mirror device batch");
    const auto stop=[&]{
      if(cancelled&&cancelled())throw std::runtime_error("Key mirror correspondence cancelled");
    };
    stop();
    uint32_t visits=0,status=0;
    bool initialize=true;
    try{
      api_.upload(cells_.data,cells.data(),cells.size_bytes());
      do{
        stop();
        const auto allowance=std::clamp((work-visits)/uint32_t(cells.size()),1u,visits_per_launch);
        api_.reset(status_.data);
        api_.mirror_vertices(view_,first,cells_.data,metric,allowance,progress_.data,donors_.data,
                             cells.size(),status_.data,initialize);
        api_.read(&status,status_.data,sizeof(status));
        const auto spent=status&~pending;
        if(spent>work-visits)throw std::length_error("Key mirror correspondence exceeds bounded work");
        visits+=spent;
        initialize=false;
      }while(status&pending);
      stop();
      api_.read(donors.data(),donors_.data,donors.size_bytes());
      stop();
    }catch(...){api_.drain();throw;}
    return visits;
  }
};
}
