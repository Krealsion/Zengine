// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_VOCABULARY_HPP
#define ZENGINE_WORKSHOP_VOCABULARY_HPP

// The Workshop host weave's state shape, empty: nothing the host holds is weave state. The desk,
// keymap, preferences and session are files; the rest is `Session`.

#include <zen/weave/shape.hpp>

namespace zengine::workshop {

/// The host weave's state: nothing.
struct WorkshopState {
    ZEN_SHAPE(WorkshopState, 1);
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_VOCABULARY_HPP
