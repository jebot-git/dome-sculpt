// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <GL/gl.h>
#include <GL/glext.h>
#include <cstddef>
#include <limits>
#include <stdexcept>
namespace dome::render_detail {
inline size_t buffer_bytes(size_t count, size_t stride) {
  const size_t maximum = size_t(std::numeric_limits<GLsizeiptr>::max());
  if (count && stride > maximum / count)
    throw std::length_error("Surface buffer size exceeds GLsizeiptr range");
  return count * stride;
}
inline size_t buffer_sum(size_t a, size_t b) {
  const size_t maximum = size_t(std::numeric_limits<GLsizeiptr>::max());
  if (a > maximum || b > maximum - a)
    throw std::length_error("Surface buffer blocks exceed GLsizeiptr range");
  return a + b;
}
} // namespace dome::render_detail
