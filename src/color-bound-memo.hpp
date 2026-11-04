// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "color-projection-quality.hpp"
#include "dome/polygon-color.hpp"
#include "humanoid-sha256.hpp"
#include <optional>
#include <algorithm>
#include <bit>
#include <type_traits>
namespace dome {class SourceColorPaintSampling;struct SourceMaterialBindings;struct DeformationState;}
namespace dome::detail {
// A call-local numerical memo, never a material/geometry validity certificate.
// Every caller still authenticates the current geometry/field and compares the
// computed result with the current claimed receipt. No owner survives the call.
struct ColorBoundMemoKey {
  std::shared_ptr<const Mesh> mesh,current_mesh;
  std::shared_ptr<const PolygonMesh> topology;
  std::shared_ptr<const PolygonColorField> field;
  std::shared_ptr<const PaintColorProjection> projection;
  std::shared_ptr<const Image> image;
  std::shared_ptr<const PaintedMesh> source_paint;
  std::shared_ptr<const SourceColorPaintSampling> sampling;
  std::shared_ptr<const SourceMaterialBindings> material;
  std::shared_ptr<const DeformationState> deformation;
  const void *recipe=nullptr;
  uint64_t max_cells=0,claimed_cells=0;
  std::array<uint32_t,4> error_bits{};
  std::array<uint32_t,2> dimensions{};
  std::array<char,64> image_content{},colors_content{};
  uint32_t source_field=0;
  bool initial=false;
};
template<class T>inline bool color_bound_same_owner(const std::shared_ptr<const T>&a,
    const std::shared_ptr<const T>&b) noexcept {
  return a.get()==b.get()&&!a.owner_before(b)&&!b.owner_before(a);
}
inline bool same_color_bound_key(const ColorBoundMemoKey&a,const ColorBoundMemoKey&b) noexcept {
  return a.recipe==b.recipe&&a.max_cells==b.max_cells&&a.claimed_cells==b.claimed_cells&&
    a.error_bits==b.error_bits&&a.dimensions==b.dimensions&&a.image_content==b.image_content&&
    a.colors_content==b.colors_content&&a.source_field==b.source_field&&a.initial==b.initial&&
    color_bound_same_owner(a.mesh,b.mesh)&&color_bound_same_owner(a.current_mesh,b.current_mesh)&&color_bound_same_owner(a.topology,b.topology)&&
    color_bound_same_owner(a.field,b.field)&&color_bound_same_owner(a.projection,b.projection)&&
    color_bound_same_owner(a.image,b.image)&&color_bound_same_owner(a.source_paint,b.source_paint)&&
    color_bound_same_owner(a.sampling,b.sampling)&&color_bound_same_owner(a.material,b.material)&&
    color_bound_same_owner(a.deformation,b.deformation);
}
inline std::array<char,64> color_bound_digest(std::span<const uint8_t> bytes) {
  // Content only invalidates a numerical memo when legally mutable metadata is
  // presented again. A digest never supplies missing ownership or authority.
  const auto text=creator_detail::sha256(bytes);std::array<char,64> out{};
  std::copy_n(text.begin(),out.size(),out.begin());return out;
}
struct ColorBoundMemoStats {size_t hits=0,misses=0,full_fallback=0;};
struct ColorBoundMemoContext {
  struct Entry {ColorBoundMemoKey key;ColorProjectionBound value;};
  std::array<Entry,64> entries{};
  size_t depth=0,count=0;
  ColorBoundMemoStats stats{},last{};
};
inline ColorBoundMemoContext &color_bound_context() {
  thread_local ColorBoundMemoContext value;return value;
}
class ColorBoundScope {
public:
  ColorBoundScope() noexcept {auto &c=color_bound_context();if(!c.depth)c.stats={};++c.depth;}
  ColorBoundScope(const ColorBoundScope&)=delete;
  ColorBoundScope&operator=(const ColorBoundScope&)=delete;
  ~ColorBoundScope() noexcept {
    auto &c=color_bound_context();if(--c.depth)return;
    c.last=c.stats;
    static_assert(std::is_nothrow_move_assignable_v<ColorBoundMemoContext::Entry>);
    for(size_t n=0;n<c.count;++n)c.entries[n]={};c.count=0;
  }
};
inline bool color_bound_scope_active() noexcept {return color_bound_context().depth!=0;}
inline ColorBoundMemoStats last_color_bound_memo_stats() noexcept {return color_bound_context().last;}
inline std::optional<ColorProjectionBound> find_color_bound(const ColorBoundMemoKey &key) {
  auto &c=color_bound_context();if(!c.depth)return {};
  for(size_t n=0;n<c.count;++n)if(same_color_bound_key(c.entries[n].key,key)) {
    if(c.entries[n].value.tested_cells>key.max_cells)
      throw std::length_error("Color projection quality exceeds its work budget");
    ++c.stats.hits;return c.entries[n].value;
  }
  ++c.stats.misses;return {};
}
inline void remember_color_bound(ColorBoundMemoKey key,const ColorProjectionBound &value) noexcept {
  auto &c=color_bound_context();if(!c.depth)return;
  if(c.count==c.entries.size()){++c.stats.full_fallback;return;}
  c.entries[c.count++]={std::move(key),value};
}
} // namespace dome::detail
