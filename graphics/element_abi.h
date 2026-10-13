// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_ELEMENT_ABI_H
#define ZENGINE_GRAPHICS_ELEMENT_ABI_H

/*
 * The element seam: the code that presents a data form, opened by a window as it opens a skin and
 * on the same conventions. Events and outcomes cross as Loom canonical bytes (`ElementEvent` in,
 * `ElementOutcome` out, `ElementDescribed` from `describe`; graphics/painting.hpp), read through
 * the one gate. At this version the code keeps nothing between calls: the window keeps each
 * element's state and hands it in with every event, naming the element by a handle that is the
 * window's own. A later version may keep the state in the code, by that handle.
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
#define ZENGINE_ELEMENT_ABI_VERSION 1u

/* What an element's library offers a window. `type` is the data form it presents, as its shape's
 * name. `describe` writes what it is (its changes in index order) and `act` one event's outcome;
 * each answers 0, or nonzero with its words in `reason` and nothing else written. */
typedef struct ZengineElementV1 {
    uint32_t abi_version;
    const char* type;
    int (*describe)(ZenByteSink described, ZenByteSink reason);
    int (*act)(const uint8_t* event, size_t event_len, ZenByteSink outcome, ZenByteSink reason);
} ZengineElementV1;

/* The symbol a window looks up, spelled once. */
#define ZENGINE_ELEMENT_SYMBOL "zengine_element"

ZEN_KERNEL_EXPORT const ZengineElementV1* zengine_element(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ZENGINE_GRAPHICS_ELEMENT_ABI_H */
