// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/material-bindings.hpp"
#include "dome/color-preparation.hpp"
#include "dome/creator-archive.hpp"
#include "dome/paint.hpp"
#include "dome/deformation.hpp"
#include "dome/humanoid-creator.hpp"
#include <algorithm>
#include <set>
namespace dome {
namespace {
constexpr size_t role_count=size_t(MToonSlot::Count);
void roles_valid(const std::optional<MToonMaterial> &material,
                 const std::array<PaintSourceTexture,role_count> &roles) {
  if(material) material->validate();
  for(size_t role=0;role<role_count;++role) {
    const auto &view=roles[role];view.validate();
    if((material && material->textures[role].index>=0)!=bool(view.image))
      throw std::invalid_argument("Resolved material role table is incomplete");
    if(view.texcoord!=0 || view.uv_transform!=std::array<float,6>{1,0,0,1,0,0})
      throw std::invalid_argument("Resolved role UV transforms belong to the typed material");
  }
}
nlohmann::json encode_roles(const std::array<PaintSourceTexture,role_count> &roles,PaintImageArchive &images) {
  auto output=nlohmann::json::array();
  for(const auto &view:roles) output.push_back({
    {"image",view.image?nlohmann::json(images.add(view.image)):nlohmann::json(nullptr)},
    {"wrap",{view.wrap_s,view.wrap_t}}, {"filter",{view.min_filter,view.mag_filter}}});
  return output;
}
std::array<PaintSourceTexture,role_count> decode_roles(const nlohmann::json &input,const PaintImageArchive &images) {
  if(!input.is_array() || input.size()!=role_count) throw std::runtime_error("Invalid resolved material role count");
  std::array<PaintSourceTexture,role_count> output;
  for(size_t role=0;role<role_count;++role) {
    auto &view=output[role];const auto &value=input[role];
    if(!value.at("image").is_null()) {
      const auto &index=value.at("image");
      if((!index.is_number_unsigned() && (!index.is_number_integer() || index.get<int64_t>()<0)) ||
         index.get<uint64_t>()>UINT32_MAX) throw std::runtime_error("Invalid material image identity");
      view.image=images.at(index.get<uint32_t>());
    }
    const auto &wrap=value.at("wrap"),&filter=value.at("filter");
    if(!wrap.is_array() || wrap.size()!=2 || !filter.is_array() || filter.size()!=2)
      throw std::runtime_error("Invalid resolved material sampler");
    view.wrap_s=wrap[0].get<int>();view.wrap_t=wrap[1].get<int>();
    view.min_filter=filter[0].get<int>();view.mag_filter=filter[1].get<int>();view.validate();
  }
  return output;
}
size_t metadata_bytes(const std::optional<MToonMaterial> &material) {
  if(!material)return 0;
  auto bytes=material->encode().dump().size();
  if(bytes>2*1024*1024) throw std::length_error("Resolved material metadata exceeds budget");
  return bytes;
}
size_t index(const nlohmann::json &value,size_t size) {
  if((!value.is_number_unsigned() && (!value.is_number_integer() || value.get<int64_t>()<0)) ||
     value.get<uint64_t>()>=size) throw std::runtime_error("Invalid resolved material archive identity");
  return size_t(value.get<uint64_t>());
}
}
void SourceNativeMaterial::validate() const {
  auto unit=[](float value){return std::isfinite(value) && value>=0 && value<=1;};
  if(!std::all_of(color.begin(),color.end(),unit) || !std::all_of(emission.begin(),emission.end(),unit) ||
     alpha<0 || alpha>3 || !unit(alpha_cutoff))
    throw std::invalid_argument("Invalid native factor baseline for resolved material");
}
void SourceMaterialBindings::validate(bool deep) const {
  if(size_t(normal_frame)>1 || size_t(animation_policy)>1 ||
     !std::isfinite(freeze_seconds) || freeze_seconds<0 || freeze_seconds>1e9 ||
     (animation_frozen && animation_policy!=SourceUvAnimationPolicy::FreezeAtTime))
    throw std::invalid_argument("Invalid derived material frame or animation provenance");
  if(sampling_reparameterized && !uv_origin)throw std::invalid_argument("Reparameterized sampling requires retained original provenance");
  if(color_preparation)color_preparation->validate(deep);
  native_reference.validate();
  roles_valid(has_mtoon?std::optional<MToonMaterial>(material):std::nullopt,roles);
  if(animation_frozen && has_mtoon &&
     (material.parameters.scroll!=std::array<float,2>{} || material.parameters.rotation_speed!=0))
    throw std::invalid_argument("An explicitly frozen material cannot retain active UV animation");
  if(uv_origin) {
    if(uv_origin->paint&&uv_origin->paint->surface_diagnostics().policy==PaintUvPolicy::PolygonAuthored)throw std::invalid_argument("Source UV origin paint requires explicit strict conversion; polygon authority cannot be inferred");
    if(!uv_origin->paint || uv_origin->paint->in_stroke())
      throw std::invalid_argument("Derived UV material has no finished immutable source");
    roles_valid(uv_origin->material,uv_origin->roles);
    if(uv_origin->deformation && deep)uv_origin->deformation->validate(uv_origin->paint->mesh());
    if(uv_origin->creator_kind.empty()) {
      if(uv_origin->creator_version || !uv_origin->creator_parameters.empty())throw std::invalid_argument("Partial creator frame provenance");
    } else {
      if(uv_origin->creator_kind!="vroid-shape" || uv_origin->creator_version!=1 || uv_origin->creator_parameters.size()>64*1024)throw std::invalid_argument("Unknown creator frame provenance");
      if(deep)HumanoidControls::decode(nlohmann::json::parse(uv_origin->creator_parameters)).validate();
    }
    if(uv_origin->appearance && uv_origin->appearance->source_vertices.size()!=uv_origin->paint->mesh().positions.size())
      throw std::invalid_argument("Original UV material source remap differs from its retained paint mesh");
  }
  if(normal_frame==SourceMaterialNormalFrame::OriginalAttributes && !normal_frames)throw std::invalid_argument("Original attributes require explicit normal frames");
  if(normal_frames) {
    if(normal_frame!=SourceMaterialNormalFrame::OriginalAttributes)throw std::invalid_argument("Original frame payload cannot reinterpret legacy converted normal pixels");
    normal_frames->validate(deep);
    if(!uv_origin || !uv_origin->paint)throw std::invalid_argument("Normal frames require retained native rest authority");
    const auto &mesh=uv_origin->paint->mesh();
    for(const auto &f:{normal_frames->pbr,normal_frames->mtoon,normal_frames->canonical})if(f) {
      if(f->origin_vertex_count!=mesh.positions.size() || f->normals.size()!=mesh.indices.size())throw std::invalid_argument("Normal frame origin geometry differs");
      if(deep && f->origin_vertices!=mesh.indices)throw std::invalid_argument("Normal frames must retain exact ordered origin corners");
      if(!f->source_vertices.empty()) {
        if(!uv_origin->appearance || !uv_origin->appearance->asset)throw std::invalid_argument("Original source corner frames require retained primitive authority");
        if(deep) {
          const auto &a=*uv_origin->appearance;const ImportedPrimitive *primitive=nullptr;
          for(const auto &p:a.asset->primitives)if(p.source_mesh==a.source_mesh && p.source_primitive==a.source_primitive){if(primitive)throw std::invalid_argument("Normal frame source primitive is ambiguous");primitive=&p;}
          if(!primitive || primitive->mesh.positions.size()!=f->source_vertex_count || primitive->mesh.indices.size()!=f->source_corner_count)throw std::invalid_argument("Normal frame source domain differs");
          for(size_t c=0;c<f->source_vertices.size();++c)if(f->source_vertices[c]!=a.source_vertices.at(mesh.indices[c]) || primitive->mesh.indices.at(f->source_corners[c])!=f->source_vertices[c])throw std::invalid_argument("Normal frame source corner provenance differs");
        }
      }
    }
    std::set<uint32_t> source_targets;
    for(const auto &t:normal_frames->targets) {
      if(t.origin_slot && (!uv_origin->deformation || !uv_origin->deformation->morphs || *t.origin_slot>=uv_origin->deformation->morphs->targets.size()))throw std::invalid_argument("Normal frame origin target is unavailable");
      if(t.source_target && !source_targets.insert(*t.source_target).second)throw std::invalid_argument("Ambiguous original source morph identity");
      if(t.source_target && (!uv_origin->appearance || !t.origin_slot || *t.origin_slot>=uv_origin->appearance->source_morph_targets.size() || uv_origin->appearance->source_morph_targets[*t.origin_slot]!=*t.source_target))throw std::invalid_argument("Normal frame source target provenance differs");
    }
  }
}
uint32_t SourceMaterialArchive::add(std::shared_ptr<const SourceMaterialBindings> value,bool deep) {
  if(!value) throw std::invalid_argument("Missing resolved material holder");
  auto found=std::find(bindings_.begin(),bindings_.end(),value);
  if(found!=bindings_.end())return uint32_t(found-bindings_.begin());
  if(bindings_.size()>=256)throw std::length_error("Resolved material archive count exceeds budget");
  value->validate(deep);
  if(value->color_preparation && value->color_preparation->original_material)
    add(value->color_preparation->original_material,deep);
  if(bindings_.size()>=256)throw std::length_error("Resolved material archive count exceeds budget");
  if(value->uv_origin && std::find(origins_.begin(),origins_.end(),value->uv_origin)==origins_.end()) {
    if(origins_.size()>=256)throw std::length_error("Original UV source archive count exceeds budget");
    origins_.push_back(value->uv_origin);
  }
  bindings_.push_back(std::move(value));return uint32_t(bindings_.size()-1);
}
const std::shared_ptr<const SourceMaterialBindings> &SourceMaterialArchive::at(uint32_t id) const {
  return bindings_.at(id);
}
nlohmann::json SourceMaterialArchive::encode(PaintImageArchive &images,CreatorAssetArchive &models) const {
  const bool prepared=std::any_of(bindings_.begin(),bindings_.end(),[](const auto &b){return bool(b->color_preparation);});
  const bool extended=prepared || std::any_of(bindings_.begin(),bindings_.end(),[](const auto &b){return bool(b->normal_frames) || (b->uv_origin && !b->sampling_reparameterized);}) || std::any_of(origins_.begin(),origins_.end(),[](const auto &o){return bool(o->deformation)||!o->creator_kind.empty();});
  NormalFrameArchive frames;
  nlohmann::json output={{"version",prepared?3:extended?2:1},{"paints",nlohmann::json::array()},{"origins",nlohmann::json::array()},{"bindings",nlohmann::json::array()}};
  std::vector<std::shared_ptr<const PaintedMesh>> paints;
  auto paint_id=[&](const std::shared_ptr<const PaintedMesh> &paint){
    auto found=std::find(paints.begin(),paints.end(),paint);
    if(found!=paints.end())return size_t(found-paints.begin());
    if(paints.size()>=256)throw std::length_error("Retained paint archive count exceeds budget");
    const auto id=paints.size();paints.push_back(paint);output["paints"].push_back(paint->encode(&images));return id;
  };
  std::vector<std::shared_ptr<const SourceColorPreparation>> preparations;
  if(prepared) {
    output["color_preparations"]=nlohmann::json::array();
    for(const auto &binding:bindings_)if(binding->color_preparation && std::find(preparations.begin(),preparations.end(),binding->color_preparation)==preparations.end()) {
      const auto &p=*binding->color_preparation;p.validate();preparations.push_back(binding->color_preparation);
      auto before=std::find(bindings_.begin(),bindings_.end(),p.original_material);
      if(p.original_material && before==bindings_.end())throw std::logic_error("Missing color preparation original binding");
      output["color_preparations"].push_back({{"version",1},{"projection",encode_color_projection(*p.projection,images)},
        {"initial_projection",encode_color_projection(*p.initial_projection,images)},{"receipt",p.receipt.encode()},
        {"paint",paint_id(p.original_paint)},
        {"appearance",p.original_appearance?encode_creator_appearance(*p.original_appearance,models,images):nlohmann::json(nullptr)},
        {"deformation",p.original_deformation?p.original_deformation->encode(p.original_paint->mesh()):nlohmann::json(nullptr)},
        {"before_binding",p.original_material?nlohmann::json(size_t(before-bindings_.begin())):nlohmann::json(nullptr)}});
    }
  }
  size_t metadata=0;
  for(const auto &origin:origins_) {
    metadata+=metadata_bytes(origin->material);
    if(metadata>16*1024*1024)throw std::length_error("Scene resolved material metadata exceeds budget");
    output["origins"].push_back({{"paint",paint_id(origin->paint)},
      {"appearance",origin->appearance?encode_creator_appearance(*origin->appearance,models,images):nlohmann::json(nullptr)},
      {"material",origin->material?origin->material->encode():nlohmann::json(nullptr)},
      {"roles",encode_roles(origin->roles,images)}});
    if(extended){output["origins"].back()["deformation"]=origin->deformation?origin->deformation->encode(origin->paint->mesh()):nlohmann::json(nullptr);
      output["origins"].back()["creator"]=origin->creator_kind.empty()?nlohmann::json(nullptr):nlohmann::json{{"kind",origin->creator_kind},{"version",origin->creator_version},{"parameters",origin->creator_parameters}};}
  }
  for(const auto &binding:bindings_) {
    binding->validate();metadata+=metadata_bytes(binding->has_mtoon?std::optional<MToonMaterial>(binding->material):std::nullopt);
    if(metadata>16*1024*1024)throw std::length_error("Scene resolved material metadata exceeds budget");
    auto origin=std::find(origins_.begin(),origins_.end(),binding->uv_origin);
    output["bindings"].push_back({{"material",binding->has_mtoon?binding->material.encode():nlohmann::json(nullptr)},
      {"roles",encode_roles(binding->roles,images)}, {"normal_frame",binding->normal_frame==SourceMaterialNormalFrame::UvDerivative?"uv-derivative":"original-attributes"},
      {"animation_policy",int(binding->animation_policy)}, {"freeze_seconds",binding->freeze_seconds},
      {"animation_frozen",binding->animation_frozen},
      {"native_reference",{{"color",binding->native_reference.color},{"emission",binding->native_reference.emission},
        {"alpha",binding->native_reference.alpha},{"alpha_cutoff",binding->native_reference.alpha_cutoff},
        {"double_sided",binding->native_reference.double_sided}}},
      {"origin",binding->uv_origin?nlohmann::json(size_t(origin-origins_.begin())):nlohmann::json(nullptr)}});
    if(prepared) {
      const auto found=std::find(preparations.begin(),preparations.end(),binding->color_preparation);
      output["bindings"].back()["color_preparation"]=binding->color_preparation?nlohmann::json(size_t(found-preparations.begin())):nlohmann::json(nullptr);
    }
    if(extended){output["bindings"].back()["frames"]=binding->normal_frames?nlohmann::json(frames.add(binding->normal_frames)):nlohmann::json(nullptr);output["bindings"].back()["sampling_reparameterized"]=binding->sampling_reparameterized;}
  }
  if(extended)output["normal_frames"]=frames.encode();
  return output;
}
size_t SourceMaterialArchive::deformation_entries(const nlohmann::json &input) {
  size_t entries=0;
  if(input.at("version")==1)return 0;
  if((input.at("version")!=2 && input.at("version")!=3) || !input.at("origins").is_array() || input.at("origins").size()>256)
    throw std::runtime_error("Invalid original deformation archive");
  auto add=[&](size_t n){if(n>8'000'000-entries)throw std::length_error("Original deformation archive exceeds scene budget");entries+=n;};
  auto count=[&](const nlohmann::json &d) {
    if(d.is_null())return;
    if(!d.at("pose").is_array() || d.at("pose").size()>256 || !d.at("morph_weights").is_array() || d.at("morph_weights").size()>128)
      throw std::runtime_error("Invalid original deformation preview dimensions");
    add(d.at("pose").size());add(d.at("morph_weights").size());
    if(!d.at("skin").is_null()) {
      const auto &vertices=d.at("skin").at("vertices");
      if(!vertices.is_array() || vertices.size()>2'000'000)throw std::runtime_error("Invalid original skin count");
      for(const auto &v:vertices){if(!v.is_array() || v.size()>32)throw std::runtime_error("Invalid original skin influences");add(v.size());}
    }
    if(!d.at("morphs").is_null()) {
      const auto &targets=d.at("morphs").at("targets");
      if(!targets.is_array() || targets.size()>128)throw std::runtime_error("Invalid original morph count");
      for(const auto &t:targets){const auto &v=t.at("delta");if(!v.is_array() || v.size()>2'000'000)throw std::runtime_error("Invalid original morph positions");add(v.size());}
    }
  };
  for(const auto &origin:input.at("origins"))count(origin.at("deformation"));
  if(input.at("version")==3) {
    const auto &preparations=input.at("color_preparations");
    if(!preparations.is_array() || preparations.empty() || preparations.size()>256)throw std::runtime_error("Invalid color preparation archive");
    for(const auto &p:preparations)count(p.at("deformation"));
  }
  return entries;
}
SourceMaterialArchive SourceMaterialArchive::decode(const nlohmann::json &input,const PaintImageArchive &images,
                                                   const CreatorAssetArchive &models) {
  if(!input.at("version").is_number_integer() || (input.at("version")!=1 && input.at("version")!=2 && input.at("version")!=3) || !input.at("paints").is_array() || input.at("paints").size()>256 || !input.at("origins").is_array() || input.at("origins").size()>256 ||
     !input.at("bindings").is_array() || input.at("bindings").size()>256)
    throw std::runtime_error("Invalid resolved material archive");
  const bool prepared=input.at("version")==3;
  const bool extended=input.at("version")!=1;
  if(!prepared) {
    if(input.contains("color_preparations"))throw std::runtime_error("Color preparations require material archive3");
    for(const auto &b:input.at("bindings"))if(b.contains("color_preparation"))throw std::runtime_error("Color preparation references require material archive3");
    for(const auto &p:input.at("paints")) {
      if(p.contains("color_projection") || (p.contains("baked") && p.at("baked").at("version")!=1))throw std::runtime_error("Prepared paint requires material archive3");
      for(const auto &l:p.at("layers"))if(l.contains("pbr_color_tiles"))throw std::runtime_error("Prepared paint layers require material archive3");
    }
  }
  if(!extended){if(input.contains("normal_frames"))throw std::runtime_error("Normal frames require material archive2");for(const auto &o:input.at("origins"))if(o.contains("deformation")||o.contains("creator"))throw std::runtime_error("Native frame origin requires material archive2");for(const auto &b:input.at("bindings"))if(b.contains("frames") || b.contains("sampling_reparameterized"))throw std::runtime_error("Normal frame references require material archive2");}
  deformation_entries(input);
  NormalFrameArchive frames;if(extended)frames=NormalFrameArchive::decode(input.at("normal_frames"));
  // Preflight all implicit SurfaceMap allocations before constructing any.
  size_t texels=0,vertices=0,tile_bytes=0,metadata=0;
  for(const auto &paint:input.at("paints")) {
    const auto &resolution=paint.at("resolution");
    if(!resolution.is_number_unsigned() && (!resolution.is_number_integer() || resolution.get<int64_t>()<64))
      throw std::runtime_error("Invalid original UV paint resolution");
    auto r=resolution.get<uint64_t>();
    if(r<64 || r>2048 || (r&(r-1)))throw std::runtime_error("Invalid original UV paint resolution");
    texels+=size_t(r)*r;
    const auto &positions=paint.at("mesh").at("positions");
    if(!positions.is_binary())throw std::runtime_error("Invalid original UV geometry");
    vertices+=positions.get_binary().size()/12;
    const auto &layers=paint.at("layers");
    if(!layers.is_array() || layers.empty() || layers.size()>16)throw std::runtime_error("Invalid original UV layers");
    for(const auto &layer:layers) {
      if(!layer.at("tiles").is_array())throw std::runtime_error("Invalid original UV tiles");
      tile_bytes+=layer.at("tiles").size()*sizeof(PaintTile);
      if(layer.contains("pbr_color_tiles")) {
        if(!layer.at("pbr_color_tiles").is_array())throw std::runtime_error("Invalid original PBR color tiles");
        tile_bytes+=layer.at("pbr_color_tiles").size()*sizeof(PaintColorTile);
      }
    }
    if(paint.contains("baked"))for(const auto *key:{"color","orm","emission","normal","coverage"}) {
      const auto &bytes=paint.at("baked").at(key);
      if(!bytes.is_binary())throw std::runtime_error("Invalid original UV baked buffer");
      tile_bytes+=bytes.get_binary().size();
    }
    if(paint.contains("baked") && paint.at("baked").contains("pbr_color")) {
      const auto &bytes=paint.at("baked").at("pbr_color");
      if(!bytes.is_binary())throw std::runtime_error("Invalid original baked PBR buffer");
      tile_bytes+=bytes.get_binary().size();
    }
    if(texels>16*1024*1024 || vertices>8'000'000 || tile_bytes>256*1024*1024)
      throw std::length_error("Original UV source archive exceeds scene budgets");
  }
  SourceMaterialArchive output;
  std::vector<std::shared_ptr<const PaintedMesh>> paints;
  for(const auto &paint:input.at("paints"))paints.push_back(PaintedMesh::decode(paint,&images));
  for(const auto &value:input.at("origins")) {
    auto origin=std::make_shared<SourceUvOrigin>();
    origin->paint=paints.at(index(value.at("paint"),paints.size()));
    if(!value.at("appearance").is_null())origin->appearance=decode_creator_appearance(value.at("appearance"),models);
    if(!value.at("material").is_null())origin->material=MToonMaterial::decode(value.at("material"));
    metadata+=metadata_bytes(origin->material);
    if(metadata>16*1024*1024)throw std::length_error("Scene resolved material metadata exceeds budget");
    origin->roles=decode_roles(value.at("roles"),images);
    if(extended){if(!value.at("deformation").is_null())origin->deformation=std::make_shared<const DeformationState>(DeformationState::decode(value.at("deformation"),origin->paint->mesh()));
      if(!value.at("creator").is_null()){const auto &c=value.at("creator");origin->creator_kind=c.at("kind").get<std::string>();if(!c.is_object() || c.size()!=3 || !c.at("version").is_number_integer() || c.at("version")!=1)throw std::runtime_error("Unsupported creator frame version");origin->creator_version=1;origin->creator_parameters=c.at("parameters").get<std::string>();}}
    output.origins_.push_back(std::move(origin));
  }
  std::vector<std::shared_ptr<const SourceColorPreparation>> preparations;
  if(prepared)preparations.resize(input.at("color_preparations").size());
  for(const auto &value:input.at("bindings")) {
    const auto &frame=value.at("normal_frame");
    if(!frame.is_string() || (frame.get_ref<const std::string&>()!="uv-derivative" && (!extended || frame.get_ref<const std::string&>()!="original-attributes")))throw std::runtime_error("Unsupported derived material normal frame");
    auto binding=std::make_shared<SourceMaterialBindings>();
    if(frame.get_ref<const std::string&>()=="original-attributes")binding->normal_frame=SourceMaterialNormalFrame::OriginalAttributes;
    if(extended && !value.at("frames").is_null())binding->normal_frames=frames.at(uint32_t(index(value.at("frames"),frames.sets().size())));
    binding->has_mtoon=!value.at("material").is_null();
    if(binding->has_mtoon)binding->material=MToonMaterial::decode(value.at("material"));
    metadata+=metadata_bytes(binding->has_mtoon?std::optional<MToonMaterial>(binding->material):std::nullopt);
    if(metadata>16*1024*1024)throw std::length_error("Scene resolved material metadata exceeds budget");
    binding->roles=decode_roles(value.at("roles"),images);
    int policy=value.at("animation_policy").get<int>();
    if(policy<0 || policy>1)throw std::runtime_error("Invalid derived material animation policy");
    binding->animation_policy=SourceUvAnimationPolicy(policy);
    binding->freeze_seconds=value.at("freeze_seconds").get<double>();
    binding->animation_frozen=value.at("animation_frozen").get<bool>();
    const auto &native=value.at("native_reference");
    binding->native_reference.color=native.at("color").get<std::array<float,4>>();
    binding->native_reference.emission=native.at("emission").get<std::array<float,3>>();
    binding->native_reference.alpha=native.at("alpha").get<int>();
    binding->native_reference.alpha_cutoff=native.at("alpha_cutoff").get<float>();
    binding->native_reference.double_sided=native.at("double_sided").get<bool>();
    if(!value.at("origin").is_null())binding->uv_origin=output.origins_.at(index(value.at("origin"),output.origins_.size()));
    binding->sampling_reparameterized=extended?value.at("sampling_reparameterized").get<bool>():bool(binding->uv_origin);
    if(prepared && !value.at("color_preparation").is_null()) {
      const auto id=index(value.at("color_preparation"),preparations.size());
      if(!preparations[id]) {
        const auto &v=input.at("color_preparations").at(id);
        if(!v.is_object() || v.size()!=8 || !v.at("version").is_number_integer() || v.at("version")!=1)throw std::runtime_error("Unsupported retained color preparation");
        auto p=std::make_shared<SourceColorPreparation>();
        p->projection=decode_color_projection(v.at("projection"),images);p->initial_projection=decode_color_projection(v.at("initial_projection"),images);
        p->receipt=ColorPreparationReceipt::decode(v.at("receipt"));p->original_paint=paints.at(index(v.at("paint"),paints.size()));
        if(!v.at("appearance").is_null())p->original_appearance=decode_creator_appearance(v.at("appearance"),models);
        if(!v.at("deformation").is_null())p->original_deformation=std::make_shared<const DeformationState>(DeformationState::decode(v.at("deformation"),p->original_paint->mesh()));
        if(!v.at("before_binding").is_null())p->original_material=output.bindings_.at(index(v.at("before_binding"),output.bindings_.size()));
        p->validate();preparations[id]=std::move(p);
      }
      binding->color_preparation=preparations[id];
    }
    binding->validate();output.bindings_.push_back(std::move(binding));
  }
  if(std::any_of(preparations.begin(),preparations.end(),[](const auto &p){return !p;}))throw std::runtime_error("Unreferenced color preparation record");
  return output;
}
} // namespace dome
