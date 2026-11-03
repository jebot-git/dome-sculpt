// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include <span>
namespace dome::detail {
struct NativeScenePayload {
  std::vector<uint8_t> stored;
  size_t decoded_bytes=0;
  bool compressed=false;
};
// Uses the exact existing bounded native CBOR/DEFLATE primitives. HDR1/2
// callers and serialization order remain untouched.
NativeScenePayload capture_native_scene_payload(const Scene &,
                                                const std::function<bool()> &);
Scene restore_native_scene_payload(std::span<const uint8_t>,bool compressed,
                                   size_t decoded_bytes,size_t max_decoded_bytes,
                                   const std::function<bool()> &);
}
