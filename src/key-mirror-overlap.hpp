// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/math.hpp"
#include <functional>
#include <memory>
#include <span>

namespace dome::detail::mirror_overlap {
struct Box { Vec3 lo,hi; };
struct Node { Box bounds; uint32_t escape,first,count,last_triangle; };
struct Pair { uint32_t target,donor; };
struct Metric { uint64_t twice_plane; float tolerance; };
inline constexpr size_t batch_capacity=256;
inline constexpr uint32_t visit_quantum=128;
using Append=std::function<void(std::span<const Pair>)>;

class Query {
public:
  virtual ~Query()=default;
  virtual uint32_t search(uint32_t first,uint32_t count,Metric,uint32_t remaining,
                          const Append &,const std::function<bool()> &cancelled)=0;
};

std::unique_ptr<Query> upload(std::span<const Box>,std::span<const Node>,
                             std::span<const uint32_t> order);
}
