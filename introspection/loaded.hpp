// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INTROSPECTION_LOADED_HPP
#define ZENGINE_INTROSPECTION_LOADED_HPP

// The Loaded pane's view, as pure functions: `parse_loaded` (the Weave Manager's answer, read),
// `project_loaded` (facts and a budget become rows, and which entry each row names),
// `mark_selected` and `names`. No bus here. The rows and their entry map are one projection,
// since the geometry that draws a row and the one that hits it must be one function. The
// population is the Kernel's loaded() map -- dynamically loaded weaves only -- said first.
// Pane law: agents/panes.md

// The provider keeps what this pane shows, never an inventory: bounded by the room, dropped at
// every grant, replaced whole by the next reading. No mirror of what is loaded, no diff, no
// arrival or departure, no timestamp.

#include "surface/vocabulary.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::introspection {

/// One loaded weave as the kernel knows it: the library name and the role bound at load. An
/// empty `role` is an observed absence (the kernel bound none), so the row may say `(no role)`.
struct LoadedWeave {
    std::string name;
    std::string role;
};

/// WHAT THIS LIST DOES NOT CONTAIN. Workshop's own header already says whose pane
/// this is; this says whose facts these are NOT.
inline constexpr const char* kNotInProcess = "in-process weaves are not in the kernel's map";

/// Where it came from and how old it is: `snapshot` first, since the map is re-read only on a
/// room grant or a wheel move -- Loom offers no arrival or departure event.
inline constexpr const char* kSnapshotSource = "snapshot from zen.ListLoaded";

/// The mark this view leaves where it could not show everything -- Workshop's own
/// three plain characters, for their reason: this canvas is plain ASCII by
/// contract and a glyph a medium cannot draw is a mark a maker cannot read.
inline constexpr const char* kElided = "...";

/// What a row says instead of a role when the kernel bound none.
inline constexpr const char* kNoRole = "(no role)";

/// The two characters heading every entry row. `>` is the statement and the accent the second
/// signal, since colour alone is invisible on a monochrome terminal. The mark replaces the
/// indent, so selecting costs no columns and cuts no name.
inline constexpr const char* kSelectedMark = "> ";
inline constexpr const char* kUnselectedMark = "  ";

/// The row of a `LoadedView` that names no entry (heading, caveat, source line, blank, omission
/// marker). Negative, so it cannot collide with an index into `shown`.
inline constexpr std::int64_t kNoEntry = -1;

/// Fit `text` into `columns`, and say so when it did not fit. A second copy of
/// `workshop::detail::fit`, deliberately: a provider is a stranger to Workshop's private
/// composition. Not a second measurer: `surface::fit_region` measured, and this spends it.
inline std::string fit(std::string text, std::int64_t columns) {
    if (columns <= 0) {
        return {};
    }
    const std::size_t room = static_cast<std::size_t>(columns);
    if (text.size() <= room) {
        return text;
    }
    const std::size_t mark = std::char_traits<char>::length(kElided);
    if (room <= mark) {
        return std::string(kElided).substr(0, room);
    }
    text.resize(room - mark);
    text += kElided;
    return text;
}

/// The Manager's answer, read: `name@role,name,...` from `Kernel::loaded()`. The wire form has no
/// escaping, so a name with a comma or an at-sign cannot be read right; the split is on the
/// last `@`, the ambiguity's better half. An empty blob is an observed empty map. The order is
/// the kernel's (name order) and is not sorted here. Empty entries are skipped.
inline std::vector<LoadedWeave> parse_loaded(std::string_view blob) {
    std::vector<LoadedWeave> out;
    std::size_t at = 0;
    for (;;) {
        const std::size_t comma = blob.find(',', at);
        const std::size_t end = comma == std::string_view::npos ? blob.size() : comma;
        const std::string_view entry = blob.substr(at, end - at);
        if (!entry.empty()) {
            const std::size_t sep = entry.rfind('@');
            if (sep == std::string_view::npos) {
                out.push_back(LoadedWeave{std::string(entry), std::string()});
            } else {
                out.push_back(LoadedWeave{std::string(entry.substr(0, sep)),
                                          std::string(entry.substr(sep + 1))});
            }
        }
        if (comma == std::string_view::npos) {
            break;
        }
        at = comma + 1;
    }
    return out;
}

/// One row per weave: the mark, the name and the role, fitted whole so marked and unmarked rows
/// are one width. The only place an entry row is spelled (`project_loaded`, `mark_selected`).
inline std::string entry_row(const LoadedWeave& w, bool chosen, std::int64_t columns) {
    return fit(std::string(chosen ? kSelectedMark : kUnselectedMark) + w.name + " @" +
                   (w.role.empty() ? std::string(kNoRole) : w.role),
               columns);
}

/// The semantic roles an entry row carries, selected and not. Two lines, in one
/// place, for `entry_row`'s reason exactly.
inline constexpr std::int64_t entry_role(bool chosen) noexcept {
    return chosen ? surface::role::kAccent : surface::role::kFill;
}
inline constexpr std::int64_t entry_ground(bool chosen) noexcept {
    return chosen ? surface::role::kMuted : surface::role::kNone;
}

/// Is this name still among these entries? Asked of the population, not the projection: a
/// windowed-out entry is present, a departed one is gone. By name, the kernel's key; a rebound
/// role is the same library.
inline bool names(const std::vector<LoadedWeave>& weaves, std::string_view name) {
    if (name.empty()) {
        return false;
    }
    for (const LoadedWeave& w : weaves) {
        if (w.name == name) {
            return true;
        }
    }
    return false;
}

