// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_TEXT_HPP
#define ZENGINE_WORKSHOP_PANE_TEXT_HPP

// THE TEXT A PANE SHAPES BEFORE IT PUBLISHES -- fitted to the columns it was granted, wrapped
// inside them, padded to them, and stripped of the bytes a canvas cannot draw.
//
// WHY THIS EXISTS, AND WHY NOW. Five packages compose rows for the pane protocol
// (`files/`, `builder-pane/`, `attention-pane/`, `info-pane/`, and the Terminal behind them),
// and each carried its own copy of these four or five functions with a comment saying it was
// somebody else's, kept here so two panes cut a row the same way. Four copies is a convention;
// the fifth is a defect waiting for the day one of them is repaired. The count is the whole of
// the argument: nothing about the functions changed, and no pane gained a base class, a
// framework or a second measurer for having them in one file.
//
// WHY IT LIVES BESIDE `pane_vocabulary.hpp` AND NOT IN `surface/`. What these bound a row to is
// the ROOM Workshop granted (`PaneRoom`), and the reason a row must fit it is `judge_content`:
// a publication is judged whole, and one row a byte too wide refuses all of it. That is a fact
// about the pane protocol rather than about a canvas, so it belongs with the protocol -- in the
// same INTERFACE target (`zengine-workshop-vocabulary`) every pane package already links to
// reach `PaneContent`.
//
// ⚠ AND THE HOST'S OWN `detail::fit` IS NOT THIS. Workshop cuts its own rows with its own
// helper, inside its own translation units; the two agree on the mark and on the arithmetic
// and are deliberately not one function, because a host that shared an implementation with the
// panes it judges would be judging its own output. If they ever disagree, the pane's rows are
// the ones that must change: `judge_content` is the wall.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::workshop::pane_text {

/// THE MARK A CUT LEAVES, and the one every pane in this tree already used.
inline constexpr const char* kElided = "...";

/// How far a wrapped continuation row is indented, when the room can hold an indent at all.
inline constexpr std::int64_t kWrapIndent = 2;

/// ONE ROW, FITTED TO A COLUMN BUDGET, WITH THE CUT MARKED. A width at or below zero is no
/// room at all and answers with nothing; a width at or below the mark's own length answers
/// with as much of the mark as fits, because a row that says only "..." still says that
/// something was cut.
inline std::string fit(std::string text, std::int64_t width) {
    if (width <= 0) {
        return {};
    }
    const std::size_t room = static_cast<std::size_t>(width);
    if (text.size() <= room) {
        return text; // it fits, so nothing about it changes -- not even its role
    }
    const std::size_t mark = std::string_view(kElided).size();
    if (room <= mark) {
        return std::string(kElided).substr(0, room);
    }
    text.resize(room - mark);
    text += kElided;
    return text;
}

/// ONE ROW, PADDED TO A WIDTH -- and truncated to it, unmarked, because a caller that pads is
/// composing a column it has already measured rather than cutting a maker's prose.
inline std::string pad(std::string text, std::size_t width) {
    if (text.size() > width) {
        text.resize(width);
        return text;
    }
    text.append(width - text.size(), ' ');
    return text;
}

/// A SENTENCE ACROSS AS MANY ROWS AS IT NEEDS, broken at a space where there is one and at the
/// budget where there is not, with continuations indented.
///
/// ⚠ ONE SPACE IS SPENT BY THE BREAK, NOT A RUN OF THEM. Two of the copies this replaces did
/// exactly this and one skipped every space it found, which silently deleted bytes a maker had
/// typed: `wrap` is spent on a build's status line and on a condition's detail, and both of
/// those carry a maker's own words. The single-space rule is the one that keeps them.
inline std::vector<std::string> wrap(const std::string& text, std::int64_t width) {
    std::vector<std::string> rows;
    if (width <= 0) {
        return rows;
    }
    const std::size_t room = static_cast<std::size_t>(width);
    const std::size_t indent =
        width > kWrapIndent + 1 ? static_cast<std::size_t>(kWrapIndent) : 0;
    std::size_t at = 0;
    while (true) {
        const std::string lead(rows.empty() ? 0 : indent, ' ');
        const std::size_t take = room - lead.size();
        if (text.size() - at <= take) {
            rows.push_back(lead + text.substr(at));
            return rows;
        }
        std::size_t cut = at + take;
        bool broke = false;
        for (std::size_t i = cut; i > at; --i) {
            if (text[i] == ' ') {
                cut = i;
                broke = true;
                break;
            }
        }
        rows.push_back(lead + text.substr(at, cut - at));
        at = cut;
        if (broke) {
            ++at; // the space that broke the line is spent by the break
        }
        if (at >= text.size()) {
            return rows;
        }
    }
}

/// THE ROW A LIST SPENDS ON WHAT IT COULD NOT SHOW. Never a bare count: `which` names what was
/// left out, so a maker reads a sentence rather than a number.
inline std::string omitted_text(std::size_t how_many, const char* which) {
    return "... " + std::to_string(how_many) + " " + which;
}

/// EVERY BYTE A CANVAS CAN DRAW, AT THE PANE'S OWN DOOR. A maker's own text -- an object's
/// name, a property's value, a path -- has never been required to be printable ASCII, and a
/// publication is judged WHOLE: one undrawable byte refuses every row the pane sent. Replacing
/// the byte with a space costs the maker a character they can see is missing; sending it costs
/// them the pane.
inline std::string drawable(std::string text) {
    for (char& c : text) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte < 0x20u || byte >= 0x7Fu) {
            c = ' ';
        }
    }
    return text;
}

/// ...AND THE SAME QUESTION ASKED THE OTHER WAY, AT THE OTHER DOOR: is this text, WHOLE,
/// something a canvas can draw?
///
/// `drawable` is for what a pane is about to SAY -- it repairs, because the alternative is
/// losing the pane. This is for what a maker just TYPED or PASTED, and it refuses, because
/// the alternative is silently changing bytes they chose. Two doors, two postures, one byte
/// rule; the browser established the refusing one and Info and the Terminal both spell it.
///
/// ⚠ IT IS HERE BECAUSE IT WAS ABOUT TO BE COPIED A THIRD TIME. `info-pane/pane.cpp` and
/// `introspection/introspection.cpp` each carried a private static of exactly this body, and
/// the Terminal pane wanted a fourth. The header's own threshold rule -- the count that
/// argued for it -- is met by the same arithmetic that created it.
inline bool admissible(const std::string& text) {
    for (const char c : text) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte < 0x20u || byte >= 0x7Fu) {
            return false;
        }
    }
    return true;
}

} // namespace zengine::workshop::pane_text

#endif // ZENGINE_WORKSHOP_PANE_TEXT_HPP
