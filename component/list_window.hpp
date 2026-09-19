// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_COMPONENT_LIST_WINDOW_HPP
#define ZENGINE_COMPONENT_LIST_WINDOW_HPP

// A WINDOW ONTO AN ORDERED COLLECTION -- which members a bounded place shows, how many it
// leaves out on each side, and how many rows it reserved to say so.
//
// WHY IT EXISTS, as a measurement. Six copies of the arithmetic live in the tree, written to
// two anchorings and three cursor policies:
//
//   cursor, least motion   desktop-pane/pane.cpp `window_for`          the largest window that
//                                                                       keeps the cursor, nearest
//                                                                       last time's first row
//   cursor, three rules    workshop/screen_gestures.cpp `list_window`  one marker per cut side;
//                          info-pane/pane.cpp, attention-pane/pane.cpp  carried byte for byte
//   cursor, centred        files/files.cpp `window_of`+`fitted_window` two marker rows, shrunk
//                          introspection/powers.hpp `powers_window`     one marker row for both
//                          composer/view.hpp `window_of`                sides, centred
//   offset (a scroll)      desktop-pane/pane.cpp `say_keys`             clamped to the last page
//
// WHAT IS SHARED is the accounting every copy owes and two of them got wrong at small budgets:
// a population that fits is shown whole; the anchor row is inside the window; the counts are
// conserved (`before + count + after == total`, always); and a marker is a ROW of the same
// budget, so `count + markers <= budget`, always -- a marker that overran the budget cut the
// last entry (measured in Files, `fitted_window`'s reason). WHAT IS NOT SHARED is the policy:
// which rows to show around the anchor, and whether a cut side is said on its own row, on one
// row for both sides, or not at all. So this file is one value and three policies, and a
// consumer keeps the policy it has: nothing here asks Files, Powers, Compose, Info or Attention
// to scroll differently, and a consumer that keeps its own function loses nothing but the
// shared accounting.
//
// THE VALUE SEPARATES THE OMITTED COUNTS (`before`, `after`: what the window did not show,
// always conserved) from the MARKER ROWS it RESERVED (`markers`: rows of the budget the
// consumer spends saying so) -- an independent review found the two conflated at zero- and
// one-row budgets. A cut side with no marker reserved is a cut the consumer must say elsewhere
// or accept; the counts still tell it which. The Pane Manager (`cursor_window`) and the Hotkeys
// pane (`cursor_window` since it gained a row cursor) are the consumers here.

#include <cstddef>
#include <cstdint>

namespace zengine::component {

/// WHICH MEMBERS A PLACE SHOWS, WHAT IT OMITTED, AND WHAT IT RESERVED TO SAY SO.
///
/// Invariants, over every input of every policy below:
///   before + count + after == total        (the omitted counts are always true)
///   count + markers        <= budget       (the reserved rows are always seated)
///   markers == 0 whenever before + after == 0
/// `markers` is how many rows the policy reserved for saying a cut: two policies reserve one per
/// cut side, one reserves a single row for both. When `before` or `after` is non-zero and no row
/// was reserved for it, the consumer sees that in the counts and says it in a heading, or not.
struct ListWindow {
    std::size_t first = 0;
    std::size_t count = 0;
    std::size_t before = 0;
    std::size_t after = 0;
    std::size_t markers = 0;

