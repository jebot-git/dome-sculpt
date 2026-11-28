// SPDX-License-Identifier: GPL-3.0-or-later
#include "key-material-preview.hpp"
#include "dome/document.hpp"
#include "dome/material-bindings.hpp"
#include "geometry-identity.hpp"
#include "normal-preparation-state.hpp"
#include "prepared-color-state.hpp"

namespace dome::detail {
namespace {
thread_local const std::function<bool()> *preview_cancelled=nullptr;
struct ValidationCancellation {
  const std::function<bool()> *previous;
  explicit ValidationCancellation(const std::function<bool()> &current)
      :previous(preview_cancelled) { preview_cancelled=&current; }
  ~ValidationCancellation(){preview_cancelled=previous;}
};
}

const std::function<bool()> &KeyMaterialPreview::validation_cancellation() noexcept {
  static const std::function<bool()> empty;
  return preview_cancelled?*preview_cancelled:empty;
}

KeyMaterialPreview::KeyMaterialPreview(const SurfaceRange &range)
    :object_(range.object),binding_(range.material_bindings),maps_(range.textures),
     key_(binding_->key_derivation),frames_(binding_->normal_frames),
     normals_(binding_->normal_preparation),colors_(binding_->color_preparation),
     normal_projection_(maps_?maps_->normal_projection:nullptr),
     color_projection_(maps_?maps_->color_projection:nullptr),bake_(maps_?maps_->baked_base:nullptr),
     polygon_(maps_?maps_->polygon_surface:nullptr) {}

bool KeyMaterialPreview::matches(const SurfaceRange &range) const noexcept {
  using geometry_detail::same_owner;
  if(object_!=range.object||!same_owner(binding_,range.material_bindings)||
     !same_owner(maps_,range.textures))return false;
  return same_owner(key_,binding_->key_derivation)&&same_owner(frames_,binding_->normal_frames)&&
    same_owner(normals_,binding_->normal_preparation)&&same_owner(colors_,binding_->color_preparation)&&
    (!maps_||(same_owner(normal_projection_,maps_->normal_projection)&&
              same_owner(color_projection_,maps_->color_projection)&&same_owner(bake_,maps_->baked_base)&&
              same_owner(polygon_,maps_->polygon_surface)));
}

void KeyMaterialPreview::authenticate(const SurfaceRange &range) const {
  if(!matches(range))throw std::invalid_argument("Key material preview lost its validated source owners");
}

std::shared_ptr<const KeyMaterialPreview> KeyMaterialPreview::prepare(const SurfaceRange &range,
    const std::function<bool()> &cancelled) {
  auto stop=[&]{
    if(cancelled&&cancelled())throw std::runtime_error("Key material preview validation cancelled");
  };
  stop();
  if(!range.material_bindings||!range.material_bindings->key_derivation)
    throw std::logic_error("Key material preview requires an owning key derivation");
  const ValidationCancellation validation(cancelled);
  // These are the deep checks used by the viewport for a new material range.
  // The remaining render checks still validate its packed streams and maps.
  range.material_bindings->validate(true);
  stop();
  prepared_color_state(range.material_bindings.get(),range.textures.get(),true);
  stop();
  prepared_normal_state(range.material_bindings.get(),range.textures.get(),true);
  stop();
  return std::shared_ptr<const KeyMaterialPreview>(new KeyMaterialPreview(range));
}
}
