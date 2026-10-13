// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_PAINTER_ABI_H
#define ZENGINE_GRAPHICS_PAINTER_ABI_H

/*
 * The painter seam: a skin is painter code a window opens, as an operator provider is opened, and
 * calls inside its own handler -- no message, no participant, no Kernel. The task crosses as a
 * `PaintTask`'s canonical bytes and the answer as a `PaintResult`'s (graphics/painting.hpp), and
 * the window reads the answer through the one gate, so a skin built by anyone is opened, called and
 * judged alike. The conventions are `zen/kernel/abi.h`'s, as operator/provider_abi.h's are.
 * Reference: graphics/docs/graphics.md.
 *
 * This header is valid C and C++.
 */

#include <zen/kernel/abi.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* This seam's own version, read before anything else in the table; a window refuses another. */
#define ZENGINE_PAINTER_ABI_VERSION 1u

/* What a skin's library offers a window. `name` is the skin's, NUL-terminated and valid for the
 * image's life. `paint` paints a task into `result` and answers 0, or answers nonzero with its
 * words in `reason` and no result; nothing a caller must free crosses. */
typedef struct ZenginePainterV1 {
    uint32_t abi_version;
    const char* name;
    int (*paint)(const uint8_t* task, size_t task_len, ZenByteSink result, ZenByteSink reason);
} ZenginePainterV1;

/* The symbol a window looks up, spelled once. */
#define ZENGINE_PAINTER_SYMBOL "zengine_painter"

ZEN_KERNEL_EXPORT const ZenginePainterV1* zengine_painter(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ZENGINE_GRAPHICS_PAINTER_ABI_H */
