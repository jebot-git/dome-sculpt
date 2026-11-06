#include "polygon-surface-range.hpp"
#include "native-paint-sampling.hpp"
#include "native-paint-metadata.hpp"
#include "texture-set-range.hpp"
// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/material-visibility.hpp"
#include "dome/document.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/source-maps.hpp"
#include "dome/material-bindings.hpp"
#include "source-colors.hpp"
#include "dome/paint-sampling.hpp"
#include "dome/normal-preview.hpp"
#include "dome/color-paint-sampling.hpp"
#include "prepared-color-state.hpp"
#include "paint-overlay-alpha.hpp"
#include "source-vrm1-background.hpp"
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
  std::shared_ptr<const detail::PaintOverlayPacket> overlay;
  const ImportedPrimitive *primitive = nullptr;
  std::shared_ptr<const std::vector<std::array<float,2>>> owned_uv0;
  std::shared_ptr<const std::vector<std::array<float,2>>> owned_uv1;
  std::map<uint32_t,std::shared_ptr<const std::vector<std::array<float,2>>>> owned_extra_uv;
  std::vector<std::array<float,4>> vertex_colors;
  std::vector<std::array<float,2>> paint_corners;
  explicit Visibility(const SurfaceRange &r, const Mesh &mesh,
                      std::span<const std::array<float,4>> packed_colors, bool explicit_colors, std::span<const std::array<float,2>> coordinates, bool per_vertex,const std::function<bool()>&cancelled) : range(r) {
    detail::validate_polygon_range(mesh,r,coordinates,per_vertex,{},{},{},false,cancelled);
    if(r.texture_set_proof){r.texture_set_proof->authenticate_stream(mesh,r);r.texture_set_proof->authenticate_tint(mesh,r,packed_colors);}
    else if(r.texture_set)throw std::invalid_argument("Visibility texture set lacks its actual field/range proof");
    cull = r.material.double_sided ? MToonCull::Off : MToonCull::Back;
    alpha = r.material.alpha; cutoff = r.material.alpha_cutoff; factor = r.material.color[3];
    auto maps = r.textures;
    if(maps&&maps->overlay&&maps->overlay->native_geometry()){
      const auto &packet=*maps->overlay;
      if(maps->source_only||packet.native_domain()!=(r.paint_sampling_attachment?r.paint_sampling_attachment->domain():nullptr)||
         (r.paint_sampling_attachment&&!r.paint_sampling_attachment->matches(packet.native_geometry(),packet.native_domain())))
        throw std::invalid_argument("Native emission visibility lost its actual independent paint attachment");
      // Native emission does not change alpha. Colour/masks already belong to
      // the derived RGBA field, so do not apply the full Source4 overlay twice.
    }
    if(maps&&maps->overlay&&!maps->overlay->native_geometry()){
      const auto &source=maps->overlay->source_maps();
      if(!maps->source_only||!source||source!=maps->source_maps||source.owner_before(maps->source_maps)||maps->source_maps.owner_before(source))
        throw std::invalid_argument("Visibility overlay lost its exact original source sampling owner");
      overlay=maps->overlay;
      owned_uv0=overlay->original_uv0();
      if(!owned_uv0||r.first_vertex>mesh.positions.size()||owned_uv0->size()>mesh.positions.size()-r.first_vertex)
        throw std::invalid_argument("Visibility source paint omitted its sealed original UV0 domain");
      paint_corners.reserve(r.count);
      for(size_t c=r.first;c<r.first+r.count;++c){const auto v=mesh.indices.at(c);
        if(v<r.first_vertex||v-r.first_vertex>=owned_uv0->size())throw std::invalid_argument("Visibility original UV0 index is outside its sealed domain");
        paint_corners.push_back(mesh.uv.at(v));
      }
    }
    const auto paint_node=detail::material_paint_attachment(r.material_bindings.get());
    if(bool(paint_node)!=bool(r.paint_sampling_attachment))throw std::invalid_argument("Visibility paint range omitted or fabricated its own sampling attachment");
    if(r.paint_sampling_attachment) {
      if(!maps||maps->source_only||!r.material_bindings||r.material_bindings->has_mtoon)
        throw std::invalid_argument("Visibility paint domain requires its paired derived material");
      const auto expected=detail::material_paint_attachment(r.material_bindings.get());
      if(!expected||expected!=r.paint_sampling_attachment||expected.owner_before(r.paint_sampling_attachment)||r.paint_sampling_attachment.owner_before(expected))
        throw std::invalid_argument("Visibility paint range differs from its own immutable material attachment");
      const auto &domain=*r.paint_sampling_attachment->domain();
      if((r.texture_set_proof?r.count!=r.texture_set_proof->draw().count:domain.source_corners().size()!=r.count)||coordinates.empty())
        throw std::invalid_argument("Visibility paint corner coordinates are incomplete");
      paint_corners.reserve(r.count);
      for(size_t c=0;c<r.count;++c) {
        const auto logical=detail::surface_logical_corner(r,c);
        auto source=domain.source_corners()[logical];
        auto expected=domain.basis()->uv.at(domain.basis()->indices.at(source));
        auto actual=coordinates[per_vertex?mesh.indices.at(r.first+c):r.first+c];
        if(actual!=expected)throw std::invalid_argument("Visibility paint coordinates differ from their owning corner domain");
        paint_corners.push_back(actual);
      }
    }
    if(maps&&maps->source_maps&&maps->source_maps->uv1){
      maps->source_maps->validate();owned_uv1=maps->source_maps->uv1;
      if(r.first_vertex>mesh.positions.size()||owned_uv1->size()>mesh.positions.size()-r.first_vertex)throw std::invalid_argument("Visibility UV1 domain exceeds packed geometry");
      for(size_t c=r.first;c<r.first+r.count;++c){auto v=mesh.indices.at(c);if(v<r.first_vertex||v-r.first_vertex>=owned_uv1->size())throw std::invalid_argument("Visibility UV1 packed index is invalid");}
    }
    if(maps&&maps->source_maps&&!maps->source_maps->extra_uv_sets.empty()){
      maps->source_maps->validate();owned_extra_uv=maps->source_maps->extra_uv_sets;
      for(const auto &[set,values]:owned_extra_uv){if(r.first_vertex>mesh.positions.size()||values->size()>mesh.positions.size()-r.first_vertex)throw std::invalid_argument("Visibility additional UV domain exceeds packed geometry");for(size_t c=r.first;c<r.first+r.count;++c){auto vertex=mesh.indices.at(c);if(vertex<r.first_vertex||vertex-r.first_vertex>=values->size())throw std::invalid_argument("Visibility additional UV packed index is invalid");}}
    }
    if(r.normal_preview_proof&&!r.normal_preview_proof->matches(r.material_bindings,maps))throw std::invalid_argument("Visibility normal preview lost its own immutable owners");
    const bool sealed24=r.paint_sampling_attachment&&r.normal_preview_proof&&r.normal_preview_proof->matches(r.material_bindings,maps)&&r.normal_preview_proof->metadata_domain()==r.paint_sampling_attachment->domain()&&
      !r.normal_preview_proof->metadata_domain().owner_before(r.paint_sampling_attachment->domain())&&!r.paint_sampling_attachment->domain().owner_before(r.normal_preview_proof->metadata_domain());
    if(r.normal_preview_proof&&r.paint_sampling_attachment&&!sealed24)throw std::invalid_argument("Visibility normal and paint metadata have different owning coordinate domains");
    const bool sealed=sealed24||detail::native_frame_preview_metadata(r.material_bindings,r.paint_sampling_attachment);
    const bool prepared=detail::prepared_color_state(r.material_bindings.get(),maps.get(),!sealed);
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
      r.material_bindings->validate(!sealed);
      if(r.material_bindings->has_mtoon&&!r.pbr_view)toon=r.material_bindings->material;
    } else if(!r.pbr_view)toon = retained_mtoon(r.appearance.get());
    if (!toon) {
      if(const auto background=detail::vrm1_source_background(r.material,r.material_bindings.get(),r.appearance.get(),maps.get(),r.pbr_view))factor=background->color[3];
      color.validate(maps&&maps->source_maps?maps->source_maps->coordinate_count():owned_uv1?2:1);
      if(prepared){color=derived_view(maps,maps->pbr_color);color.wrap_s=color.wrap_t=33071;color.min_filter=color.mag_filter=9729;}
      if(explicit_colors) {
        if(!packed_colors.empty()&&!r.texture_set_proof) {
          size_t end=r.first_vertex;
          for(size_t c=r.first;c<r.first+r.count;++c){auto v=mesh.indices.at(c);if(v<r.first_vertex)throw std::invalid_argument("Packed visibility color offset is outside its range");end=std::max(end,size_t(v)+1);}
          if(end>packed_colors.size())throw std::invalid_argument("Packed visibility color range is incomplete");
          auto slice=packed_colors.subspan(r.first_vertex,end-r.first_vertex);
          if(prepared){for(const auto &color:slice)if(color!=std::array<float,4>{1,1,1,1})throw std::invalid_argument("Prepared visibility cannot carry a second vertex tint");}
          else vertex_colors.assign(slice.begin(),slice.end());
        } else if(!prepared&&r.appearance&&!detail::source_vertex_colors(r.appearance.get(),r.appearance->source_vertices.size()).empty())
          throw std::invalid_argument("Packed visibility omitted its authored vertex colors");
      } else if(r.appearance && !prepared)vertex_colors=detail::source_vertex_colors(r.appearance.get(),r.appearance->source_vertices.size());
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
        if(overlay){
          const auto &logical=overlay->original_uv1();
          if(!logical)
            throw std::invalid_argument("Visibility MToon UV1 lacks actual original authored authority");
          return view;
        }
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
                         const std::array<float,3> &bary, int coordinate) const {
    std::array<float,2> result{};
    for (size_t c = 0; c < 3; ++c) {
      auto id = m.indices.at(size_t(triangle) * 3 + c);
      std::array<float,2> q{};
      if(coordinate>=2){
        auto values=owned_extra_uv.find(uint32_t(coordinate));if(values==owned_extra_uv.end()||id<range.first_vertex)throw std::runtime_error("Visibility additional UV has no exact coordinate owner");q=values->second->at(id-range.first_vertex);
      }else if (coordinate==1) {
        if(id<range.first_vertex)throw std::runtime_error("Visibility UV1 packed range is invalid");
        if(toon&&overlay&&overlay->original_uv1())q=overlay->original_uv1()->at(id-range.first_vertex);
        else if(owned_uv1)q=owned_uv1->at(id-range.first_vertex);else{
          if(!primitive)throw std::runtime_error("Visibility UV1 has no coordinate authority");
          auto source=range.appearance->source_vertices.at(id-range.first_vertex);q=primitive->uv1.at(source);
        }
      } else if(owned_uv0){if(id<range.first_vertex)throw std::invalid_argument("Visibility original UV0 packed offset is invalid");q=owned_uv0->at(id-range.first_vertex);}
      else if (!m.uv.empty()) q = m.uv.at(id);
      for (size_t k = 0; k < 2; ++k) result[k] += q[k] * bary[c];
    }
    return result;
  }
  bool visible(const Mesh &m, uint32_t triangle, const std::array<float,3> &bary,
               Vec3 direction, double seconds,bool culling=true) const {
    size_t first = size_t(triangle) * 3;
    auto a = m.positions[m.indices[first]], b = m.positions[m.indices[first+1]], c = m.positions[m.indices[first+2]];
    bool front = (dot(cross(b-a,c-a), direction) < 0) != range.reflected;
    if (culling&&((cull == MToonCull::Back && !front) || (cull == MToonCull::Front && front))) return false;
    if (alpha == Material::AlphaMode::Opaque) return true;
    auto primary = uv(m, triangle, bary, false);
    auto sample = [](const PaintSourceTexture &v, std::array<float,2> q, size_t channel = 3) {
      auto resolved=v;resolved.texcoord=0;return sample_source_texture(resolved, q[0], q[1], false)[channel];
    };
    float opacity;
    auto sampling=primary;
    if(!paint_corners.empty()){
      sampling={};const auto local=size_t(triangle)*3-range.first;
      for(size_t c=0;c<3;++c)for(size_t k=0;k<2;++k)sampling[k]+=bary[c]*paint_corners.at(local+c)[k];
    }
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
      if(overlay){const auto paint=detail::sample_overlay_alpha(*overlay,sampling[0],sampling[1],true);
        opacity=opacity*(1-paint.coverage)+paint.premultiplied;}
      if (range.textures && !range.textures->source_only) {
        auto derived = derived_view(range.textures, range.textures->color);
        const float replacement=sample(coverage,primary,0);
        if(replacement!=0)opacity = sample(derived, primary) + (opacity - range.material.color[3]*sample(pbr,primary)) * (1-replacement);
      }
    } else {
      opacity = factor * sample(color, color.texcoord!=0?uv(m,triangle,bary,color.texcoord):sampling);
      if(overlay){const auto paint=detail::sample_overlay_alpha(*overlay,sampling[0],sampling[1],true);
        opacity=opacity*(1-paint.coverage)+paint.premultiplied;}
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
  Impl(const Mesh &mesh, std::span<const SurfaceRange> ranges, const std::function<bool()> &cancelled,
       std::span<const std::array<float,4>> colors,bool explicit_colors, std::span<const std::array<float,2>> paint={},bool per_vertex=false)
      : query(mesh, cancelled) {
    if(!colors.empty()&&colors.size()!=mesh.positions.size())throw std::invalid_argument("Packed visibility color vertex count differs from geometry");
    for(size_t i=0;i<colors.size();++i){if((i&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Visibility color validation cancelled");for(float c:colors[i])if(!std::isfinite(c)||c<0||c>1)throw std::invalid_argument("Packed visibility requires finite linear unit RGBA");}
    if(!paint.empty()&&paint.size()!=(per_vertex?mesh.positions.size():mesh.indices.size()))throw std::invalid_argument("Visibility paint coordinate alignment differs from geometry");
    for(size_t c=0;c<paint.size();++c){if((c&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Visibility paint validation cancelled");for(float v:paint[c])if(!std::isfinite(v))throw std::invalid_argument("Visibility paint coordinates are nonfinite");}
    size_t end = 0;
    for (const auto &range : ranges) {
      if (cancelled && cancelled()) throw std::runtime_error("Visibility build cancelled");
      if (range.first != end || range.count % 3 || range.count > mesh.indices.size() - end)
        throw std::invalid_argument("Visibility requires contiguous triangle-aligned surface ranges");
      end += range.count;
      surfaces.emplace_back(range, mesh,colors,explicit_colors,paint,per_vertex,cancelled);
    }
    if (end != mesh.indices.size()) throw std::invalid_argument("Visibility surface ranges are incomplete");
  }
};
MaterialSurfaceQuery::MaterialSurfaceQuery(const Mesh &mesh, std::span<const SurfaceRange> ranges,
                                           const std::function<bool()> &cancelled)
    : impl_(std::make_unique<Impl>(mesh, ranges, cancelled,std::span<const std::array<float,4>>{},false)) {}
MaterialSurfaceQuery::MaterialSurfaceQuery(const Mesh &mesh,std::span<const SurfaceRange> ranges,
 const std::function<bool()> &cancelled,std::span<const std::array<float,4>> colors)
    : impl_(std::make_unique<Impl>(mesh,ranges,cancelled,colors,true)) {}
MaterialSurfaceQuery::MaterialSurfaceQuery(const Mesh &mesh,std::span<const SurfaceRange> ranges,
 const std::function<bool()> &cancelled,std::span<const std::array<float,4>> colors,PaintCornerCoordinates paint)
    :impl_(std::make_unique<Impl>(mesh,ranges,cancelled,colors,true,paint.values,false)){}
MaterialSurfaceQuery::MaterialSurfaceQuery(const Mesh &mesh,std::span<const SurfaceRange> ranges,
 const std::function<bool()> &cancelled,std::span<const std::array<float,4>> colors,PaintVertexCoordinates paint)
    :impl_(std::make_unique<Impl>(mesh,ranges,cancelled,colors,true,paint.values,true)){}
MaterialSurfaceQuery::~MaterialSurfaceQuery() = default;
SurfaceHit MaterialSurfaceQuery::closest(Vec3 point) const { return impl_->query.closest(point); }
std::optional<SurfaceHit> MaterialSurfaceQuery::nearby(Vec3 point,float maximum,double seconds) const {
  if(!std::isfinite(maximum)||maximum<=0||!std::isfinite(seconds))throw std::invalid_argument("Invalid direct selection query");
  SurfaceQueryOptions options;options.max_triangle_tests=65536;options.closest_maximum=maximum;
  options.closest_filter=[&](uint32_t triangle,const std::array<float,3> &bary){
    const size_t corner=size_t(triangle)*3;
    auto found=std::upper_bound(impl_->surfaces.begin(),impl_->surfaces.end(),corner,[](size_t value,const Visibility &surface){return value<surface.range.first;});
    if(found==impl_->surfaces.begin())return false;
    --found;
    // A controller can touch either side or enter a surface slightly. Keep
    // alpha/UV/MToon animation, but don't impose camera backface culling.
    return corner<found->range.first+found->range.count&&found->visible(impl_->query.mesh(),triangle,bary,{},seconds,false);
  };
  return impl_->query.nearest(point,options);

}
SurfaceHit MaterialSurfaceQuery::closest(Vec3 point,uint64_t only_object) const {
  SurfaceQueryOptions options;options.max_triangle_tests=65536;
  options.triangle_filter=[&](uint32_t triangle) {
    const size_t corner=size_t(triangle)*3;
    for(const auto &surface:impl_->surfaces)if(surface.range.object==only_object&&corner>=surface.range.first&&corner-surface.range.first<surface.range.count)return true;
    return false;
  };
  return impl_->query.closest(point,options);
}
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

std::optional<SurfaceHit> MaterialSurfaceQuery::raycast_excluding(Vec3 origin, Vec3 direction,
    float maximum,double seconds,std::span<const uint64_t> excluded) const {
  if(!std::isfinite(seconds))throw std::invalid_argument("Invalid visibility animation time");
  SurfaceQueryOptions options;options.max_triangle_tests=65536;
  options.ray_filter=[&](uint32_t triangle,const std::array<float,3> &bary) {
    const size_t index=size_t(triangle)*3;
    auto found=std::upper_bound(impl_->surfaces.begin(),impl_->surfaces.end(),index,
      [](size_t value,const Visibility &surface){return value<surface.range.first;});
    if(found==impl_->surfaces.begin())return false;
    --found;
    return index<found->range.first+found->range.count &&
      std::find(excluded.begin(),excluded.end(),found->range.object)==excluded.end() &&
      found->visible(impl_->query.mesh(),triangle,bary,direction,seconds);
  };
  return impl_->query.raycast(origin,direction,maximum,options);
}
SurfaceMaterialIdentity MaterialSurfaceQuery::material_identity(uint32_t triangle)const{
 const auto corner=size_t(triangle)*3;
 auto found=std::upper_bound(impl_->surfaces.begin(),impl_->surfaces.end(),corner,[](size_t at,const Visibility&v){return at<v.range.first;});
 if(found==impl_->surfaces.begin())throw std::out_of_range("Material query triangle has no range");--found;
 if(corner>=found->range.first+found->range.count)throw std::out_of_range("Material query triangle is outside its range");
 return {found->range.object,found->range.texture_set};
}
uint32_t MaterialSurfaceQuery::logical_triangle(uint32_t triangle)const{
 (void)material_identity(triangle);const auto corner=size_t(triangle)*3;
 auto found=std::upper_bound(impl_->surfaces.begin(),impl_->surfaces.end(),corner,[](size_t at,const Visibility&v){return at<v.range.first;});--found;
 const auto local=corner-found->range.first;
 return found->range.texture_set_proof?found->range.texture_set_proof->logical_corner(local,false)/3:uint32_t(local/3);
}
std::optional<SurfaceHit> MaterialSurfaceQuery::raycast_excluding_materials(Vec3 origin,Vec3 direction,
    float maximum,double seconds,std::span<const SurfaceMaterialIdentity>excluded)const{
 if(!std::isfinite(seconds))throw std::invalid_argument("Invalid visibility animation time");
 SurfaceQueryOptions options;options.max_triangle_tests=65536;
 options.ray_filter=[&](uint32_t triangle,const std::array<float,3>&bary){
  const auto corner=size_t(triangle)*3;
  auto found=std::upper_bound(impl_->surfaces.begin(),impl_->surfaces.end(),corner,[](size_t at,const Visibility&v){return at<v.range.first;});
  if(found==impl_->surfaces.begin())return false;--found;
  const SurfaceMaterialIdentity identity{found->range.object,found->range.texture_set};
  return corner<found->range.first+found->range.count&&std::find(excluded.begin(),excluded.end(),identity)==excluded.end()&&found->visible(impl_->query.mesh(),triangle,bary,direction,seconds);
 };
 return impl_->query.raycast(origin,direction,maximum,options);
}
} // namespace dome
