// SPDX-License-Identifier: GPL-3.0-or-later
// MToon lighting adaptation: UniVRM/MToon, MIT. See docs/MTOON-LICENSE.txt.
#include "dome/mtoon.hpp"
#include "dome/mtoon-shader.hpp"
#include <set>
namespace dome {
namespace {
using Json = nlohmann::json;
constexpr float pi = 3.14159265358979323846f, eps = 0.0009765625f;
constexpr size_t S = size_t(MToonSlot::Count);
const std::array<const char *, S> names0 = {"_MainTex",
                                            "_ShadeTexture",
                                            "_BumpMap",
                                            "_EmissionMap",
                                            "_RimTexture",
                                            "_SphereAdd",
                                            "",
                                            "_OutlineWidthTexture",
                                            "_UvAnimMaskTexture",
                                            "_ReceiveShadowTexture",
                                            "_ShadingGradeTexture"};
const std::array<const char *, S> names1 = {"baseColorTexture",
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
float sat(float x) { return std::clamp(x, 0.f, 1.f); }
Vec3 unit(Vec3 v) {
  if (!finite(v))
    throw std::invalid_argument("Nonfinite MToon direction");
  float m = std::max({std::abs(v.x), std::abs(v.y), std::abs(v.z)});
  if (m < 1e-20f)
    throw std::invalid_argument("Zero MToon direction");
  return normalized(v / m);
}
Vec3 mul(Vec3 a, Vec3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
Vec3 mix(Vec3 a, Vec3 b, float t) { return a * (1 - t) + b * t; }
Vec3 minimum(Vec3 a, Vec3 b) {
  return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}
Vec3 desaturate(Vec3 a, float t) {
  float m = std::max({eps, a.x, a.y, a.z});
  return mix(a, {m, m, m}, t);
}
Json rgb(Vec3 v) { return Json::array({v.x, v.y, v.z}); }
void bounded_json(const Json &j, bool native = false) {
  size_t count = 0, bytes = 0;
  auto walk = [&](auto &&self, const Json &v, unsigned depth) -> void {
    if (depth > (native ? 36 : 32) || ++count > (native ? 200'000 : 100'000))
      throw std::length_error("MToon JSON structural budget exceeded");
    bytes += 16;
    if (v.is_string())
      bytes += v.get_ref<const std::string &>().size();
    if (bytes > (native ? 4 : 2) * 1024 * 1024)
      throw std::length_error("MToon JSON byte budget exceeded");
    if (v.is_number_float() && !std::isfinite(v.get<double>()))
      throw std::invalid_argument("Nonfinite MToon JSON value");
    if (v.is_object())
      for (auto it = v.begin(); it != v.end(); ++it) {
        bytes += it.key().size();
        self(self, it.value(), depth + 1);
      }
    else if (v.is_array())
      for (auto &x : v)
        self(self, x, depth + 1);
  };
  walk(walk, j, 0);
}
float number(const Json &j, const char *name, float fallback) {
  if (!j.contains(name))
    return fallback;
  if (!j[name].is_number())
    throw std::invalid_argument(std::string("MToon number required: ") + name);
  float x = j[name].get<float>();
  if (!std::isfinite(x))
    throw std::invalid_argument("MToon number outside float range");
  return x;
}
int integer(const Json &j, const char *name, int fallback, int low, int high) {
  if (!j.contains(name))
    return fallback;
  if (!j[name].is_number())
    throw std::invalid_argument("MToon integer required");
  double x = j[name].get<double>();
  if (!std::isfinite(x) || x < low || x > high || std::trunc(x) != x)
    throw std::invalid_argument(std::string("Invalid MToon integer: ") + name);
  return int(x);
}
bool boolean(const Json &j, const char *name, bool fallback) {
  if (!j.contains(name))
    return fallback;
  if (!j[name].is_boolean())
    throw std::invalid_argument("MToon boolean required");
  return j[name].get<bool>();
}
std::array<float, 4> vector4(const Json &j, const char *name,
                             std::array<float, 4> fallback, size_t size = 4) {
  if (!j.contains(name))
    return fallback;
  auto &v = j[name];
  if (!v.is_array() || v.size() != size)
    throw std::invalid_argument(std::string("MToon vector dimension: ") + name);
  for (size_t k = 0; k < size; k++) {
    if (!v[k].is_number())
      throw std::invalid_argument("MToon vector number required");
    fallback[k] = v[k].get<float>();
    if (!std::isfinite(fallback[k]))
      throw std::invalid_argument("Nonfinite MToon vector");
  }
  return fallback;
}
Vec3 color(const Json &j, const char *name, Vec3 fallback, bool gamma,
           size_t n = 3) {
  auto v = vector4(j, name, {fallback.x, fallback.y, fallback.z, 1}, n);
  Vec3 out{v[0], v[1], v[2]};
  if (gamma)
    out = {mtoon_srgb_to_linear(out.x), mtoon_srgb_to_linear(out.y),
           mtoon_srgb_to_linear(out.z)};
  return out;
}
Json gamma_color(Vec3 c, float alpha) {
  return Json::array({mtoon_linear_to_srgb(c.x), mtoon_linear_to_srgb(c.y),
                      mtoon_linear_to_srgb(c.z), alpha});
}
void patch(Json &source, const Json &old, const Json &now) {
  if (old == now)
    return;
  if (old.is_object() && now.is_object()) {
    if (!source.is_object())
      source = Json::object();
    for (auto it = old.begin(); it != old.end(); ++it)
      if (!now.contains(it.key()))
        source.erase(it.key());
    for (auto it = now.begin(); it != now.end(); ++it) {
      if (old.contains(it.key())) {
        if (old[it.key()] != it.value())
          patch(source[it.key()], old[it.key()], it.value());
      } else
        source[it.key()] = it.value();
    }
  } else
    source = now;
}
void slot_defaults(MToonMaterial &m) {
  for (size_t i = 0; i < S; i++) {
    auto &s = m.textures[i];
    s.channel = 0;
    s.encoding =
        (i == size_t(MToonSlot::Normal)) ? MToonEncoding::Normal
        : (i == size_t(MToonSlot::Base) || i == size_t(MToonSlot::Shade) ||
           i == size_t(MToonSlot::Emission) || i == size_t(MToonSlot::Rim) ||
           i == size_t(MToonSlot::Matcap) || m.version() == MToonVersion::V0)
            ? MToonEncoding::Srgb
            : MToonEncoding::Linear;
    if (m.version() == MToonVersion::V1) {
      if (i == size_t(MToonSlot::OutlineWidth))
        s.channel = 1;
      if (i == size_t(MToonSlot::UvMask))
        s.channel = 2;
    }
  }
}
Json texture_json(const MToonTexture &s, bool scale) {
  Json j = {{"index", s.index}, {"texCoord", s.texcoord}};
  if (scale)
    j["scale"] = s.strength;
  if (s.offset != std::array<float, 2>{} ||
      s.scale != std::array<float, 2>{1, 1} || s.rotation != 0 ||
      s.transform_texcoord >= 0) {
    auto &t = j["extensions"]["KHR_texture_transform"];
    t = {{"offset", s.offset}, {"scale", s.scale}, {"rotation", s.rotation}};
    if (s.transform_texcoord >= 0)
      t["texCoord"] = s.transform_texcoord;
  }
  return j;
}
void parse_texture(const Json &j, MToonTexture &s, bool scalar_scale) {
  if (!j.is_object())
    throw std::invalid_argument("MToon texture info must be object");
  if (!j.contains("index"))
    throw std::invalid_argument("MToon texture index required");
  s.index = integer(j, "index", -1, 0, INT32_MAX - 1);
  s.texcoord = uint32_t(integer(j, "texCoord", 0, 0, 999999999));
  s.strength = scalar_scale ? number(j, "scale", 1) : 1;
  if (j.contains("extensions") &&
      j["extensions"].contains("KHR_texture_transform")) {
    auto &t = j["extensions"]["KHR_texture_transform"];
    if (!t.is_object())
      throw std::invalid_argument("Invalid MToon texture transform");
    auto a = vector4(t, "offset", {}, 2),
         b = vector4(t, "scale", {1, 1, 0, 0}, 2);
    s.offset = {a[0], a[1]};
    s.scale = {b[0], b[1]};
    s.rotation = number(t, "rotation", 0);
    s.transform_texcoord =
        t.contains("texCoord") ? integer(t, "texCoord", 0, 0, 999999999) : -1;
  }
}
void check_range(float x, float low, float high) {
  if (!std::isfinite(x) || x < low || x > high)
    throw std::invalid_argument("MToon parameter outside domain");
}
} // namespace
MToonMaterial::MToonMaterial() {
  source_ = {
      {"extensions", {{"VRMC_materials_mtoon", {{"specVersion", "1.0"}}}}}};
  slot_defaults(*this);
  initial_known_ = known();
}
std::array<float, 4> mtoon_default_texel(MToonSlot slot) {
  if (size_t(slot) >= S)
    throw std::invalid_argument("Unknown MToon texture role");
  if (slot == MToonSlot::Normal)
    return {.5f, .5f, 1, 1};
  if (slot == MToonSlot::Matcap || slot == MToonSlot::Shift)
    return {0, 0, 0, 1};
  return {1, 1, 1, 1};
}
float mtoon_srgb_to_linear(float x) {
  if (!std::isfinite(x) || x < 0)
    throw std::invalid_argument("Invalid sRGB value");
  double result = x <= .04045f ? double(x) / 12.92
                               : std::pow((double(x) + .055) / 1.055, 2.4);
  if (result > std::numeric_limits<float>::max())
    throw std::overflow_error("sRGB conversion exceeds float range");
  return float(result);
}
float mtoon_linear_to_srgb(float x) {
  if (!std::isfinite(x) || x < 0)
    throw std::invalid_argument("Invalid linear color value");
  return float(x <= .0031308f ? double(x) * 12.92
                              : 1.055 * std::pow(double(x), 1 / 2.4) - .055);
}
std::array<float, 4> mtoon_decode_texel(MToonEncoding e,
                                        std::array<float, 4> v) {
  for (auto x : v)
    check_range(x, 0, 1);
  if (e == MToonEncoding::Srgb)
    for (size_t k = 0; k < 3; k++)
      v[k] = mtoon_srgb_to_linear(v[k]);
  return v;
}
MToonMaterial MToonMaterial::from_vrm0(const Json &j, size_t count) {
  bounded_json(j);
  if (!j.is_object() || j.value("shader", std::string{}) != "VRM/MToon")
    throw std::invalid_argument("Expected VRM0 VRM/MToon material properties");
  MToonMaterial m;
  m.version_ = MToonVersion::V0;
  m.source_ = j;
  slot_defaults(m);
  auto &p = m.parameters;
  const Json empty = Json::object();
  auto &f = j.contains("floatProperties") ? j["floatProperties"] : empty;
  auto &v = j.contains("vectorProperties") ? j["vectorProperties"] : empty;
  auto &t = j.contains("textureProperties") ? j["textureProperties"] : empty;
  if (!f.is_object() || !v.is_object() || !t.is_object())
    throw std::invalid_argument("Invalid VRM0 property maps");
  for (auto it = f.begin(); it != f.end(); ++it)
    number(f, it.key().c_str(), 0);
  for (auto it = v.begin(); it != v.end(); ++it) {
    if (!it.value().is_array())
      throw std::invalid_argument("VRM0 vector property must be numeric array");
    for (auto &x : it.value())
      if (!x.is_number())
        throw std::invalid_argument(
            "VRM0 vector property must be numeric array");
  }
  for (auto it = t.begin(); it != t.end(); ++it)
    integer(t, it.key().c_str(), -1, -1, INT32_MAX - 1);
  if (j.contains("keywordMap")) {
    if (!j["keywordMap"].is_object())
      throw std::invalid_argument("VRM0 keyword map must be object");
    for (auto &x : j["keywordMap"])
      if (!x.is_boolean())
        throw std::invalid_argument("VRM0 keyword must be boolean");
  }
  if (j.contains("tagMap")) {
    if (!j["tagMap"].is_object())
      throw std::invalid_argument("VRM0 tag map must be object");
    for (auto &x : j["tagMap"])
      if (!x.is_string())
        throw std::invalid_argument("VRM0 tag must be string");
  }
  auto base = vector4(v, "_Color", {1, 1, 1, 1});
  for (size_t k = 0; k < 3; k++)
    p.base_color[k] = mtoon_srgb_to_linear(base[k]);
  p.base_color[3] = base[3];
  p.shade_color = color(v, "_ShadeColor", {.97f, .81f, .86f}, true, 4);
  p.emission = color(v, "_EmissionColor", {}, false, 4);
  p.rim_color = color(v, "_RimColor", {}, true, 4);
  p.outline_color = color(v, "_OutlineColor", {}, true, 4);
  p.shift = number(f, "_ShadeShift", 0);
  p.toony = number(f, "_ShadeToony", .9f);
  p.receive_shadow = number(f, "_ReceiveShadowRate", 1);
  p.shading_grade = number(f, "_ShadingGradeRate", 1);
  p.light_color_attenuation = number(f, "_LightColorAttenuation", 0);
  p.indirect_intensity = number(f, "_IndirectLightIntensity", .1f);
  p.normal_scale = number(f, "_BumpScale", 1);
  p.rim_power = number(f, "_RimFresnelPower", 1);
  p.rim_lift = number(f, "_RimLift", 0);
  p.rim_mix = number(f, "_RimLightingMix", 0);
  p.alpha_cutoff = number(f, "_Cutoff", .5f);
  p.outline_width = number(f, "_OutlineWidth", .5f);
  p.outline_mix = number(f, "_OutlineLightingMix", 1);
  p.outline_max_distance = number(f, "_OutlineScaledMaxDistance", 1);
  p.outline = MToonOutline(integer(f, "_OutlineWidthMode", 0, 0, 2));
  p.legacy_outline_fixed = integer(f, "_OutlineColorMode", 1, 0, 1) == 0;
  p.scroll = {number(f, "_UvAnimScrollX", 0), number(f, "_UvAnimScrollY", 0)};
  p.rotation_speed = number(f, "_UvAnimRotation", 0);
  p.legacy_shader_version = number(f, "_MToonVersion", 39);
  int mode = integer(f, "_BlendMode", 0, 0, 3);
  p.alpha = mode == 0   ? MToonAlpha::Opaque
            : mode == 1 ? MToonAlpha::Mask
                        : MToonAlpha::Blend;
  p.depth_write = integer(f, "_ZWrite", mode == 2 ? 0 : 1, 0, 1) != 0;
  p.source_blend = integer(f, "_SrcBlend", mode < 2 ? 1 : 5, 0, 10);
  p.destination_blend = integer(f, "_DstBlend", mode < 2 ? 0 : 10, 0, 10);
  p.render_queue = integer(j, "renderQueue", -1, -1, 5000);
  if (p.render_queue == -1)
    p.render_queue = mode == 0   ? 2000
                     : mode == 1 ? 2450
                     : mode == 2 ? 3000
                                 : 2501;
  p.cull = MToonCull(integer(f, "_CullMode", 2, 0, 2));
  p.outline_cull = MToonCull(integer(f, "_OutlineCullMode", 1, 0, 2));
  p.alpha_to_coverage = integer(f, "_AlphaToMask", 0, 0, 1) != 0;
  for (size_t i = 0; i < S; i++)
    if (*names0[i]) {
      auto &s = m.textures[i];
      s.index = integer(t, names0[i], -1, -1, INT32_MAX - 1);
      auto os = vector4(v, names0[i], {0, 0, 1, 1});
      s.offset = {os[0], os[1]};
      s.scale = {os[2], os[3]};
    }
  if (j.contains("keywordMap")) {
    p.normal_enabled = j["keywordMap"].value("_NORMALMAP", false);
    auto &keywords = j["keywordMap"];
    if ((keywords.value("_ALPHATEST_ON", false) &&
         p.alpha != MToonAlpha::Mask) ||
        (keywords.value("_ALPHABLEND_ON", false) &&
         p.alpha != MToonAlpha::Blend))
      m.diagnostics.push_back("Retained conflicting legacy alpha keywords; "
                              "typed blend mode needs explicit author review");
  }
  if (j.contains("keywordMap"))
    for (auto it = j["keywordMap"].begin(); it != j["keywordMap"].end(); ++it)
      if (it.key() != "_NORMALMAP" && it.key() != "_ALPHATEST_ON" &&
          it.key() != "_ALPHABLEND_ON")
        m.diagnostics.push_back("Retained legacy shader keyword; execution "
                                "requires explicit mapping: " +
                                it.key());
  if (j.contains("tagMap"))
    for (auto it = j["tagMap"].begin(); it != j["tagMap"].end(); ++it)
      if (it.key() != "RenderType")
        m.diagnostics.push_back("Retained unmapped legacy render tag: " +
                                it.key());
  m.initial_known_ = m.known();
  m.validate(count);
  for (size_t i = 0; i < S; i++)
    if (m.textures[i].texcoord > 1 || m.textures[i].transform_texcoord > 1)
      m.diagnostics.push_back("Retained texture coordinate set beyond UV1; "
                              "renderer must supply the selected set");
  auto known = m.initial_known_;
  for (auto category :
       {"floatProperties", "vectorProperties", "textureProperties"})
    if (j.contains(category))
      for (auto it = j[category].begin(); it != j[category].end(); ++it)
        if (!known[category].contains(it.key()))
          m.diagnostics.push_back(std::string("Retained unmapped VRM0 ") +
                                  category + "." + it.key());
  m.diagnostics.push_back(
      "VRM0 retains legacy gamma color factors, R mask channels, turn-based UV "
      "animation and legacy lighting; no VRM1 coercion");
  return m;
}
MToonMaterial MToonMaterial::from_vrm1(const Json &j, size_t count) {
  bounded_json(j);
  if (!j.is_object() || !j.contains("extensions") ||
      !j["extensions"].contains("VRMC_materials_mtoon"))
    throw std::invalid_argument("VRMC_materials_mtoon extension required");
  auto &e = j["extensions"]["VRMC_materials_mtoon"];
  if (!e.is_object() || e.value("specVersion", std::string{}) != "1.0")
    throw std::invalid_argument("Unsupported MToon extension version");
  MToonMaterial m;
  m.version_ = MToonVersion::V1;
  m.source_ = j;
  slot_defaults(m);
  auto &p = m.parameters;
  Json empty = Json::object();
  auto &b =
      j.contains("pbrMetallicRoughness") ? j["pbrMetallicRoughness"] : empty;
  if (!b.is_object())
    throw std::invalid_argument("Invalid glTF base-color material");
  p.base_color = vector4(b, "baseColorFactor", {1, 1, 1, 1});
  p.shade_color = color(e, "shadeColorFactor", {1, 1, 1}, false);
  p.emission = color(j, "emissiveFactor", {}, false);
  p.rim_color = color(e, "parametricRimColorFactor", {}, false);
  p.matcap_color = color(e, "matcapFactor", {1, 1, 1}, false);
  p.outline_color = color(e, "outlineColorFactor", {}, false);
  p.shift = number(e, "shadingShiftFactor", 0);
  p.toony = number(e, "shadingToonyFactor", .9f);
  p.gi_equalization = number(e, "giEqualizationFactor", .9f);
  p.rim_power = number(e, "parametricRimFresnelPowerFactor", 5);
  p.rim_lift = number(e, "parametricRimLiftFactor", 0);
  p.rim_mix = number(e, "rimLightingMixFactor", 1);
  p.outline_width = number(e, "outlineWidthFactor", 0);
  p.outline_mix = number(e, "outlineLightingMixFactor", 1);
  p.scroll = {number(e, "uvAnimationScrollXSpeedFactor", 0),
              number(e, "uvAnimationScrollYSpeedFactor", 0)};
  p.rotation_speed = number(e, "uvAnimationRotationSpeedFactor", 0);
  auto mode = j.value("alphaMode", std::string("OPAQUE"));
  if (mode != "OPAQUE" && mode != "MASK" && mode != "BLEND")
    throw std::invalid_argument("Invalid MToon alpha mode");
  p.alpha = mode == "OPAQUE" ? MToonAlpha::Opaque
            : mode == "MASK" ? MToonAlpha::Mask
                             : MToonAlpha::Blend;
  p.alpha_cutoff = number(j, "alphaCutoff", .5f);
  p.depth_write = p.alpha != MToonAlpha::Blend ||
                  boolean(e, "transparentWithZWrite", false);
  p.cull = boolean(j, "doubleSided", false) ? MToonCull::Off : MToonCull::Back;
  p.queue_offset = integer(e, "renderQueueOffsetNumber", 0, -9, 9);
  p.render_queue = p.alpha == MToonAlpha::Opaque ? 2000
                   : p.alpha == MToonAlpha::Mask ? 2450
                   : p.depth_write               ? 2501
                                                 : 3000;
  p.source_blend = p.alpha == MToonAlpha::Blend ? 5 : 1;
  p.destination_blend = p.alpha == MToonAlpha::Blend ? 10 : 0;
  auto outline = e.value("outlineWidthMode", std::string("none"));
  if (outline != "none" && outline != "worldCoordinates" &&
      outline != "screenCoordinates")
    throw std::invalid_argument("Invalid MToon outline mode");
  p.outline = outline == "none"               ? MToonOutline::None
              : outline == "worldCoordinates" ? MToonOutline::World
                                              : MToonOutline::Screen;
  for (size_t i = 0; i < S; i++)
    if (*names1[i]) {
      auto &owner = i == 0 ? b : (i == 2 || i == 3) ? j : e;
      if (owner.contains(names1[i])) {
        auto &info = owner[names1[i]];
        parse_texture(info, m.textures[i], i == 6);
        if (info.contains("extensions"))
          for (auto it = info["extensions"].begin();
               it != info["extensions"].end(); ++it)
            if (it.key() != "KHR_texture_transform")
              m.diagnostics.push_back(
                  std::string("Retained uninterpreted texture extension ") +
                  names1[i] + ":" + it.key());
      }
    }
  p.normal_scale =
      j.contains("normalTexture") ? number(j["normalTexture"], "scale", 1) : 1;
  p.normal_enabled = m.textures[2].index >= 0;
  if (j["extensions"].contains("KHR_materials_emissive_strength"))
    p.emission_strength =
        number(j["extensions"]["KHR_materials_emissive_strength"],
               "emissiveStrength", 1);
  m.initial_known_ = m.known();
  m.validate(count);
  for (size_t i = 0; i < S; i++)
    if (m.textures[i].texcoord > 1 || m.textures[i].transform_texcoord > 1)
      m.diagnostics.push_back("Retained texture coordinate set beyond UV1; "
                              "renderer must supply the selected set");
  for (auto it = e.begin(); it != e.end(); ++it)
    if (!m.initial_known_["extensions"]["VRMC_materials_mtoon"].contains(
            it.key()))
      m.diagnostics.push_back("Retained unmapped MToon1 field: " + it.key());
  for (auto it = j["extensions"].begin(); it != j["extensions"].end(); ++it)
    if (it.key() != "VRMC_materials_mtoon" &&
        it.key() != "KHR_materials_unlit" &&
        it.key() != "KHR_materials_emissive_strength")
      m.diagnostics.push_back(
          "Retained uninterpreted companion material extension: " + it.key());
  if (!e.contains("shadeColorFactor"))
    m.diagnostics.push_back(
        "Omitted shade color follows pinned JSON schema/UniVRM white default; "
        "prose README has a conflicting black default");
  return m;
}
Json MToonMaterial::known() const {
  const auto &p = parameters;
  Json j = Json::object();
  if (version_ == MToonVersion::V0) {
    auto &f = j["floatProperties"];
    f = {{"_ShadeShift", p.shift},
         {"_ShadeToony", p.toony},
         {"_ReceiveShadowRate", p.receive_shadow},
         {"_ShadingGradeRate", p.shading_grade},
         {"_LightColorAttenuation", p.light_color_attenuation},
         {"_IndirectLightIntensity", p.indirect_intensity},
         {"_BumpScale", p.normal_scale},
         {"_RimFresnelPower", p.rim_power},
         {"_RimLift", p.rim_lift},
         {"_RimLightingMix", p.rim_mix},
         {"_Cutoff", p.alpha_cutoff},
         {"_OutlineWidth", p.outline_width},
         {"_OutlineLightingMix", p.outline_mix},
         {"_OutlineScaledMaxDistance", p.outline_max_distance},
         {"_OutlineWidthMode", int(p.outline)},
         {"_OutlineColorMode", p.legacy_outline_fixed ? 0 : 1},
         {"_UvAnimScrollX", p.scroll[0]},
         {"_UvAnimScrollY", p.scroll[1]},
         {"_UvAnimRotation", p.rotation_speed},
         {"_MToonVersion", p.legacy_shader_version},
         {"_BlendMode", p.alpha == MToonAlpha::Opaque ? 0
                        : p.alpha == MToonAlpha::Mask ? 1
                        : p.depth_write               ? 3
                                                      : 2},
         {"_ZWrite", p.depth_write ? 1 : 0},
         {"_SrcBlend", p.source_blend},
         {"_DstBlend", p.destination_blend},
         {"_CullMode", int(p.cull)},
         {"_OutlineCullMode", int(p.outline_cull)},
         {"_AlphaToMask", p.alpha_to_coverage ? 1 : 0}};
    j["renderQueue"] = p.render_queue;
    j["keywordMap"]["_NORMALMAP"] = p.normal_enabled;
    auto &v = j["vectorProperties"];
    auto alpha = [&](const char *name, float fallback) {
      if (source_.contains("vectorProperties") &&
          source_["vectorProperties"].contains(name))
        return source_["vectorProperties"][name][3].get<float>();
      return fallback;
    };
    v["_Color"] = gamma_color(
        {p.base_color[0], p.base_color[1], p.base_color[2]}, p.base_color[3]);
    v["_ShadeColor"] = gamma_color(p.shade_color, alpha("_ShadeColor", 1));
    v["_RimColor"] = gamma_color(p.rim_color, alpha("_RimColor", 1));
    v["_OutlineColor"] =
        gamma_color(p.outline_color, alpha("_OutlineColor", 1));
    v["_EmissionColor"] = {p.emission.x, p.emission.y, p.emission.z,
                           alpha("_EmissionColor", 0)};
    j["textureProperties"] = Json::object();
    for (size_t i = 0; i < S; i++)
      if (*names0[i]) {
        auto &s = textures[i];
        if (s.index >= 0)
          j["textureProperties"][names0[i]] = s.index;
        v[names0[i]] = {s.offset[0], s.offset[1], s.scale[0], s.scale[1]};
      }
  } else {
    j["pbrMetallicRoughness"]["baseColorFactor"] = p.base_color;
    j["emissiveFactor"] = rgb(p.emission);
    j["alphaMode"] = p.alpha == MToonAlpha::Opaque ? "OPAQUE"
                     : p.alpha == MToonAlpha::Mask ? "MASK"
                                                   : "BLEND";
    j["alphaCutoff"] = p.alpha_cutoff;
    j["doubleSided"] = p.cull == MToonCull::Off;
    auto &e = j["extensions"]["VRMC_materials_mtoon"];
    e = {{"specVersion", "1.0"},
         {"transparentWithZWrite",
          p.alpha == MToonAlpha::Blend && p.depth_write},
         {"renderQueueOffsetNumber", p.queue_offset},
         {"shadeColorFactor", rgb(p.shade_color)},
         {"shadingShiftFactor", p.shift},
         {"shadingToonyFactor", p.toony},
         {"giEqualizationFactor", p.gi_equalization},
         {"parametricRimColorFactor", rgb(p.rim_color)},
         {"parametricRimFresnelPowerFactor", p.rim_power},
         {"parametricRimLiftFactor", p.rim_lift},
         {"rimLightingMixFactor", p.rim_mix},
         {"matcapFactor", rgb(p.matcap_color)},
         {"outlineColorFactor", rgb(p.outline_color)},
         {"outlineWidthMode", p.outline == MToonOutline::None ? "none"
                              : p.outline == MToonOutline::World
                                  ? "worldCoordinates"
                                  : "screenCoordinates"},
         {"outlineWidthFactor", p.outline_width},
         {"outlineLightingMixFactor", p.outline_mix},
         {"uvAnimationScrollXSpeedFactor", p.scroll[0]},
         {"uvAnimationScrollYSpeedFactor", p.scroll[1]},
         {"uvAnimationRotationSpeedFactor", p.rotation_speed}};
    j["extensions"]["KHR_materials_emissive_strength"]["emissiveStrength"] =
        p.emission_strength;
    for (size_t i = 0; i < S; i++)
      if (*names1[i] && textures[i].index >= 0) {
        auto &owner = i == 0               ? j["pbrMetallicRoughness"]
                      : (i == 2 || i == 3) ? j
                                           : e;
        auto slot = textures[i];
        if (i == 2)
          slot.strength = p.normal_scale;
        owner[names1[i]] = texture_json(slot, i == 2 || i == 6);
      }
  }
  return j;
}
void MToonMaterial::validate(size_t count) const {
  auto &p = parameters;
  for (auto x : p.base_color)
    check_range(x, 0, 1);
  for (auto c : {p.shade_color, p.emission, p.rim_color, p.matcap_color,
                 p.outline_color})
    for (float f : {c.x, c.y, c.z})
      check_range(
          f, 0,
          version_ == MToonVersion::V1 ? 1 : std::numeric_limits<float>::max());
  for (float x : {p.toony, p.gi_equalization, p.rim_mix, p.alpha_cutoff,
                  p.outline_mix, p.receive_shadow, p.shading_grade,
                  p.light_color_attenuation, p.indirect_intensity})
    check_range(x, 0, 1);
  for (float x : {p.shift, p.rim_lift, p.normal_scale, p.scroll[0], p.scroll[1],
                  p.rotation_speed, p.legacy_shader_version})
    if (!std::isfinite(x))
      throw std::invalid_argument("Nonfinite MToon scalar");
  for (float x : {p.rim_power, p.outline_width, p.outline_max_distance,
                  p.emission_strength})
    check_range(x, 0, std::numeric_limits<float>::max());
  if (int(p.alpha) < 0 || int(p.alpha) > 2 || int(p.cull) < 0 ||
      int(p.cull) > 2 || int(p.outline) < 0 || int(p.outline) > 2 ||
      int(p.outline_cull) < 0 || int(p.outline_cull) > 2 ||
      p.render_queue < 0 || p.render_queue > 5000 || p.source_blend < 0 ||
      p.source_blend > 10 || p.destination_blend < 0 ||
      p.destination_blend > 10)
    throw std::invalid_argument("Invalid MToon render state");
  if (version_ == MToonVersion::V0) {
    check_range(p.shift, -1, 1);
    check_range(p.rim_power, 0, 100);
    check_range(p.rim_lift, 0, 1);
  } else if (p.cull == MToonCull::Front || p.outline_cull != MToonCull::Front ||
             p.queue_offset < -9 || p.queue_offset > 9 ||
             (p.alpha != MToonAlpha::Blend && p.queue_offset != 0) ||
             (p.alpha == MToonAlpha::Blend &&
              (p.depth_write ? p.queue_offset < 0 : p.queue_offset > 0)))
    throw std::invalid_argument(
        "MToon1 render queue/culling violates extension semantics");
  if (version_ == MToonVersion::V1) {
    int queue = p.alpha == MToonAlpha::Opaque ? 2000
                : p.alpha == MToonAlpha::Mask ? 2450
                : p.depth_write               ? 2501
                                              : 3000;
    if (p.normal_enabled != (textures[2].index >= 0))
      throw std::invalid_argument(
          "MToon1 normal-map enabled state follows texture presence");
    if (p.render_queue != queue ||
        p.source_blend != (p.alpha == MToonAlpha::Blend ? 5 : 1) ||
        p.destination_blend != (p.alpha == MToonAlpha::Blend ? 10 : 0) ||
        p.alpha_to_coverage || (p.alpha != MToonAlpha::Blend && !p.depth_write))
      throw std::invalid_argument(
          "Edited MToon1 derived render state is inconsistent");
  } else if (p.emission_strength != 1 || p.queue_offset != 0)
    throw std::invalid_argument(
        "VRM0 has no independent emissive-strength/queue-offset parameter");
  for (size_t i = 0; i < S; i++) {
    auto &s = textures[i];
    auto encoding = i == 2 ? MToonEncoding::Normal
                    : (i == 0 || i == 1 || i == 3 || i == 4 || i == 5 ||
                       version_ == MToonVersion::V0)
                        ? MToonEncoding::Srgb
                        : MToonEncoding::Linear;
    uint8_t channel = version_ == MToonVersion::V1 ? (i == 7   ? 1
                                                      : i == 8 ? 2
                                                               : 0)
                                                   : 0;
    if (s.encoding != encoding || s.channel != channel)
      throw std::invalid_argument(
          "Texture role encoding/channel differs from source MToon version");
    if (s.texcoord > 999999999 || s.transform_texcoord > 999999999)
      throw std::invalid_argument(
          "MToon texture coordinate index exceeds semantic range");
    if (version_ == MToonVersion::V1 && i != 6 && s.strength != 1)
      throw std::invalid_argument(
          "Texture strength applies only to shading-shift slot; normal scale "
          "is a material parameter");
    if (s.index < -1 || (s.index >= 0 && size_t(s.index) >= count) ||
        s.transform_texcoord < -1)
      throw std::invalid_argument("MToon texture reference outside asset");
    for (float x : {s.offset[0], s.offset[1], s.scale[0], s.scale[1],
                    s.rotation, s.strength})
      if (!std::isfinite(x))
        throw std::invalid_argument("Nonfinite MToon texture parameter");
    if (version_ == MToonVersion::V0 &&
        (s.texcoord || s.transform_texcoord != -1 || s.rotation != 0 ||
         s.strength != 1))
      throw std::invalid_argument(
          "VRM0 texture metadata cannot represent this edited transform");
    if (version_ == MToonVersion::V0 && i == 6 && s.index >= 0)
      throw std::invalid_argument(
          "VRM0 has shading grade rather than VRM1 shift texture");
    if (version_ == MToonVersion::V1 && (i == 9 || i == 10) && s.index >= 0)
      throw std::invalid_argument(
          "VRM1 has no legacy receive-shadow/grade texture");
  }
}
Json MToonMaterial::source_json() const {
  validate();
  auto j = source_;
  patch(j, initial_known_, known());
  return j;
}

#define DOME_MTOON_FLOAT_FIELDS(X)                                             \
  X(shift)                                                                     \
  X(toony)                                                                     \
  X(gi_equalization)                                                           \
  X(rim_power)                                                                 \
  X(rim_lift)                                                                  \
  X(rim_mix)                                                                   \
  X(normal_scale) X(emission_strength) X(alpha_cutoff) X(outline_width)        \
      X(outline_mix) X(rotation_speed) X(receive_shadow) X(shading_grade)      \
          X(light_color_attenuation) X(indirect_intensity)                     \
              X(outline_max_distance) X(legacy_shader_version)
#define DOME_MTOON_BOOL_FIELDS(X)                                              \
  X(depth_write) X(alpha_to_coverage) X(legacy_outline_fixed) X(normal_enabled)
#define DOME_MTOON_INT_FIELDS(X)                                               \
  X(render_queue) X(queue_offset) X(source_blend) X(destination_blend)
#define DOME_MTOON_RGB_FIELDS(X)                                               \
  X(shade_color) X(emission) X(rim_color) X(matcap_color) X(outline_color)
Json MToonMaterial::draft_json() const {
  Json j;
  auto &p = j["parameters"];
#define FIELD(name) p[#name] = parameters.name;
  DOME_MTOON_FLOAT_FIELDS(FIELD)
  DOME_MTOON_BOOL_FIELDS(FIELD)
  DOME_MTOON_INT_FIELDS(FIELD)
#undef FIELD
#define FIELD(name) p[#name] = rgb(parameters.name);
  DOME_MTOON_RGB_FIELDS(FIELD)
#undef FIELD
  p["base_color"] = parameters.base_color;
  p["scroll"] = parameters.scroll;
  p["alpha"] = int(parameters.alpha);
  p["cull"] = int(parameters.cull);
  p["outline_cull"] = int(parameters.outline_cull);
  p["outline"] = int(parameters.outline);
  j["textures"] = Json::array();
  for (auto &s : textures)
    j["textures"].push_back({{"index", s.index},
                             {"texcoord", s.texcoord},
                             {"transform_texcoord", s.transform_texcoord},
                             {"offset", s.offset},
                             {"scale", s.scale},
                             {"rotation", s.rotation},
                             {"strength", s.strength},
                             {"encoding", int(s.encoding)},
                             {"channel", s.channel}});
  return j;
}
Json MToonMaterial::encode() const {
  return {{"format", "dome-mtoon"},
          {"version", 1},
          {"sourceVersion", int(version_)},
          {"material", source_json()},
          {"draft", draft_json()}};
}
MToonMaterial MToonMaterial::decode(const Json &j, size_t count) {
  bounded_json(j, true);
  if (!j.is_object() || j.size() != 5 ||
      j.value("format", std::string{}) != "dome-mtoon" ||
      integer(j, "version", -1, 1, 1) != 1)
    throw std::invalid_argument("Unsupported native MToon schema");
  (void)j.at("sourceVersion");
  int v = integer(j, "sourceVersion", -1, 0, 1);
  auto m = v == 0 ? from_vrm0(j.at("material"), count)
                  : from_vrm1(j.at("material"), count);
  auto &d = j.at("draft");
  if (!d.is_object() || d.size() != 2 || !d.at("parameters").is_object() ||
      d["parameters"].size() != m.draft_json()["parameters"].size() ||
      !d.at("textures").is_array() || d["textures"].size() != S)
    throw std::invalid_argument("Invalid native MToon draft dimensions");
  auto &p = d.at("parameters");
#define FIELD(name)                                                            \
  (void)p.at(#name);                                                           \
  m.parameters.name = number(p, #name, 0);
  DOME_MTOON_FLOAT_FIELDS(FIELD)
#undef FIELD
#define FIELD(name) m.parameters.name = p.at(#name).get<bool>();
  DOME_MTOON_BOOL_FIELDS(FIELD)
#undef FIELD
#define FIELD(name)                                                            \
  (void)p.at(#name);                                                           \
  m.parameters.name = integer(p, #name, 0, INT32_MIN, INT32_MAX);
  DOME_MTOON_INT_FIELDS(FIELD)
#undef FIELD
#define FIELD(name)                                                            \
  (void)p.at(#name);                                                           \
  m.parameters.name = color(p, #name, {}, false);
  DOME_MTOON_RGB_FIELDS(FIELD)
#undef FIELD
  (void)p.at("base_color");
  (void)p.at("scroll");
  m.parameters.base_color = vector4(p, "base_color", {});
  auto scroll = vector4(p, "scroll", {}, 2);
  m.parameters.scroll = {scroll[0], scroll[1]};
  for (auto name : {"alpha", "cull", "outline_cull", "outline"})
    (void)p.at(name);
  m.parameters.alpha = MToonAlpha(integer(p, "alpha", 0, 0, 2));
  m.parameters.cull = MToonCull(integer(p, "cull", 0, 0, 2));
  m.parameters.outline_cull = MToonCull(integer(p, "outline_cull", 0, 0, 2));
  m.parameters.outline = MToonOutline(integer(p, "outline", 0, 0, 2));
  for (size_t i = 0; i < S; i++) {
    auto &t = d["textures"][i];
    if (!t.is_object() || t.size() != 9)
      throw std::invalid_argument("Invalid native texture draft");
    for (auto name : {"index", "texcoord", "transform_texcoord", "offset",
                      "scale", "rotation", "strength", "encoding", "channel"})
      (void)t.at(name);
    auto &s = m.textures[i];
    s.index = integer(t, "index", -1, -1, INT32_MAX - 1);
    s.texcoord = uint32_t(integer(t, "texcoord", 0, 0, 999999999));
    s.transform_texcoord = integer(t, "transform_texcoord", -1, -1, 999999999);
    auto offset = vector4(t, "offset", {}, 2),
         scale = vector4(t, "scale", {}, 2);
    s.offset = {offset[0], offset[1]};
    s.scale = {scale[0], scale[1]};
    s.rotation = number(t, "rotation", 0);
    s.strength = number(t, "strength", 1);
    s.encoding = MToonEncoding(integer(t, "encoding", 0, 0, 2));
    s.channel = uint8_t(integer(t, "channel", 0, 0, 3));
  }
  m.validate(count);
  return m;
}
#undef DOME_MTOON_FLOAT_FIELDS
#undef DOME_MTOON_BOOL_FIELDS
#undef DOME_MTOON_INT_FIELDS
#undef DOME_MTOON_RGB_FIELDS
MToonRenderState MToonMaterial::render_state(bool outline) const {
  validate();
  auto &p = parameters;
  MToonRenderState state;
  state.cull = outline ? p.outline_cull : p.cull;
  state.depth_write = p.depth_write;
  state.alpha_to_coverage = p.alpha_to_coverage;
  state.blend = p.alpha == MToonAlpha::Blend || p.source_blend != 1 ||
                p.destination_blend != 0;
  state.source_rgb = p.source_blend;
  state.destination_rgb = p.destination_blend;
  state.source_alpha = version_ == MToonVersion::V0 ? p.source_blend : 1;
  state.destination_alpha = p.destination_blend;
  state.maximum_alpha = version_ == MToonVersion::V0;
  state.queue =
      p.render_queue + (version_ == MToonVersion::V1 ? p.queue_offset : 0);
  return state;
}
MToonGpuParameters MToonMaterial::gpu_parameters() const {
  validate();
  auto &p = parameters;
  MToonGpuParameters u{};
  u[0] = p.base_color;
  u[1] = {p.shade_color.x, p.shade_color.y, p.shade_color.z, p.shift};
  u[2] = {p.emission.x * p.emission_strength,
          p.emission.y * p.emission_strength,
          p.emission.z * p.emission_strength, p.toony};
  u[3] = {p.rim_color.x, p.rim_color.y, p.rim_color.z, p.rim_power};
  u[4] = {p.matcap_color.x, p.matcap_color.y, p.matcap_color.z, p.rim_lift};
  u[5] = {p.outline_color.x, p.outline_color.y, p.outline_color.z,
          p.outline_mix};
  u[6] = {p.gi_equalization, p.rim_mix, p.normal_scale, p.alpha_cutoff};
  u[7] = {p.receive_shadow, p.shading_grade, p.light_color_attenuation,
          p.indirect_intensity};
  u[8] = {float(int(version_)), float(int(p.alpha)), float(int(p.cull)),
          p.legacy_outline_fixed ? 1.f : 0.f};
  u[9] = {p.scroll[0], p.scroll[1], p.rotation_speed, p.outline_width};
  u[10] = {float(int(p.outline)), p.outline_max_distance,
           float(int(p.outline_cull)), textures[6].strength};
  u[11][0] = p.normal_enabled ? 1.f : 0.f;
  return u;
}
Vec3 mtoon_facing_normal(MToonVersion v, Vec3 n, Vec3 view, bool front,
                         bool outline) {
  if (!finite(n) || !finite(view) || length(n) < 1e-12f ||
      length(view) < 1e-12f)
    throw std::invalid_argument("Invalid MToon normal/view");
  n = unit(n);
  view = unit(view);
  if (v == MToonVersion::V0) {
    if (dot(n, view) < 0)
      n = n * -1;
    if (outline)
      n = n * -1;
  } else if (!front)
    n = n * -1;
  return n;
}
Vec3 mtoon_normal_map(Vec3 encoded, float scale) {
  for (float x : {encoded.x, encoded.y, encoded.z})
    check_range(x, 0, 1);
  if (!std::isfinite(scale))
    throw std::invalid_argument("Invalid normal map scale");
  auto n = Vec3{(encoded.x * 2 - 1) * scale, (encoded.y * 2 - 1) * scale,
                encoded.z * 2 - 1};
  if (length(n) < 1e-12f)
    return {0, 0, 1};
  return unit(n);
}
Vec3 mtoon_shading_normal(MToonVersion version, Vec3 geometric,
                          std::array<float, 4> tangent, Vec3 encoded,
                          float scale, Vec3 view, bool front, bool outline) {
  auto n = unit(geometric);
  Vec3 t = unit({tangent[0], tangent[1], tangent[2]});
  if (!std::isfinite(tangent[3]) || std::abs(std::abs(tangent[3]) - 1) > 1e-5f)
    throw std::invalid_argument("Invalid tangent handedness");
  if (version == MToonVersion::V1 && !front)
    n = n * -1;
  t = unit(t - n * dot(t, n));
  auto b = cross(n, t) * tangent[3];
  auto ts = mtoon_normal_map(encoded, scale);
  auto mapped = unit(t * ts.x + b * ts.y + n * ts.z);
  return version == MToonVersion::V0
             ? mtoon_facing_normal(version, mapped, view, front, outline)
             : mapped;
}
Vec3 mtoon_material_normal(const MToonMaterial &m, Vec3 geo,
                           std::array<float, 4> tangent, Vec3 encoded,
                           Vec3 view, bool front, bool outline) {
  m.validate();
  return m.parameters.normal_enabled
             ? mtoon_shading_normal(m.version(), geo, tangent, encoded,
                                    m.parameters.normal_scale, view, front,
                                    outline)
             : mtoon_facing_normal(m.version(), geo, view, front, outline);
}
MToonFragment mtoon_shade(const MToonMaterial &m, const MToonSamples &s,
                          const MToonLighting &l) {
  m.validate();
  auto &p = m.parameters;
  for (auto v : {l.normal, l.view, l.light_direction, l.light_color, l.gi,
                 l.gi_up, l.gi_down, s.shade, s.emission, s.rim, s.matcap})
    if (!finite(v))
      throw std::invalid_argument("Nonfinite MToon lighting sample");
  for (auto a : s.base)
    check_range(a, 0, std::numeric_limits<float>::max());
  check_range(s.base[3], 0, 1);
  for (float x : {l.shadow, s.receive_shadow, s.shading_grade})
    check_range(x, 0, 1);
  if (!std::isfinite(s.shift))
    throw std::invalid_argument("Nonfinite shading shift sample");
  auto n = l.normal_oriented
               ? unit(l.normal)
               : mtoon_facing_normal(m.version(), l.normal, l.view,
                                     l.front_facing, l.outline_pass);
  auto view = unit(l.view), light = unit(l.light_direction);
  float dotnl = dot(n, light), alpha = p.base_color[3] * s.base[3];
  MToonFragment out;
  if (p.alpha == MToonAlpha::Mask && alpha < p.alpha_cutoff) {
    out.discard = true;
    return out;
  }
  if (p.alpha != MToonAlpha::Blend)
    alpha = 1;
  Vec3 lit = mul({p.base_color[0], p.base_color[1], p.base_color[2]},
                 {s.base[0], s.base[1], s.base[2]}),
       shade = mul(p.shade_color, s.shade),
       uniform = (l.gi_up + l.gi_down) * .5f;
  Vec3 lighting = l.light_color, gi;
  float ratio;
  if (m.version() == MToonVersion::V0) {
    float attenuation =
        l.shadow * ((1 - p.receive_shadow * s.receive_shadow) +
                    l.shadow * p.receive_shadow * s.receive_shadow);
    float grade = 1 - p.shading_grade * (1 - s.shading_grade);
    float intensity = (dotnl * .5f + .5f) * attenuation * grade * 2 - 1;
    float high = 1 * (1 - p.toony) + p.shift * p.toony;
    ratio = sat((intensity - p.shift) / std::max(eps, high - p.shift));
    lighting = desaturate(lighting, p.light_color_attenuation);
    gi = desaturate(mix(uniform, l.gi, p.indirect_intensity),
                    p.light_color_attenuation);
  } else {
    float x = dotnl + p.shift + s.shift * m.textures[6].strength;
    float width = 1 - p.toony;
    ratio =
        width <= 1e-7f ? (x >= 0 ? 1.f : 0.f) : sat((x + width) / (2 * width));
    lighting = lighting * l.shadow;
    gi = mix(l.gi, uniform, p.gi_equalization);
  }
  Vec3 color = mul(mix(shade, lit, ratio), lighting) + mul(gi, lit);
  if (m.version() == MToonVersion::V0)
    color = minimum(color, lit);
  if (m.version() == MToonVersion::V1 || !l.outline_pass) {
    float rim = std::pow(sat(1 - dot(n, view) + p.rim_lift),
                         std::max(p.rim_power, eps));
    Vec3 rim_light =
        mix({1, 1, 1}, lighting + (m.version() == MToonVersion::V0 ? gi : l.gi),
            p.rim_mix);
    Vec3 parametric = mul(p.rim_color, s.rim) * rim;
    Vec3 matcap = mul(p.matcap_color, s.matcap);
    if (m.version() == MToonVersion::V1)
      color += mul(mul(matcap + p.rim_color * rim, s.rim), rim_light);
    else
      color += mul(parametric, rim_light) + matcap;
    color += mul(p.emission, s.emission) * p.emission_strength;
  }
  if (l.outline_pass) {
    if (m.version() == MToonVersion::V0 && p.legacy_outline_fixed)
      color = p.outline_color;
    else
      color = mul(p.outline_color, mix({1, 1, 1}, color, p.outline_mix));
  }
  out.color = {color.x, color.y, color.z, alpha};
  for (auto x : out.color)
    if (!std::isfinite(x))
      throw std::runtime_error("MToon lighting overflow");
  return out;
}
std::array<float, 2> mtoon_uv(const MToonMaterial &m, MToonSlot role,
                              std::array<float, 2> uv, double seconds,
                              float mask) {
  m.validate();
  if (!std::isfinite(seconds) || !std::isfinite(mask))
    throw std::invalid_argument("Invalid MToon UV animation");
  if (size_t(role) >= S)
    throw std::invalid_argument("Unknown MToon texture role");
  check_range(mask, 0, 1);
  auto &p = m.parameters;
  auto &s = m.textures[size_t(role)];
  auto &main = m.textures[0];
  double x = uv[0], y = uv[1], t = seconds * mask;
  auto transform = [&](const MToonTexture &a) {
    double xx = x * a.scale[0], yy = y * a.scale[1], c = std::cos(a.rotation),
           si = std::sin(a.rotation);
    x = c * xx - si * yy + a.offset[0];
    y = si * xx + c * yy + a.offset[1];
  };
  if (role == MToonSlot::Matcap) {
    if (m.version() == MToonVersion::V1)
      transform(s);
    return {float(x), float(y)};
  }
  bool animate =
      role != MToonSlot::UvMask &&
      !(m.version() == MToonVersion::V0 && role == MToonSlot::OutlineWidth);
  if (m.version() == MToonVersion::V0) {
    transform(main);
    if (animate) {
      y = 1 - y;
      x += p.scroll[0] * t;
      y += p.scroll[1] * t;
      double a = std::remainder(double(p.rotation_speed) * 2 * pi * t, 2 * pi),
             c = std::cos(a), si = std::sin(a), xx = x - .5, yy = y - .5;
      x = c * xx - si * yy + .5;
      y = 1 - (si * xx + c * yy + .5);
    }
  } else {
    if (animate) {
      double a = std::remainder(double(p.rotation_speed) * t, 2 * pi),
             c = std::cos(a), si = std::sin(a), xx = x - .5, yy = y - .5;
      x = c * xx + si * yy + .5 + p.scroll[0] * t;
      y = -si * xx + c * yy + .5 + p.scroll[1] * t;
    }
    transform(s);
  }
  if (!std::isfinite(x) || !std::isfinite(y) ||
      std::abs(x) > std::numeric_limits<float>::max() ||
      std::abs(y) > std::numeric_limits<float>::max())
    throw std::runtime_error("MToon UV overflow");
  return {float(x), float(y)};
}
std::array<float, 2> mtoon_matcap_uv(Vec3 n, Vec3 v, Vec3 up) {
  if (!finite(n) || !finite(v) || !finite(up) || length(n) < 1e-12f ||
      length(v) < 1e-12f)
    throw std::invalid_argument("Invalid matcap frame");
  n = unit(n);
  v = unit(v);
  up = unit(up);
  auto right = cross(v, up);
  if (length(right) < 1e-6f)
    right = cross(v, std::abs(v.x) < .8f ? Vec3{1, 0, 0} : Vec3{0, 0, 1});
  right = unit(right);
  auto u = unit(cross(right, v));
  return {.5f - dot(right, n) * .5f, .5f - dot(u, n) * .5f};
}
float mtoon_outline_world_width(const MToonMaterial &m, float sample) {
  m.validate();
  check_range(sample, 0, 1);
  return m.parameters.outline == MToonOutline::World
             ? m.parameters.outline_width * sample *
                   (m.version() == MToonVersion::V0 ? .01f : 1.f)
             : 0;
}
std::array<float, 2> mtoon_outline_clip_offset(const MToonMaterial &m,
                                               std::array<float, 2> projected,
                                               Vec3 n, float w, float aspect,
                                               float sample) {
  m.validate();
  check_range(sample, 0, 1);
  if (!finite(n) || !std::isfinite(w) || w <= 0 || !std::isfinite(aspect) ||
      aspect <= 0)
    throw std::invalid_argument("Invalid outline projection");
  if (m.parameters.outline != MToonOutline::Screen)
    return {};
  if (!std::isfinite(projected[0]) || !std::isfinite(projected[1]))
    throw std::invalid_argument("Nonfinite projected outline normal");
  float len = std::hypot(projected[0], projected[1]);
  if (len < 1e-12f)
    return {};
  float width = m.parameters.outline_width * sample;
  if (m.version() == MToonVersion::V0)
    width *= .01f * std::min(w, m.parameters.outline_max_distance) *
             sat(1 - std::abs(normalized(n).z));
  else
    width *= 2 * w;
  return {projected[0] / len * width / aspect, projected[1] / len * width};
}
std::string_view mtoon_glsl() { return mtoon_shader_source; }
} // namespace dome
