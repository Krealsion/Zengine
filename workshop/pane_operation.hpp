// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_OPERATION_HPP
#define ZENGINE_WORKSHOP_PANE_OPERATION_HPP
#include <zen/weave/shape.hpp>
#include <cstdint>
#include <string>

namespace zengine::workshop {
// Continue a current pane gesture, named separately from this request’s correlation. The application checks
// the initiating actor's live authority for this operation. This does not widen the pane's
// own grant or license later operations; the pane must still make its ordinary gated send.
struct PaneOperationRequested {
    std::string pane;
    std::string role;
    std::string shape;
    std::int64_t version = 0;
    std::int64_t gesture = 0;
    ZEN_SHAPE(PaneOperationRequested, 1, ZEN_FIELD(pane), ZEN_FIELD(role), ZEN_FIELD(shape),
              ZEN_FIELD(version), ZEN_FIELD(gesture));
};
struct PaneOperationAnswered {
    bool allowed = false;
    std::string reason;
    ZEN_SHAPE(PaneOperationAnswered, 1, ZEN_FIELD(allowed), ZEN_FIELD(reason));
};
} // namespace zengine::workshop
#endif
