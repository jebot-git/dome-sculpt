// SPDX-License-Identifier: GPL-3.0-or-later
#include "affine-matrix.hpp"
#include "emission-authority.hpp"
#include "emission-export.hpp"
#include "affine-factor.hpp"
#include "dome/source-placement.hpp"
#include "dome/material-bindings.hpp"
#include "dome/material-visibility.hpp"
#include "dome/scene.hpp"
#include "dome/gltf-export.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/source-maps.hpp"
#include "dome/tangents.hpp"
#include "io.hpp"
#include "gltf-document.hpp"
#include "raw-source-emission.hpp"
#include "native-authoring-compiler.hpp"
#include "normal-frame-export.hpp"
#include "vrm1-normal-export.hpp"
#include "prepared-color-state.hpp"
#include "current-colors.hpp"
#include "texture-set-export.hpp"
#include "bone-metadata.hpp"
#include "humanoid-placement.hpp"
#include <bit>
#include <deque>
#include <map>
#include <set>
#include <nlohmann/json.hpp>
namespace dome {
namespace {
void append(std::vector<uint8_t> &bytes, uint32_t value) {
  for (int n = 0; n < 4; ++n)
    bytes.push_back(uint8_t(value >> (n * 8)));
}
} // namespace
void export_glb(const Mesh &mesh, const std::filesystem::path &path) {
  export_glb(std::vector<ObjectSurface>{{1, "Sculpt", {}, Material{}, mesh}},
             path);
}
void export_glb(const std::vector<ObjectSurface> &objects,
                const std::filesystem::path &path) {
  export_glb(objects,path,GltfExportOptions{});
}
GltfExportReport export_glb(const std::vector<ObjectSurface> &objects,
                           const std::filesystem::path &path,const GltfExportOptions &options) {
  auto stop=[&]{if(options.cancelled && options.cancelled())throw std::runtime_error("glTF export cancelled");};
  stop();
  if(path.extension()!=".glb")throw std::invalid_argument("Binary glTF export requires .glb");
  if(!options.max_output_bytes || options.max_output_bytes>256*1024*1024)
    throw std::invalid_argument("Invalid glTF output resource budget");
  std::optional<detail::EmissionExportPrepared> physical;
  if(options.emission)physical=detail::prepare_emission_export(objects,*options.emission,options.cancelled);
  auto document=physical?detail::make_gltf_emission(*physical,options.recompute_changed_normals,options.cancelled):detail::make_gltf(objects,detail::SourceMorphSpace::Native,options.recompute_changed_normals,options.cancelled);
  if(physical)detail::finish_emission_export(document,*physical,*options.emission,options.max_output_bytes,options.cancelled);
  document.json["buffers"]={{{"byteLength",document.binary.size()}}};
  uint64_t bytes=28+((uint64_t(document.json.dump().size())+3)&~uint64_t(3))+
                    ((uint64_t(document.binary.size())+3)&~uint64_t(3));
  if(bytes>options.max_output_bytes)throw std::length_error("glTF output resource budget exceeded");
  GltfExportReport report{objects.size(),document.normal_targets_recomputed,{}};
  if(physical)for(const auto &entry:physical->images)report.diagnostics.push_back("HDR object "+std::to_string(entry.object)+": exact FLOAT32/native payload; measured standard fallback maximum absolute error "+std::to_string(entry.maximum_absolute_error)+", erased nonzero samples "+std::to_string(entry.erased_nonzero_samples));
  if(report.normal_targets_recomputed)report.diagnostics.push_back(
    "Source NORMAL deltas recomputed from edited geometry; original authoring data remains unchanged");
  stop();detail::write_glb(std::move(document),path,options.cancelled,options.before_commit);return report;
}
namespace detail {
struct EditableMToonExport {
  TextureSetLimits budget;
  bool &wrote_preview;
};
static GltfDocument make_gltf_impl(const std::vector<ObjectSurface> &input_objects,
    SourceMorphSpace source_space,bool recompute_changed_normals,std::function<bool()> cancelled,
    const NativeAuthoringCapture *raw_authority,const EmissionExportPrepared *physical,
    const Vrm1AuthoringCompilation *normal_owner,EditableMToonExport *editable_preview=nullptr) {
  auto stop=[&]{if(cancelled && cancelled())throw std::runtime_error("glTF export cancelled");};
  size_t work=0;auto checkpoint=[&]{if((++work&1023)==0)stop();};
  stop();
  const auto *texture_set_owner=normal_owner?&normal_owner->capture():raw_authority;
  for(const auto&object:input_objects)if(object.textures&&object.textures->current_grid_preview)
    throw std::invalid_argument("Render-only current-grid preview is not export image authority; materialize actual native field images");
  for(const auto &object:input_objects)if((object.textures&&object.textures->overlay)||detail::hdr_emission(object.material.emissive)||(object.material_bindings&&detail::physical_emission(*object.material_bindings))) {
    if(object.texture_set_surface&&(texture_set_owner||(physical&&matches_texture_set_emission_surface(*physical,object))))continue;
    if(!(physical&&matches_native_emission_surface(*physical,object,cancelled))&&(!raw_authority||(!native_paint_compiled_emission(*raw_authority,object,cancelled)&&!raw_source_emission(*raw_authority,object,cancelled))))
      throw std::invalid_argument("Physical emission requires a precision-qualified export adapter");
  }
  for(const auto &object:input_objects)if((object.paint_sampling_attachment||!object.paint_corner_coordinates.empty())&&
      (source_space!=SourceMorphSpace::Native||!object.material_bindings||(!object.material_bindings->normal_frames&&!object.material_bindings->native_paint_sampling)))
    throw std::invalid_argument("Retained paint export requires its complete native normal/colour coordinate compiler");
  for(const auto &object:input_objects) {
    const auto expected=material_paint_attachment(object.material_bindings.get());
    if(expected!=object.paint_sampling_attachment||expected.owner_before(object.paint_sampling_attachment)||object.paint_sampling_attachment.owner_before(expected))throw std::invalid_argument("Current paint export omitted or fabricated its own sampling attachment");
  }
  GltfDocument result;
  const auto &objects=input_objects;
  if (objects.empty() || objects.size()>256)
    throw std::invalid_argument("The scene has no visible surface to export");

  std::vector<std::optional<TextureSetExport>> bundles(objects.size());
  size_t bundle_working_bytes=0;
  for(size_t i=0;i<objects.size();++i)if(objects[i].texture_set_surface){
    TextureSetLimits limits;limits.cancelled=cancelled;
    if(bundle_working_bytes>=limits.maximum_bytes)throw std::length_error("Aggregate texture-set export budget exhausted");
    limits.maximum_bytes-=bundle_working_bytes;
    bool measured=physical&&matches_texture_set_emission_surface(*physical,objects[i]);
    if(texture_set_owner){
      const auto &originals=texture_set_owner->originals();
      const auto original=std::find_if(originals.begin(),originals.end(),[&](const auto &entry){return entry.id==objects[i].id;});
      const auto &plan=objects[i].texture_set_surface;
      if(original==originals.end()||original->texture_set_surface!=plan||
         original->texture_set_surface.owner_before(plan)||plan.owner_before(original->texture_set_surface))
        throw std::invalid_argument("Editable texture set borrowed another native surface plan");
      measured=true;
    }
    bundles[i]=compile_texture_set_export(objects[i],limits,measured,bool(raw_authority));
    bundle_working_bytes+=bundles[i]->working_bound;
  }else if(source_space==SourceMorphSpace::Native&&!normal_owner&&objects[i].retopo_mtoon_export){
    TextureSetLimits limits;limits.cancelled=cancelled;
    if(bundle_working_bytes>=limits.maximum_bytes)throw std::length_error("Aggregate material export budget exhausted");
    limits.maximum_bytes-=bundle_working_bytes;
    bundles[i]=compile_retopo_mtoon_export(objects[i],limits);
    if(bundles[i])bundle_working_bytes+=bundles[i]->working_bound;
  }
  auto object_at=[&](size_t i)->const ObjectSurface&{return bundles[i]?bundles[i]->surface:objects[i];};

  for(const auto &object:objects)if(object.source_dormant)
    throw std::invalid_argument("Source attributes are dormant for "+object.name+
      "; restore the original mesh or explicitly prepare, reproject and bake its source data before export");
  auto &j=result.json;
  j = {
      {"asset", {{"version", "2.0"}, {"generator", "Dome Sculpt"}}},
      {"scene", 0},
      {"scenes", {{{"nodes", nlohmann::json::array()}}}}};
  auto &data=result.binary;
  auto floats = [&](auto values) {
    if(values.size()*sizeof(float)>256*1024*1024-std::min(data.size(),size_t(256*1024*1024)))throw std::length_error("glTF geometry resource limit exceeded before appending attributes");
    for (float f : values) {
      checkpoint();
      append(data, std::bit_cast<uint32_t>(f));
    }
  };
  auto add = [&](size_t offset, size_t bytes, int target, size_t count,
                 int component, const char *type) {
    int i = int(j["bufferViews"].size());
    j["bufferViews"].push_back(
        {{"buffer", 0}, {"byteOffset", offset}, {"byteLength", bytes}});
    if (target)
      j["bufferViews"][i]["target"] = target;
    j["accessors"].push_back({{"bufferView", i},
                              {"componentType", component},
                              {"count", count},
                              {"type", type}});
    return int(j["accessors"].size()) - 1;
  };
  // A native affine edit is represented by two individually valid TRS nodes;
  // original source locals remain separate exact matrices. Never serialize a
  // sheared world/inverse transform as a single glTF node.matrix.
  auto edit_nodes = [&](const std::shared_ptr<const NativeAffineEdit> &edit) {
    std::vector<nlohmann::json> nodes;
    if(edit)for(const auto &factor:factor_edit_for_gltf(edit->matrix(),cancelled).nodes)
      nodes.push_back({{"matrix",factor.float_matrix()}});
    return nodes;
  };
  auto attach = [&](int parent,nlohmann::json node) {
    if(cancelled&&cancelled())throw std::runtime_error("glTF export cancelled");
    const int index=int(j["nodes"].size());j["nodes"].push_back(std::move(node));
    if(parent<0)j["scenes"][0]["nodes"].push_back(index);
    else j["nodes"][parent]["children"].push_back(index);
    return index;
  };
  std::map<uint64_t, int> group_nodes;
  std::map<uint64_t, nlohmann::json> group_records;
  for (const auto &object : objects) {
    stop();
    uint64_t parent = 0;
    std::set<uint64_t> path;
    for (const auto &g : object.groups) {
      if (!g.id || g.parent != parent || !path.insert(g.id).second || path.size() > 128 ||
          g.name.empty() || g.name.size() > 256 || g.name.find('\0') != std::string::npos)
        throw std::invalid_argument("Invalid export group path");
      validate_transform(g.transform);
      auto t = g.transform;
      nlohmann::json node = {{"name", g.name},
        {"translation", {t.position.x,t.position.y,t.position.z}},
        {"rotation", {t.orientation.x,t.orientation.y,t.orientation.z,t.orientation.w}},
        {"scale", {t.scale,t.scale,t.scale}}};
      if (g.humanoid) {
        g.humanoid->validate();
        node["extras"]["domeSculptCreator"] = {{"type", "humanoid"}, {"version", 1},
                                               {"controls", g.humanoid->encode()}};
      }
      auto chain=edit_nodes(g.affine_edit);chain.push_back(std::move(node));
      if(g.source_placement) {
        g.source_placement->validate();
        // Keep the original local float bits; native edits never overwrite it.
        chain.push_back({{"matrix",g.source_placement->model->asset->nodes.at(g.source_placement->source_node).local}});
      }
      auto record=nlohmann::json{{"parentId",parent},{"chain",chain}};
      if (auto it = group_records.find(g.id); it != group_records.end()) {
        if (it->second != record) throw std::invalid_argument("Conflicting export group definitions");
      } else {
        if (group_nodes.size() >= 128) throw std::length_error("Export group limit exceeded");
        int leaf=parent?group_nodes.at(parent):-1;
        for(auto &part:chain)leaf=attach(leaf,std::move(part));
        group_nodes[g.id]=leaf;group_records[g.id]=std::move(record);
      }
      parent = g.id;
    }
    (void)object.world_matrix();
  }
  struct SharedRig { int root, first_joint, skin; };
  std::map<std::string, SharedRig> shared_rigs;
  std::map<const ImportedAsset *,detail::SourceBoneMetadata> bone_metadata;
  std::vector<bool> vertex_transparency;
  vertex_transparency.reserve(objects.size());
  // Compiled coordinate bindings are not native PaintSourceTexture authority.
  std::vector<unsigned> retained_normal_sets;retained_normal_sets.reserve(objects.size());
  std::vector<unsigned> retained_paint_sets;retained_paint_sets.reserve(objects.size());
  auto append_material=[&](const Material&m,bool vertex_alpha){
    m.validate();const int id=int(j["materials"].size());
    j["materials"].push_back({{"name",m.name},{"pbrMetallicRoughness",{{"baseColorFactor",m.color},{"metallicFactor",m.metallic},{"roughnessFactor",m.roughness}}},{"emissiveFactor",m.emissive},{"doubleSided",m.double_sided},{"alphaMode",m.color[3]<1||vertex_alpha?"BLEND":"OPAQUE"}});
    auto&encoded=j["materials"].back();
    if(m.alpha!=Material::AlphaMode::Automatic)encoded["alphaMode"]=m.alpha==Material::AlphaMode::Mask?"MASK":m.alpha==Material::AlphaMode::Blend?"BLEND":"OPAQUE";
    if(m.alpha==Material::AlphaMode::Mask)encoded["alphaCutoff"]=m.alpha_cutoff;
    if(m.unlit){encoded["extensions"]["KHR_materials_unlit"]=nlohmann::json::object();auto&used=j["extensionsUsed"];if(!used.is_array())used=nlohmann::json::array();if(std::find(used.begin(),used.end(),"KHR_materials_unlit")==used.end())used.push_back("KHR_materials_unlit");}
    return id;
  };
  for (size_t object_index=0;object_index<objects.size();++object_index) {
    const auto&input=object_at(object_index);
    stop();
    retained_normal_sets.push_back(0);retained_paint_sets.push_back(0);
    auto validated_colors=detail::current_vertex_colors(input,cancelled);
    bool vertex_alpha=false;
    if(!validated_colors.empty())for(auto v:input.mesh.indices) {
      checkpoint();vertex_alpha|=validated_colors.at(v)[3]<1;
    }
    vertex_transparency.push_back(vertex_alpha);
    // The canonical frame exporter authenticates the retained attachment and
    // verifies the original colour receipt on its writable atlas.
    if(const auto &binding=input.material_bindings;binding&&binding->color_preparation) {
      if(!binding->canonical_paint_sampling)
        binding->color_preparation->validate_current(input.mesh,input.appearance.get(),cancelled);
      else if(!binding->normal_frames)
        throw std::invalid_argument("Retained color export needs its own canonical frame and sampling attachment");
    }
    std::optional<NormalFrameExport> prepared;
    if(input.material_bindings && input.material_bindings->normal_frames)prepared=normal_owner?normal_frame_export_vrm1(*normal_owner,input,cancelled):normal_frame_export(input,source_space,cancelled,source_space==SourceMorphSpace::Native);
    else if(input.material_bindings&&input.material_bindings->native_paint_sampling)prepared=native_paint_export(input,source_space,cancelled);
    else if(!bundles[object_index]&&native_mikk_texture(input))prepared=native_mikk_export(input,cancelled);
    const auto &o=prepared?prepared->surface:input;auto *frame=prepared?&*prepared:nullptr;
    GltfObjectIdentity emission;
    if(frame){emission.canonical_normal=frame->canonical_normal;emission.prepared_normal=frame->prepared_normal;}
    const Mesh *hdr_mesh=nullptr;
    if(physical){
      const auto field=std::find_if(physical->images.begin(),physical->images.end(),[&](const auto &entry){
        return entry.object==input.id&&entry.prepared_source_mesh;
      });
      if(field!=physical->images.end())hdr_mesh=field->prepared_source_mesh.get();
    }
    const bool gather_uvs=hdr_mesh||(frame&&(!frame->normal_coordinates.empty()||!frame->paint_coordinates.empty()));
    auto &mesh = o.mesh;
    const auto source_uv1=input.textures&&input.textures->source_maps?input.textures->source_maps->uv1:nullptr;
    const auto source_maps=input.textures?input.textures->source_maps:nullptr;
    if(source_maps&&(source_uv1||!source_maps->extra_uv_sets.empty()))source_maps->validate_surface(input.mesh);
    std::optional<SourceMorphTransport> source_attributes;
    if(!frame)source_attributes.emplace(o,source_space,cancelled);
    mesh.validate();
    o.material.validate();
    if (o.deformation)
      o.deformation->validate(mesh);
    validate_transform(o.transform);
    if (mesh.positions.empty())
      throw std::invalid_argument("Cannot export an empty mesh");
    size_t begin = data.size();
    Vec3 lo = mesh.positions.front(), hi = lo;
    for (auto p : mesh.positions) {
      floats(std::array{p.x, p.y, p.z});
      lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
      hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
    }
    int positions = add(begin, data.size() - begin, 34962,
                        mesh.positions.size(), 5126, "VEC3");
    j["accessors"][positions]["min"] = {lo.x, lo.y, lo.z};
    j["accessors"][positions]["max"] = {hi.x, hi.y, hi.z};
    begin = data.size();
    for (auto p : mesh.normals)
      floats(std::array{p.x, p.y, p.z});
    int normals = add(begin, data.size() - begin, 34962, mesh.normals.size(),
                      5126, "VEC3");
    nlohmann::json attributes = {{"POSITION", positions}, {"NORMAL", normals}};
    std::vector<std::array<float,2>> emitted_uv1;
    std::map<unsigned,std::vector<std::array<float,2>>> emitted_extra_uv;
    if (!mesh.uv.empty()) {
      begin = data.size();
      for (auto p : mesh.uv)
        floats(p);
      attributes["TEXCOORD_0"] =
          add(begin, data.size() - begin, 34962, mesh.uv.size(), 5126, "VEC2");
    }
    auto current_colors=frame?detail::remap_current_vertex_colors(validated_colors,frame->rest_vertices,cancelled):std::move(validated_colors);
    if(!current_colors.empty()) {
      begin=data.size();for(const auto &color:current_colors)floats(color);
      attributes["COLOR_0"]=add(begin,data.size()-begin,34962,current_colors.size(),5126,"VEC4");
    }
    std::vector<Tangent> tangents=frame?frame->tangents:bundles[object_index]?bundles[object_index]->tangents:std::vector<Tangent>{};
    if(auto source=frame?frame->source:source_attributes->source()) {
      const auto &a=*o.appearance;
      auto attribute=[&](const auto &values,const char *name,const char *type) {
        if(values.empty())return;
        begin=data.size();for(auto v:a.source_vertices){if(v>=values.size())throw std::invalid_argument("Source attribute vertex index is missing");floats(values[v]);}
        attributes[name]=add(begin,data.size()-begin,34962,mesh.positions.size(),5126,type);
      };
      if(!source_uv1){
        attribute(source->uv1,"TEXCOORD_1","VEC2");
        if(gather_uvs&&!source->uv1.empty())
          for(auto v:a.source_vertices)emitted_uv1.push_back(source->uv1.at(v));
      }
      else if(!source->uv1.empty()){
        if(a.source_vertices.size()!=mesh.positions.size())throw std::invalid_argument("Export UV1 appearance domain differs from emitted vertices");
        for(size_t vertex=0;vertex<mesh.positions.size();++vertex){checkpoint();auto native=frame?frame->rest_vertices.at(vertex):vertex;
          if(source_uv1->at(native)!=source->uv1.at(a.source_vertices[vertex]))throw std::invalid_argument("Export PBR and retained source UV1 domains differ");
        }
      }
      for(const auto &[set,values]:source->extra_uv_sets){
        if(set<2||set>7)throw std::invalid_argument("Original additional UV attribute exceeds export policy");
        if(source_maps&&source_maps->extra_uv_sets.contains(set)){
          const auto &current=*source_maps->extra_uv_sets.at(set);
          for(size_t v=0;v<mesh.positions.size();++v){checkpoint();auto native=frame?frame->rest_vertices.at(v):v;if(current.at(native)!=values.at(a.source_vertices.at(v)))throw std::invalid_argument("Export additional source UV authority differs from original attributes");}
        }else{
          std::vector<std::array<float,2>> remapped;remapped.reserve(a.source_vertices.size());for(auto v:a.source_vertices){checkpoint();remapped.push_back(values.at(v));}
          begin=data.size();for(auto uv:remapped)floats(uv);attributes["TEXCOORD_"+std::to_string(set)]=add(begin,data.size()-begin,34962,mesh.positions.size(),5126,"VEC2");emitted_extra_uv[set]=std::move(remapped);
        }
      }
      if(!frame)tangents=source_attributes->base_tangents();
      if(!frame && !tangents.empty() && o.textures && !o.textures->normal.rgba.empty()) {
        auto canonical=tangent_corners(mesh,cancelled);
        for(size_t corner=0;corner<canonical.size();++corner) {
          checkpoint();
          auto authored=tangents.at(mesh.indices[corner]);auto target=canonical[corner];
          if(length(Vec3{authored[0]-target[0],authored[1]-target[1],authored[2]-target[2]})>1e-4f || authored[3]*target[3]<0)
            throw std::invalid_argument("Canonical baked normals conflict with retained source tangents; explicit tangent basis conversion is required");
        }
      }
    }
    if(source_uv1){
      begin=data.size();
      for(size_t vertex=0;vertex<mesh.positions.size();++vertex){checkpoint();auto uv=source_uv1->at(frame?frame->rest_vertices.at(vertex):vertex);floats(uv);if(gather_uvs)emitted_uv1.push_back(uv);}
      attributes["TEXCOORD_1"]=add(begin,data.size()-begin,34962,mesh.positions.size(),5126,"VEC2");
    }
    if(source_maps)for(const auto &[set,coordinates]:source_maps->extra_uv_sets){
      std::vector<std::array<float,2>> remapped;remapped.reserve(mesh.positions.size());for(size_t v=0;v<mesh.positions.size();++v){checkpoint();remapped.push_back(coordinates->at(frame?frame->rest_vertices.at(v):v));}
      begin=data.size();for(auto uv:remapped)floats(uv);attributes["TEXCOORD_"+std::to_string(set)]=add(begin,data.size()-begin,34962,mesh.positions.size(),5126,"VEC2");emitted_extra_uv[set]=std::move(remapped);
    }
    if(frame&&!frame->normal_coordinates.empty()){
      if(frame->normal_coordinates.size()!=mesh.positions.size())throw std::invalid_argument("Retained normal export vertex correspondence differs");
      if(frame->normal_coordinates==mesh.uv)retained_normal_sets.back()=0;
      else if(attributes.contains("TEXCOORD_1")&&frame->normal_coordinates==emitted_uv1)retained_normal_sets.back()=1;
      else{
        auto existing=std::find_if(emitted_extra_uv.begin(),emitted_extra_uv.end(),[&](const auto &entry){return entry.second==frame->normal_coordinates;});
        if(existing!=emitted_extra_uv.end())retained_normal_sets.back()=existing->first;
        else{unsigned coordinate=1;while(attributes.contains("TEXCOORD_"+std::to_string(coordinate)))++coordinate;
          if(coordinate>7)throw std::length_error("Retained normal export exceeds source UV coordinate policy");
          begin=data.size();for(auto uv:frame->normal_coordinates)floats(uv);
          attributes["TEXCOORD_"+std::to_string(coordinate)]=add(begin,data.size()-begin,34962,mesh.positions.size(),5126,"VEC2");retained_normal_sets.back()=coordinate;
        }
      }
      // The source image/sampler remains a valid native view. Only the glTF
      // compiler owns the separate emitted accessor/texture-info association.
      emission.prepared_normal.texcoord=0;
    }
    if(frame&&!frame->paint_coordinates.empty()) {
      const auto &coordinates=frame->paint_coordinates;
      if(coordinates.size()!=mesh.positions.size())throw std::invalid_argument("Paint export vertex correspondence differs");
      unsigned coordinate=0;
      if(coordinates==mesh.uv)coordinate=0;
      else if(attributes.contains("TEXCOORD_1")&&coordinates==emitted_uv1)coordinate=1;
      else if(!frame->normal_coordinates.empty()&&coordinates==frame->normal_coordinates)coordinate=retained_normal_sets.back();
      else if(auto found=std::find_if(emitted_extra_uv.begin(),emitted_extra_uv.end(),[&](const auto &entry){return entry.second==coordinates;});found!=emitted_extra_uv.end())coordinate=found->first;
      else {
        coordinate=1;while(coordinate<8&&attributes.contains("TEXCOORD_"+std::to_string(coordinate)))++coordinate;
        if(coordinate==8)throw std::length_error("Paint export has no free glTF coordinate domain");
        begin=data.size();for(auto uv:coordinates){checkpoint();floats(uv);}
        attributes["TEXCOORD_"+std::to_string(coordinate)]=add(begin,data.size()-begin,34962,mesh.positions.size(),5126,"VEC2");
      }
      retained_paint_sets.back()=coordinate;
    }
    if(bundles[object_index]||hdr_mesh){
      const auto coordinate=[&](const std::vector<std::array<float,2>>&values)->unsigned{
        if(values.size()!=mesh.positions.size())throw std::invalid_argument("Compiled paint coordinate domain differs from emitted geometry");
        if(values==mesh.uv)return 0;
        if(attributes.contains("TEXCOORD_1")&&values==emitted_uv1)return 1;
        for(const auto&[set,existing]:emitted_extra_uv)if(values==existing)return set;
        unsigned set=1;while(set<8&&attributes.contains("TEXCOORD_"+std::to_string(set)))++set;if(set==8)throw std::length_error("Paint export has no free glTF coordinate domain");
        begin=data.size();for(auto uv:values){checkpoint();floats(uv);}attributes["TEXCOORD_"+std::to_string(set)]=add(begin,data.size()-begin,34962,mesh.positions.size(),5126,"VEC2");emitted_extra_uv[set]=values;return set;
      };
      if(hdr_mesh){
        if(bundles[object_index]||hdr_mesh->indices!=input.mesh.indices)
          throw std::invalid_argument("Prepared source HDR requires its exact emitted vertex correspondence");
        if(frame){
          std::vector<std::array<float,2>> writable;
          writable.reserve(frame->rest_vertices.size());
          for(auto vertex:frame->rest_vertices){checkpoint();writable.push_back(hdr_mesh->uv.at(vertex));}
          emission.prepared_emission_texcoord=coordinate(writable);
        }else emission.prepared_emission_texcoord=coordinate(hdr_mesh->uv);
      }else{
        const auto &bundle=*bundles[object_index];
        if(!bundle.paint_coordinates.empty())retained_paint_sets.back()=coordinate(bundle.paint_coordinates);
        if(!bundle.normal_coordinates.empty())retained_normal_sets.back()=coordinate(bundle.normal_coordinates);
      }
    }
    bool uv_gap=false;
    for(unsigned coordinate=0;coordinate<8;++coordinate) {
      if(!attributes.contains("TEXCOORD_"+std::to_string(coordinate)))uv_gap=true;
      else if(uv_gap)throw std::invalid_argument("glTF export requires genuine contiguous TEXCOORD arrays; missing source sets cannot be invented or relabeled");
    }
    if(!tangents.empty()){
      begin=data.size();for(auto t:tangents)floats(t);
      attributes["TANGENT"]=add(begin,data.size()-begin,34962,mesh.positions.size(),5126,"VEC4");
    }
    if (o.deformation && o.deformation->skin) {
      const auto &weights = o.deformation->skin->vertices;
      size_t sets = 0;
      for (const auto &vertex : weights) {
        checkpoint();
        sets = std::max(sets, (vertex.size() + 3) / 4);
      }
      if(!weights.empty() && sets>(256*1024*1024-std::min(data.size(),size_t(256*1024*1024)))/(weights.size()*24))throw std::length_error("Padded skin attribute arrays exceed glTF output resource bound");
      for (size_t set = 0; set < sets; ++set) {
        begin = data.size();
        for (const auto &vertex : weights)
          for (size_t k = 0; k < 4; ++k) {
            checkpoint();
            size_t influence = set * 4 + k;
            uint16_t joint = influence < vertex.size()
                                 ? uint16_t(vertex[influence].joint)
                                 : 0;
            data.push_back(uint8_t(joint));
            data.push_back(uint8_t(joint >> 8));
          }
        attributes["JOINTS_" + std::to_string(set)] = add(
            begin, data.size() - begin, 34962, weights.size(), 5123, "VEC4");
        begin = data.size();
        for (const auto &vertex : weights)
          for (size_t k = 0; k < 4; ++k) {
            checkpoint();
            size_t influence = set * 4 + k;
            append(data, std::bit_cast<uint32_t>(influence < vertex.size()
                                                     ? vertex[influence].weight
                                                     : 0.f));
          }
        attributes["WEIGHTS_" + std::to_string(set)] = add(
            begin, data.size() - begin, 34962, weights.size(), 5126, "VEC4");
      }
    }
    begin = data.size();
    for (auto index : mesh.indices) {
      checkpoint();
      append(data, index);
    }
    int indices = add(begin, data.size() - begin, 34963, mesh.indices.size(),
                      5125, "SCALAR");
    int parent=o.groups.empty()?-1:group_nodes.at(o.groups.back().id);
    for(auto &prefix:edit_nodes(o.affine_edit))parent=attach(parent,std::move(prefix));
    int material = int(j["materials"].size()),mesh_index = int(j["meshes"].size());
    auto t = o.transform;
    auto &m = o.material;
    const int node=attach(parent,
        {{"name", o.name},
         {"mesh", mesh_index},
         {"translation", {t.position.x, t.position.y, t.position.z}},
         {"rotation",
          {t.orientation.x, t.orientation.y, t.orientation.z, t.orientation.w}},
         {"scale", {t.scale, t.scale, t.scale}}});
    emission.node=node; emission.mesh=mesh_index; emission.material=material;
    j["meshes"].push_back({{"name", o.name},
                           {"primitives",
                            {{{"attributes", attributes},
                              {"indices", indices},
                              {"material", material},
                              {"mode", 4}}}}});
    if (o.deformation) {
      const auto &d = *o.deformation;
      if (d.morphs && !d.morphs->targets.empty()) {
        auto targets = nlohmann::json::array(), names = nlohmann::json::array();
        size_t native_target=0;
        for (const auto &target : d.morphs->targets) {
          begin = data.size();
          Vec3 low = target.delta.front(), high = low;
          for (auto p : target.delta) {
            floats(std::array{p.x, p.y, p.z});
            low = {std::min(low.x, p.x), std::min(low.y, p.y),
                   std::min(low.z, p.z)};
            high = {std::max(high.x, p.x), std::max(high.y, p.y),
                    std::max(high.z, p.z)};
          }
          int accessor = add(begin, data.size() - begin, 34962,
                             target.delta.size(), 5126, "VEC3");
          j["accessors"][accessor]["min"] = {low.x, low.y, low.z};
          j["accessors"][accessor]["max"] = {high.x, high.y, high.z};
          nlohmann::json channels={{"POSITION",accessor}};
          auto direction=[&](const std::vector<Vec3> &values,const char *name){
            if(values.empty())return;
            begin=data.size();
            for(auto p:values)floats(std::array{p.x,p.y,p.z});
            channels[name]=add(begin,data.size()-begin,34962,values.size(),5126,"VEC3");
          };
          if(frame){
            direction(frame->normal_targets.at(native_target),"NORMAL");
            direction(frame->tangent_targets.at(native_target),"TANGENT");
          }else{
            const auto &bundle=bundles[object_index];
            const auto directions=bundle?SourceMorphChannels{}:source_attributes->target(native_target,recompute_changed_normals);
            const auto &values=bundle?bundle->morph_channels.at(native_target):directions;
            direction(values.normals,"NORMAL");direction(values.tangents,"TANGENT");
            result.normal_targets_recomputed+=values.normals_recomputed;
          }
          ++native_target;
          targets.push_back(std::move(channels));
          names.push_back(target.name);
        }
        j["meshes"][mesh_index]["primitives"][0]["targets"] = targets;
        j["meshes"][mesh_index]["extras"]["targetNames"] = names;
        j["meshes"][mesh_index]["weights"] =
            d.morph_weights.empty()
                ? std::vector<float>(d.morphs->targets.size(), 0)
                : d.morph_weights;
      }
      if (d.skeleton) {
        const auto &s = *d.skeleton;
        const detail::SourceBoneMetadata *source_bones=nullptr;
        if(o.appearance&&!o.appearance->source_joint_nodes.empty()){
          const auto &appearance=*o.appearance;
          if(!appearance.asset||appearance.source_joint_nodes.size()!=s.bones.size())
            throw std::invalid_argument("Bone authoring metadata requires the actual current source joint identity table");
          auto at=bone_metadata.find(appearance.asset.get());
          if(at==bone_metadata.end())at=bone_metadata.emplace(appearance.asset.get(),detail::source_bone_metadata(*appearance.asset,cancelled)).first;
          source_bones=&at->second;std::set<uint32_t> unique;
          for(auto source:appearance.source_joint_nodes)
            if(source>=source_bones->extras.size()||!unique.insert(source).second)
              throw std::invalid_argument("Bone authoring metadata source joint is invalid or repeated");
        }
        if(!d.skin){
          bool authored=false;for(size_t k=0;k<s.bones.size();++k)
            authored|=s.bones[k].length!=Bone{}.length||(source_bones&&source_bones->lengths[o.appearance->source_joint_nodes[k]].has_value());
          if(authored)throw std::invalid_argument("Unbound skeleton bone-length export needs an explicit mesh/rig association; save the native project");
        }
        // Keep the skinned surface under its exact object/group chain. glTF
        // computes skinned positions from the joints, but the mesh-node global
        // determinant still determines winding and tangent handedness. Moving
        // it to an identity scene root loses reflected front-face authority.
        // Rest inverse-bind matrices exclude the object transform.
        int mesh_node = int(j["nodes"].size());
        emission.node=mesh_node;
        j["nodes"][node].erase("mesh");
        j["nodes"][node]["children"] = nlohmann::json::array({mesh_node});
        j["nodes"].push_back(
            {{"name", o.name + " surface"}, {"mesh", mesh_index}});
        std::string rig_key;
        if (o.deformation->skin && !o.groups.empty()) {
          auto identity = nlohmann::json{{"group",o.groups.back().id},
            {"skeleton",s.encode()}, {"pose",nlohmann::json::array()},
            {"object",j["nodes"][node]}};
          // Object names/mesh identities are irrelevant; TRS must match exactly.
          identity["object"].erase("name"); identity["object"].erase("children");
          if(o.affine_edit)identity["nativeAffineEdit"]=o.affine_edit->matrix().matrix();
          for(const auto &pose:d.pose){ identity["pose"].push_back({
            {"t",{pose.translation.x,pose.translation.y,pose.translation.z}},
            {"r",{pose.rotation.w,pose.rotation.x,pose.rotation.y,pose.rotation.z}},
            {"s",{pose.scale.x,pose.scale.y,pose.scale.z}}});
            if(pose.full_local_matrix)identity["pose"].back()["full_local_matrix"]=*pose.full_local_matrix;
          }
          if(d.humanoid) identity["humanoid"]=d.humanoid->encode();
          if(!d.source_skin_joints.empty())identity["nativeSourceJointSlots"]=d.source_skin_joints;
          if(source_bones){
            auto extras=nlohmann::json::array();bool present=false;
            for(auto source:o.appearance->source_joint_nodes){
              const auto &value=source_bones->extras[source];present|=value.has_value();
              extras.push_back(value?nlohmann::json{{"present",true},{"value",*value}}
                                    :nlohmann::json{{"present",false}});
            }
            // Equal skeleton values do not authorize losing a different
            // source joint's opaque authoring data through rig deduplication.
            if(present)identity["sourceBoneExtras"]=std::move(extras);
          }
          rig_key=identity.dump();
        }
        if (!rig_key.empty() && shared_rigs.contains(rig_key)) {
          // All parts retain separate geometry/morphs but share one actual rig.
          const auto &shared=shared_rigs.at(rig_key);
          j["nodes"][mesh_node]["skin"]=shared.skin;
          for(size_t k=0;k<s.bones.size();++k) emission.joints.push_back(uint32_t(shared.first_joint+k));
        } else {
        const auto local_matrices=s.local_matrices(d.pose);
        int first_joint = int(j["nodes"].size());
        std::vector<std::optional<AffineGltfFactors>> joint_factors(s.bones.size());
        std::vector<int> joint_roots(s.bones.size());
        for (size_t index = 0; index < s.bones.size(); ++index) {
          emission.joints.push_back(uint32_t(first_joint+index));
          const auto &b = s.bones[index];
          const auto &t = d.pose.empty() ? b.rest : d.pose[index];
          j["nodes"].push_back(
              {{"name", b.name},
               {"translation",
                {t.translation.x, t.translation.y, t.translation.z}},
               {"rotation",
                {t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w}},
               {"scale", {t.scale.x, t.scale.y, t.scale.z}}});
          if(source_bones){const auto &extra=source_bones->extras[o.appearance->source_joint_nodes[index]];
            if(extra)j["nodes"].back()["extras"]=*extra;
          }
          detail::write_bone_length(j["nodes"].back(),b.length);
          joint_roots[index]=first_joint+int(index);
          if(b.rest_matrix||t.full_local_matrix){
            j["nodes"].back().erase("translation");j["nodes"].back().erase("rotation");j["nodes"].back().erase("scale");
            if(detail::gltf_trs_matrix(local_matrices[index]))j["nodes"].back()["matrix"]=local_matrices[index];
            else {joint_factors[index]=factor_edit_for_gltf(AffineTransform(local_matrices[index]),cancelled);j["nodes"].back()["matrix"]=joint_factors[index]->nodes[1].float_matrix();}
          }
        }
        // Keep bone leaf IDs contiguous/stable for JOINTS and semantic metadata.
        // A verified affine helper is the parent of that bone, never a sheared
        // glTF node or a rewritten inverse bind.
        for(size_t index=0;index<s.bones.size();++index)if(joint_factors[index]){
          joint_roots[index]=int(j["nodes"].size());
          j["nodes"].push_back({{"name",s.bones[index].name+" affine pose/rest factor"},{"matrix",joint_factors[index]->nodes[0].float_matrix()},{"children",nlohmann::json::array({first_joint+int(index)})}});
        }
        int rig_parent=node;
        if(s.space_matrix){
          if(detail::gltf_trs_matrix(*s.space_matrix)){
            rig_parent=attach(node,{{"name",o.name+" bind space"},{"matrix",*s.space_matrix}});
          }else{
            // inverse(original mesh world) can contain shear even when every
            // authored local is valid TRS. Retain it as a bounded verified TRS
            // chain, never an invalid glTF matrix or altered inverse bind.
            const auto factors=factor_edit_for_gltf(AffineTransform(*s.space_matrix),cancelled);
            for(const auto &factor:factors.nodes){
              rig_parent=attach(rig_parent,{{"name",o.name+" bind space factor"},{"matrix",factor.float_matrix()}});
            }
          }
        }
        for (size_t index = 0; index < s.bones.size(); ++index) {
          int parent = s.bones[index].parent < 0
                           ? rig_parent
                           : first_joint + s.bones[index].parent;
          j["nodes"][parent]["children"].push_back(joint_roots[index]);
        }
        if (d.skin) {
          begin = data.size();
          for (const auto &matrix : s.inverse_bind())
            floats(matrix);
          int accessor =
              add(begin, data.size() - begin, 0, s.bones.size(), 5126, "MAT4");
          auto joints = nlohmann::json::array();
          for (size_t index = 0; index < s.bones.size(); ++index)
            joints.push_back(first_joint + int(index));
          int skin = int(j["skins"].size());
          j["skins"].push_back({{"name", o.name + " skin"},
                                {"joints", joints},
                                {"inverseBindMatrices", accessor},
                                {"skeleton", node}});
          j["nodes"][mesh_node]["skin"] = skin;
        }
        if (d.humanoid) {
          // Application metadata only: a complete VRMC_vrm also needs model
          // licensing/meta and other constraints, so no VRM extension is
          // claimed.
          d.humanoid->validate(s, true);
          if (!d.pose.empty()){
            const auto prefix=detail::humanoid_placement_prefix(s,*d.humanoid);
            const auto rest=s.local_matrices(),posed=s.local_matrices(d.pose);
            for(size_t joint=0;joint<d.pose.size();++joint){
              const auto &t=d.pose[joint];
              if (t.scale.x <= 0 || t.scale.y <= 0 || t.scale.z <= 0 ||
                  (t.full_local_matrix&&AffineTransform(*t.full_local_matrix).orientation()!=AffineOrientation::Preserving)){
                if(!prefix.test(joint)||rest[joint]!=posed[joint])
                  throw std::invalid_argument("Humanoid export requires positive editable pose scales; exact external placement must remain unchanged");
              }
            }
          }
          auto bones = nlohmann::json::object();
          for (const auto &[role, index] : d.humanoid->joints)
            bones[role] = {{"node", first_joint + int(index)}};
          j["nodes"][node]["extras"]["domeSculptHumanoid"] = {
              {"version", 1}, {"humanBones", bones}};
        }
        if(!rig_key.empty()) shared_rigs[rig_key]={node,first_joint,int(j["skins"].size())-1};
        }
      }
      if (d.expressions)
        j["meshes"][mesh_index]["extras"]["domeSculptExpressions"] =
            d.expressions->encode();
    }
    if (data.size() > 256 * 1024 * 1024)
      throw std::length_error("glTF geometry resource limit exceeded");
    append_material(m,vertex_alpha);
    // A native paint coordinate expansion has no artist normal-frame owner.
    // Only the real frame-backed route carries this deformation policy receipt.
    if(frame&&o.material_bindings&&o.material_bindings->normal_frames) {
      auto &extra=j["meshes"][mesh_index]["primitives"][0]["extras"]["domeSculptNormalTransport"];
      extra={{"version",1},{"skinPolicy","weighted-joint-inverse-transpose-1"},{"frameTargetIds",nlohmann::json::array()}};
      for(auto id:o.material_bindings->normal_frames->current_targets)extra["frameTargetIds"].push_back(std::to_string(id));
    }
    result.objects.push_back(std::move(emission));
  }
  struct ExtraMaterial {size_t object,field;int material;};
  std::vector<ExtraMaterial>extra_materials;
  for(size_t i=0;i<objects.size();++i)if(bundles[i]&&objects[i].texture_set_surface){
    stop();const auto&bundle=*bundles[i];auto&primitives=j["meshes"][result.objects[i].mesh]["primitives"];
    const auto base=primitives.at(0);const int first_accessor=base.at("indices");const auto index_template=j["accessors"].at(first_accessor);
    primitives=nlohmann::json::array();
    for(size_t f=0;f<bundle.fields.size();++f){
      stop();const auto&field=bundle.fields[f];auto primitive=base;auto accessor=index_template;
      accessor["count"]=field.count;accessor["byteOffset"]=field.first*sizeof(uint32_t);
      int index=first_accessor;if(f){index=int(j["accessors"].size());j["accessors"].push_back(std::move(accessor));}else j["accessors"][index]=std::move(accessor);
      primitive["indices"]=index;primitive["extras"]["dome_sculpt_texture_set"]={{"id",field.set},{"name",field.name}};
      if(!field.tangent_attribute){
        primitive.at("attributes").erase("TANGENT");
        if(auto targets=primitive.find("targets");targets!=primitive.end())
          for(auto &target:*targets)target.erase("TANGENT");
      }
      if(f){const int material=append_material(field.material,false);primitive["material"]=material;extra_materials.push_back({i,f,material});}
      primitives.push_back(std::move(primitive));
    }
  }
  // Images are shared independently of samplers and texture-coordinate transforms.
  std::map<const Image *, int> image_ids;
  std::map<std::array<int, 4>, int> sampler_ids;
  std::map<std::pair<int, int>, int> texture_ids;
  auto use_extension = [&](const char *name) {
    auto &used = j["extensionsUsed"];
    if (!used.is_array()) used = nlohmann::json::array();
    if (std::find(used.begin(), used.end(), name) == used.end()) used.push_back(name);
  };
  auto texture = [&](const Image &image, const PaintSourceTexture &slot) {
    image.validate();
    int source;
    if (auto found = image_ids.find(&image); found != image_ids.end()) source = found->second;
    else {
      auto bytes = image.png();
      if (bytes.size() > 256 * 1024 * 1024 - std::min(data.size(), size_t(256 * 1024 * 1024)))
        throw std::length_error("glTF image resource limit exceeded");
      int view = int(j["bufferViews"].size());
      source = int(j["images"].size());
      j["bufferViews"].push_back({{"buffer",0},{"byteOffset",data.size()},{"byteLength",bytes.size()}});
      data.insert(data.end(),bytes.begin(),bytes.end());
      while(data.size()%4) data.push_back(0);
      j["images"].push_back({{"bufferView",view},{"mimeType","image/png"}});
      image_ids[&image]=source;
    }
    std::array key{slot.wrap_s,slot.wrap_t,slot.min_filter,slot.mag_filter};
    int sampler;
    if(auto found=sampler_ids.find(key);found!=sampler_ids.end()) sampler=found->second;
    else {
      sampler=int(j["samplers"].size());
      nlohmann::json record={{"wrapS",slot.wrap_s},{"wrapT",slot.wrap_t}};
      if(slot.min_filter) record["minFilter"]=slot.min_filter;
      if(slot.mag_filter) record["magFilter"]=slot.mag_filter;
      j["samplers"].push_back(record); sampler_ids[key]=sampler;
    }
    auto key2=std::pair{source,sampler};
    if(auto found=texture_ids.find(key2);found!=texture_ids.end()) return found->second;
    int index=int(j["textures"].size());
    j["textures"].push_back({{"source",source},{"sampler",sampler}});
    texture_ids[key2]=index; return index;
  };
  // Keep generated texture owners alive for the image-identity table.
  std::vector<std::shared_ptr<const Image>> mtoon_paint_images;
  std::deque<MToonFloatPreview> mtoon_float_previews;
  TextureSetLimits retopo_paint_limits;
  retopo_paint_limits.maximum_bytes-=bundle_working_bytes;
  retopo_paint_limits.maximum_work=256'000'000;
  retopo_paint_limits.cancelled=cancelled;
  const auto write_set_mtoon=[&](const TextureSetView &field,const TextureSetExportField &compiled,
                                 size_t object,size_t primitive,int material_id,bool variant=false){
    const auto &binding=field.material_bindings;
    if(!binding||!binding->has_mtoon)return false;
    auto retain_native=[&]{
      if(variant)return false;
      if(!raw_authority)
        throw std::invalid_argument("This MToon texture set needs an exact portable material adapter. Editable GLB preserves the native material and paint.");
      j["materials"][material_id]["extras"]["domeSculptNativeMaterial"]="MToon in embedded authoring data";
      return false;
    };
    const auto *hdr=compiled.emission&&compiled.emission->filter_error?&*compiled.emission:nullptr;
    const bool try_editable_float=editable_preview&&compiled.emission&&!hdr&&
        compiled.portable_mtoon_bake_frame;
    if(field.source||binding->material.version()!=MToonVersion::V1||
       binding->physical_emission||(compiled.emission&&!hdr&&!try_editable_float))
      return retain_native();

    const auto &attributes=j["meshes"][result.objects[object].mesh]["primitives"][primitive]["attributes"];
    if(binding->roles[size_t(MToonSlot::Normal)].image&&attributes.contains("TANGENT")&&
       !compiled.portable_mtoon_bake_frame&&!compiled.portable_mtoon_derivative_frame)
      return retain_native();

    (void)binding->material.copy_working_bytes(12*1024*1024,cancelled);
    auto typed=binding->material;
    auto original=typed.source_json();
    if(!variant&&(original.contains("occlusionTexture")||
       (original.contains("pbrMetallicRoughness")&&original.at("pbrMetallicRoughness").contains("metallicRoughnessTexture"))))
      return retain_native();

    // Source JSON retains independent metadata. Remap only extensions whose
    // wire semantics are known here, including transforms on individual slots.
    std::set<std::string> extensions;
    const auto collect_extensions=[&](auto &&self,const nlohmann::json &value)->bool{
      if(value.is_array())return std::all_of(value.begin(),value.end(),[&](const auto &child){return self(self,child);});
      if(!value.is_object())return true;
      for(const auto &[name,child]:value.items()){
        if(name=="extras")continue;
        if(name=="extensions"){
          if(!child.is_object())return false;
          for(const auto &[extension,body]:child.items()){
            (void)body;
            if(extension!="VRMC_materials_mtoon"&&extension!="KHR_texture_transform"&&
               extension!="KHR_materials_emissive_strength")
              return false;
            extensions.insert(extension);
          }
        }
        if(!self(self,child))return false;
      }
      return true;
    };
    if(!collect_extensions(collect_extensions,original))return retain_native();
    const auto &native=binding->native_reference;
    const auto &current=field.material;
    auto &params=typed.parameters;
    const auto unit=[](float value){return std::isfinite(value)&&value>=0&&value<=1;};
    for(size_t channel=0;channel<4;++channel){
      auto &factor=params.base_color[channel];
      factor=mtoon_factor(factor,native.color[channel],current.color[channel]);
      if(!unit(factor))return retain_native();
    }
    std::array emission{params.emission.x,params.emission.y,params.emission.z};
    for(size_t channel=0;channel<emission.size();++channel){
      auto &factor=emission[channel];
      factor=mtoon_factor(factor,native.emission[channel],current.emissive[channel]);
      if(!(hdr||try_editable_float)&&!unit(factor))return retain_native();
    }
    params.emission={emission[0],emission[1],emission[2]};
    if(current.double_sided!=native.double_sided)
      params.cull=current.double_sided?MToonCull::Off:MToonCull::Back;
    if(current.alpha!=Material::AlphaMode::Automatic&&int(current.alpha)!=native.alpha){
      params.alpha=current.alpha==Material::AlphaMode::Opaque?MToonAlpha::Opaque:
          current.alpha==Material::AlphaMode::Mask?MToonAlpha::Mask:MToonAlpha::Blend;
      const bool blend=params.alpha==MToonAlpha::Blend;
      params.depth_write=!blend;
      params.source_blend=blend?5:1;
      params.destination_blend=blend?10:0;
      params.queue_offset=0;
      params.render_queue=blend?3000:params.alpha==MToonAlpha::Mask?2450:2000;
    }
    if(current.alpha_cutoff!=native.alpha_cutoff)params.alpha_cutoff=current.alpha_cutoff;
    for(size_t role=0;role<binding->roles.size();++role){
      if(binding->roles[role].image&&role!=size_t(MToonSlot::Matcap)&&typed.textures[role].texcoord&&
         !(role==size_t(MToonSlot::Normal)&&compiled.portable_mtoon_normal_coordinates))
        return retain_native();
    }
    auto roles=binding->roles;
    MToonFloatPreview *float_preview=nullptr;
    if(try_editable_float){
      auto candidate=compile_editable_mtoon_float_preview(field,typed,editable_preview->budget);
      if(!candidate)return retain_native();
      float_preview=&mtoon_float_previews.emplace_back(std::move(*candidate));
      hdr=&float_preview->emission;
      editable_preview->wrote_preview=true;
    }
    std::optional<PortableMToonSetPaint> paint;
    PaintSourceTexture paint_sampler;
    paint_sampler.wrap_s=paint_sampler.wrap_t=33071;
    paint_sampler.min_filter=paint_sampler.mag_filter=9729;
    if(field.painted->has_paint_data()){
      if(variant)paint=compile_retopo_mtoon_paint(field,typed,retopo_paint_limits);
      else if(float_preview)paint=std::move(float_preview->paint);
      else {
        TextureSetLimits limits;
        limits.cancelled=cancelled;
        paint=compile_mtoon_set_paint(field,compiled,typed,limits);
      }
      if(!paint)return retain_native();
      const auto replace_role=[&](MToonSlot slot,const std::shared_ptr<const Image> &image,
                                  MToonEncoding encoding=MToonEncoding::Srgb){
        auto &role=roles[size_t(slot)];
        role=paint_sampler;
        role.image=image;
        typed.textures[size_t(slot)]=MToonTexture{.index=0,.encoding=encoding};
        mtoon_paint_images.push_back(image);
      };
      if(paint->color){
        replace_role(MToonSlot::Base,paint->color);
        params.base_color={1,1,1,paint->alpha_factor};
      }
      if(paint->shade){
        replace_role(MToonSlot::Shade,paint->shade);
        params.shade_color={1,1,1};
      }
      if(paint->emission){
        replace_role(MToonSlot::Emission,paint->emission);
        params.emission={1,1,1};
      }
      if(paint->normal){
        if(!attributes.contains("TANGENT"))throw std::logic_error("Portable MToon Bake needs its canonical tangent stream");
        replace_role(MToonSlot::Normal,paint->normal,MToonEncoding::Normal);
        params.normal_enabled=true;
        params.normal_scale=1;
      }
    }
    if(hdr){
      roles[size_t(MToonSlot::Emission)]={};
      typed.textures[size_t(MToonSlot::Emission)]=MToonTexture{
          .index=texture(hdr->image,paint_sampler),.encoding=MToonEncoding::Srgb};
      params.emission={hdr->factor[0],hdr->factor[1],hdr->factor[2]};
      params.emission_strength=hdr->strength;
    }
    for(size_t role=0;role<roles.size();++role){
      stop();
      const auto &view=roles[role];
      if(view.image)typed.textures[role].index=texture(*view.image,view);
    }
    if(compiled.portable_mtoon_normal_coordinates){
      const auto coordinate=retained_normal_sets.at(object);
      if(!attributes.contains("TEXCOORD_"+std::to_string(coordinate)))
        throw std::logic_error("Compiled MToon normal coordinate accessor is missing");
      auto &normal=typed.textures[size_t(MToonSlot::Normal)];
      normal.texcoord=coordinate;
      normal.transform_texcoord=-1;
      normal.offset={};
      normal.scale={1,1};
      normal.rotation=0;
    }
    auto output=typed.source_json();
    if(variant){
      output.erase("occlusionTexture");
      output["pbrMetallicRoughness"].erase("metallicRoughnessTexture");
    }
    if(params.alpha!=MToonAlpha::Mask)
      output.erase("alphaCutoff");
    if(hdr&&hdr->strength==1){
      // This strength is generated from resolved radiance. Its default
      // representation must not depend on whether the source was reopened.
      output.at("extensions").erase("KHR_materials_emissive_strength");
      extensions.erase("KHR_materials_emissive_strength");
    }
    output["name"]=field.name;
    output["pbrMetallicRoughness"]["metallicFactor"]=current.metallic;
    output["pbrMetallicRoughness"]["roughnessFactor"]=current.roughness;
    if(paint&&(!variant||paint->color||paint->emission)){
      if(!variant){
        auto &pbr=output["pbrMetallicRoughness"];
        const auto orm=texture(float_preview?float_preview->metal_rough:compiled.textures->metal_rough,paint_sampler);
        pbr["metallicFactor"]=pbr["roughnessFactor"]=1;
        pbr["metallicRoughnessTexture"]={{"index",orm}};
        output["occlusionTexture"]={{"index",orm}};
      }
      output["extras"]["domeSculptMToonPaint"]={
          {"version",1},{"sampling","clamp-linear-level-zero"},
          {"maximumLinearColorError",paint->color_error},
          {"maximumAlphaError",paint->alpha_error},
          {"maximumLinearEmissionError",hdr?*hdr->filter_error:variant?paint->emission_error*params.emission_strength:paint->emission_error},
          {"emissiveStrength",params.emission_strength}};
      if(paint->shade){
        auto &bounds=output["extras"]["domeSculptMToonPaint"];
        bounds["maximumLinearShadeError"]=paint->shade_error;
        bounds["maximumShadeInterpolationError"]=paint->shade_interpolation_error;
      }
      if(paint->source_filter_error>0)
        output["extras"]["domeSculptMToonPaint"]["maximumSourceFilterError"]=paint->source_filter_error;
      if(paint->retopo_uv_domain)
        output["extras"]["domeSculptMToonPaint"]["errorDomain"]="mesh-uv-triangles";
      if(paint->normal)
        output["extras"]["domeSculptMToonPaint"]["maximumNormalErrorRadians"]=paint->normal_error_radians;
    }
    if(hdr){
      output["extras"]["domeSculptPortableEmission"]={{"version",1},
          {"evaluation","finite-current-authored-mtoon-texture-set"},{"units","native-linear-radiance"},
          {"lossless",false},{"unlitIgnoresEmission",false},{"errorScope","clamp-linear-level-zero"},
          {"betweenTexelErrorBound",*hdr->filter_error},
          {"maximumAbsoluteError",hdr->measured.maximum_absolute_error},
          {"maximumRelativeError",hdr->measured.maximum_relative_error},
          {"erasedNonzeroSamples",hdr->measured.erased_nonzero_samples}};
    }
    extensions.clear();
    if(!collect_extensions(collect_extensions,output))
      throw std::logic_error("MToon material writing introduced an unknown extension");
    for(const auto &extension:extensions)use_extension(extension.c_str());
    j["materials"][material_id]=std::move(output);
    return true;
  };
  std::array<bool,8> uv_available{};
  auto source_info = [&](const PaintSourceTexture &slot) {
    slot.validate(8);
    if(slot.texcoord<0||slot.texcoord>7||!uv_available[size_t(slot.texcoord)]) throw std::invalid_argument("Export surface has no requested source UV set");
    nlohmann::json info={{"index",texture(*slot.image,slot)}};
    if(slot.texcoord) info["texCoord"]=slot.texcoord;
    const auto &m=slot.uv_transform;
    if(m!=std::array<float,6>{1,0,0,1,0,0}) {
      // KHR_texture_transform represents rotation*scale, never arbitrary shear.
      double sx=std::hypot(double(m[0]),double(m[2]));
      double rotation=sx>1e-15?std::atan2(double(m[2]),double(m[0])):
                                      std::atan2(-double(m[1]),double(m[3]));
      double sy=-std::sin(rotation)*m[1]+std::cos(rotation)*m[3];
      double scale=std::max({1.,sx,std::abs(sy)});
      if(std::abs(-std::sin(rotation)*sy-m[1])>1e-6*scale ||
         std::abs(std::cos(rotation)*sy-m[3])>1e-6*scale)
        throw std::invalid_argument("Source texture shear cannot be represented by KHR_texture_transform");
      info["extensions"]["KHR_texture_transform"]={{"offset",{m[4],m[5]}},
        {"rotation",rotation},{"scale",{sx,sy}}};
      use_extension("KHR_texture_transform");
    }
    return info;
  };
  auto write_textures=[&](const ObjectSurface&object,size_t i,int material_id,bool compiled_paint=false,bool compiled_role=false,const TextureSetExportEmission *field_emission=nullptr){
    if(!object.textures)return;
    if(!j["meshes"][result.objects[i].mesh]["primitives"][0]["attributes"].contains("TEXCOORD_0")) throw std::invalid_argument("Textured meshes require UV coordinates");
    for(size_t coordinate=0;coordinate<8;++coordinate)uv_available[coordinate]=j["meshes"][result.objects[i].mesh]["primitives"][0]["attributes"].contains("TEXCOORD_"+std::to_string(coordinate));
    const auto &maps=*object.textures;
    const bool prepared_color=detail::prepared_color_state(object.material_bindings.get(),&maps);
    detail::prepared_normal_state(object.material_bindings.get(),&maps);
    auto &material=j["materials"][material_id]; auto &pbr=material["pbrMetallicRoughness"];
    if(maps.source_only) {
      auto present=[](const Image &image){return image.width || image.height || !image.rgba.empty();};
      if(!maps.source_maps || present(maps.color) || present(maps.metal_rough) || present(maps.emission) || present(maps.normal) || present(maps.color_coverage) || maps.baked_base || present(maps.baked_normal) || present(maps.baked_coverage) || present(maps.emission_coverage)||present(maps.mtoon_normal)||maps.normal_projection)
        throw std::invalid_argument("Source-only textures require original bindings without derived images");
      const auto &source=*maps.source_maps; source.validate();
      if(source.color.image) pbr["baseColorTexture"]=source_info(source.color);
      if(source.metal_rough.image) pbr["metallicRoughnessTexture"]=source_info(source.metal_rough);
      if(source.emission.image) material["emissiveTexture"]=source_info(source.emission);
    } else {
      PaintSourceTexture sampler; sampler.wrap_s=sampler.wrap_t=33071;
      sampler.min_filter=sampler.mag_filter=9729;
      pbr["baseColorFactor"]={1,1,1,1}; pbr["metallicFactor"]=1; pbr["roughnessFactor"]=1;
      // Match the current viewport's effective sampling of already-composited
      // color/emission. Original transforms are baked into these UV0 images;
      // wrapping and filtering remain per-role, except prepared PBR color.
      auto derived_sampler=[&](const PaintSourceTexture &source){
        auto view=source;view.texcoord=0;view.uv_transform={1,0,0,1,0,0};
        if(!view.min_filter)view.min_filter=9729;
        if(!view.mag_filter)view.mag_filter=9729;
        return view;
      };
      auto color_sampler=sampler,emission_sampler=sampler;
      if(maps.source_maps){
        if(!prepared_color&&maps.source_maps->color.image)color_sampler=derived_sampler(maps.source_maps->color);
        if(maps.source_maps->emission.image)emission_sampler=derived_sampler(maps.source_maps->emission);
      }
      pbr["baseColorTexture"]={{"index",texture(prepared_color?maps.pbr_color:maps.color,color_sampler)}};
      int orm=texture(maps.metal_rough,sampler);
      pbr["metallicRoughnessTexture"]={{"index",orm}};
      material["occlusionTexture"]={{"index",orm}};
      if(!maps.normal.rgba.empty()&&!(maps.normal_projection&&maps.normal_projection->version==2)) material["normalTexture"]={{"index",texture(maps.normal,sampler)}};
      if(field_emission){
        material["emissiveTexture"]={{"index",texture(field_emission->image,sampler)}};
        material["emissiveFactor"]=field_emission->factor;
        if(field_emission->strength>1){
          material["extensions"]["KHR_materials_emissive_strength"]={{"emissiveStrength",field_emission->strength}};
          use_extension("KHR_materials_emissive_strength");
        }
        material["extras"]["domeSculptPortableEmission"]={{"version",1},
          {"evaluation","finite-current-core-pbr-texture-set"},{"units","native-linear-radiance"},
          {"lossless",false},{"unlitIgnoresEmission",object.material.unlit},
          {"errorScope","encoded-texel-quantization"},{"betweenTexelErrorBound",nullptr},
          {"maximumAbsoluteError",field_emission->measured.maximum_absolute_error},
          {"maximumRelativeError",field_emission->measured.maximum_relative_error},
          {"erasedNonzeroSamples",field_emission->measured.erased_nonzero_samples}};
      }else{
        material["emissiveTexture"]={{"index",texture(maps.emission,emission_sampler)}};
        material["emissiveFactor"]={1,1,1};
      }
      if(object.paint_sampling_attachment||compiled_paint) {
        const auto coordinate=retained_paint_sets.at(i);
        const auto &attributes=j["meshes"][result.objects[i].mesh]["primitives"][0]["attributes"];
        if(!attributes.contains("TEXCOORD_"+std::to_string(coordinate)))throw std::logic_error("Compiled paint coordinate accessor is missing");
        if(coordinate)for(auto *info:{&pbr["baseColorTexture"],&pbr["metallicRoughnessTexture"],&material["occlusionTexture"],&material["emissiveTexture"]})(*info)["texCoord"]=coordinate;
        if(coordinate&&object.material_bindings&&object.material_bindings->native_paint_sampling&&
           !maps.normal.rgba.empty())
          material["normalTexture"]["texCoord"]=coordinate;
      }
    }
    if(compiled_role&&!maps.normal.rgba.empty()){
      const auto coordinate=retained_normal_sets.at(i);const auto&attributes=j["meshes"][result.objects[i].mesh]["primitives"][0]["attributes"];
      if(!attributes.contains("TEXCOORD_"+std::to_string(coordinate)))throw std::logic_error("Compiled texture-set normal role accessor is missing");
      if(coordinate)material["normalTexture"]["texCoord"]=coordinate;material["normalTexture"]["scale"]=1;
    }
    if(maps.source_maps) {
      const auto &source=*maps.source_maps; source.validate();
      if(source.normal.image && maps.normal.rgba.empty()) {
        material["normalTexture"]=source_info(source.normal);
        material["normalTexture"]["scale"]=source.normal_scale;
      }
      if(source.occlusion.image && maps.source_only) {
        material["occlusionTexture"]=source_info(source.occlusion);
        material["occlusionTexture"]["strength"]=source.occlusion_strength;
      }
    }
    if(object.material.alpha==Material::AlphaMode::Automatic)
      material["alphaMode"]=(object.material.color[3]<1 || vertex_transparency[i] ||
          (prepared_color?maps.pbr_transparent:maps.transparent))?"BLEND":"OPAQUE";
    if(result.objects[i].canonical_normal.image) {
      material["normalTexture"]=source_info(result.objects[i].canonical_normal);
      material["normalTexture"]["scale"]=1;
    }
    if(detail::retained_normal_coordinates(object.material_bindings.get(),&maps)){
      material["normalTexture"]=source_info(result.objects[i].prepared_normal);
      const auto coordinate=retained_normal_sets.at(i);
      const auto &attributes=j["meshes"][result.objects[i].mesh]["primitives"][0]["attributes"];
      if(!attributes.contains("TEXCOORD_"+std::to_string(coordinate)))throw std::logic_error("Compiled normal coordinate accessor is missing");
      if(coordinate)material["normalTexture"]["texCoord"]=coordinate;
      material["normalTexture"]["scale"]=1;
    }
  };
  for(size_t i=0;i<objects.size();++i){
    const auto *field=bundles[i]&&objects[i].texture_set_surface?&bundles[i]->fields.front():nullptr;
    if(field){
      const auto owners=objects[i].texture_set_surface->fields();
      const auto original=std::ranges::find(owners,field->set,&TextureSetView::id);
      if(original==owners.end())throw std::logic_error("Exported material set lost its owning field");
      if(write_set_mtoon(*original,*field,i,0,result.objects[i].material))continue;
    }
    write_textures(object_at(i),i,result.objects[i].material,
        field&&!bundles[i]->paint_coordinates.empty(),field&&field->normal_role,
        field&&field->emission?&*field->emission:nullptr);
    if(!field&&bundles[i]&&objects[i].retopo_mtoon_export){
      auto &materials=j["materials"];
      const int id=int(materials.size());
      materials.push_back(nlohmann::json::object());
      if(write_set_mtoon(objects[i].retopo_mtoon_export->field(),bundles[i]->fields.front(),i,0,id,true)){
        use_extension("KHR_materials_variants");
        j["extensions"]["KHR_materials_variants"]["variants"]={{{"name","MToon"}}};
        const auto &identity=result.objects[i];
        auto &primitive=j["meshes"][identity.mesh]["primitives"][0];
        primitive["extensions"]["KHR_materials_variants"]["mappings"]={{{"material",id},{"variants",{0}}}};
      }else materials.erase(materials.size()-1);
    }
  }
  for(const auto&extra:extra_materials){
    stop();const auto&field=bundles[extra.object]->fields[extra.field];ObjectSurface material_view;
    const auto owners=objects[extra.object].texture_set_surface->fields();
    const auto original=std::ranges::find(owners,field.set,&TextureSetView::id);
    if(original==owners.end())throw std::logic_error("Exported material set lost its owning field");
    if(write_set_mtoon(*original,field,extra.object,extra.field,extra.material))continue;
    material_view.material=field.material;material_view.textures=field.textures;
    write_textures(material_view,extra.object,extra.material,!bundles[extra.object]->paint_coordinates.empty(),field.normal_role,field.emission?&*field.emission:nullptr);
  }
  return result;
}
GltfDocument make_gltf(const std::vector<ObjectSurface> &objects,SourceMorphSpace space,
    bool recompute_changed_normals,std::function<bool()> cancelled) {
  return make_gltf_impl(objects,space,recompute_changed_normals,std::move(cancelled),nullptr,nullptr,nullptr);
}
GltfDocument make_gltf_emission(const EmissionExportPrepared &prepared,bool recompute_changed_normals,std::function<bool()> cancelled){
 validate_native_emission_surfaces(prepared,cancelled);
 return make_gltf_impl(prepared.surfaces,SourceMorphSpace::Native,recompute_changed_normals,std::move(cancelled),nullptr,&prepared,nullptr);
}
GltfDocument make_gltf_authoring(const NativeAuthoringCapture &capture,
    const NativeAuthoringCompilation &compiled,bool recompute_changed_normals,
    std::function<bool()> cancelled,bool *wrote_preview) {
  const auto &objects=compiled.surfaces;
  std::optional<EditableMToonExport> preview;
  if(wrote_preview){
    preview.emplace(EditableMToonExport{compiled.texture_set_preview,*wrote_preview});
    preview->budget.cancelled=cancelled;
  }
  auto document=make_gltf_impl(objects,SourceMorphSpace::Native,recompute_changed_normals,cancelled,
      &capture,nullptr,nullptr,preview?&*preview:nullptr);
  auto &j=document.json;
  for(size_t i=0;i<objects.size();++i) {
    if(cancelled&&cancelled())throw std::runtime_error("Raw source emission finish cancelled");
    const auto &object=objects[i];
    if(object.texture_set_surface)continue;
    if(!hdr_emission(object.material.emissive)&&
       !(object.material_bindings&&physical_emission(*object.material_bindings)))continue;
    if(native_paint_compiled_emission(capture,object,cancelled))continue;
    const auto factors=raw_source_emission(capture,object,cancelled);
    if(!factors)throw std::invalid_argument("Raw emission output lacks its authenticated source-role replay");
    auto &identity=document.objects.at(i);
    if(identity.node<0||identity.mesh<0||identity.material<0||
       j.at("nodes").at(identity.node).at("mesh")!=identity.mesh)
      throw std::logic_error("Raw emission output lost its actual emitted object mapping");
    for(const auto *table:{"materials","meshes"})if(j.at(table).size()>=65536)
      throw std::length_error("Raw emission output resource table exceeds limits");
    size_t mesh_users=0;for(const auto &node:j.at("nodes"))
      if(node.contains("mesh")&&node.at("mesh")==identity.mesh)++mesh_users;
    if(mesh_users>1){auto mesh=j.at("meshes").at(identity.mesh);identity.mesh=int(j.at("meshes").size());
      j["meshes"].push_back(std::move(mesh));j["nodes"][identity.node]["mesh"]=identity.mesh;}
    auto &primitives=j.at("meshes").at(identity.mesh).at("primitives");
    if(primitives.size()!=1||primitives.at(0).at("material")!=identity.material)
      throw std::logic_error("Raw emission output primitive differs from its emitted material mapping");
    size_t material_users=0;for(const auto &mesh:j.at("meshes"))for(const auto &p:mesh.at("primitives"))
      if(p.contains("material")&&p.at("material")==identity.material)++material_users;
    if(material_users>1){auto material=j.at("materials").at(identity.material);identity.material=int(j.at("materials").size());
      j["materials"].push_back(std::move(material));primitives[0]["material"]=identity.material;}
    auto &material=j["materials"].at(identity.material);
    material["emissiveFactor"]=factors->factor;
    if(factors->strength>1){material["extensions"]["KHR_materials_emissive_strength"]={{"emissiveStrength",factors->strength}};
      auto &used=j["extensionsUsed"];if(!used.is_array())used=nlohmann::json::array();
      if(std::find(used.begin(),used.end(),"KHR_materials_emissive_strength")==used.end())used.push_back("KHR_materials_emissive_strength");}
    material["extras"]["domeSculptPortableEmission"]={{"version",1},{"evaluation","current-raw-source-sampling"},
      {"units","native-linear-radiance"},{"lossless",false},{"imageResampled",false},
      {"originalFactorsRetained",factors->original_factors},{"maximumFactorProductError",factors->product_error},
      {"unlitIgnoresEmission",object.material.unlit}};
  }
  return document;
}
GltfDocument make_vrm1_authoring_gltf(const Vrm1AuthoringCompilation &owner,
 const std::function<bool()> &cancelled){
 return make_gltf_impl(owner.surfaces(),SourceMorphSpace::Native,true,cancelled,nullptr,nullptr,&owner);
}

} // namespace detail
void detail::write_glb(GltfDocument document,const std::filesystem::path &path) {
  write_glb(std::move(document),path,{},{});
}
void detail::write_glb(GltfDocument document,const std::filesystem::path &path,
    const std::function<bool()> &cancelled,const std::function<void()> &before_commit) {
  auto stop=[&]{if(cancelled && cancelled())throw std::runtime_error("glTF export cancelled");};
  stop();
  auto &j=document.json; auto &data=document.binary;
  while(data.size()%4) data.push_back(0);
  j["buffers"] = {{{"byteLength", data.size()}}};
  std::string text = j.dump();
  stop();
  while (text.size() % 4)
    text.push_back(' ');
  uint64_t length = 12 + 8 + text.size() + 8 + data.size();
  if (length > UINT32_MAX)
    throw std::length_error("glTF exceeds the GLB size limit");
  std::vector<uint8_t> output;
  output.reserve(length);
  append(output, 0x46546c67);
  append(output, 2);
  append(output, uint32_t(length));
  append(output, uint32_t(text.size()));
  append(output, 0x4e4f534a);
  auto copy=[&](const auto &bytes){
    for(size_t offset=0;offset<bytes.size();) {
      stop();auto count=std::min(bytes.size()-offset,size_t(1024*1024));
      output.insert(output.end(),bytes.begin()+offset,bytes.begin()+offset+count);
      offset+=count;
    }
  };
  copy(text);
  append(output, uint32_t(data.size()));
  append(output, 0x004e4942);
  copy(data);
  atomic_write(path, output,AtomicWriteOptions{cancelled,before_commit});
}
} // namespace dome
