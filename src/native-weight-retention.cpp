// SPDX-License-Identifier: GPL-3.0-or-later
#include "polygon-surface-preview.hpp"
#include "native-weight-retention.hpp"
#include "baked-normal-field.hpp"
#include "canonical-normal-uv.hpp"
#include "canonical-paint-sampling.hpp"
#include "dome/color-paint-sampling.hpp"
#include "dome/normal-sampling.hpp"
#include "dome/polygon-color.hpp"
#include "dome/texture-sets.hpp"
#include "native-paint-frame-source.hpp"
#include "native-paint-sampling.hpp"
#include "normal-sampling-access.hpp"
#include "texture-set-field-source.hpp"

namespace dome::detail::native_weight_retention_impl {
template<class Layout> void Counter::uv_layout(const std::shared_ptr<const Layout> &layout) {
  if (!enter(layout)) return;
  add(sizeof(*layout));
  vector(layout->source_vertices);
  vector(layout->indices);
  vector(layout->uv);
  vector(layout->pins);
  vector(layout->seams);
  leave(layout);
}

void Counter::paint_surface(const NativeWeightPaintAccess::Surface &surface) {
  if (!enter(surface.owner)) return;
  add(surface.bytes);
  if (enter(surface.raster)) {
    add(surface.raster_bytes);
    leave(surface.raster);
  }
  mesh(surface.geometry);
  texture_domain(surface.membership);
  sampling_domain(surface.domain);
  if (enter(surface.original_sampling)) {
    add(surface.original_bytes);
    paint(surface.before);
    maps(surface.original_maps);
    baked(surface.original_bake);
    for (const auto &uv : surface.original_uvs) shared_vector(uv);
    leave(surface.original_sampling);
  }
  leave(surface.owner);
}

void Counter::sampling_domain(const std::shared_ptr<const PaintSamplingDomain> &domain) {
  if (!enter(domain)) return;
  add(sizeof(*domain));
  mesh(domain->basis_);
  mesh(domain->derived_);
  vector(domain->source_corners_);
  leave(domain);
}

void Counter::attachment(const std::shared_ptr<const PaintSamplingAttachment> &value) {
  if (!enter(value)) return;
  add(sizeof(*value));
  paint_surface(NativeWeightPaintAccess::surface(*value));
  leave(value);
}

void Counter::color_projection(const std::shared_ptr<const PaintColorProjection> &color) {
  if (!enter(color)) return;
  add(sizeof(*color));
  image(color->vertex_rgba);
  leave(color);
}

void Counter::normal_detail(const std::shared_ptr<const NormalDetailAuthority> &normal) {
  if (!enter(normal)) return;
  add(sizeof(*normal));
  image(normal->pbr);
  image(normal->mtoon);
  vector(normal->coverage);
  leave(normal);
}

void Counter::normal_projection(const std::shared_ptr<const PaintNormalProjection> &normal) {
  if (!enter(normal)) return;
  add(sizeof(*normal));
  image(normal->pbr);
  image(normal->mtoon);
  const auto &domain = normal->sampling_domain;
  if (enter(domain)) {
    add(sizeof(*domain));
    vector(domain->coordinates);
    vector(domain->mtoon_coordinates);
    vector(domain->indices);
    image(domain->resolved_pbr);
    image(domain->resolved_mtoon);
    normal_detail(domain->detail);
    leave(domain);
  }
  leave(normal);
}

void Counter::baked_normal(const std::shared_ptr<const BakedNormalField> &field) {
  if (!enter(field)) return;
  add(sizeof(*field));
  canonical_conversion(field->origin_);
  baked_normal(field->prior_);
  image(field->pbr_);
  image(field->mtoon_);
  shared_vector(field->coverage_);
  image(field->write_pbr_);
  image(field->write_mtoon_);
  shared_vector(field->write_coverage_);
  leave(field);
}

void Counter::baked(const std::shared_ptr<const BakedTextures> &paint) {
  if (!enter(paint)) return;
  const auto &maps = paint->maps;
  unsupported(maps.overlay, "Native weight input cannot retain render-only Bake overlays");
  unsupported(maps.current_grid_preview, "Native weight input cannot retain render-only Bake previews");
  unsupported(maps.baked_base, "Native weight input cannot retain nested Bake textures");
  add(sizeof(*paint));
  for (const auto *plane : {&maps.color, &maps.metal_rough, &maps.emission, &maps.normal,
                            &maps.color_coverage, &maps.baked_normal, &maps.baked_coverage,
                            &maps.emission_coverage, &maps.pbr_color, &maps.mtoon_normal})
    vector(plane->rgba);
  this->maps(maps.source_maps);
  polygon_preview(maps.polygon_surface);
  color_projection(maps.color_projection);
  normal_projection(maps.normal_projection);
  vector(paint->coverage);
  normal_detail(paint->normal_authority);
  baked_normal(paint->normal_field);
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

void Counter::material(const std::shared_ptr<const SourceMaterialBindings> &binding) {
  if (!enter(binding)) return;
  add(sizeof(*binding));
  toon(binding->material);
  for (const auto &role : binding->roles) image(role.image);
  frame_set(binding->normal_frames);
  uv_origin(binding->uv_origin);
  canonical_conversion(binding->canonical_attributes);
  canonical_uv(binding->canonical_uv);
  native_sampling(binding->native_paint_sampling);
  native_frames(binding->native_paint_frames);
  color_preparation(binding->color_preparation);
  normal_preparation(binding->normal_preparation);
  derived_key(binding->key_derivation);
  canonical_sampling(binding->canonical_paint_sampling);
  polygon_owner(binding->polygon_surface_transfer);
  leave(binding);
}

void Counter::position_domain(const std::shared_ptr<const FramePositionDomain> &domain) {
  if (!enter(domain)) return;
  add(sizeof(*domain));
  for (const auto &geometry : {domain->basis(), domain->before_basis(),
                               domain->current_geometry(), domain->source_frame_basis()})
    mesh(geometry);
  vector(domain->current_vertices());
  vector(domain->origin_vertices());
  vector(domain->source_frame_vertices());
  position_domain(domain->previous());
  leave(domain);
}

void Counter::frame_set(const std::shared_ptr<const NormalFrameSet> &value) {
  if (!enter(value)) return;
  add(sizeof(*value));
  frames(value->pbr);
  frames(value->mtoon);
  frames(value->canonical);
  vector(value->targets);
  for (const auto &target : value->targets) vector(target.positions);
  vector(value->current_targets);
  position_domain(value->position_domain);
  leave(value);
}

void Counter::derived_key(const std::shared_ptr<const SourceKeyDerivation> &key) {
  if (!enter(key)) return;
  if (!key->valid_owner(key)) throw std::invalid_argument("Native weight input lost its derived shapekey owner");
  add(sizeof(*key));
  paint(key->before_paint());
  material(key->before_material());
  deformation(key->before_deformation());
  appearance(key->before_appearance());
  position_domain(key->position_domain());
  frame_set(key->frames());
  vector(key->targets());
  vector(key->current_targets_);
  leave(key);
}

void Counter::native_frames(const std::shared_ptr<const NativePaintFrameSource> &value) {
  if (!enter(value)) return;
  if (!value->valid_owner(value)) throw std::invalid_argument("Native weight input lost its native frame owner");
  add(sizeof(*value));
  paint(value->before_paint());
  material(value->before_material());
  uv_origin(value->origin());
  frame_set(value->base_frames());
  frame_set(value->frames());
  derived_key(value->keys());
  vector(value->metadata_);
  for (const auto &role : value->role_owners_) frames(role);
  for (const auto &domain : value->domain_owners_) position_domain(domain);
  leave(value);
}

void Counter::normal_sampling(const std::shared_ptr<const SourceNormalSampling> &value) {
  if (!enter(value)) return;
  add(sizeof(*value));
  const auto [seal, bytes] = retained_normal_sampling_seal(*value);
  if (enter(seal)) {
    add(bytes);
    leave(seal);
  }
  uv_layout(value->layout);
  normal_projection(value->projection);
  paint(value->before_paint);
  appearance(value->before_appearance);
  deformation(value->before_deformation);
  material(value->before_material);
  material(value->sampling_material);
  if (value->current_sampling_reference) toon(*value->current_sampling_reference);
  sampling_domain(value->metadata_sampling_domain);
  leave(value);
}

void Counter::color_preparation(const std::shared_ptr<const SourceColorPreparation> &value) {
  if (!enter(value)) return;
  add(sizeof(*value));
  color_projection(value->projection);
  color_projection(value->initial_projection);
  paint(value->original_paint);
  appearance(value->original_appearance);
  deformation(value->original_deformation);
  material(value->original_material);
  polygon_color_surface(value->polygon_colors);
  const auto &reprojection = value->uv_reprojection;
  if (enter(reprojection)) {
    add(sizeof(*reprojection));
    uv_layout(reprojection->layout);
    polygon_color_surface(reprojection->polygon_colors);
    color_projection(reprojection->projection);
    paint(reprojection->before_paint);
    appearance(reprojection->before_appearance);
    deformation(reprojection->before_deformation);
    material(reprojection->before_material);
    leave(reprojection);
  }
  const auto &sampling = value->paint_sampling;
  if (enter(sampling)) {
    add(sizeof(*sampling));
    sampling_domain(sampling->domain());
    uv_layout(sampling->layout());
    polygon_color_surface(sampling->current_descriptor());
    polygon_color_surface(sampling->sampling_descriptor());
    paint(sampling->before_paint());
    appearance(sampling->before_appearance());
    deformation(sampling->before_deformation());
    material(sampling->before_material());
    attachment(sampling->geometry_attachment());
    leave(sampling);
  }
  leave(value);
}

void Counter::normal_preparation(const std::shared_ptr<const SourceNormalPreparation> &value) {
  if (!enter(value)) return;
  add(sizeof(*value));
  normal_projection(value->projection);
  normal_projection(value->initial_projection);
  frame_set(value->target_frames);
  paint(value->original_paint);
  appearance(value->original_appearance);
  deformation(value->original_deformation);
  material(value->original_material);
  const auto &reprojection = value->uv_reprojection;
  if (enter(reprojection)) {
    add(sizeof(*reprojection));
    uv_layout(reprojection->layout);
    normal_projection(reprojection->projection);
    paint(reprojection->before_paint);
    appearance(reprojection->before_appearance);
    deformation(reprojection->before_deformation);
    material(reprojection->before_material);
    leave(reprojection);
  }
  normal_sampling(value->sampling_domain);
  derived_key(value->key_derivation);
  canonical_conversion(value->canonical_conversion);
  leave(value);
}

void Counter::canonical_conversion(const std::shared_ptr<const CanonicalNormalConversion> &value) {
  if (!enter(value)) return;
  if (!value->valid_owner(value)) throw std::invalid_argument("Native weight input lost its canonical normal owner");
  add(sizeof(*value));
  text(value->material_.name);
  paint(value->before_paint());
  material(value->before_material());
  deformation(value->before_deformation());
  frame_set(value->frames());
  normal_projection(value->projection());
  leave(value);
}

void Counter::canonical_uv(const std::shared_ptr<const CanonicalNormalUv> &value) {
  if (!enter(value)) return;
  if (!value->valid_owner(value)) throw std::invalid_argument("Native weight input lost its canonical UV owner");
  add(sizeof(*value));
  canonical_conversion(value->origin());
  uv_layout(value->layout());
  mesh(value->mesh());
  vector(value->coordinates());
  leave(value);
}

void Counter::canonical_sampling(const std::shared_ptr<const CanonicalPaintSampling> &value) {
  if (!enter(value)) return;
  if (!value->valid_owner(value)) throw std::invalid_argument("Native weight input lost its canonical paint owner");
  add(sizeof(*value));
  origin(value->before());
  canonical_uv(value->normal_uv());
  attachment(value->attachment());
  shared_vector(value->current_sources_);
  leave(value);
}

void Counter::native_sampling(const std::shared_ptr<const NativePaintSampling> &value) {
  if (!enter(value)) return;
  if (!value->valid_owner(value)) throw std::invalid_argument("Native weight input lost its native paint owner");
  add(sizeof(*value));
  text(value->before_native_material().name);
  paint(value->before_paint());
  material(value->before_material());
  uv_layout(value->layout());
  sampling_domain(value->domain());
  attachment(value->attachment());
  leave(value);
}

void Counter::polygon_mesh(const PolygonMesh &mesh) {
  vector(mesh.vertices());
  vector(mesh.edges());
  vector(mesh.faces());
  for (const auto &face : mesh.faces()) vector(face.corners);
}

void Counter::polygon_colors(const std::shared_ptr<const PolygonColorField> &field) {
  if (!enter(field)) return;
  add(sizeof(*field));
  shared_vector(field->source_owner());
  vector(field->corners());
  for (const auto &corner : field->corners()) vector(corner.provenance);
  leave(field);
}

void Counter::polygon_color_surface(const std::shared_ptr<const PolygonColorSurface> &surface) {
  if (!enter(surface)) return;
  add(sizeof(*surface));
  const auto &topology = surface->topology;
  if (enter(topology)) {
    add(sizeof(*topology));
    polygon_mesh(*topology);
    leave(topology);
  }
  polygon_colors(surface->field);
  leave(surface);
}

void Counter::polygon(const std::shared_ptr<const PolygonAuthority> &value) {
  if (!enter(value)) return;
  add(sizeof(*value));
  polygon_mesh(value->topology);
  polygon_colors(value->colors);
  origin(value->origin);
  leave(value);
}

void Counter::polygon_owner(const std::shared_ptr<const PolygonSurfaceOwner> &value) {
  if (!enter(value)) return;
  if (!value->valid_owner(value)) throw std::invalid_argument("Native weight input lost its polygon field owner");
  add(sizeof(*value));
  origin(value->before());
  attachment(value->current_attachment());
  const auto &trace = value->trace_;
  if (enter(trace)) {
    add(sizeof(*trace));
    for (const auto &mesh : {trace->original(), trace->current()}) {
      if (!enter(mesh)) continue;
      add(sizeof(*mesh));
      polygon_mesh(*mesh);
      leave(mesh);
    }
    vector(trace->steps());
    for (const auto &step : trace->steps()) vector(step.edit.selection.ids);
    leave(trace);
  }
  const auto &corners = value->corners_;
  if (enter(corners)) {
    add(sizeof(*corners));
    vector(corners->native);
    vector(corners->paint);
    vector(corners->normal);
    leave(corners);
  }
  leave(value);
}

void Counter::polygon_preview(const std::shared_ptr<const PolygonSurfacePreview> &preview) {
  if (!enter(preview)) return;
  unsupported(preview->posed_frames_,"Native weight input cannot retain a transient posed polygon preview");
  add(sizeof(*preview));
  baked(preview->baked());
  image(preview->native_bake());
  material(preview->binding());
  polygon_owner(preview->field_owner());
  attachment(preview->attachment());
  uv_origin(preview->origin_);
  const auto &streams = preview->streams_;
  if (enter(streams)) {
    add(sizeof(*streams));
    vector(streams->normals);
    vector(streams->tangents);
    vector(streams->paint);
    vector(streams->normal);
    vector(streams->native_frame_charts);
    vector(streams->frames);
    leave(streams);
  }
  leave(preview);
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
  add(sizeof(*sets));
  vector(sets->additional_);
  shared_vector(sets->slots_);
  texture_domain(sets->primary_);
  if (!sets->primary_ || sets->primary_->topology_ != sets->topology_)
    throw std::invalid_argument("Native weight input lost the primary texture-set topology");
  field_source(sets->primary_source_);
  for (const auto &field : sets->additional_) field_view(field);
  leave(sets);
}

template<class Field> void Counter::field_view(const Field &field) {
  text(field.name);
  text(field.material.name);
  texture_domain(field.domain);
  paint(field.painted);
  appearance(field.appearance);
  material(field.material_bindings);
  bake_recipe(field.bake_recipe);
  field_source(field.source);
}

void Counter::field_source(const std::shared_ptr<const TextureSetFieldSource> &source) {
  if (!enter(source)) return;
  source->authenticate_owner(source);
  add(sizeof(*source));
  field_view(source->field());
  paint(source->original_);
  paint(source->readable_);
  polygon_owner(source->polygon_);
  polygon_preview(source->preview_);
  leave(source);
}
} // namespace dome::detail::native_weight_retention_impl
