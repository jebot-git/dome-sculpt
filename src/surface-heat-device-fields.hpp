// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "surface-heat-device.hpp"
#include "surface-heat-device-kernel.hpp"
#include <bit>
#include <stdexcept>
#include <utility>

namespace dome::detail::surface_heat_device {
template<class Api>
class DeviceFields final:public Fields {
  template<class T> struct Buffer {
    Api &api;
    T *data;
    Buffer(Api &context,size_t count):api(context),
      data(count?static_cast<T*>(api.allocate(count*sizeof(T))):nullptr) {}
    ~Buffer(){if(data)api.release(data);}
    Buffer(const Buffer &)=delete;
  };
  struct SparseBuffer {
    Buffer<uint32_t> starts,columns;
    Buffer<Scalar> coefficients,diagonal;
    SparseBuffer(Api &api,const SparseRows &input,bool shared_diagonal):
      starts(api,input.starts.size()),columns(api,input.columns.size()),
      coefficients(api,input.coefficients.size()),diagonal(api,shared_diagonal?0:input.diagonal.size()) {}
    void upload(Api &api,const SparseRows &input) const {
      api.upload(starts.data,input.starts.data(),input.starts.size_bytes());
      if(!input.columns.empty()){
        api.upload(columns.data,input.columns.data(),input.columns.size_bytes());
        api.upload(coefficients.data,input.coefficients.data(),input.coefficients.size_bytes());
      }
      if(diagonal.data)api.upload(diagonal.data,input.diagonal.data(),input.diagonal.size_bytes());
    }
    SparseView view(const Scalar *shared=nullptr) const {
      return {starts.data,columns.data,coefficients.data,shared?shared:diagonal.data};
    }
  };
  Api api_;
  size_t count_;
  static constexpr size_t vectors=static_cast<size_t>(Vector::Count);
  SparseBuffer matrix_,lower_,upper_;
  Buffer<Scalar> vectors_,partial_a_,partial_b_;
  static size_t chunks(size_t count){return (count+reduction_chunk-1)/reduction_chunk;}
  Scalar *address(Vector v) const {return vectors_.data+static_cast<size_t>(v)*count_;}
public:
  explicit DeviceFields(const System &input):count_(input.matrix.diagonal.size()),
    matrix_(api_,input.matrix,false),lower_(api_,input.lower,false),upper_(api_,input.upper,true),
    vectors_(api_,vectors*count_),partial_a_(api_,chunks(count_)),partial_b_(api_,chunks(count_)) {
    try {
      matrix_.upload(api_,input.matrix);
      lower_.upload(api_,input.lower);
      upper_.upload(api_,input.upper);
      api_.wait();
    }catch(...){api_.drain();throw;}
  }
  ~DeviceFields() override {api_.drain();}
  void write(Vector to,std::span<const double> weights) override {
    if(weights.size()!=count_)throw std::logic_error("Surface Heat device input dimensions");
    try{api_.upload(address(to),weights.data(),weights.size_bytes());api_.wait();}
    catch(...){api_.drain();throw;}
  }
  void read(Vector from,std::span<double> weights) override {
    if(weights.size()!=count_)throw std::logic_error("Surface Heat device output dimensions");
    try{api_.read(weights.data(),address(from),weights.size_bytes());}
    catch(...){api_.drain();throw;}
  }
  void zero(Vector v) override {api_.launch(count_,Zero{address(v)});}
  void copy(Vector out,Vector in) override {api_.launch(count_,Copy{address(out),address(in)});}
  void combine(Vector out,Vector a,double scale,Vector b) override {
    api_.launch(count_,Combine{address(out),address(a),address(b),std::bit_cast<Scalar>(scale)});
  }
  void multiply(Vector result,Vector input) override {
    api_.launch(count_,Multiply{matrix_.view(),address(input),address(result)});
  }
  void forward(uint32_t first,uint32_t last) override {
    api_.launch(last-first,Triangular{lower_.view(),address(Vector::Residual),address(Vector::Forward),first});
  }
  void backward(uint32_t first,uint32_t last) override {
    api_.launch(last-first,Triangular{upper_.view(lower_.diagonal.data),address(Vector::Forward),address(Vector::Preconditioned),first});
  }
  double dot(Vector a,Vector b) override {
    auto *partial=partial_a_.data,*next=partial_b_.data;
    size_t count=chunks(count_);
    api_.launch(count,DotChunks{address(a),address(b),partial,count_});
    while(count>1){
      const size_t groups=chunks(count);
      api_.launch(groups,DotChunks{partial,nullptr,next,count});
      std::swap(next,partial);
      count=groups;
    }
    double scalar;
    try{api_.read(&scalar,partial,sizeof(scalar));}
    catch(...){api_.drain();throw;}
    return scalar;
  }
};
} // namespace dome::detail::surface_heat_device
