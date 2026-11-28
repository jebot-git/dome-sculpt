// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "weight-transfer-kernel.hpp"
#include "retopo-surface-kernel.hpp"
#include "weight-brush-kernel.hpp"
#include <algorithm>
#include <stdexcept>

namespace dome::detail::weight_transfer {
template<class Api,class T>
struct DeviceBuffer {
  Api &api;
  T *data;
  DeviceBuffer(Api &context,size_t size):api(context),data(size?static_cast<T*>(api.allocate(size*sizeof(T))):nullptr){}
  ~DeviceBuffer(){if(data)api.release(data);}
  DeviceBuffer(const DeviceBuffer &)=delete;
};

template<class Api>
class DeviceSurface {
protected:
  template<class T> using Buffer=DeviceBuffer<Api,T>;
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

namespace dome::detail::weight_brush {
template<class Api>
class DeviceQuery final:public Query {
  template<class T>using Buffer=weight_transfer::DeviceBuffer<Api,T>;
  static constexpr size_t max_capsules=4096;
  Api api_;
  size_t capacity_;
  Buffer<Capsule> direct_,reflected_;
  Buffer<Vec3> points_{api_,batch_capacity};
  Buffer<Hit> hits_{api_,batch_capacity};
  Buffer<uint32_t> count_{api_,1};
  uint32_t capsules_=0;
  bool mirror_=false;

public:
  DeviceQuery(std::span<const Capsule> direct,std::span<const Capsule> reflected,bool reusable):
      capacity_(reusable?max_capsules:std::min(direct.size(),max_capsules)),
      direct_(api_,capacity_),reflected_(api_,reflected.empty()?0:capacity_) {
    set_capsules(direct,reflected);
  }

  void set_capsules(std::span<const Capsule> direct,std::span<const Capsule> reflected) override {
    if(direct.empty()||direct.size()>capacity_||
       (!reflected.empty()&&(!reflected_.data||reflected.size()!=direct.size())))
      throw std::logic_error("Invalid weight brush device stroke");
    capsules_=0;
    try{
      api_.upload(direct_.data,direct.data(),direct.size_bytes());
      if(!reflected.empty())api_.upload(reflected_.data,reflected.data(),reflected.size_bytes());
      api_.wait();
    }catch(...){api_.drain();throw;}
    capsules_=uint32_t(direct.size());
    mirror_=!reflected.empty();
  }

  size_t search(std::span<const Vec3> points,unsigned sides,std::span<Hit> hits) override {
    const auto tests=capsules_*(sides==(Direct|Reflected)?2u:1u);
    if(!tests||points.empty()||points.size()>batch_capacity||points.size()!=hits.size()||
       points.size()>batch_capsules/tests||!sides||sides>(Direct|Reflected)||
       ((sides&Reflected)&&!mirror_))
      throw std::logic_error("Invalid weight brush device batch");
    uint32_t count=0;
    try{
      api_.upload(points_.data,points.data(),points.size_bytes());
      api_.reset(count_.data);
      api_.brush({direct_.data,reflected_.data,capsules_,sides},points_.data,points.size(),hits_.data,count_.data);
      api_.read(&count,count_.data,sizeof(count));
      if(count>points.size())throw std::logic_error("Weight brush device returned an invalid hit count");
      if(count)api_.read(hits.data(),hits_.data,size_t(count)*sizeof(Hit));
    }catch(...){api_.drain();throw;}
    std::ranges::sort(hits.first(count),{},&Hit::vertex);
    return count;
  }
};
}

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

template<class Api>
class DeviceTransferQuery final:public TransferQuery,private weight_transfer::DeviceSurface<Api> {
  template<class T> using Buffer=typename weight_transfer::DeviceSurface<Api>::template Buffer<T>;
  using weight_transfer::DeviceSurface<Api>::api_;
  Buffer<Vec3> normals_;
  Buffer<uint8_t> membership_;
  Buffer<TransferPoint> points_;
  Buffer<Progress> progress_;
  Buffer<TransferHit> hits_;
  Buffer<TransferStatus> status_;
  size_t faces_;
  TransferMetric metric_;
public:
  DeviceTransferQuery(const weight_transfer::Surface &surface,std::span<const Vec3> normals,TransferMetric metric):
      weight_transfer::DeviceSurface<Api>(surface),normals_(api_,normals.size()),
      membership_(api_,normals.size()),points_(api_,transfer_capacity),
      progress_(api_,transfer_capacity),hits_(api_,transfer_capacity),status_(api_,1),
      faces_(normals.size()),metric_(metric){
    try{
      api_.upload(normals_.data,normals.data(),normals.size_bytes());
      api_.wait();
    }catch(...){api_.drain();throw;}
  }
  void material(std::span<const uint8_t> membership) override {
    if(membership.size()!=faces_)throw std::logic_error("Invalid Retopology material membership");
    try{
      api_.upload(membership_.data,membership.data(),membership.size_bytes());
      api_.wait();
    }catch(...){api_.drain();throw;}
  }
  uint32_t search(std::span<const TransferPoint> points,std::span<TransferHit> hits,
                  uint32_t work,const std::function<bool()> &cancelled) override {
    if(points.empty()||points.size()>transfer_capacity||points.size()!=hits.size())
      throw std::logic_error("Invalid Retopology material query batch");
    auto check=[&]{if(cancelled&&cancelled())throw std::runtime_error("Retopology transfer cancelled");};
    check();
    uint32_t visits=0;
    try{
      api_.upload(points_.data,points.data(),points.size_bytes());
      bool first=true;
      TransferStatus status{};
      do{
        check();
        api_.reset_transfer(status_.data);
        api_.transfer(this->view_,normals_.data,membership_.data,metric_,points_.data,
                      hits_.data,progress_.data,points.size(),status_.data,first);
        api_.read(&status,status_.data,sizeof(status));
        if(status.flags&invalid_triangle)throw std::runtime_error("Retopology lost a valid closest triangle metric");
        if((status.flags&over_budget)||status.visits>work-visits)
          throw std::length_error("Retopology transfer exceeds the surface work budget");
        visits+=status.visits;
        first=false;
      }while(status.flags&pending_nodes);
      check();
      api_.read(hits.data(),hits_.data,hits.size_bytes());
      check();
    }catch(...){api_.drain();throw;}
    return visits;
  }
};
}
