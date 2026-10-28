// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TERMINAL_VIEW_HPP
#define ZENGINE_TERMINAL_VIEW_HPP

// The Terminal pane's view of the record: each entry said as a sentence and wrapped into rows of
// the pane's width (WL-TERM-07), the window onto those rows and the gestures that move it, the
// rows above and below it said (WL-TERM-14, WL-TERM-15), and the pane composed from them with the
// caret in its line (WL-TERM-06, WL-TERM-10). The members are `TerminalPaneWeave`'s, declared in
// `pane.cpp`, which includes this file after the class; it compiles in no other place.
// Workshop law: agents/workshop/terminal.md, agents/workshop/terminal-pane.md

#include "terminal/vocabulary.hpp"

#include "workshop/pane_rows.hpp"
#include "workshop/pane_facets.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/pane_vocabulary.hpp"
#include "workshop/terminal_seam_vocabulary.hpp"

#include "component/text_box.hpp"
#include "graphics/role.hpp"
#include "workshop/pane_rows.hpp"

#include <zen/weave.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

using zengine::workshop::pane_text::drawable;
using zengine::workshop::pane_text::fit;
using zengine::workshop::pane_text::wrap;

/// ONE COLUMN OF THE INPUT ROW THE TEXT MAY NOT USE -- the caret's own. A caret at the end
/// of a full row would otherwise sit one past the room, and in a cell projection it stands on
/// the blank cell after the last character.
constexpr std::int64_t kCaretCols = 1;

/// The chrome a pane spends on being this pane, whatever is in it: the header, the legend (where
/// its layout shows it), the omission marker and the input row.
constexpr std::int64_t kChromeRows = 4;

// ---- Rendering one participant's record ---------------------------------------------
//
// EVERY FUNCTION HERE WAS `screen_terminal.cpp`'s, and every one of them is presentation:
// what a sentence says, and how many rows of THIS pane it costs. They read a `ShownEntry`
// instead of a `loom::TranscriptEntry`, which is the same ten facts.

inline std::string entry_address(const ShownEntry& e) {
    if (e.addressing == ws::kAddressWeave) {
        return "#" + std::to_string(e.target);
    }
    if (e.addressing == ws::kAddressRole) {
        return "@" + e.role;
    }
    if (e.addressing == ws::kAddressPublish) {
        return "* (" + std::to_string(e.recipients) + " queued)";
    }
    return "?";
}

inline std::string entry_shape(const ShownEntry& e) {
    return e.shape + " v" + std::to_string(e.version);
}

inline std::string entry_line(const ShownEntry& e) {
    if (e.kind == ws::kEntryCommand) {
        return "> " + e.text;
    }
    if (e.kind == ws::kEntryRefusal) {
        return "!! " + e.text;
    }
    if (e.kind == ws::kEntryNotice) {
        return "-- " + e.text;
    }
    if (e.kind == ws::kEntrySubmitted) {
        return "^ " + entry_shape(e) + " -> " + entry_address(e) + "  SUBMITTED";
    }
    if (e.kind == ws::kEntryReceived) {
        return "v " + entry_shape(e) + " from #" + std::to_string(e.sender);
    }
    if (e.kind == ws::kEntryAnswer) {
        return "v " + entry_shape(e) + " from #" + std::to_string(e.sender) +
               "  [Loom: answers ask " + std::to_string(e.answers) + "]";
    }
    return e.text;
}

/// WHAT `^` MEANS, said once, on a row the pane shows unless its layout keeps the legend off.
inline std::string legend_text() { return "SUBMITTED = authored; a sender is not told its fate"; }

inline std::vector<std::string> entry_wrapped(const ShownEntry& e, std::int64_t width) {
    return wrap(entry_line(e), width);
}

/// THE RECORD AS ROWS OF ONE WIDTH: every entry wrapped, and the row each entry starts on. The
/// unit a view scrolls by and a marker counts in is a ROW, because a wrapped entry cut at the top
/// of a view is part of one entry and not one of a number of messages.
struct WrappedRecord {
    std::vector<std::string> rows;
    std::vector<std::int64_t> starts;
    std::vector<std::size_t> subjects;
    std::int64_t total() const { return static_cast<std::int64_t>(rows.size()); }
};

