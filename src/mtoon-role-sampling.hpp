// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/material-bindings.hpp"
#include "dome/material-visibility.hpp"

namespace dome::detail {

inline bool mtoon_shared_shade(const SourceMaterialBindings &binding) {
  const auto base=size_t(MToonSlot::Base), shade=size_t(MToonSlot::Shade);
  const auto &a=binding.roles[base], &b=binding.roles[shade];
  const auto &x=binding.material.textures[base], &y=binding.material.textures[shade];
  return a.image && a.image==b.image && a.wrap_s==b.wrap_s && a.wrap_t==b.wrap_t &&
      a.min_filter==b.min_filter && a.mag_filter==b.mag_filter &&
      mtoon_coordinate(x)==mtoon_coordinate(y) &&
      (binding.material.version()==MToonVersion::V0 ||
       (x.offset==y.offset && x.scale==y.scale && x.rotation==y.rotation));
}

} // namespace dome::detail
