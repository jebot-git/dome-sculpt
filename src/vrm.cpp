// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/material-bindings.hpp"
#include "normal-frame-runtime.hpp"
#include "dome/vrm.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/mtoon.hpp"
#include "gltf-document.hpp"
#include "vrm-painted.hpp"
#include <bit>
#include <set>
namespace dome {
namespace {
using Json = nlohmann::json;
[[noreturn]] void fail(const std::string &s) {
  throw std::invalid_argument("VRM0: " + s);
}
size_t index(const Json &v, size_t bound, const char *field,
             bool sentinel = false) {
  if (!v.is_number_integer())
    fail(std::string(field) + " must be an integer");
  if (sentinel && v == -1)
    return SIZE_MAX;
  if ((v.is_number_unsigned()
           ? v.get<uint64_t>() >= bound
           : v.get<int64_t>() < 0 || uint64_t(v.get<int64_t>()) >= bound))
    fail(std::string(field) + " is outside its source table");
  return v.get<size_t>();
}
void object(const Json &j, const char *field) {
  if (!j.is_object())
    fail(std::string(field) + " must be an object");
}
void array(const Json &j, const char *field) {
  if (!j.is_array())
    fail(std::string(field) + " must be an array");
}
void numeric(const Json &j, const char *field, double low = -1e20,
             double high = 1e20) {
  if (!j.is_number() || !std::isfinite(j.get<double>()) ||
      j.get<double>() < low || j.get<double>() > high)
    fail(std::string(field) + " must be finite and in range");
}
void text(const Json &j, const char *field) {
  if (!j.is_string() || j.get_ref<const std::string &>().size() > 16384 ||
      j.get_ref<const std::string &>().find('\0') != std::string::npos)
    fail(std::string(field) + " must be bounded text");
}
void boolean(const Json &j, const char *field) {
  if (!j.is_boolean())
    fail(std::string(field) + " must be boolean");
}
void choice(const Json &j, const char *field,
            std::initializer_list<const char *> allowed) {
  text(j, field);
  for (auto value : allowed)
    if (j == value)
      return;
  fail(std::string(field) + " has an unknown enum");
}
void vector(const Json &j, const char *field) {
  object(j, field);
  for (const char *axis : {"x", "y", "z"})
    if (j.contains(axis))
      numeric(j.at(axis), field);
}
void numeric_fields(const Json &j, std::initializer_list<const char *> fields,
                    double low = -1e20, double high = 1e20) {
  for (auto f : fields)
    if (j.contains(f))
      numeric(j.at(f), f, low, high);
}
void scan(const Json &j, size_t depth = 0, size_t *visits = nullptr) {
  size_t local = 0;
  if (!visits)
    visits = &local;
  if (depth > 64 || ++*visits > 1'000'000)
    fail("metadata resource bound exceeded");
  if (j.is_number_float())
    numeric(j, "metadata number");
  if (j.is_string())
    text(j, "metadata string");
  if (j.is_structured())
    for (auto &v : j)
      scan(v, depth + 1, visits);
}
int mapped(const Json &v, const std::vector<int> &map, const char *field,
           bool sentinel = false) {
  size_t id = index(v, map.size(), field, sentinel);
  if (id == SIZE_MAX)
    return -1;
  if (map[id] < 0)
    fail(std::string(field) + " has no exact emitted identity");
  return map[id];
}
std::vector<int> mapped_mesh(const Json &v, const Vrm0IdentityMap &map) {
  size_t id = index(v, map.meshes.size(), "mesh");
  if (map.meshes[id].empty())
    fail("mesh has no emitted identity");
  return map.meshes[id];
}
void stop(const VrmExportOptions &o) {
  if (o.cancelled && o.cancelled())
    throw std::runtime_error("VRM export cancelled");
}
uint32_t word(const std::vector<uint8_t> &b, size_t at) {
  if (at > b.size() || b.size() - at < 4)
    fail("truncated original GLB");
  return uint32_t(b[at]) | uint32_t(b[at + 1]) << 8 |
         uint32_t(b[at + 2]) << 16 | uint32_t(b[at + 3]) << 24;
}
Json original_json(const CreatorAppearance &a) {
  if (!a.original_bytes || a.original_bytes->size() < 28 ||
      word(*a.original_bytes, 0) != 0x46546c67 ||
      word(*a.original_bytes, 4) != 2 ||
      word(*a.original_bytes, 8) != a.original_bytes->size() ||
      word(*a.original_bytes, 16) != 0x4e4f534a)
    fail("retained source is not a GLB");
  size_t length = word(*a.original_bytes, 12);
  if (length > 16 * 1024 * 1024 || length > a.original_bytes->size() - 20)
    fail("source JSON bound exceeded");
  auto &bytes = *a.original_bytes;
  auto result = Json::parse(bytes.begin() + 20, bytes.begin() + 20 + length);
  scan(result);
  return result;
}
} // namespace
Vrm0Metadata Vrm0Metadata::parse(const Json &j, size_t nodes, size_t meshes,
                                 size_t textures, size_t materials) {
  object(j, "extension");
  scan(j);
  if (j.value("specVersion", "") != "0.0")
    fail("source version is not 0.0; migration is not implicit");
  for (const char *field : {"meta", "humanoid", "firstPerson",
                            "blendShapeMaster", "secondaryAnimation"}) {
    if (!j.contains(field))
      fail(std::string("missing required avatar section ") + field);
    object(j.at(field), field);
  }
  if (j.contains("exporterVersion"))
    text(j.at("exporterVersion"), "exporterVersion");
  const auto &meta = j.at("meta");
  for (const char *field :
       {"title", "version", "author", "contactInformation", "reference",
        "otherPermissionUrl", "otherLicenseUrl"})
    if (meta.contains(field))
      text(meta.at(field), field);
  for (const char *field : {"title", "author", "licenseName"})
    if (!meta.contains(field))
      fail(std::string("missing metadata ") + field);
  if (meta.contains("texture"))
    index(meta.at("texture"), textures, "thumbnail", true);
  if (meta.contains("allowedUserName"))
    choice(meta.at("allowedUserName"), "allowedUserName",
           {"OnlyAuthor", "ExplicitlyLicensedPerson", "Everyone"});
  for (const char *field :
       {"violentUssageName", "sexualUssageName", "commercialUssageName"})
    if (meta.contains(field))
      choice(meta.at(field), field, {"Disallow", "Allow"});
  choice(meta.at("licenseName"), "licenseName",
         {"Redistribution_Prohibited", "CC0", "CC_BY", "CC_BY_NC", "CC_BY_SA",
          "CC_BY_NC_SA", "CC_BY_ND", "CC_BY_NC_ND", "Other"});
  if (meta.at("licenseName") == "Other" &&
      (!meta.contains("otherLicenseUrl") || meta.at("otherLicenseUrl") == ""))
    fail("Other license requires its URL");
  const auto &human = j.at("humanoid");
  array(human.at("humanBones"), "humanBones");
  std::set<std::string> roles;
  std::set<size_t> bones;
  auto valid_roles = human_roles();
  for (const auto &bone : human.at("humanBones")) {
    object(bone, "human bone");
    text(bone.at("bone"), "bone role");
    std::string role = bone.at("bone");
    // VRM0's intermediate thumb names correspond to VRM1 metacarpal semantics.
    std::string lookup = role;
    if (role == "leftThumbIntermediate")
      lookup = "leftThumbProximal";
    if (role == "rightThumbIntermediate")
      lookup = "rightThumbProximal";
    if (role == "leftThumbProximal")
      lookup = "leftThumbMetacarpal";
    if (role == "rightThumbProximal")
      lookup = "rightThumbMetacarpal";
    if (std::none_of(valid_roles.begin(), valid_roles.end(),
                     [&](auto &v) { return v.name == lookup; }))
      fail("unknown humanoid role");
    if (!roles.insert(role).second ||
        !bones.insert(index(bone.at("node"), nodes, "human bone node")).second)
      fail("duplicate humanoid role/node");
    if (bone.contains("useDefaultValues"))
      boolean(bone.at("useDefaultValues"), "useDefaultValues");
    for (const char *f : {"min", "max", "center"})
      if (bone.contains(f))
        vector(bone.at(f), f);
    numeric_fields(bone, {"axisLength"}, 0);
  }
  for (const char *required :
       {"hips", "spine", "head", "leftUpperArm", "leftLowerArm", "leftHand",
        "rightUpperArm", "rightLowerArm", "rightHand", "leftUpperLeg",
        "leftLowerLeg", "leftFoot", "rightUpperLeg", "rightLowerLeg",
        "rightFoot"})
    if (!roles.contains(required))
      fail(std::string("missing required humanoid role ") + required);
  numeric_fields(human,
                 {"armStretch", "legStretch", "upperArmTwist", "lowerArmTwist",
                  "upperLegTwist", "lowerLegTwist", "feetSpacing"});
  if (human.contains("hasTranslationDoF"))
    boolean(human.at("hasTranslationDoF"), "hasTranslationDoF");
  const auto &first = j.at("firstPerson");
  if (first.contains("firstPersonBone"))
    index(first.at("firstPersonBone"), nodes, "firstPersonBone", true);
  if (first.contains("firstPersonBoneOffset"))
    vector(first.at("firstPersonBoneOffset"), "firstPersonBoneOffset");
  if (first.contains("lookAtTypeName"))
    choice(first.at("lookAtTypeName"), "lookAtTypeName",
           {"Bone", "BlendShape"});
  for (const char *f : {"lookAtHorizontalInner", "lookAtHorizontalOuter",
                        "lookAtVerticalDown", "lookAtVerticalUp"})
    if (first.contains(f)) {
      auto &degree = first.at(f);
      object(degree, f);
      numeric_fields(degree, {"xRange", "yRange"});
      if (degree.contains("curve")) {
        array(degree.at("curve"), "look-at curve");
        for (auto &v : degree.at("curve"))
          numeric(v, "curve sample");
      }
    }
  if (first.contains("meshAnnotations")) {
    array(first.at("meshAnnotations"), "meshAnnotations");
    for (auto &a : first.at("meshAnnotations")) {
      object(a, "mesh annotation");
      index(a.at("mesh"), meshes, "annotation mesh");
      if (a.contains("firstPersonFlag"))
        choice(a.at("firstPersonFlag"), "firstPersonFlag",
               {"Auto", "Both", "ThirdPersonOnly", "FirstPersonOnly"});
    }
  }
  const auto &blend = j.at("blendShapeMaster");
  if (blend.contains("blendShapeGroups")) {
    array(blend.at("blendShapeGroups"), "blendShapeGroups");
    for (auto &group : blend.at("blendShapeGroups")) {
      object(group, "blendShapeGroup");
      if (group.contains("name"))
        text(group.at("name"), "expression name");
      if (group.contains("presetName"))
        choice(group.at("presetName"), "presetName",
               {"unknown", "neutral", "a", "i", "u", "e", "o", "blink", "joy",
                "angry", "sorrow", "fun", "lookup", "lookdown", "lookleft",
                "lookright", "blink_l", "blink_r"});
      if (group.contains("isBinary"))
        boolean(group.at("isBinary"), "isBinary");
      if (group.contains("binds")) {
        array(group.at("binds"), "expression binds");
        for (auto &bind : group.at("binds")) {
          object(bind, "expression bind");
          index(bind.at("mesh"), meshes, "expression mesh");
          index(bind.at("index"), 1'000'000, "expression target");
          numeric(bind.at("weight"), "expression weight", 0, 100);
        }
      }
      if (group.contains("materialValues")) {
        array(group.at("materialValues"), "materialValues");
        for (auto &bind : group.at("materialValues")) {
          object(bind, "material bind");
          text(bind.at("materialName"), "materialName");
          text(bind.at("propertyName"), "propertyName");
          array(bind.at("targetValue"), "targetValue");
          for (auto &v : bind.at("targetValue"))
            numeric(v, "material target");
        }
      }
    }
  }
  const auto &spring = j.at("secondaryAnimation");
  size_t colliders = 0;
  if (spring.contains("colliderGroups")) {
    array(spring.at("colliderGroups"), "colliderGroups");
    colliders = spring.at("colliderGroups").size();
    for (auto &group : spring.at("colliderGroups")) {
      object(group, "collider group");
      index(group.at("node"), nodes, "collider node");
      array(group.at("colliders"), "colliders");
      for (auto &c : group.at("colliders")) {
        object(c, "collider");
        if (c.contains("offset"))
          vector(c.at("offset"), "collider offset");
        if (c.contains("radius"))
          numeric(c.at("radius"), "collider radius", 0);
      }
    }
  }
  if (spring.contains("boneGroups")) {
    array(spring.at("boneGroups"), "boneGroups");
    for (auto &group : spring.at("boneGroups")) {
      object(group, "spring group");
      if (group.contains("comment"))
        text(group.at("comment"), "spring comment");
      numeric_fields(group, {"stiffiness", "gravityPower", "hitRadius"}, 0);
      numeric_fields(group, {"dragForce"}, 0, 1);
      if (group.contains("gravityDir"))
        vector(group.at("gravityDir"), "gravityDir");
      if (group.contains("center"))
        index(group.at("center"), nodes, "spring center", true);
      if (group.contains("bones")) {
        array(group.at("bones"), "spring bones");
        for (auto &v : group.at("bones"))
          index(v, nodes, "spring bone");
      }
      if (group.contains("colliderGroups")) {
        array(group.at("colliderGroups"), "spring collider groups");
        for (auto &v : group.at("colliderGroups"))
          index(v, colliders, "spring collider group");
      }
    }
  }
  array(j.at("materialProperties"), "materialProperties");
  if (j.at("materialProperties").size() != materials)
    fail("MToon property count differs from glTF materials");
  for (auto &m : j.at("materialProperties"))
    MToonMaterial::from_vrm0(m, textures);
  Vrm0Metadata result;
  result.source_ = j;
  return result;
}
Json Vrm0Metadata::remap(const Vrm0IdentityMap &map) const {
  auto result = source_;
  if (result["meta"].contains("texture"))
    result["meta"]["texture"] =
        mapped(result["meta"]["texture"], map.textures, "thumbnail", true);
  for (auto &bone : result["humanoid"]["humanBones"])
    bone["node"] = mapped(bone.at("node"), map.nodes, "human bone node");
  auto &first = result["firstPerson"];
  if (first.contains("firstPersonBone"))
    first["firstPersonBone"] =
        mapped(first.at("firstPersonBone"), map.nodes, "firstPersonBone", true);
  if (first.contains("meshAnnotations")) {
    Json expanded = Json::array();
    for (auto annotation : first["meshAnnotations"])
      for (auto mesh : mapped_mesh(annotation.at("mesh"), map)) {
        annotation["mesh"] = mesh;
        expanded.push_back(annotation);
      }
    first["meshAnnotations"] = expanded;
  }
  if (result["blendShapeMaster"].contains("blendShapeGroups"))
    for (auto &group : result["blendShapeMaster"]["blendShapeGroups"])
      if (group.contains("binds")) {
        Json expanded = Json::array();
        for (const auto &source_bind : group["binds"])
          for (auto mesh : mapped_mesh(source_bind.at("mesh"), map)) {
            auto bind = source_bind;
            bind["mesh"] = mesh;
            auto found = map.morph_targets.find(mesh);
            if (found == map.morph_targets.end())
              fail("expression mesh has no exact morph map");
            bind["index"] =
                mapped(source_bind.at("index"), found->second, "expression target");
            expanded.push_back(bind);
          }
        group["binds"] = expanded;
      }
  auto &spring = result["secondaryAnimation"];
  if (spring.contains("colliderGroups"))
    for (auto &group : spring["colliderGroups"])
      group["node"] = mapped(group.at("node"), map.nodes, "collider node");
  if (spring.contains("boneGroups"))
    for (auto &group : spring["boneGroups"]) {
      if (group.contains("center"))
        group["center"] =
            mapped(group.at("center"), map.nodes, "spring center", true);
      if (group.contains("bones"))
        for (auto &bone : group["bones"])
          bone = mapped(bone, map.nodes, "spring bone");
    }
  for (auto &material : result["materialProperties"])
    if (material.contains("textureProperties"))
      for (auto &value : material["textureProperties"].items())
        value.value() =
            mapped(value.value(), map.textures, "MToon texture", true);
  return result;
}
VrmExportReport export_vrm(const std::vector<ObjectSurface> &input_objects,
                           const std::filesystem::path &path,
                           const VrmExportOptions &options) {
  if (path.extension() != ".vrm")
    fail("output must use .vrm");
  if (!options.max_output_bytes || options.max_output_bytes > 256 * 1024 * 1024)
    fail("invalid output resource bound");
  if (options.paint_resolution && (options.paint_resolution<64 || options.paint_resolution>4096))
    fail("explicit painted texture resolution must be64..4096");
  VrmExportReport report;
  stop(options);
  if (input_objects.empty())
    fail("no avatar surfaces");

  for(const auto &object:input_objects)if(object.source_dormant)
    fail("source attributes are dormant for "+object.name+
      "; restore the original mesh or explicitly prepare, reproject and bake its source data before export");
  auto objects = input_objects;
  const CreatorAppearance *origin = nullptr;
  uint64_t group = 0;
  for (const auto &o : objects)
    if (o.appearance) {
      if (!origin) {
        origin = o.appearance.get();
        if (o.groups.empty())
          fail("source avatar lacks a group");
        group = o.groups.back().id;
      }
      if (o.appearance->asset != origin->asset ||
          o.appearance->original_bytes != origin->original_bytes)
        fail("multiple original avatars require separate exports");
    }
  if (!origin || !origin->asset ||
      origin->asset->mode != ImportMode::PreserveGltf)
    fail("avatar lacks preserved original source");
  const auto &asset = *origin->asset;
  auto original = original_json(*origin);
  if (!original.contains("extensions") ||
      !original.at("extensions").contains("VRM"))
    fail("only original VRM0 is supported by this source-preserving path");
  for (auto &extension : original.value("extensionsUsed", Json::array()))
    if (extension != "VRM" && extension != "KHR_materials_unlit" &&
        extension != "KHR_texture_transform")
      fail("source extension requires an explicit export remapper");
  if (original.contains("cameras") && !original.at("cameras").empty())
    fail("source cameras require an explicit source-node exporter");
  for (const auto &node : asset.nodes)
    if (node.camera >= 0 || node.light >= 0)
      fail(
          "source camera/light nodes require an explicit source-node exporter");
  for (const auto &mesh : original.at("meshes"))
    for (const auto &primitive : mesh.at("primitives"))
      for (const auto &attribute : primitive.at("attributes").items()) {
        const auto &name = attribute.key();
        if (name != "POSITION" && name != "NORMAL" && name != "TANGENT" &&
            name != "COLOR_0" && name != "TEXCOORD_0" && name != "TEXCOORD_1" &&
            !name.starts_with("JOINTS_") && !name.starts_with("WEIGHTS_"))
          fail("source vertex attribute requires an explicit preservation "
               "mapping");
      }
  if (original.contains("animations") && !original["animations"].empty())
    fail("animation export is outside this path; tracks would be lost");
  auto metadata = Vrm0Metadata::parse(original.at("extensions").at("VRM"),
                                      asset.nodes.size(), asset.meshes.size(),
                                      asset.textures.size(),
                                      original.at("materials").size());
  if (!origin->source_basis_quaternion)
    fail("source coordinate basis provenance is missing");
  Quat basis = origin->source_basis_quaternion->unit(),
       inverse = basis.inverse();
  auto convert_trs = [&](CanvasTransform &t) {
    auto p = inverse.rotate({t.position.x, t.position.y, t.position.z});
    t.position = {p.x, p.y, p.z};
    auto q = Quat{t.orientation.w, t.orientation.x, t.orientation.y,
                  t.orientation.z};
    q = (inverse * q * basis).unit();
    t.orientation = {q.x, q.y, q.z, q.w};
  };
  auto convert_joint = [&](JointTransform &t) {
    t.translation = inverse.rotate(t.translation);
    t.rotation = (inverse * t.rotation * basis).unit();
    // Nonuniform scale can be conjugated without shear only for signed-axis
    // bases.
    if (std::abs(t.scale.x - t.scale.y) > 1e-6f ||
        std::abs(t.scale.y - t.scale.z) > 1e-6f) {
      std::array<Vec3, 3> axes = {inverse.rotate({1, 0, 0}),
                                  inverse.rotate({0, 1, 0}),
                                  inverse.rotate({0, 0, 1})};
      Vec3 scale{};
      std::array<float, 3> old = {t.scale.x, t.scale.y, t.scale.z};
      for (size_t i = 0; i < 3; ++i) {
        auto axis = axes[i];
        if (std::max({std::abs(axis.x), std::abs(axis.y), std::abs(axis.z)}) <
            1 - 1e-6f)
          fail("nonuniform joint scale and source basis would require shear");
        scale.x += std::abs(axis.x) * old[i];
        scale.y += std::abs(axis.y) * old[i];
        scale.z += std::abs(axis.z) * old[i];
      }
      t.scale = scale;
    }
  };
  for (auto &o : objects) {
    stop(options);
    o.mesh.validate();
    if(o.material_bindings && o.material_bindings->normal_frames)detail::normal_frame_current_authority(*o.material_bindings,o.mesh,o.deformation.get(),options.cancelled);
    if (o.deformation)
      o.deformation->validate(o.mesh);
    if (!o.appearance || !o.appearance->source_basis_quaternion)
      fail("every source part needs coordinate basis provenance");
    auto q = *o.appearance->source_basis_quaternion;
    float norm = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;
    if (!std::isfinite(norm) || std::abs(norm - 1) > 1e-5f ||
        std::abs(std::abs(q.w * basis.w + q.x * basis.x + q.y * basis.y +
                          q.z * basis.z) -
                 1) > 1e-5f)
      fail("avatar source coordinate bases differ or are not unit quaternions");
    for (auto &p : o.mesh.positions)
      p = inverse.rotate(p);
    for (auto &n : o.mesh.normals)
      n = inverse.rotate(n);
    convert_trs(o.transform);
    for (auto &g : o.groups)
      convert_trs(g.transform);
    if (!o.deformation)
      fail("source part lacks deformation");
    auto d = std::make_shared<DeformationState>(*o.deformation);
    if (d->skeleton)
      for (auto &bone : d->skeleton->bones)
        convert_joint(bone.rest);
    for (auto &pose : d->pose)
      convert_joint(pose);
    d->source = morph_topology_id(o.mesh);
    if (d->morphs) {
      for (auto &target : d->morphs->targets)
        for (auto &delta : target.delta)
          delta = inverse.rotate(delta);
      d->morphs->topology = d->source;
    }
    if (d->landmarks) {
      for (auto &[name, p] : d->landmarks->points)
        p = inverse.rotate(p);
      d->landmarks->left = inverse.rotate(d->landmarks->left);
      d->landmarks->up = inverse.rotate(d->landmarks->up);
      d->landmarks->front = inverse.rotate(d->landmarks->front);
    }
    o.deformation = std::move(d);
  }
  // Prove a uniform fit before reusing source-local physics/camera data.
  float fit_scale = 1, farthest = 0;
  Vec3 fit_offset{}, p0{}, q0{};
  bool first_vertex = true, uniform_fit = true;
  auto source_primitive =
      [&](const CreatorAppearance &a) -> const ImportedPrimitive & {
    const ImportedPrimitive *found = nullptr;
    for (auto &p : asset.primitives)
      if (p.source_mesh == a.source_mesh &&
          p.source_primitive == a.source_primitive) {
        if (found)
          fail("ambiguous fit primitive");
        found = &p;
      }
    if (!found)
      fail("missing fit primitive");
    return *found;
  };
  for (auto &o : objects) {
    const auto &a = *o.appearance;
    const auto &p = source_primitive(a);
    if (a.source_vertices.size() != o.mesh.positions.size())
      fail("fit vertex mapping count differs");
    for (size_t v = 0; v < a.source_vertices.size(); ++v) {
      auto id = a.source_vertices[v];
      if (id >= p.mesh.positions.size())
        fail("fit vertex is missing");
      if (first_vertex) {
        p0 = p.mesh.positions[id];
        q0 = o.mesh.positions[v];
        first_vertex = false;
      }
      auto dp = p.mesh.positions[id] - p0;
      float distance = dot(dp, dp);
      if (distance > std::max(1e-8f, farthest)) {
        farthest = distance;
        fit_scale = dot(o.mesh.positions[v] - q0, dp) / distance;
      }
    }
  }
  fit_offset = q0 - p0 * fit_scale;
  if (first_vertex || !std::isfinite(fit_scale) || fit_scale <= 0)
    fail("invalid uniform source fit");
  for (auto &o : objects) {
    const auto &a = *o.appearance;
    const auto &p = source_primitive(a);
    for (size_t v = 0; v < a.source_vertices.size(); ++v)
      if (length(o.mesh.positions[v] -
                 (p.mesh.positions[a.source_vertices[v]] * fit_scale +
                  fit_offset)) > 2e-5f)
        uniform_fit = false;
  }
  if (!uniform_fit)
    fail("nonlinear source rest fitting needs explicit first-person/collider "
         "metadata refitting");
  auto document = detail::make_gltf(objects, detail::SourceMorphSpace::Original,
                                    true, options.cancelled);
  report.normal_targets_recomputed = document.normal_targets_recomputed;
  auto &j = document.json;
  auto &binary = document.binary;
  auto check_budget = [&] {
    stop(options);
    if (binary.size() > options.max_output_bytes)
      throw std::length_error("VRM output resource budget exceeded");
  };
  auto extension = [&](const char *name) {
    auto &used = j["extensionsUsed"];
    if (!used.is_array())
      used = Json::array();
    if (std::find(used.begin(), used.end(), name) == used.end())
      used.push_back(name);
  };
  Vrm0IdentityMap identities;
  identities.nodes.assign(asset.nodes.size(), -1);
  identities.meshes.resize(asset.meshes.size());
  identities.textures.assign(asset.textures.size(), -1);
  std::vector<int> original_images;
  // Keep source encoded image payloads, including thumbnail and extra MToon
  // maps.
  for (const auto &image : asset.images) {
    if (image.mime_type != "image/png" && image.mime_type != "image/jpeg")
      fail("source image encoding is not core glTF PNG/JPEG");
    if (image.bytes.size() >
        options.max_output_bytes -
            std::min(binary.size(), options.max_output_bytes))
      throw std::length_error("VRM image resource budget exceeded");
    int view = int(j["bufferViews"].size());
    original_images.push_back(int(j["images"].size()));
    j["bufferViews"].push_back({{"buffer", 0},
                                {"byteOffset", binary.size()},
                                {"byteLength", image.bytes.size()}});
    binary.insert(binary.end(), image.bytes.begin(), image.bytes.end());
    while (binary.size() % 4)
      binary.push_back(0);
    j["images"].push_back({{"bufferView", view},
                           {"mimeType", image.mime_type},
                           {"name", image.name}});
    check_budget();
  }
  for (size_t i = 0; i < asset.textures.size(); ++i) {
    const auto &t = asset.textures[i];
    if (t.image < 0 || size_t(t.image) >= original_images.size())
      fail("missing source image");
    int sampler = int(j["samplers"].size());
    Json s = {{"wrapS", t.wrap_s}, {"wrapT", t.wrap_t}};
    if (t.min_filter)
      s["minFilter"] = t.min_filter;
    if (t.mag_filter)
      s["magFilter"] = t.mag_filter;
    j["samplers"].push_back(s);
    identities.textures[i] = int(j["textures"].size());
    j["textures"].push_back(
        {{"source", original_images[t.image]}, {"sampler", sampler}});
  }
  std::set<std::pair<int, int>> primitives;
  std::vector<int> avatar_joints;
  Json output_properties = Json::array();
  Json output_materials = Json::array();
  struct MaterialVariant {
    int source;
    std::string source_name, name;
    Json signature;
    std::set<std::string> rewritten;
  };
  std::vector<MaterialVariant> variants;
  std::set<int> painted_variants;
  std::map<int,std::vector<int>> source_variants;
  std::map<std::string,std::vector<int>> source_material_names;
  std::set<std::string> reserved_names, emitted_names;
  for (size_t source=0;source<metadata.source().at("materialProperties").size();++source) {
    auto name=metadata.source().at("materialProperties")[source].value("name",std::string{});
    source_material_names[name].push_back(int(source)); reserved_names.insert(name);
  }
  struct RoleImage { std::weak_ptr<const Image> owner;int image; };
  std::map<const Image*,RoleImage,std::less<const Image*>> role_images;
  if (origin->decoded_images && origin->decoded_images->size()==original_images.size())
    for (size_t image=0;image<original_images.size();++image)
      role_images[origin->decoded_images->at(image).get()]={origin->decoded_images->at(image),original_images[image]};
  std::map<std::array<int,4>,int> role_samplers;
  for (size_t sampler=0;sampler<j["samplers"].size();++sampler) {
    const auto &s=j["samplers"][sampler];
    role_samplers.try_emplace(std::array{s.value("wrapS",10497),s.value("wrapT",10497),s.value("minFilter",0),s.value("magFilter",0)},int(sampler));
  }
  std::map<std::pair<int,int>,int> role_textures;
  for (size_t texture=0;texture<j["textures"].size();++texture) {
    const auto &t=j["textures"][texture];
    role_textures.try_emplace(std::pair{t.at("source").get<int>(),t.value("sampler",-1)},int(texture));
  }
  auto role_texture=[&](const PaintSourceTexture &role) {
    stop(options);role.validate();
    if (!role.image) fail("resolved MToon role has no image");
    if (size_t(role.image->width)*role.image->height>16*1024*1024)
      throw std::length_error("VRM role image pixel budget exceeded");
    int image;
    if (auto found=role_images.find(role.image.get());found!=role_images.end() && !found->second.owner.expired()) image=found->second.image;
    else {
      auto png=role.image->png();
      if (png.size()>options.max_output_bytes-std::min(binary.size(),options.max_output_bytes))
        throw std::length_error("VRM role image output budget exceeded");
      int view=int(j["bufferViews"].size());image=int(j["images"].size());
      j["bufferViews"].push_back({{"buffer",0},{"byteOffset",binary.size()},{"byteLength",png.size()}});
      binary.insert(binary.end(),png.begin(),png.end());while(binary.size()%4)binary.push_back(0);
      j["images"].push_back({{"bufferView",view},{"mimeType","image/png"}});
      role_images[role.image.get()]={role.image,image};
    }
    std::array key{role.wrap_s,role.wrap_t,role.min_filter,role.mag_filter};int sampler;
    if (auto found=role_samplers.find(key);found!=role_samplers.end()) sampler=found->second;
    else {
      sampler=int(j["samplers"].size());Json record={{"wrapS",role.wrap_s},{"wrapT",role.wrap_t}};
      if(role.min_filter)record["minFilter"]=role.min_filter;
      if(role.mag_filter)record["magFilter"]=role.mag_filter;
      j["samplers"].push_back(record);role_samplers[key]=sampler;
    }
    auto pair=std::pair{image,sampler};
    if(auto found=role_textures.find(pair);found!=role_textures.end()) return found->second;
    int texture=int(j["textures"].size());j["textures"].push_back({{"source",image},{"sampler",sampler}});
    role_textures[pair]=texture;check_budget();return texture;
  };
  // Images may have been encoded by separate generic/typed material paths.
  // Deduplicate material states by exact embedded image bytes and sampler
  // semantics, verifying equality after the hash rather than trusting it.
  struct ImageIdentity { size_t offset=0,size=0;std::string mime;uint64_t hash=0;int canonical=-1; };
  std::vector<ImageIdentity> image_identities;
  std::map<std::pair<std::string,uint64_t>,std::vector<int>> image_hashes;
  auto canonical_image=[&](int id) {
    while(image_identities.size()<=size_t(id)) {
      auto current=image_identities.size();const auto &image=j["images"].at(current);
      const auto &view=j["bufferViews"].at(image.at("bufferView").get<size_t>());
      ImageIdentity value;value.offset=view.value("byteOffset",size_t{});value.size=view.at("byteLength");value.mime=image.at("mimeType");
      if(value.offset>binary.size() || value.size>binary.size()-value.offset)fail("material image range is invalid");
      value.hash=1469598103934665603ull;
      for(size_t k=0;k<value.size;++k)value.hash=(value.hash^binary[value.offset+k])*1099511628211ull;
      auto &bucket=image_hashes[{value.mime,value.hash}];value.canonical=int(current);
      for(int previous:bucket) {
        const auto &other=image_identities.at(size_t(previous));
        if(other.size==value.size && std::equal(binary.begin()+other.offset,binary.begin()+other.offset+other.size,binary.begin()+value.offset)) {
          value.canonical=other.canonical;break;
        }
      }
      if(value.canonical==int(current))bucket.push_back(int(current));
      image_identities.push_back(std::move(value));
    }
    return image_identities.at(size_t(id)).canonical;
  };
  auto texture_identity=[&](const Json &value) {
    auto id=index(value,j["textures"].size(),"emitted material texture",true);
    if(id==SIZE_MAX)return Json(-1);
    const auto &texture=j["textures"][id];Json sampler={{"wrapS",10497},{"wrapT",10497}};
    if(texture.contains("sampler"))sampler=j["samplers"].at(texture.at("sampler").get<size_t>());
    if(!sampler.contains("wrapS"))sampler["wrapS"]=10497;
    if(!sampler.contains("wrapT"))sampler["wrapT"]=10497;
    return Json{{"image",canonical_image(texture.at("source"))},{"sampler",sampler}};
  };
  auto material_signature=[&](Json material,Json properties) {
    material.erase("name");properties.erase("name");
    auto info=[&](Json &value){value["index"]=texture_identity(value.at("index"));};
    for(const char *slot:{"normalTexture","occlusionTexture","emissiveTexture"})if(material.contains(slot))info(material[slot]);
    if(material.contains("pbrMetallicRoughness"))for(const char *slot:{"baseColorTexture","metallicRoughnessTexture"})
      if(material["pbrMetallicRoughness"].contains(slot))info(material["pbrMetallicRoughness"][slot]);
    if(properties.contains("textureProperties"))for(auto &texture:properties["textureProperties"].items())texture.value()=texture_identity(texture.value());
    return Json{{"material",std::move(material)},{"properties",std::move(properties)}};
  };
  auto add_material=[&](int source,Json material,Json properties,std::set<std::string> rewritten) {
    auto signature=material_signature(material,properties);
    for(int previous:source_variants[source])if(variants[size_t(previous)].signature==signature) {
      variants[size_t(previous)].rewritten.insert(rewritten.begin(),rewritten.end());return previous;
    }
    auto original_name=metadata.source().at("materialProperties").at(size_t(source)).value("name",std::string{});
    auto source_name=original_name.empty()?"Source material "+std::to_string(source):original_name;
    auto name=source_name;
    if(source_material_names.at(original_name).size()>1 || emitted_names.contains(name) ||
       (original_name.empty() && reserved_names.contains(name))) {
      auto suffix=" [Dome "+std::to_string(source)+":"+std::to_string(source_variants[source].size()+1)+"]";
      name=source_name+suffix;size_t collision=0;
      while(reserved_names.contains(name)||emitted_names.contains(name))name=source_name+suffix+"#"+std::to_string(++collision);
    }
    emitted_names.insert(name);material["name"]=name;properties["name"]=name;
    int id=int(variants.size());source_variants[source].push_back(id);
    variants.push_back({source,source_name,name,std::move(signature),std::move(rewritten)});
    output_materials.push_back(std::move(material));output_properties.push_back(std::move(properties));return id;
  };
  auto remap_info = [&](Json &info) {
    info["index"] =
        mapped(info.at("index"), identities.textures, "material texture");
    if (info.contains("extensions"))
      for (auto &e : info["extensions"].items())
        if (e.key() != "KHR_texture_transform")
          fail("material texture extension lacks a remapper");
  };
  for (size_t i = 0; i < objects.size(); ++i) {
    stop(options);
    const auto &o = objects[i];
    const auto &emitted = document.objects.at(i);
    if (o.groups.empty() || o.groups.back().id != group || !o.deformation ||
        !o.deformation->skeleton || !o.deformation->skin ||
        !o.deformation->humanoid)
      fail("every avatar part requires the same grouped humanoid rig");
    if (avatar_joints.empty())
      avatar_joints.assign(emitted.joints.begin(), emitted.joints.end());
    if (std::vector<int>(emitted.joints.begin(), emitted.joints.end()) !=
        avatar_joints)
      fail("avatar parts do not share one actual emitted rig/pose/transform");
    if (!o.appearance)
      fail("generated parts need an explicit VRM material/source policy; "
           "export them separately as glTF");
    const auto &a = *o.appearance;
    if (a.source_joint_nodes.size() != emitted.joints.size() ||
        a.source_joint_nodes.empty() || a.source_joint_nodes.size() > 256)
      fail("missing exact source joint map; reopen/recreate source instead of "
           "inferring names");
    std::set<uint32_t> unique;
    const auto &s = *o.deformation->skeleton;
    auto fitted_world = s.world();
    for (size_t joint = 0; joint < a.source_joint_nodes.size(); ++joint) {
      auto source = a.source_joint_nodes[joint];
      if (source >= asset.nodes.size() || !unique.insert(source).second)
        fail("invalid source joint identity");
      int parent = s.bones[joint].parent;
      int expected =
          parent < 0 ? -1 : int(a.source_joint_nodes.at(size_t(parent)));
      if (asset.nodes[source].parent != expected)
        fail("source joint ancestry differs from native mapping");
      const auto &source_world = asset.nodes[source].world;
      Vec3 expected_world =
          Vec3{source_world[12], source_world[13], source_world[14]} *
              fit_scale +
          fit_offset;
      auto &actual_world = fitted_world[joint];
      if (length(Vec3{actual_world[12], actual_world[13], actual_world[14]} -
                 expected_world) > 2e-5f)
        fail("edited source rest joint positions require explicit local "
             "metadata refitting");
      const auto &original_node = asset.nodes[source];
      const auto &rest = s.bones[joint].rest;
      Quat original_rotation{
          original_node.rotation[3], original_node.rotation[0],
          original_node.rotation[1], original_node.rotation[2]};
      float rotation_dot = rest.rotation.w * original_rotation.w +
                           rest.rotation.x * original_rotation.x +
                           rest.rotation.y * original_rotation.y +
                           rest.rotation.z * original_rotation.z;
      if (original_node.has_matrix ||
          std::abs(std::abs(rotation_dot) - 1) > 1e-5f ||
          length(rest.scale - Vec3{original_node.scale[0],
                                   original_node.scale[1],
                                   original_node.scale[2]}) > 1e-5f)
        fail("edited rest axes/scales require explicit bone-local metadata "
             "rebasing");
      int &mapped_node = identities.nodes[source];
      if (mapped_node >= 0 && mapped_node != int(emitted.joints[joint]))
        fail("source joint has multiple emitted identities");
      mapped_node = int(emitted.joints[joint]);
    }
    if (a.source_mesh < 0 || size_t(a.source_mesh) >= asset.meshes.size() ||
        a.source_primitive < 0 || a.source_node < 0 ||
        size_t(a.source_node) >= asset.nodes.size() ||
        asset.nodes[a.source_node].mesh != a.source_mesh)
      fail("invalid source mesh/node identity");
    if (!primitives.insert({a.source_mesh, a.source_primitive}).second)
      fail("duplicate source primitive creates ambiguous avatar identity");
    identities.meshes[a.source_mesh].push_back(emitted.mesh);
    const ImportedPrimitive *primitive = nullptr;
    for (auto &p : asset.primitives)
      if (p.source_mesh == a.source_mesh &&
          p.source_primitive == a.source_primitive) {
        if (primitive)
          fail("ambiguous source primitive");
        primitive = &p;
      }
    if (!primitive || a.source_vertices.size() != o.mesh.positions.size() ||
        a.source_material < 0 ||
        primitive->material != uint32_t(a.source_material + 1))
      fail("source accessor/material mapping changed");
    auto &target_map = identities.morph_targets[emitted.mesh];
    target_map.assign(primitive->morph_targets.size(), -1);
    if(o.material_bindings && o.material_bindings->normal_frames) {
      const auto &frames=*o.material_bindings->normal_frames;
      const size_t count=o.deformation->morphs?o.deformation->morphs->targets.size():0;
      if(frames.current_targets.size()!=count)fail("current native target/frame identity map is stale");
      for(size_t target=0;target<count;++target) {
        auto id=frames.current_targets[target];auto item=std::find_if(frames.targets.begin(),frames.targets.end(),[&](const auto &t){return t.id==id;});
        if(item==frames.targets.end())fail("native target frame identity is missing");
        if(!item->source_target)continue; // Authored native-only target remains emitted.
        auto source_id=*item->source_target;
        if(source_id>=primitive->morph_targets.size() || target_map[source_id]>=0)fail("source target frame identity is ambiguous or outside the original table");
        target_map[source_id]=int(target);
      }
    } else {
    if (primitive->morph_targets.size() !=
        (o.deformation->morphs ? o.deformation->morphs->targets.size() : 0))
      fail("source morph count changed; expression remapping requires explicit "
           "identities");
    if (a.source_morph_targets.size() != primitive->morph_targets.size())
      fail("missing exact source morph identity map");
    for (size_t target = 0; target < a.source_morph_targets.size(); ++target) {
      auto source_id = a.source_morph_targets[target];
      if (source_id >= primitive->morph_targets.size() ||
          target_map[source_id] >= 0)
        fail("source morph identity map is not unique and in range");
      target_map[source_id] = int(target);
    }
    }
    auto source_material =
        original.at("materials").at(size_t(a.source_material));
    if (source_material.contains("extensions"))
      for (auto &e : source_material["extensions"].items())
        if (e.key() != "KHR_materials_unlit")
          fail("source material extension lacks a version-preserving export "
               "path");
    for (const char *slot :
         {"normalTexture", "occlusionTexture", "emissiveTexture"})
      if (source_material.contains(slot))
        remap_info(source_material[slot]);
    if (source_material.contains("pbrMetallicRoughness"))
      for (const char *slot : {"baseColorTexture", "metallicRoughnessTexture"})
        if (source_material["pbrMetallicRoughness"].contains(slot))
          remap_info(source_material["pbrMetallicRoughness"][slot]);
    // The core writer already distinguishes raw source factors from composites.
    auto generated = j["materials"][emitted.material];
    auto &out_pbr = source_material["pbrMetallicRoughness"];
    if (!out_pbr.is_object())
      out_pbr = Json::object();
    for (const char *f :
         {"baseColorFactor", "metallicFactor", "roughnessFactor",
          "baseColorTexture", "metallicRoughnessTexture"})
      if (generated.at("pbrMetallicRoughness").contains(f))
        out_pbr[f] = generated.at("pbrMetallicRoughness").at(f);
    for (const char *f : {"normalTexture", "occlusionTexture", "emissiveTexture", "emissiveFactor", "alphaMode",
                          "alphaCutoff", "doubleSided"})
      if (generated.contains(f))
        source_material[f] = generated[f];
    auto toon = MToonMaterial::from_vrm0(metadata.source()
                                             .at("materialProperties")
                                             .at(size_t(a.source_material)),
                                         asset.textures.size());
    const auto &fallback=asset.materials.at(size_t(a.material));
    SourceNativeMaterial reference;reference.color=fallback.color;reference.emission=fallback.emissive;
    reference.alpha=fallback.alpha==ImportedMaterial::AlphaMode::Opaque?int(Material::AlphaMode::Opaque):
      fallback.alpha==ImportedMaterial::AlphaMode::Mask?int(Material::AlphaMode::Mask):int(Material::AlphaMode::Blend);
    reference.alpha_cutoff=fallback.alpha_cutoff;reference.double_sided=fallback.double_sided;
    std::array<PaintSourceTexture,size_t(MToonSlot::Count)> roles;
    if(o.material_bindings) {
      o.material_bindings->validate();
      if(!o.material_bindings->has_mtoon || o.material_bindings->material.version()!=MToonVersion::V0)
        fail("current source material is not a version-preserving MToon0 binding");
      toon=o.material_bindings->material;roles=o.material_bindings->roles;reference=o.material_bindings->native_reference;
      if(o.material_bindings->sampling_reparameterized && !o.material_bindings->normal_frames && toon.parameters.normal_enabled && roles[size_t(MToonSlot::Normal)].image)
        fail("derived UV normal roles require retained original frame/tangent transport before VRM export");
    } else {
      if(!a.decoded_images || a.decoded_images->size()!=asset.images.size())fail("source role images are unavailable");
      for(size_t role=0;role<roles.size();++role)if(toon.textures[role].index>=0) {
        const auto &texture=asset.textures.at(size_t(toon.textures[role].index));
        if(texture.image<0 || size_t(texture.image)>=a.decoded_images->size())fail("source MToon role image is missing");
        auto &view=roles[role];view.image=a.decoded_images->at(size_t(texture.image));
        if(!view.image)fail("source MToon role has no decoded image");
        view.wrap_s=texture.wrap_s;view.wrap_t=texture.wrap_t;view.min_filter=texture.min_filter;view.mag_filter=texture.mag_filter;
      }
    }
    reference.validate();
    if(o.material.alpha!=Material::AlphaMode::Automatic && int(o.material.alpha)!=reference.alpha) {
      bool blend=o.material.alpha==Material::AlphaMode::Blend;
      toon.parameters.alpha=blend?MToonAlpha::Blend:o.material.alpha==Material::AlphaMode::Mask?MToonAlpha::Mask:MToonAlpha::Opaque;
      toon.parameters.depth_write=!blend;toon.parameters.source_blend=blend?5:1;toon.parameters.destination_blend=blend?10:0;
      toon.parameters.render_queue=blend?3000:o.material.alpha==Material::AlphaMode::Mask?2450:2000;
    }
    if(o.material.double_sided!=reference.double_sided)toon.parameters.cull=o.material.double_sided?MToonCull::Off:MToonCull::Back;
    if(o.material.alpha_cutoff!=reference.alpha_cutoff)toon.parameters.alpha_cutoff=o.material.alpha_cutoff;
    std::set<std::string> rewritten;
    if (!o.textures || o.textures->source_only) {
      for (size_t c = 0; c < 4; ++c)
        toon.parameters.base_color[c]=detail::vrm_native_factor(toon.parameters.base_color[c],reference.color[c],o.material.color[c]);
      toon.parameters.emission = {
          detail::vrm_native_factor(toon.parameters.emission.x,reference.emission[0],o.material.emissive[0]),
          detail::vrm_native_factor(toon.parameters.emission.y,reference.emission[1],o.material.emissive[1]),
          detail::vrm_native_factor(toon.parameters.emission.z,reference.emission[2],o.material.emissive[2])};
    } else {
      if(!o.textures->normal.rgba.empty() && !emitted.canonical_normal.image)fail("typed baked normal export needs explicit canonical tangent conversion");
      auto compiled=detail::vrm_painted_material(o,std::move(toon),std::move(roles),reference,options.paint_resolution,options.cancelled);
      toon=std::move(compiled.material);roles=std::move(compiled.roles);rewritten=std::move(compiled.rewritten_properties);
    }
    if(emitted.canonical_normal.image) {
      roles[size_t(MToonSlot::Normal)]=emitted.canonical_normal;
      toon.parameters.normal_enabled=true;toon.parameters.normal_scale=1;
      for(const char *name:{"_BumpScale","_MainTex_ST","_MainTex_ST_S","_MainTex_ST_T","_UvAnimScrollX","_UvAnimScrollY","_UvAnimRotation"})rewritten.insert(name);
    }
    if(o.material_bindings && o.material_bindings->sampling_reparameterized) {
      // Source UV baking changes the coordinate domain of these properties.
      // A later material-value transform cannot silently use the old domain.
      for(const char *name:{"_MainTex_ST","_MainTex_ST_S","_MainTex_ST_T","_UvAnimScrollX","_UvAnimScrollY","_UvAnimRotation"})rewritten.insert(name);
    }
    static constexpr std::array<const char*,size_t(MToonSlot::Count)> names={"_MainTex","_ShadeTexture","_BumpMap","_EmissionMap","_RimTexture","_SphereAdd","","_OutlineWidthTexture","_UvAnimMaskTexture","_ReceiveShadowTexture","_ShadingGradeTexture"};
    std::set<std::string> known_names;
    for(size_t role=0;role<roles.size();++role)if(*names[role]) {
      known_names.insert(names[role]);toon.textures[role].index=roles[role].image?role_texture(roles[role]):-1;
    }
    auto properties=toon.source_json();
    if(properties.contains("textureProperties"))for(auto &value:properties["textureProperties"].items())
      if(!known_names.contains(value.key()))value.value()=mapped(value.value(),identities.textures,"unknown retained MToon texture",true);
    source_material["alphaMode"]=toon.parameters.alpha==MToonAlpha::Mask?"MASK":toon.parameters.alpha==MToonAlpha::Blend?"BLEND":"OPAQUE";
    if(toon.parameters.alpha==MToonAlpha::Mask)source_material["alphaCutoff"]=toon.parameters.alpha_cutoff;
    else source_material.erase("alphaCutoff");
    source_material["doubleSided"]=toon.parameters.cull==MToonCull::Off;
    auto material=add_material(a.source_material,std::move(source_material),std::move(properties),std::move(rewritten));
    if(o.textures && !o.textures->source_only)painted_variants.insert(material);
    j["meshes"][emitted.mesh]["primitives"][0]["material"]=material;
    check_budget();
  }
  if (primitives.size() != asset.primitives.size())
    fail("source avatar is incomplete; missing source primitives cannot be "
         "silently dropped");
  // Material-value expressions may legally refer to an unused source material.
  // Keep that exact source identity instead of dropping its definition merely
  // because no primitive currently consumes it.
  for(size_t source=0;source<metadata.source().at("materialProperties").size();++source)if(source_variants[int(source)].empty()) {
    auto material=original.at("materials").at(source);
    if(material.contains("extensions"))for(const auto &e:material["extensions"].items())
      if(e.key()!="KHR_materials_unlit")fail("unused source material extension lacks an explicit remapper");
    for(const char *slot:{"normalTexture","occlusionTexture","emissiveTexture"})if(material.contains(slot))remap_info(material[slot]);
    if(material.contains("pbrMetallicRoughness"))for(const char *slot:{"baseColorTexture","metallicRoughnessTexture"})
      if(material["pbrMetallicRoughness"].contains(slot))remap_info(material["pbrMetallicRoughness"][slot]);
    auto properties=metadata.source().at("materialProperties").at(source);
    if(properties.contains("textureProperties"))for(auto &value:properties["textureProperties"].items())
      value.value()=mapped(value.value(),identities.textures,"unused MToon texture",true);
    add_material(int(source),std::move(material),std::move(properties),{});
  }
  j["materials"]=std::move(output_materials);
  auto vrm = metadata.remap(identities);
  auto scale_vector = [&](Json &v) {
    for (const char *axis : {"x", "y", "z"})
      if (v.contains(axis))
        v[axis] = v.at(axis).get<double>() * fit_scale;
  };
  if (vrm["firstPerson"].contains("firstPersonBoneOffset"))
    scale_vector(vrm["firstPerson"]["firstPersonBoneOffset"]);
  if (vrm["secondaryAnimation"].contains("colliderGroups"))
    for (auto &g : vrm["secondaryAnimation"]["colliderGroups"])
      for (auto &c : g["colliders"]) {
        if (c.contains("offset"))
          scale_vector(c["offset"]);
        if (c.contains("radius"))
          c["radius"] = c.at("radius").get<double>() * fit_scale;
      }
  if (vrm["secondaryAnimation"].contains("boneGroups"))
    for (auto &g : vrm["secondaryAnimation"]["boneGroups"])
      if (g.contains("hitRadius"))
        g["hitRadius"] = g.at("hitRadius").get<double>() * fit_scale;

  vrm["materialProperties"] = output_properties;
  // VRM0 expressions address material names, while native authoring addresses
  // original material IDs. Expand an original binding to every distinct state
  // fork of that ID, never to a same-named but unrelated source material.
  if(vrm["blendShapeMaster"].contains("blendShapeGroups")) for (auto &expression :
       vrm["blendShapeMaster"].at("blendShapeGroups")) {
    for (auto &bind : expression.value("binds", Json::array())) {
      size_t mesh =
          index(bind.at("mesh"), j["meshes"].size(), "emitted expression mesh");
      size_t targets = j["meshes"][mesh]["primitives"][0]
                           .value("targets", Json::array())
                           .size();
      index(bind.at("index"), targets, "emitted expression morph");
    }
    if(expression.contains("materialValues")) {
      Json expanded=Json::array();
      for(const auto &bind:expression["materialValues"]) {
        auto found=source_material_names.find(bind.at("materialName").get<std::string>());
        if(found==source_material_names.end() || found->second.size()!=1)
          fail("expression material name lacks one exact original material identity");
        for(int variant:source_variants.at(found->second.front())) {
          const auto &state=variants.at(size_t(variant));
          auto property=bind.at("propertyName").get<std::string>();
          if(state.rewritten.contains(property))
            fail("paint/source-UV compilation changes expression property "+property+"; an exact expression mapping is required");
          auto copy=bind;copy["materialName"]=state.name;expanded.push_back(std::move(copy));
        }
      }
      expression["materialValues"]=std::move(expanded);
    }
  }
  vrm["exporterVersion"] = "Dome Sculpt source-preserving VRM0";
  Vrm0Metadata::parse(vrm, j["nodes"].size(), j["meshes"].size(),
                      j["textures"].size(), j["materials"].size());
  j["extensions"]["VRM"] = vrm;
  extension("VRM");
  j["buffers"] = {{{"byteLength", binary.size()}}};
  uint64_t output_size = 28 + ((uint64_t(j.dump().size()) + 3) & ~uint64_t(3)) +
                         ((uint64_t(binary.size()) + 3) & ~uint64_t(3));
  if (output_size > options.max_output_bytes)
    throw std::length_error("VRM metadata output budget exceeded");
  stop(options);
  report.diagnostics.push_back(
      "Source basis restored explicitly; spring/collider offsets and radii "
      "retain authored values after translation/proportion fitting and need "
      "physics verification");
  report.meshes = objects.size();
  report.joints = avatar_joints.size();
  report.materials=variants.size();report.painted_materials=painted_variants.size();
  if(report.painted_materials) {
    report.diagnostics.push_back("Painted VRM0 main/shade/emission compiled with explicit coverage; original source material identities and native layers retained");
    if(options.paint_resolution)report.diagnostics.push_back("Painted texture resolution explicitly resampled to "+std::to_string(options.paint_resolution));
  }
  if (report.normal_targets_recomputed)
    report.diagnostics.push_back(
        "Changed morph normals recomputed from edited geometry; exact original "
        "NORMAL deltas remain in the native source archive");
  detail::write_glb(std::move(document), path);
  return report;
}
} // namespace dome
