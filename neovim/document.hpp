// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_NEOVIM_DOCUMENT_HPP
#define ZENGINE_NEOVIM_DOCUMENT_HPP

// One document, two models, and the exact arithmetic between them. The standard Editor holds
// lines in which a final empty line is the final newline, one line convention, and a caret and
// anchor as (row, byte) with an exclusive end; Neovim holds lines plus `endofline` and a
// `fileformat`, a cursor on a character, and a charwise Visual selection with both ends
// inclusive, whose end may sit past a line's last byte (selecting its newline).
// Workshop law: agents/workshop/neovim.md

// Measured before written (321 positions placed into Neovim 0.11.6 on Windows, 0.11.6 and 0.12.5
// on Linux, read back): 305 exact, and 16 in two named adjustments -- a caret on the final empty
// line goes to the end of the last line, and a one-character selection has no direction, so a
// backward one comes back forward; both reported. A maker can overrule three choices
// (docs/workshop/neovim.md): a caret past a line's end enters INSERT mode, a linewise selection
// is carried charwise, and a blockwise one as its cursor only.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::neovim {

/// A position in the standard model: a line, and a byte of that line (0-based both).
struct Pos {
    std::int64_t row = 0;
    std::int64_t byte = 0;

    friend constexpr bool operator==(const Pos& a, const Pos& b) noexcept {
        return a.row == b.row && a.byte == b.byte;
    }
    friend constexpr bool operator!=(const Pos& a, const Pos& b) noexcept { return !(a == b); }
    friend constexpr bool operator<(const Pos& a, const Pos& b) noexcept {
        return a.row != b.row ? a.row < b.row : a.byte < b.byte;
    }
};

// ---- the bytes --------------------------------------------------------------------------

/// FILE BYTES AS NEOVIM'S LINES: the line break (`dos` when every newline is CRLF), the lines
/// without their breaks, and whether the bytes ended in a break -- which Neovim keeps as
/// `endofline` rather than as a line.
struct NeovimText {
    std::vector<std::string> lines;
    bool dos = false;
    bool final_newline = false;
};

inline NeovimText neovim_text(std::string_view bytes) {
    NeovimText out;
    std::size_t lf = 0;
    std::size_t crlf = 0;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (bytes[i] == '\n') {
            ++lf;
            if (i > 0 && bytes[i - 1] == '\r') {
                ++crlf;
            }
        }
    }
    out.dos = lf > 0 && crlf == lf;
    const std::string_view brk = out.dos ? std::string_view("\r\n") : std::string_view("\n");
    std::size_t start = 0;
    for (;;) {
        const std::size_t at = bytes.find(brk, start);
        if (at == std::string_view::npos) {
            out.lines.emplace_back(bytes.substr(start));
            break;
        }
        out.lines.emplace_back(bytes.substr(start, at - start));
        start = at + brk.size();
    }
    if (out.lines.size() > 1 && out.lines.back().empty() && bytes.size() >= brk.size() &&
        bytes.substr(bytes.size() - brk.size()) == brk) {
        out.lines.pop_back();
        out.final_newline = true;
    }
    return out;
}

/// NEOVIM'S LINES AS THE FILE BYTES `:w` WRITES -- `neovim_text` read backwards.
inline std::string file_bytes(const std::vector<std::string>& lines, bool dos, bool final_newline) {
    const std::string_view brk = dos ? std::string_view("\r\n") : std::string_view("\n");
    std::string out;
    std::size_t total = 0;
    for (const std::string& l : lines) {
        total += l.size() + brk.size();
    }
    out.reserve(total);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) {
            out += brk;
        }
        out += lines[i];
    }
    if (final_newline) {
        out += brk;
    }
    return out;
}

/// DOES `:w` END THE FILE WITH A BREAK? Measured, not assumed: a buffer of one empty line writes
/// one break exactly when Neovim counts a byte in it (`wordcount().bytes`), whatever `eol` says
/// (a freshly read empty file reports `eol` true and writes nothing); any other buffer writes one
/// when `endofline` is set, or `fixendofline` is and `binary` is not.
inline bool writes_final_newline(const std::vector<std::string>& lines, bool eol, bool fixeol,
                                 bool binary, std::int64_t counted_bytes) {
    if (lines.size() == 1 && lines.front().empty()) {
        return counted_bytes > 0;
    }
    return eol || (fixeol && !binary);
}

