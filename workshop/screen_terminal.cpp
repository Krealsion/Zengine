// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The bodies of `screen.hpp`'s sections -- rendering one participant's record, the editable line,
// and the completion list -- compiled once into `zengine-workshop-logic` and linked by the host
// and every suite; the declarations, the constants and the constexpr functions stay in the
// header.
// Workshop law: agents/workshop/terminal.md (+2 registers; agents/workshop.md routes)

#include "screen.hpp"

namespace zengine::workshop {

// ---- Rendering one participant's record ------------------------------------------------

// WL-TERM-02 -- agents/workshop/terminal.md
std::string terminal_address(const loom::TranscriptEntry& e) {
    switch (e.addressing) {
    case loom::Addressing::Weave: return "#" + std::to_string(e.target.value);
    case loom::Addressing::Role: return "@" + e.role;
    case loom::Addressing::Publish: return "* (" + std::to_string(e.recipients) + " queued)";
    }
    return "?";
}

std::string terminal_shape(const loom::TranscriptEntry& e) {
    return e.shape + " v" + std::to_string(e.version);
}

// WL-TERM-07 -- agents/workshop/terminal.md
std::string terminal_line(const loom::TranscriptEntry& e) {
    switch (e.kind) {
    case loom::TranscriptKind::LocalCommand: return "> " + e.text;
    case loom::TranscriptKind::LocalRefusal: return "!! " + e.text;
    case loom::TranscriptKind::LocalNotice: return "-- " + e.text;
    case loom::TranscriptKind::Submitted:
        return "^ " + terminal_shape(e) + " -> " + terminal_address(e) + "  SUBMITTED";
    case loom::TranscriptKind::Received:
        return "v " + terminal_shape(e) + " from #" + std::to_string(e.sender.value);
    case loom::TranscriptKind::AnswerReceived:
        return "v " + terminal_shape(e) + " from #" + std::to_string(e.sender.value) +
               "  [Loom: answers ask " + std::to_string(e.answers) + "]";
    }
    return e.text;
}

// WL-TERM-07 -- agents/workshop/terminal.md
std::string terminal_legend() {
    return "SUBMITTED = authored; a sender is not told its fate";
}

// WL-TERM-07 -- agents/workshop/terminal.md
std::vector<std::string> terminal_wrapped(const loom::TranscriptEntry& e,
                                          std::int64_t width) {
    return detail::wrap(terminal_line(e), width);
}

// WL-TERM-03 -- agents/workshop/terminal.md
std::size_t entries_that_fit(const std::vector<loom::TranscriptEntry>& entries,
                             std::int64_t width, std::size_t rows) {
    std::size_t taken = 0;
    std::size_t used = 0;
    for (std::size_t i = entries.size(); i > 0; --i) {
        const std::size_t cost = terminal_wrapped(entries[i - 1], width).size();
        if (taken > 0 && used + cost > rows) {
            break;
        }
        used += cost;
        ++taken;
        if (used >= rows) {
            break;
        }
    }
    return taken;
}

// WL-TERM-03 -- agents/workshop/terminal.md
std::string terminal_omission(const TerminalPane& t) {
    if (t.earlier == 0 && t.dropped == 0) {
        return "[the whole of this session's record is on screen]";
    }
    std::string text = "... " + std::to_string(t.earlier) + " earlier";
    if (t.dropped > 0) {
        text += ", " + std::to_string(t.dropped) + " dropped for good";
    }
    return text;
}

// ---- The editable line, resolved ONCE ---------------------------------------------------

// WL-TEXT-13 -- agents/workshop/text-box.md
std::int64_t terminal_caret_column(const TerminalInputPlace& p,
                                   const component::TextBox& box) noexcept {
    return surface::add_cells(p.first_column, static_cast<std::int64_t>(box.caret_column()));
}

// WL-TEXT-13 -- agents/workshop/text-box.md
std::size_t terminal_caret_of_column(const TerminalInputPlace& p,
                                     const component::TextBox& box,
                                     std::int64_t column) noexcept {
    return box.position_at_column(surface::sub_px(column, p.first_column));
}

TerminalSelectionSpan terminal_selection_columns(const TerminalInputPlace& p,
                                                 const component::TextBox& box) noexcept {
    const component::TextBox::VisibleSpan vis = box.visible_selection(p.columns);
    if (!vis.present()) {
        return TerminalSelectionSpan{};
    }
    return TerminalSelectionSpan{surface::add_cells(p.first_column, vis.begin),
                                 surface::add_cells(p.first_column, vis.end), true};
}

// ---- The completion list, inside the pane it belongs to ---------------------------------

// WL-TERM-05, WL-TERM-06 -- agents/workshop/terminal.md
std::vector<surface::SurfaceTextRow> completion_rows(const Completion& comp,
                                                     std::size_t capacity,
                                                     std::int64_t width) {
    std::vector<surface::SurfaceTextRow> rows;
    if (capacity == 0) {
        return rows;
    }
    const std::size_t room = capacity - 1; // the heading always costs one
    const std::size_t first = completion_first_shown(comp.selected, capacity);
    const std::size_t last = comp.candidates.size() < first + room ? comp.candidates.size()
                                                                   : first + room;
    std::string heading = comp.heading;
    if (comp.candidates.size() > room) {
        // WHICH SLICE, SAID OUT LOUD -- including the slice that is nothing at all, which is
        // what a pane too short for a single candidate row shows. "none of 5" is a worse
        // picture than five rows and a far better sentence than five rows' worth of silence.
        heading = (room == 0 ? std::string("none")
                             : std::to_string(first + 1) + "-" + std::to_string(last)) +
                  " of " + std::to_string(comp.candidates.size()) + "  " + heading;
    }
    rows.push_back(surface::SurfaceTextRow{detail::fit(heading, width), surface::role::kMuted,
                                           surface::role::kNone});
    for (std::size_t i = first; i < last; ++i) {
        const Candidate& c = comp.candidates[i];
        const bool chosen = i == comp.selected;
        std::string text = (chosen ? "> " : "  ") + c.display;
        if (!c.detail.empty()) {
            // The detail is what a candidate MEANS, and it is the first thing a narrow pane
            // gives up: `detail::fit` cuts the whole row, so a list in a small window shows
            // names and a list in a large one shows names and meanings.
            text += "   " + c.detail;
        }
        rows.push_back(surface::SurfaceTextRow{
            detail::fit(text, width), chosen ? surface::role::kAccent : surface::role::kFill,
            chosen ? surface::role::kMuted : surface::role::kNone});
    }
    return rows;
}

// WL-TERM-03, WL-TERM-07 -- agents/workshop/terminal.md; WL-GEO-01 -- agents/workshop/geometry.md
void paint_terminal(surface::SurfaceLayer& layer, const TerminalPane& t,
                    const Screen& sc, const Keymap& keymap) {
    if (!t.open) {
        return;
    }
    layer.rects.push_back(surface::SurfaceRect{sc.terminal_x, sc.terminal_y, sc.terminal_w,
                                           sc.terminal_h, surface::role::kMuted});

    surface::SurfaceTextRegion pane;
    pane.x = sc.terminal_x;
    pane.y = sc.terminal_y;
    pane.w = sc.terminal_w;
    pane.h = sc.terminal_h;
    pane.rows.resize(sc.terminal_lines);
    const auto row = [&pane, &sc](std::size_t line, const std::string& text, std::int64_t role) {
        if (line >= pane.rows.size()) {
            return; // the pane is smaller than its own chrome: the floor already refused this
        }
        pane.rows[line] = surface::SurfaceTextRow{detail::fit(text, sc.terminal_cols), role};
    };

    // The header NAMES THE IDENTITY whose record this is. A presentation may hold controls for
    // more than one identity, and the moment it stops saying which one it is showing is the
    // moment the two look like one thing with two windows.
    row(0,
        t.attached ? "TERMINAL -- weave #" + std::to_string(t.id.value) + "  (" +
                         hotkey_text(keymap, Act::kTerminalToggle) + " closes)"
                   : "TERMINAL -- no participant was mounted on this bus",
        surface::role::kAccent);

    row(1, terminal_legend(), surface::role::kMuted);

    // THE TRANSCRIPT, WRAPPED -- one entry becomes as many rows as its sentence needs, and the
    // pane is a list of ROWS from here down rather than a list of entries. `refresh_terminal`
    // chose `shown` with the same arithmetic (`entries_that_fit`) against the same
    // `terminal_cols`, so this loop is where that choice is CARRIED OUT rather than where it is
    // made; the truncation below can only fire for a single entry taller than the whole pane,
    // which is the case that function names.
    std::vector<std::string> lines;
    for (const loom::TranscriptEntry& e : t.shown) {
        for (std::string& line : terminal_wrapped(e, sc.terminal_cols)) {
            lines.push_back(std::move(line));
        }
    }
    if (lines.size() > sc.terminal_rows) {
        lines.resize(sc.terminal_rows);
    }

    for (std::size_t i = 0; i < sc.terminal_rows; ++i) {
        row(2 + i, i < lines.size() ? lines[i] : std::string(), surface::role::kFill);
    }
    row(sc.terminal_lines - 2, terminal_omission(t), surface::role::kMuted);
    // THE LINE BEING TYPED, AND THE CARET SAID SEPARATELY FROM IT.
    //
    // Earlier the caret was a `_` this function appended, which was truthful only because
    // the caret could only ever be at the end. It can be anywhere now, so the position is
    // published as a fact ABOUT the region (`caret_row`/`caret_col`) and each medium answers
    // it in its own type: a window fills a bar between two characters, and the cell
    // projection inserts `_` at the same column -- which, for a caret at the end of the line,
    // is byte-for-byte the row this function used to write itself.
    //
    // AND WHILE THERE IS NOTHING ON IT, IT NAMES THE GESTURE THAT ANSWERS "what can I
    // say here". It is on this row rather than in the legend because it is
    // about what to do NEXT rather than about what a word means, and because it
    // erases itself: the moment a maker types anything the line has their text on it
    // and the list is doing the same job better. A tool whose discovery gesture is
    // itself undiscoverable has moved the problem rather than solved it.
    //
    // AND IT IS A WINDOW ONTO THE LINE RATHER THAN THE WHOLE OF IT. `visible` is the
    // slice the row has room for; the authored command is untouched behind it, and nothing
    // in the row says how much is off either side -- there is no marker, no arrow and no
    // ellipsis, because the caret staying put is what tells a maker the line moved and an
    // indicator would be a second thing to keep true. Note that `detail::fit`'s `...` can no
    // longer fire on this row: the slice is at most `columns` and the prompt is exactly the
    // difference, so the row is always short enough. That marker's job here has been taken
    // over by a window a maker can move.
    const TerminalInputPlace typing = terminal_input_place(sc);
    const bool prompting = t.input.empty() && !t.completion.open;
    row(sc.terminal_lines - 1,
        prompting ? ">    " + hotkey_text(keymap, Act::kTerminalComplete) +
                        ": what can this terminal say?"
                  : "> " + t.input.visible(typing.columns),
        t.attached ? surface::role::kAccent : surface::role::kAlert);
    // ONE MEASURER: the column comes from the same resolution the row was written against,
    // and the same one a press is answered with, so a caret cannot land where the text is
    // not and a click cannot land where the caret would not. that resolution
    // includes WHICH PART of the line is on the row, and all three read the one answer
    // `TerminalInput` holds rather than each deciding for itself.
    pane.caret_row = typing.prose_row;
    pane.caret_col = terminal_caret_column(typing, t.input);
    // AND THE SELECTION, THE SAME WAY: the visible part of the component's own
    // range, prompt-shifted by the same helper family the caret goes through, published as
    // the region's selection so each medium answers in its own voice — reverse video in a
    // cell, a band under the glyphs in a window. A selection scrolled wholly off the slice
    // publishes nothing, which is the truthful picture of a row that shows none of it.
    const TerminalSelectionSpan marked = terminal_selection_columns(typing, t.input);
    if (marked.present) {
        pane.sel_begin_row = typing.prose_row;
        pane.sel_begin_col = marked.begin;
        pane.sel_end_row = typing.prose_row;
        pane.sel_end_col = marked.end;
    }

    layer.texts.push_back(std::move(pane));

    // THE COMPLETION LIST, LAST, SO IT IS ON TOP OF THE PANE IT BELONGS TO. Painter's order
    // across `texts` is list order, the same rule every other list on a canvas already
    // states, so "the list covers the transcript" needs no z-order and no framework -- it
    // needs the push to come second.
    //
    // AND ONE MEASURER, AGAIN. `completion_place` decides how many rows there are and
    // `completion_rows` fills exactly that many; nothing upstream was told a number it could
    // disagree with, which is why the list can say "3-5 of 9" and be right.
    if (!t.completion.open || t.dismissed) {
        return;
    }
    const CompletionPlace place =
        completion_place(sc, t.completion.candidates.size() + 1 /*the heading*/);
    if (!place.visible) {
        return; // a pane too small to hold a heading and a candidate shows neither
    }
    surface::SurfaceTextRegion list;
    list.x = place.x;
    list.y = place.y;
    list.w = place.w;
    list.h = place.h;
    list.rows = completion_rows(t.completion, place.rows, sc.terminal_cols);
    layer.texts.push_back(std::move(list));
}

} // namespace zengine::workshop
