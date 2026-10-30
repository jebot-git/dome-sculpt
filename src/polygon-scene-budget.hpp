// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scene-feature-version.hpp"
#include "volume-archive.hpp"
#include <nlohmann/json.hpp>
namespace dome::detail {
// A Scene12 aggregate preflight runs before any Volume, SurfaceMap, mesh, skin
// or retained source object is decoded. Every serialized payload is charged;
// sharing is represented by bounded table references, not an assumed alias.
struct PolygonSceneBudget {
  size_t samples = 0, pixels = 0, paint_bytes = 0, deformations = 0, remaps = 0,
         retopo_entries = 0, retopo_bytes = 0, retained_vertices = 0;
  static void add(size_t &n, size_t amount, size_t maximum,
                  const char *message) {
    if (n > maximum || amount > maximum - n)
      throw std::length_error(message);
    n += amount;
  }
  void volume(const nlohmann::json &v) {
    archive_detail::volume_header(v);
    size_t layers = 0;
    if (v.contains("sculpt_layers")) {
      const auto &ls = v.at("sculpt_layers");
      if (!ls.is_array() || ls.size() > 16)
        throw std::invalid_argument("Invalid retained polygon sculpt layers");
      for (const auto &l : ls) {
        const auto &d = l.at("delta");
        if (!d.is_array())
          throw std::invalid_argument("Invalid retained polygon sculpt layer");
        add(layers, d.size(), 4'000'000, "Scene sculpt layer budget exceeded");
      }
    }
    size_t n = std::max(layers, v.at("samples").size());
    if (v.contains("sculpt_mask"))
      add(n, v.at("sculpt_mask").at("values").size(), 4'000'000,
          "Scene sculpt mask budget exceeded");
    add(samples, n, 4'000'000,
        "Scene retained polygon sculpt storage exceeds budget");
  }
  void paint(const nlohmann::json &p, bool retained=false) {
    if (p.is_null())
      return;
    const auto &r = p.at("resolution");
    if (!r.is_number_unsigned() &&
        (!r.is_number_integer() || r.get<int64_t>() < 0))
      throw std::invalid_argument("Invalid polygon paint resolution");
    auto n = r.get<uint64_t>();
    if (n < 64 || n > 2048 || (n & (n - 1)))
      throw std::invalid_argument("Invalid polygon paint resolution");
    add(pixels, size_t(n) * n, 16 * 1024 * 1024,
        "Scene retained paint-map budget exceeded");
    const auto &ls = p.at("layers");
    if (!ls.is_array() || ls.empty() || ls.size() > 16)
      throw std::invalid_argument("Invalid polygon paint layers");
    for (const auto &l : ls) {
      const auto &t = l.at("tiles");
      if (!t.is_array() || t.size() > 4096)
        throw std::invalid_argument("Invalid polygon paint tile count");
      add(paint_bytes, t.size() * sizeof(PaintTile), 256 * 1024 * 1024,
          "Scene retained paint storage exceeded");
      if (l.contains("pbr_color_tiles")) {
        const auto &pbr = l.at("pbr_color_tiles");
        if (!pbr.is_array() || pbr.size() > 4096)
          throw std::invalid_argument("Invalid polygon PBR color tile count");
        add(paint_bytes, pbr.size() * sizeof(PaintColorTile),
            256 * 1024 * 1024, "Scene retained PBR paint storage exceeded");
      }
    }
    const auto &mesh = p.at("mesh");
    if(retained && mesh.at("positions").is_binary())
      add(retained_vertices,mesh.at("positions").get_binary().size()/12,8'000'000,
          "Scene retained polygon/UV geometry exceeds budget");
    for (const auto &item : mesh.items())
      if (item.value().is_binary())
        add(paint_bytes, item.value().get_binary().size(), 256 * 1024 * 1024,
            "Scene retained mesh storage exceeded");
    if (p.contains("baked"))
      for (const auto *key :
           {"color", "orm", "emission", "normal", "coverage"}) {
        const auto &b = p.at("baked").at(key);
        if (!b.is_binary())
          throw std::invalid_argument("Invalid retained baked buffer");
        add(paint_bytes, b.get_binary().size(), 256 * 1024 * 1024,
            "Scene retained baked storage exceeded");
      }
    if (p.contains("baked") && p.at("baked").contains("pbr_color")) {
      const auto &pbr = p.at("baked").at("pbr_color");
      if (!pbr.is_binary())
        throw std::invalid_argument("Invalid retained baked PBR buffer");
      add(paint_bytes, pbr.get_binary().size(), 256 * 1024 * 1024,
          "Scene retained baked PBR storage exceeded");
    }
    if(p.contains("baked")&&p.at("baked").contains("mtoon_normal")){
      const auto &bytes=p.at("baked").at("mtoon_normal");if(!bytes.is_binary())throw std::invalid_argument("Invalid retained prepared MToon normal buffer");
      add(paint_bytes,bytes.get_binary().size(),256*1024*1024,"Scene retained normal bake storage exceeded");
    }
  }
  void deformation(const nlohmann::json &d, unsigned version) {
    if (d.is_null())
      return;
    if (minimum_deformation_scene_version(d) > version)
      throw std::invalid_argument(
          "Retained authored inverse binds require Scene11");
    const auto &p = d.at("pose"), &w = d.at("morph_weights");
    if (!p.is_array() || p.size() > 256 || !w.is_array() || w.size() > 128)
      throw std::invalid_argument(
          "Invalid retained deformation preview dimensions");
    auto add_entries = [&](size_t n) {
      add(deformations, n, 8'000'000,
          "Scene retained deformation budget exceeded");
    };
    add_entries(p.size());
    add_entries(w.size());
    if (!d.at("skin").is_null()) {
      const auto &rows = d.at("skin").at("vertices");
      if (!rows.is_array() || rows.size() > 2'000'000)
        throw std::length_error("Invalid retained skin rows");
      for (const auto &v : rows) {
        if (!v.is_array() || v.size() > 32)
          throw std::length_error("Invalid retained skin influences");
        add_entries(v.size());
      }
    }
    if (!d.at("morphs").is_null()) {
      const auto &ts = d.at("morphs").at("targets");
      if (!ts.is_array() || ts.size() > 128)
        throw std::length_error("Invalid retained morph targets");
      for (const auto &t : ts) {
        const auto &v = t.at("delta");
        if (!v.is_array() || v.size() > 2'000'000)
          throw std::length_error("Invalid retained morph rows");
        add_entries(v.size());
      }
    }
  }
  void appearance(const nlohmann::json &a) {
    if (a.is_null())
      return;
    const auto &v = a.at("source_vertices");
    if (!v.is_binary() || v.get_binary().size() % 4)
      throw std::invalid_argument("Invalid retained polygon source remap");
    add(remaps, v.get_binary().size() / 4, 8'000'000,
        "Scene retained source remap budget exceeded");
  }
  void retopology(const nlohmann::json &r) {
    if (r.is_null())
      return;
    if (!r.is_object() || r.size() != 2 || !r.at("topology").is_string())
      throw std::invalid_argument("Invalid retained polygon retopology");
    const auto &s = r.at("topology").get_ref<const std::string &>();
    add(retopo_bytes, s.size(), 64 * 1024 * 1024,
        "Scene retained retopology payload exceeded");
    auto j = nlohmann::json::parse(
        s, [](int depth, nlohmann::json::parse_event_t, nlohmann::json &) {
          if (depth > 32)
            throw std::length_error("Retained retopology nesting exceeded");
          return true;
        });
    if (!j.is_object() || j.size() != 5 || j.at("version") != 1)
      throw std::invalid_argument("Invalid retained retopology topology");
    for (const auto *key : {"vertices", "edges", "faces"}) {
      const auto &v = j.at(key);
      if (!v.is_array() ||
          v.size() > (std::string_view(key) == "edges" ? 300'000 : 100'000))
        throw std::length_error("Retained retopology record budget exceeded");
      add(retopo_entries, v.size(), 1'000'000,
          "Scene retained retopology budget exceeded");
    }
  }
};
} // namespace dome::detail
