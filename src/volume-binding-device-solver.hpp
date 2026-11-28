// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "volume-binding-device-kernel.hpp"
#include <algorithm>
#include <stdexcept>

namespace dome::detail::volume_device {
template<class Api>
class DeviceSolver final:public Solver {
  template<class T> struct Buffer {
    Api &api;
    T *data=nullptr;
    Buffer(Api &context,size_t count):api(context) {
      if(count)data=static_cast<T*>(api.allocate(count*sizeof(T)));
    }
    ~Buffer(){if(data)api.release(data);}
    Buffer(const Buffer &)=delete;
  };
  Api api_;
  size_t count_,interior_count_,maximum_seeds_;
  Buffer<InteriorNode> interior_;
  Buffer<int32_t> grid_;
  Buffer<uint32_t> starts_;
  Buffer<Edge> edges_;
  Buffer<Seed> seeds_;
  Buffer<float> a_,b_;
  Buffer<uint32_t> changed_;
  View graph_;

  template<class T> void upload_to(Buffer<T> &to,std::span<const T> data) {
    if(!data.empty())api_.upload(to.data,data.data(),data.size_bytes());
  }
  void checkpoint(const std::function<bool()> &cancelled) const {
    if(cancelled&&cancelled())throw std::runtime_error("Volume binding cancelled");
  }
public:
  DeviceSolver(const Graph &graph,size_t maximum_seeds):
    count_(graph.starts.size()-1),interior_count_(graph.interior.size()),
    maximum_seeds_(maximum_seeds),interior_(api_,interior_count_),
    grid_(api_,graph.grid_to_node.size()),starts_(api_,graph.starts.size()),
    edges_(api_,graph.edges.size()),seeds_(api_,maximum_seeds_),
    a_(api_,count_),b_(api_,count_),changed_(api_,1),
    graph_{interior_.data,grid_.data,starts_.data,edges_.data,graph.grid,interior_count_} {
    try {
      upload_to(interior_,graph.interior);
      upload_to(grid_,graph.grid_to_node);
      upload_to(starts_,graph.starts);
      upload_to(edges_,graph.edges);
      api_.wait();
    }catch(...){api_.drain();throw;}
  }

  void solve(std::span<const Seed> seeds,std::span<float> boundary,
             Propagation &budget,const std::function<bool()> &cancelled) override {
    if(seeds.empty()||seeds.size()>maximum_seeds_||boundary.size()!=count_-interior_count_)
      throw std::logic_error("Invalid volume binding device field");
    checkpoint(cancelled);
    try {
      auto *current=a_.data,*next=b_.data;
      api_.initialize(current,count_);
      upload_to(seeds_,seeds);
      api_.scatter(current,seeds_.data,seeds.size());
      const size_t passes_per_batch=std::clamp(size_t(2'000'000)/count_,size_t(1),size_t(8));
      for(;;){
        const size_t passes=std::min(passes_per_batch,budget.remaining_nodes/count_);
        if(!passes)throw std::length_error("Volume binding GPU propagation budget exceeded");
        budget.remaining_nodes-=passes*count_;
        budget.passes+=passes;
        uint32_t changed=0;
        api_.reset(changed_.data);
        for(size_t pass=0;pass<passes;++pass){
          api_.step(graph_,current,next,count_,pass+1==passes?changed_.data:nullptr);
          std::swap(current,next);
        }
        try{api_.read(&changed,changed_.data,sizeof(changed));}
        catch(...){api_.drain();throw;}
        checkpoint(cancelled);
        if(changed)continue;
        api_.read(boundary.data(),current+interior_count_,boundary.size_bytes());
        checkpoint(cancelled);
        return;
      }
    }catch(...){api_.drain();throw;}
  }
};
} // namespace dome::detail::volume_device
