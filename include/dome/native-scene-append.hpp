// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scene.hpp"
namespace dome {
struct NativeSceneAppend {
  Scene scene;
  std::vector<uint64_t> objects;
};
// Detached worker transaction. All package objects (including hidden objects)
// are appended. Live IDs and model-instance identities are isolated from the
// destination; historical native paths rebase consistently. Original source-node
// IDs and source-object provenance stay exact. Combined state must remain within
// the existing256MiB native project save limit.
// Neither input changes on cancellation, allocation failure or quota refusal.
NativeSceneAppend append_native_scene(const Scene &destination,
                                     const Scene &package,
                                     const std::function<bool()> &cancelled = {});
}
