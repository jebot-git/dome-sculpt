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
#include "current-paint-preview.hpp"
#include "source-vrm1-background.hpp"
#include "sculpt-placed-chunks.hpp"
#include "packed-material-query.hpp"
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
// SAX preflight counts the same bounded source tokens without constructing a
// second retained JSON tree. It covers decoded strings, map/array nodes and
// temporary/copy growth before the ordinary typed parser makes those copies.
struct VisibilityJsonBudget : nlohmann::json_sax<nlohmann::json> {
 size_t &bytes;const std::function<bool()>&cancelled;size_t events=0,depth=0;bool inspect_shader=false,root_object=false,root_shader_key=false,vrm0=false;enum class Shader{Absent,String,Other};Shader shader=Shader::Absent;
 VisibilityJsonBudget(size_t&n,const std::function<bool()>&fn,bool inspect):bytes(n),cancelled(fn),inspect_shader(inspect){}
 void add(size_t n,size_t width=1){if(bytes>512ull*1024*1024||(width&&n>(512ull*1024*1024-bytes)/width))throw std::length_error("Material query typed metadata exceeds prospective budget");bytes+=n*width;}
 bool event(){if(depth>32||++events>100000)throw std::length_error("Material query typed metadata exceeds source depth/token bounds");if((events&255)==0&&cancelled&&cancelled())throw std::runtime_error("Material query JSON preflight cancelled");add(1,256);return true;}
 void value(bool string_value=false,bool vrm=false){if(inspect_shader&&depth==1&&root_shader_key){shader=string_value?Shader::String:Shader::Other;vrm0=vrm;root_shader_key=false;}}
 bool null()override{value();return event();}bool boolean(bool)override{value();return event();}bool number_integer(number_integer_t)override{value();return event();}bool number_unsigned(number_unsigned_t)override{value();return event();}bool number_float(number_float_t,const string_t&)override{value();return event();}
 bool string(string_t&s)override{value(true,s=="VRM/MToon");add(s.size()+1,4);return event();}bool binary(binary_t&s)override{value();add(s.size(),4);return event();}bool key(string_t&s)override{if(inspect_shader&&depth==1)root_shader_key=s=="shader";add(s.size()+1,4);return event();}
 bool start_object(std::size_t)override{if(depth==0)root_object=true;value();event();++depth;return true;}bool end_object()override{if(!depth)throw std::invalid_argument("Invalid typed JSON object depth");--depth;return event();}bool start_array(std::size_t)override{value();event();++depth;return true;}bool end_array()override{if(!depth)throw std::invalid_argument("Invalid typed JSON array depth");--depth;return event();}
 bool parse_error(std::size_t,const std::string&,const nlohmann::detail::exception&)override{return false;}
};
bool visibility_json_budget(size_t&bytes,const std::string&text,const std::function<bool()>&cancelled,bool inspect_shader=false){if(text.empty())return false;if(text.size()>2*1024*1024)throw std::length_error("MToon source metadata exceeds renderer bounds");if(cancelled&&cancelled())throw std::runtime_error("Material query JSON preflight cancelled");VisibilityJsonBudget budget(bytes,cancelled,inspect_shader);if(!nlohmann::json::sax_parse(text,&budget))throw std::invalid_argument("Malformed material query typed JSON");if(inspect_shader&&(!budget.root_object||budget.shader==VisibilityJsonBudget::Shader::Other))throw std::invalid_argument("MToon shader metadata requires an object/string value");return budget.vrm0;}

