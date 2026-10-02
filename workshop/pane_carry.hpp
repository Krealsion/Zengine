// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_CARRY_HPP
#define ZENGINE_WORKSHOP_PANE_CARRY_HPP
#include <zen/weave/shape.hpp>
#include <zen/value.hpp>
#include <cstddef>
#include <cstdint>
#include <string>

namespace zengine::workshop {
/// The carrier's bounds: an envelope larger than kMaxCarryBytes is refused, never truncated, and a
/// label past kMaxCarryLabelBytes is refused; a pane cuts its label to the bound before it asks.
inline constexpr std::size_t kMaxCarryBytes = 65536;
inline constexpr std::size_t kMaxCarryLabelBytes = 128;
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
// Version 2 keeps the pure copy payload and adds a source-owned transfer token. Workshop
// stamps the actual source office/pane; receiving code must never infer that from item metadata.
namespace v2 {
struct PaneValueCarryRequested {
    std::string pane;
    std::string label;
    loom::Bytes data;
    bool drag = true;
    std::string token;
    ZEN_SHAPE(PaneValueCarryRequested, 2, ZEN_FIELD(pane), ZEN_FIELD(label), ZEN_FIELD(data),
              ZEN_FIELD(drag), ZEN_FIELD(token));
};
struct PaneValueDrop {
    std::string pane;
    loom::Bytes data;
    std::int64_t row = 0, column = 0, picture = 0;
    std::string source_office, source_pane, token;
    ZEN_SHAPE(PaneValueDrop, 2, ZEN_FIELD(pane), ZEN_FIELD(data), ZEN_FIELD(row), ZEN_FIELD(column),
              ZEN_FIELD(picture), ZEN_FIELD(source_office), ZEN_FIELD(source_pane), ZEN_FIELD(token));
};
} // namespace v2
/// A value copy placed on a canvas pane: where it landed in the canvas's own local subunits, in
/// which granted room and on which picture, with v2's attribution. A canvas has no rows, so its
/// provider hit-tests the place in the picture it drew and owns what the drop means there.
struct PaneCanvasValueDrop {
    std::string pane;
    std::int64_t grant = 0, picture = 0, x = 0, y = 0;
    loom::Bytes data;
    std::string source_office, source_pane, token;
    ZEN_SHAPE(PaneCanvasValueDrop, 1, ZEN_FIELD(pane), ZEN_FIELD(grant), ZEN_FIELD(picture),
              ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(data), ZEN_FIELD(source_office),
              ZEN_FIELD(source_pane), ZEN_FIELD(token));
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
