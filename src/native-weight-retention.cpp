// SPDX-License-Identifier: GPL-3.0-or-later
#include "native-weight-retention.hpp"
#include "dome/normal-sampling.hpp"
#include "dome/texture-sets.hpp"

namespace dome::detail::native_weight_retention_impl {
void Counter::baked(const std::shared_ptr<const BakedTextures> &paint) {
  if (!enter(paint)) return;
  const auto &maps = paint->maps;
  unsupported(maps.overlay, "Native weight input cannot retain render-only Bake overlays");
  unsupported(maps.current_grid_preview, "Native weight input cannot retain render-only Bake previews");
  unsupported(maps.polygon_surface, "Native weight input needs the retained polygon Bake closure");
  unsupported(maps.baked_base, "Native weight input cannot retain nested Bake textures");
  unsupported(maps.color_projection, "Native weight input needs the prepared Bake colour closure");
  unsupported(maps.normal_projection, "Native weight input needs the prepared Bake normal closure");
  unsupported(paint->normal_field, "Native weight input needs the independent baked normal closure");
  add(sizeof(*paint));
  for (const auto *plane : {&maps.color, &maps.metal_rough, &maps.emission, &maps.normal,
                            &maps.color_coverage, &maps.baked_normal, &maps.baked_coverage,
                            &maps.emission_coverage, &maps.pbr_color, &maps.mtoon_normal})
    vector(plane->rgba);
  this->maps(maps.source_maps);
  vector(paint->coverage);
  const auto &normal = paint->normal_authority;
  if (enter(normal)) {
    add(sizeof(*normal));
    image(normal->pbr);
    image(normal->mtoon);
    vector(normal->coverage);
    leave(normal);
  }
  const auto &emission = paint->emission_field;
  if (enter(emission)) {
    add(sizeof(*emission));
    vector(emission->values_);
    vector(emission->varying_);
    vector(emission->coverage_words_);
    leave(emission);
  }
  leave(paint);
}

void Counter::texture_domain(const std::shared_ptr<const TextureSetFaceDomain> &domain) {
  if (!enter(domain)) return;
  domain->authenticate_owner(domain);
  add(sizeof(*domain));
  vector(domain->triangles_);
  shared_vector(domain->assignments_);
  const auto &topology = domain->topology_;
  if (enter(topology)) {
    add(sizeof(*topology));
    vector(topology->faces_);
    vector(topology->corners_);
    mesh(topology->geometry_);
    leave(topology);
  }
  leave(domain);
}

void Counter::texture_sets(const std::shared_ptr<const TextureSetCollection> &sets) {
  if (!enter(sets)) return;
  unsupported(sets->primary_source_, "Native weight input needs the retained texture-set source closure");
  add(sizeof(*sets));
  vector(sets->additional_);
  shared_vector(sets->slots_);
  texture_domain(sets->primary_);
  if (!sets->primary_ || sets->primary_->topology_ != sets->topology_)
    throw std::invalid_argument("Native weight input lost the primary texture-set topology");
  for (const auto &field : sets->additional_) {
    unsupported(field.source, "Native weight input needs the retained texture-set source closure");
    text(field.name);
    text(field.material.name);
    texture_domain(field.domain);
    paint(field.painted);
    appearance(field.appearance);
    material(field.material_bindings);
    bake_recipe(field.bake_recipe);
  }
  leave(sets);
}
} // namespace dome::detail::native_weight_retention_impl
