// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_FILES_BROWSER_VIEW_HPP
#define ZENGINE_FILES_BROWSER_VIEW_HPP

// The Files pane's browser as a weaver sees it: the members of `FilesWeave` that lay out its
// header and the path in it, the places section, the listing's window and the control strip under
// it, and record what each row and face means.
// A part of `files/files.cpp`, which includes it after `FilesWeave` and its aliases; nothing
// else includes it.
// Files law: agents/workshop/files.md

#include "files/files.hpp"
#include "files/marks.hpp"
#include "files/vocabulary.hpp"
#include "workshop/path_admission.hpp"
#include "workshop/pane_rows.hpp"
#include "workshop/pane_text.hpp"

#include "component/accordion.hpp"
#include "component/list_window.hpp"
#include "graphics/role.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

/// One directory row's text.
inline std::string row_text(const FileRow& row) {
    std::string out = ws::shown_name(row.name);
    if (row.directory) {
        out += "/";
        if (row.linked) {
            out += "  (link)";
        }
    }
    if (!row.openable) {
        out += "  (name this Workshop cannot open)";
    }
    return out;
}

// ---- The listing's budget, and the controls a weaver can press ----------------------------

/// HOW MANY ROWS THE LISTING MAY SPEND: the room, less this pane's own header, less the
/// notice row `say` puts in front of it, less the control strip's own rows. A pane has no
/// band to write a notice on and no chrome to hang buttons off, so both come out of the
/// same budget -- and the composition is asked for fewer rows rather than having its last
/// row silently dropped after the fact. The Builder's `publish` spends the same subtraction
/// for the same reason.
inline std::int64_t FilesWeave::body_budget(std::int64_t strip_rows) const {
    return rows() - kHeaderRows - (notice_.empty() ? 0 : 1) - strip_rows;
}

/// The browser's controls, in the order a weaver reads them; `[menu]` is first, as the route to
/// everything, and `pack_controls` never drops the first control that fits. Nothing here asks
/// an operating system anything (WL-FILES-07): availability is read off what this pane holds,
/// so the mark jumps are always offered and refuse in words at the gesture. An unavailable
/// control is still drawn, as `(open)`, and pressing it answers with the operation's own
/// refusal.
inline std::vector<FilesWeave::ControlRow> FilesWeave::browser_controls() const {
    const FileRow* row = ws::row_at(listing_, static_cast<std::size_t>(state_.cursor));
    const bool somewhere = !state_.current_dir.empty();
    const bool usable = row != nullptr && row->openable;
    std::vector<ControlRow> controls;
    controls.push_back(ControlRow{files::kActionMenu, "menu", true});
    controls.push_back(ControlRow{files::kActionOpen,
                                  row != nullptr && row->directory ? "enter" : "open",
                                  usable && somewhere});
    controls.push_back(ControlRow{files::kActionParent, "up a directory",
                                  somewhere && !ws::at_filesystem_root(state_.current_dir)});
    controls.push_back(ControlRow{files::kActionRefresh, "look again", somewhere});
    controls.push_back(ControlRow{files::kActionUseRecipes, "use as recipes",
                                  usable && !row->directory && somewhere});
    controls.push_back(ControlRow{files::kActionPickBuildable, "pick buildable",
                                  listing_.known && somewhere});
    controls.push_back(ControlRow{files::kActionMark,
                                  marks_.marked(state_.current_dir) ? "unmark here"
                                                                    : "mark here",
                                  somewhere});
    controls.push_back(ControlRow{files::kActionPreviousMark, "previous mark", true});
    controls.push_back(ControlRow{files::kActionNextMark, "next mark", true});
    return controls;
}

/// DRAW THE STRIP (`PaneRowsFacet::push_controls`), every face a target naming the operation it
/// asks for. The strip grows with the room (`component::strip_budget`), and what does not fit
/// is counted and reachable through `[menu]`; a room too small for one strip row leaves the
/// right press, which opens the same rows wherever the hand is.
inline void FilesWeave::say_controls(const std::vector<ControlRow>& controls) {
    room().push_controls(controls, fit_row(), [](const ControlRow& control) {
        return FilesMeaning{files_row::kControl, 0, control.id, {}};
    });
}

// ---- The browser's picture ----------------------------------------------------------------

