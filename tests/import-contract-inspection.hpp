// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "dome/paint.hpp"
#include "dome/source-maps.hpp"
#include "dome/material-bindings.hpp"
#include "dome/normal-frames.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <utility>

namespace import_contract {
using Json = nlohmann::json;
inline Json maps(const dome::PaintedMesh &paint) {
  const auto source = paint.source_maps();
  if (!source) return nullptr;
  source->validate_surface(paint.mesh());
  Json result{{"empty", source->empty()}, {"uv1", bool(source->uv1)},
              {"extra_uv_sets", Json::array()}, {"roles", Json::object()}};
  for (const auto &[set, values] : source->extra_uv_sets)
    result["extra_uv_sets"].push_back(set);
  const std::pair<const char *, const dome::PaintSourceTexture *> roles[]{
      {"color", &source->color}, {"metal_rough", &source->metal_rough},
      {"emission", &source->emission}, {"normal", &source->normal},
      {"occlusion", &source->occlusion}};
  for (const auto &[name, view] : roles) {
    Json role{{"present", bool(view->image)}, {"texcoord", view->texcoord},
              {"uv_transform", view->uv_transform},
              {"sampler", {view->wrap_s, view->wrap_t,
                            view->min_filter, view->mag_filter}}};
    if (view->image) {
      role["coordinate_count"] = dome::source_texture_coordinates(
          paint.mesh(), *source, view->texcoord).size();
      role["image_size"] = {view->image->width, view->image->height};
    }
    result["roles"][name] = std::move(role);
  }
  return result;
}
// This is an observer of actual decoded native owners, never an admission
// receipt or a JSON marker used to grant authority. Existing --inspect stays
// byte-for-byte the ordinary Scene encoder interface.
inline Json inspect(const dome::Scene &scene) {
  Json result{{"scene", scene.encode()}, {"native_objects", Json::object()}};
  for (const auto &object : scene.objects()) {
    Json facts{{"mesh_active", object.mesh_active}};
    if (object.painted) {
      const auto &paint = *object.painted;
      const auto before = paint.source_sampling_original();
      facts["resolution"] = paint.resolution();
      facts["strict"] = paint.surface_diagnostics().policy == dome::PaintUvPolicy::Strict;
      facts["prepared"] = paint.source_sampling_prepared();
      facts["valid_source_owner"] = paint.valid_source_sampling_attachment();
      facts["maps"] = maps(paint);
      facts["before_maps"] = before ? maps(*before) : Json(nullptr);
      facts["vertices"] = paint.mesh().positions.size();
      if (before) {
        const auto remap = paint.source_sampling_vertices();
        bool geometry = remap.size() == paint.mesh().positions.size();
        for (size_t v = 0; geometry && v < remap.size(); ++v) {
          geometry = remap[v] < before->mesh().positions.size();
          if (!geometry) break;
          const auto a = paint.mesh().positions[v];
          const auto b = before->mesh().positions[remap[v]];
          geometry = a.x == b.x && a.y == b.y && a.z == b.z;
        }
        facts["original_geometry_exact"] = geometry;
        bool normal = false, image = false;
        if (geometry && paint.source_maps() && before->source_maps() &&
            paint.source_maps()->normal.image && before->source_maps()->normal.image) {
          const auto current = dome::source_texture_coordinates(
              paint.mesh(), *paint.source_maps(), paint.source_maps()->normal.texcoord);
          const auto original = dome::source_texture_coordinates(
              before->mesh(), *before->source_maps(), before->source_maps()->normal.texcoord);
          normal = current.size() == remap.size();
          for (size_t v = 0; normal && v < remap.size(); ++v)
            normal = remap[v] < original.size() && current[v] == original[remap[v]];
          image = paint.source_maps()->normal.image == before->source_maps()->normal.image &&
                  paint.source_maps()->normal_scale == before->source_maps()->normal_scale;
        }
        facts["original_normal_coordinates_exact"] = normal;
        facts["original_normal_image_strength_exact"] = image;
      }
    }
    if (const auto binding = object.material_bindings) {
      facts["sampling_reparameterized"] = binding->sampling_reparameterized;
      facts["normal_frames"] = bool(binding->normal_frames);
      facts["normal_pbr_frames"] = binding->normal_frames && bool(binding->normal_frames->pbr);
      facts["has_mtoon"] = binding->has_mtoon;
      facts["normal_mtoon_frames"] = binding->normal_frames && bool(binding->normal_frames->mtoon);
      if (binding->uv_origin) {
        bool images = true;
        for (size_t slot = 0; slot < binding->roles.size(); ++slot)
          images &= binding->roles[slot].image == binding->uv_origin->roles[slot].image;
        facts["mtoon_images_retained"] = images;
      }
      facts["origin_maps"] = binding->uv_origin && binding->uv_origin->paint
          ? maps(*binding->uv_origin->paint) : Json(nullptr);
    }
    result["native_objects"][std::to_string(object.id)] = std::move(facts);
  }
  return result;
}
} // namespace import_contract
