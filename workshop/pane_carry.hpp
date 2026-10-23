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
} // namespace v2

// ---- Where a carried item lands: a point of the receiving pane's area ------------------------
//
// While something is carried, Workshop's own area stands in front of every pane's, so the hand's
// moves and its release are Workshop's to read; where the hand is over a pane, Workshop says the
// place in that pane's area -- its name, and a point in its pixels from its corner -- and the
// pane reads it against what it drew there and owns what the item means at that place. A drop
// passes on the window's word of what it drew under the hand where the hand was over that area
// (`graphics::AreaButtonChanged`'s `under`, `part` and `drawing`), which a pane judges a drop by
// as it judges a press; `under` is "" where the window said nothing of that area.

/// A value copy placed on a pane: where it landed in the pane's area, with the source office and
/// pane Workshop stamps and the source's transfer token.
struct PaneCanvasValueDrop {
    std::string pane;
    std::string area;
    std::int64_t x = 0, y = 0;
    loom::Bytes data;
    std::string source_office, source_pane, token;
    std::string under{};
    std::string part{};
    std::int64_t drawing = 0;
    ZEN_SHAPE(PaneCanvasValueDrop, 4, ZEN_FIELD(pane), ZEN_FIELD(area), ZEN_FIELD(x),
              ZEN_FIELD(y), ZEN_FIELD(data), ZEN_FIELD(source_office), ZEN_FIELD(source_pane),
              ZEN_FIELD(token), ZEN_FIELD(under), ZEN_FIELD(part), ZEN_FIELD(drawing));
};

/// A carried reference placed on a pane, as a value copy is: where it landed in the pane's area.
struct PaneCanvasDrop {
    std::string pane;
    std::string area;
    std::int64_t x = 0, y = 0;
    loom::Bytes data;
    std::string under{};
    std::string part{};
    std::int64_t drawing = 0;
    ZEN_SHAPE(PaneCanvasDrop, 3, ZEN_FIELD(pane), ZEN_FIELD(area), ZEN_FIELD(x), ZEN_FIELD(y),
              ZEN_FIELD(data), ZEN_FIELD(under), ZEN_FIELD(part), ZEN_FIELD(drawing));
};

/// Where a carried item rests over a pane, for a pane that accepts it: a point of its area, or
/// `over` false once the hand left the area it last named. Presentation only: no gesture, focus or
/// key moves.
struct PaneCanvasHover {
    std::string pane;
    std::string area;
    std::int64_t x = 0, y = 0;
    bool over = true;
    ZEN_SHAPE(PaneCanvasHover, 3, ZEN_FIELD(pane), ZEN_FIELD(area), ZEN_FIELD(x), ZEN_FIELD(y),
              ZEN_FIELD(over));
};
} // namespace zengine::workshop
#endif
