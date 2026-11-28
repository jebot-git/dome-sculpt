// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/math.hpp"
#include <functional>
#include <memory>
#include <span>

namespace dome::detail::weight_brush {
inline constexpr size_t batch_capacity=16384,batch_capsules=1'048'576;
inline constexpr size_t maximum_vertices=2'000'000;

struct Capsule {
  Vec3 from,direction,minimum,maximum;
  float squared_length,radius,strength,pressure;
};
struct Hit {
  uint32_t vertex=0;
  // Positive selects the direct joint, negative selects the reflected joint.
  float alpha=0;
};
enum Sides : unsigned { Direct=1, Reflected=2 };

class Query {
public:
  virtual ~Query()=default;
  virtual void set_capsules(std::span<const Capsule> direct,std::span<const Capsule> reflected)=0;
  // Results contain only affected vertices, ordered by input position.
  virtual size_t search(std::span<const Vec3>,unsigned sides,std::span<Hit>)=0;
  // Retain a flattened, immutable posed surface list for one preview. A
  // device allocation shortage leaves the ordinary batched GPU query usable.
  virtual bool cache_points(std::span<const std::span<const Vec3>>,
                            const std::function<bool()> &cancelled)=0;
  virtual void clear_cached_hits()=0;
  // Results contain only increases in the accumulated winning falloff.
  virtual size_t search_cached(size_t first,unsigned sides,std::span<Hit>)=0;
};
std::unique_ptr<Query> upload(std::span<const Capsule> direct,std::span<const Capsule> reflected,
                              bool reusable=false);
}
