// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INTROSPECTION_LOADED_HPP
#define ZENGINE_INTROSPECTION_LOADED_HPP

// THE LOADED-WEAVE VIEW, as two pure functions.
//
//     parse_loaded    the Weave Manager's answer  ->  the facts it carries
//     project_loaded  the facts + a prose budget  ->  the rows a maker reads
//
// NEITHER TOUCHES A BUS, and that is the point of the file. The provider weave
// beside it owns WHEN to observe and WHOM to believe; this owns what the answer
// MEANS and how it is spent, so both questions can be asked of a value in a test
// instead of of a running system.
//
// ---- WHAT IS BEING PROJECTED, EXACTLY ---------------------------------------
//
// `zen.ListLoaded` is answered from `Kernel::loaded()` -- the kernel's own live
// map of the libraries it opened, never a cache (zen/kernel/control.hpp). So the
// population is DYNAMICALLY LOADED WEAVES and nothing else. Every weave the host
// mounted in-process -- Workshop's own, the boot weave, the control door, the
// Weave Manager itself, the Builder tool, the build runner, the terminal
// participant -- is a live participant that this list does not contain and cannot
// speak about.
//
// THAT ABSENCE IS SAID ON THE CANVAS, in `kNotInProcess` below, and it is said
// before the list is offered a single row rather than when there is room left
// over. A count with an unstated population is the exact shape of an honest
// number that leaves a false picture: a maker reading `loaded weaves -- 4` beside
// a running Builder would be right to conclude the Builder is not running.
// Absence is a fact only if it was observed, and what was observed here is the
// kernel's map -- so the sentence bounding it is not decoration, it is half of
// the fact.
//
// ---- WHAT IS NOT HERE -------------------------------------------------------
//
// No mirror of the inventory is kept anywhere. `parse_loaded` returns a value the
// caller spends immediately and drops; there is no `known_weaves_`, no diff
// against a previous reading, no arrival/departure derivation, and no timestamp --
// this weave holds no clock, and inventing one out of a message it happened to
// receive would be a story rather than an observation.

#include "surface/vocabulary.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::introspection {

/// ONE LOADED WEAVE, as the kernel knows it: the name the library was loaded
/// under, and the role it was bound to at load, if any.
///
/// AN EMPTY `role` IS AN OBSERVED ABSENCE, not a missing reading. `LoadLibrary`
/// carries a role field that may be empty and the kernel binds one only when it is
/// not, so an empty answer is the kernel saying "this one holds none" -- which is
/// why the projection is allowed to write `(no role)` rather than leaving the
/// column blank.
struct LoadedWeave {
    std::string name;
    std::string role;
};

/// WHAT THIS LIST DOES NOT CONTAIN. Workshop's own header already says whose pane
/// this is; this says whose facts these are NOT.
inline constexpr const char* kNotInProcess = "in-process weaves are not in the kernel's map";

/// WHERE IT CAME FROM AND HOW OLD IT IS, in one line.
///
/// `snapshot` is the honest word and it is the first one on the row: this provider
/// re-reads the map when Workshop grants it room and at no other moment, because
/// Loom gives a participant no arrival or departure event to listen for, and a
/// consumer that polled for one would be a consumer polling. So between two grants
/// these rows are a reading and not a feed, and the pane says which.
inline constexpr const char* kSnapshotSource = "snapshot from zen.ListLoaded, on room grant";

/// The mark this view leaves where it could not show everything -- Workshop's own
/// three plain characters, for their reason: this canvas is plain ASCII by
/// contract and a glyph a medium cannot draw is a mark a maker cannot read.
inline constexpr const char* kElided = "...";

/// What a row says instead of a role when the kernel bound none.
inline constexpr const char* kNoRole = "(no role)";

/// Fit `text` into `columns`, AND SAY SO when it did not fit.
///
/// A SECOND COPY OF `workshop::detail::fit`'S FEW LINES, DELIBERATELY. That one
/// lives inside `workshop/screen.hpp` -- Workshop's private composition, a quarter
/// of a megabyte of it -- and a provider is a STRANGER to that file by design: the
/// external contract is four shapes and a budget. Reaching into it to save nine
/// lines would make every external tool a consumer of Workshop's internals, which
/// is precisely the coupling the pane seam exists to refuse.
///
/// It is not a second MEASURER either, which is the thing that would matter. The
/// measuring was done once by `surface::fit_region`, on Workshop's side, and
/// arrived here as a number. This only spends it.
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

