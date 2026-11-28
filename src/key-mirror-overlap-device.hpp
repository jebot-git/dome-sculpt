// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "key-mirror-overlap-kernel.hpp"
#include "weight-transfer-device-query.hpp"

namespace dome::detail::mirror_overlap {
template<class Api> class DeviceQuery final:public Query {
  template<class T>using Buffer=weight_transfer::DeviceBuffer<Api,T>;
  Api api_;
  uint32_t triangles_,capacity_;
  Buffer<Box> boxes_;
  Buffer<Node> nodes_;
  Buffer<uint32_t> order_;
  Buffer<Progress> progress_;
  Buffer<Pair> pairs_;
  Buffer<Status> status_;
  View view_;
  std::vector<Pair> results_;

public:
  DeviceQuery(std::span<const Box> boxes,std::span<const Node> nodes,std::span<const uint32_t> order):
      triangles_(uint32_t(boxes.size())),capacity_(uint32_t(std::min(boxes.size(),batch_capacity))),
      boxes_(api_,boxes.size()),nodes_(api_,nodes.size()),order_(api_,order.size()),
      progress_(api_,capacity_),pairs_(api_,capacity_*visit_quantum),status_(api_,1),
      view_{boxes_.data,nodes_.data,order_.data,uint32_t(nodes.size())},results_(capacity_*visit_quantum) {
    try{
      api_.upload(boxes_.data,boxes.data(),boxes.size_bytes());
      api_.upload(nodes_.data,nodes.data(),nodes.size_bytes());
      api_.upload(order_.data,order.data(),order.size_bytes());
      api_.wait();
    }catch(...){api_.drain();throw;}
  }

  uint32_t search(uint32_t first,uint32_t count,Metric metric,uint32_t remaining,
                  const Append &append,const std::function<bool()> &cancelled) override {
    if(!count||count>capacity_||first>triangles_||count>triangles_-first)
      throw std::logic_error("Invalid Mirror refinement query batch");
    Status result;
    uint32_t used=0;
    bool initialize=true;
    try{
      do{
        if(cancelled&&cancelled())throw std::runtime_error("Mirror refinement cancelled");
        const auto allowance=std::clamp((remaining-used)/count,1u,visit_quantum);
        api_.mirror_overlaps(view_,first,count,metric,allowance,progress_.data,pairs_.data,status_.data,initialize);
        api_.read(&result,status_.data,sizeof(result));
        const auto visits=result.visits&~pending;
        if(visits>remaining-used)throw std::length_error("Mirror refinement exceeds bounded work");
        if(result.hits>count*allowance)throw std::logic_error("Invalid Mirror refinement candidate count");
        used+=visits;
        if(result.hits){
          const auto selected=std::span(results_).first(result.hits);
          api_.read(selected.data(),pairs_.data,selected.size_bytes());
          append(selected);
        }
        initialize=false;
      }while(result.visits&pending);
    }catch(...){api_.drain();throw;}
    return used;
  }
};
}
