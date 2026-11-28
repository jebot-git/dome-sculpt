// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/math.hpp"
#include <functional>
#include <memory>
#include <span>

namespace dome::detail::key_mirror {
struct Cell { int64_t x,y,z; bool operator==(const Cell &) const = default; };
struct Bin { Cell cell; uint32_t first,count; };
struct Metric { uint64_t twice_plane,squared_tolerance; };
inline constexpr size_t batch_capacity=4096;
inline constexpr uint32_t missing=UINT32_MAX,conflict=missing-1;

class Query {
public:
  virtual ~Query()=default;
  virtual uint32_t search(size_t first,std::span<const Cell>,Metric,uint32_t work,
      std::span<uint32_t> donors,const std::function<bool()> &cancelled)=0;
};

std::unique_ptr<Query> upload(std::span<const Bin>,std::span<const uint32_t>,
                             std::span<const Vec3> rest,std::span<const Vec3> delta);
}
