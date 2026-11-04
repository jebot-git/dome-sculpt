// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/mesh.hpp"
#include <algorithm>
#include <stdexcept>
namespace dome::detail {
// Peak conservative storage for the real current-corner coordinate compiler:
// live input, identity/remapped layouts, hash nodes/buckets and emitted geometry.
inline size_t native_paint_export_working_bytes(const Mesh &mesh){
 if(mesh.positions.size()>2'000'000||mesh.indices.size()>6'000'000||mesh.indices.size()%3)
  throw std::length_error("Native paint coordinate export exceeds geometry policy");
 const uint64_t vertices=std::min<size_t>(mesh.indices.size(),2'000'000);
 const uint64_t bytes=uint64_t(mesh.positions.size())*36+uint64_t(mesh.indices.size())*13+vertices*128+4096;
 if(bytes>512*1024*1024)throw std::length_error("Native paint coordinate export exceeds working policy before allocation");
 return size_t(bytes);
}
}