inline WrappedRecord wrap_record(const std::vector<ShownEntry>& entries, std::int64_t width) {
    WrappedRecord out;
    out.starts.reserve(entries.size());
    for (const ShownEntry& e : entries) {
        out.starts.push_back(out.total());
        for (std::string& one : entry_wrapped(e, width)) {
            out.rows.push_back(std::move(one));
            out.subjects.push_back(out.starts.size() - 1);
        }
    }
    return out;
}

/// WHAT IS ABOVE THE VIEW, said at its top: retained rows scrolled past, and -- a different fact --
/// entries the participant evicted for good, which no scroll reaches.
inline std::string omission_text(std::int64_t above, std::int64_t below, std::int64_t dropped, bool lost) {
    std::string text;
    if (lost) {
        text = "... what you were reading was dropped for good";
    } else if (above > 0) {
        text = "... " + std::to_string(above) + " more rows above";
    } else if (dropped == 0) {
        return below > 0 ? "[the start of this session's record]"
                         : "[the whole of this session's record is on screen]";
    } else {
        text = "[the oldest kept]";
    }
    if (dropped > 0 && !lost) {
        text += "; " + std::to_string(dropped) + " older entries dropped for good";
    }
    return text;
}

/// WHAT IS BELOW THE VIEW, said at its bottom, and the way back to the newest output.
inline std::string below_text(std::int64_t below) {
    return "... " + std::to_string(below) + " more rows below -- press here for the newest";
}

/// WHICH SLICE OF A LIST IS SHOWN, given the selection and the room: the window follows the
/// selection and never scrolls past the end.
inline std::size_t first_shown(std::size_t selected, std::size_t total, std::size_t room) {
    if (room == 0 || total <= room) {
        return 0;
    }
    if (selected < room) {
        return 0;
    }
    const std::size_t last_first = total - room;
    const std::size_t want = selected - room + 1;
    return want < last_first ? want : last_first;
}

// ---- Reading the record: the view's top, and the gestures that move it -------------------

/// THE ROW AT THE TOP OF A VIEW `view` ROWS TALL. Following, the row that puts the newest row
/// at the bottom; anchored, the anchor's row -- clamped into the entry a re-wrap may have
/// shortened, and never so far down that the view runs past the newest row.
inline std::int64_t TerminalPaneWeave::view_top(const WrappedRecord& record, std::int64_t view) const {
    const std::int64_t last = record.total() > view ? record.total() - view : 0;
    if (reading_.follow) {
        return last;
    }
    const std::int64_t index = reading_.seq - known_.dropped;
    if (index < 0 || record.starts.empty()) {
        return 0;
    }
    if (index >= static_cast<std::int64_t>(record.starts.size())) {
        return last;
    }
    const std::size_t i = static_cast<std::size_t>(index);
    const std::int64_t end =
        i + 1 < record.starts.size() ? record.starts[i + 1] : record.total();
    const std::int64_t cost = end - record.starts[i];
    const std::int64_t row = reading_.row < cost ? reading_.row : (cost > 0 ? cost - 1 : 0);
    const std::int64_t top = record.starts[i] + row;
    return top < last ? top : last;
}

/// MOVE THE VIEW BY `by` ROWS (negative is older). Reaching the newest row follows it again;
/// anywhere else the view is anchored to the entry and row now at its top.
inline void TerminalPaneWeave::scroll(std::int64_t by) {
    const WrappedRecord record = wrap_record(known_.entries, columns());
    const std::int64_t view = view_rows_ > 0 ? view_rows_ : 1;
    const std::int64_t last = record.total() > view ? record.total() - view : 0;
    std::int64_t top = view_top(record, view) + by;
    top = top < 0 ? 0 : top;
    reading_.lost = false;
    if (top >= last) {
        reading_ = Reading{};
        return;
    }
    std::size_t i = 0;
    while (i + 1 < record.starts.size() && record.starts[i + 1] <= top) {
        ++i;
    }
    reading_.follow = false;
    reading_.seq = known_.dropped + static_cast<std::int64_t>(i);
    reading_.row = top - (record.starts.empty() ? 0 : record.starts[i]);
}

