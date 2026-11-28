// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "weight-transfer-kernel.hpp"
#include "retopo-surface-kernel.hpp"
#include <stdexcept>

namespace dome::detail::weight_transfer {
template<class Api>
class DeviceSurface {
protected:
  template<class T> struct Buffer {
    Api &api;
    T *data;
    Buffer(Api &context,size_t size):api(context),data(static_cast<T*>(api.allocate(size*sizeof(T)))){}
    ~Buffer(){api.release(data);}
    Buffer(const Buffer &)=delete;
  };
  Api api_;
  Buffer<Node> nodes_;
  Buffer<uint32_t> order_,indices_;
  Buffer<Vec3> vertices_;
  SurfaceView view_;
  explicit DeviceSurface(const Surface &surface):nodes_(api_,surface.nodes.size()),
    order_(api_,surface.order.size()),indices_(api_,surface.indices.size()),
    vertices_(api_,surface.vertices.size()),
    view_{nodes_.data,order_.data,indices_.data,vertices_.data} {
    try{
      api_.upload(nodes_.data,surface.nodes.data(),surface.nodes.size_bytes());
      api_.upload(order_.data,surface.order.data(),surface.order.size_bytes());
      api_.upload(indices_.data,surface.indices.data(),surface.indices.size_bytes());
      api_.upload(vertices_.data,surface.vertices.data(),surface.vertices.size_bytes());
      api_.wait();
    }catch(...){api_.drain();throw;}
  }
};

template<class Api>
class DeviceQuery final:public Query,private DeviceSurface<Api> {
  template<class T> using Buffer=typename DeviceSurface<Api>::template Buffer<T>;
  using DeviceSurface<Api>::api_;
  Buffer<Vec3> points_;
  Buffer<Hit> hits_;
  Buffer<uint32_t> visited_;
  size_t maximum_per_point_;

public:
  explicit DeviceQuery(const Surface &surface):DeviceSurface<Api>(surface),points_(api_,batch_capacity),
    hits_(api_,batch_capacity),visited_(api_,1),
    maximum_per_point_(surface.nodes.size()+surface.order.size()){}

  uint32_t search(std::span<const Vec3> points,float radius,uint32_t work,std::span<Hit> hits) override {
    if(points.empty()||points.size()>batch_capacity||points.size()!=hits.size()||!work)
      throw std::logic_error("Invalid weight transfer device batch");
    uint32_t visits=0;
    try{
      api_.upload(points_.data,points.data(),points.size_bytes());
      api_.reset(visited_.data);
      const bool all_queries_fit=points.size()<=work/maximum_per_point_;
      api_.launch(this->view_,points_.data,radius,hits_.data,points.size(),visited_.data,work,all_queries_fit);
      api_.read(&visits,visited_.data,sizeof(visits));
      if(visits<=work)api_.read(hits.data(),hits_.data,hits.size_bytes());
    }catch(...){api_.drain();throw;}
    return visits;
  }
};
} // namespace dome::detail::weight_transfer

namespace dome::detail::retopo_surface {
template<class Api>
class DeviceQuery final:public Query,private weight_transfer::DeviceSurface<Api> {
  template<class T> using Buffer=typename weight_transfer::DeviceSurface<Api>::template Buffer<T>;
  using weight_transfer::DeviceSurface<Api>::api_;
  Buffer<Vec3> points_;
  Buffer<Progress> progress_;
  Buffer<Hit> hits_;
  Buffer<uint32_t> status_;
public:
  explicit DeviceQuery(const weight_transfer::Surface &surface):
      weight_transfer::DeviceSurface<Api>(surface),points_(api_,batch_capacity),
      progress_(api_,batch_capacity),hits_(api_,batch_capacity),status_(api_,1){}

  void search(std::span<const Vec3> points,uint32_t budget,std::span<Hit> hits,
              const std::function<bool()> &cancelled) override {
    if(points.empty()||points.size()>batch_capacity||points.size()!=hits.size()||!budget)
      throw std::logic_error("Invalid Retopology projection batch");
    auto check=[&]{if(cancelled&&cancelled())throw std::runtime_error("Retopology projection cancelled");};
    check();
    try{
      api_.upload(points_.data,points.data(),points.size_bytes());
      bool first=true;
      uint32_t status=0;
      do{
        check();
        api_.reset(status_.data);
        api_.nearest(this->view_,points_.data,hits_.data,progress_.data,points.size(),budget,status_.data,first);
        api_.read(&status,status_.data,sizeof(status));
        if(status&invalid_triangle)throw std::runtime_error("Degenerate closest triangle");
        if(status&over_budget)throw std::length_error("Closest surface triangle work budget exceeded");
        first=false;
      }while(status&pending_nodes);
      check();
      api_.read(hits.data(),hits_.data,hits.size_bytes());
      check();
    }catch(...){api_.drain();throw;}
  }
};
}
