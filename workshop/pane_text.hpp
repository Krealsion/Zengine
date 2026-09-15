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

/// A LINE SOMEBODY ELSE WROTE, SPELLED IN WHAT A CANVAS CAN DRAW -- and how many characters
/// had to be spelled (`spelled`, when given, is increased by that many).
///
/// `drawable` repairs a pane's own words a byte at a time; this is for words a pane only
/// CARRIES -- a compiler's diagnostic, an owner's refusal -- which arrive as UTF-8 and must stay
/// readable. So it reads characters, not bytes: the punctuation compilers and build tools print
/// becomes its ASCII twin (the quotes `'` and `"`, the dashes `-`, the ellipsis `...`, a
/// no-break space ` `, the guillemets `<<` `>>`); a tab is a space; every other character a
/// canvas cannot draw is one `?`, a malformed byte included; and a terminal escape sequence
/// (ESC, `[`, its parameters and its final byte) is left out whole. Every one of those but the
/// tab is counted, so a reader can be told that what they see was spelled, and how much of it.
/// The line's printable ASCII passes through untouched, which is every path and line number.
// WL-OUT-03 -- agents/workshop/build-output.md
inline std::string ascii_spelling(const std::string& text, std::size_t* spelled = nullptr) {
    std::string out;
    out.reserve(text.size());
    std::size_t count = 0;
    const std::size_t n = text.size();
    std::size_t i = 0;
    while (i < n) {
        const unsigned char b = static_cast<unsigned char>(text[i]);
        if (b >= 0x20u && b < 0x7Fu) {
            out += static_cast<char>(b);
            ++i;
            continue;
        }
        if (b == '\t') {
            out += ' ';
            ++i;
            continue;
        }
        if (b == 0x1Bu) {
            // AN ESCAPE SEQUENCE IS A TERMINAL'S INSTRUCTION, NOT TEXT: a CSI one is left out to
            // its final byte, and a lone ESC by itself.
            ++count;
            ++i;
            if (i < n && text[i] == '[') {
                ++i;
                while (i < n) {
                    const unsigned char c = static_cast<unsigned char>(text[i]);
                    ++i;
                    if (c >= 0x40u && c <= 0x7Eu) {
                        break;
                    }
                }
            }
            continue;
        }
        std::size_t width = 0;
        std::uint32_t cp = 0;
        if (b >= 0xC2u && b <= 0xDFu) {
            width = 2;
            cp = b & 0x1Fu;
        } else if (b >= 0xE0u && b <= 0xEFu) {
            width = 3;
            cp = b & 0x0Fu;
        } else if (b >= 0xF0u && b <= 0xF4u) {
            width = 4;
            cp = b & 0x07u;
        }
        bool whole = width != 0 && i + width <= n;
        for (std::size_t k = 1; whole && k < width; ++k) {
            const unsigned char c = static_cast<unsigned char>(text[i + k]);
            if ((c & 0xC0u) != 0x80u) {
                whole = false;
            } else {
                cp = (cp << 6) | (c & 0x3Fu);
            }
        }
        ++count;
        if (!whole) {
            out += '?';
            ++i;
            continue;
        }
        i += width;
        switch (cp) {
        case 0x2018: case 0x2019: case 0x201A: case 0x201B: case 0x2032:
            out += '\'';
            break;
        case 0x201C: case 0x201D: case 0x201E: case 0x201F: case 0x2033:
            out += '"';
            break;
        case 0x2010: case 0x2011: case 0x2012: case 0x2013: case 0x2014: case 0x2015:
        case 0x2212:
            out += '-';
            break;
        case 0x2026:
            out += "...";
            break;
        case 0x00A0: case 0x2002: case 0x2003: case 0x2009: case 0x202F:
            out += ' ';
            break;
        case 0x00AB:
            out += "<<";
            break;
        case 0x00BB:
            out += ">>";
            break;
        case 0x2039:
            out += '<';
            break;
        case 0x203A:
            out += '>';
            break;
        default:
            out += '?';
            break;
        }
    }
    if (spelled != nullptr) {
        *spelled += count;
    }
    return out;
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
/// the Powers pane's own image each carried a private static of exactly this body, and the
/// Terminal pane wanted a fourth. The header's own threshold rule -- the count that
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