/// A PAGE OF THE VIEW, keeping one row of the page before it in sight.
inline std::int64_t TerminalPaneWeave::page() const { return view_rows_ > 1 ? view_rows_ - 1 : 1; }

// ---- The rows, and the caret in them --------------------------------------------------

/// The pane, composed: refusal, header, legend, what is above the view, the view, what is
/// below it, the completion list or the history row, the input row, and the caret on it. The budget is spent in priority order: the input row first (a Terminal with no line
/// is not one), then a standing refusal, the header, the marker above the view and the
/// legend; the rest is split between the list (at most half) and the transcript. Every row is
/// budgeted before it is composed, so a late row can never take back the input line.
inline void TerminalPaneWeave::say(loom::Mail& mail) {
    if (!room().granted() || !this->template facet<Settings>().held()) {
        return; // no room, or no row yet: there is nothing this pane could truthfully fill
    }
    room().begin();
    input_row_ = -1;
    legend_row_ = -1;
    list_first_row_ = -1;
    list_row_count_ = 0;
    list_shown_first_ = 0;
    std::vector<zengine::workshop::TextRow> out;
    const auto push = [&out, this](std::string text, std::int64_t role) {
        out.push_back(zengine::workshop::TextRow{drawable(fit(std::move(text), columns())), role});
    };
    if (rows() <= 0 || columns() <= 0) {
        (void)room().map().settle();
        send(std::move(out), {}, mail);
        return;
    }
    // The input row is taken first and a notice second, before anything else is composed: a
    // refusal belongs beside the line it is about, inside the budget. Two rows is the smallest
    // room that holds both; in one row the line keeps it and the refusal is not shown.
    const bool notice = !notice_.empty() && rows() >= 2;
    const std::int64_t body = rows() - 1 - (notice ? 1 : 0);
    const bool header = body >= 1;
    const bool omission = body >= 2;
    const bool legend = legend_shown_ && body >= kChromeRows;
    std::int64_t rest = body - (header ? 1 : 0) - (omission ? 1 : 0) - (legend ? 1 : 0);
    if (rest < 0) {
        rest = 0;
    }

    // A REFUSAL THE DOOR GAVE, over the top row, where a weaver will see it.
    if (notice) {
        push(notice_, zengine::graphics::role::kAlert);
    }

    // THE HEADER NAMES THE IDENTITY whose record this is. A presentation may hold
    // controls for more than one identity, and the moment it stops saying which one it
    // is showing is the moment the two look like one thing with two windows.
    if (header) {
        push(known_.attached
                 ? "TERMINAL -- weave #" + std::to_string(known_.participant)
                 : "TERMINAL -- no participant was mounted on this bus",
             zengine::graphics::role::kAccent);
    }
    if (legend) {
        // WHERE THE SETTING SHOWS, SO WHERE A VALUE IT CANNOT USE IS SAID.
        legend_row_ = static_cast<std::int64_t>(out.size());
        push(legend_problem_.empty() ? legend_text() : legend_problem_,
             legend_problem_.empty() ? zengine::graphics::role::kMuted : zengine::graphics::role::kAlert);
    }

    // THE LIST'S SHARE, DECIDED BEFORE THE TRANSCRIPT'S so the transcript gets what is
    // left rather than the other way round.
    const bool list_open = offer_applies() && offered_.open && !dismissed_;
    // THE ROW ABOVE THE LINE SAYS WHICH COMMAND IS RECALLED when there is no list: browsing
    // history is a different state from composing, and a weaver has to be able to see which.
    const std::string history = list_open ? std::string() : history_heading();
    std::size_t list_wanted = list_open ? offered_.candidates.size() + 1 /*the heading*/
                                        : (history.empty() ? 0 : 1);
    const std::size_t list_ceiling = static_cast<std::size_t>(rest / 2);
    if (list_wanted > list_ceiling) {
        list_wanted = list_ceiling;
    }
    // A LIST OF ONE ROW IS A HEADING WITH NO CANDIDATES UNDER IT, and that is a complete
    // answer rather than an empty box -- `kCompletionMinRows` was one for exactly this
    // reason, measured: with a floor of two, `send * s` showed nothing at all.
    if ((list_open || !history.empty()) && list_wanted == 0 && rest >= 2) {
        list_wanted = 1;
    }
    const std::int64_t transcript_rows = rest - static_cast<std::int64_t>(list_wanted);

    // THE TRANSCRIPT, WRAPPED AND SCROLLED -- one entry becomes as many rows as its sentence
    // needs, and the view is a window onto all of them. Following, the newest row is at the
    // bottom. Scrolled away, a row below the view says how much is below and is the press back
    // to the newest; where there is no row for it, the top marker says it instead.
    const WrappedRecord record = wrap_record(known_.entries, columns());
    std::int64_t view = transcript_rows > 0 ? transcript_rows : 0;
    std::int64_t top = view_top(record, view);
    std::int64_t below = record.total() - (top + view);
    const bool bottom = !reading_.follow && below > 0 && view >= 2;
    if (bottom) {
        view -= 1;
        top = view_top(record, view);
        below = record.total() - (top + view);
    }
    below = below > 0 ? below : 0;
    view_rows_ = view;
    top_marker_row_ = -1;
    bottom_marker_row_ = -1;
    if (omission) {
        top_marker_row_ = static_cast<std::int64_t>(out.size());
        std::string said = omission_text(top, below, known_.dropped, reading_.lost);
        if (below > 0 && !bottom) {
            said += " -- " + std::to_string(below) + " more rows below";
        }
        push(std::move(said), zengine::graphics::role::kMuted);
    }
    for (std::int64_t i = 0; i < view; ++i) {
        const std::int64_t at = top + i;
        if (at < record.total()) {
            const auto& entry = known_.entries[record.subjects[static_cast<std::size_t>(at)]];
            if (entry.observation > 0 && (entry.kind == ws::kEntrySubmitted ||
                entry.kind == ws::kEntryReceived || entry.kind == ws::kEntryAnswer)) {
                room().map().row(static_cast<std::int64_t>(out.size()),
                                 {known_.participant, entry.observation});
            }
        }
        push(at < record.total() ? record.rows[static_cast<std::size_t>(at)] : std::string(),
             zengine::graphics::role::kFill);
    }
    if (bottom) {
        bottom_marker_row_ = static_cast<std::int64_t>(out.size());
        push(below_text(below), zengine::graphics::role::kMuted);
    }
    if (list_wanted > 0 && list_open) {
        list_first_row_ = static_cast<std::int64_t>(out.size());
        list_row_count_ = static_cast<std::int64_t>(list_wanted);
        say_list(list_wanted, push);
    } else if (list_wanted > 0) {
        push(history, zengine::graphics::role::kMuted);
    }

    // THE LINE BEING TYPED, AND -- while there is nothing on it -- the gesture that
    // answers "what can I say here". It is on this row rather than in the legend because
    // it is about what to do NEXT rather than about what a word means, and because it
    // erases itself: the moment a weaver types anything the line has their text on it.
    const std::int64_t visible = columns() - kPromptCols - kCaretCols;
    line_.keep_caret_visible(visible > 0 ? visible : 0);
    input_row_ = static_cast<std::int64_t>(out.size());
    const bool prompting = line_.empty() && !list_open;
    push(prompting ? prompt_ + "    Tab: what can this terminal say?  Up: recall a command"
                   : prompt_ + " " + line_.visible(visible > 0 ? visible : 0),
         known_.attached ? zengine::graphics::role::kAccent : zengine::graphics::role::kAlert);

    if (static_cast<std::int64_t>(out.size()) > rows()) {
        out.resize(static_cast<std::size_t>(rows()));
    }
    if (input_row_ >= static_cast<std::int64_t>(out.size())) {
        input_row_ = -1; // cut away: there is no row to put a caret on
    }
    (void)room().map().settle();
    std::vector<ws::RowPart> parts = named_rows(static_cast<std::int64_t>(out.size()));
    send(std::move(out), std::move(parts), mail);
}

