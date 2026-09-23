// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_CARRY_HPP
#define ZENGINE_WORKSHOP_PANE_CARRY_HPP
#include <zen/weave/shape.hpp>
#include <zen/value.hpp>
#include <cstdint>
#include <string>

namespace zengine::workshop {
// A pane supplies an owned typed envelope. Workshop transports it without interpreting its
// contents. A receiving pane owns decoding, acceptance and any subsequent operation.
struct PaneCarryRequested {
    std::string pane;
    std::string label;
    loom::Bytes data;
    ZEN_SHAPE(PaneCarryRequested, 1, ZEN_FIELD(pane), ZEN_FIELD(label), ZEN_FIELD(data));
};
struct PaneCarryAnswered {
    bool carried = false;
    std::string reason;
    ZEN_SHAPE(PaneCarryAnswered, 1, ZEN_FIELD(carried), ZEN_FIELD(reason));
};
/// Copy transfer. `drag` binds placement to the initiating primary press's release. False
/// supplies the keyboard pick-and-place equivalent. Data is an owned, opaque value envelope.
struct PaneValueCarryRequested {
    std::string pane;
    std::string label;
    loom::Bytes data;
    bool drag = true;
    ZEN_SHAPE(PaneValueCarryRequested, 1, ZEN_FIELD(pane), ZEN_FIELD(label), ZEN_FIELD(data), ZEN_FIELD(drag));
};
struct PaneValueDrop {
    std::string pane;
    loom::Bytes data;
    std::int64_t row = 0;
    std::int64_t column = 0;
    std::int64_t picture = 0;
    ZEN_SHAPE(PaneValueDrop, 1, ZEN_FIELD(pane), ZEN_FIELD(data), ZEN_FIELD(row), ZEN_FIELD(column),
              ZEN_FIELD(picture));
};
struct PaneDrop {
    std::string pane;
    loom::Bytes data;
    std::int64_t row = 0;
    std::int64_t column = 0;
    std::int64_t picture = 0;
    ZEN_SHAPE(PaneDrop, 1, ZEN_FIELD(pane), ZEN_FIELD(data), ZEN_FIELD(row), ZEN_FIELD(column),
              ZEN_FIELD(picture));
};
} // namespace zengine::workshop
#endif
