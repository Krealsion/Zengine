// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_SETUP_CONTROL_HPP
#define ZENGINE_WORKSHOP_SETUP_CONTROL_HPP
#include <zen/weave/shape.hpp>
#include <string>
namespace zengine::workshop {
/// Apply a validated setup to the current layout, without writing any file.
struct SetupApplyRequested {
    std::string setup;
    ZEN_SHAPE(SetupApplyRequested, 1, ZEN_FIELD(setup));
};
/// Explicitly discard a pane's local draft/view state. An in-flight owner operation refuses.
/// Does not undo submitted commands or modify files or stored inventory entries.
struct PaneResetRequested {
    std::string pane;
    ZEN_SHAPE(PaneResetRequested, 1, ZEN_FIELD(pane));
};
} // namespace zengine::workshop
#endif