/// THE ROWS DRAWN in the pane's area, its caret and selection standing in them.
inline void TerminalPaneWeave::send(std::vector<zengine::workshop::TextRow> out,
                                    std::vector<ws::RowPart> parts, loom::Mail& mail) {
    room().caret() = caret();
    room().send(mail, std::move(out), std::move(parts));
}

template <typename Push> inline void TerminalPaneWeave::say_list(std::size_t room, const Push& push) {
    const std::size_t body = room - 1; // the heading always costs one
    const std::size_t first = first_shown(selected_, offered_.candidates.size(), body);
    list_shown_first_ = first;
    const std::size_t last = offered_.candidates.size() < first + body
                                 ? offered_.candidates.size()
                                 : first + body;
    std::string heading = offered_.heading;
    if (offered_.candidates.size() > body) {
        // WHICH SLICE, SAID OUT LOUD -- including the slice that is nothing at all, which
        // is what a pane too short for a single candidate row shows. "none of 5" is a
        // worse picture than five rows and a far better sentence than five rows' worth of
        // silence.
        heading = (body == 0 ? std::string("none")
                             : std::to_string(first + 1) + "-" + std::to_string(last)) +
                  " of " + std::to_string(offered_.candidates.size()) + "  " + heading;
    }
    // WITH THE CURSOR THE WEAVER MOVED, ENTER TAKES WHAT IT IS ON: said where the list is read,
    // after what the list is, so a narrow pane gives up this before the list's own words.
    if (cursor_moved_ && !offered_.candidates.empty()) {
        heading += " -- Enter: take it, Shift+Enter: run the line";
    }
    push(std::move(heading), zengine::graphics::role::kMuted);
    for (std::size_t i = first; i < last; ++i) {
        const ShownCandidate& c = offered_.candidates[i];
        const bool chosen = i == selected_;
        std::string text = (chosen ? "> " : "  ") + c.display;
        if (!c.detail.empty()) {
            // The detail is what a candidate MEANS, and it is the first thing a narrow
            // pane gives up: `fit` cuts the whole row, so a list in a small window shows
            // names and a list in a large one shows names and meanings.
            text += "   " + c.detail;
        }
        push(std::move(text), chosen ? zengine::graphics::role::kAccent : zengine::graphics::role::kFill);
    }
}

