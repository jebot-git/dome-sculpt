// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/humanoid-creator.hpp"
#include "dome/projection.hpp"
#include "dome/source-maps.hpp"
#include "humanoid-sha256.hpp"
#include "vroid-groom.hpp"
#include "dome/normal-frames.hpp"
#include "source-frame-shape.hpp"
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <numeric>
#include <set>
namespace dome {
CreatedHumanoid create_makehuman_humanoid(const HumanoidControls &,
                                          const std::function<bool()> &);
namespace {
struct AssetIdentity {
  const char *id;
  const char *file;
  const char *title;
  const char *hash;
};
constexpr AssetIdentity identities[] = {
    {"vroid-beta-female", "base-female.vrm", "Base_Female",
     "1995f174f6c8f2211f3c56c9e5fe4f9daf27ee5e648f129479582a259238149b"},
    {"vroid-beta-male", "base-male.vrm", "Base_Male",
     "ab9af434a229283c192546486f9e6eb9cd855bf7d8d45b5037ac211938cb903c"}};
void stop(const std::function<bool()> &cancel) {
  if (cancel && cancel())
    throw std::runtime_error("VRoid baseline creation cancelled");
}
[[noreturn]] void fail(const std::string &s) {
  throw std::runtime_error("VRoid baseline: " + s);
}
uint32_t word(std::span<const uint8_t> b, size_t at) {
  if (at + 4 > b.size())
    fail("truncated source header");
  return uint32_t(b[at]) | uint32_t(b[at + 1]) << 8 |
         uint32_t(b[at + 2]) << 16 | uint32_t(b[at + 3]) << 24;
}
Vec3 flip(Vec3 p) { return {-p.x, p.y, -p.z}; }
const AssetIdentity &identity(std::string_view id) {
  for (const auto &i : identities)
    if (i.id == id)
      return i;
  fail("unknown licensed asset identity");
}
std::filesystem::path locate(const AssetIdentity &i,
                             const std::filesystem::path &explicit_root) {
  std::vector<std::filesystem::path> roots;
  if (!explicit_root.empty())
    roots.push_back(explicit_root);
  else {
    std::error_code ec;
    auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec)
      roots.push_back(exe.parent_path() /
                      "../share/dome-sculpt/assets/humanoid");
#ifdef DOME_SCULPT_SOURCE_ASSETS
    roots.emplace_back(DOME_SCULPT_SOURCE_ASSETS);
#else
    auto source = std::filesystem::path(__FILE__);
    if (source.is_absolute())
      roots.push_back(source.parent_path().parent_path() / "assets/humanoid");
#endif
#ifdef DOME_SCULPT_INSTALLED_ASSETS
    roots.emplace_back(DOME_SCULPT_INSTALLED_ASSETS);
#endif
  }
  for (const auto &root : roots) {
    auto path = root / "vroid" / i.file;
    if (std::filesystem::is_regular_file(path))
      return std::filesystem::canonical(path);
  }
  fail("required CC0 source missing: " + std::string(i.file) +
       " (install the matching humanoid assets)");
}
struct Source {
  std::shared_ptr<const std::vector<uint8_t>> original_bytes;
  std::shared_ptr<const ImportedAsset> asset;
  std::shared_ptr<const std::vector<std::shared_ptr<const Image>>> images;
  nlohmann::json json;
  std::shared_ptr<const std::string> vrm_json;
  Skeleton skeleton;
  HumanoidMapping mapping;
  std::vector<int> node_to_joint;
};
std::shared_ptr<const Source> load(const AssetIdentity &id,
                                   const std::filesystem::path &root,
                                   const std::function<bool()> &cancel) {
  auto path = locate(id, root);
  std::string key = path.string() + ":" + id.hash;
  static std::mutex mutex;
  static std::map<std::string, std::shared_ptr<const Source>> cache;
  {
    std::lock_guard lock(mutex);
    if (auto previous = cache.contains(key) ? cache.at(key) : nullptr) {
      stop(cancel);
      return previous;
    }
  }
  stop(cancel);
  auto size = std::filesystem::file_size(path);
  if (size > 64 * 1024 * 1024 || size < 28)
    fail("source byte size outside bounds");
  std::ifstream input(path, std::ios::binary);
  if (!input)
    fail("source cannot be opened");
  std::vector<uint8_t> bytes(size);
  input.read(reinterpret_cast<char *>(bytes.data()), std::streamsize(size));
  if (!input)
    fail("source read incomplete");
  if (creator_detail::sha256(bytes) != id.hash)
    fail("CC0 source SHA256 identity mismatch: " + path.string());
  if (word(bytes, 0) != 0x46546c67 || word(bytes, 4) != 2 ||
      word(bytes, 8) != bytes.size() || word(bytes, 16) != 0x4e4f534a)
    fail("invalid licensed source GLB header");
  size_t json_size = word(bytes, 12);
  if (json_size > 8 * 1024 * 1024 || 20 + json_size > bytes.size())
    fail("source JSON resource bounds exceeded");
  auto source = std::make_shared<Source>();
  source->json =
      nlohmann::json::parse(bytes.begin() + 20, bytes.begin() + 20 + json_size);
  const auto &vrm = source->json.at("extensions").at("VRM");
  if (vrm.at("specVersion") != "0.0" ||
      vrm.at("exporterVersion") != "VRoidStudio-0.14.0" ||
      vrm.at("meta").at("title") != id.title ||
      vrm.at("meta").at("licenseName") != "CC0")
    fail("author/exporter/license metadata differs from pinned identity");
  source->vrm_json = std::make_shared<const std::string>(vrm.dump());
  ImportLimits options;
  options.mode = ImportMode::PreserveGltf;
  options.external_resources = false;
  options.allow_legacy_vrm_skin_hint = true;
  options.cancelled = cancel;
  auto asset = std::make_shared<ImportedAsset>(import_glb(bytes, options));
  if (asset->primitives.empty() || asset->primitives.size() > 64 ||
      asset->nodes.size() > 256 || asset->images.size() > 64)
    fail("unexpected source dimensions");
  source->node_to_joint.assign(asset->nodes.size(), -1);
  std::set<uint32_t> nodes;
  for (const auto &skin : asset->skins)
    for (auto joint : skin.joints)
      for (int n = int(joint); n >= 0; n = asset->nodes[n].parent)
        nodes.insert(uint32_t(n));
  if (nodes.empty() || nodes.size() > 256)
    fail("source joint hierarchy outside bounds");
  for (auto node : nodes) {
    source->node_to_joint[node] = int(source->skeleton.bones.size());
    Bone b;
    b.name = asset->nodes[node].name;
    source->skeleton.bones.push_back(b);
  }
  for (auto node : nodes) {
    auto &b = source->skeleton.bones[source->node_to_joint[node]];
    const auto &n = asset->nodes[node];
    if (n.has_matrix)
      fail("known normalized source unexpectedly uses matrix joints");
    b.parent = n.parent < 0 ? -1 : source->node_to_joint[n.parent];
    b.rest.translation = {n.translation[0], n.translation[1], n.translation[2]};
    b.rest.rotation = {n.rotation[3], n.rotation[0], n.rotation[1],
                       n.rotation[2]};
    b.rest.scale = {n.scale[0], n.scale[1], n.scale[2]};
    b.length = .025f;
    for (auto child : n.children)
      if (nodes.contains(child)) {
        const auto &t = asset->nodes[child].translation;
        float len = length({t[0], t[1], t[2]});
        if (len > .0001f) {
          b.length = len;
          break;
        }
      }
  }
  auto inverse = source->skeleton.inverse_bind();
  for (const auto &skin : asset->skins) {
    if (!skin.inverse_bind_matrices_authored ||
        skin.inverse_bind_matrices.size() < skin.joints.size())
      fail("known source lacks authored inverse-bind data");
    for (size_t slot = 0; slot < skin.joints.size(); ++slot) {
      const auto &a = skin.inverse_bind_matrices[slot];
      const auto &b = inverse[source->node_to_joint[skin.joints[slot]]];
      for (size_t k = 0; k < 16; ++k)
        if (std::abs(a[k] - b[k]) > 2e-5f * (1 + std::abs(b[k])))
          fail("authored inverse bind differs from native joint rest hierarchy "
               "(slot " +
               std::to_string(slot) + ")");
    }
  }
  for (const auto &entry : vrm.at("humanoid").at("humanBones")) {
    std::string role = entry.at("bone");
    if (role.ends_with("ThumbProximal"))
      role.replace(role.size() - 8, 8, "Metacarpal");
    else if (role.ends_with("ThumbIntermediate"))
      role.replace(role.size() - 12, 12, "Proximal");
    uint32_t node = entry.at("node");
    if (node >= source->node_to_joint.size() ||
        source->node_to_joint[node] < 0 ||
        !source->mapping.joints
             .emplace(role, uint32_t(source->node_to_joint[node]))
             .second)
      fail("invalid source humanoid assignment");
  }
  source->mapping.validate(source->skeleton);
  auto images = std::make_shared<std::vector<std::shared_ptr<const Image>>>();
  uint64_t pixels = 0;
  for (const auto &encoded : asset->images) {
    stop(cancel);
    if (encoded.mime_type != "image/png")
      fail("known source image is not PNG");
    auto image = decode_projection_png(encoded.bytes);
    pixels += uint64_t(image.width) * image.height;
    if (pixels > 64ull * 1024 * 1024)
      fail("decoded source image budget exceeded");
    images->push_back(std::make_shared<const Image>(std::move(image)));
  }
  source->original_bytes =
      std::make_shared<const std::vector<uint8_t>>(std::move(bytes));
  source->images = images;
  source->asset = asset;
  stop(cancel);
  {
    std::lock_guard lock(mutex);
    if (cache.size() >= 2)
      cache.erase(cache.begin());
    cache[key] = source;
  }
  return source;
}
float smooth(float a, float b, float x) {
  float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
  return t * t * (3 - 2 * t);
}
struct Shaper {
  const HumanoidControls &c;
  Vec3 hips, neck, head, left_eye, right_eye;
  float floor = 0, scale = 1;
  Vec3 raw(Vec3 original) const {
    Vec3 p = flip(original);
    float torso = smooth(hips.y - .05f, hips.y + .08f, p.y) *
                  (1 - smooth(neck.y - .08f, neck.y, p.y));
    float upper = smooth(hips.y + .1f, neck.y - .05f, p.y) *
                  (1 - smooth(neck.y, neck.y + .04f, p.y));
    float waist = std::exp(-std::pow((p.y - (hips.y + .12f)) / .12f, 2.f));
    p.x *= 1 + c.build * .08f * torso + c.shoulder_width * .1f * upper +
           c.waist_width * .12f * waist;
    p.z *= 1 + c.build * .10f * torso;
    if (p.y < hips.y)
      p.y = hips.y + (p.y - hips.y) * (1 + c.leg_length * .12f);
    float shoulder = std::max(.08f, std::abs(left_eye.x) * 3.5f);
    if (std::abs(p.x) > shoulder)
      p.x += (p.x < 0 ? -1.f : 1.f) * (std::abs(p.x) - shoulder) *
             c.arm_length * .12f;
    float h = smooth(neck.y - .015f, neck.y + .04f, p.y) *
              (1 - smooth(.12f, .22f, std::abs(p.x)));
    p.x *= 1 + h * (c.head_width * .10f + c.anime * .08f);
    p.y += h * (p.y - neck.y) * (c.head_height * .08f + c.anime * .06f);
    p.z += h * (p.z - head.z) * c.anime * .04f;
    for (auto eye : {left_eye, right_eye}) {
      Vec3 d = p - eye;
      float rx = std::max(.025f, std::abs(left_eye.x - right_eye.x) * .75f),
            ry = .045f;
      float w = std::exp(-(d.x * d.x / (rx * rx) + d.y * d.y / (ry * ry))) *
                smooth(0, .1f, p.z - head.z);
      p.x += (d.x * c.eye_size * .18f +
              (eye.x < 0 ? -1.f : 1.f) * c.eye_spacing * .008f) *
             w;
      p.y += d.y * c.eye_size * .22f * w;
    }
    float nose = std::exp(-std::pow(p.x / .025f, 2.f) -
                          std::pow((p.y - (left_eye.y - .035f)) / .035f, 2.f));
    p.z += (p.z - head.z) * c.nose_size * .15f * nose;
    float mouth = std::exp(-std::pow(p.x / .055f, 2.f) -
                           std::pow((p.y - (left_eye.y - .075f)) / .025f, 2.f));
    p.x *= 1 + c.mouth_width * .12f * mouth;
    float jaw = std::exp(-std::pow((p.y - (head.y - .01f)) / .045f, 2.f));
    p.x *= 1 + c.jaw_width * .08f * jaw;
    return p;
  }
  Vec3 operator()(Vec3 p) const {
    p = raw(p);
    return {p.x * scale, (p.y - floor) * scale, p.z * scale};
  }
  bool neutral_frame() const {
    return c.build == 0 && c.shoulder_width == 0 && c.waist_width == 0 &&
        c.leg_length == 0 && c.arm_length == 0 && c.head_width == 0 &&
        c.head_height == 0 && c.anime == 0 && c.eye_size == 0 &&
        c.eye_spacing == 0 && c.nose_size == 0 && c.mouth_width == 0 &&
        c.jaw_width == 0;
  }
  std::array<Vec3, 3> differential_axes(Vec3 p) const {
    if (neutral_frame())
      return {flip({1, 0, 0}), flip({0, 1, 0}), flip({0, 0, 1})};
    constexpr float step = .0001f;
    Vec3 a = (raw(p + Vec3{step, 0, 0}) - raw(p - Vec3{step, 0, 0})) /
             (2 * step),
         b = (raw(p + Vec3{0, step, 0}) - raw(p - Vec3{0, step, 0})) /
             (2 * step),
         d = (raw(p + Vec3{0, 0, step}) - raw(p - Vec3{0, 0, step})) /
             (2 * step);
    float det = dot(a, cross(b, d));
    if (!std::isfinite(det) || det < .05f)
      fail("style warp has singular/reflected local frame");
    return {a, b, d};
  }
  // Private creation-time hook: future immutable native frame keys must use
  // this actual shape at each original base/target position, not infer a
  // nonlinear transform from the final vertices. No archive/layout change.
  RigMatrix frame_differential(Vec3 p) const {
    auto axes = differential_axes(p);
    for (auto &axis : axes) axis = axis * scale;
    const auto &a = axes[0], &b = axes[1], &d = axes[2];
    return {a.x, a.y, a.z, 0, b.x, b.y, b.z, 0, d.x, d.y, d.z, 0,
            0, 0, 0, 1};
  }
  Vec3 normal(Vec3 p, Vec3 n) const {
    if (neutral_frame())
      return flip(n);
    auto axes = differential_axes(p);
    const auto &a = axes[0], &b = axes[1], &d = axes[2];
    float det = dot(a, cross(b, d));
    return normalized(
        (cross(b, d) * n.x + cross(d, a) * n.y + cross(a, b) * n.z) / det);
  }
};
} // namespace
namespace {
std::shared_ptr<const PaintSourceMaps> source_maps(const Source &s,
                                                   const ImportedMaterial &m) {
  auto maps = std::make_shared<PaintSourceMaps>();
  auto view = [&](PaintSourceTexture &out, int texture, int texcoord) {
    if (texture < 0)
      return;
    if (size_t(texture) >= s.asset->textures.size())
      fail("material texture outside source table");
    const auto &t = s.asset->textures[texture];
    if (t.image < 0 || size_t(t.image) >= s.images->size())
      fail("material image outside source table");
    out.image = s.images->at(t.image);
    out.wrap_s = t.wrap_s;
    out.wrap_t = t.wrap_t;
    out.min_filter = t.min_filter;
    out.mag_filter = t.mag_filter;
    out.texcoord = texcoord;
  };
  view(maps->color, m.color_texture, m.color_texcoord);
  view(maps->metal_rough, m.metal_rough_texture, m.metal_rough_texcoord);
  view(maps->emission, m.emissive_texture, m.emissive_texcoord);
  view(maps->normal, m.normal_texture, m.normal_texcoord);
  view(maps->occlusion, m.occlusion_texture, m.occlusion_texcoord);
  maps->normal_scale = m.normal_scale;
  maps->occlusion_strength = m.occlusion_strength;
  return maps;
}
std::string morph_name(const Source &s, int mesh, size_t target) {
  const auto &names = s.asset->meshes[mesh].target_names;
  if (target < names.size() && !names[target].empty())
    return names[target].substr(0, 100) + " [" + std::to_string(target) + "]";
  for (const auto &group : s.json.at("extensions")
                               .at("VRM")
                               .at("blendShapeMaster")
                               .at("blendShapeGroups"))
    for (const auto &bind : group.at("binds"))
      if (bind.at("mesh") == mesh && bind.at("index") == target)
        return group.at("name").get<std::string>().substr(0, 100) + " [" +
               std::to_string(target) + "]";
  return "Source target " + std::to_string(target);
}
std::optional<AvatarExpressions> expressions(const Source &s, int mesh,
                                             const MorphSet &targets) {
  AvatarExpressions result;
  const std::map<std::string, std::string> presets{
      {"a", "aa"}, {"i", "ih"}, {"u", "ou"}, {"e", "ee"}, {"o", "oh"}};
  for (const auto &group : s.json.at("extensions")
                               .at("VRM")
                               .at("blendShapeMaster")
                               .at("blendShapeGroups")) {
    std::string p = group.at("presetName");
    auto found = presets.find(p);
    if (found == presets.end())
      continue;
    for (const auto &bind : group.at("binds"))
      if (bind.at("mesh") == mesh) {
        size_t index = bind.at("index");
        float weight = bind.at("weight").get<float>() * .01f;
        if (index >= targets.targets.size())
          fail("vowel binding outside source target table");
        if (weight > 0)
          result.visemes[found->second].push_back(
              {targets.targets[index].name, weight});
      }
  }
  if (result.visemes.empty())
    return {};
  result.validate(targets);
  return result;
}
CreatedHumanoid create_vroid(const HumanoidControls &c,
                             const std::function<bool()> &cancel,
                             const std::filesystem::path &root) {
  c.validate();
  auto source = load(identity(c.baseline), root, cancel);
  const auto &a = *source->asset;
  auto worlds = source->skeleton.world();
  auto anchor = [&](const char *role) {
    auto m = worlds[source->mapping.joints.at(role)];
    return flip({m[12], m[13], m[14]});
  };
  Shaper shape{c,
               anchor("hips"),
               anchor("neck"),
               anchor("head"),
               anchor("leftEye"),
               anchor("rightEye")};
  shape.floor = 1000;
  float ceiling = -1000;
  for (const auto &primitive : a.primitives)
    for (auto vertex : primitive.mesh.indices) {
      auto p = shape.raw(primitive.mesh.positions[vertex]);
      shape.floor = std::min(shape.floor, p.y);
      ceiling = std::max(ceiling, p.y);
    }
  if (ceiling - shape.floor < 1)
    fail("source body height is implausible");
  shape.scale = c.height / (ceiling - shape.floor);
  // These specific VRM0 sources are normalized. Preserve their joint axes and
  // reject unexpected bind-frame variants, rather than changing authored IBMs.
  Skeleton skeleton = source->skeleton;
  std::vector<Vec3> joint_positions;
  for (const auto &matrix : worlds) {
    for (size_t k = 0; k < 12; ++k) {
      float expected = k == 0 || k == 5 || k == 10 ? 1.f : 0.f;
      if (std::abs(matrix[k] - expected) > 1e-5f)
        fail("known VRM0 baseline is not normalized");
    }
    joint_positions.push_back(shape({matrix[12], matrix[13], matrix[14]}));
  }
  for (size_t i = 0; i < skeleton.bones.size(); ++i) {
    auto &bone = skeleton.bones[i];
    bone.rest.translation =
        bone.parent < 0 ? joint_positions[i]
                        : joint_positions[i] - joint_positions[bone.parent];
    bone.rest.rotation = {};
    bone.rest.scale = {1, 1, 1};
    bone.length *= shape.scale;
  }
  skeleton.validate();
  skeleton.inverse_bind();
  source->mapping.validate(skeleton);
  CreatedHumanoid output;
  output.controls = c;
  bool primary = false;
  for (size_t primitive_index = 0; primitive_index < a.primitives.size();
       ++primitive_index) {
    stop(cancel);
    const auto &p = a.primitives[primitive_index];
    if (!p.mesh_local || p.source_mesh < 0 || p.material >= a.materials.size())
      fail("source primitive lacks authored identity");
    for (size_t i = 0; i < 16; ++i) {
      float expected = i % 5 == 0 ? 1.f : 0.f;
      if (std::abs(p.source_world[i] - expected) > 1e-5f)
        fail("known source mesh transform is not identity");
    }
    int source_node = -1;
    for (size_t n = 0; n < a.nodes.size(); ++n)
      if (a.nodes[n].mesh == p.source_mesh) {
        if (source_node >= 0)
          fail("known baseline mesh is unexpectedly multiply instanced");
        source_node = int(n);
      }
    if (source_node < 0 || p.material == 0)
      fail("source primitive lacks a unique instance/material");
    const auto &node = a.nodes[source_node];
    if (node.skin < 0 || size_t(node.skin) >= a.skins.size())
      fail("source primitive lacks skin");
    const auto &skin = a.skins[node.skin];
    const auto &material = a.materials[p.material];
    std::set<uint32_t> used(p.mesh.indices.begin(), p.mesh.indices.end());
    if (used.empty())
      continue;
    std::vector<uint32_t> original(used.begin(), used.end()),
        remap(p.mesh.positions.size(), UINT32_MAX);
    HumanoidPart part;
    part.name = a.meshes[p.source_mesh].name + " / " + material.name + " #" +
                std::to_string(p.source_primitive);
    part.color = material.color;
    part.roughness = material.roughness;
    part.toon = true;
    part.unlit = material.unlit;
    std::string lower = material.name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char v) { return char(std::tolower(v)); });
    std::array<float, 4> tint{1, 1, 1, 1};
    if (lower.find("hair") != std::string::npos)
      tint = c.hair_color;
    else if (lower.find("eyeiris") != std::string::npos)
      tint = c.iris_color;
    else if (lower.find("_skin") != std::string::npos)
      tint = c.skin_color;
    for (size_t i = 0; i < 4; ++i)
      part.color[i] *= tint[i];
    for (auto vertex : original) {
      remap[vertex] = uint32_t(part.mesh.positions.size());
      part.mesh.positions.push_back(shape(p.mesh.positions[vertex]));
      part.mesh.normals.push_back(
          shape.normal(p.mesh.positions[vertex], p.mesh.normals[vertex]));
      if (!p.mesh.uv.empty())
        part.mesh.uv.push_back(p.mesh.uv[vertex]);
    }
    for (auto vertex : p.mesh.indices)
      part.mesh.indices.push_back(remap[vertex]);
    part.mesh.validate();
    part.deformation = DeformationState(part.mesh);
    part.deformation.skeleton = skeleton;
    part.deformation.humanoid = source->mapping;
    part.deformation.skin = SkinWeights{};
    for (auto vertex : original) {
      std::map<uint32_t, float> accumulated;
      for (const auto &set : p.influences)
        for (size_t k = 0; k < 4; ++k) {
          float weight = set.weights.at(vertex)[k];
          if (weight <= 0)
            continue;
          uint32_t slot = set.joints.at(vertex)[k];
          if (slot >= skin.joints.size())
            fail("skin joint slot outside authored skin");
          int joint = source->node_to_joint[skin.joints[slot]];
          if (joint < 0)
            fail("weighted joint outside native skeleton");
          accumulated[uint32_t(joint)] += weight;
        }
      VertexWeights weights;
      for (auto [joint, weight] : accumulated)
        weights.push_back({joint, weight});
      part.deformation.skin->vertices.push_back(normalize_weights(
          std::move(weights), skeleton.bones.size(), {}, {32, 0}));
    }
    if (!p.morph_targets.empty()) {
      part.deformation.morphs = MorphSet(part.mesh);
      for (size_t target = 0; target < p.morph_targets.size(); ++target) {
        stop(cancel);
        const auto &delta = p.morph_targets[target].positions;
        std::vector<Vec3> positions;
        positions.reserve(original.size());
        for (auto vertex : original)
          positions.push_back(
              shape(p.mesh.positions[vertex] +
                    (delta.empty() ? Vec3{} : delta.at(vertex))));
        part.deformation.morphs->set_target(
            part.mesh, morph_name(*source, p.source_mesh, target), positions);
      }
      const auto &defaults =
          node.weights.empty() ? a.meshes[p.source_mesh].weights : node.weights;
      if (!defaults.empty())
        part.deformation.morph_weights = defaults;
      part.deformation.expressions =
          expressions(*source, p.source_mesh, *part.deformation.morphs);
    }
    auto appearance = std::make_shared<CreatorAppearance>();
    appearance->original_bytes = source->original_bytes;
    appearance->asset = source->asset;
    appearance->decoded_images = source->images;
    appearance->material = int(p.material);
    appearance->source_mesh = p.source_mesh;
    appearance->source_node = source_node;
    appearance->source_material = int(p.material) - 1;
    appearance->source_primitive = p.source_primitive;
    appearance->source_vertices = std::move(original);
    appearance->vrm_json = source->vrm_json;
    appearance->source_basis_quaternion = Quat{0, 0, 1, 0};
    appearance->source_joint_nodes.assign(skeleton.bones.size(), UINT32_MAX);
    for (size_t node = 0; node < source->node_to_joint.size(); ++node)
      if (source->node_to_joint[node] >= 0)
        appearance->source_joint_nodes[size_t(source->node_to_joint[node])] =
            uint32_t(node);
    std::set<uint32_t> unique_nodes;
    for (size_t joint = 0; joint < skeleton.bones.size(); ++joint) {
      auto node = appearance->source_joint_nodes[joint];
      if (node >= a.nodes.size() || !unique_nodes.insert(node).second)
        fail("source joint-node map is incomplete or not unique");
      int parent = a.nodes[node].parent;
      int expected = parent < 0 ? -1 : source->node_to_joint[size_t(parent)];
      if (skeleton.bones[joint].parent != expected)
        fail("native joint hierarchy differs from explicit source node "
             "identity");
    }

