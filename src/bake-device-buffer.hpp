// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>

namespace dome::detail {
template<class Api,class T> struct BakeDeviceBuffer {
  Api &api;
  T *data;
  BakeDeviceBuffer(Api &context,size_t size):api(context),data(size?static_cast<T*>(api.allocate(size*sizeof(T))):nullptr){}
  ~BakeDeviceBuffer(){if(data)api.release(data);}
  BakeDeviceBuffer(const BakeDeviceBuffer &)=delete;
};
} // namespace dome::detail
