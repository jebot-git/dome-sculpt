// SPDX-License-Identifier: GPL-3.0-or-later
#include "vrm-import.hpp"
#include "vrm1-metadata.hpp"
#include <set>
namespace dome::detail {
namespace {
uint32_t index(const nlohmann::json &v, size_t count, const char *label) {
  if (!v.is_number_integer() ||
      (v.is_number_unsigned()
           ? v.get<uint64_t>() >= count
           : v.get<int64_t>() < 0 || uint64_t(v.get<int64_t>()) >= count))
    throw std::invalid_argument(std::string("VRM import invalid ") + label);
  return v.get<uint32_t>();
}
void stop(const std::function<bool()> &cancelled) {
  if (cancelled && cancelled())
    throw std::runtime_error("VRM source adoption cancelled");
}
void finite_vector(const nlohmann::json &value, size_t size) {
  if (!value.is_array() || value.size() != size)
    throw std::invalid_argument("VRM1 binding has an invalid vector dimension");
  for (const auto &component : value)
    if (!component.is_number() || !std::isfinite(component.get<double>()))
      throw std::invalid_argument(
          "VRM1 binding has a nonfinite numeric vector");
}
void vrm1_references(const nlohmann::json &source, const ImportedAsset &asset,
                     const std::function<bool()> &cancelled) {
  if (source.at("meta").contains("thumbnailImage"))
    index(source.at("meta").at("thumbnailImage"), asset.images.size(),
          "thumbnail image");
  if (source.contains("firstPerson")) {
    const auto &first = source.at("firstPerson");
    if (first.contains("meshAnnotations")) {
      const auto &annotations = first.at("meshAnnotations");
      if (!annotations.is_array())
        throw std::invalid_argument("VRM1 mesh annotations must be an array");
      for (const auto &annotation : annotations) {
        stop(cancelled);
        index(annotation.at("node"), asset.nodes.size(), "first-person node");
        const auto type = annotation.at("type").get<std::string>();
        if (type != "auto" && type != "both" && type != "thirdPersonOnly" &&
            type != "firstPersonOnly")
          throw std::invalid_argument("VRM1 first-person type is unsupported");
      }
    }
  }
  if (!source.contains("expressions"))
    return;
  const auto &expressions = source.at("expressions");
  if (!expressions.is_object())
    throw std::invalid_argument("VRM1 expressions must be an object");
  for (const char *table : {"preset", "custom"}) {
    if (!expressions.contains(table))
      continue;
    if (!expressions.at(table).is_object())
      throw std::invalid_argument("VRM1 expression table must be an object");
    for (const auto &entry : expressions.at(table).items()) {
      stop(cancelled);
      const auto &expression = entry.value();
      if (!expression.is_object())
        throw std::invalid_argument("VRM1 expression must be an object");
      if (expression.contains("isBinary") &&
          !expression.at("isBinary").is_boolean())
        throw std::invalid_argument(
            "VRM1 expression binary flag must be boolean");
      for (const char *key :
           {"overrideBlink", "overrideLookAt", "overrideMouth"})
        if (expression.contains(key)) {
          const auto mode = expression.at(key).get<std::string>();
          if (mode != "none" && mode != "block" && mode != "blend")
            throw std::invalid_argument("VRM1 expression override is invalid");
        }
      for (const char *key : {"morphTargetBinds", "materialColorBinds",
                              "textureTransformBinds"}) {
        if (!expression.contains(key))
          continue;
        const auto &binds = expression.at(key);
        if (!binds.is_array() || binds.empty())
          throw std::invalid_argument(
              "VRM1 expression bindings must be a nonempty array");
        for (const auto &bind : binds) {
          stop(cancelled);
          if (std::string_view(key) == "morphTargetBinds") {
            auto node =
                index(bind.at("node"), asset.nodes.size(), "expression node");
            auto mesh = asset.nodes[node].mesh;
            if (mesh < 0)
              throw std::invalid_argument("VRM1 expression node has no mesh");
            for (auto primitive : asset.meshes.at(size_t(mesh)).primitives)
              index(bind.at("index"),
                    asset.primitives.at(primitive).morph_targets.size(),
                    "expression target");
            if (!bind.at("weight").is_number() ||
                !std::isfinite(bind.at("weight").get<double>()))
              throw std::invalid_argument(
                  "VRM1 expression weight must be finite");
          } else {
            index(bind.at("material"), asset.materials.size() - 1,
                  "expression material");
            if (std::string_view(key) == "materialColorBinds") {
              const auto type = bind.at("type").get<std::string>();
              if (!std::set<std::string>{"color", "emissionColor", "shadeColor",
                                         "matcapColor", "rimColor",
                                         "outlineColor"}
                       .contains(type))
                throw std::invalid_argument(
                    "VRM1 expression material color type is invalid");
              finite_vector(bind.at("targetValue"), 4);
            } else {
              if(bind.contains("scale"))finite_vector(bind.at("scale"), 2);
              if(bind.contains("offset"))finite_vector(bind.at("offset"), 2);
            }
          }
        }
      }
    }
  }
}
std::string native_role(std::string name, VrmImportVersion version) {
  if (version == VrmImportVersion::V0)
    for (const char *side : {"left", "right"}) {
      if (name == std::string(side) + "ThumbProximal")
        return std::string(side) + "ThumbMetacarpal";
      if (name == std::string(side) + "ThumbIntermediate")
        return std::string(side) + "ThumbProximal";
    }
  return name;
}
} // namespace
VrmImportVersion vrm_import_version(const nlohmann::json &j) {
  if (!j.contains("extensions"))
    return VrmImportVersion::None;
  const auto &e = j.at("extensions");
  const bool v0 = e.contains("VRM"), v1 = e.contains("VRMC_vrm");
  if (v0 && v1)
    throw std::invalid_argument(
        "VRM import cannot infer a version from mixed VRM0/VRM1 metadata");
  if (!v0 && !v1)
    return VrmImportVersion::None;
  const auto &version = e.at(v0 ? "VRM" : "VRMC_vrm").at("specVersion");
  if (!version.is_string() ||
      version.get_ref<const std::string &>() != (v0 ? "0.0" : "1.0"))
    throw std::invalid_argument(
        "VRM import source version requires an explicit adapter");
  return v0 ? VrmImportVersion::V0 : VrmImportVersion::V1;
}
VrmImportMetadata vrm_import_metadata(const nlohmann::json &j,
                                      const ImportedAsset &asset,
                                      const std::function<bool()> &cancelled) {
  VrmImportMetadata out;
  out.version = vrm_import_version(j);
  if (out.version == VrmImportVersion::None)
    return out;
  stop(cancelled);
  out.source =
      j.at("extensions")
          .at(out.version == VrmImportVersion::V0 ? "VRM" : "VRMC_vrm");
  if (out.version == VrmImportVersion::V0) {
    Vrm0Metadata::parse(out.source, asset.nodes.size(), asset.meshes.size(),
                        asset.textures.size(), asset.materials.size() - 1);
    out.vrm0_json = std::make_shared<const std::string>(out.source.dump());
    for (const auto &bone : out.source.at("humanoid").at("humanBones")) {
      const auto name =
          native_role(bone.at("bone").get<std::string>(), out.version);
      if (!out.human_nodes
               .emplace(name, index(bone.at("node"), asset.nodes.size(),
                                    "humanoid node"))
               .second)
        throw std::invalid_argument("VRM import repeats a humanoid role");
    }
  } else {
    validate_vrm1_metadata(j,cancelled);
    // Version-specific structural/reference checks; unknown metadata remains in
    // the original source, never reinterpreted as VRM0 material properties.
    const auto &meta = out.source.at("meta"),
               &bones = out.source.at("humanoid").at("humanBones");
    if (!meta.is_object() || !meta.at("name").is_string() ||
        meta.at("name").get_ref<const std::string &>().empty() ||
        !meta.at("licenseUrl").is_string() ||
        meta.at("licenseUrl").get_ref<const std::string &>().empty() ||
        !meta.at("authors").is_array() || meta.at("authors").empty() ||
        !bones.is_object() || bones.size() > 55)
      throw std::invalid_argument(
          "VRM1 import requires complete meta and humanoid authority");
    for (const auto &author : meta.at("authors"))
      if (!author.is_string() || author.get_ref<const std::string &>().empty())
        throw std::invalid_argument(
            "VRM1 author identity must be a nonempty string");
    vrm1_references(out.source, asset, cancelled);
    for (const auto &bone : bones.items())
      if (!out.human_nodes
               .emplace(bone.key(), index(bone.value().at("node"),
                                          asset.nodes.size(), "humanoid node"))
               .second)
        throw std::invalid_argument("VRM1 import repeats a humanoid role");
  }
  std::set<uint32_t> nodes;
  for (const auto &[name, node] : out.human_nodes)
    nodes.insert(node);
  for (const auto &skin : asset.skins)
    for (auto joint : skin.joints)
      nodes.insert(joint);
  out.rig_nodes.assign(nodes.begin(), nodes.end());
  out.materials.resize(asset.materials.size());
  out.material_properties.resize(asset.materials.size());
  for (size_t material = 1; material < asset.materials.size(); ++material) {
    stop(cancelled);
    if (out.version == VrmImportVersion::V0) {
      const auto &p = out.source.at("materialProperties").at(material - 1);
      out.material_properties[material] = p.dump();
      const auto &shader = p.at("shader").get_ref<const std::string &>();
      if (shader == "VRM/MToon")
        out.materials[material] =
            MToonMaterial::from_vrm0(p, asset.textures.size());
      else if (shader != "VRM_USE_GI")
        throw std::invalid_argument(
            "VRM0 source shader requires an explicit native adapter: " +
            shader);
    } else {
      const auto &m = j.at("materials").at(material - 1);
      if (m.contains("extensions") &&
          m.at("extensions").contains("VRMC_materials_mtoon"))
        out.materials[material] =
            MToonMaterial::from_vrm1(m, asset.textures.size());
    }
  }
  // Typed texture roles can sample a channel even when every core PBR role
  // is absent. Never invent an original sampling UV by generating a paint
  // atlas.
  for (const auto &primitive : asset.primitives) {
    stop(cancelled);
    const auto &typed = out.materials.at(primitive.material);
    if (!typed)
      continue;
    for (size_t slot=0;slot<typed->textures.size();++slot) {
      const auto &role=typed->textures[slot];
      // MatCap samples a view-space normal, not either mesh UV accessor.
      if (role.index < 0 || slot==size_t(MToonSlot::Matcap))
        continue;
      const int coordinate = typed->version() == MToonVersion::V0 ? 0
                             : role.transform_texcoord >= 0
                                 ? role.transform_texcoord
                                 : int(role.texcoord);
      const auto &uv = coordinate == 0 ? primitive.mesh.uv : primitive.uv1;
      if (coordinate < 0 || coordinate > 1 ||
          uv.size() != primitive.mesh.positions.size())
        throw std::invalid_argument(
            "VRM typed texture role requires original TEXCOORD_" +
            std::to_string(coordinate));
    }
  }
  return out;
}
HumanoidMapping vrm_import_humanoid(const VrmImportMetadata &metadata,
                                    const ImportedRig &rig) {
  HumanoidMapping mapping;
  for (const auto &[role, node] : metadata.human_nodes) {
    auto i = std::find(rig.source_nodes.begin(), rig.source_nodes.end(), node);
    if (i == rig.source_nodes.end())
      throw std::invalid_argument(
          "VRM humanoid node is outside complete imported rig");
    mapping.joints.emplace(role, uint32_t(i - rig.source_nodes.begin()));
  }
  mapping.validate(rig.skeleton);
  return mapping;
}
std::optional<AvatarExpressions>
vrm_import_visemes(const VrmImportMetadata &metadata, int mesh, int node,
                   const MorphSet &morphs) {
  AvatarExpressions out;
  const std::map<std::string, std::string> names{
      {"a", "aa"}, {"i", "ih"}, {"u", "ou"}, {"e", "ee"}, {"o", "oh"}};
  if (metadata.version == VrmImportVersion::V0) {
    for (const auto &group :
         metadata.source.at("blendShapeMaster")
             .value("blendShapeGroups", nlohmann::json::array())) {
      if (group.value("isBinary", false))
        continue; // Never reinterpret a binary driver as linear.
      const auto preset = group.value("presetName", std::string{});
      auto found = names.find(preset);
      if (found == names.end())
        continue;
      for (const auto &bind : group.value("binds", nlohmann::json::array()))
        if (bind.at("mesh").get<int>() == mesh) {
          auto target = index(bind.at("index"), morphs.targets.size(),
                              "expression target");
          float weight = bind.at("weight").get<float>() * .01f;
          if (weight > 0)
            out.visemes[found->second].push_back(
                {morphs.targets[target].name, weight});
        }
    }
  }
  if (metadata.version == VrmImportVersion::V1 &&
      metadata.source.contains("expressions")) {
    const auto &expressions = metadata.source.at("expressions");
    if (expressions.contains("preset"))
      for (const auto &item : expressions.at("preset").items()) {
        const auto &group = item.value();
        if (group.value("isBinary", false) ||
            !std::set<std::string>{"aa", "ih", "ou", "ee", "oh"}.contains(
                item.key()))
          continue;
        if (group.contains("morphTargetBinds"))
          for (const auto &bind : group.at("morphTargetBinds"))
            if (bind.at("node").get<int>() == node) {
              auto target = index(bind.at("index"), morphs.targets.size(),
                                  "expression target");
              float weight = bind.at("weight").get<float>();
              if (!std::isfinite(weight))
                throw std::invalid_argument(
                    "VRM1 expression weight is nonfinite");
              if (weight > 0)
                out.visemes[item.key()].push_back(
                    {morphs.targets[target].name, weight});
            }
      }
  }
  // Native vowel preview is a subset. Binary/material/look-at/physics bindings
  // remain exact raw source metadata and need their own versioned driver.
  if (out.visemes.empty())
    return {};
  out.validate(morphs);
  return out;
}
std::array<PaintSourceTexture, size_t(MToonSlot::Count)>
vrm_import_roles(const MToonMaterial &m, const ImportedAsset &asset,
                 const std::vector<std::shared_ptr<const Image>> &images) {
  std::array<PaintSourceTexture, size_t(MToonSlot::Count)> roles;
  for (size_t role = 0; role < roles.size(); ++role)
    if (m.textures[role].index >= 0) {
      const auto &t = asset.textures.at(size_t(m.textures[role].index));
      auto &view = roles[role];
      view.image = images.at(size_t(t.image));
      view.wrap_s = t.wrap_s;
      view.wrap_t = t.wrap_t;
      view.min_filter = t.min_filter;
      view.mag_filter = t.mag_filter;
    }
  return roles;
}
} // namespace dome::detail