    appearance->source_morph_targets.resize(p.morph_targets.size());
    std::iota(appearance->source_morph_targets.begin(),
              appearance->source_morph_targets.end(), 0u);
    appearance->material_json =
        source->json.at("materials").at(p.material - 1).dump();
    appearance->mtoon_json = source->json.at("extensions")
                                 .at("VRM")
                                 .at("materialProperties")
                                 .at(p.material - 1)
                                 .dump();
    part.appearance = appearance;
    part.source_maps = source_maps(*source, material);
    part.deformation.validate(part.mesh);
    // Mint original-attribute authority while this exact nonlinear Shaper is
    // available. Existing mesh/POSITION target creation remains unchanged.
    auto frame_set=std::make_shared<NormalFrameSet>();
    for(size_t target=0;target<p.morph_targets.size();++target) {
      frame_set->targets.push_back({target+1,uint32_t(target),uint32_t(target),{}});
      frame_set->current_targets.push_back(target+1);
    }
    frame_set->next_target=p.morph_targets.size()+1;
    auto mint=[&](uint32_t coordinate) {
      const auto &frame_uv=coordinate==0?p.mesh.uv:p.uv1;
      auto original_frames=detail::source_tangent_frames(p,frame_uv,cancel);
      auto native=detail::source_shape_key_frames(p,original_frames,[&](Vec3 point){return shape.frame_differential(point);},cancel);
      auto frames=std::make_shared<NormalFrames>();frames->origin_vertex_count=uint32_t(part.mesh.positions.size());
      frames->origin_vertices=part.mesh.indices;frames->source_vertex_count=uint32_t(p.mesh.positions.size());
      frames->source_corner_count=uint32_t(p.mesh.indices.size());frames->source_vertices=p.mesh.indices;
      frames->source_corners.resize(p.mesh.indices.size());std::iota(frames->source_corners.begin(),frames->source_corners.end(),0u);
      frames->generated_tangents=native.generated;if(native.generated)frames->generated_uv=coordinate;
      frames->normals=std::move(native.normals);frames->tangents=std::move(native.base);
      auto all_zero=[&](const std::vector<Vec3> &values){for(size_t n=0;n<values.size();++n){if((n&1023)==0)stop(cancel);auto v=values[n];if(v.x!=0 || v.y!=0 || v.z!=0)return false;}return true;};
      for(size_t target=0;target<p.morph_targets.size();++target) {
        // Preserve genuinely omitted channels when the complete generated native
        // key is proven zero. Generated tangents were still evaluated for every
        // target before this lossless zero-channel compaction.
        if(p.morph_targets[target].normals.empty() && all_zero(native.normal_targets[target]))native.normal_targets[target].clear();
        if((native.generated || p.morph_targets[target].tangents.empty()) && all_zero(native.targets[target]))native.targets[target].clear();
        frames->targets.push_back({target+1,std::move(native.normal_targets[target]),std::move(native.targets[target])});
      }
      frames->validate();return frames;
    };
    if(material.normal_texcoord<0 || material.normal_texcoord>1)fail("original normal UV coordinate is unsupported");
    frame_set->mtoon=mint(0); // Legacy MToon0 MainTex ST changes sampling only.
    frame_set->pbr=(!p.tangents.empty() || material.normal_texcoord==0)?frame_set->mtoon:mint(uint32_t(material.normal_texcoord));
    frame_set->validate();part.normal_frames=std::move(frame_set);
    if (!primary && lower.find("body") != std::string::npos &&
        lower.find("skin") != std::string::npos) {
      output.primary_part = output.parts.size();
      primary = true;
    }
    output.parts.push_back(std::move(part));
  }
  output.notes = {
      "Actual author-licensed CC0 VRoidStudio0.14.0 base geometry, UVs, rig, "
      "morphs, embedded images and material metadata retained.",
      "Only unreferenced accessor vertices are compacted; CreatorAppearance "
      "maps every native vertex back to its authored accessor index.",
      "VRM0 -Z-facing/right+X source is converted by a proper 180-degree Y "
      "basis rotation to native +Z-facing/left+X; no winding or UV flip.",
      "Authored inverse binds are checked against their original normalized "
      "joint hierarchy before native shape fitting; legacy Face skin skeleton "
      "hint is retained as an explicit advisory diagnostic.",
      "Raw MToon/normal/AO/alpha/material and avatar metadata remain retained "
      "independently of preview support; this does not claim complete VRM "
      "export.",
      "Body/style settings are bounded relative modifications; hair controls "
      "keep source hair unchanged; nonzero controls add an original fitted "
      "groom extension."};
  for (const auto &w : a.warnings)
    output.notes.push_back(w);
  creator_detail::append_vroid_groom(output, cancel);
  output.validate();
  stop(cancel);
  return output;
}
} // namespace
CreatedHumanoid create_humanoid(const HumanoidControls &c,
                                const std::function<bool()> &cancel,
                                const std::filesystem::path &assets) {
  c.validate();
  if (c.baseline == "makehuman-hm08")
    return create_makehuman_humanoid(c, cancel);
  return create_vroid(c, cancel, assets);
}
} // namespace dome