inline void FilesWeave::say_browser() {
    const std::string why = ws::provenance_words(marks_.provenance(state_.current_dir));
    std::string header = "Files";
    const std::size_t total = listing_.rows.size();
    if (!settled_) {
        header += " (waiting)";
    } else if (!listing_.known) {
        header += " --";
    } else if (total == 0) {
        header += " empty";
    } else {
        const std::size_t at = static_cast<std::size_t>(state_.cursor) < total
                                   ? static_cast<std::size_t>(state_.cursor) + 1 : total;
        header += " " + std::to_string(at) + "/" + std::to_string(total);
        if (listing_.bounded) {
            header += "+ (stopped counting)";
        }
    }
    if (!why.empty()) {
        header += "  " + why;
    }
    say_header(header + "  ");
    const std::vector<ControlRow> controls = browser_controls();
    std::int64_t peek_from = -1;
    std::int64_t body_rows = body_budget(room().strip_rows(controls));
    body_rows -= say_places(body_rows, &peek_from);
    if (body_rows > 0) {
        if (!listing_.known) {
            push_row(listing_.refusal.empty() ? "nothing has been listed yet"
                                              : listing_.refusal,
                     zengine::graphics::role::kMuted);
        } else if (total == 0) {
            push_row("this directory is empty", zengine::graphics::role::kMuted);
        } else {
            say_entries(total, static_cast<std::size_t>(body_rows));
        }
    }
    say_controls(controls);
    if (peek_from >= 0) {
        say_peek(peek_from);
    }
}

/// THE HEADER: the pane, its count and why the place is known, then where it is looking -- the
/// path cut in its middle to the room the row leaves it, its root and its end kept (`fit_path`),
/// the run of columns it fills its own part. While the pointer rests on a path the row cut, the
/// whole path stands across the row instead, scrolled to where the pointer's column puts it
/// (`scrub`), and the row entire is the path's.
inline void FilesWeave::say_header(const std::string& head) {
    const std::string lead = ascii_spelling(head);
    const std::string here = ascii_spelling(where());
    const std::int64_t at = static_cast<std::int64_t>(lead.size());
    const std::int64_t room_for = columns() - at;
    const FilesMeaning path{files_row::kPath, 0, {}, here};
    if (room_for < static_cast<std::int64_t>(here.size()) && reveal_ &&
        reveal_->kind == files_row::kPath) {
        push_row(scrub(here, columns(), reveal_->column), zengine::graphics::role::kAccent, path);
        return;
    }
    const std::string shown = room_for > 0 ? fit_path(here, room_for) : std::string();
    const std::int64_t row = room().next_row();
    push_row(lead + shown, zengine::graphics::role::kAccent);
    if (!shown.empty()) {
        (void)room().map().span(
            row, at, static_cast<std::int64_t>(shown.size()),
            component::solid_columns(fitted(lead + shown), static_cast<std::size_t>(columns())),
            path);
    }
}

/// HOW FAR A PLACE'S ROW IS INDENTED: past its section's mark and a space.
inline constexpr std::size_t kPlaceIndent = 4;

/// THE COLUMN THE PLACES' PATHS SHARE: as wide as the widest of them, within what the row leaves
/// beside the indent and the widest reason a place is known.
inline std::size_t FilesWeave::place_path_columns(const std::vector<MarkedPlace>& stops) const {
    std::size_t widest = 0, said = 0;
    for (const MarkedPlace& place : stops) {
        widest = (std::max)(widest, ascii_spelling(place.path).size());
        said = (std::max)(said, ws::provenance_words(place.from).size());
    }
    const std::int64_t room = columns() - static_cast<std::int64_t>(kPlaceIndent) -
                              (said > 0 ? 2 + static_cast<std::int64_t>(said) : 0);
    return room <= 0 ? 0 : (std::min)(widest, static_cast<std::size_t>(room));
}

/// ONE PLACE'S ROW: indented, its path cut in its middle to the column the places share, and why
/// it is known -- `origin`, `marked`, `filesystem root`.
inline std::string FilesWeave::place_text(const MarkedPlace& place,
                                          std::size_t path_columns) const {
    const std::string why = ws::provenance_words(place.from);
    std::string out = std::string(kPlaceIndent, ' ') +
                      ws::pane_text::pad(fit_path(ascii_spelling(place.path),
                                                  static_cast<std::int64_t>(path_columns)),
                                         path_columns);
    if (!why.empty()) {
        out += "  " + why;
    }
    return out;
}

