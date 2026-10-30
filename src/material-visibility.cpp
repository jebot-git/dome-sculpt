// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/material-visibility.hpp"
#include "dome/document.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/source-maps.hpp"
#include "dome/material-bindings.hpp"
#include "source-colors.hpp"
#include "prepared-color-state.hpp"
namespace dome {
std::optional<MToonMaterial> retained_mtoon(const CreatorAppearance *appearance){
  if(!appearance)return {};
  auto parse=[](const std::string&s){if(s.size()>2*1024*1024)throw std::length_error("MToon source metadata exceeds renderer bounds");size_t count=0;return nlohmann::json::parse(s,[&](int depth,nlohmann::json::parse_event_t,const nlohmann::json&){if(depth>32||++count>100000)throw std::length_error("MToon source metadata exceeds renderer bounds");return true;});};
  std::optional<MToonMaterial> result;
  if(!appearance->mtoon_json.empty()){auto j=parse(appearance->mtoon_json);if(j.value("shader",std::string{})=="VRM/MToon")result=MToonMaterial::from_vrm0(j,appearance->asset?appearance->asset->textures.size():0);}
  if(!result&&!appearance->material_json.empty()){auto j=parse(appearance->material_json);if(j.contains("extensions")&&j["extensions"].contains("VRMC_materials_mtoon"))result=MToonMaterial::from_vrm1(j,appearance->asset?appearance->asset->textures.size():0);}
  if(result&&(!appearance->asset||!appearance->decoded_images||appearance->decoded_images->size()!=appearance->asset->images.size()||appearance->material<0||size_t(appearance->material)>=appearance->asset->materials.size()))throw std::runtime_error("MToon retained image/material source is incomplete");
  return result;
}
int mtoon_coordinate(const MToonTexture&t){int coordinate=t.transform_texcoord>=0?t.transform_texcoord:int(t.texcoord);if(coordinate<0||coordinate>1)throw std::runtime_error("MToon preview requires a retained UV0 or UV1 set");return coordinate;}
SourceNativeMaterial native_source(const CreatorAppearance &appearance) {
  const auto &m=appearance.asset->materials.at(size_t(appearance.material));
  return {m.color,m.emissive,m.alpha==ImportedMaterial::AlphaMode::Opaque?1:m.alpha==ImportedMaterial::AlphaMode::Mask?2:3,m.alpha_cutoff,m.double_sided};
}
float mtoon_factor(float typed,float original,float current){if(current==original)return typed;if(original!=0)return typed*current/original;return current==0?typed:current;}
namespace {
PaintSourceTexture derived_view(const std::shared_ptr<const TextureMaps> &maps,
                                const Image &image) {
  PaintSourceTexture view;
  view.wrap_s = view.wrap_t = 33071;
  if (maps->source_maps && maps->source_maps->color.image) view = maps->source_maps->color;
  view.image = std::shared_ptr<const Image>(maps, &image);
  view.uv_transform = {1,0,0,1,0,0};
  return view;
}
struct Visibility {
  SurfaceRange range;
  MToonCull cull = MToonCull::Back;
  Material::AlphaMode alpha = Material::AlphaMode::Automatic;
  float cutoff = .5f, factor = 1;
  std::optional<MToonMaterial> toon;
  PaintSourceTexture color, mask, coverage, pbr;
  const ImportedPrimitive *primitive = nullptr;
  std::vector<std::array<float,4>> vertex_colors;
  explicit Visibility(const SurfaceRange &r, const Mesh &mesh) : range(r) {
    cull = r.material.double_sided ? MToonCull::Off : MToonCull::Back;
    alpha = r.material.alpha; cutoff = r.material.alpha_cutoff; factor = r.material.color[3];
    auto maps = r.textures;
    const bool prepared=detail::prepared_color_state(r.material_bindings.get(),maps.get());
    if (maps) {
      if (maps->source_only) {
        auto present=[](const Image &i){return i.width || i.height || !i.rgba.empty();};
        if(!maps->source_maps || present(maps->color) || present(maps->metal_rough) ||
           present(maps->emission) || present(maps->normal) || present(maps->color_coverage) ||
           present(maps->emission_coverage) || maps->baked_base || present(maps->baked_normal) || present(maps->baked_coverage))
          throw std::invalid_argument("Visibility source-only maps contain derived payloads");
        color = maps->source_maps->color;
      }
      else { color = derived_view(maps, maps->color); factor = 1; }
    }
    if(r.material_bindings) {
      r.material_bindings->validate();
      if(r.material_bindings->has_mtoon)toon=r.material_bindings->material;
    } else toon = retained_mtoon(r.appearance.get());
    if (!toon) {
      if(prepared){color=derived_view(maps,maps->pbr_color);color.wrap_s=color.wrap_t=33071;color.min_filter=color.mag_filter=9729;}
      if(r.appearance && !prepared)vertex_colors=detail::source_vertex_colors(r.appearance.get(),r.appearance->source_vertices.size());
      if(!vertex_colors.empty()) {
        if(r.first_vertex>mesh.positions.size() || vertex_colors.size()>mesh.positions.size()-r.first_vertex)throw std::invalid_argument("Visibility vertex-color domain is outside packed geometry");
        for(size_t c=r.first;c<r.first+r.count;++c){const auto v=mesh.indices.at(c);if(v<r.first_vertex || v-r.first_vertex>=vertex_colors.size())throw std::invalid_argument("Visibility vertex-color identity is outside its packed range");}
      }
      return;
    }
    const auto source=r.material_bindings?r.material_bindings->native_reference:native_source(*r.appearance);
    const auto &parameters = toon->parameters;
    cull = parameters.cull;
    if (r.material.double_sided != source.double_sided)
      cull = r.material.double_sided ? MToonCull::Off : MToonCull::Back;
    alpha = parameters.alpha == MToonAlpha::Opaque ? Material::AlphaMode::Opaque :
            parameters.alpha == MToonAlpha::Mask ? Material::AlphaMode::Mask : Material::AlphaMode::Blend;
    auto original = Material::AlphaMode(source.alpha);
    if (r.material.alpha != Material::AlphaMode::Automatic && r.material.alpha != original) alpha = r.material.alpha;
    cutoff = r.material.alpha_cutoff != source.alpha_cutoff ? r.material.alpha_cutoff : parameters.alpha_cutoff;
    factor = mtoon_factor(parameters.base_color[3], source.color[3], r.material.color[3]);
    auto original_view = [&](MToonSlot role) {
      PaintSourceTexture view;
      auto &slot = toon->textures[size_t(role)];
      if (slot.index < 0) return view;
      if(r.material_bindings)view=r.material_bindings->roles[size_t(role)];
      else {
        const auto &a=*r.appearance;
        const auto &texture = a.asset->textures.at(size_t(slot.index));
        if (texture.image < 0 || size_t(texture.image) >= a.decoded_images->size())
          throw std::runtime_error("Visibility source texture is unavailable");
        view.image = a.decoded_images->at(size_t(texture.image));
        view.wrap_s = texture.wrap_s; view.wrap_t = texture.wrap_t;
        view.min_filter = texture.min_filter; view.mag_filter = texture.mag_filter;
      }
      view.validate();
      if (mtoon_coordinate(slot) == 1) {
        if(!r.appearance || !r.appearance->asset)
          throw std::runtime_error("Visibility UV1 requires retained primitive provenance");
        const auto &a=*r.appearance;
        if (!primitive) for (const auto &p : a.asset->primitives)
          if (p.source_mesh == a.source_mesh && p.source_primitive == a.source_primitive) {
            if (primitive) throw std::runtime_error("Visibility source primitive is ambiguous");
            primitive = &p;
          }
        if (!primitive || primitive->uv1.size() != primitive->mesh.positions.size() ||
            a.source_vertices.empty() || r.first_vertex > mesh.positions.size() ||
            a.source_vertices.size() > mesh.positions.size() - r.first_vertex)
          throw std::runtime_error("Visibility authored UV1 data is unavailable");
        for (auto id : a.source_vertices) {
          if (id >= primitive->uv1.size())throw std::runtime_error("Visibility UV1 source vertex is invalid");
          const auto q=primitive->uv1[id];
          if(!std::isfinite(q[0]) || !std::isfinite(q[1]))throw std::runtime_error("Visibility UV1 is nonfinite");
        }
        for (size_t i = r.first; i < r.first + r.count; ++i)
          if (mesh.indices[i] < r.first_vertex || mesh.indices[i] - r.first_vertex >= a.source_vertices.size())
            throw std::runtime_error("Visibility UV1 packed index is outside its source range");
      }
      return view;
    };
    color = original_view(MToonSlot::Base); mask = original_view(MToonSlot::UvMask);
    if (maps && !maps->source_only) {
      coverage = derived_view(maps, maps->color_coverage);
      if(maps->source_maps)pbr = maps->source_maps->color;
    }
  }
  std::array<float,2> uv(const Mesh &m, uint32_t triangle,
                         const std::array<float,3> &bary, bool secondary) const {
    std::array<float,2> result{};
    for (size_t c = 0; c < 3; ++c) {
      auto id = m.indices.at(size_t(triangle) * 3 + c);
      std::array<float,2> q{};
      if (secondary) {
        if (!primitive || id < range.first_vertex) throw std::runtime_error("Visibility UV1 packed range is invalid");
        auto source = range.appearance->source_vertices.at(id - range.first_vertex);
        q = primitive->uv1.at(source);
      } else if (!m.uv.empty()) q = m.uv.at(id);
      for (size_t k = 0; k < 2; ++k) result[k] += q[k] * bary[c];
    }
    return result;
  }
  bool visible(const Mesh &m, uint32_t triangle, const std::array<float,3> &bary,
               Vec3 direction, double seconds) const {
    size_t first = size_t(triangle) * 3;
    auto a = m.positions[m.indices[first]], b = m.positions[m.indices[first+1]], c = m.positions[m.indices[first+2]];
    bool front = dot(cross(b-a,c-a), direction) < 0;
    if ((cull == MToonCull::Back && !front) || (cull == MToonCull::Front && front)) return false;
    if (alpha == Material::AlphaMode::Opaque) return true;
    auto primary = uv(m, triangle, bary, false);
    auto sample = [](const PaintSourceTexture &v, std::array<float,2> q, size_t channel = 3) {
      return sample_source_texture(v, q[0], q[1], false)[channel];
    };
    float opacity;
    if (toon) {
      auto coordinate = [&](MToonSlot role) {
        auto &slot = toon->textures[size_t(role)];
        auto input = slot.index >= 0 && mtoon_coordinate(slot) == 1 ? uv(m, triangle, bary, true) : primary;
        return input;
      };
      auto mask_uv = mtoon_uv(*toon, MToonSlot::UvMask, coordinate(MToonSlot::UvMask), seconds);
      float animation = sample(mask, mask_uv, toon->textures[size_t(MToonSlot::UvMask)].channel);
      auto main_uv = mtoon_uv(*toon, MToonSlot::Base, coordinate(MToonSlot::Base), seconds, animation);
      opacity = factor * sample(color, main_uv);
      if (range.textures && !range.textures->source_only) {
        auto derived = derived_view(range.textures, range.textures->color);
        const float replacement=sample(coverage,primary,0);
        if(replacement!=0)opacity = sample(derived, primary) + (opacity - range.material.color[3]*sample(pbr,primary)) * (1-replacement);
      }
    } else {
      opacity = factor * sample(color, primary);
      if(!vertex_colors.empty()){float a=0;for(size_t k=0;k<3;++k)a+=bary[k]*vertex_colors.at(m.indices[first+k]-range.first_vertex)[3];opacity*=a;}
    }
    opacity = std::clamp(opacity, 0.f, 1.f);
    return alpha == Material::AlphaMode::Mask ? opacity >= cutoff : opacity > 0;
  }
};
} // namespace
struct MaterialSurfaceQuery::Impl {
  SurfaceQuery query;
  std::vector<Visibility> surfaces;
  Impl(const Mesh &mesh, std::span<const SurfaceRange> ranges, const std::function<bool()> &cancelled)
      : query(mesh, cancelled) {
    size_t end = 0;
    for (const auto &range : ranges) {
      if (cancelled && cancelled()) throw std::runtime_error("Visibility build cancelled");
      if (range.first != end || range.count % 3 || range.count > mesh.indices.size() - end)
        throw std::invalid_argument("Visibility requires contiguous triangle-aligned surface ranges");
      end += range.count;
      surfaces.emplace_back(range, mesh);
    }
    if (end != mesh.indices.size()) throw std::invalid_argument("Visibility surface ranges are incomplete");
  }
};
MaterialSurfaceQuery::MaterialSurfaceQuery(const Mesh &mesh, std::span<const SurfaceRange> ranges,
                                           const std::function<bool()> &cancelled)
    : impl_(std::make_unique<Impl>(mesh, ranges, cancelled)) {}
MaterialSurfaceQuery::~MaterialSurfaceQuery() = default;
SurfaceHit MaterialSurfaceQuery::closest(Vec3 point) const { return impl_->query.closest(point); }
std::optional<SurfaceHit> MaterialSurfaceQuery::raycast(Vec3 origin, Vec3 direction,
                                                       float maximum, double seconds,uint64_t only_object) const {
  if (!std::isfinite(seconds)) throw std::invalid_argument("Invalid visibility animation time");
  SurfaceQueryOptions options; options.max_triangle_tests = 65536;
  options.ray_filter = [&](uint32_t triangle, const std::array<float,3> &bary) {
    auto index = size_t(triangle)*3;
    auto found = std::upper_bound(impl_->surfaces.begin(), impl_->surfaces.end(), index,
                                [](size_t value, const Visibility &surface) { return value < surface.range.first; });
    if (found == impl_->surfaces.begin()) return false;
    --found;
    return (!only_object || found->range.object==only_object) && index < found->range.first + found->range.count &&
           found->visible(impl_->query.mesh(), triangle, bary, direction, seconds);
  };
  return impl_->query.raycast(origin, direction, maximum, options);
}
} // namespace dome
