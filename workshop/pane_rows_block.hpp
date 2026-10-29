// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_ROWS_BLOCK_HPP
#define ZENGINE_WORKSHOP_PANE_ROWS_BLOCK_HPP

// Text as a helper inside any pane's picture: rows composed with what each row and run of columns
// means, a control strip, a list through its window, the caret and the selection, drawn into the
// pane's picture wherever it places them, and a press read back to what the row it landed on means.
// A pane that draws one picture (`PaneDrawingFacet`) calls it beside its own shapes; a pane that
// says rows (`PaneRowsFacet`) holds one for its whole area. workshop/docs/workshop-panes.md, "Text
// inside a picture".

#include "component/control_strip.hpp"
#include "component/list_window.hpp"
#include "component/row_map.hpp"
#include "graphics/canvas.hpp"
#include "graphics/vocabulary.hpp"
#include "pane_drawing.hpp"
#include "pane_parts.hpp"
#include "pane_rows.hpp"
#include "pane_text.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// ROWS PLACED IN A PICTURE: `drawn`, as `rows_drawing` drew it for a room of `width` by `height`
/// pixels, put into `into` as the group `name` at `x`, `y` and clipped to that room, each node and
/// element record beneath it; without the rows' own ground unless `ground`, so they stand on what
/// the picture drew there. A press on the picture reaches a part of the rows over its face, as it
/// would in a picture of its own (`graphics::part_at` follows the group).
inline void place_rows(graphics::Sketch& into, const std::string& name, std::int64_t x,
                       std::int64_t y, std::int64_t width, std::int64_t height,
                       const RowsDrawing& drawn, bool ground = true) {
    into.group(name, x, y, width, height);
    for (graphics::Node node : drawn.sketch.nodes()) {
        if (!ground && node.path == "room") {
            continue;
        }
        node.path = name + "/" + node.path;
        into.put(std::move(node));
    }
    for (graphics::ElementRecord record : drawn.sketch.records()) {
        record.path = name + "/" + record.path;
        into.say(std::move(record));
    }
}

/// THE ROW TOOLS, FOR ANY PICTURE: the rows being composed for a room of `rows` by `columns`, what
/// each row and run of columns means (`component::RowMap<Meaning>`), the caret and the selection.
/// `take` gives them with the parts their meanings name; `draw` puts them in a picture as a group
/// and keeps which meaning each part names, so a press the window landed on a part is read back to
/// what it means (`named`).
template <class Meaning>
class RowsBlock {
public:
    /// BEGIN A PICTURE in a room of `rows` by `columns`: no rows, no meanings, no caret. What the
    /// last picture meant is kept aside until the rows are taken or drawn.
    void begin(std::int64_t rows, std::int64_t columns) {
        rows_ = rows;
        columns_ = columns;
        map_.begin();
        composing_.clear();
        caret_ = RowsCaret{};
    }
    std::int64_t rows() const noexcept { return rows_; }
    std::int64_t columns() const noexcept { return columns_; }

    /// ONE ROW, while the room has one for it, and what it means when it means something. False,
    /// recording nothing, once the room has run out: a row nobody can see names nothing.
    bool push(TextRow row, std::optional<Meaning> meaning = std::nullopt) {
        if (next_row() >= rows_) {
            return false;
        }
        if (meaning) {
            map_.row(next_row(), std::move(*meaning));
        }
        composing_.push_back(std::move(row));
        return true;
    }
    /// The row the next `push` lands on.
    std::int64_t next_row() const noexcept { return static_cast<std::int64_t>(composing_.size()); }
    std::vector<TextRow>& composing() noexcept { return composing_; }
    component::RowMap<Meaning>& map() noexcept { return map_; }
    const component::RowMap<Meaning>& map() const noexcept { return map_; }
    /// Where the caret and the selection stand in the rows being composed; none by default.
    RowsCaret& caret() noexcept { return caret_; }
    const RowsCaret& caret() const noexcept { return caret_; }

    /// HOW MANY ROWS A STRIP OF THESE CONTROLS WOULD TAKE in this room, at most `max_rows`
    /// (`component::strip_budget`): asked before the body is laid out, so the body is given what
    /// is genuinely left and the strip is never the row that falls off the end.
    template <class Row>
    std::int64_t strip_rows(const std::vector<Row>& controls,
                            std::int64_t max_rows = component::kMaxStripRows) const {
        return component::strip_rows_for(controls, columns_,
                                         component::strip_budget(rows_, max_rows));
    }

