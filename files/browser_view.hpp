// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_FILES_BROWSER_VIEW_HPP
#define ZENGINE_FILES_BROWSER_VIEW_HPP

// The Files pane's browser as a weaver sees it: the members of `FilesWeave` that lay out the
// listing's window and the control strip under it, and record what each row and face means.
// A part of `files/files.cpp`, which includes it after `FilesWeave` and its aliases; nothing
// else includes it.
// Files law: agents/workshop/files.md

#include "files/files.hpp"
#include "files/marks.hpp"
#include "files/vocabulary.hpp"
#include "workshop/path_admission.hpp"

#include "component/list_window.hpp"
#include "graphics/role.hpp"

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
    header += "  " + where();
    push_row(header, zengine::graphics::role::kAccent);
    const std::vector<ControlRow> controls = browser_controls();
    const std::int64_t body_rows = body_budget(room().strip_rows(controls));
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