    std::size_t end() const noexcept { return first + count; }
    bool shows(std::size_t i) const noexcept { return i >= first && i < end(); }
    std::size_t marker_rows() const noexcept { return markers; }
    /// A CUT THE WINDOW COULD NOT RESERVE A ROW FOR -- the consumer's to say elsewhere.
    bool unsaid_cut() const noexcept { return (before > 0 || after > 0) && markers == 0; }
};

namespace detail {

inline ListWindow nothing_shown(std::size_t total, std::size_t at) noexcept {
    ListWindow w;
    if (at > total) {
        at = total;
    }
    w.first = at;
    w.before = at;
    w.after = total - at;
    return w;
}

} // namespace detail

/// A CURSOR-ANCHORED, LEAST-MOTION WINDOW (the desktop's `window_for`, WL-DESK-10): the largest
/// window that keeps `cursor` visible inside `budget` rows with one marker row reserved per cut
/// side, and among equally large ones the one whose first row is nearest `hint` (last time's
/// first row), so a list scrolls by the least it can rather than jumping.
///
/// TOTAL: a cursor past the population is read as its last row; a budget of zero shows nothing
/// and counts everything; a budget that cannot seat the cursor's row beside the markers it would
/// need (one or two rows, members hidden on both sides) shows the cursor's row alone with NO
/// marker reserved and the counts still true (`unsaid_cut`).
inline ListWindow cursor_window(std::size_t total, std::size_t cursor, std::size_t hint,
                                std::size_t budget) noexcept {
    if (total == 0) {
        return ListWindow{};
    }
    if (cursor >= total) {
        cursor = total - 1;
    }
    if (budget == 0) {
        return detail::nothing_shown(total, cursor);
    }
    if (total <= budget) {
        ListWindow all;
        all.count = total;
        return all;
    }
    ListWindow best;
    bool found = false;
    std::size_t best_distance = 0;
    for (std::size_t first = 0; first <= cursor; ++first) {
        for (std::size_t count = budget; count >= 1; --count) {
            if (first + count > total || cursor >= first + count) {
                continue;
            }
            const std::size_t above = first > 0 ? 1u : 0u;
            const std::size_t below = first + count < total ? 1u : 0u;
            if (count + above + below > budget) {
                continue;
            }
            const std::size_t distance = first > hint ? first - hint : hint - first;
            if (!found || count > best.count ||
                (count == best.count && distance < best_distance)) {
                best.first = first;
                best.count = count;
                best.before = first;
                best.after = total - first - count;
                best.markers = above + below;
                best_distance = distance;
                found = true;
            }
            break; // the largest count for this `first`; a smaller one is never better
        }
    }
    if (!found) {
        // ONE OR TWO ROWS, CUT ON BOTH SIDES: the cursor's row, and the truth in the counts.
        best = ListWindow{};
        best.first = cursor;
        best.count = 1;
        best.before = cursor;
        best.after = total - cursor - 1;
        best.markers = 0;
    }
    return best;
}

/// A CURSOR-ANCHORED, CENTRED WINDOW WITH ONE MARKER ROW FOR BOTH SIDES (Powers' and the
/// Composer's `window_of`, carried): the cursor sits at the middle of `budget - 1` rows, and the
/// last row says what is hidden above and below in one sentence. A one-row budget shows the
/// marker and no member, as those consumers do today; a zero budget shows nothing.
inline ListWindow centred_window(std::size_t total, std::size_t cursor,
                                 std::size_t budget) noexcept {
    if (total == 0) {
        return ListWindow{};
    }
    if (cursor >= total) {
        cursor = total - 1;
    }
    if (budget == 0) {
        return detail::nothing_shown(total, cursor);
    }
    if (total <= budget) {
        ListWindow all;
        all.count = total;
        return all;
    }
    ListWindow w;
    if (budget == 1) {
        w = detail::nothing_shown(total, 0); // the marker, and no room for a member beside it
        w.markers = 1;
        return w;
    }
    w.count = budget - 1; // one row of the budget is the omission marker's
    std::size_t first = cursor >= w.count / 2 ? cursor - w.count / 2 : 0;
    if (first > total - w.count) {
        first = total - w.count;
    }
    w.first = first;
    w.before = first;
    w.after = total - first - w.count;
    w.markers = 1;
    return w;
}

/// AN OFFSET-ANCHORED WINDOW -- a scroll, not a cursor (the Hotkeys pane's `say_keys`): `top`
/// is the first member the maker asked to see, clamped so the last page is full, and each cut
/// side reserves a row for its marker.
///
/// TOTAL: a budget under three rows cannot seat a member between two markers, so it shows up to
/// `budget` members from `top` with no marker reserved and the counts true (`unsaid_cut`).
inline ListWindow scroll_window(std::size_t total, std::size_t top, std::size_t budget) noexcept {
    if (total == 0) {
        return ListWindow{};
    }
    if (top >= total) {
        top = total - 1;
    }
    if (budget == 0) {
        return detail::nothing_shown(total, top);
    }
    if (total <= budget) {
        ListWindow all;
        all.count = total;
        return all;
    }
    ListWindow w;
    if (budget < 3) {
        const std::size_t remaining = total - top;
        w.first = top;
        w.count = remaining < budget ? remaining : budget;
        w.before = top;
        w.after = total - top - w.count;
        w.markers = 0;
        return w;
    }
    const std::size_t last_top = total - (budget - 1); // the final page: `^` and the rest
    if (top > last_top) {
        top = last_top;
    }
    std::size_t inner = budget - (top > 0 ? 1u : 0u);
    const bool below = top + inner < total;
    inner -= below ? 1u : 0u;
    w.first = top;
    w.count = inner;
    w.before = top;
    w.after = total - top - inner;
    w.markers = (top > 0 ? 1u : 0u) + (below ? 1u : 0u);
    return w;
}

/// WHERE A SCROLL LANDS after `by` steps (negative is up), clamped to the population. The
/// consumer stores the answer as its offset; `scroll_window` clamps it again against the budget
/// it has at the next composition.
inline std::size_t scroll_by(std::size_t total, std::size_t top, std::int64_t by) noexcept {
    if (total == 0) {
        return 0;
    }
    std::int64_t at = static_cast<std::int64_t>(top) + by;
    if (at < 0) {
        at = 0;
    }
    if (at >= static_cast<std::int64_t>(total)) {
        at = static_cast<std::int64_t>(total) - 1;
    }
    return static_cast<std::size_t>(at);
}

} // namespace zengine::component

#endif // ZENGINE_COMPONENT_LIST_WINDOW_HPP
