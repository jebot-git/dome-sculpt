// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "key-mirror-kernel.hpp"
#include "weight-transfer-device-query.hpp"

namespace dome::detail::key_mirror {
template<class Api,class Data,class State,class Match>
class DeviceQuery final:public Query<Match> {
  template<class T>using Buffer=weight_transfer::DeviceBuffer<Api,T>;
  Api api_;
  size_t targets_,capacity_;
  Buffer<Bin> bins_;
  Buffer<uint32_t> order_;
  Buffer<Vec3> rest_;
  Buffer<Data> data_;
  Buffer<Cell> cells_;
  Buffer<State> progress_;
  Buffer<Match> matches_;
  Buffer<uint32_t> status_;
  View view_;
public:
  DeviceQuery(std::span<const Bin> bins,std::span<const uint32_t> order,
              std::span<const Vec3> rest,std::span<const Data> data,size_t capacity):
      targets_(order.size()),capacity_(capacity),bins_(api_,bins.size()),order_(api_,order.size()),
      rest_(api_,rest.size()),data_(api_,data.size()),cells_(api_,capacity),progress_(api_,capacity),
      matches_(api_,capacity),status_(api_,1),view_{bins_.data,order_.data,uint32_t(bins.size())} {
    try{
      api_.upload(bins_.data,bins.data(),bins.size_bytes());
      api_.upload(order_.data,order.data(),order.size_bytes());
      api_.upload(rest_.data,rest.data(),rest.size_bytes());
      api_.upload(data_.data,data.data(),data.size_bytes());
      api_.wait();
    }catch(...){api_.drain();throw;}
  }

  uint32_t search(size_t first,std::span<const Cell> cells,Metric metric,uint32_t work,
      std::span<Match> matches,const std::function<bool()> &cancelled) override {
    if(cells.empty()||cells.size()>capacity_||matches.size()!=cells.size()||
       first>targets_||cells.size()>targets_-first)
      throw std::logic_error("Invalid shapekey Mirror device batch");
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
        api_.mirror_keys(view_,rest_.data,data_.data,first,cells_.data,metric,allowance,
                         progress_.data,matches_.data,cells.size(),status_.data,initialize);
        api_.read(&status,status_.data,sizeof(status));
        const auto spent=status&~pending;
        if(spent>work-visits)throw std::length_error("Key mirror correspondence exceeds bounded work");
        visits+=spent;
        initialize=false;
      }while(status&pending);
      stop();
      api_.read(matches.data(),matches_.data,matches.size_bytes());
      stop();
    }catch(...){api_.drain();throw;}
    return visits;
  }
};

template<class Api> using VertexQuery=DeviceQuery<Api,Vec3,Progress,uint32_t>;
template<class Api> using TriangleQuery=DeviceQuery<Api,uint32_t,TriangleProgress,TriangleMatch>;
}
