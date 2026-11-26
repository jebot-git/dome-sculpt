// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "baked-emission-codec.hpp"
#include "dome/paint.hpp"
#include "dome/paint-sampling.hpp"
#include <map>
#include <set>

namespace dome::detail {
inline constexpr unsigned shared_bake_scene_version=44;
inline constexpr unsigned shared_bake_reference_version=7;
inline constexpr size_t scene_paint_byte_limit=256*1024*1024;

// Only plain native linked paint has an independent, authenticated raster
// without a per-wrapper baked colour, original-source or normal event.
inline bool shareable_bake(const PaintedMesh &paint){
  const auto domain=paint.paint_sampling_domain();
  const auto bake=paint.baked();
  return domain&&domain->version()==1&&bake&&bake->emission_field&&
      bake->emission_field->storage().dense.empty()&&
      paint.surface_diagnostics().policy==PaintUvPolicy::Strict&&
      !paint.source_maps()&&!paint.color_projection()&&!paint.normal_projection()&&
      !paint.source_sampling_prepared()&&!bake->normal_field&&!bake->normal_authority&&
      bake->color_interpretation==BakedColorInterpretation::VertexComponent&&
      bake->normal_interpretation!=BakedNormalInterpretation::PreparedAttributes;
}
using SharedBakeIdentity=std::pair<const BakedTextures *,const PaintSamplingDomain *>;
inline SharedBakeIdentity shared_bake_identity(const PaintedMesh &paint){
  return {paint.baked().get(),paint.paint_sampling_domain().get()};
}

class SharedBakeWriter {
  struct Slot {size_t uses=0;std::optional<uint32_t> id;};
  inline static thread_local SharedBakeWriter *current_=nullptr;
  SharedBakeWriter *previous_;
  const Scene *scene_;
  const PaintImageArchive *archive_=nullptr;
  std::map<SharedBakeIdentity,Slot> slots_;
  nlohmann::json entries_=nlohmann::json::array();
public:
  explicit SharedBakeWriter(const Scene &scene):previous_(current_),scene_(&scene){current_=this;}
  ~SharedBakeWriter(){current_=previous_;}
  SharedBakeWriter(const SharedBakeWriter &)=delete;
  static void note(const Scene &scene,SharedBakeIdentity identity){
    if(current_&&current_->scene_==&scene&&!current_->archive_)++current_->slots_[identity].uses;
  }
  void begin(const PaintImageArchive &archive){archive_=&archive;}
  nlohmann::json take(){return std::move(entries_);}
  bool empty() const noexcept{return entries_.empty();}
  template<class Encode,class Domain>
  static nlohmann::json encode(const PaintedMesh &paint,const PaintImageArchive *archive,
      Domain domain,Encode serialize){
    if(!current_||!archive||current_->archive_!=archive||!shareable_bake(paint))return serialize();
    auto slot=current_->slots_.find(shared_bake_identity(paint));
    if(slot==current_->slots_.end()||slot->second.uses<2)return serialize();
    if(!slot->second.id){
      auto &bank=current_->entries_;
      if(bank.size()>=256)throw std::length_error("Shared baked field count exceeds archive budget");
      const auto id=uint32_t(bank.size());
      bank.push_back({{"resolution",paint.resolution()},{"domain",domain()},{"bake",serialize()}});
      slot->second.id=id;
    }
    return {{"version",shared_bake_reference_version},{"id",*slot->second.id}};
  }
};

inline bool shared_bake_reference(const nlohmann::json &value){
  return value.is_object()&&value.contains("version")&&value.at("version")==shared_bake_reference_version;
}
class SharedBakeReader {
public:
  struct Entry {
    const nlohmann::json *record=nullptr;
    uint32_t resolution=0,domain=0;
    mutable std::shared_ptr<const BakedTextures> owner;
    mutable std::set<const nlohmann::json *> users;
  };
  SharedBakeReader(const nlohmann::json &,const BakedEmissionReadCapability &);
  const Entry &at(const nlohmann::json &) const;
  const Entry *resolve(const nlohmann::json &record,const nlohmann::json &paint,uint32_t resolution) const;
  void require_complete() const;
  size_t bytes() const noexcept{return bytes_;}
private:
  std::vector<Entry> entries_;
  size_t bytes_=0;
};

inline const SharedBakeReader::Entry *shared_bake_for(const nlohmann::json &record,
    const nlohmann::json &paint,uint32_t resolution,const BakedEmissionReadCapability *capability){
  if(!paint.contains("baked")||!shared_bake_reference(paint.at("baked")))return nullptr;
  if(!capability||!capability->shared_bakes())
    throw std::invalid_argument("Shared baked field requires its owning Scene archive");
  return capability->shared_bakes()->resolve(record,paint,resolution);
}
inline const nlohmann::json &shared_bake_payload(const nlohmann::json &value,
    const BakedEmissionReadCapability *capability){
  if(!shared_bake_reference(value))return value;
  if(!capability||!capability->shared_bakes())
    throw std::invalid_argument("Shared baked field requires its owning Scene archive");
  return *capability->shared_bakes()->at(value).record;
}
}
