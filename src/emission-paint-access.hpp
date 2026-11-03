// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/paint.hpp"
namespace dome::detail {
struct EmissionExportAccess {
 static std::shared_ptr<PaintedMesh> without_physical(const PaintedMesh &);
 static std::shared_ptr<const TextureMaps> other_channels(const PaintedMesh &,Material,const std::function<bool()> &);
};
}