/// THE PLACES, A SECTION OF THE BROWSER (`component::Accordion`): its header always, `+ places (4)`
/// while shut or peeking and `- places` while open, its mark a fold mark (`TextRow::mark`); and,
/// open, a row a place, through a window that says what it left out. The section takes its rows
/// from the listing and leaves it one, and a room with no row for both keeps the listing's. A
/// peeking section's header is in the accent ink, and its places are drawn over what follows once
/// the rest is composed (`say_peek`, from `*peek_from`). Returns the rows the section took.
inline std::int64_t FilesWeave::say_places(std::int64_t body_rows, std::int64_t* peek_from) {
    if (body_rows < 2) {
        return 0;
    }
    const std::vector<MarkedPlace> stops = places();
    const component::Fold fold = places_.fold();
    std::string head = std::string(component::fold_mark(fold)) + " places";
    if (fold != component::Fold::kOpen) {
        head += " (" + std::to_string(stops.size()) + ")";
    }
    const std::int64_t row = room().next_row();
    (void)room().push(ws::TextRow{fitted(head),
                                  fold == component::Fold::kPeek ? zengine::graphics::role::kAccent
                                                                 : zengine::graphics::role::kFill,
                                  zengine::graphics::role::kNone, 0},
                      FilesMeaning{files_row::kPlaces, 0, {}, {}});
    if (fold == component::Fold::kPeek) {
        *peek_from = row + 1;
    }
    if (fold != component::Fold::kOpen) {
        return 1;
    }
    const component::SectionSpan span = component::section_span(
        fold, 0, stops.size(), static_cast<std::size_t>(body_rows - 2));
    const component::ListWindow win = component::scroll_window(stops.size(), 0, span.body);
    const std::size_t path_columns = place_path_columns(stops);
    const std::int64_t asked = room().push_window(win, fit_row(), [&](std::size_t i) {
        const std::string whole = ascii_spelling(stops[i].path);
        const bool revealed = reveal_ && reveal_->kind == files_row::kPlace &&
                              reveal_->index == i && whole.size() > path_columns;
        push_row(revealed ? scrub(whole, columns(), reveal_->column)
                          : place_text(stops[i], path_columns),
                 revealed ? zengine::graphics::role::kAccent : zengine::graphics::role::kFill,
                 FilesMeaning{files_row::kPlace, i, {}, stops[i].path});
    });
    return 1 + asked;
}

/// THE PLACES A PEEK SHOWS, drawn over what follows the section's header -- the listing, the strip
/// and the room's blank rows -- to the room's end, taking none of it: the rows under them mean
/// nothing while they show (`cover`), and a press on one opens the section. What the room below
/// the header cannot hold is counted on its last row.
inline void FilesWeave::say_peek(std::int64_t from) {
    const std::vector<MarkedPlace> stops = places();
    const component::SectionSpan span = component::section_span(
        component::Fold::kPeek, 0, stops.size(),
        static_cast<std::size_t>((std::max)(std::int64_t{0}, rows() - from)));
    if (span.body == 0) {
        return;
    }
    room().cover(from, static_cast<std::int64_t>(span.body));
    const component::ListWindow win = component::scroll_window(stops.size(), 0, span.body);
    const std::size_t path_columns = place_path_columns(stops);
    std::int64_t row = from;
    for (std::size_t i = win.first; i < win.end(); ++i, ++row) {
        room().composing()[static_cast<std::size_t>(row)] =
            ws::TextRow{fitted(place_text(stops[i], path_columns)), zengine::graphics::role::kAccent};
        room().map().row(row, FilesMeaning{files_row::kPeek, i, {}, stops[i].path});
    }
    if (const std::size_t more = component::more_marker(win); more > 0) {
        room().composing()[static_cast<std::size_t>(row)] = ws::TextRow{
            fitted("  " + ws::pane_text::omitted_text(more, "more")), zengine::graphics::role::kMuted};
        room().map().row(row, FilesMeaning{files_row::kPeek, win.end(), {}, {}});
    }
}

/// The listing through a window that moves as little as it can (`component::cursor_window`).
/// Least motion, not centring: a window re-laid on every selection scrolled the rows out from
/// under the pressing hand, and the picture fence would refuse a double-click's second press.
/// The markers are rows of the same budget (`PaneRowsFacet::push_window`), and one entry is
/// always seated.
inline void FilesWeave::say_entries(std::size_t total, std::size_t body_rows) {
    const component::ListWindow win = component::cursor_window(
        total, static_cast<std::size_t>(state_.cursor), window_hint_, body_rows);
    window_hint_ = win.first;
    (void)room().push_window(win, fit_row(), [&](std::size_t i) {
        const bool here = i == static_cast<std::size_t>(state_.cursor);
        const FileRow& row = listing_.rows[i];
        push_row(std::string(here ? "> " : "  ") + row_text(row),
                 here ? zengine::graphics::role::kAccent
                      : (row.openable ? zengine::graphics::role::kFill : zengine::graphics::role::kMuted),
                 FilesMeaning{files_row::kEntry, i, {}, row.name});
    });
}

} // namespace

#endif // ZENGINE_FILES_BROWSER_VIEW_HPP
