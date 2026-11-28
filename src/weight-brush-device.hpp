// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/math.hpp"
#include <memory>
#include <span>

namespace dome::detail::weight_brush {
inline constexpr size_t batch_capacity=16384,batch_capsules=1'048'576;

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
};
std::unique_ptr<Query> upload(std::span<const Capsule> direct,std::span<const Capsule> reflected,
                              bool reusable=false);
}