// ---- the positions ------------------------------------------------------------------------

namespace detail {

inline std::int64_t line_len(const std::vector<std::string>& lines, std::int64_t row) {
    if (row < 0 || static_cast<std::size_t>(row) >= lines.size()) {
        return 0;
    }
    return static_cast<std::int64_t>(lines[static_cast<std::size_t>(row)].size());
}

} // namespace detail

/// WHERE NEOVIM IS PUT: its mode, the Visual start (Visual only) and the cursor, in Neovim's
/// own (0-based row, 0-based byte) terms.
struct Placement {
    enum class Mode : std::uint8_t { Normal, Insert, Visual };
    Mode mode = Mode::Normal;
    Pos start;
    Pos cursor;
    bool adjusted = false;
    std::string note; ///< the adjustment, in words; empty when exact
};

/// A STANDARD CARET AND ANCHOR, PLACED IN NEOVIM. `lines` are Neovim's lines (the document
/// without its final empty line when `final_newline`).
inline Placement place(const std::vector<std::string>& lines, bool final_newline, Pos anchor,
                       Pos caret) {
    Placement out;
    const std::int64_t n = static_cast<std::int64_t>(lines.empty() ? 1 : lines.size());
    const std::int64_t standard_rows = n + (final_newline ? 1 : 0);
    const auto clamp_into_document = [&](Pos p) {
        if (p.row < 0) {
            p.row = 0;
        }
        if (p.row >= standard_rows) {
            p.row = standard_rows - 1;
        }
        const std::int64_t len = p.row < n ? detail::line_len(lines, p.row) : 0;
        if (p.byte < 0) {
            p.byte = 0;
        }
        if (p.byte > len) {
            p.byte = len;
        }
        return p;
    };
    anchor = clamp_into_document(anchor);
    caret = clamp_into_document(caret);
    if (anchor == caret) {
        Pos p = caret;
        if (p.row >= n) {
            p = Pos{n - 1, detail::line_len(lines, n - 1)};
            out.adjusted = true;
            out.note = "the caret after the final newline was placed at the end of the last line";
        }
        const std::int64_t len = detail::line_len(lines, p.row);
        out.cursor = p;
        out.mode = p.byte >= len && len > 0 ? Placement::Mode::Insert : Placement::Mode::Normal;
        return out;
    }
    const Pos begin = anchor < caret ? anchor : caret;
    const Pos end = anchor < caret ? caret : anchor; // exclusive
    // THE LAST SELECTED BYTE, INCLUSIVE: an end at the start of a line selected the previous
    // line's newline, which is that line's past-end position.
    const Pos last = end.byte == 0 ? Pos{end.row - 1, detail::line_len(lines, end.row - 1)}
                                   : Pos{end.row, end.byte - 1};
    const bool forward = caret == end;
    out.mode = Placement::Mode::Visual;
    out.start = forward ? begin : last;
    out.cursor = forward ? last : begin;
    if (!forward && out.start == out.cursor) {
        out.adjusted = true;
        out.note = "a one-character selection has no direction in Vim; its caret is at its end";
    }
    return out;
}

/// THE KEYS THAT PUT NEOVIM WHERE `place` SAID, sent with `nvim_input`. `<Cmd>` moves the cursor
/// without leaving the mode; Visual reaches a start past a line's end by entering at the clamped
/// cell, moving past the end (which Visual allows) and swapping ends with `o`.
inline std::string placement_keys(const Placement& p) {
    const auto at = [](Pos pos) {
        return "<Cmd>call cursor(" + std::to_string(pos.row + 1) + ", " +
               std::to_string(pos.byte + 1) + ")<CR>";
    };
    switch (p.mode) {
    case Placement::Mode::Normal:
        return "<Esc>" + at(p.cursor);
    case Placement::Mode::Insert:
        return "<Esc>" + at(Pos{p.cursor.row, 0}) + "A";
    case Placement::Mode::Visual:
        return "<Esc>" + at(p.start) + "v" + at(p.start) + "o" + at(p.cursor);
    }
    return std::string();
}

