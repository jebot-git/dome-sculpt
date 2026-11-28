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

inline constexpr size_t transfer_capacity=4096;
struct TransferPoint {
  uint64_t position[3];
  Vec3 normal;
  bool restrict_material;
};
struct TransferHit {
  uint64_t distance;
  uint32_t triangle=UINT32_MAX;
  float bary[3]{};
};
struct TransferMetric {
  uint64_t rows[3][3];
  uint64_t distance_limit;
  bool affine;
};
struct TransferStatus {uint32_t flags,visits;};
class TransferQuery {
public:
  virtual ~TransferQuery()=default;
  virtual void material(std::span<const uint8_t>)=0;
  virtual uint32_t search(std::span<const TransferPoint>,std::span<TransferHit>,
                          uint32_t work,const std::function<bool()> &cancelled)=0;
};
std::unique_ptr<TransferQuery> upload_transfer(const weight_transfer::Surface &,
                                              std::span<const Vec3>,TransferMetric);
}
