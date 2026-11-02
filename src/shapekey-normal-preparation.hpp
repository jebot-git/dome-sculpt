// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/shapekey-authoring.hpp"
namespace dome::detail {
ShapeKeyResult author_prepared_normal_shapekey(Scene &,uint64_t,const ShapeKeyEdit &,
    const std::function<bool()> &);
}
