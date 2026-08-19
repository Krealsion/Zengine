// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_INTROSPECTION_LOADED_HPP
#define ZENGINE_INTROSPECTION_LOADED_HPP

// THE LOADED-WEAVE VIEW, as four pure functions.
//
//     parse_loaded    the Weave Manager's answer  ->  the facts it carries
//     project_loaded  the facts + a prose budget  ->  the rows a maker reads,
//                                                     AND which entry each row names
//     mark_selected   a view + one entry's name   ->  the same view, one row marked
//     names           the facts + a name          ->  is this entry still among them
//
// NONE OF THEM TOUCHES A BUS, and that is the point of the file. The provider
// weave beside it owns WHEN to observe and WHOM to believe; this owns what the
// answer MEANS and how it is spent, so both questions can be asked of a value in
// a test instead of of a running system.
//
// ---- THE ROW MAP IS THE ONE THING SEL-0 ADDED, AND WHY IT IS HERE ------------
//
// A press arrives as a ROW of the room this pane was granted, and the pane has to
// answer which observed entry -- if any -- it drew there. That question is
// `project_loaded` read backwards, and the whole reason it is answered by
// `project_loaded` ITSELF rather than by a second function beside it is the rule
// HD-3 paid for once already: THE GEOMETRY THAT DREW A THING AND THE GEOMETRY
// THAT HITS IT MUST BE ONE FUNCTION. A separate row-to-entry calculation would
// agree with the picture until the first list that had to window, which is to say
// it would be wrong only when nobody was looking.
//
// So `project_loaded` returns a `LoadedView`: the rows, the entries that actually
// reached one, and a per-row index between them. The provider retains that value
// and NOTHING ELSE -- see the next section, which the retention changes.
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
// STILL NO MIRROR OF THE INVENTORY, and SEL-0 is the phase most likely to be
// misread as having added one, so the distinction is written here rather than
// left to be inferred:
//
//     an INVENTORY   is a second answer to "what is loaded", which the Kernel's
//                    map already owns. Keeping one would put a copy beside the
//                    authority, and the copy is what goes stale.
//     a PROJECTION   is what THIS PANE IS CURRENTLY SHOWING -- the same kind of
//                    thing Workshop keeps in `ExternalPane::shown` one layer out,
//                    and the same kind of thing every list in this application
//                    holds while a maker is looking at it.
//
// The provider retains the second and never the first. It answers "which entry
// did the maker press" and is incapable of answering "what is loaded now": it is
// bounded by the granted ROOM rather than by the population, it is dropped the
// moment a new room is granted, and it is replaced whole by the next reading.
//
// `parse_loaded` is still a value the caller spends and drops. There is no
// `known_weaves_`, no diff against a previous reading, no arrival or departure
// derivation, and no timestamp -- this weave holds no clock, and inventing one
// out of a message it happened to receive would be a story rather than an
// observation.

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

/// THE TWO CHARACTERS AT THE HEAD OF EVERY ENTRY ROW -- one of them, per row.
///
/// `>` IS THE STATEMENT AND THE ACCENT ROLE IS THE SECOND SIGNAL, never the other
/// way round. Workshop's own two lists (`object_row_text`, `completion_rows`) both
/// spell selection exactly this way and both write down the reason: a mark is what
/// says "this one" on a medium with no colour at all, and a colour alone would be a
/// selection a maker in a monochrome terminal could not see. This provider is a
/// stranger to those functions and re-spells the convention rather than reaching for
/// them -- the same trade `fit` below makes and for the same reason.
///
/// AND THE MARK COSTS NO COLUMNS. The unselected row was already indented by two, so
/// selecting one exchanges two spaces for a mark and a space: the width of every row
/// is identical selected or not, no budget moves, and a list cannot start cutting
/// names because something in it became selected.
inline constexpr const char* kSelectedMark = "> ";
inline constexpr const char* kUnselectedMark = "  ";

/// THE ROW OF A `LoadedView` THAT NAMES NO ENTRY -- a heading, the caveat, the source
/// line, the blank separator, or the omission marker.
///
/// NEGATIVE for `role::kNone`'s and `kNoCaret`'s reason: an index into `shown` is
/// non-negative by construction, so nothing a later projection might mean can collide
/// with it, and a consumer that forgot to test would index far outside the vector
/// rather than into its first entry.
inline constexpr std::int64_t kNoEntry = -1;

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

/// ONE ROW PER WEAVE: whether it is the selected one, what it is called, and the
/// role it holds.
///
/// INDENTED BY TWO so the list reads as a list under its heading, which is the
/// Info panel's own spelling for a run of rows inside a region -- and since SEL-0
/// the two characters are a MARK when this is the entry the maker selected.
///
/// THE WHOLE ROW IS FITTED, MARK INCLUDED, which is what keeps the marked and
/// unmarked forms the same width at every column count: `fit` cuts the tail, and
/// the two characters this list needs in order to be readable at all are at the
/// head where a cut cannot reach them.
///
/// IT IS THE ONLY PLACE AN ENTRY ROW IS SPELLED. `project_loaded` builds rows with
/// it and `mark_selected` rebuilds rows with it, so there is exactly one answer to
/// "what does a selected row look like" and no way for the two to drift.
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

