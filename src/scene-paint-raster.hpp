// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <nlohmann/json.hpp>

namespace dome {
class PaintedMesh;
class PaintSamplingDomain;

namespace detail {
// An archived Paint2/3 leaf has its own geometry and editable data. Its
// explicit domain identity can share the atlas samples during one Scene read.
class ScenePaintRasterReader {
  using Key=std::pair<const PaintSamplingDomain *,uint32_t>;
  std::map<Key,std::shared_ptr<const void>> surfaces_;
  inline static thread_local ScenePaintRasterReader *current_=nullptr;
  ScenePaintRasterReader *previous_;
public:
  ScenePaintRasterReader():previous_(current_){current_=this;}
  ~ScenePaintRasterReader(){current_=previous_;}
  ScenePaintRasterReader(const ScenePaintRasterReader&)=delete;
  ScenePaintRasterReader&operator=(const ScenePaintRasterReader&)=delete;
  static bool active() noexcept {return current_!=nullptr;}
  static std::shared_ptr<const void> find(const PaintSamplingDomain &domain,uint32_t resolution){
    if(!current_)return {};
    const auto item=current_->surfaces_.find({&domain,resolution});
    return item==current_->surfaces_.end()?nullptr:item->second;
  }
  static void retain(const PaintSamplingDomain &domain,uint32_t resolution,std::shared_ptr<const void> surface){
    if(current_)current_->surfaces_.try_emplace({&domain,resolution},std::move(surface));
  }
};

class EncodedPaintRasterBudget {
  std::set<std::pair<uint32_t,uint32_t>> rasters_;
public:
  size_t texels(const nlohmann::json &record,const nlohmann::json &payload,uint32_t resolution){
    // Envelope, reference type, policy and resolution have already passed
    // the caller's parse checks. A bank index is scoped to this Scene alone.
    if(ScenePaintRasterReader::active()&&record.contains("version")&&
       (record.at("version")==2||record.at("version")==3)&&!record.at("domain").is_null()&&
       !payload.contains("uv_policy")&&
       !rasters_.emplace(record.at("domain").get<uint32_t>(),resolution).second)return 0;
    return size_t(resolution)*resolution;
  }
};

// Identity of the actual immutable samples in a shareable live paint leaf.
const void *retained_scene_paint_raster(const PaintedMesh &) noexcept;
}
}
