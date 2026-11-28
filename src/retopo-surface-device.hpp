// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "weight-transfer-device.hpp"
#include <functional>

namespace dome::detail::retopo_surface {
inline constexpr size_t batch_capacity=256;

struct Hit {
  uint64_t distance_bits;
  Vec3 position;
  float bary[3];
  uint32_t triangle;
};

class Query {
public:
  virtual ~Query()=default;
  virtual void search(std::span<const Vec3>,uint32_t triangle_budget,std::span<Hit>,
                      const std::function<bool()> &cancelled)=0;
};
std::unique_ptr<Query> upload(const weight_transfer::Surface &);
}
