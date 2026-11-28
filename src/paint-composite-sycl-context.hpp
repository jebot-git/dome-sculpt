// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <sycl/sycl.hpp>

namespace dome::paint_compute {
class MaterialAtlas;
struct MaterialAtlasSyclAccess {
  static sycl::queue queue(const MaterialAtlas &);
};
}
