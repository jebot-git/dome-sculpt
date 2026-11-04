// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "baked-normal-field.hpp"
#include "canonical-normal-uv.hpp"
#include <nlohmann/json.hpp>
#include <map>
namespace dome::detail {
// One synchronous Scene codec owns this thread-local graph. Nested Scene
// operations get independent scopes; the prior context is restored on unwind.
// No capability or raw-pointer cache survives the owning operation.
class BakedNormalArchive {
public:
  explicit BakedNormalArchive(PaintImageArchive &);
  BakedNormalArchive(const nlohmann::json *,const PaintImageArchive &);
  ~BakedNormalArchive();
  BakedNormalArchive(const BakedNormalArchive&)=delete;
  static BakedNormalArchive &writer();
  static BakedNormalArchive &reader();
  static size_t preflight(const nlohmann::json &,const nlohmann::json &material);
  uint32_t add_origin(std::shared_ptr<const CanonicalNormalConversion>);
  uint32_t add_uv(std::shared_ptr<const CanonicalNormalUv>);
  std::shared_ptr<const CanonicalNormalUv> uv_at(uint32_t);
  uint32_t add_field(std::shared_ptr<const BakedNormalField>);
  void register_paint(const std::shared_ptr<const PaintedMesh> &,uint32_t);
  void paint_resolver(std::function<std::shared_ptr<const PaintedMesh>(uint32_t)>);
  std::shared_ptr<const CanonicalNormalConversion> origin_at(uint32_t);
  std::shared_ptr<const BakedNormalField> field_at(uint32_t);
  nlohmann::json encode();
  bool empty() const noexcept{return origins_.empty();}
  void require_reachable() const;
private:
  static thread_local BakedNormalArchive *current_;
  BakedNormalArchive *previous_=nullptr;
  PaintImageArchive *write_images_=nullptr;
  const PaintImageArchive &images_;
  const nlohmann::json *input_=nullptr;
  std::function<std::shared_ptr<const PaintedMesh>(uint32_t)> paint_resolver_;
  std::map<const PaintedMesh*,uint32_t> paint_ids_;
  std::vector<std::shared_ptr<const CanonicalNormalConversion>> origins_;
  std::vector<std::shared_ptr<const BakedNormalField>> fields_;
  std::vector<std::shared_ptr<const CanonicalNormalUv>> uv_;
  std::vector<uint8_t> uv_used_;
  std::vector<uint8_t> origin_busy_,field_busy_,origin_used_,field_used_;
};
} // namespace dome::detail
