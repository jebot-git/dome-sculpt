// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/shapekey-authoring.hpp"

namespace dome::detail {
ShapeKeyResult author_refined_shapekey(Scene &,uint64_t,const ShapeKeyEdit &,std::string name,
                                     const std::function<bool()> &cancelled);
}
