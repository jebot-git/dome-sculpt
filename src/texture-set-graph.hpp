// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/texture-sets.hpp"
namespace dome::detail {
struct TextureSetGraphStats {
  size_t live_sets=0,retained_domains=0,collections=0,topologies=0;
  size_t geometry_bytes=0,membership_bytes=0,metadata_bytes=0;
};
// Per-call owner ledger. No cross-call cache, pixel replay or source permission.
// Existing Scene paint/image/frame budgets still authenticate/charge each field.
TextureSetGraphStats validate_texture_set_graph(const Scene&,const TextureSetLimits & = {});
}
