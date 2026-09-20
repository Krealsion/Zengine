// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_COMPONENT_CONTROL_STRIP_HPP
#define ZENGINE_COMPONENT_CONTROL_STRIP_HPP

// A ROW OF LABELLED CONTROLS PACKED INTO THE WIDTH A PANE HAS -- where each one is drawn, so a
// press can be answered by the control the maker could see, and how many did not fit.
//
// WHY IT EXISTS, as a measurement. Files and the Builder both had to grow a visible strip of
// buttons in the same beat, and the arithmetic was the same both times: a face per control, a
// single space between them, wrap when the next one will not fit, stop at the rows the
// composition can spare, and report what was left out so the pane can say where the rest are.
// Two consumers, written together, with the third (Attention) explicitly not converted here.
//
// WHAT IT OWNS: the faces and the placement. It does NOT own what a control means, whether it
// is available, whether pressing it is allowed, or what it says when refused -- those are the
// pane's, judged again when the press arrives. It records nothing across calls and reads no
// state: `pack` is pure.
//
// THE TWO FACES. An available control is `[label]`; an unavailable one is `(label)`. Both are
// PLACED, because a maker who presses one is owed the reason rather than silence -- a control
// the pane will refuse is still a control the pane must answer for. What is NOT placed is a
// control that did not fit: `dropped` counts those, and a pane that drops any owes the maker
// another route to them (the pane's own menu is what the two consumers use).
//
// ⚠ THE FIRST CONTROL IS NEVER DROPPED while one row is available and the width can hold it.
// Both consumers put their `[menu]` there for that reason: the route to everything survives
// the narrowest room that has a strip at all.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::component {

/// ONE CONTROL A PANE OFFERS: what it reads as, and whether the pane believes the operation
/// applies right now. Availability is a HINT drawn on the face; it is not permission, and the
/// pane asks its own question again when the press arrives.
struct Control {
    std::string label;
    bool available = true;
};

/// WHERE ONE CONTROL WAS DRAWN: which packed row, which column its face begins at, and how wide
/// the face is (brackets included -- the whole face is the target).
struct PlacedControl {
    std::size_t index = 0; ///< into the `Control` list handed to `pack`
    std::int64_t row = 0;  ///< into `ControlStrip::rows`
    std::int64_t first = 0;
    std::int64_t width = 0;
};

/// THE PACKED STRIP: the text rows to draw, where each placed control sits, and how many the
/// budget could not seat.
struct ControlStrip {
    std::vector<std::string> rows;
    std::vector<PlacedControl> placed;
    std::size_t dropped = 0;

    bool empty() const noexcept { return rows.empty(); }
};

/// THE FACE OF ONE CONTROL, spelled the one way so a pane's own measurement and this packing
/// agree on its width.
inline std::string control_face(const Control& control) {
    return control.available ? "[" + control.label + "]" : "(" + control.label + ")";
}

/// PACK `controls` INTO AT MOST `max_rows` ROWS OF `columns` COLUMNS, one space between faces.
///
/// A face wider than the whole width is dropped rather than cut: a control a maker cannot read
/// is not a control, and half a label under a hand is worse than none. Rows are only as long as
/// what they carry, so the caller may fit them into the room as it fits any other row.
inline ControlStrip pack_controls(const std::vector<Control>& controls, std::int64_t columns,
                                  std::int64_t max_rows) {
    ControlStrip strip;
    if (columns <= 0 || max_rows <= 0) {
        strip.dropped = controls.size();
        return strip;
    }
    std::string row;
    for (std::size_t i = 0; i < controls.size(); ++i) {
        const std::string face = control_face(controls[i]);
        const std::int64_t width = static_cast<std::int64_t>(face.size());
        if (width > columns) {
            ++strip.dropped; // wider than the pane: not drawable, and not a target
            continue;
        }
        const std::int64_t at = row.empty() ? 0 : static_cast<std::int64_t>(row.size()) + 1;
        if (at + width > columns) {
            if (static_cast<std::int64_t>(strip.rows.size()) + 1 >= max_rows) {
                // The last row this strip may spend is already the one being filled: everything
                // from here on has no seat, and the pane says where the rest of them are.
                strip.dropped += controls.size() - i;
                break;
            }
            strip.rows.push_back(std::move(row));
            row.clear();
        }
        const std::int64_t first = row.empty() ? 0 : static_cast<std::int64_t>(row.size()) + 1;
        if (!row.empty()) {
            row += ' ';
        }
        row += face;
        strip.placed.push_back(
            PlacedControl{i, static_cast<std::int64_t>(strip.rows.size()), first, width});
    }
    if (!row.empty()) {
        strip.rows.push_back(std::move(row));
    }
    return strip;
}

} // namespace zengine::component

#endif // ZENGINE_COMPONENT_CONTROL_STRIP_HPP
