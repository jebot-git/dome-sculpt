// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/import.hpp"
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <numeric>
#include <set>
#include <sstream>
namespace dome {
namespace {
namespace fs = std::filesystem;
void stop(const ImportLimits &o) {
  if (o.cancelled && o.cancelled())
    throw std::runtime_error("Mesh import cancelled");
}
struct Budget {
  size_t used = 0, limit;
  void take(size_t n) {
    if (n > limit - used)
      throw std::length_error("Mesh import resource budget exceeded");
    used += n;
  }
};
struct alignas(std::max_align_t) Allocation {
  size_t size;
};
void *allocate(void *user, size_t n) {
  auto &b = *static_cast<Budget *>(user);
  if (n > b.limit - b.used || n > SIZE_MAX - sizeof(Allocation))
    return nullptr;
  auto *a = static_cast<Allocation *>(std::malloc(sizeof(Allocation) + n));
  if (!a)
    return nullptr;
  a->size = n;
  b.used += n;
  return a + 1;
}
void release(void *user, void *p) {
  if (!p)
    return;
  auto *a = static_cast<Allocation *>(p) - 1;
  static_cast<Budget *>(user)->used -= a->size;
  std::free(a);
}
std::string json_text(const char *value, size_t maximum,
                      Budget *budget = nullptr) {
  std::string_view raw = value ? value : "";
  if (raw.size() > maximum)
    throw std::length_error("glTF string exceeds resource limit");
  bool escaped = raw.find(char(92)) != std::string_view::npos;
  if (budget) {
    if (raw.size() > SIZE_MAX / (escaped ? 6 : 1))
      throw std::length_error("glTF string resource overflow");
    budget->take(raw.size() * (escaped ? 6 : 1));
  }
  auto decoded = (!escaped && budget)
                     ? std::string(raw)
                     : nlohmann::json::parse("\"" + std::string(raw) + "\"")
                           .get<std::string>();
  if (decoded.find('\0') != std::string::npos)
    throw std::runtime_error("NUL in glTF text value");
  return decoded;
}
std::string label(const char *n, const char *fallback) {
  return json_text(n ? n : fallback, 1024);
}
std::string owned_label(const char *n, const char *fallback, Budget &b) {
  auto value = label(n, fallback);
  b.take(value.size());
  return value;
}
std::vector<uint8_t> read_file(const fs::path &p, size_t limit, Budget &b) {
  std::ifstream f(p, std::ios::binary | std::ios::ate);
  if (!f)
    throw std::runtime_error("Cannot read import resource: " + p.string());
  auto n = f.tellg();
  if (n < 0 || uint64_t(n) > limit)
    throw std::length_error("Import file exceeds byte limit");
  b.take(size_t(n));
  std::vector<uint8_t> bytes(size_t(n), uint8_t{});
  f.seekg(0);
  f.read(reinterpret_cast<char *>(bytes.data()), n);
  if (!f)
    throw std::runtime_error("Truncated import resource");
  return bytes;
}
int hex(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}
std::string uri_decode(std::string_view s) {
  std::string out;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '%') {
      if (i + 2 >= s.size() || hex(s[i + 1]) < 0 || hex(s[i + 2]) < 0)
        throw std::runtime_error("Invalid resource URI escape");
      out += char(hex(s[i + 1]) * 16 + hex(s[i + 2]));
      i += 2;
    } else
      out += s[i];
  }
  if (out.find('\0') != std::string::npos ||
      out.find('\\') != std::string::npos ||
      out.find(':') != std::string::npos ||
      out.find('?') != std::string::npos || out.find('#') != std::string::npos)
    throw std::runtime_error("Unsupported external resource URI");
  return out;
}
fs::path resource(const fs::path &root, std::string_view uri,
                  const ImportLimits &o) {
  if (!o.external_resources)
    throw std::runtime_error("External import resources are disabled");
  auto name = uri_decode(uri);
  fs::path path(name);
  if (path.is_absolute())
    throw std::runtime_error("Absolute import resource path rejected");
  auto resolved = fs::weakly_canonical(root / path);
  auto a = root.begin(), e = root.end(), p = resolved.begin();
  for (; a != e; ++a, ++p)
    if (p == resolved.end() || *a != *p)
      throw std::runtime_error("Import resource escapes its asset directory");
  if (!fs::is_regular_file(resolved))
    throw std::runtime_error("Import resource is not a regular file");
  return resolved;
}
std::vector<uint8_t> base64(std::string_view text, Budget &b) {
  if (text.size() % 4)
    throw std::runtime_error("Invalid base64 resource length");
  size_t n = text.size() / 4 * 3;
  if (!text.empty() && text.back() == '=')
    n--;
  if (text.size() > 1 && text[text.size() - 2] == '=')
    n--;
  b.take(n);
  std::vector<uint8_t> out;
  out.reserve(n);
  auto digit = [](char c) {
    if (c >= 'A' && c <= 'Z')
      return c - 'A';
    if (c >= 'a' && c <= 'z')
      return c - 'a' + 26;
    if (c >= '0' && c <= '9')
      return c - '0' + 52;
    if (c == '+')
      return 62;
    if (c == '/')
      return 63;
    return -1;
  };
  for (size_t i = 0; i < text.size(); i += 4) {
    uint32_t word = 0;
    int padding = 0;
    for (int k = 0; k < 4; k++) {
      char c = text[i + k];
      if (c == '=') {
        if (k < 2 || i + 4 != text.size())
          throw std::runtime_error("Invalid base64 padding");
        padding++;
        word <<= 6;
      } else {
        if (padding || digit(c) < 0)
          throw std::runtime_error("Invalid base64 resource");
        word = (word << 6) | digit(c);
      }
    }
    out.push_back(word >> 16);
    if (padding < 2)
      out.push_back(word >> 8);
    if (!padding)
      out.push_back(word);
  }
  if (out.size() != n)
    throw std::runtime_error("Invalid decoded resource length");
  return out;
}
std::vector<uint8_t> data_uri(std::string_view uri, std::string &mime,
                              Budget &b) {
  auto comma = uri.find(',');
  if (!uri.starts_with("data:") || comma == uri.npos || comma < 12 ||
      uri.substr(comma - 7, 7) != ";base64")
    throw std::runtime_error("Only base64 data URIs are supported");
  mime = uri.substr(5, comma - 12);
  return base64(uri.substr(comma + 1), b);
}
std::string mime_for(const fs::path &p) {
  auto s = p.extension().string();
  for (auto &c : s)
    c = char(std::tolower(static_cast<unsigned char>(c)));
  return s == ".png"                   ? "image/png"
         : s == ".jpg" || s == ".jpeg" ? "image/jpeg"
         : s == ".webp"                ? "image/webp"
                                       : "application/octet-stream";
}
void unit(float v) {
  if (!std::isfinite(v) || v < 0 || v > 1)
    throw std::runtime_error("Imported material value outside0..1");
}
void generate_normals(Mesh &m, bool display_fallback = false) {
  std::vector<Vec3> fallback;
  if (display_fallback)
    fallback.assign(m.positions.size(), {0, 1, 0});
  m.normals.assign(m.positions.size(), {});
  for (size_t t = 0; t < m.indices.size(); t += 3) {
    auto a = m.indices[t], b = m.indices[t + 1], c = m.indices[t + 2];
    auto n =
        cross(m.positions[b] - m.positions[a], m.positions[c] - m.positions[a]);
    if (display_fallback)
      for (auto i : {a, b, c})
        if (length(m.normals[i]) < 1e-12f)
          fallback[i] = n;
    m.normals[a] += n;
    m.normals[b] += n;
    m.normals[c] += n;
  }
  for (size_t i = 0; i < m.normals.size(); i++) {
    auto &n = m.normals[i];
    if (length(n) < 1e-12f) {
      if (display_fallback)
        n = fallback[i];
      else
        throw std::runtime_error("Cannot derive a surface normal");
    }
    n = normalized(n);
  }
}
void range(size_t offset, size_t count, size_t stride, size_t element,
           size_t size) {
  if (!count || !element || stride < element || offset > size ||
      element > size - offset ||
      (count - 1) > (size - offset - element) / stride)
    throw std::runtime_error("Imported accessor exceeds buffer bounds");
}
void accessor_bounds(const cgltf_accessor &a) {
  size_t element = cgltf_calc_size(a.type, a.component_type);
  if (a.type == cgltf_type_invalid || !element || !a.count)
    throw std::runtime_error("Empty or invalid glTF accessor");
  size_t component_size = cgltf_component_size(a.component_type);
  if (!component_size || a.offset % component_size ||
      a.stride % component_size ||
      (a.buffer_view && a.buffer_view->offset % component_size))
    throw std::runtime_error("Misaligned glTF accessor");
  if (a.buffer_view)
    range(a.offset, a.count, a.stride, element, a.buffer_view->size);
  else if (!a.is_sparse)
    throw std::runtime_error("glTF accessor has no data");
  if (a.is_sparse) {
    auto &s = a.sparse;
    if (!s.count || s.count > a.count || !s.indices_buffer_view ||
        !s.values_buffer_view)
      throw std::runtime_error("Invalid sparse glTF accessor");
    size_t component = cgltf_component_size(s.indices_component_type);
    if (s.indices_component_type != cgltf_component_type_r_8u &&
        s.indices_component_type != cgltf_component_type_r_16u &&
        s.indices_component_type != cgltf_component_type_r_32u)
      throw std::runtime_error("Invalid sparse index component");
    range(s.indices_byte_offset, s.count, component, component,
          s.indices_buffer_view->size);
    range(s.values_byte_offset, s.count, element, element,
          s.values_buffer_view->size);
    if (s.indices_byte_offset % component ||
        s.indices_buffer_view->offset % component ||
        s.values_byte_offset % component_size ||
        s.values_buffer_view->offset % component_size)
      throw std::runtime_error("Misaligned sparse accessor");
    auto *p =
        cgltf_buffer_view_data(s.indices_buffer_view) + s.indices_byte_offset;
    uint64_t previous = 0;
    for (size_t i = 0; i < s.count; i++) {
      uint64_t index = 0;
      for (size_t k = 0; k < component; k++)
        index |= uint64_t(p[i * component + k]) << (k * 8);
      if (index >= a.count || (i && index <= previous))
        throw std::runtime_error(
            "Sparse accessor indices must be ordered, distinct and in range");
      previous = index;
    }
  }
}
std::vector<float> floats(const cgltf_accessor *a, cgltf_type type,
                          size_t count, Budget &b) {
  if (!a || a->type != type || a->count != count)
    throw std::runtime_error("glTF attribute type or count mismatch");
  size_t dim = cgltf_num_components(type);
  if (count > SIZE_MAX / dim / sizeof(float))
    throw std::length_error("glTF attribute size overflow");
  b.take(count * dim * sizeof(float));
  std::vector<float> values(count * dim);
  if (cgltf_accessor_unpack_floats(a, values.data(), values.size()) !=
      values.size())
    throw std::runtime_error("Cannot decode glTF accessor");
  for (auto f : values)
    if (!std::isfinite(f))
      throw std::runtime_error("Nonfinite glTF attribute");
  return values;
}
ImportedMatrix identity_matrix() {
  return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}