struct Visibility {
  SurfaceRange range;
  MToonCull cull = MToonCull::Back;
  Material::AlphaMode alpha = Material::AlphaMode::Automatic;
  float cutoff = .5f, factor = 1;
  std::optional<MToonMaterial> toon;
  PaintSourceTexture color, mask, coverage, pbr;
  std::shared_ptr<const detail::PaintOverlayPacket> overlay;
  std::shared_ptr<const detail::CurrentGridPaintPreview> current_grid;
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
      if(maps->current_grid_preview){current_grid=maps->current_grid_preview;current_grid->authenticate_range(mesh,r,true);factor=1;}
      else if (maps->source_only) {
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
      opacity = current_grid?current_grid->sample_alpha(sampling[0],sampling[1]):
        factor * sample(color, color.texcoord!=0?uv(m,triangle,bary,color.texcoord):sampling);
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
  std::unique_ptr<detail::PackedMaterialQuery> query;
  detail::SculptSceneChunks::Ptr chunks;
  std::vector<Visibility> surfaces;
  template<class T> static bool policy_contains(std::span<const T> excluded,const T &value,size_t &metadata_tests,bool bounded=true){
    for(const auto &candidate:excluded){
      if(bounded&&++metadata_tests>262144)throw std::length_error("Material query bounds work budget exceeded");
      if(candidate==value)return true;
    }return false;
  }
  // Surface ranges are authenticated, sorted and contiguous original corner
  // intervals. A false answer is a proof that no primitive in this interval
  // can pass the known object/material policy; arbitrary filters remain in the
  // native query and still consume its single total triangle budget.
  template<class Allowed> bool eligible_interval(uint32_t first,uint32_t count,const Allowed &allowed,size_t &metadata_tests)const {
    auto charge=[&]{if(++metadata_tests>262144)throw std::length_error("Material query bounds work budget exceeded");};
    charge();const size_t begin=size_t(first)*3,end=begin+size_t(count)*3;
    auto found=std::upper_bound(surfaces.begin(),surfaces.end(),begin,
      [&](size_t c,const Visibility &v){charge();return c<v.range.first;});
    if(found!=surfaces.begin())--found;
    for(;found!=surfaces.end()&&found->range.first<end;++found){
      charge();if(found->range.first+found->range.count>begin&&allowed(SurfaceMaterialIdentity{found->range.object,found->range.texture_set}))return true;
    }
    return false;
  }
  uint64_t object_for(uint32_t triangle) const {
    const size_t corner=size_t(triangle)*3;
    auto found=std::upper_bound(surfaces.begin(),surfaces.end(),corner,
        [](size_t value,const Visibility &surface){return value<surface.range.first;});
    if(found==surfaces.begin())throw std::logic_error("Material hit has no actual owning range");
    --found;
    if(corner>=found->range.first+found->range.count)throw std::logic_error("Material hit is outside its actual owning range");
    return found->range.object;
  }
  Impl(const Mesh &mesh, std::span<const SurfaceRange> ranges, const std::function<bool()> &cancelled,
       std::span<const std::array<float,4>> colors,bool explicit_colors, std::span<const std::array<float,2>> paint={},bool per_vertex=false)
      : query(nullptr) {
    auto charge=[](size_t&bytes,size_t count,size_t width){if(width&&count>(SIZE_MAX-bytes)/width)throw std::length_error("Material query metadata forecast overflow");bytes+=count*width;};
    size_t metadata=0;charge(metadata,ranges.size(),16384);charge(metadata,mesh.indices.size(),8);if(metadata>512ull*1024*1024)throw std::length_error("Material query metadata budget exceeded before allocation");
    if(!colors.empty()&&colors.size()!=mesh.positions.size())throw std::invalid_argument("Packed visibility color vertex count differs from geometry");
    for(size_t i=0;i<colors.size();++i){if((i&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Visibility color validation cancelled");for(float c:colors[i])if(!std::isfinite(c)||c<0||c>1)throw std::invalid_argument("Packed visibility requires finite linear unit RGBA");}
    if(!paint.empty()&&paint.size()!=(per_vertex?mesh.positions.size():mesh.indices.size()))throw std::invalid_argument("Visibility paint coordinate alignment differs from geometry");
    for(size_t c=0;c<paint.size();++c){if((c&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Visibility paint validation cancelled");for(float v:paint[c])if(!std::isfinite(v))throw std::invalid_argument("Visibility paint coordinates are nonfinite");}
    size_t end=0;
    for(const auto&range:ranges){
      if(cancelled&&cancelled())throw std::runtime_error("Visibility build cancelled");
      if(end>mesh.indices.size()||range.first!=end||range.count%3||range.count>mesh.indices.size()-end)throw std::invalid_argument("Visibility requires contiguous triangle-aligned surface ranges");
      end+=range.count;size_t color_domain=0;
      if(explicit_colors&&!colors.empty()){
        if(range.first_vertex>mesh.positions.size())throw std::invalid_argument("Packed visibility color origin is outside geometry");size_t last=range.first_vertex;
        for(size_t c=range.first;c<end;++c){if((c&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Visibility color forecast cancelled");const auto v=mesh.indices[c];if(v<range.first_vertex||v>=mesh.positions.size())throw std::invalid_argument("Packed visibility color offset is outside its range");last=std::max(last,size_t(v)+1);}color_domain=last-range.first_vertex;
      }
      if(range.appearance){const auto&appearance=*range.appearance;color_domain=std::max(color_domain,appearance.source_vertices.size());charge(metadata,appearance.source_vertices.size(),sizeof(std::array<float,2>));}
      if(range.material_bindings){if(range.material_bindings->has_mtoon&&!range.pbr_view){if(metadata>512ull*1024*1024)throw std::length_error("Material query metadata exceeds budget");charge(metadata,range.material_bindings->material.copy_working_bytes(512ull*1024*1024-metadata,cancelled),1);}}
      else if(range.appearance&&!range.pbr_view){if(!visibility_json_budget(metadata,range.appearance->mtoon_json,cancelled,true))visibility_json_budget(metadata,range.appearance->material_json,cancelled);}
      // Color copies cover the complete referenced/source vertex DOMAIN,
      // including unused gaps. They are independent of corner paint coordinates.
      charge(metadata,color_domain,sizeof(std::array<float,4>));
      if(metadata>512ull*1024*1024)throw std::length_error("Material query attribute/typed metadata budget exceeded before allocation");
    }
    if(end!=mesh.indices.size())throw std::invalid_argument("Visibility surface ranges are incomplete");
    if(!mesh.indices.empty())query=std::make_unique<detail::PackedMaterialQuery>(mesh,cancelled,detail::PackedMaterialQuery::Limits{},metadata);
    surfaces.reserve(ranges.size());
    for(const auto&range:ranges){if(cancelled&&cancelled())throw std::runtime_error("Visibility build cancelled");surfaces.emplace_back(range,mesh,colors,explicit_colors,paint,per_vertex,cancelled);}

  }
  std::optional<MaterialSurfaceHit> target(Vec3 point,Vec3 direction,float maximum,double seconds,
      uint64_t only,std::span<const uint64_t> excluded,bool ray,bool visibility,
      std::span<const SurfaceMaterialIdentity> excluded_materials={}) const;
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
MaterialSurfaceQuery::MaterialSurfaceQuery(const Mesh &mesh,std::span<const SurfaceRange> ranges,
 const std::function<bool()> &cancelled,std::span<const std::array<float,4>> colors,PaintVertexCoordinates paint,
 std::shared_ptr<const detail::SculptSceneChunks> chunks)
    :impl_(std::make_unique<Impl>(mesh,ranges,cancelled,colors,true,paint.values,true)) {
  if(!chunks)throw std::invalid_argument("Missing actual mixed sculpt scene owner");
  for(const auto &object:chunks->objects()) {
    if(cancelled&&cancelled())throw std::runtime_error("Mixed scene query cancelled");
    if(!object.placed||!object.placed->has_queries())
      throw std::invalid_argument("Mixed query requires worker-built current chunk acceleration");
    if(std::any_of(ranges.begin(),ranges.end(),[&](const auto &range){return range.object==object.object;}))
      throw std::invalid_argument("A mixed object cannot occupy both packed and chunk namespaces");
  }
  // An empty generated-geometry directory carries capture metadata only.
  // Its packed triangles still have the single legacy namespace. Retain a
  // mixed namespace only when it actually contains generated geometry.
  if(!chunks->objects().empty())impl_->chunks=std::move(chunks);
}
MaterialSurfaceQuery::~MaterialSurfaceQuery() = default;
namespace {
bool bounds_ray(const detail::SculptChunkBounds &box,Vec3 p,Vec3 d,float maximum) {
  double lo=0,hi=maximum;
  const std::array<float,3> a{box.minimum.x,box.minimum.y,box.minimum.z},b{box.maximum.x,box.maximum.y,box.maximum.z};
  const std::array<float,3> origin{p.x,p.y,p.z},direction{d.x,d.y,d.z};
  for(size_t k=0;k<3;++k) {
    if(direction[k]==0){if(origin[k]<a[k]||origin[k]>b[k])return false;continue;}
    auto x=(double(a[k])-origin[k])/direction[k],y=(double(b[k])-origin[k])/direction[k];
    if(x>y)std::swap(x,y);lo=std::max(lo,x);hi=std::min(hi,y);if(lo>hi)return false;
  }
  return true;
}
double bounds_distance(const detail::SculptChunkBounds &box,Vec3 p) {
  double result=0;
  const std::array<float,3> a{box.minimum.x,box.minimum.y,box.minimum.z},b{box.maximum.x,box.maximum.y,box.maximum.z},v{p.x,p.y,p.z};
  for(size_t k=0;k<3;++k){auto d=std::max({double(a[k])-v[k],0.,double(v[k])-b[k]});result+=d*d;}
  return result;
}
}
std::optional<MaterialSurfaceHit> MaterialSurfaceQuery::Impl::target(Vec3 point,Vec3 direction,
    float maximum,double seconds,uint64_t only,std::span<const uint64_t> excluded,bool ray,bool visibility,
    std::span<const SurfaceMaterialIdentity> excluded_materials) const {
  if(!finite(point)||!std::isfinite(seconds)||std::isnan(maximum)||maximum<=0||
     std::max({std::abs(point.x),std::abs(point.y),std::abs(point.z)})>1000||
     (ray&&(!finite(direction)||!std::isfinite(length(direction))||length(direction)<1e-12f||!std::isfinite(maximum))))
    throw std::invalid_argument("Invalid mixed material query");
  if(ray)direction=normalized(direction);
  size_t triangle_tests=0,metadata_tests=0;const bool partitioned=query&&query->usage().partitioned;
  auto allowed=[&](uint64_t object){return (!only||only==object)&&!policy_contains(excluded,object,metadata_tests,partitioned);};
  auto material_allowed=[&](SurfaceMaterialIdentity identity){return !policy_contains(excluded_materials,identity,metadata_tests,partitioned);};
  auto count=[&](uint32_t){if(++triangle_tests>65536)throw std::length_error("Mixed scene triangle work budget exceeded");return true;};
  std::optional<MaterialSurfaceHit> best;double best_distance2=INFINITY;
  auto upper_limit=[&]{return best?std::min(maximum,std::nextafter(float(std::sqrt(best_distance2)),INFINITY)):maximum;};
  auto rank=[&](uint64_t object){
    if(chunks){auto i=std::find(chunks->object_order().begin(),chunks->object_order().end(),object);return size_t(i-chunks->object_order().begin());}
    auto i=std::find_if(surfaces.begin(),surfaces.end(),[&](const auto &s){return s.range.object==object;});return size_t(i-surfaces.begin());
  };
  auto accept=[&](MaterialSurfaceHit hit,double distance2){
    bool earlier=false;
    if(best&&distance2==best_distance2) {
      if(hit.object!=best->object)earlier=rank(hit.object)<rank(best->object);
      else if(hit.chunk&&best->chunk)
        earlier=hit.chunk->triangles.at(hit.surface.triangle)<best->chunk->triangles.at(best->surface.triangle);
      else if(!hit.chunk&&!best->chunk)earlier=hit.surface.triangle<best->surface.triangle;
    }
    if(!best||distance2<best_distance2||earlier){best=std::move(hit);best_distance2=distance2;}
  };
  if(query) {
    SurfaceQueryOptions options;options.max_triangle_tests=65536;options.closest_maximum=maximum;
    options.triangle_filter=[&](uint32_t t){
      count(t);const auto corner=size_t(t)*3;
      auto found=std::upper_bound(surfaces.begin(),surfaces.end(),corner,
          [](size_t c,const Visibility &v){return c<v.range.first;});
      if(found==surfaces.begin())return false;--found;
      return corner<found->range.first+found->range.count&&allowed(found->range.object)&&
          material_allowed({found->range.object,found->range.texture_set});
    };
    auto visible=[&](uint32_t t,const std::array<float,3> &bary){
      if(!visibility)return true;
      const auto c=size_t(t)*3;auto found=std::upper_bound(surfaces.begin(),surfaces.end(),c,
        [](size_t v,const Visibility &s){return v<s.range.first;});
      if(found==surfaces.begin())return false;--found;
      return c<found->range.first+found->range.count&&found->visible(query->mesh(),t,bary,direction,seconds,ray);
    };
    options.ray_filter=visible;options.closest_filter=visible;
    detail::PackedMaterialQuery::Eligibility eligible;
    if(only||!excluded.empty()||!excluded_materials.empty())eligible=[&](uint32_t first,uint32_t count){
      return eligible_interval(first,count,[&](SurfaceMaterialIdentity id){return allowed(id.object)&&material_allowed(id);},metadata_tests);
    };
    auto hit=ray?query->raycast_result(point,direction,maximum,options,eligible):query->nearest_result(point,options,eligible);
    if(hit)accept({hit->surface,object_for(hit->surface.triangle),{}},hit->squared_distance);
  }
  if(chunks)for(const auto &object:chunks->objects()) {
    if(!allowed(object.object)||!material_allowed({object.object,0}))continue;
    const auto &material=object.material;
    if(visibility&&material.alpha!=Material::AlphaMode::Opaque&&
       (material.alpha==Material::AlphaMode::Mask?material.color[3]<material.alpha_cutoff:material.color[3]<=0))continue;
    object.placed->visit_bounds([&](const auto &bounds){
      if(++metadata_tests>262144)throw std::length_error("Mixed scene bounds work budget exceeded");
      const auto limit=upper_limit();
      return ray?bounds_ray(bounds,point,direction,limit):bounds_distance(bounds,point)<=std::nextafter(best?best_distance2:double(maximum)*maximum,INFINITY);
    },[&](const detail::SculptPlacedChunk::Ptr &chunk){
      SurfaceQueryOptions options;options.max_triangle_tests=65536;
      options.closest_maximum=upper_limit();
      options.triangle_filter=count;
      const auto &frame=chunk->placement_frame();
      const auto local_direction=frame.inverse().direction({direction.x,direction.y,direction.z});
      if(ray&&!material.double_sided)options.ray_filter=[&](uint32_t t,const std::array<float,3>&){
        const auto &m=*chunk->geometry_owner();const auto c=size_t(t)*3;
        const auto a=m.positions[m.indices[c]],b=m.positions[m.indices[c+1]],v=m.positions[m.indices[c+2]];
        const double ux=double(b.x)-a.x,uy=double(b.y)-a.y,uz=double(b.z)-a.z;
        const double vx=double(v.x)-a.x,vy=double(v.y)-a.y,vz=double(v.z)-a.z;
        return (uy*vz-uz*vy)*local_direction[0]+(uz*vx-ux*vz)*local_direction[1]+(ux*vy-uy*vx)*local_direction[2]<0;
      };
      const auto limit=upper_limit();
      auto hit=ray?chunk->query()->raycast_result(point,direction,limit,frame.world(),options):chunk->query()->nearest_result(point,frame.world(),options);
      if(hit)accept({hit->surface,object.object,chunk->source()},hit->squared_distance);
    });
  }
  return best;
}
MaterialSurfaceHit MaterialSurfaceQuery::closest_target(Vec3 p,uint64_t object) const {
  auto hit=impl_->target(p,{},INFINITY,0,object,{},false,false);
  if(!hit)throw std::runtime_error("No eligible source surface triangle");return *hit;
}
std::optional<MaterialSurfaceHit> MaterialSurfaceQuery::nearby_target(Vec3 p,float maximum,double seconds) const {
  return impl_->target(p,{},maximum,seconds,0,{},false,true);
}
std::optional<MaterialSurfaceHit> MaterialSurfaceQuery::raycast_target(Vec3 p,Vec3 d,float maximum,double seconds,uint64_t object) const {
  return impl_->target(p,d,maximum,seconds,object,{},true,true);
}
std::optional<MaterialSurfaceHit> MaterialSurfaceQuery::raycast_excluding_target(Vec3 p,Vec3 d,float maximum,double seconds,std::span<const uint64_t> excluded) const {
  return impl_->target(p,d,maximum,seconds,0,excluded,true,true);
}
std::optional<MaterialSurfaceHit> MaterialSurfaceQuery::raycast_excluding_materials_target(Vec3 p,Vec3 d,
    float maximum,double seconds,std::span<const SurfaceMaterialIdentity> excluded) const {
  return impl_->target(p,d,maximum,seconds,0,{},true,true,excluded);
}
SurfaceHit MaterialSurfaceQuery::closest(Vec3 point) const {
  if(impl_->chunks)throw std::logic_error("A mixed scene requires a typed material hit");
  if(!impl_->query)throw std::logic_error("No packed material geometry"); return impl_->query->closest(point); }
std::optional<SurfaceHit> MaterialSurfaceQuery::nearby(Vec3 point,float maximum,double seconds) const {
  if(impl_->chunks)throw std::logic_error("A mixed scene requires a typed material hit");
  if(!impl_->query)throw std::logic_error("No packed material geometry");
  if(!std::isfinite(maximum)||maximum<=0||!std::isfinite(seconds))throw std::invalid_argument("Invalid direct selection query");
  SurfaceQueryOptions options;options.max_triangle_tests=65536;options.closest_maximum=maximum;
  options.closest_filter=[&](uint32_t triangle,const std::array<float,3> &bary){
    const size_t corner=size_t(triangle)*3;
    auto found=std::upper_bound(impl_->surfaces.begin(),impl_->surfaces.end(),corner,[](size_t value,const Visibility &surface){return value<surface.range.first;});
    if(found==impl_->surfaces.begin())return false;
    --found;
    // A controller can touch either side or enter a surface slightly. Keep
    // alpha/UV/MToon animation, but don't impose camera backface culling.
    return corner<found->range.first+found->range.count&&found->visible(impl_->query->mesh(),triangle,bary,{},seconds,false);
  };
  return impl_->query->nearest(point,options);

}
SurfaceHit MaterialSurfaceQuery::closest(Vec3 point,uint64_t only_object) const {
  if(impl_->chunks)throw std::logic_error("A mixed scene requires a typed material hit");
  if(!impl_->query)throw std::logic_error("No packed material geometry");
  SurfaceQueryOptions options;options.max_triangle_tests=65536;
  options.triangle_filter=[&](uint32_t triangle) {
    const size_t corner=size_t(triangle)*3;
    for(const auto &surface:impl_->surfaces)if(surface.range.object==only_object&&corner>=surface.range.first&&corner-surface.range.first<surface.range.count)return true;
    return false;
  };
  size_t metadata_tests=0;detail::PackedMaterialQuery::Eligibility eligible=[&](uint32_t first,uint32_t count){
    return impl_->eligible_interval(first,count,[&](SurfaceMaterialIdentity id){return id.object==only_object;},metadata_tests);
  };
  return impl_->query->closest(point,options,eligible);
}
std::optional<SurfaceHit> MaterialSurfaceQuery::raycast(Vec3 origin, Vec3 direction,
                                                       float maximum, double seconds,uint64_t only_object) const {
  if(impl_->chunks||!impl_->query)throw std::logic_error("A mixed scene requires a typed material hit");
  if (!std::isfinite(seconds)) throw std::invalid_argument("Invalid visibility animation time");
  SurfaceQueryOptions options; options.max_triangle_tests = 65536;
  options.ray_filter = [&](uint32_t triangle, const std::array<float,3> &bary) {
    auto index = size_t(triangle)*3;
    auto found = std::upper_bound(impl_->surfaces.begin(), impl_->surfaces.end(), index,
                                [](size_t value, const Visibility &surface) { return value < surface.range.first; });
    if (found == impl_->surfaces.begin()) return false;
    --found;
    return (!only_object || found->range.object==only_object) && index < found->range.first + found->range.count &&
           found->visible(impl_->query->mesh(), triangle, bary, direction, seconds);
  };
  size_t metadata_tests=0;detail::PackedMaterialQuery::Eligibility eligible;
  if(only_object)eligible=[&](uint32_t first,uint32_t count){
    return impl_->eligible_interval(first,count,[&](SurfaceMaterialIdentity id){return id.object==only_object;},metadata_tests);
  };
  return impl_->query->raycast(origin, direction, maximum, options,eligible);
}

std::optional<SurfaceHit> MaterialSurfaceQuery::raycast_excluding(Vec3 origin, Vec3 direction,
    float maximum,double seconds,std::span<const uint64_t> excluded) const {
  if(impl_->chunks||!impl_->query)throw std::logic_error("A mixed scene requires a typed material hit");
  if(!std::isfinite(seconds))throw std::invalid_argument("Invalid visibility animation time");
  size_t metadata_tests=0;const bool partitioned=impl_->query->usage().partitioned;SurfaceQueryOptions options;options.max_triangle_tests=65536;
  options.ray_filter=[&](uint32_t triangle,const std::array<float,3> &bary) {
    const size_t index=size_t(triangle)*3;
    auto found=std::upper_bound(impl_->surfaces.begin(),impl_->surfaces.end(),index,
      [](size_t value,const Visibility &surface){return value<surface.range.first;});
    if(found==impl_->surfaces.begin())return false;
    --found;
    return index<found->range.first+found->range.count &&
      !Impl::policy_contains(excluded,found->range.object,metadata_tests,partitioned) &&
      found->visible(impl_->query->mesh(),triangle,bary,direction,seconds);
  };
  detail::PackedMaterialQuery::Eligibility eligible;
  if(!excluded.empty())eligible=[&](uint32_t first,uint32_t count){
    return impl_->eligible_interval(first,count,[&](SurfaceMaterialIdentity id){return !Impl::policy_contains(excluded,id.object,metadata_tests);},metadata_tests);
  };
  return impl_->query->raycast(origin,direction,maximum,options,eligible);
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
 if(impl_->chunks||!impl_->query)throw std::logic_error("A mixed scene requires a typed material hit");
 if(!std::isfinite(seconds))throw std::invalid_argument("Invalid visibility animation time");
 size_t metadata_tests=0;const bool partitioned=impl_->query->usage().partitioned;SurfaceQueryOptions options;options.max_triangle_tests=65536;
 options.ray_filter=[&](uint32_t triangle,const std::array<float,3>&bary){
  const auto corner=size_t(triangle)*3;
  auto found=std::upper_bound(impl_->surfaces.begin(),impl_->surfaces.end(),corner,[](size_t at,const Visibility&v){return at<v.range.first;});
  if(found==impl_->surfaces.begin())return false;--found;
  const SurfaceMaterialIdentity identity{found->range.object,found->range.texture_set};
  return corner<found->range.first+found->range.count&&!Impl::policy_contains(excluded,identity,metadata_tests,partitioned)&&found->visible(impl_->query->mesh(),triangle,bary,direction,seconds);
 };
 detail::PackedMaterialQuery::Eligibility eligible;
 if(!excluded.empty())eligible=[&](uint32_t first,uint32_t count){
  return impl_->eligible_interval(first,count,[&](SurfaceMaterialIdentity id){return !Impl::policy_contains(excluded,id,metadata_tests);},metadata_tests);
 };
 return impl_->query->raycast(origin,direction,maximum,options,eligible);
}
} // namespace dome