/// IS THIS NAME STILL AMONG THESE ENTRIES -- the one question a held selection asks
/// of a fresh reading.
///
/// IT ASKS THE POPULATION AND NOT THE PROJECTION, which is the whole of why it takes
/// a `std::vector<LoadedWeave>` rather than a `LoadedView`. An entry that is loaded
/// but currently windowed out of a short pane is PRESENT and merely unshown; an entry
/// the kernel no longer has is GONE. Asking the rows would confuse the two and clear a
/// maker's selection every time they made a pane small.
///
/// BY NAME, because a library's name is what the kernel's map is keyed by, and it is
/// the identity `LoadedSelected` carries. A role is an attribute of an entry: a weave
/// that was rebound between two readings is the same loaded library, still selected.
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

/// THE PANE'S CONTENT AND THE MAP BACK FROM IT (SEL-0).
///
/// `rows` is what Workshop is sent and what a maker reads. `shown` is the entries
/// that actually reached one of those rows, in row order. `entry_of_row` is parallel
/// to `rows` and holds an index into `shown`, or `kNoEntry` for every row that names
/// no entry -- the heading, the caveat, the source line, the blank separator, and the
/// omission marker.
///
/// THE OMISSION MARKER IS `kNoEntry` AND THAT IS LOAD-BEARING. `... 35 more` is a
/// POPULATION FACT -- it says how much of the list is not here -- and it is not a
/// stand-in for one hidden entry. A maker who presses it has pressed a sentence about
/// a count, and the honest answer is that no entry was selected. Making it select the
/// first hidden one would invent a gesture nothing in this pane offers.
///
/// IT IS BOUNDED BY THE ROOM, NOT BY THE POPULATION. `shown` can never hold more
/// entries than the granted rows could show, so a provider retaining this value
/// retains a pane's worth of strings whatever the kernel has loaded.
struct LoadedView {
    std::vector<surface::SurfaceTextRow> rows;
    std::vector<LoadedWeave> shown;
    std::vector<std::int64_t> entry_of_row;
};

/// WHICH ENTRY A ROW OF THIS VIEW NAMES, or nothing.
///
/// TOTAL over every row index, including ones no press can produce: a row outside the
/// view is `nullptr`, exactly as a row that names no entry is. A provider is handed a
/// row off a wire and must not have to bound it twice.
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

/// MARK THE ROW NAMING `selected`, AND UNMARK EVERY OTHER ENTRY ROW.
///
/// THE ONE PLACE SELECTION BECOMES VISIBLE, and it is separate from `project_loaded`
/// on purpose: a press changes which entry is marked and changes nothing else at all
/// -- not the population, not the count, not the window, not the omission, not a
/// single non-entry row. Rebuilding the projection to move a mark would re-read
/// nothing and re-decide everything, and the first thing it would get wrong is the
/// snapshot a maker is looking at.
///
/// AN EMPTY `selected`, OR A NAME NO SHOWN ENTRY CARRIES, LEAVES EVERY ROW UNMARKED --
/// which is exactly what a pane whose selected entry is currently windowed out should
/// look like. The selection is still held by the provider; there is simply no row here
/// to put a mark on, and the mark returns with the entry.
///
/// `columns` IS THE ROOM THE VIEW WAS PROJECTED FOR. It is passed rather than
/// remembered because a `LoadedView` is not a place and holds no budget; the caller
/// holds the room, and handing this a different one would produce rows the room it was
/// built for cannot carry.
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
/// IT PROJECTS UNMARKED AND SAYS WHICH ROW IS WHICH (SEL-0). Selection is applied
/// afterwards by `mark_selected`, so this function is unchanged in every decision it
/// ever made: the count, the reservation, the window, the omission and the roles of
/// every non-entry row are what they were, and a selected pane is the same pane with
/// two characters and one row's ink different. The map it now returns beside the rows
/// is what a press is read through.
///
/// TOTAL over every budget, including ones no pane has. Zero rows or zero columns
/// is an empty projection; and being exactly inside the grant is this function's
/// obligation rather than a courtesy, because Workshop refuses an over-budget
/// update WHOLE and a provider that does not measure loses everything it said.
inline LoadedView project_loaded(const std::vector<LoadedWeave>& weaves, std::int64_t rows,
                                 std::int64_t columns) {
    LoadedView view;
    // EVERY ROW APPENDED IS PAIRED WITH ONE MAP ENTRY, and this closure
    // is what makes forgetting one impossible rather than merely unlikely: the two
    // vectors are one row list in two pieces, and a projection whose map was one entry
    // short would misread every press below the row that went missing.
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
        for (std::size_t i = 0; i < shown; ++i) {
            // THE ENTRY IS COPIED INTO `shown` AS IT IS DRAWN, so what the map points at
            // is the observation this row was made from and not a second lookup into a
            // population the caller may already have dropped. That is the whole of what
            // makes a later press answer with WHAT THE MAKER SAW.
            view.shown.push_back(weaves[i]);
            say(surface::SurfaceTextRow{entry_row(weaves[i], false, columns), entry_role(false),
                                        entry_ground(false)},
                static_cast<std::int64_t>(view.shown.size()) - 1);
        }
        budget -= static_cast<std::int64_t>(shown);
        if (shown < weaves.size()) {
            // `kNoEntry`: a population fact, not a hidden entry. See `LoadedView`.
            say(surface::SurfaceTextRow{
                    fit("  " + std::string(kElided) + " " + std::to_string(weaves.size() - shown) +
                            " more",
                        columns),
                    surface::role::kMuted},
                kNoEntry);
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
