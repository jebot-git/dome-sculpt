// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "dome/bake.hpp"
#include "emission-bake-source.hpp"
#include <nlohmann/json_fwd.hpp>
namespace dome::detail {
// A separate continuous emission authority. Colour/normal/editable paint retain
// their chosen current grid; this immutable source field retains its own knots.
// Only certified exact ordered planar correspondence is admitted initially.
class BakedEmissionDomain final {
public:
 // Rows of a target-UV0 to source-paint affine transform, including offsets.
 using Mapping=std::array<std::array<float,3>,2>;
 static constexpr Mapping identity{{{1,0,0},{0,1,0}}};
 struct TextureLayout {
  uint32_t width,height;
  std::array<uint32_t,2> legacy{},image{};
 };
 static size_t preflight(const Scene &,const std::function<bool()> &cancelled={});
 static void preflight(const PaintedMesh &,const std::function<bool()> &cancelled={});
 static size_t preflight_archive(const nlohmann::json &,const std::function<bool()> &cancelled={});
 static std::shared_ptr<const BakedEmissionDomain> capture(const Scene &,uint64_t source,
     std::shared_ptr<const Mesh> target,uint32_t target_resolution,CanvasTransform target_world,
     const std::function<bool()> &cancelled={});
 static std::shared_ptr<const BakedEmissionDomain> decode(const nlohmann::json &,
     std::shared_ptr<const Mesh> target,uint32_t target_resolution,
     const std::function<bool()> &cancelled={});
 nlohmann::json encode() const;
 BakedEmissionSample sample(float u,float v)const;
 const Mapping &mapping()const noexcept{return mapping_;}
 const Mapping &image_mapping()const noexcept{return image_mapping_;}
 const auto &image()const noexcept{return image_;}
 float legacy_strength()const noexcept{return legacy_strength_;}
 TextureLayout texture_layout()const noexcept;
 bool mapped()const noexcept{return mapping_!=identity;}
 unsigned version()const noexcept{return image_?3:mapped()?2:1;}
 const auto &field()const noexcept{return field_;}
 const auto &target_geometry()const noexcept{return target_;}
 uint64_t source_id()const noexcept{return source_;}
 uint32_t target_resolution()const noexcept{return target_resolution_;}
 size_t storage_bytes()const noexcept{return retained_bytes_;}
 void validate_current(const std::shared_ptr<const Mesh>&,uint32_t)const;
 void validate_current(const PaintedMesh &)const;
 bool valid_owner(const std::shared_ptr<const BakedEmissionDomain>&)const noexcept;
 BakedEmissionDomain(const BakedEmissionDomain&)=delete;
private:
 // Decode replays the exact supplied context. New captures first select the
 // source/dependency closure; existing Scene43 before-records are not rewritten.
 static std::shared_ptr<const BakedEmissionDomain> capture_provenance(const Scene &,uint64_t,
     std::shared_ptr<const Mesh>,uint32_t,CanvasTransform,const std::function<bool()> &);
 BakedEmissionDomain()=default;
 std::weak_ptr<const BakedEmissionDomain> self_;
 std::shared_ptr<const Scene> before_;
 std::shared_ptr<const Mesh> target_;
 std::shared_ptr<const BakedEmissionField> field_;
 CanvasTransform target_world_{};
 uint64_t source_=0;
 uint32_t target_resolution_=0;
 size_t retained_bytes_=0;
 Mapping mapping_=identity;
 Mapping image_mapping_=identity;
 std::optional<EmissionImageRole> image_;
 float legacy_strength_=1;
};
} // namespace dome::detail
