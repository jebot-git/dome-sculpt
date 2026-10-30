// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/material-bindings.hpp"
#include "dome/scene.hpp"
#include "dome/gltf-export.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/source-maps.hpp"
#include "dome/tangents.hpp"
#include "io.hpp"
#include "gltf-document.hpp"
#include "normal-frame-export.hpp"
#include <bit>
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
  auto document=detail::make_gltf(objects,detail::SourceMorphSpace::Native,
                                 options.recompute_changed_normals,options.cancelled);
  document.json["buffers"]={{{"byteLength",document.binary.size()}}};
  uint64_t bytes=28+((uint64_t(document.json.dump().size())+3)&~uint64_t(3))+
                    ((uint64_t(document.binary.size())+3)&~uint64_t(3));
  if(bytes>options.max_output_bytes)throw std::length_error("glTF output resource budget exceeded");
  GltfExportReport report{objects.size(),document.normal_targets_recomputed,{}};
  if(report.normal_targets_recomputed)report.diagnostics.push_back(
    "Source NORMAL deltas recomputed from edited geometry; original authoring data remains unchanged");
  stop();detail::write_glb(std::move(document),path);return report;
}
detail::GltfDocument detail::make_gltf(const std::vector<ObjectSurface> &input_objects,
    SourceMorphSpace source_space,bool recompute_changed_normals,std::function<bool()> cancelled) {
  GltfDocument result;
  const auto &objects=input_objects;
  if (objects.empty() || objects.size()>256)
    throw std::invalid_argument("The scene has no visible surface to export");

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
    for (float f : values)
      append(data, std::bit_cast<uint32_t>(f));
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
  std::map<uint64_t, int> group_nodes;
  std::map<uint64_t, nlohmann::json> group_records;
  for (const auto &object : objects) {
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
      auto record = node;
      record["parentId"] = parent;
      if (auto it = group_records.find(g.id); it != group_records.end()) {
        if (it->second != record) throw std::invalid_argument("Conflicting export group definitions");
      } else {
        if (group_nodes.size() >= 128) throw std::length_error("Export group limit exceeded");
        int index = int(j["nodes"].size());
        j["nodes"].push_back(std::move(node));
        group_nodes[g.id] = index;
        group_records[g.id] = std::move(record);
        if (parent) j["nodes"][group_nodes.at(parent)]["children"].push_back(index);
        else j["scenes"][0]["nodes"].push_back(index);
      }
      parent = g.id;
    }
    validate_transform(object.world_transform());
  }
  struct SharedRig { int root, first_joint, skin; };
  std::map<std::string, SharedRig> shared_rigs;
  for (const auto &input : objects) {
    std::optional<NormalFrameExport> prepared;
    if(input.material_bindings && input.material_bindings->normal_frames)prepared=normal_frame_export(input,source_space,cancelled);
    const auto &o=prepared?prepared->surface:input;auto *frame=prepared?&*prepared:nullptr;
    GltfObjectIdentity emission;
    if(frame)emission.canonical_normal=frame->canonical_normal;
    auto &mesh = o.mesh;
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
    if (!mesh.uv.empty()) {
      begin = data.size();
      for (auto p : mesh.uv)
        floats(p);
      attributes["TEXCOORD_0"] =
          add(begin, data.size() - begin, 34962, mesh.uv.size(), 5126, "VEC2");
    }
    std::vector<Tangent> tangents=frame?frame->tangents:std::vector<Tangent>{};
    if(auto source=frame?frame->source:source_attributes->source()) {
      const auto &a=*o.appearance;
      auto attribute=[&](const auto &values,const char *name,const char *type) {
        if(values.empty())return;
        begin=data.size();for(auto v:a.source_vertices){if(v>=values.size())throw std::invalid_argument("Source attribute vertex index is missing");floats(values[v]);}
        attributes[name]=add(begin,data.size()-begin,34962,mesh.positions.size(),5126,type);
      };
      attribute(source->uv1,"TEXCOORD_1","VEC2");attribute(source->colors,"COLOR_0","VEC4");
      if(!frame)tangents=source_attributes->base_tangents();
      if(!frame && !tangents.empty() && o.textures && !o.textures->normal.rgba.empty()) {
        auto canonical=tangent_corners(mesh,cancelled);
        for(size_t corner=0;corner<canonical.size();++corner) {
          auto authored=tangents.at(mesh.indices[corner]);auto target=canonical[corner];
          if(length(Vec3{authored[0]-target[0],authored[1]-target[1],authored[2]-target[2]})>1e-4f || authored[3]*target[3]<0)
            throw std::invalid_argument("Canonical baked normals conflict with retained source tangents; explicit tangent basis conversion is required");
        }
      }
    }
    if(!tangents.empty()){
      begin=data.size();for(auto t:tangents)floats(t);
      attributes["TANGENT"]=add(begin,data.size()-begin,34962,mesh.positions.size(),5126,"VEC4");
    }
    if (o.deformation && o.deformation->skin) {
      const auto &weights = o.deformation->skin->vertices;
      size_t sets = 0;
      for (const auto &vertex : weights)
        sets = std::max(sets, (vertex.size() + 3) / 4);
      if(!weights.empty() && sets>(256*1024*1024-std::min(data.size(),size_t(256*1024*1024)))/(weights.size()*24))throw std::length_error("Padded skin attribute arrays exceed glTF output resource bound");
      for (size_t set = 0; set < sets; ++set) {
        begin = data.size();
        for (const auto &vertex : weights)
          for (size_t k = 0; k < 4; ++k) {
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
    for (auto index : mesh.indices)
      append(data, index);
    int indices = add(begin, data.size() - begin, 34963, mesh.indices.size(),
                      5125, "SCALAR");
    int node = int(j["nodes"].size()), material = int(j["materials"].size()),
        mesh_index = int(j["meshes"].size());
    emission.node=node; emission.mesh=mesh_index; emission.material=material;
    auto t = o.transform;
    auto &m = o.material;
    if (o.groups.empty()) j["scenes"][0]["nodes"].push_back(node);
    else j["nodes"][group_nodes.at(o.groups.back().id)]["children"].push_back(node);
    j["nodes"].push_back(
        {{"name", o.name},
         {"mesh", mesh_index},
         {"translation", {t.position.x, t.position.y, t.position.z}},
         {"rotation",
          {t.orientation.x, t.orientation.y, t.orientation.z, t.orientation.w}},
         {"scale", {t.scale, t.scale, t.scale}}});
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
          SourceMorphChannels directions;
          if(frame){directions.normals=frame->normal_targets.at(native_target);directions.tangents=frame->tangent_targets.at(native_target);++native_target;}
          else directions=source_attributes->target(native_target++,recompute_changed_normals);
          auto direction=[&](const std::vector<Vec3> &values,const char *name){
            if(values.empty())return;
            begin=data.size();
            for(auto p:values)floats(std::array{p.x,p.y,p.z});
            channels[name]=add(begin,data.size()-begin,34962,values.size(),5126,"VEC3");
          };
          direction(directions.normals,"NORMAL");direction(directions.tangents,"TANGENT");
          result.normal_targets_recomputed+=directions.normals_recomputed;
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
        // Object TRS parents joints. A skinned surface becomes an identity
        // scene root because glTF ignores skinned-node ancestor transforms.
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
          for(const auto &pose:d.pose) identity["pose"].push_back({
            {"t",{pose.translation.x,pose.translation.y,pose.translation.z}},
            {"r",{pose.rotation.w,pose.rotation.x,pose.rotation.y,pose.rotation.z}},
            {"s",{pose.scale.x,pose.scale.y,pose.scale.z}}});
          if(d.humanoid) identity["humanoid"]=d.humanoid->encode();
          rig_key=identity.dump();
        }
        if (!rig_key.empty() && shared_rigs.contains(rig_key)) {
          // All parts retain separate geometry/morphs but share one actual rig.
          const auto &shared=shared_rigs.at(rig_key);
          j["nodes"][node].erase("children");
          j["scenes"][0]["nodes"].push_back(mesh_node);
          j["nodes"][mesh_node]["skin"]=shared.skin;
          for(size_t k=0;k<s.bones.size();++k) emission.joints.push_back(uint32_t(shared.first_joint+k));
        } else {
        int first_joint = int(j["nodes"].size());
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
        }
        for (size_t index = 0; index < s.bones.size(); ++index) {
          int parent = s.bones[index].parent < 0
                           ? node
                           : first_joint + s.bones[index].parent;
          j["nodes"][parent]["children"].push_back(first_joint + int(index));
        }
        if (d.skin) {
          j["nodes"][node]["children"] = nlohmann::json::array();
          j["scenes"][0]["nodes"].push_back(mesh_node);
          // Restore rig roots after detaching the skinned surface.
          for (size_t index = 0; index < s.bones.size(); ++index)
            if (s.bones[index].parent < 0)
              j["nodes"][node]["children"].push_back(first_joint + int(index));
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
          if (!d.pose.empty())
            for (const auto &t : d.pose)
              if (t.scale.x <= 0 || t.scale.y <= 0 || t.scale.z <= 0)
                throw std::invalid_argument(
                    "Humanoid export requires positive pose scales");
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
    j["materials"].push_back(
        {{"name", m.name},
         {"pbrMetallicRoughness",
          {{"baseColorFactor", m.color},
           {"metallicFactor", m.metallic},
           {"roughnessFactor", m.roughness}}},
         {"emissiveFactor", m.emissive},
         {"doubleSided", m.double_sided},
         {"alphaMode", m.color[3] < 1 ? "BLEND" : "OPAQUE"}});
    if(frame) {
      auto &extra=j["meshes"][mesh_index]["primitives"][0]["extras"]["domeSculptNormalTransport"];
      extra={{"version",1},{"skinPolicy","weighted-joint-inverse-transpose-1"},{"frameTargetIds",nlohmann::json::array()}};
      for(auto id:o.material_bindings->normal_frames->current_targets)extra["frameTargetIds"].push_back(std::to_string(id));
    }
    result.objects.push_back(std::move(emission));
    auto &encoded = j["materials"].back();
    if (m.alpha != Material::AlphaMode::Automatic)
      encoded["alphaMode"] = m.alpha == Material::AlphaMode::Mask ? "MASK" :
                             m.alpha == Material::AlphaMode::Blend ? "BLEND" : "OPAQUE";
    if (m.alpha == Material::AlphaMode::Mask) encoded["alphaCutoff"] = m.alpha_cutoff;
    if (m.unlit) {
      encoded["extensions"]["KHR_materials_unlit"] = nlohmann::json::object();
      auto &used = j["extensionsUsed"];
      if (!used.is_array()) used = nlohmann::json::array();
      if (std::find(used.begin(), used.end(), "KHR_materials_unlit") == used.end())
        used.push_back("KHR_materials_unlit");
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
  bool secondary_uv_available=false;
  auto source_info = [&](const PaintSourceTexture &slot) {
    slot.validate();
    if(slot.texcoord!=0 && !(slot.texcoord==1 && secondary_uv_available)) throw std::invalid_argument("Export surface has no requested source UV set");
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
  for(size_t i=0;i<objects.size();++i) if(objects[i].textures) {
    if(objects[i].mesh.uv.empty()) throw std::invalid_argument("Textured meshes require UV coordinates");
    secondary_uv_available=j["meshes"][result.objects[i].mesh]["primitives"][0]["attributes"].contains("TEXCOORD_1");
    const auto &maps=*objects[i].textures;
    auto &material=j["materials"][i]; auto &pbr=material["pbrMetallicRoughness"];
    if(maps.source_only) {
      auto present=[](const Image &image){return image.width || image.height || !image.rgba.empty();};
      if(!maps.source_maps || present(maps.color) || present(maps.metal_rough) || present(maps.emission) || present(maps.normal) || present(maps.color_coverage) || maps.baked_base || present(maps.baked_normal) || present(maps.baked_coverage) || present(maps.emission_coverage))
        throw std::invalid_argument("Source-only textures require original bindings without derived images");
      const auto &source=*maps.source_maps; source.validate();
      if(source.color.image) pbr["baseColorTexture"]=source_info(source.color);
      if(source.metal_rough.image) pbr["metallicRoughnessTexture"]=source_info(source.metal_rough);
      if(source.emission.image) material["emissiveTexture"]=source_info(source.emission);
    } else {
      PaintSourceTexture sampler; sampler.wrap_s=sampler.wrap_t=33071;
      sampler.min_filter=sampler.mag_filter=9729;
      pbr["baseColorFactor"]={1,1,1,1}; pbr["metallicFactor"]=1; pbr["roughnessFactor"]=1;
      pbr["baseColorTexture"]={{"index",texture(maps.color,sampler)}};
      int orm=texture(maps.metal_rough,sampler);
      pbr["metallicRoughnessTexture"]={{"index",orm}};
      material["occlusionTexture"]={{"index",orm}};
      if(!maps.normal.rgba.empty()) material["normalTexture"]={{"index",texture(maps.normal,sampler)}};
      material["emissiveTexture"]={{"index",texture(maps.emission,sampler)}};
      material["emissiveFactor"]={1,1,1};
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
    if(!maps.source_only && objects[i].material.alpha==Material::AlphaMode::Automatic)
      material["alphaMode"]=maps.transparent?"BLEND":"OPAQUE";
    if(result.objects[i].canonical_normal.image) {
      material["normalTexture"]=source_info(result.objects[i].canonical_normal);
      material["normalTexture"]["scale"]=1;
    }
  }
  return result;
}
void detail::write_glb(GltfDocument document,const std::filesystem::path &path) {
  auto &j=document.json; auto &data=document.binary;
  while(data.size()%4) data.push_back(0);
  j["buffers"] = {{{"byteLength", data.size()}}};
  std::string text = j.dump();
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
  output.insert(output.end(), text.begin(), text.end());
  append(output, uint32_t(data.size()));
  append(output, 0x004e4942);
  output.insert(output.end(), data.begin(), data.end());
  atomic_write(path, output);
}
} // namespace dome
