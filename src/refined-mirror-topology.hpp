// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "key-mirror-refinement.hpp"

namespace dome::detail {
struct IndexedMirrorRefinement {
  Mesh mesh;
  std::vector<uint32_t> parents, representatives;
};
IndexedMirrorRefinement index_mirror_refinement(const KeyMirrorRefinement &,
    const std::function<bool()> &cancelled,size_t max_bytes=128*1024*1024);
}