/// WHERE NEOVIM IS, AS ITS EXPORT SAID: `nvim_get_mode().mode`, the cursor (0-based row, byte)
/// and the Visual start (0-based row, byte).
struct NeovimPosition {
    std::string mode;
    Pos cursor;
    Pos vstart;
};

/// A NEOVIM POSITION, CARRIED TO THE STANDARD MODEL.
struct Carried {
    Pos anchor;
    Pos caret;
    enum class Kind : std::uint8_t { Caret, Charwise, Linewise, Blockwise };
    Kind kind = Kind::Caret;
    std::string note; ///< what was not carried exactly, in words; empty when exact
};

inline Carried carry(const std::vector<std::string>& lines, bool final_newline,
                     const NeovimPosition& at) {
    Carried out;
    const std::int64_t n = static_cast<std::int64_t>(lines.empty() ? 1 : lines.size());
    const std::int64_t standard_rows = n + (final_newline ? 1 : 0);
    const auto on_line = [&](Pos p) {
        if (p.row < 0) {
            p.row = 0;
        }
        if (p.row >= n) {
            p.row = n - 1;
        }
        const std::int64_t len = detail::line_len(lines, p.row);
        if (p.byte < 0) {
            p.byte = 0;
        }
        if (p.byte > len) {
            p.byte = len;
        }
        return p;
    };
    // THE POSITION AFTER AN INCLUSIVE ONE: past a line's end is its newline, and after the
    // newline is the start of the next standard line (the final empty line included).
    const auto after = [&](Pos p) {
        p = on_line(p);
        if (p.byte >= detail::line_len(lines, p.row)) {
            if (p.row + 1 < standard_rows) {
                return Pos{p.row + 1, 0};
            }
            return p;
        }
        return Pos{p.row, p.byte + 1};
    };
    const char major = at.mode.empty() ? 'n' : at.mode[0];
    const Pos cursor = on_line(at.cursor);
    if (major == 'v' || major == 's') {
        const Pos a = on_line(at.vstart);
        const Pos lo = a < cursor ? a : cursor;
        const Pos hi = a < cursor ? cursor : a;
        const Pos begin = lo;
        const Pos end = after(hi);
        out.kind = Carried::Kind::Charwise;
        if (!(cursor < a)) {
            out.anchor = begin;
            out.caret = end;
        } else {
            out.anchor = end;
            out.caret = begin;
        }
        return out;
    }
    if (major == 'V' || major == 'S') {
        const std::int64_t a_row = on_line(at.vstart).row;
        const std::int64_t lo = a_row < cursor.row ? a_row : cursor.row;
        const std::int64_t hi = a_row < cursor.row ? cursor.row : a_row;
        const Pos begin{lo, 0};
        const Pos end = hi + 1 < standard_rows ? Pos{hi + 1, 0} : Pos{hi, detail::line_len(lines, hi)};
        out.kind = Carried::Kind::Linewise;
        out.note = "the linewise selection was carried as the same lines, charwise";
        if (cursor.row >= a_row) {
            out.anchor = begin;
            out.caret = end;
        } else {
            out.anchor = end;
            out.caret = begin;
        }
        return out;
    }
    out.anchor = cursor;
    out.caret = cursor;
    if (major == '\x16' || major == '\x13') {
        out.kind = Carried::Kind::Blockwise;
        out.note = "the blockwise selection was carried as its cursor only";
        return out;
    }
    // THE PLAIN MODES CARRY EXACTLY. Anything more -- an operator pending (`no`), a completion
    // menu (`ic`), a command line, a terminal -- is a state the standard model has no word for.
    if (at.mode != "n" && at.mode != "i" && at.mode != "R") {
        out.note = "Neovim's mode `" + at.mode + "` was reset; the cursor was carried";
    }
    return out;
}

/// THE VIEWPORT, EACH WAY: Neovim's `topline` is 1-based; a horizontal offset means something only
/// when lines do not wrap.
inline std::int64_t first_row_of(std::int64_t topline) noexcept {
    return topline > 0 ? topline - 1 : 0;
}
inline std::int64_t first_col_of(std::int64_t leftcol, bool wrap) noexcept {
    return wrap || leftcol < 0 ? 0 : leftcol;
}

} // namespace zengine::neovim

#endif // ZENGINE_NEOVIM_DOCUMENT_HPP
