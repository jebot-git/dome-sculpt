// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/math.hpp"
#include <functional>
#include <memory>
#include <span>

namespace dome::detail::bake_ambient {
struct Node {
  Vec3 lo,hi;
  uint32_t escape=0,first=0,count=0;
};
struct Surface {
  std::span<const Node> nodes;
  std::span<const uint32_t> order,indices;
  std::span<const Vec3> vertices;
};
struct Frame {
  Vec3 origin,tangent,bitangent,normal;
};

inline constexpr size_t max_rays_per_batch=65'536;
inline constexpr size_t max_texels_per_batch=8'192;

class Query {
public:
  virtual ~Query()=default;
  virtual void count_blocked(std::span<const Frame>,std::span<uint16_t>,
                             const std::function<bool()> &cancelled)=0;
};
std::unique_ptr<Query> upload(const Surface &,std::span<const Vec3> pattern,float maximum);
} // namespace dome::detail::bake_ambient