/// THE MANAGER'S ANSWER, READ.
///
/// The wire form is `zen.Result` carrying `name@role,name,name@role` -- a
/// comma-joined blob assembled by the control door out of `Kernel::loaded()` and
/// `Kernel::role_of()`. It is the only enumeration Loom offers a participant, and
/// reading it is the whole of this function.
///
/// TWO PROPERTIES OF THAT WIRE FORM ARE WORTH STATING RATHER THAN DISCOVERING:
///
///   - IT HAS NO ESCAPING. A library name containing a comma or an at-sign would
///     be read wrong here, and there is no reading that would get it right,
///     because the producer emitted no delimiter of its own. The split is on the
///     LAST at-sign so a name carrying one still resolves against a role that does
///     not, which is the ambiguity's better half rather than a solution to it.
///     Every name in this system is a CMake target stem and every role is a dotted
///     identifier, so nothing reachable today spends the ambiguity -- but a
///     projection that silently mangled a name would be a lie, so it is written
///     down where the reading happens.
///   - AN EMPTY BLOB IS AN EMPTY MAP, and the caller may say so. The control door
///     never refuses this shape; it answers from the live map every time. So zero
///     is an observed count and not a silence -- the silence, if there is one, is
///     the answer never arriving, and that is a state this function is never
///     called in.
///
/// THE ORDER IS THE KERNEL'S AND IS NOT SORTED HERE. `Kernel::loaded()` walks a
/// `std::map` keyed by library name, so the answer arrives in name order, the same
/// order every run, independent of the host's boot sequence. Nothing here reorders
/// it: a view that sorted a list its owner already ordered would be a second
/// opinion about a fact, and the windowing below would then hide entries by a rule
/// the owner never applied.
///
/// TOTAL. An empty entry is skipped rather than yielding a nameless row, because a
/// blank line in an inventory is indistinguishable from a weave whose name did not
/// survive the trip.
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

/// ONE ROW PER WEAVE: what it is called, and the role it holds.
///
/// INDENTED BY TWO so the list reads as a list under its heading, which is the
/// Info panel's own spelling for a run of rows inside a region.
inline std::string entry_row(const LoadedWeave& w, std::int64_t columns) {
    return fit("  " + w.name + " @" + (w.role.empty() ? std::string(kNoRole) : w.role), columns);
}

/// THE WHOLE VIEW, spent against the room Workshop granted.
///
/// THE PRIORITY ORDER IS THE POINT, and it is this, most-protected first:
///
///     the heading          what this is, and how many there are
///     `kNotInProcess`      what this is NOT -- half of what the count means
///     the list             windowed, with every omission counted
///     `kSnapshotSource`    where it came from and how old it is
///     one blank row        only out of room nothing else wanted
///
/// THE CAVEAT IS RESERVED BEFORE THE LIST IS OFFERED ANYTHING BUT ITS FIRST ROW,
/// which is HD-8's reservation argument in a second place: a fixed demand and a
/// variable one do not share a budget, they are subtracted in that order. A pane
/// too short to hold both its inventory and the sentence that bounds it keeps the
/// sentence -- an inventory a maker would read as the whole system is worse than
/// an inventory with a row missing, and the missing rows are counted out loud
/// where the names would have been.
///
/// AND THE LIST'S FIRST ROW OUTRANKS THE CAVEAT, because a pane showing only its
/// own small print has stopped being a view of anything.
///
/// ONE ENTRY AND ITS MARKER ARE ONE DEMAND, and getting that wrong is what this
/// function was measured doing: reserving a single row for "the list" bought a row
/// the marker then took, so a four-row body spent two rows on notes and showed no
/// weave at all. Showing PART of a list obliges saying how much was hidden, so the
/// two rows are claimed together or neither is -- which is why the arithmetic below
/// hands the list a budget and lets it decide, rather than counting its rows here.
///
/// THE HEADING IS THE FLOOR OF THE ACCOUNTING. At a one-row body there is no list
/// and no note, and the population is still stated -- so even there nothing is
/// hidden without being counted.
///
/// TOTAL over every budget, including ones no pane has. Zero rows or zero columns
/// is an empty projection; and being exactly inside the grant is this function's
/// obligation rather than a courtesy, because Workshop refuses an over-budget
/// update WHOLE and a provider that does not measure loses everything it said.
inline std::vector<surface::SurfaceTextRow> project_loaded(const std::vector<LoadedWeave>& weaves,
                                                           std::int64_t rows,
                                                           std::int64_t columns) {
    std::vector<surface::SurfaceTextRow> out;
    if (rows <= 0 || columns <= 0) {
        return out;
    }
    out.push_back(surface::SurfaceTextRow{
        fit("loaded weaves -- " + std::to_string(weaves.size()), columns), surface::role::kAccent});

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
        for (std::size_t i = 0; i < shown; ++i) {
            out.push_back(
                surface::SurfaceTextRow{entry_row(weaves[i], columns), surface::role::kFill});
        }
        budget -= static_cast<std::int64_t>(shown);
        if (shown < weaves.size()) {
            out.push_back(surface::SurfaceTextRow{
                fit("  " + std::string(kElided) + " " + std::to_string(weaves.size() - shown) +
                        " more",
                    columns),
                surface::role::kMuted});
            --budget;
        }
    }
    // A spare row nothing else wanted separates the list from the small print. It
    // is the LAST claim on the budget, so it never costs an entry or a note.
    if (budget > 0 && caveat > 0) {
        // `role::kFill`, which is the default, and NOT `role::kNone`: that value is
        // the absence of a BACKGROUND and is negative so an unknown-role fallback
        // cannot swallow it. Nothing may hand it to a Skin's role-to-ink table, and
        // a row's `role` field is exactly that table's argument.
        out.push_back(surface::SurfaceTextRow{std::string(), surface::role::kFill});
    }
    if (caveat > 0) {
        out.push_back(surface::SurfaceTextRow{fit(kNotInProcess, columns), surface::role::kMuted});
    }
    if (source > 0) {
        out.push_back(surface::SurfaceTextRow{fit(kSnapshotSource, columns), surface::role::kMuted});
    }
    return out;
}

} // namespace zengine::introspection

#endif // ZENGINE_INTROSPECTION_LOADED_HPP