    /// THE CONTROL STRIP, ITS FACES RECORDED AS TARGETS: each packed row fitted by `fit_row` and
    /// pushed, and each face it seats a span meaning `meaning_of(control)`. What did not fit is
    /// counted on the last strip row, and the route to it is the pane's menu, never only a key.
    /// A face the fit cut is not recorded (`RowMap::span` refuses it), and nothing is once the
    /// room runs out: a press on what a weaver cannot read must not operate a control.
    template <class Row, class FitRow, class MeaningOf>
    void push_controls(const std::vector<Row>& controls, FitRow&& fit_row, MeaningOf&& meaning_of,
                       std::int64_t max_rows = component::kMaxStripRows) {
        const component::ControlStrip strip = component::pack_controls(
            controls, columns_, component::strip_budget(rows_, max_rows));
        for (std::size_t i = 0; i < strip.rows.size(); ++i) {
            std::string text = strip.rows[i];
            if (i + 1 == strip.rows.size() && strip.dropped > 0) {
                text += "  +" + std::to_string(strip.dropped) + " in menu";
            }
            const std::int64_t row = next_row();
            if (!push(TextRow{fit_row(text), graphics::role::kFill})) {
                return;
            }
            const std::int64_t solid = component::solid_columns(
                composing_[static_cast<std::size_t>(row)].text, text.size());
            for (const component::PlacedControl& placed : strip.placed) {
                if (placed.row == static_cast<std::int64_t>(i)) {
                    map_.span(row, placed.first, placed.width, solid,
                              meaning_of(controls[placed.index]));
                }
            }
        }
    }

    /// A LIST THROUGH ITS WINDOW: the row saying what the window left out above, `emit_entry(i)`
    /// for each member it shows, and the row saying what it left out below -- each marker said
    /// only where the window reserved it a row (`component::earlier_marker`), fitted by
    /// `fit_row`. Returns the rows it asked for, markers included.
    template <class FitRow, class EmitEntry>
    std::int64_t push_window(const component::ListWindow& win, FitRow&& fit_row,
                             EmitEntry&& emit_entry) {
        std::int64_t asked = 0;
        const auto marker = [&](std::size_t omitted, const char* which) {
            if (omitted > 0) {
                (void)push(TextRow{fit_row("  " + pane_text::omitted_text(omitted, which)),
                                   graphics::role::kMuted});
                ++asked;
            }
        };
        marker(component::earlier_marker(win), "earlier");
        for (std::size_t i = win.first; i < win.end(); ++i) {
            emit_entry(i);
            ++asked;
        }
        marker(component::more_marker(win), "more");
        return asked;
    }

    /// ROWS A BODY IS DRAWN OVER -- `count` from `first`, as many as the room holds: what was
    /// composed there means nothing now (`RowMap::cover`), and a row not composed yet is a blank
    /// one, so the body is written over each with `composing()` and `map().row`.
    void cover(std::int64_t first, std::int64_t count) {
        if (first < 0 || count <= 0) {
            return;
        }
        const std::int64_t end = (std::min)(first + count, rows_);
        if (end <= first) {
            return;
        }
        map_.cover(first, end - first);
        if (static_cast<std::int64_t>(composing_.size()) < end) {
            composing_.resize(static_cast<std::size_t>(end));
        }
    }

    /// NOTHING IS SAID THIS BEAT, so no row of it names anything.
    void hold() { (void)map_.settle(); }

    /// THE ROWS COMPOSED, TAKEN: the rows, each part named `name_of(meaning)` and available as
    /// `available_of(meaning)` says, and the number for what they mean (`RowMap::picture`). The
    /// next picture begins empty.
    template <class NameOf, class AvailableOf>
    ComposedRows take(NameOf&& name_of, AvailableOf&& available_of) {
        (void)map_.settle();
        ComposedRows out;
        out.parts = row_parts(map_, columns_, name_of, available_of);
        out.rows = std::move(composing_);
        out.number = map_.picture();
        composing_.clear();
        named_.clear();
        for (const auto& span : map_.spans()) {
            named_.emplace(name_of(span.meaning), span.meaning);
        }
        return out;
    }

    /// THE ROWS DRAWN INTO A PICTURE, as the group `name` with its top left at `x`, `y`: taken as
    /// `take` takes them, a row a line of `text` and a character an advance of it (`cells` where
    /// the window's unit is the cell), the caret in them when `with_caret`, standing on their own
    /// ground when `ground`, and clipped to the room. Returns the rows' room in pixels, so the
    /// picture can place what stands beside them.
    template <class NameOf, class AvailableOf>
    RowsRoom draw(graphics::Sketch& into, const std::string& name, std::int64_t x,
                  std::int64_t y, const graphics::TextMeasure& text, bool cells,
                  NameOf&& name_of, AvailableOf&& available_of, bool with_caret = true,
                  bool ground = true) {
        const RowsCaret caret = with_caret ? caret_ : RowsCaret{};
        ComposedRows rows = take(name_of, available_of);
        RowsRoom room;
        room.width = columns_ * text.advance;
        room.height = rows_ * text.line;
        room.text = text;
        room.cells = cells;
        const RowsDrawing drawn = rows_drawing(rows.rows, rows.parts, room, columns_, caret, rows_);
        place_rows(into, name, x, y, room.width, room.height, drawn, ground);
        return room;
    }

    /// WHAT A PRESS MEANS, by the part the window landed it on: the meaning whose name the part
    /// carries in the rows last taken or drawn, or none.
    const Meaning* named(const std::string& part) const {
        const auto found = named_.find(part);
        return found == named_.end() ? nullptr : &found->second;
    }

private:
    std::int64_t rows_ = 0, columns_ = 0;
    std::vector<TextRow> composing_;
    component::RowMap<Meaning> map_;
    RowsCaret caret_;
    std::map<std::string, Meaning> named_; ///< each part's meaning, as the rows last taken name it
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_ROWS_BLOCK_HPP
