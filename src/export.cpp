// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include "io.hpp"
#include <bit>
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
  if (path.extension() != ".glb")
    throw std::invalid_argument(
        "This exporter currently writes binary glTF (.glb)");
  if (objects.empty())
    throw std::invalid_argument("The scene has no visible surface to export");
  nlohmann::json j = {
      {"asset", {{"version", "2.0"}, {"generator", "Dome Sculpt"}}},
      {"scene", 0},
      {"scenes", {{{"nodes", nlohmann::json::array()}}}}};
  std::vector<uint8_t> data;
  auto floats = [&](auto values) {
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
  for (auto &o : objects) {
    auto &mesh = o.mesh;
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
    if (o.deformation && o.deformation->skin) {
      const auto &weights = o.deformation->skin->vertices;
      size_t sets = 0;
      for (const auto &vertex : weights)
        sets = std::max(sets, (vertex.size() + 3) / 4);
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
    auto t = o.transform;
    auto &m = o.material;
    j["scenes"][0]["nodes"].push_back(node);
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
          targets.push_back({{"POSITION", accessor}});
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
        j["nodes"][node].erase("mesh");
        j["nodes"][node]["children"] = nlohmann::json::array({mesh_node});
        j["nodes"].push_back(
            {{"name", o.name + " surface"}, {"mesh", mesh_index}});
        int first_joint = int(j["nodes"].size());
        for (size_t index = 0; index < s.bones.size(); ++index) {
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
  }
  for (size_t i = 0; i < objects.size(); ++i)
    if (objects[i].textures) {
      if (objects[i].mesh.uv.empty())
        throw std::invalid_argument("Textured meshes require UV coordinates");
      auto &maps = *objects[i].textures;
      auto &material = j["materials"][i];
      if (j["samplers"].empty())
        j["samplers"] = {{{"magFilter", 9729},
                          {"minFilter", 9729},
                          {"wrapS", 33071},
                          {"wrapT", 33071}}};
      auto texture = [&](const Image &image) {
        auto bytes = image.png();
        int view = int(j["bufferViews"].size()),
            source = int(j["images"].size()), index = int(j["textures"].size());
        j["bufferViews"].push_back({{"buffer", 0},
                                    {"byteOffset", data.size()},
                                    {"byteLength", bytes.size()}});
        data.insert(data.end(), bytes.begin(), bytes.end());
        while (data.size() % 4)
          data.push_back(0);
        j["images"].push_back(
            {{"bufferView", view}, {"mimeType", "image/png"}});
        j["textures"].push_back({{"source", source}, {"sampler", 0}});
        return index;
      };
      auto &pbr = material["pbrMetallicRoughness"];
      pbr["baseColorFactor"] = {1, 1, 1, 1};
      pbr["metallicFactor"] = 1;
      pbr["roughnessFactor"] = 1;
      pbr["baseColorTexture"] = {{"index", texture(maps.color)}};
      int orm = texture(maps.metal_rough);
      pbr["metallicRoughnessTexture"] = {{"index", orm}};
      material["occlusionTexture"] = {{"index", orm}};
      if (!maps.normal.rgba.empty() || maps.normal.width || maps.normal.height)
        material["normalTexture"] = {{"index", texture(maps.normal)}};
      material["emissiveTexture"] = {{"index", texture(maps.emission)}};
      material["emissiveFactor"] = {1, 1, 1};
      material["alphaMode"] = maps.transparent ? "BLEND" : "OPAQUE";
    }
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
