// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// C error boundary: libjpeg longjmp never bypasses C++ object destructors.
int dome_import_jpeg(const uint8_t *input,size_t size,size_t max_pixels,
                     uint8_t **rgba,unsigned *width,unsigned *height,
                     char *message,size_t message_size,
                     int (*cancelled)(void *),void *cancel_context);

#ifdef __cplusplus
}
#endif
