// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/math.hpp"
#include <cstdint>
#include <memory>
#include <span>

namespace dome::detail::weight_transfer {
struct Node {
  Vec3 lo,hi;
  uint32_t first=0,count=0,left=0,right=0;
};
struct Barycentric {
  float a,b,c;
};
struct Hit {
  uint32_t triangle=UINT32_MAX;
  Barycentric weight{};
};
struct Surface {
  std::span<const Node> nodes;
  std::span<const uint32_t> order,indices;
  std::span<const Vec3> vertices;
};
inline constexpr size_t batch_capacity=1024;

class Query {
public:
  virtual ~Query()=default;
  // An over-budget dispatch returns only the visit count. The caller may
  // retry with a smaller batch as long as its remaining work permits it.
  virtual uint32_t search(std::span<const Vec3>,float squared_radius,uint32_t work,
                          std::span<Hit>)=0;
};
std::unique_ptr<Query> upload(const Surface &);
} // namespace dome::detail::weight_transfer