/// WHERE THE CARET IS, AND WHAT IS SELECTED -- in the rows' own lattice, never written
/// into them; none when the input row was cut away.
///
/// ONE MEASURER: the column comes from the same window the row was written against and
/// the same one a press is answered with, so a caret cannot land where the text is not
/// and a click cannot land where the caret would not.
inline ws::RowsCaret TerminalPaneWeave::caret() const {
    ws::RowsCaret caret;
    if (input_row_ < 0) {
        return caret;
    }
    const std::int64_t visible = columns() - kPromptCols - kCaretCols;
    caret.row = input_row_;
    caret.column = kPromptCols + static_cast<std::int64_t>(line_.caret_column());
    // AND THE SELECTION, THE SAME WAY: the visible part of the component's own range,
    // prompt-shifted by the same arithmetic the caret goes through. A selection scrolled
    // wholly off the slice says nothing, which is the truthful picture of a row that
    // shows none of it.
    const component::TextBox::VisibleSpan span =
        line_.visible_selection(visible > 0 ? visible : 0);
    if (span.present()) {
        caret.sel_begin_row = input_row_;
        caret.sel_begin_col = kPromptCols + span.begin;
        caret.sel_end_row = input_row_;
        caret.sel_end_col = kPromptCols + span.end;
    }
    return caret;
}

} // namespace

#endif // ZENGINE_TERMINAL_VIEW_HPP
