// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_COMPONENT_ROW_MAP_HPP
#define ZENGINE_COMPONENT_ROW_MAP_HPP

// THE PICTURE READ BACKWARDS -- what each published row, and each run of columns inside one,
// MEANS, recorded by the same pass that composed the text, so a press cannot land where a row
// is not (the one-geometry rule, on the pane's side of the seam).
//
// WHY IT EXISTS, as a measurement. Five panes kept this record, each in its own shape: Info's
// `Row`/`placed`, Files' `entry_at_row`, Powers' `PowersSpan`/`target_at` (rows AND columns,
// because its chrome row carries three controls side by side), the Composer's `RenderedRow`,
// and the Terminal's four named rows. The Pane Manager's `[open]` mark beside its name and the
// Hotkeys table's cells are the consumers that earned the column answer here: one row means two
// things, and the inverse must say which. `solid` is Powers' rule carried whole: a control the
// width cut is not a target.
//
// WHAT IT OWNS: the spans of one composition, replaced whole each time the rows are said, and a
// PICTURE NUMBER for that composition -- unchanged by a recomposition whose spans are equal,
// changed by one that moves a row. It knows no row's text, no role, no medium and no wire shape:
// `Meaning` is whatever the consumer's press handler switches on. Two consumers earned it (the
// Pane Manager and Hotkeys); the panes above keep theirs until each chooses to move.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zengine::component {

/// HOW MANY LEADING COLUMNS OF A FITTED ROW ARE GENUINE TEXT: everything when the text fit,
/// everything but the cut mark when `fit` had to cut (`powers.hpp` `solid_columns`, carried;
/// `mark` is the length of the consumer's cut mark, `kElided`'s three).
inline std::int64_t solid_columns(const std::string& drawn, std::size_t wanted,
                                  std::size_t mark = 3) noexcept {
    std::int64_t solid = static_cast<std::int64_t>(drawn.size());
    if (drawn.size() < wanted) {
        solid -= static_cast<std::int64_t>(mark);
    }
    return solid < 0 ? 0 : solid;
}

template <class Meaning>
class RowMap {
public:
    /// ONE RUN OF ONE ROW THAT CARRIES ONE MEANING. `first`/`last` are inclusive prose columns;
    /// `last == kWholeRow` is the row entire.
    struct Span {
        std::int64_t row = 0;
        std::int64_t first = 0;
        std::int64_t last = kWholeRow;
        Meaning meaning{};

        bool operator==(const Span& o) const {
            return row == o.row && first == o.first && last == o.last && meaning == o.meaning;
        }
    };

    static constexpr std::int64_t kWholeRow = -1;

    /// BEGIN A NEW COMPOSITION. What was recorded is kept aside until `settle`, which compares
    /// the two and numbers the picture: the same spans keep the same number.
    void begin() {
        previous_.swap(spans_);
        spans_.clear();
    }

    /// END THE COMPOSITION: the picture number moves exactly when the spans did. Returns it.
    std::int64_t settle() {
        if (!settled_ || spans_ != previous_) {
            ++picture_;
        }
        settled_ = true;
        previous_.clear();
        return picture_;
    }

    /// THE NUMBER OF THE PICTURE LAST SETTLED -- what a pane publishes on `v3::PaneContent`
    /// and compares a press's echo against. 0 until the first `settle`.
    std::int64_t picture() const noexcept { return picture_; }

    /// Does a press stamped `echoed` name this picture? A press echoing 0 names none: a host
    /// that never admitted a numbered picture, which a consumer may choose to act on anyway.
    bool current(std::int64_t echoed) const noexcept { return echoed == picture_; }

    void clear() noexcept { spans_.clear(); }
    std::size_t size() const noexcept { return spans_.size(); }
    const std::vector<Span>& spans() const noexcept { return spans_; }

    /// THE WHOLE ROW MEANS ONE THING -- a list entry, a heading a press may name.
    void row(std::int64_t row, Meaning meaning) {
        spans_.push_back(Span{row, 0, kWholeRow, std::move(meaning)});
    }

    /// A RUN OF COLUMNS MEANS ONE THING -- a control inside a row. Refused, and not recorded,
    /// when the run is not inside the row's `solid` columns: a press on the `...` a cut left
    /// behind must not operate a control the maker cannot see. Returns whether it was recorded.
    bool span(std::int64_t row, std::int64_t first, std::int64_t width, std::int64_t solid,
              Meaning meaning) {
        if (width <= 0 || first < 0 || first + width > solid) {
            return false;
        }
        spans_.push_back(Span{row, first, first + width - 1, std::move(meaning)});
        return true;
    }

    /// THE MEANING UNDER A PLACE, or nullptr: the narrowest span that contains it wins, so a
    /// control inside an entry row answers before the row does. Total over every row and column,
    /// including ones no press can produce.
    const Meaning* at(std::int64_t row, std::int64_t column) const noexcept {
        const Span* best = nullptr;
        std::int64_t best_width = 0;
        for (const Span& s : spans_) {
            if (s.row != row) {
                continue;
            }
            const bool whole = s.last == kWholeRow;
            if (!whole && (column < s.first || column > s.last)) {
                continue;
            }
            const std::int64_t width = whole ? kWholeRowWidth : s.last - s.first + 1;
            if (best == nullptr || width < best_width) {
                best = &s;
                best_width = width;
            }
        }
        return best == nullptr ? nullptr : &best->meaning;
    }

    /// THE ROW'S OWN MEANING, ignoring controls inside it, or nullptr.
    const Meaning* at_row(std::int64_t row) const noexcept {
        for (const Span& s : spans_) {
            if (s.row == row && s.last == kWholeRow) {
                return &s.meaning;
            }
        }
        return nullptr;
    }

    /// THE FIRST ROW THAT CARRIES THIS MEANING (whole-row spans only), or -1: what a keyboard
    /// route needs to name the place of the row it acts on.
    std::int64_t row_of(const Meaning& meaning) const noexcept {
        for (const Span& s : spans_) {
            if (s.last == kWholeRow && s.meaning == meaning) {
                return s.row;
            }
        }
        return -1;
    }

private:
    /// Wider than any prose row, so a whole-row span loses to any column span on its row.
    static constexpr std::int64_t kWholeRowWidth = 1 << 30;
    std::vector<Span> spans_;
    std::vector<Span> previous_;
    std::int64_t picture_ = 0;
    bool settled_ = false;
};

} // namespace zengine::component

#endif // ZENGINE_COMPONENT_ROW_MAP_HPP