std::string extras(const cgltf_extras &e, Budget &b, const cgltf_data &d) {
  if (e.start_offset == 0 && e.end_offset == 0)
    return {};
  size_t begin = e.start_offset, end = e.end_offset;
  if (end < begin || end > d.json_size)
    throw std::runtime_error("Invalid glTF extras bounds");
  // jsmn string offsets exclude their quotes; preserve any JSON value exactly.
  if (begin > 0 && end < d.json_size && d.json[begin - 1] == '"' &&
      d.json[end] == '"') {
    --begin;
    ++end;
  }
  b.take(end - begin);
  return std::string(d.json + begin, end - begin);
}
std::vector<float> authored_weights(const float *values, size_t count,
                                    Budget &b) {
  b.take(count * sizeof(float));
  std::vector<float> out;
  for (size_t i = 0; i < count; i++) {
    if (!std::isfinite(values[i]))
      throw std::runtime_error("Nonfinite authored morph weight");
    out.push_back(values[i]);
  }
  return out;
}
size_t morph_count(const cgltf_mesh &m) {
  size_t n = 0;
  for (size_t i = 0; i < m.primitives_count; i++)
    n = std::max(n, m.primitives[i].targets_count);
  return n;
}
void metadata(const cgltf_data &d, ImportedAsset &out, Budget &b,
              const ImportLimits &o) {
  size_t work = 0;
  auto visit = [&] {
    if (++work > o.max_metadata_visits)
      throw std::length_error("glTF hierarchy metadata work budget exceeded");
    if (work % 256 == 0)
      stop(o);
  };
  out.mode = o.mode;
  out.extras_json = extras(d.extras, b, d);
  out.default_scene = d.scene ? int(d.scene - d.scenes) : -1;
  b.take(d.nodes_count * sizeof(ImportedNode) +
         d.meshes_count * sizeof(ImportedMesh) +
         d.skins_count * sizeof(ImportedSkin) +
         d.scenes_count * sizeof(ImportedScene));
  std::vector<uint32_t> roots(d.nodes_count);
  for (size_t i = 0; i < d.nodes_count; i++) {
    visit();
    auto &n = d.nodes[i];
    if (n.has_matrix && (n.has_translation || n.has_rotation || n.has_scale))
      throw std::runtime_error("Node cannot author both matrix and TRS");
    if (n.has_mesh_gpu_instancing)
      throw std::runtime_error(
          "GPU instancing extension needs a dedicated import path");
    ImportedNode node;
    node.name = owned_label(n.name, "Node", b);
    node.extras_json = extras(n.extras, b, d);
    node.parent = n.parent ? int(n.parent - d.nodes) : -1;
    node.mesh = n.mesh ? int(n.mesh - d.meshes) : -1;
    node.skin = n.skin ? int(n.skin - d.skins) : -1;
    node.camera = n.camera ? int(n.camera - d.cameras) : -1;
    node.light = n.light ? int(n.light - d.lights) : -1;
    if (n.camera || n.light)
      out.warnings.push_back("Camera/light payload is not imported for node " +
                             std::to_string(i) + "; source reference retained");
    node.has_matrix = n.has_matrix;
    node.has_translation = n.has_translation;
    node.has_rotation = n.has_rotation;
    node.has_scale = n.has_scale;
    std::copy_n(n.translation, 3, node.translation.begin());
    std::copy_n(n.rotation, 4, node.rotation.begin());
    std::copy_n(n.scale, 3, node.scale.begin());
    for (float f : node.translation)
      if (!std::isfinite(f))
        throw std::runtime_error("Nonfinite node translation");
    for (float f : node.scale)
      if (!std::isfinite(f))
        throw std::runtime_error("Nonfinite node scale");
    double q = 0;
    for (float f : node.rotation) {
      if (!std::isfinite(f))
        throw std::runtime_error("Nonfinite node quaternion");
      q += double(f) * f;
    }
    if (std::abs(q - 1) > 1e-4)
      throw std::runtime_error(
          "Node rotation quaternion must have unit length");
    cgltf_node_transform_local(&n, node.local.data());
    cgltf_node_transform_world(&n, node.world.data());
    for (auto matrix : {node.local, node.world}) {
      for (float f : matrix)
        if (!std::isfinite(f))
          throw std::runtime_error("Nonfinite node matrix");
      if (std::abs(matrix[3]) + std::abs(matrix[7]) + std::abs(matrix[11]) +
              std::abs(matrix[15] - 1) >
          1e-6f)
        throw std::runtime_error(
            "Projective node matrix is not a glTF TRS transform");
    }
    b.take(n.children_count * sizeof(uint32_t));
    for (size_t j = 0; j < n.children_count; j++) {
      visit();
      node.children.push_back(uint32_t(n.children[j] - d.nodes));
    }
    node.weights = authored_weights(n.weights, n.weights_count, b);
    if (n.weights_count && (!n.mesh || n.weights_count != morph_count(*n.mesh)))
      throw std::runtime_error("Node morph default count mismatch");
    auto *p = &n;
    while (p->parent) {
      visit();
      p = p->parent;
    }
    roots[i] = uint32_t(p - d.nodes);
    out.nodes.push_back(std::move(node));
  }
  for (size_t i = 0; i < d.meshes_count; i++) {
    visit();
    auto &m = d.meshes[i];
    if (!m.primitives_count)
      throw std::runtime_error("glTF mesh has no primitives");
    ImportedMesh mesh;
    mesh.name = owned_label(m.name, "Mesh", b);
    mesh.extras_json = extras(m.extras, b, d);
    auto n = morph_count(m);
    if ((m.weights_count && m.weights_count != n) ||
        (m.target_names_count && m.target_names_count != n))
      throw std::runtime_error("Mesh morph defaults/names count mismatch");
    mesh.weights = authored_weights(m.weights, m.weights_count, b);
    for (size_t t = 0; t < m.target_names_count; t++) {
      visit();
      auto name = owned_label(m.target_names[t], "", b);
      b.take(name.size());
      mesh.target_names.push_back(std::move(name));
    }
    out.meshes.push_back(std::move(mesh));
  }
  for (size_t i = 0; i < d.skins_count; i++) {
    visit();
    auto &s = d.skins[i];
    if (!s.joints_count || s.joints_count > d.nodes_count)
      throw std::runtime_error("Invalid skin joint count");
    ImportedSkin skin;
    skin.name = owned_label(s.name, "Skin", b);
    skin.extras_json = extras(s.extras, b, d);
    skin.skeleton = s.skeleton ? int(s.skeleton - d.nodes) : -1;
    b.take(s.joints_count * sizeof(uint32_t));
    std::set<uint32_t> distinct;
    uint32_t root = roots[s.joints[0] - d.nodes];
    for (size_t j = 0; j < s.joints_count; j++) {
      visit();
      auto id = uint32_t(s.joints[j] - d.nodes);
      if (!distinct.insert(id).second || roots[id] != root)
        throw std::runtime_error(
            "Skin joints must be unique with a common hierarchy root");
      skin.joints.push_back(id);
      if (s.skeleton) {
        auto *p = s.joints[j];
        while (p && p != s.skeleton) {
          visit();
          p = p->parent;
        }
        if (!p)
          throw std::runtime_error("Skin skeleton is not a joint ancestor");
      }
    }
    if (s.inverse_bind_matrices) {
      auto *a = s.inverse_bind_matrices;
      if (a->component_type != cgltf_component_type_r_32f || a->normalized ||
          a->count < s.joints_count)
        throw std::runtime_error("Invalid inverse bind matrix accessor");
      auto values = floats(a, cgltf_type_mat4, a->count, b);
      b.take(a->count * sizeof(ImportedMatrix));
      skin.inverse_bind_matrices_authored = true;
      for (size_t j = 0; j < a->count; j++) {
        visit();
        ImportedMatrix matrix;
        std::copy_n(values.data() + j * 16, 16, matrix.begin());
        if (j < s.joints_count && (matrix[3] != 0 || matrix[7] != 0 ||
                                   matrix[11] != 0 || matrix[15] != 1))
          throw std::runtime_error("Joint inverse bind matrix must be affine");
        skin.inverse_bind_matrices.push_back(matrix);
      }
    }
    out.skins.push_back(std::move(skin));
  }
  std::map<uint32_t, std::set<uint32_t>> scenes_for_root;
  for (size_t i = 0; i < d.scenes_count; i++) {
    visit();
    auto &s = d.scenes[i];
    ImportedScene scene;
    scene.name = owned_label(s.name, "Scene", b);
    scene.extras_json = extras(s.extras, b, d);
    b.take(s.nodes_count * sizeof(uint32_t));
    std::set<uint32_t> distinct;
    for (size_t j = 0; j < s.nodes_count; j++) {
      visit();
      auto root = uint32_t(s.nodes[j] - d.nodes);
      if (!distinct.insert(root).second)
        throw std::runtime_error("Duplicate scene root");
      scene.roots.push_back(root);
      scenes_for_root[root].insert(uint32_t(i));
    }
    out.scenes.push_back(std::move(scene));
  }
  std::set<std::pair<uint32_t, uint32_t>> checked;
  for (size_t i = 0; i < d.nodes_count; i++)
    if (d.nodes[i].skin) {
      visit();
      auto skin = uint32_t(d.nodes[i].skin - d.skins);
      if (!d.nodes[i].mesh)
        throw std::runtime_error("Skinned node has no mesh");
      auto jointroot = roots[out.skins[skin].joints[0]];
      for (auto scene : scenes_for_root[roots[i]]) {
        visit();
        if (checked.insert({skin, scene}).second) {
          if (!scenes_for_root[jointroot].contains(scene))
            throw std::runtime_error(
                "Skin common root must belong to the mesh instance scene");
        }
      }
    }
  stop(o);
}
int component_number(cgltf_component_type type) {
  switch (type) {
  case cgltf_component_type_r_8u:
    return 5121;
  case cgltf_component_type_r_16u:
    return 5123;
  case cgltf_component_type_r_32f:
    return 5126;
  default:
    return 0;
  }
}
void deformation(const cgltf_primitive &p, const cgltf_mesh &mesh,
                 ImportedPrimitive &prim, Budget &b, const ImportLimits &o,
                 ImportedAsset &out) {
  size_t count = prim.mesh.positions.size();
  std::map<uint32_t, std::pair<const cgltf_accessor *, const cgltf_accessor *>>
      sets;
  for (size_t i = 0; i < p.attributes_count; i++) {
    auto &a = p.attributes[i];
    if (a.type != cgltf_attribute_type_joints &&
        a.type != cgltf_attribute_type_weights)
      continue;
    std::string prefix =
        a.type == cgltf_attribute_type_joints ? "JOINTS_" : "WEIGHTS_";
    if (a.index < 0 || a.index > 999999999 || !a.name ||
        a.name != prefix + std::to_string(a.index))
      throw std::runtime_error("Invalid influence set semantic index");
    auto &set = sets[uint32_t(a.index)];
    auto &slot = a.type == cgltf_attribute_type_joints ? set.first : set.second;
    if (slot)
      throw std::runtime_error("Duplicate influence attribute");
    slot = a.data;
  }
  uint32_t expected = 0;
  for (auto &[id, pair] : sets) {
    stop(o);
    auto [j, w] = pair;
    if (id != expected++ || !j || !w)
      throw std::runtime_error(
          "Joint and weight sets must be paired and consecutive");
    if ((j->component_type != cgltf_component_type_r_8u &&
         j->component_type != cgltf_component_type_r_16u) ||
        j->normalized)
      throw std::runtime_error(
          "Joint indices must be non-normalized unsigned byte/short VEC4");
    if ((w->component_type != cgltf_component_type_r_32f &&
         w->component_type != cgltf_component_type_r_8u &&
         w->component_type != cgltf_component_type_r_16u) ||
        (w->component_type == cgltf_component_type_r_32f ? w->normalized
                                                         : !w->normalized))
      throw std::runtime_error(
          "Weights must be float or normalized unsigned byte/short VEC4");
    auto joints = floats(j, cgltf_type_vec4, count, b),
         weights = floats(w, cgltf_type_vec4, count, b);
    b.take(count *
           (sizeof(std::array<uint32_t, 4>) + sizeof(std::array<float, 4>)));
    ImportedInfluenceSet set;
    set.set = id;
    set.joint_component_type = component_number(j->component_type);
    set.weight_component_type = component_number(w->component_type);
    set.weights_normalized = w->normalized;
    for (size_t v = 0; v < count; v++) {
      if (v % 512 == 0)
        stop(o);
      std::array<uint32_t, 4> joint;
      std::array<float, 4> weight;
      for (size_t k = 0; k < 4; k++) {
        joint[k] = uint32_t(joints[v * 4 + k]);
        weight[k] = weights[v * 4 + k];
        if (weight[k] < 0)
          throw std::runtime_error("Negative skin weight");
      }
      set.joints.push_back(joint);
      set.weights.push_back(weight);
    }
    prim.influences.push_back(std::move(set));
  }
  bool warned = false;
  for (size_t v = 0; v < count && !sets.empty(); v++) {
    if (v % 256 == 0)
      stop(o);
    std::vector<uint32_t> active;
    double sum = 0;
    uint64_t integer_sum = 0;
    bool integer = true;
    size_t nonzero = 0;
    for (auto &set : prim.influences)
      for (size_t k = 0; k < 4; k++) {
        auto weight = set.weights[v][k];
        sum += weight;
        if (weight > 0) {
          active.push_back(set.joints[v][k]);
          ++nonzero;
        }
        if (set.weight_component_type == 5126)
          integer = false;
        else
          integer_sum +=
              uint64_t(std::llround(
                  double(weight) *
                  (set.weight_component_type == 5121 ? 255 : 65535))) *
              (set.weight_component_type == 5121 ? 257 : 1);
      }
    std::sort(active.begin(), active.end());
    if (std::adjacent_find(active.begin(), active.end()) != active.end())
      throw std::runtime_error(
          "Nonzero influences must use distinct joint indices");
    if (integer && integer_sum != 65535)
      throw std::runtime_error(
          "Normalized integer influence weights must sum exactly to one");
    if (!integer && std::abs(sum - 1) > 2e-7 * std::max(size_t(1), nonzero) &&
        !warned) {
      out.warnings.push_back("Authored floating skin weights do not sum to "
                             "one; values are preserved without normalization");
      warned = true;
    }
  }
  auto targets = morph_count(mesh);
  b.take(targets * sizeof(ImportedMorphTarget));
  prim.morph_targets.resize(targets);
  prim.morph_targets_authored = p.targets_count != 0;
  if (p.targets_count && p.targets_count != targets)
    throw std::runtime_error("Inconsistent explicit morph target counts");
  for (size_t t = 0; t < p.targets_count; t++) {
    stop(o);
    std::set<cgltf_attribute_type> channels;
    for (size_t a = 0; a < p.targets[t].attributes_count; a++) {
      auto &attr = p.targets[t].attributes[a];
      if (attr.index != 0 || !channels.insert(attr.type).second)
        throw std::runtime_error(
            "Duplicate/unsupported morph target channel index");
      std::vector<Vec3> *dest = nullptr;
      if (attr.type == cgltf_attribute_type_position)
        dest = &prim.morph_targets[t].positions;
      else if (attr.type == cgltf_attribute_type_normal)
        dest = &prim.morph_targets[t].normals;
      else if (attr.type == cgltf_attribute_type_tangent)
        dest = &prim.morph_targets[t].tangents;
      else
        throw std::runtime_error(
            "Morph channel requires an explicit supported import path");
      if (!cgltf_find_accessor(&p, attr.type, 0))
        throw std::runtime_error("Morph target channel has no base attribute");
      if (attr.data->component_type != cgltf_component_type_r_32f ||
          attr.data->normalized)
        throw std::runtime_error("Core morph differences must use float VEC3");
      auto values = floats(attr.data, cgltf_type_vec3, count, b);
      b.take(count * sizeof(Vec3));
      for (size_t v = 0; v < count; v++) {
        if (v % 512 == 0)
          stop(o);
        dest->push_back({values[v * 3], values[v * 3 + 1], values[v * 3 + 2]});
      }
    }
  }
}
void skin_mappings(const cgltf_data &d, ImportedAsset &out,
                   const ImportLimits &o) {
  size_t work = 0;
  std::set<std::pair<int, int>> checked;
  std::set<int> bound;
  for (auto &node : out.nodes)
    if (node.skin >= 0) {
      bound.insert(node.mesh);
      if (!checked.insert({node.mesh, node.skin}).second)
        continue;
      auto &skin = out.skins[node.skin];
      for (auto id : out.meshes[node.mesh].primitives) {
        auto &p = out.primitives[id];
        if (p.influences.empty())
          throw std::runtime_error(
              "Skinned mesh primitive has no joint/weight sets");
        for (size_t v = 0; v < p.mesh.positions.size(); v++) {
          if (v % 256 == 0)
            stop(o);
          double sum = 0;
          for (auto &set : p.influences)
            for (size_t k = 0; k < 4; k++) {
              if (++work > o.max_metadata_visits)
                throw std::length_error(
                    "Skin instance mapping work budget exceeded");
              if (set.joints[v][k] >= skin.joints.size())
                throw std::runtime_error("Influence joint slot exceeds an "
                                         "instancing skin's joint map");
              sum += set.weights[v][k];
            }
          if (sum <= 0)
            throw std::runtime_error("Skinned vertex has no nonzero influence");
        }
      }
    }
  for (size_t mesh = 0; mesh < out.meshes.size(); mesh++)
    if (!bound.contains(int(mesh)))
      for (auto id : out.meshes[mesh].primitives)
        if (!out.primitives[id].influences.empty())
          out.warnings.push_back("Joint/weight data retained for mesh " +
                                 std::to_string(mesh) +
                                 " without any bound skin instance");
  (void)d;
  stop(o);
}
ImportedAsset gltf(const fs::path &path, const ImportLimits &o, Budget &b) {
  auto input = read_file(path, o.max_file_bytes, b);
  stop(o);
  cgltf_options options{};
  options.memory = {allocate, release, &b};
  cgltf_data *raw = nullptr;
  if (cgltf_parse(&options, input.data(), input.size(), &raw) !=
      cgltf_result_success)
    throw std::runtime_error("Cannot parse glTF/GLB");
  std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data(raw, cgltf_free);
  auto &d = *data;
  if (d.extensions_required_count)
    throw std::runtime_error(
        "Required glTF extensions need an explicit supported import path");
  if (d.nodes_count > 100000 || d.meshes_count > 65536 ||
      d.skins_count > 65536 || d.scenes_count > 65536 ||
      d.animations_count > 65536 || d.materials_count > 65536 ||
      d.images_count > 65536 || d.textures_count > 65536)
    throw std::length_error("glTF scene/resource count exceeds limit");
  std::vector<std::vector<uint8_t>> buffers(d.buffers_count);
  for (size_t i = 0; i < d.buffers_count; i++) {
    stop(o);
    auto &buf = d.buffers[i];
    if (!buf.size || buf.size > o.max_resource_bytes)
      throw std::length_error("Invalid glTF buffer size");
    if (buf.uri) {
      auto uri = json_text(buf.uri, o.max_file_bytes, &b);
      if (std::string_view(uri).starts_with("data:")) {
        std::string ignored;
        buffers[i] = data_uri(uri, ignored, b);
      } else
        buffers[i] = read_file(resource(path.parent_path(), uri, o),
                               o.max_resource_bytes, b);
      if (buffers[i].size() < buf.size)
        throw std::runtime_error("Truncated glTF buffer");
      buf.data = buffers[i].data();
    } else {
      if (i || !d.bin || d.bin_size < buf.size)
        throw std::runtime_error("Missing GLB binary buffer");
      buf.data = const_cast<void *>(d.bin);
    }
    buf.data_free_method = cgltf_data_free_method_none;
  }
  for (size_t i = 0; i < d.buffer_views_count; i++) {
    auto &v = d.buffer_views[i];
    if (!v.buffer || !v.buffer->data || v.offset > v.buffer->size ||
        v.size > v.buffer->size - v.offset || v.has_meshopt_compression)
      throw std::runtime_error("Invalid or compressed glTF buffer view");
  }
  for (size_t i = 0; i < d.accessors_count; i++) {
    if (i % 256 == 0)
      stop(o);
    accessor_bounds(d.accessors[i]);
  }
  // Bound depth/work before cgltf validation performs parent-chain walks.
  size_t hierarchy_work = 0;
  for (size_t i = 0; i < d.nodes_count; i++) {
    if (i % 256 == 0)
      stop(o);
    auto *p = &d.nodes[i];
    size_t depth = 0;
    while (p) {
      if (++hierarchy_work > o.max_metadata_visits)
        throw std::length_error("glTF hierarchy work budget exceeded");
      if (++depth > 256)
        throw std::runtime_error("glTF node hierarchy is too deep");
      p = p->parent;
    }
  }
  // cgltf1.15 predates the clarified allowance for primitives with implicit
  // zero targets alongside explicitly morphed primitives. Normalize only the
  // temporary validator view, then restore authored presence before import.
  struct ZeroTargets {
    cgltf_primitive *primitive;
    std::vector<cgltf_morph_target> zeros;
  };
  std::vector<ZeroTargets> implicit;
  if (o.mode == ImportMode::PreserveGltf)
    for (size_t i = 0; i < d.meshes_count; i++) {
      auto &m = d.meshes[i];
      size_t targets = morph_count(m);
      for (size_t j = 0; j < m.primitives_count; j++)
        if (targets && !m.primitives[j].targets_count) {
          b.take(targets * sizeof(cgltf_morph_target));
          implicit.push_back(
              {&m.primitives[j], std::vector<cgltf_morph_target>(targets)});
        }
    }
  for (auto &p : implicit) {
    p.primitive->targets = p.zeros.data();
    p.primitive->targets_count = p.zeros.size();
  }
  auto validated = cgltf_validate(&d);
  for (auto &p : implicit) {
    p.primitive->targets = nullptr;
    p.primitive->targets_count = 0;
  }
  if (validated != cgltf_result_success)
    throw std::runtime_error("Invalid glTF data");
  ImportedAsset out;
  out.mode = o.mode;
  if (o.mode == ImportMode::PreserveGltf)
    metadata(d, out, b, o);
  for (size_t i = 0; i < d.animations_count; i++)
    out.omitted_animations.push_back(
        {owned_label(d.animations[i].name, "Animation", b),
         d.animations[i].channels_count, d.animations[i].samplers_count});
  if (d.animations_count)
    out.warnings.push_back("Animation tracks are not imported; the static node "
                           "transforms are retained");
  for (size_t i = 0; i < d.extensions_used_count; i++) {
    out.extensions_used.push_back(
        owned_label(d.extensions_used[i], "Extension", b));
    out.warnings.push_back("Optional extension is not interpreted: " +
                           out.extensions_used.back());
  }
  out.materials.push_back({});
  for (size_t i = 0; i < d.images_count; i++) {
    stop(o);
    auto &image = d.images[i];
    ImportedImage im;
    im.name = owned_label(image.name, "Image", b);
    im.mime_type = owned_label(image.mime_type, "", b);
    if (image.uri) {
      im.source_uri = json_text(image.uri, o.max_file_bytes, &b);
      if (std::string_view(im.source_uri).starts_with("data:")) {
        std::string mime;
        im.bytes = data_uri(im.source_uri, mime, b);
        if (im.mime_type.empty())
          im.mime_type = mime;
      } else {
        auto source = resource(path.parent_path(), im.source_uri, o);
        im.bytes = read_file(source, o.max_resource_bytes, b);
        if (im.mime_type.empty())
          im.mime_type = mime_for(source);
      }
    } else if (image.buffer_view) {
      auto &v = *image.buffer_view;
      b.take(v.size);
      auto *bytes = cgltf_buffer_view_data(&v);
      im.bytes.assign(bytes, bytes + v.size);
      im.source_uri =
          "bufferView:" + std::to_string(image.buffer_view - d.buffer_views);
    } else
      throw std::runtime_error("Image has no encoded data");
    out.images.push_back(std::move(im));
  }
  for (size_t i = 0; i < d.textures_count; i++) {
    auto &t = d.textures[i];
    if (!t.image || t.has_basisu || t.has_webp)
      throw std::runtime_error(
          "Unsupported compressed/alternative glTF texture source");
    ImportedTexture tex;
    tex.image = int(t.image - d.images);
    if (t.sampler) {
      tex.wrap_s = t.sampler->wrap_s;
      tex.wrap_t = t.sampler->wrap_t;
      tex.min_filter = t.sampler->min_filter;
      tex.mag_filter = t.sampler->mag_filter;
    }
    out.textures.push_back(tex);
  }
  auto texture = [&](const cgltf_texture_view &v) {
    if (!v.texture)
      return -1;
    if (v.texcoord < 0 || v.texcoord > 1 || v.has_transform)
      throw std::runtime_error("Unsupported texture coordinate set/transform");
    return int(v.texture - d.textures);
  };
  for (size_t i = 0; i < d.materials_count; i++) {
    auto &m = d.materials[i];
    if (m.has_pbr_specular_glossiness || m.has_clearcoat ||
        m.has_transmission || m.has_volume || m.has_ior || m.has_specular ||
        m.has_sheen || m.has_emissive_strength || m.has_iridescence ||
        m.has_diffuse_transmission || m.has_anisotropy || m.has_dispersion ||
        m.unlit)
      throw std::runtime_error(
          "This material requires a non-core glTF shader import path");
    ImportedMaterial material;
    material.name = owned_label(m.name, "Material", b);
    auto &pbr = m.pbr_metallic_roughness;
    std::copy_n(pbr.base_color_factor, 4, material.color.begin());
    std::copy_n(m.emissive_factor, 3, material.emissive.begin());
    material.metallic = pbr.metallic_factor;
    material.roughness = pbr.roughness_factor;
    for (auto f : material.color)
      unit(f);
    for (auto f : material.emissive)
      unit(f);
    unit(material.metallic);
    unit(material.roughness);
    material.double_sided = m.double_sided;
    material.alpha = m.alpha_mode == cgltf_alpha_mode_blend
                         ? ImportedMaterial::AlphaMode::Blend
                     : m.alpha_mode == cgltf_alpha_mode_mask
                         ? ImportedMaterial::AlphaMode::Mask
                         : ImportedMaterial::AlphaMode::Opaque;
    material.alpha_cutoff = m.alpha_cutoff;
    unit(material.alpha_cutoff);
    material.color_texture = texture(pbr.base_color_texture);
    material.metal_rough_texture = texture(pbr.metallic_roughness_texture);
    material.normal_texture = texture(m.normal_texture);
    material.occlusion_texture = texture(m.occlusion_texture);
    material.emissive_texture = texture(m.emissive_texture);
    material.color_texcoord = pbr.base_color_texture.texcoord;
    material.metal_rough_texcoord = pbr.metallic_roughness_texture.texcoord;
    material.normal_texcoord = m.normal_texture.texcoord;
    material.occlusion_texcoord = m.occlusion_texture.texcoord;
    material.emissive_texcoord = m.emissive_texture.texcoord;
    material.normal_scale =
        m.normal_texture.texture ? m.normal_texture.scale : 1;
    material.occlusion_strength =
        m.occlusion_texture.texture ? m.occlusion_texture.scale : 1;
    if (!std::isfinite(material.normal_scale) || material.normal_scale < 0)
      throw std::runtime_error("Invalid normal texture scale");
    unit(material.occlusion_strength);
    out.materials.push_back(material);
  }
  std::vector<cgltf_node *> nodes;
  std::set<cgltf_node *> visited;
  auto visit = [&](auto &&self, cgltf_node *n) -> void {
    if (!visited.insert(n).second)
      return;
    nodes.push_back(n);
    for (size_t i = 0; i < n->children_count; i++)
      self(self, n->children[i]);
  };
  if (d.scene) {
    for (size_t i = 0; i < d.scene->nodes_count; i++)
      visit(visit, d.scene->nodes[i]);
  } else if (d.scenes_count) {
    for (size_t i = 0; i < d.scenes[0].nodes_count; i++)
      visit(visit, d.scenes[0].nodes[i]);
  } else
    for (size_t i = 0; i < d.nodes_count; i++)
      if (!d.nodes[i].parent)
        visit(visit, &d.nodes[i]);
  struct MeshUse {
    cgltf_mesh *mesh;
    cgltf_node *node;
  };
  std::vector<MeshUse> uses;
  const bool preserve = o.mode == ImportMode::PreserveGltf;
  if (preserve)
    for (size_t i = 0; i < d.meshes_count; i++)
      uses.push_back({&d.meshes[i], nullptr});
  else
    for (auto *n : nodes)
      if (n->mesh)
        uses.push_back({n->mesh, n});
  size_t vertex_count = 0, triangle_count = 0;
  for (auto use : uses) {
    auto *n = use.node;
    auto *mesh = use.mesh;
    stop(o);
    if (!preserve && (n->skin || n->has_mesh_gpu_instancing))
      throw std::runtime_error(
          "Skinned/instanced nodes require a dedicated authoring import path");
    auto world = identity_matrix();
    if (!preserve)
      cgltf_node_transform_world(n, world.data());
    for (float f : world)
      if (!std::isfinite(f))
        throw std::runtime_error("Nonfinite node transform");
    if (std::abs(world[3]) + std::abs(world[7]) + std::abs(world[11]) > 1e-6f ||
        std::abs(world[15] - 1) > 1e-6f)
      throw std::runtime_error("Projective mesh transform unsupported");
    Vec3 col0{world[0], world[1], world[2]}, col1{world[4], world[5], world[6]},
        col2{world[8], world[9], world[10]};
    double det = dot(col0, cross(col1, col2));
    double scale = double(length(col0)) * length(col1) * length(col2);
    if (!std::isfinite(det) || !std::isfinite(scale) || scale <= 0 ||
        std::abs(det) < 1e-10 * scale)
      throw std::runtime_error("Singular mesh transform");
    for (size_t j = 0; j < mesh->primitives_count; j++) {
      auto &p = mesh->primitives[j];
      if (p.type != cgltf_primitive_type_triangles ||
          (!preserve && p.targets_count) || p.has_draco_mesh_compression)
        throw std::runtime_error(
            "Only static uncompressed triangle primitives are supported");
      auto *pa = cgltf_find_accessor(&p, cgltf_attribute_type_position, 0);
      if (!pa || pa->count > o.max_vertices - vertex_count)
        throw std::length_error("Imported vertex budget exceeded");
      size_t count = pa->count;
      vertex_count += count;
      size_t indices = p.indices ? p.indices->count : count;
      if (indices % 3 || indices / 3 > o.max_triangles - triangle_count)
        throw std::length_error("Imported triangle budget exceeded");
      triangle_count += indices / 3;
      b.take(count * (sizeof(Vec3) * 2 + sizeof(std::array<float, 2>)) +
             indices * sizeof(uint32_t));
      if (out.primitives.size() >= 65536)
        throw std::length_error("Too many imported primitives");
      b.take(sizeof(ImportedPrimitive) + 2048);
      ImportedPrimitive prim;
      prim.name =
          label(n ? n->name : mesh->name, mesh->name ? mesh->name : "Mesh") +
          " / " + std::to_string(j);
      prim.mesh_local = preserve;
      prim.source_mesh = int(mesh - d.meshes);
      prim.source_primitive = int(j);
      prim.source_node = n ? int(n - d.nodes) : -1;
      if (preserve)
        prim.extras_json = extras(p.extras, b, d);
      prim.source_world = world;
      prim.material = p.material ? uint32_t(p.material - d.materials) + 1 : 0;
      auto pos = floats(pa, cgltf_type_vec3, count, b);
      auto *na = cgltf_find_accessor(&p, cgltf_attribute_type_normal, 0);
      std::vector<float> normal;
      prim.normals_authored = na != nullptr;
      if (na)
        normal = floats(na, cgltf_type_vec3, count, b);
      for (size_t k = 0; k < count; k++) {
        if (k % 1024 == 0)
          stop(o);
        Vec3 v{pos[k * 3], pos[k * 3 + 1], pos[k * 3 + 2]};
        auto vertex = preserve ? v
                               : col0 * v.x + col1 * v.y + col2 * v.z +
                                     Vec3{world[12], world[13], world[14]};
        if (!finite(vertex) || std::max({std::abs(vertex.x), std::abs(vertex.y),
                                         std::abs(vertex.z)}) > 100)
          throw std::runtime_error(
              "Mesh position outside100m authoring workspace");
        prim.mesh.positions.push_back(vertex);
        if (na) {
          Vec3 vn{normal[k * 3], normal[k * 3 + 1], normal[k * 3 + 2]};
          auto q = (cross(col1, col2) * vn.x + cross(col2, col0) * vn.y +
                    cross(col0, col1) * vn.z) /
                   float(det);
          if (!std::isfinite(length(q)) || length(q) < 1e-12f)
            throw std::runtime_error("Zero mesh normal");
          prim.mesh.normals.push_back(preserve ? vn : normalized(q));
        }
      }
      if (p.indices) {
        if (p.indices->type != cgltf_type_scalar ||
            (!preserve && p.indices->is_sparse) || p.indices->normalized ||
            (p.indices->component_type != cgltf_component_type_r_8u &&
             p.indices->component_type != cgltf_component_type_r_16u &&
             p.indices->component_type != cgltf_component_type_r_32u))
          throw std::runtime_error("Unsupported index accessor");
        std::vector<float> sparse_indices;
        if (p.indices->is_sparse)
          sparse_indices = floats(p.indices, cgltf_type_scalar, indices, b);
        for (size_t k = 0; k < indices; k++) {
          if (k % 1024 == 0)
            stop(o);
          if (p.indices->is_sparse &&
              (sparse_indices[k] < 0 || sparse_indices[k] >= count))
            throw std::runtime_error("Sparse triangle index out of range");
          auto index = p.indices->is_sparse
                           ? cgltf_size(sparse_indices[k])
                           : cgltf_accessor_read_index(p.indices, k);
          if (index >= count)
            throw std::runtime_error("Triangle index out of range");
          prim.mesh.indices.push_back(uint32_t(index));
        }
      } else
        for (size_t k = 0; k < count; k++)
          prim.mesh.indices.push_back(uint32_t(k));
      if (det < 0)
        for (size_t k = 0; k < indices; k += 3)
          std::swap(prim.mesh.indices[k + 1], prim.mesh.indices[k + 2]);
      for (size_t a = 0; a < p.attributes_count; a++) {
        auto &attr = p.attributes[a];
        for (size_t previous = 0; previous < a; ++previous)
          if (p.attributes[previous].type == attr.type &&
              p.attributes[previous].index == attr.index)
            throw std::runtime_error("Duplicate glTF vertex attribute");
        if (attr.index != 0 && attr.type != cgltf_attribute_type_texcoord &&
            !(preserve && (attr.type == cgltf_attribute_type_joints ||
                           attr.type == cgltf_attribute_type_weights)))
          throw std::runtime_error("Unsupported glTF attribute set");
        if (preserve && (attr.type == cgltf_attribute_type_joints ||
                         attr.type == cgltf_attribute_type_weights))
          continue;
        if (attr.type == cgltf_attribute_type_texcoord &&
            (attr.index == 0 || attr.index == 1)) {
          auto uv = floats(attr.data, cgltf_type_vec2, count, b);
          auto &dest = attr.index ? prim.uv1 : prim.mesh.uv;
          for (size_t k = 0; k < count; k++)
            dest.push_back({uv[k * 2], uv[k * 2 + 1]});
        } else if (attr.type == cgltf_attribute_type_color && attr.index == 0) {
          auto type = attr.data->type;
          if (type != cgltf_type_vec3 && type != cgltf_type_vec4)
            throw std::runtime_error("Unsupported vertex color type");
          auto colors = floats(attr.data, type, count, b);
          for (float value : colors)
            if (value < 0 || value > 1)
              throw std::runtime_error(
                  "Vertex colors must be in the unit range");
          size_t dim = cgltf_num_components(type);
          for (size_t k = 0; k < count; k++)
            prim.colors.push_back({colors[k * dim], colors[k * dim + 1],
                                   colors[k * dim + 2],
                                   dim == 4 ? colors[k * dim + 3] : 1});
        } else if (attr.type == cgltf_attribute_type_tangent) {
          auto tangents = floats(attr.data, cgltf_type_vec4, count, b);
          for (size_t k = 0; k < count; k++) {
            auto q = preserve
                         ? Vec3{tangents[k * 4], tangents[k * 4 + 1],
                                tangents[k * 4 + 2]}
                         : col0 * tangents[k * 4] + col1 * tangents[k * 4 + 1] +
                               col2 * tangents[k * 4 + 2];
            if (!std::isfinite(length(q)) || length(q) < 1e-12f ||
                std::abs(std::abs(tangents[k * 4 + 3]) - 1) > 1e-5f)
              throw std::runtime_error("Invalid vertex tangent");
            if (!preserve)
              q = normalized(q);
            prim.tangents.push_back(
                {q.x, q.y, q.z, tangents[k * 4 + 3] * (det < 0 ? -1 : 1)});
          }
        } else if (attr.type != cgltf_attribute_type_position &&
                   attr.type != cgltf_attribute_type_normal)
          throw std::runtime_error(
              "Unsupported vertex attribute; import would lose data");
      }
      if (preserve)
        deformation(p, *mesh, prim, b, o, out);
      if (prim.material >= out.materials.size())
        throw std::runtime_error("Invalid primitive material");
      auto &mat = out.materials[prim.material];
      for (auto [texture, set] :
           {std::pair{mat.color_texture, mat.color_texcoord},
            {mat.metal_rough_texture, mat.metal_rough_texcoord},
            {mat.normal_texture, mat.normal_texcoord},
            {mat.occlusion_texture, mat.occlusion_texcoord},
            {mat.emissive_texture, mat.emissive_texcoord}})
        if (texture >= 0 && (set ? prim.uv1.empty() : prim.mesh.uv.empty()))
          throw std::runtime_error(
              "Textured primitive is missing the referenced UV set");
      for (auto color : prim.colors)
        for (float c : color)
          unit(c);
      if (!na) {
        if (preserve)
          b.take(count * sizeof(Vec3));
        generate_normals(prim.mesh, preserve);
      }
      prim.mesh.validate();
      if (preserve)
        out.meshes[prim.source_mesh].primitives.push_back(
            uint32_t(out.primitives.size()));
      out.primitives.push_back(std::move(prim));
    }
  }
  if (preserve)
    skin_mappings(d, out, o);
  if (!preserve && out.primitives.empty())
    throw std::runtime_error("No triangle mesh in imported glTF scene");
  stop(o);
  return out;
}
int obj_index(std::string_view text, size_t count) {
  if (text.empty())
    return -1;
  int64_t n = 0;
  auto [p, e] = std::from_chars(text.data(), text.data() + text.size(), n);
  if (e != std::errc{} || p != text.data() + text.size() || !n)
    throw std::runtime_error("Invalid OBJ index");
  int64_t i = n > 0 ? n - 1 : int64_t(count) + n;
  if (i < 0 || uint64_t(i) >= count)
    throw std::runtime_error("OBJ index out of range");
  return int(i);
}
std::vector<std::array<size_t, 3>> triangulate(const std::vector<Vec3> &p,
                                               const ImportLimits &o) {
  if (p.size() < 3 || p.size() > 4096)
    throw std::runtime_error("OBJ face size outside limit");
  Vec3 normal{};
  for (size_t i = 0; i < p.size(); i++) {
    auto a = p[i], b = p[(i + 1) % p.size()];
    normal.x += (a.y - b.y) * (a.z + b.z);
    normal.y += (a.z - b.z) * (a.x + b.x);
    normal.z += (a.x - b.x) * (a.y + b.y);
  }
  int axis = std::abs(normal.y) > std::abs(normal.x) ? 1 : 0;
  if (std::abs(normal.z) >
      (axis == 0 ? std::abs(normal.x) : std::abs(normal.y)))
    axis = 2;
  auto xy = [&](size_t i) {
    auto a = p[i];
    return axis == 0   ? std::array<double, 2>{a.y, a.z}
           : axis == 1 ? std::array<double, 2>{a.x, a.z}
                       : std::array<double, 2>{a.x, a.y};
  };
  auto area = [&](size_t a, size_t b, size_t c) {
    auto x = xy(a), y = xy(b), z = xy(c);
    return (y[0] - x[0]) * (z[1] - x[1]) - (y[1] - x[1]) * (z[0] - x[0]);
  };
  double twice_area = 0;
  for (size_t i = 0; i < p.size(); i++) {
    auto a = xy(i), b = xy((i + 1) % p.size());
    twice_area += a[0] * b[1] - a[1] * b[0];
  }
  if (std::abs(twice_area) < 1e-15)
    throw std::runtime_error("Degenerate OBJ polygon");
  double sign = twice_area > 0 ? 1 : -1;
  std::vector<size_t> polygon(p.size());
  std::iota(polygon.begin(), polygon.end(), 0);
  std::vector<std::array<size_t, 3>> result;
  size_t work = 0;
  // Ear clipping alone may accept a crossing contour with nonzero signed area.
  // Reject crossing/touching nonadjacent edges before producing triangles.
  for (size_t i = 0; i < p.size(); i++)
    for (size_t j = i + 1; j < p.size(); j++) {
      if ((++work % 256) == 0)
        stop(o);
      size_t in = (i + 1) % p.size(), jn = (j + 1) % p.size();
      if (in == j || jn == i)
        continue;
      auto on = [&](size_t a, size_t b, size_t q) {
        auto x = xy(a), y = xy(b), z = xy(q);
        return std::abs(area(a, b, q)) <= 1e-14 &&
               z[0] >= std::min(x[0], y[0]) - 1e-14 &&
               z[0] <= std::max(x[0], y[0]) + 1e-14 &&
               z[1] >= std::min(x[1], y[1]) - 1e-14 &&
               z[1] <= std::max(x[1], y[1]) + 1e-14;
      };
      double a = area(i, in, j), b = area(i, in, jn), c = area(j, jn, i),
             d = area(j, jn, in);
      if ((((a > 0 && b < 0) || (a < 0 && b > 0)) &&
           ((c > 0 && d < 0) || (c < 0 && d > 0))) ||
          on(i, in, j) || on(i, in, jn) || on(j, jn, i) || on(j, jn, in))
        throw std::runtime_error("Self-intersecting OBJ polygon");
    }
  while (polygon.size() > 3) {
    bool clipped = false;
    for (size_t i = 0; i < polygon.size(); i++) {
      size_t a = polygon[(i + polygon.size() - 1) % polygon.size()],
             b = polygon[i], c = polygon[(i + 1) % polygon.size()];
      if (area(a, b, c) * sign <= 1e-14)
        continue;
      bool inside = false;
      for (auto q : polygon) {
        if ((++work % 256) == 0)
          stop(o);
        if (work > 32'000'000)
          throw std::length_error(
              "OBJ polygon triangulation exceeds work budget");
        if (q != a && q != b && q != c && area(a, b, q) * sign >= -1e-14 &&
            area(b, c, q) * sign >= -1e-14 && area(c, a, q) * sign >= -1e-14) {
          inside = true;
          break;
        }
      }
      if (inside)
        continue;
      result.push_back({a, b, c});
      polygon.erase(polygon.begin() + i);
      clipped = true;
      break;
    }
    if (!clipped)
      throw std::runtime_error("OBJ polygon is self-intersecting or has "
                               "ambiguous repeated/collinear corners");
  }
  result.push_back({polygon[0], polygon[1], polygon[2]});
  return result;
}
ImportedAsset obj(const fs::path &path, const ImportLimits &o, Budget &b) {
  auto bytes = read_file(path, o.max_file_bytes, b);
  if (std::find(bytes.begin(), bytes.end(), uint8_t(0)) != bytes.end())
    throw std::runtime_error("NUL in OBJ text");
  std::istringstream stream(std::string(bytes.begin(), bytes.end()));
  std::vector<Vec3> positions, normals;
  std::vector<std::array<float, 2>> uvs;
  std::vector<std::array<float, 4>> colors;
  bool has_colors = false;
  ImportedAsset out;
  out.materials.push_back({});
  out.materials[0].metallic = 0;
  std::map<std::string, uint32_t> material_ids;
  std::set<std::string> defined_materials;
  std::string object = "Object", group = "", material;
  uint32_t current = 0;
  std::string line;
  int smoothing = 0;
  size_t face_number = 0, vertex_count = 0, triangles = 0;
  std::map<std::array<int64_t, 4>, uint32_t> corners;
  ImportedPrimitive *active = nullptr;
  std::vector<bool> normal_missing;
  bool any_uv = false, missing_uv = false;
  size_t line_number = 0;
  auto flush = [&] {
    if (!active)
      return;
    if (active->mesh.indices.empty()) {
      out.primitives.pop_back();
      active = nullptr;
      return;
    }
    if (std::any_of(normal_missing.begin(), normal_missing.end(),
                    [](bool v) { return v; })) {
      auto source = active->mesh.normals;
      generate_normals(active->mesh);
      for (size_t i = 0; i < source.size(); i++)
        if (!normal_missing[i])
          active->mesh.normals[i] = source[i];
    }
    if (!any_uv)
      active->mesh.uv.clear();
    else if (missing_uv)
      out.warnings.push_back(
          "OBJ primitive has corners without UVs; missing UVs are(0,0)");
    if (!has_colors)
      active->colors.clear();
    active->mesh.validate();
    active = nullptr;
    corners.clear();
    normal_missing.clear();
    any_uv = missing_uv = false;
  };
  auto material_index = [&](const std::string &name) {
    auto [it, added] =
        material_ids.emplace(name, uint32_t(out.materials.size()));
    if (added) {
      ImportedMaterial m;
      m.name = name;
      m.metallic = 0;
      out.materials.push_back(m);
    }
    return it->second;
  };
  auto load_mtl = [&](std::string name) {
    auto source = resource(path.parent_path(), name, o);
    out.warnings.push_back("MTL Phong lighting is approximated as core PBR; "
                           "non-core properties are not authoring channels");
    auto raw = read_file(source, o.max_file_bytes, b);
    std::istringstream text(std::string(raw.begin(), raw.end()));
    ImportedMaterial *m = nullptr;
    std::string l;
    while (std::getline(text, l)) {
      stop(o);
      auto hash = l.find('#');
      if (hash != std::string::npos)
        l.erase(hash);
      std::istringstream words(l);
      std::string type;
      words >> type;
      if (type == "newmtl") {
        std::string n;
        std::getline(words >> std::ws, n);
        if (n.empty() || n.size() > 1024)
          throw std::runtime_error("Invalid MTL material name");
        m = &out.materials[material_index(n)];
        defined_materials.insert(n);
      } else if (m) {
        if (type == "Kd") {
          if (!(words >> m->color[0] >> m->color[1] >> m->color[2]))
            throw std::runtime_error("Invalid MTL Kd");
          for (float f : m->color)
            unit(f);
        } else if (type == "Ke") {
          if (!(words >> m->emissive[0] >> m->emissive[1] >> m->emissive[2]))
            throw std::runtime_error("Invalid MTL Ke");
          for (float f : m->emissive)
            unit(f);
        } else if (type == "d" || type == "Tr") {
          float alpha;
          if (!(words >> alpha))
            throw std::runtime_error("Invalid MTL opacity");
          m->color[3] = type == "Tr" ? 1 - alpha : alpha;
          unit(m->color[3]);
          m->alpha = m->color[3] < 1 ? ImportedMaterial::AlphaMode::Blend
                                     : ImportedMaterial::AlphaMode::Opaque;
        } else if (type == "Ns") {
          float ns;
          if (!(words >> ns) || !std::isfinite(ns) || ns < 0)
            throw std::runtime_error("Invalid MTL specular exponent");
          m->roughness = std::clamp(std::sqrt(2.f / (ns + 2)), 0.f, 1.f);
        } else if (type == "map_Kd") {
          std::string uri;
          std::getline(words >> std::ws, uri);
          if (uri.empty() || uri[0] == '-')
            throw std::runtime_error(
                "MTL texture options require an explicit import path");
          auto imagepath = resource(
              path.parent_path(),
              (source.parent_path().lexically_relative(path.parent_path()) /
               uri)
                  .generic_string(),
              o);
          ImportedImage image;
          image.name = uri;
          image.source_uri =
              fs::relative(imagepath, path.parent_path()).generic_string();
          image.mime_type = mime_for(imagepath);
          image.bytes = read_file(imagepath, o.max_resource_bytes, b);
          out.images.push_back(std::move(image));
          ImportedTexture texture;
          texture.image = int(out.images.size() - 1);
          out.textures.push_back(texture);
          m->color_texture = int(out.textures.size() - 1);
        } else if (!type.empty() && type != "Ka" && type != "Ks" &&
                   type != "Ni" && type != "illum")
          out.warnings.push_back(
              "Unsupported MTL property retained only as a warning: " + type);
      }
    }
  };
  while (std::getline(stream, line)) {
    if (++line_number % 256 == 0)
      stop(o);
    auto comment = line.find('#');
    if (comment != std::string::npos)
      line.erase(comment);
    std::istringstream words(line);
    std::string type;
    words >> type;
    if (type.empty())
      continue;
    if (type == "v") {
      std::vector<float> values;
      float f;
      while (words >> f)
        values.push_back(f);
      if (!words.eof() || values.size() < 3 ||
          (values.size() != 3 && values.size() != 4 && values.size() != 6 &&
           values.size() != 7))
        throw std::runtime_error("Unsupported OBJ vertex record");
      for (float v : values)
        if (!std::isfinite(v))
          throw std::runtime_error("Nonfinite OBJ vertex");
      Vec3 p{values[0], values[1], values[2]};
      if (values.size() == 4) {
        if (values[3] == 0)
          throw std::runtime_error("Zero homogeneous OBJ vertex weight");
        p = p / values[3];
      }
      if (!finite(p) ||
          std::max({std::abs(p.x), std::abs(p.y), std::abs(p.z)}) > 100)
        throw std::runtime_error("OBJ position outside100m workspace");
      positions.push_back(p);
      std::array<float, 4> color{1, 1, 1, 1};
      if (values.size() >= 6) {
        has_colors = true;
        for (int k = 0; k < 3; k++) {
          unit(values[k + 3]);
          color[k] = values[k + 3];
        }
        if (values.size() == 7) {
          unit(values[6]);
          color[3] = values[6];
        }
      }
      colors.push_back(color);
      if (positions.size() > o.max_vertices)
        throw std::length_error("OBJ source vertex count exceeded");
      b.take(sizeof(Vec3) + sizeof(color));
    } else if (type == "vn") {
      Vec3 n;
      if (!(words >> n.x >> n.y >> n.z) || !finite(n) ||
          !std::isfinite(length(n)) || length(n) < 1e-12f)
        throw std::runtime_error("Invalid OBJ normal");
      normals.push_back(normalized(n));
      if (normals.size() > o.max_vertices)
        throw std::length_error("OBJ normal count exceeded");
      b.take(sizeof(Vec3));
    } else if (type == "vt") {
      std::array<float, 2> uv{};
      if (!(words >> uv[0]))
        throw std::runtime_error("Invalid OBJ UV");
      if (!(words >> uv[1]))
        uv[1] = 0;
      if (!std::isfinite(uv[0]) || !std::isfinite(uv[1]))
        throw std::runtime_error("Nonfinite OBJ UV");
      uvs.push_back(uv);
      if (uvs.size() > o.max_vertices)
        throw std::length_error("OBJ UV count exceeded");
      b.take(sizeof(uv));
    } else if (type == "o" || type == "g") {
      flush();
      std::string n;
      std::getline(words >> std::ws, n);
      if (n.size() > 1024)
        throw std::runtime_error("OBJ group name too long");
      if (type == "o")
        object = n.empty() ? "Object" : n;
      else
        group = n;
    } else if (type == "usemtl") {
      flush();
      std::getline(words >> std::ws, material);
      if (material.size() > 1024)
        throw std::runtime_error("OBJ material name too long");
      current = material_index(material);
    } else if (type == "mtllib") {
      flush();
      std::string name;
      while (words >> name)
        load_mtl(name);
    } else if (type == "s") {
      std::string n;
      words >> n;
      if (n == "off" || n == "0")
        smoothing = 0;
      else {
        auto [p, e] = std::from_chars(n.data(), n.data() + n.size(), smoothing);
        if (e != std::errc{} || p != n.data() + n.size() || smoothing <= 0)
          throw std::runtime_error("Invalid OBJ smoothing group");
      }
    } else if (type == "f") {
      ++face_number;
      std::vector<std::array<int, 3>> face;
      std::vector<Vec3> polygon;
      std::string token;
      while (words >> token) {
        if (face.size() >= 4096)
          throw std::length_error("OBJ polygon has too many corners");
        auto a = token.find('/'), z = a == std::string::npos
                                          ? std::string::npos
                                          : token.find('/', a + 1);
        if (z != std::string::npos &&
            token.find('/', z + 1) != std::string::npos)
          throw std::runtime_error("Invalid OBJ face corner");
        int vi = obj_index(std::string_view(token).substr(0, a),
                           positions.size()),
            ti = a == std::string::npos
                     ? -1
                     : obj_index(
                           std::string_view(token).substr(
                               a + 1, z == std::string::npos ? z : z - a - 1),
                           uvs.size()),
            ni = z == std::string::npos
                     ? -1
                     : obj_index(std::string_view(token).substr(z + 1),
                                 normals.size());
        if (vi < 0)
          throw std::runtime_error("Missing OBJ position index");
        face.push_back({vi, ti, ni});
        polygon.push_back(positions[vi]);
      }
      auto tris = triangulate(polygon, o);
      if (tris.size() > o.max_triangles - triangles)
        throw std::length_error("OBJ triangle budget exceeded");
      triangles += tris.size();
      if (!active) {
        if (out.primitives.size() >= 65536)
          throw std::length_error("Too many OBJ groups");
        b.take(sizeof(ImportedPrimitive) + object.size() + group.size() + 4);
        out.primitives.push_back({});
        active = &out.primitives.back();
        active->name = object + (group.empty() ? "" : " / " + group);
        active->material = current;
      }
      for (auto tri : tris)
        for (auto k : tri) {
          auto ref = face[k];
          std::array<int64_t, 4> key = {ref[0], ref[1], ref[2],
                                        ref[2] >= 0 ? 0
                                        : smoothing ? smoothing
                                                    : -int64_t(face_number)};
          auto [it, added] =
              corners.emplace(key, uint32_t(active->mesh.positions.size()));
          if (added) {
            if (++vertex_count > o.max_vertices)
              throw std::length_error("OBJ expanded vertex budget exceeded");
            b.take(sizeof(Vec3) * 2 + sizeof(std::array<float, 2>) +
                   sizeof(std::array<float, 4>));
            active->mesh.positions.push_back(positions[ref[0]]);
            active->mesh.normals.push_back(ref[2] >= 0 ? normals[ref[2]]
                                                       : Vec3{});
            normal_missing.push_back(ref[2] < 0);
            active->mesh.uv.push_back(ref[1] >= 0 ? uvs[ref[1]]
                                                  : std::array<float, 2>{});
            any_uv |= ref[1] >= 0;
            missing_uv |= ref[1] < 0;
            active->colors.push_back(colors[ref[0]]);
          }
          active->mesh.indices.push_back(it->second);
          b.take(sizeof(uint32_t));
        }
    } else
      out.warnings.push_back("Unsupported OBJ record: " + type);
  }
  flush();
  if (out.primitives.empty())
    throw std::runtime_error("OBJ contains no triangle surface");
  for (auto &[name, index] : material_ids)
    if (!defined_materials.contains(name))
      out.warnings.push_back("OBJ material has no definition; using default: " +
                             name);
  stop(o);
  return out;
}
} // namespace
ImportedAsset import_mesh(const fs::path &source, const ImportLimits &o) {
  if (!o.max_file_bytes || o.max_file_bytes > 512 * 1024 * 1024 ||
      o.max_resource_bytes < o.max_file_bytes ||
      o.max_resource_bytes > 1024ull * 1024 * 1024 || !o.max_vertices ||
      o.max_vertices > 2'000'000 || !o.max_triangles ||
      o.max_triangles > 2'000'000 || !o.max_metadata_visits ||
      o.max_metadata_visits > 200'000'000 ||
      (o.mode != ImportMode::StaticScene && o.mode != ImportMode::PreserveGltf))
    throw std::invalid_argument("Invalid import limits");
  stop(o);
  auto path = fs::canonical(source);
  if (!fs::is_regular_file(path))
    throw std::runtime_error("Import source is not a regular file");
  auto ext = path.extension().string();
  for (auto &c : ext)
    c = char(std::tolower(static_cast<unsigned char>(c)));
  Budget budget{0, o.max_resource_bytes};
  if (ext == ".obj")
    return obj(path, o, budget);
  if (ext == ".gltf" || ext == ".glb")
    return gltf(path, o, budget);
  throw std::runtime_error("Unsupported mesh import format (use OBJ/glTF/GLB)");
}
} // namespace dome
