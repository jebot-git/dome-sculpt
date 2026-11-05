// SPDX-License-Identifier: GPL-3.0-or-later
#include "emission-export.hpp"
#include "vrm1-export.hpp"
#include "dome/material-bindings.hpp"
#include "normal-frame-export.hpp"
#include "vrm-painted.hpp"
#include "vrm-source-graph.hpp"
#include "vrm1-metadata.hpp"
#include "vrm1-authoring.hpp"
#include "native-authoring-attach.hpp"
#include <set>
#include <bit>
namespace dome::detail {
namespace {
using J = nlohmann::json;
void require(bool value, const char *message) {
  if (!value)
    throw std::invalid_argument(std::string("VRM1 export: ") + message);
}
void stop(const VrmExportOptions &o) {
  if (o.cancelled && o.cancelled())
    throw std::runtime_error("VRM1 export cancelled");
}
bool painted(const Image &image) {
  for (size_t p = 0; p < image.rgba.size(); p += 4)
    if (image.rgba[p])
      return true;
  return false;
}
} // namespace
static VrmExportReport export_vrm1_impl(const std::vector<ObjectSurface> &objects,
                            const J &original,
                            const std::filesystem::path &path,
                            const VrmExportOptions &options,EmissionExportPrepared *physical,
                            const Vrm1AuthoringCompilation *authoring) {
  stop(options);
  validate_vrm1_metadata(original, options.cancelled, true);
  require(!objects.empty(), "no original source surfaces");
  require(objects.front().appearance && objects.front().appearance->asset,
          "missing immutable original source");
  const auto &origin = *objects.front().appearance;
  const auto &asset = *origin.asset;
  require(asset.mode == ImportMode::PreserveGltf,
          "original source is not a preserving capture");
  if (original.contains("animations") && !original.at("animations").empty())
    throw std::invalid_argument(
        "VRM1 export: animation tracks require a separate adapter");
  for (const auto &extension : original.value("extensionsUsed", J::array())) {
    const auto &name = extension.get_ref<const std::string &>();
    require(name == "VRMC_vrm" || name == "VRMC_materials_mtoon" ||
                name == "VRMC_springBone" || name == "VRMC_node_constraint" ||
                name == "KHR_materials_unlit" ||
                name == "KHR_texture_transform" ||
                name == "KHR_materials_emissive_strength",
            "source extension has no typed preserving remapper");
  }
  for (const auto &node : asset.nodes)
    require(node.camera < 0 && node.light < 0,
            "cameras/lights require explicit source-node export");
  require(!original.contains("scenes") || original.at("scenes").size() <= 1,
          "multiple authored scenes need an explicit scene-identity adapter");
  for (const auto &extension : original.at("extensions").items())
    require(extension.key() == "VRMC_vrm" ||
                extension.key() == "VRMC_springBone",
            "root extension has no typed reference remapper");
  for (const auto &node : original.at("nodes"))
    if (node.contains("extensions"))
      for (const auto &extension : node.at("extensions").items())
        require(extension.key() == "VRMC_node_constraint",
                "node extension has no typed reference remapper");
  struct ExpressionLayerIdentity {int source,lower,upper;bool forward_uv,shade_alias;std::array<float,3> shade_divisor;};
  std::vector<ExpressionLayerIdentity> expression_layers;
  std::set<int> uv_expression_materials, color_expression_materials;
  const auto &vrm = original.at("extensions").at("VRMC_vrm");
  if (vrm.contains("expressions"))
    for (const char *table : {"preset", "custom"})
      if (vrm.at("expressions").contains(table))
        for (const auto &expression : vrm.at("expressions").at(table)) {
          if (expression.contains("textureTransformBinds"))
            for (const auto &bind : expression.at("textureTransformBinds"))
              uv_expression_materials.insert(bind.at("material").get<int>());
          if (expression.contains("materialColorBinds"))
            for (const auto &bind : expression.at("materialColorBinds"))
              color_expression_materials.insert(bind.at("material").get<int>());
        }
  for (const auto &object : objects) {
    stop(options);
    require(object.appearance && object.appearance->asset == origin.asset &&
                object.appearance->original_bytes == origin.original_bytes,
            "parts do not share one immutable original model");
    require(!object.source_dormant, "original source is dormant");
    const auto &appearance = *object.appearance;
    const bool hdr_object=physical&&std::any_of(physical->images.begin(),physical->images.end(),[&](const auto &image){return image.object==object.id;});
    if(hdr_object){require(!uv_expression_materials.contains(appearance.source_material),"HDR emission on a UV-expression material requires an exact animated sampling adapter");if(vrm.contains("expressions"))for(const auto *table:{"preset","custom"})if(vrm.at("expressions").contains(table))for(const auto &expression:vrm.at("expressions").at(table))for(const auto &bind:expression.value("materialColorBinds",J::array()))if(bind.at("material").get<int>()==appearance.source_material&&bind.at("type").get_ref<const std::string&>()=="emissionColor")throw std::invalid_argument("VRM1 HDR emission expressions require an exact material-factor adapter");}
    if (uv_expression_materials.contains(appearance.source_material)) {
      require(
          !object.paint_sampling_attachment &&
              (!object.textures || object.textures->source_only),
          "UV expressions on compiled paint require a source-domain adapter");
      if (object.material_bindings)
        require(!object.material_bindings->sampling_reparameterized &&
                    !object.material_bindings->animation_frozen,
                "UV expression targets require explicit transport through "
                "prepared/frozen sampling");
    }
    if (color_expression_materials.contains(appearance.source_material) &&
        (!object.material_bindings || !object.material_bindings->has_mtoon))
      require(!object.textures || object.textures->source_only,
              "PBR colour expressions on compiled factors require an explicit "
              "target adapter");
    const ImportedPrimitive *primitive = nullptr;
    require(appearance.source_basis_quaternion &&
                appearance.source_basis_quaternion->x == 0 &&
                appearance.source_basis_quaternion->y == 0 &&
                appearance.source_basis_quaternion->z == 0 &&
                std::abs(appearance.source_basis_quaternion->w) == 1,
            "VRM1 source axes require exact native glTF basis");
    if (object.material_bindings && object.material_bindings->has_mtoon) {
      const auto frames = object.material_bindings->normal_frames;
      require(frames && frames->pbr && frames->mtoon &&
                  normal_frame_attribute_equal(*frames->pbr, *frames->mtoon,
                                               true, options.cancelled),
              "MToon1 and core PBR require one independently compatible glTF "
              "normal/tangent frame");
    }
    for (const auto &candidate : asset.primitives)
      if (candidate.source_mesh == appearance.source_mesh &&
          candidate.source_primitive == appearance.source_primitive) {
        require(!primitive, "ambiguous original primitive");
        primitive = &candidate;
      }
    require(primitive && appearance.source_vertices.size() ==
                             object.mesh.positions.size(),
            "current geometry has no original vertex witness");
    for (size_t v = 0; v < appearance.source_vertices.size(); ++v) {
      if ((v & 4095) == 0)
        stop(options);
      auto id = appearance.source_vertices[v];
      require(id < primitive->mesh.positions.size(),
              "original vertex witness is out of range");
      const auto &a = object.mesh.positions[v],
                 &b = primitive->mesh.positions[id];
      require(a.x == b.x && a.y == b.y && a.z == b.z,
              "base shape refitting requires explicit VRM1 metadata transport");
    }
    if (object.textures && !object.textures->source_only &&
        !(authoring && authoring->compiled(object))) {
      require(!painted(object.textures->color_coverage) &&
                  !painted(object.textures->emission_coverage),
              "painted MToon1 role composition requires the version-specific "
              "compiler");
    }
  }
  auto graph = vrm_source_graph(objects, original, options.cancelled,
                                options.max_output_bytes);
  bool constraints = false;
  for (const auto &node : original.at("nodes"))
    constraints |= node.contains("extensions") &&
                   node.at("extensions").contains("VRMC_node_constraint");
  if (constraints || original.at("extensions").contains("VRMC_springBone")) {
    AffineTransform placement;
    for (const auto &group : graph.prefix)
      placement = placement * group_local_matrix(group);
    const auto &m = placement.matrix();
    for (size_t a = 0; a < 3; ++a)
      for (size_t b = 0; b < 3; ++b) {
        double dot = 0;
        for (size_t k = 0; k < 3; ++k)
          dot += m[a * 4 + k] * m[b * 4 + k];
        require(std::abs(dot - double(a == b)) < 1e-6,
                "scaled/sheared native physics placement needs a typed "
                "radius/frame adapter");
      }
    require(
        placement.orientation() == AffineOrientation::Preserving,
        "reflected native physics placement needs an exact handedness adapter");
  }
  if (graph.fitted_rest &&
      (constraints || original.at("extensions").contains("VRMC_springBone")))
    throw std::invalid_argument("VRM1 export: spring/constraint REST changes "
                                "require an exact typed runtime adapter");
  if (constraints)
    for (size_t n = 0; n < graph.current_local.size(); ++n)
      require(graph.current_local[n] == graph.rest_local[n],
              "posed constraint sources require native constraint evaluation "
              "before source-retaining export");
  // VRM1 uses native glTF axes. Original is the older VRM0 basis conversion,
  // not a version-neutral switch for all VRM formats.
  auto document =
      make_gltf(objects, SourceMorphSpace::Native, true, options.cancelled);
  auto &j = document.json;
  auto &binary = document.binary;
  Vrm1IdentityMap identities;
  identities.nodes =
      emit_vrm_source_graph(document, objects, graph, options.cancelled);
  identities.surfaces.resize(asset.nodes.size());
  identities.materials.resize(original.value("materials", J::array()).size());
  auto budget = [&] {
    stop(options);
    require(binary.size() <= options.max_output_bytes,
            "binary output resource bound exceeded");
  };
  auto append_image = [&](std::span<const uint8_t> bytes,
                          const std::string &mime, const std::string &name) {
    require(mime == "image/png" || mime == "image/jpeg",
            "non-core source image encoding");
    require(bytes.size() <=
                options.max_output_bytes -
                    std::min(options.max_output_bytes, binary.size()),
            "encoded image output bound exceeded");
    int view = int(j["bufferViews"].size()), image = int(j["images"].size());
    j["bufferViews"].push_back({{"buffer", 0},
                                {"byteOffset", binary.size()},
                                {"byteLength", bytes.size()}});
    binary.insert(binary.end(), bytes.begin(), bytes.end());
    while (binary.size() % 4)
      binary.push_back(0);
    j["images"].push_back(
        {{"bufferView", view}, {"mimeType", mime}, {"name", name}});
    budget();
    return image;
  };
  for (const auto &image : asset.images)
    identities.images.push_back(
        append_image(image.bytes, image.mime_type, image.name));
  std::vector<int> original_textures;
  for (const auto &texture : asset.textures) {
    require(texture.image >= 0 &&
                size_t(texture.image) < identities.images.size(),
            "source texture has no image");
    auto sampler = int(j["samplers"].size());
    J value = {{"wrapS", texture.wrap_s}, {"wrapT", texture.wrap_t}};
    if (texture.min_filter)
      value["minFilter"] = texture.min_filter;
    if (texture.mag_filter)
      value["magFilter"] = texture.mag_filter;
    j["samplers"].push_back(std::move(value));
    original_textures.push_back(int(j["textures"].size()));
    j["textures"].push_back(
        {{"source", identities.images[size_t(texture.image)]},
         {"sampler", sampler}});
  }
  std::map<const Image *, int> images;
  if (origin.decoded_images) {
    require(origin.decoded_images->size() == identities.images.size(),
            "source decoded image identity differs");
    for (size_t i = 0; i < identities.images.size(); ++i)
      images.try_emplace(origin.decoded_images->at(i).get(),
                         identities.images[i]);
  }
  std::map<std::array<int, 4>, int> samplers;
  std::map<std::pair<int, int>, int> textures;
  auto role_texture = [&](const PaintSourceTexture &view) {
    stop(options);
    view.validate(8);
    require(bool(view.image), "typed role has no image");
    require(view.uv_transform == std::array<float, 6>{1, 0, 0, 1, 0, 0} &&
                view.texcoord == 0,
            "typed role sampling must remain in its versioned descriptor");
    int image;
    if (auto at = images.find(view.image.get()); at != images.end())
      image = at->second;
    else {
      auto png = view.image->png();
      image = append_image(png, "image/png", "Native MToon1 role");
      images.emplace(view.image.get(), image);
    }
    std::array key{view.wrap_s, view.wrap_t, view.min_filter, view.mag_filter};
    int sampler;
    if (auto at = samplers.find(key); at != samplers.end())
      sampler = at->second;
    else {
      sampler = int(j["samplers"].size());
      J value = {{"wrapS", view.wrap_s}, {"wrapT", view.wrap_t}};
      if (view.min_filter)
        value["minFilter"] = view.min_filter;
      if (view.mag_filter)
        value["magFilter"] = view.mag_filter;
      j["samplers"].push_back(std::move(value));
      samplers.emplace(key, sampler);
    }
    auto pair = std::pair{image, sampler};
    if (auto at = textures.find(pair); at != textures.end())
      return at->second;
    auto texture = int(j["textures"].size());
    j["textures"].push_back({{"source", image}, {"sampler", sampler}});
    textures.emplace(pair, texture);
    return texture;
  };
  auto remap_info = [&](J &info) {
    auto index = info.at("index").get<int64_t>();
    require(index >= 0 && uint64_t(index) < original_textures.size(),
            "source texture identity is out of range");
    info["index"] = original_textures[size_t(index)];
  };
  auto remap_material = [&](J value) {
    for (const char *slot :
         {"normalTexture", "occlusionTexture", "emissiveTexture"})
      if (value.contains(slot))
        remap_info(value[slot]);
    if (value.contains("pbrMetallicRoughness"))
      for (const char *slot : {"baseColorTexture", "metallicRoughnessTexture"})
        if (value["pbrMetallicRoughness"].contains(slot))
          remap_info(value["pbrMetallicRoughness"][slot]);
    if (value.contains("extensions")) {
      for (const auto &extension : value["extensions"].items())
        require(extension.key() == "KHR_materials_unlit" ||
                    extension.key() == "KHR_materials_emissive_strength" ||
                    extension.key() == "VRMC_materials_mtoon",
                "material extension lacks an exact remapper");
      if (value["extensions"].contains("VRMC_materials_mtoon")) {
        auto &toon = value["extensions"]["VRMC_materials_mtoon"];
        for (const char *slot :
             {"shadeMultiplyTexture", "shadingShiftTexture", "matcapTexture",
              "rimMultiplyTexture", "outlineWidthMultiplyTexture",
              "uvAnimationMaskTexture"})
          if (toon.contains(slot))
            remap_info(toon[slot]);
      }
    }
    return value;
  };
  VrmExportReport report;
  report.normal_targets_recomputed = document.normal_targets_recomputed;
  std::set<std::tuple<int, int, int>> instances;
  std::set<uint32_t> joints;
  static constexpr std::array<const char *, size_t(MToonSlot::Count)> slots = {
      "baseColorTexture",
      "shadeMultiplyTexture",
      "normalTexture",
      "emissiveTexture",
      "rimMultiplyTexture",
      "matcapTexture",
      "shadingShiftTexture",
      "outlineWidthMultiplyTexture",
      "uvAnimationMaskTexture",
      "",
      ""};
  for (size_t i = 0; i < objects.size(); ++i) {
    stop(options);
    const auto &object = objects[i];
    const auto &a = *object.appearance;
    const auto &emitted = document.objects[i];
    require(a.source_node >= 0 && size_t(a.source_node) < asset.nodes.size() &&
                asset.nodes[size_t(a.source_node)].mesh == a.source_mesh,
            "source node/mesh identity differs");
    require(instances.emplace(a.source_node, a.source_mesh, a.source_primitive)
                .second,
            "duplicate native source primitive");
    const ImportedPrimitive *primitive = nullptr;
    for (const auto &p : asset.primitives)
      if (p.source_mesh == a.source_mesh &&
          p.source_primitive == a.source_primitive)
        primitive = &p;
    require(primitive, "original primitive missing");
    const auto &raw_mesh = original.at("meshes").at(size_t(a.source_mesh));
    const auto &raw_primitive =
        raw_mesh.at("primitives").at(size_t(a.source_primitive));
    require(!raw_mesh.contains("extensions") ||
                raw_mesh.at("extensions").empty(),
            "mesh extension needs an explicit payload/reference adapter");
    require(!raw_primitive.contains("extensions") ||
                raw_primitive.at("extensions").empty(),
            "primitive extension needs an explicit payload/reference adapter");
    for (const auto &attribute : raw_primitive.at("attributes").items()) {
      const auto &name = attribute.key();
      bool supported = name == "POSITION" || name == "NORMAL" ||
                       name == "TANGENT" || name == "COLOR_0";
      for (int uv = 0; uv < 8; ++uv)
        supported |= name == "TEXCOORD_" + std::to_string(uv);
      // The preserving importer has already authenticated consecutive paired
      // influence-set names and their complete native skin ownership.
      supported |= name.starts_with("JOINTS_") || name.starts_with("WEIGHTS_");
      require(supported,
              "custom vertex attribute needs an exact current-corner adapter");
    }
    auto &current_mesh = j["meshes"][emitted.mesh];
    if (raw_mesh.contains("extras")) {
      // Opaque extras retain their values. targetNames is the one
      // recognized field whose current morph identities the writer owns.
      require(raw_mesh.at("extras").is_object(),
              "non-object mesh extras need an explicit metadata adapter");
      auto extras = raw_mesh.at("extras");
      extras.erase("targetNames");
      if (current_mesh.contains("extras") &&
          current_mesh.at("extras").contains("targetNames"))
        extras["targetNames"] = current_mesh.at("extras").at("targetNames");
      if (!extras.empty())
        current_mesh["extras"] = std::move(extras);
    }
    if (raw_primitive.contains("extras"))
      current_mesh["primitives"][0]["extras"] = raw_primitive.at("extras");
    Vrm1SurfaceIdentity surface;
    surface.node = emitted.node;
    surface.morph_targets.assign(primitive->morph_targets.size(), -1);
    const size_t targets = object.deformation && object.deformation->morphs
                               ? object.deformation->morphs->targets.size()
                               : 0;
    if (object.material_bindings && object.material_bindings->normal_frames) {
      const auto &frames = *object.material_bindings->normal_frames;
      require(frames.current_targets.size() == targets,
              "current target-frame identity count differs");
      for (size_t target = 0; target < targets; ++target) {
        auto at = std::find_if(frames.targets.begin(), frames.targets.end(),
                               [&](const auto &t) {
                                 return t.id == frames.current_targets[target];
                               });
        require(at != frames.targets.end(), "current target frame missing");
        if (at->source_target) {
          auto source = *at->source_target;
          require(source < surface.morph_targets.size() &&
                      surface.morph_targets[source] < 0,
                  "ambiguous source morph witness");
          surface.morph_targets[source] = int(target);
        }
      }
    } else {
      require(a.source_morph_targets.size() == targets,
              "current morphs lack exact source identities");
      for (size_t t = 0; t < targets; ++t) {
        auto source = a.source_morph_targets[t];
        require(source < surface.morph_targets.size() &&
                    surface.morph_targets[source] < 0,
                "source morph witness out of range or duplicate");
        surface.morph_targets[source] = int(t);
      }
    }
    identities.surfaces[size_t(a.source_node)].push_back(std::move(surface));
    for (auto joint : emitted.joints)
      joints.insert(joint);
    if (a.source_material < 0) {
      require(primitive->material == 0,
              "implicit material source identity differs");
      continue;
    }
    require(size_t(a.source_material) < identities.materials.size() &&
                primitive->material == uint32_t(a.source_material + 1),
            "source material witness differs");
    auto value =
        remap_material(original.at("materials").at(size_t(a.source_material)));
    const auto generated = j["materials"][emitted.material];
    if (object.material_bindings && object.material_bindings->has_mtoon) {
      const auto &binding = *object.material_bindings;
      binding.validate();
      require(binding.material.version() == MToonVersion::V1,
              "current MToon binding is a different source version");
      require(!binding.canonical_attributes&&!binding.canonical_uv&&!binding.native_paint_sampling,
              "retained normal/native paint needs its exact typed V1 adapter");
      require(!binding.normal_preparation,
              "prepared normal interpretation needs a typed V1 adapter");
      require(!binding.color_preparation,
              "independent PBR/MToon colour planes need a proven shared V1 "
              "base-colour representation");
      const auto *compiled = authoring ? authoring->material(object) : nullptr;
      auto material = compiled ? compiled->material : binding.material;
      auto roles = compiled ? compiled->roles : binding.roles;
      if (!compiled) {
      for (size_t c = 0; c < 4; ++c)
        material.parameters.base_color[c] = vrm_native_factor(
            material.parameters.base_color[c],
            binding.native_reference.color[c], object.material.color[c]);
      material.parameters.emission = {
          vrm_native_factor(material.parameters.emission.x,
                            binding.native_reference.emission[0],
                            object.material.emissive[0]),
          vrm_native_factor(material.parameters.emission.y,
                            binding.native_reference.emission[1],
                            object.material.emissive[1]),
          vrm_native_factor(material.parameters.emission.z,
                            binding.native_reference.emission[2],
                            object.material.emissive[2])};
      }
      if (object.material.double_sided != binding.native_reference.double_sided)
        material.parameters.cull =
            object.material.double_sided ? MToonCull::Off : MToonCull::Back;
      if (object.material.alpha != Material::AlphaMode::Automatic &&
          int(object.material.alpha) != binding.native_reference.alpha)
        material.parameters.alpha =
            object.material.alpha == Material::AlphaMode::Opaque
                ? MToonAlpha::Opaque
            : object.material.alpha == Material::AlphaMode::Mask
                ? MToonAlpha::Mask
                : MToonAlpha::Blend;
      if(object.material.alpha_cutoff!=binding.native_reference.alpha_cutoff)
        material.parameters.alpha_cutoff = object.material.alpha_cutoff;
      for (size_t role = 0; role < roles.size(); ++role)
        if (*slots[role]) {
          if (roles[role].image) {
            const auto coordinate = mtoon_coordinate(material.textures[role]);
            const auto &attributes =
                j["meshes"][emitted.mesh]["primitives"][0]["attributes"];
            require(role == size_t(MToonSlot::Matcap) ||
                        attributes.contains("TEXCOORD_" +
                                            std::to_string(coordinate)),
                    "typed role has no emitted source UV witness");
            material.textures[role].index = role_texture(roles[role]);
          } else
            material.textures[role].index = -1;
        }
      value = material.source_json();
      for (const char *slot :
           {"metallicFactor", "roughnessFactor", "metallicRoughnessTexture"}) {
        value["pbrMetallicRoughness"].erase(slot);
        if (generated.at("pbrMetallicRoughness").contains(slot))
          value["pbrMetallicRoughness"][slot] =
              generated.at("pbrMetallicRoughness").at(slot);
      }
      value.erase("occlusionTexture");
      if (generated.contains("occlusionTexture"))
        value["occlusionTexture"] = generated.at("occlusionTexture");
    } else {
      // The generic writer owns actual current PBR images/factors. Preserve
      // other original fields while replacing these explicitly versioned core
      // roles.
      value["pbrMetallicRoughness"] = generated.at("pbrMetallicRoughness");
      for (const char *slot :
           {"normalTexture", "occlusionTexture", "emissiveTexture",
            "emissiveFactor", "alphaMode", "alphaCutoff", "doubleSided"}) {
        value.erase(slot);
        if (generated.contains(slot))
          value[slot] = generated.at(slot);
      }
      if (value.contains("extensions"))
        value["extensions"].erase("KHR_materials_emissive_strength");
    }
    j["materials"][emitted.material] = std::move(value);
    identities.materials[size_t(a.source_material)].push_back(emitted.material);
    if(const auto *layer=authoring?authoring->expression_layer(object):nullptr){
      // Replay the same owning frame expansion as make_gltf, then authenticate
      // the actual emitted index bytes. Never infer correspondence by position.
      auto frame=normal_frame_export(object,SourceMorphSpace::Native,options.cancelled,true);
      const auto lower=j["meshes"][emitted.mesh]["primitives"][0];
      const auto &position=j["accessors"][lower.at("attributes").at("POSITION").get<size_t>()];
      require(position.at("count").get<size_t>()==frame.rest_vertices.size(),"expression layer frame expansion count differs");
      require(frame.surface.mesh.indices.size()==object.mesh.indices.size(),"expression layer ordered corners differ");
      const auto &index=j["accessors"][lower.at("indices").get<size_t>()];
      require(index.at("componentType")==5125&&index.at("type")=="SCALAR"&&index.at("count")==frame.surface.mesh.indices.size(),"expression layer emitted index representation differs");
      const auto &view=j["bufferViews"][index.at("bufferView").get<size_t>()];
      const size_t index_offset=view.value("byteOffset",size_t(0))+index.value("byteOffset",size_t(0));
      require(index_offset<=binary.size()&&frame.surface.mesh.indices.size()<=(binary.size()-index_offset)/4,"expression layer emitted indices exceed binary");
      for(size_t c=0;c<frame.surface.mesh.indices.size();++c){
        if((c&4095)==0)stop(options);
        const size_t at=index_offset+c*4;const uint32_t actual=uint32_t(binary[at])|(uint32_t(binary[at+1])<<8)|(uint32_t(binary[at+2])<<16)|(uint32_t(binary[at+3])<<24);
        require(actual==frame.surface.mesh.indices[c]&&frame.rest_vertices.at(actual)==object.mesh.indices[c],"expression layer actual corner identity differs");
      }
      auto upper=j["materials"][emitted.material];auto &toon=upper["extensions"]["VRMC_materials_mtoon"];
      unsigned channel=0;std::set<unsigned> used;
      auto coordinate=[&](const J &info){unsigned c=info.value("texCoord",0u);if(info.contains("extensions")&&info.at("extensions").contains("KHR_texture_transform"))c=info.at("extensions").at("KHR_texture_transform").value("texCoord",c);used.insert(c);};
      // Every lower role, including the PBR fallback, retains its coordinates.
      const auto &pbr=upper.at("pbrMetallicRoughness");for(const char *key:{"baseColorTexture","metallicRoughnessTexture"})if(pbr.contains(key))coordinate(pbr.at(key));
      for(const char *key:{"normalTexture","occlusionTexture","emissiveTexture"})if(upper.contains(key))coordinate(upper.at(key));
      for(const auto &entry:toon.items())if(entry.value().is_object()&&entry.value().contains("index"))coordinate(entry.value());
      while(channel<8&&used.contains(channel))++channel;
      require(channel<8,"expression layer has no unused genuine texture-coordinate set");
      require(frame.rest_vertices.size()<=(options.max_output_bytes-std::min(binary.size(),options.max_output_bytes))/8,"expression layer coordinates exceed output bound");
      const size_t offset=binary.size();for(auto rest:frame.rest_vertices){stop(options);const auto uv=layer->paint_coordinates.at(rest);for(float f:uv){const uint32_t bits=std::bit_cast<uint32_t>(f);for(unsigned b=0;b<4;++b)binary.push_back(uint8_t(bits>>(b*8)));}}
      const int uv_view=int(j["bufferViews"].size());j["bufferViews"].push_back({{"buffer",0},{"byteOffset",offset},{"byteLength",binary.size()-offset},{"target",34962}});
      const int uv_accessor=int(j["accessors"].size());j["accessors"].push_back({{"bufferView",uv_view},{"componentType",5126},{"count",frame.rest_vertices.size()},{"type","VEC2"}});
      PaintSourceTexture paint;paint.image=layer->image;paint.wrap_s=paint.wrap_t=33071;paint.min_filter=paint.mag_filter=9729;
      const int texture=role_texture(paint);
      upper["pbrMetallicRoughness"]["baseColorFactor"]=std::array<float,4>{1,1,1,1};
      upper["pbrMetallicRoughness"]["baseColorTexture"]={{"index",texture},{"texCoord",channel},{"extensions",{{"KHR_texture_transform",{{"scale",layer->scale},{"offset",layer->offset}}}}}};
      if(layer->shade_alias){
        // The native shared image modifies the shade sample by P/nativeRGB.
        // Keep that independent contribution and its actual shade expressions,
        // rather than reuse the moving original image or tint the paint base.
        toon["shadeMultiplyTexture"]=upper["pbrMetallicRoughness"]["baseColorTexture"];
        auto factor=toon.value("shadeColorFactor",std::array<float,3>{1,1,1});
        for(size_t k=0;k<3;++k){const double value=double(factor[k])/layer->shade_divisor[k];require(std::isfinite(value)&&value>=0&&value<=1,"expression layer shade factor exceeds unit representation");factor[k]=float(value);}
        toon["shadeColorFactor"]=factor;
      }
      upper["alphaMode"]="BLEND";upper.erase("alphaCutoff");toon["transparentWithZWrite"]=false;toon["renderQueueOffsetNumber"]=0;toon["outlineWidthMode"]="none";toon["outlineWidthFactor"]=0;
      upper["name"]=object.name+" independent paint contribution";
      // Source material extras stay on the original lower material. The upper
      // is a derived standard primitive, with no editor authority of its own.
      upper.erase("extras");(void)MToonMaterial::from_vrm1(upper,j["textures"].size());
      const int upper_material=int(j["materials"].size());j["materials"].push_back(std::move(upper));auto primitive=lower;primitive.erase("extras");primitive["material"]=upper_material;primitive["attributes"]["TEXCOORD_"+std::to_string(channel)]=uv_accessor;j["meshes"][emitted.mesh]["primitives"].push_back(std::move(primitive));
      expression_layers.push_back({a.source_material,emitted.material,upper_material,layer->forward_uv,layer->shade_alias,layer->shade_divisor});budget();
    }

    ++report.meshes;
  }
  size_t expected = 0;
  for (size_t n = 0; n < asset.nodes.size(); ++n)
    if (asset.nodes[n].mesh >= 0)
      for (auto primitive :
           asset.meshes.at(size_t(asset.nodes[n].mesh)).primitives) {
        const auto &p = asset.primitives.at(primitive);
        require(
            ++expected <= instances.size() &&
                instances.contains({int(n), p.source_mesh, p.source_primitive}),
            "missing original mesh instance cannot be discarded");
      }
  require(expected == instances.size(), "source mesh instance set differs");
  for (size_t source = 0; source < identities.materials.size(); ++source)
    if (identities.materials[source].empty()) {
      identities.materials[source].push_back(int(j["materials"].size()));
      j["materials"].push_back(
          remap_material(original.at("materials")[source]));
    }
  auto source = original;
  source["extensions"]["VRMC_vrm"]["humanoid"] = graph.humanoid;
  if (graph.fitted_rest &&
      source["extensions"]["VRMC_vrm"].contains("lookAt")) {
    auto old_head = original.at("extensions")
                        .at("VRMC_vrm")
                        .at("humanoid")
                        .at("humanBones")
                        .at("head")
                        .at("node")
                        .get<size_t>();
    auto head =
        graph.humanoid.at("humanBones").at("head").at("node").get<size_t>();
    const auto &old = asset.nodes.at(old_head).world;
    const auto &now = graph.rest_world.at(head);
    for (size_t column = 0; column < 3; ++column)
      for (size_t row = 0; row < 3; ++row)
        require(old[column * 4 + row] == now[column * 4 + row],
                "look-at REST axis changes need a version-specific range/frame "
                "adapter");
    auto &look = source["extensions"]["VRMC_vrm"]["lookAt"];
    auto offset = look.value("offsetFromHeadBone", std::array<double, 3>{});
    auto mapped =
        (AffineTransform(now).inverse() * AffineTransform(old)).point(offset);
    look["offsetFromHeadBone"] = mapped;
  }
  auto extensions = remap_vrm1_metadata(source, identities, options.cancelled);
  for (auto &extension : extensions.items())
    j["extensions"][extension.key()] = std::move(extension.value());
  remap_vrm1_constraints(j, source, identities, options.cancelled);
  if(!expression_layers.empty()){
    auto &definitions=j["extensions"]["VRMC_vrm"]["expressions"];
    for(const auto *table:{"preset","custom"})if(definitions.contains(table))for(auto &expression:definitions[table]){
      for(const char *kind:{"textureTransformBinds","materialColorBinds"})if(expression.contains(kind)){
        const auto original_binds=expression[kind];
        for(const auto &bind:original_binds)for(const auto &layer:expression_layers)if(bind.at("material").get<int>()==layer.lower){
          if(std::string_view(kind)=="textureTransformBinds"&&!layer.forward_uv)continue;
          if(std::string_view(kind)=="materialColorBinds"&&(bind.at("type")=="color"||bind.at("type")=="outlineColor"))continue;
          auto forwarded=bind;forwarded["material"]=layer.upper;
          if(layer.shade_alias&&std::string_view(kind)=="materialColorBinds"&&bind.at("type")=="shadeColor")for(size_t k=0;k<3;++k){const double value=bind.at("targetValue").at(k).get<double>()/layer.shade_divisor[k];require(std::isfinite(value)&&value>=0&&value<=1e20,"expression layer shade target exceeds finite nonnegative source representation");forwarded["targetValue"][k]=value;}
          expression[kind].push_back(std::move(forwarded));
        }
      }
    }
  }

  auto use = [&](const J &extension) {
    auto &used = j["extensionsUsed"];
    if (!used.is_array())
      used = J::array();
    if (std::find(used.begin(), used.end(), extension) == used.end())
      used.push_back(extension);
  };
  for (const auto &extension : original.value("extensionsUsed", J::array()))
    use(extension);
  use("VRMC_vrm");
  if(!expression_layers.empty())use("KHR_texture_transform");
  if (original.contains("extensionsRequired"))
    j["extensionsRequired"] = original.at("extensionsRequired");
  if (original.contains("extras"))
    j["extras"] = original.at("extras");
  report.meshes = objects.size();
  report.joints = joints.size();
  report.materials = j["materials"].size();
  report.diagnostics.push_back(
      "Source-version VRM1 metadata/node/material/morph identities preserved; "
      "physics and constraint simulation are not evaluated by this exporter");
  if(physical){finish_emission_export(document,*physical,*options.emission,options.max_output_bytes,options.cancelled);validate_vrm1_metadata(j,options.cancelled,true);for(const auto &image:physical->images)report.diagnostics.push_back("Exact native/FLOAT32 HDR object "+std::to_string(image.object)+"; measured VRM1 fallback maximum radiance error "+std::to_string(image.maximum_absolute_error));}
  if(authoring) {
    attach_native_authoring(document,authoring->capture(),options.max_output_bytes,options.cancelled);
    report.diagnostics.insert(report.diagnostics.end(),authoring->diagnostics().begin(),authoring->diagnostics().end());
    for(const auto &object:objects) if(authoring->compiled(object)) ++report.painted_materials;
    report.diagnostics.push_back("Exact native authoring Scene retained; portable typed MToon1 view is not declared lossless");
  }
  budget();
  const auto json_bytes = j.dump().size();
  require(json_bytes <= options.max_output_bytes - binary.size() &&
              json_bytes + binary.size() + 28 <= options.max_output_bytes,
          "complete GLB output bound exceeded");
  write_glb(std::move(document), path, options.cancelled,
            options.before_commit);
  return report;
}
VrmExportReport export_vrm1(const std::vector<ObjectSurface> &objects,
    const J &original,const std::filesystem::path &path,
    const VrmExportOptions &options,EmissionExportPrepared *physical) {
  return export_vrm1_impl(objects,original,path,options,physical,nullptr);
}
VrmExportReport export_vrm1_authoring(const Vrm1AuthoringCompilation &compiled,
    const std::filesystem::path &path,const VrmExportOptions &options) {
  if(options.emission)throw std::invalid_argument("Competing native HDR and VRM1 authoring export owners");
  return export_vrm1_impl(compiled.surfaces(),compiled.original(),path,options,nullptr,&compiled);
}
} // namespace dome::detail