/// The pane's content and the map back from it: `rows` go to Workshop, `shown` are the entries
/// that reached a row, and `entry_of_row` indexes `shown` or is `kNoEntry`. The omission marker
/// is `kNoEntry`: a count, not a hidden entry. Bounded by the room, never by the population.
struct LoadedView {
    std::vector<surface::SurfaceTextRow> rows;
    std::vector<LoadedWeave> shown;
    std::vector<std::int64_t> entry_of_row;
};

/// Which entry a row names, or nothing; total over every row, since a provider is handed a row
/// off a wire.
inline const LoadedWeave* entry_at_row(const LoadedView& view, std::int64_t row) {
    if (row < 0 || row >= static_cast<std::int64_t>(view.entry_of_row.size())) {
        return nullptr;
    }
    const std::int64_t which = view.entry_of_row[static_cast<std::size_t>(row)];
    if (which == kNoEntry || which >= static_cast<std::int64_t>(view.shown.size())) {
        return nullptr;
    }
    return &view.shown[static_cast<std::size_t>(which)];
}

/// Mark the row naming `selected` and unmark every other entry row -- nothing else moves, so a
/// press re-decides nothing. A name no shown row carries leaves all unmarked, and the mark
/// returns with the entry. `columns` is the room the view was projected for.
inline void mark_selected(LoadedView& view, std::string_view selected, std::int64_t columns) {
    for (std::size_t i = 0; i < view.rows.size() && i < view.entry_of_row.size(); ++i) {
        const std::int64_t which = view.entry_of_row[i];
        if (which == kNoEntry || which >= static_cast<std::int64_t>(view.shown.size())) {
            continue;
        }
        const LoadedWeave& w = view.shown[static_cast<std::size_t>(which)];
        const bool chosen = !selected.empty() && w.name == selected;
        view.rows[i].text = entry_row(w, chosen, columns);
        view.rows[i].role = entry_role(chosen);
        view.rows[i].background = entry_ground(chosen);
    }
}

/// The whole view, spent against the granted room, most-protected first: the heading and count,
/// `kNotInProcess`, the list (windowed, omissions counted), `kSnapshotSource`, one blank. The
/// caveat is reserved before the list gets more than its first row, since a list read as the
/// whole system is worse than one row short; an entry and its marker are one demand. Projected
/// unmarked (`mark_selected` applies a selection), and inside the grant, since Workshop refuses
/// an over-budget update whole.
inline LoadedView project_loaded(const std::vector<LoadedWeave>& weaves, std::int64_t rows,
                                 std::int64_t columns, std::size_t origin = 0) {
    LoadedView view;
    // Every row appended pairs with one map entry, so no row can lose its entry.
    const auto say = [&view](surface::SurfaceTextRow row, std::int64_t which) {
        view.rows.push_back(std::move(row));
        view.entry_of_row.push_back(which);
    };
    if (rows <= 0 || columns <= 0) {
        return view;
    }
    say(surface::SurfaceTextRow{fit("loaded weaves -- " + std::to_string(weaves.size()), columns),
                                surface::role::kAccent},
        kNoEntry);

    std::int64_t left = rows - 1;
    // The caveat, unless taking it would leave the list with no row at all.
    const std::int64_t caveat = left >= 1 && (weaves.empty() || left >= 2) ? 1 : 0;
    left -= caveat;
    // ...and the source line only out of GENUINE slack: the population must fit whole
    // and still leave a row over. A pane that had to window its inventory spends that
    // row on the inventory instead, and keeps the caveat it already reserved.
    const std::int64_t source =
        caveat == 1 && static_cast<std::int64_t>(weaves.size()) < left ? 1 : 0;
    std::int64_t budget = left - source;

    if (budget > 0 && !weaves.empty()) {
        const std::size_t room = static_cast<std::size_t>(budget);
        const std::size_t shown = weaves.size() <= room ? weaves.size() : room - 1;
        origin = std::min(origin, weaves.size() - shown);
        for (std::size_t i = origin; i < origin + shown; ++i) {
            // Copied into `shown` as it is drawn, so a press answers with what the maker saw.
            view.shown.push_back(weaves[i]);
            say(surface::SurfaceTextRow{entry_row(weaves[i], false, columns), entry_role(false),
                                        entry_ground(false)},
                static_cast<std::int64_t>(view.shown.size()) - 1);
        }
        budget -= static_cast<std::int64_t>(shown);
        if (shown < weaves.size()) {
            // `kNoEntry`: a population fact, not a hidden entry. See `LoadedView`.
            say(surface::SurfaceTextRow{
                    fit("  " + std::string(kElided) + " " +
                            (origin ? std::to_string(origin) + " earlier, " : "") +
                            std::to_string(weaves.size() - origin - shown) + " more",
                        columns),
                    surface::role::kMuted},
                kNoEntry);
            --budget;
        }
    }
    // A spare row nothing else wanted separates the list from the small print. It
    // is the LAST claim on the budget, so it never costs an entry or a note.
    if (budget > 0 && caveat > 0) {
        // `role::kFill`, not `role::kNone`: that is the absence of a background, never an ink.
        say(surface::SurfaceTextRow{std::string(), surface::role::kFill}, kNoEntry);
    }
    if (caveat > 0) {
        say(surface::SurfaceTextRow{fit(kNotInProcess, columns), surface::role::kMuted}, kNoEntry);
    }
    if (source > 0) {
        say(surface::SurfaceTextRow{fit(kSnapshotSource, columns), surface::role::kMuted},
            kNoEntry);
    }
    return view;
}

} // namespace zengine::introspection

#endif // ZENGINE_INTROSPECTION_LOADED_HPP
