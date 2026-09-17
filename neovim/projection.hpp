// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_NEOVIM_PROJECTION_HPP
#define ZENGINE_NEOVIM_PROJECTION_HPP

// NEOVIM'S SCREEN, SAID IN THE PANE PROTOCOL'S WORDS.
//
// A Workshop pane says rows of printable ASCII, one role per row, one caret and ONE selection
// range in reading order (`workshop/pane_vocabulary.hpp`). Neovim's screen is richer than that,
// and this file is where the difference is decided -- deliberately, in one place, so a maker
// reading `docs/workshop/neovim.md` and a case reading this file see the same rules:
//
//   TEXT      every cell becomes one byte. Printable ASCII is itself; box drawing becomes `-`, `|`
//             or `+`; a few common symbols get their nearest ASCII; anything else is `?`, and so
//             is the right half of a double-width character. THE DOCUMENT IS UNTOUCHED -- this is
//             only what the pane can draw.
//   ROWS      a row carrying an error or a warning is an alert; a row that is mostly status line,
//             tab line or window bar is chrome; the `~` rows past a buffer's end are muted;
//             everything else is ordinary text.
//   CURSOR    a bar or underline cursor (Insert, Replace, the command line's insert) is the pane's
//             caret; a block cursor is not a caret -- it is the cell it covers.
//   SELECTION ONE range, by priority:
//               1. VISUAL: the Visual cells from first to last in reading order, EXTENDED THROUGH
//                  THE CURSOR CELL (Neovim does not mark it -- measured); blockwise Visual shows
//                  only the cursor row's segment, because a rectangle is not a reading-order range
//               2. the completion menu's selected item (`PmenuSel`)
//               3. the block cursor's own cell, so Normal mode shows where it is
//
// Semantic spans -- syntax, search matches, a real rectangle -- are the next seam: the pane
// protocol has one range, and this file does not pretend it has more.

#include "neovim/grid.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::neovim {

enum class RowKind : std::uint8_t {
    Text,   ///< ordinary text
    Chrome, ///< status line, tab line, window bar
    Muted,  ///< the `~` filler past a buffer's end
    Alert,  ///< a row carrying an error or a warning
};

enum class SelectionSource : std::uint8_t { None, Visual, Popup, Block };

/// ONE CELL AS ONE BYTE the pane can draw. `grapheme` is exactly what Neovim put in the cell.
inline char ascii_of(std::string_view grapheme) noexcept {
    if (grapheme.size() == 1) {
        const unsigned char b = static_cast<unsigned char>(grapheme[0]);
        return b >= 0x20u && b < 0x7Fu ? grapheme[0] : '?';
    }
    if (grapheme.empty()) {
        return '?'; // the right half of a double-width character
    }
    // Decode the first code point; a malformed sequence is `?`.
    const unsigned char b0 = static_cast<unsigned char>(grapheme[0]);
    std::uint32_t cp = 0;
    std::size_t need = 0;
    if ((b0 & 0xE0u) == 0xC0u) {
        cp = b0 & 0x1Fu;
        need = 1;
    } else if ((b0 & 0xF0u) == 0xE0u) {
        cp = b0 & 0x0Fu;
        need = 2;
    } else if ((b0 & 0xF8u) == 0xF0u) {
        cp = b0 & 0x07u;
        need = 3;
    } else {
        return '?';
    }
    if (grapheme.size() < need + 1) {
        return '?';
    }
    for (std::size_t i = 1; i <= need; ++i) {
        const unsigned char b = static_cast<unsigned char>(grapheme[i]);
        if ((b & 0xC0u) != 0x80u) {
            return '?';
        }
        cp = (cp << 6) | (b & 0x3Fu);
    }
    if (cp >= 0x2500u && cp <= 0x257Fu) {
        // BOX DRAWING: the horizontal and vertical strokes, and a corner or junction for the rest.
        switch (cp) {
        case 0x2500: case 0x2501: case 0x2504: case 0x2505: case 0x2508: case 0x2509:
        case 0x254C: case 0x254D: case 0x2550: case 0x2574: case 0x2576: case 0x2578:
        case 0x257A: case 0x257C: case 0x257E:
            return '-';
        case 0x2502: case 0x2503: case 0x2506: case 0x2507: case 0x250A: case 0x250B:
        case 0x254E: case 0x254F: case 0x2551: case 0x2575: case 0x2577: case 0x2579:
        case 0x257B: case 0x257D: case 0x257F:
            return '|';
        default:
            return '+';
        }
    }
    switch (cp) {
    case 0x00A0: return ' ';  // no-break space
    case 0x00B7: case 0x2022: case 0x2026: case 0x22C5: return '.';
    case 0x00BB: case 0x203A: case 0x2192: case 0x25B8: case 0x25B6: case 0x276F: return '>';
    case 0x00AB: case 0x2039: case 0x2190: case 0x25C2: case 0x25C0: case 0x276E: return '<';
    case 0x2191: case 0x25B4: case 0x25B2: return '^';
    case 0x2193: case 0x25BE: case 0x25BC: return 'v';
    case 0x00AC: case 0x2014: case 0x2013: case 0x2012: return '-';
    case 0x21B2: case 0x23CE: case 0x21B5: return '$';
    case 0x2588: case 0x2589: case 0x258A: case 0x258B: case 0x258C: case 0x2590: return '#';
    default: return '?';
    }
}

/// WHAT THE PANE SHOWS: the rows, and the caret and the one range beside them, in the room's
/// lattice (row 0 is the first row of the grid; the grid is the whole room).
struct Screen {
    struct Row {
        std::string text;
        RowKind kind = RowKind::Text;
    };
    std::vector<Row> rows;
    std::int64_t caret_row = -1; ///< -1: no caret (a block cursor is shown as its cell instead)
    std::int64_t caret_col = 0;
    std::int64_t sel_begin_row = -1; ///< -1: no range
    std::int64_t sel_begin_col = 0;
    std::int64_t sel_end_row = -1;
    std::int64_t sel_end_col = 0; ///< exclusive
    SelectionSource source = SelectionSource::None;
};

/// THE VISUAL KIND Neovim reports (`mode()`'s first character): `v`/`V`/Ctrl-V, and Select
/// mode's `s`/`S`/Ctrl-S, which Neovim draws exactly as Visual. Anything else is 0.
inline bool blockwise_kind(char kind) noexcept {
    return kind == '\x16' || kind == '\x13';
}

namespace detail {

struct Cursor {
    std::int64_t row = 0;
    std::int64_t col = 0;
};

inline bool before(std::int64_t r1, std::int64_t c1, std::int64_t r2, std::int64_t c2) noexcept {
    return r1 != r2 ? r1 < r2 : c1 < c2;
}

} // namespace detail

/// PROJECT THE GRID. `visual_kind` is what the owner last heard Neovim's mode was (the grid's
/// `mode_change` names a mode family and cannot tell charwise from blockwise).
inline Screen project(const Grid& grid, char visual_kind = 0) {
    Screen out;
    const std::int64_t rows = grid.rows();
    const std::int64_t cols = grid.columns();
    out.rows.resize(static_cast<std::size_t>(rows));
    std::int64_t vis_first_r = -1;
    std::int64_t vis_first_c = 0;
    std::int64_t vis_last_r = -1;
    std::int64_t vis_last_c = 0;
    std::int64_t pop_first_r = -1;
    std::int64_t pop_first_c = 0;
    std::int64_t pop_last_r = -1;
    std::int64_t pop_last_c = 0;
    for (std::int64_t r = 0; r < rows; ++r) {
        Screen::Row& row = out.rows[static_cast<std::size_t>(r)];
        row.text.resize(static_cast<std::size_t>(cols), ' ');
        std::int64_t chrome = 0;
        bool alert = false;
        for (std::int64_t c = 0; c < cols; ++c) {
            const Cell& cell = grid.at(r, c);
            row.text[static_cast<std::size_t>(c)] = ascii_of(cell.text);
            const std::uint32_t g = grid.groups(cell.hl);
            if ((g & (ui_group::kStatusLine | ui_group::kTabLine)) != 0) {
                ++chrome;
            }
            if ((g & (ui_group::kErrorMsg | ui_group::kWarningMsg)) != 0) {
                alert = true;
            }
            if ((g & ui_group::kVisual) != 0) {
                if (vis_first_r < 0) {
                    vis_first_r = r;
                    vis_first_c = c;
                }
                vis_last_r = r;
                vis_last_c = c;
            }
            if ((g & ui_group::kPmenuSel) != 0) {
                if (pop_first_r < 0) {
                    pop_first_r = r;
                    pop_first_c = c;
                }
                pop_last_r = r;
                pop_last_c = c;
            }
        }
        if (alert) {
            row.kind = RowKind::Alert;
        } else if (cols > 0 && chrome * 2 >= cols) {
            row.kind = RowKind::Chrome;
        } else if (cols > 0 && (grid.groups_at(r, 0) & ui_group::kEndOfBuffer) != 0) {
            row.kind = RowKind::Muted;
        }
    }
    if (rows == 0 || cols == 0) {
        return out;
    }
    detail::Cursor cursor{grid.cursor_row(), grid.cursor_column()};
    const bool cursor_on_grid = cursor.row >= 0 && cursor.row < rows && cursor.col >= 0 && cursor.col < cols;
    const bool block = grid.cursor_shape() == CursorShape::Block;
    if (cursor_on_grid && !block) {
        out.caret_row = cursor.row;
        out.caret_col = cursor.col;
    }
    const bool visual_mode = grid.mode().rfind("visual", 0) == 0;
    if (vis_first_r >= 0) {
        std::int64_t br = vis_first_r;
        std::int64_t bc = vis_first_c;
        std::int64_t er = vis_last_r;
        std::int64_t ec = vis_last_c + 1;
        if (blockwise_kind(visual_kind) && cursor_on_grid) {
            // ONE ROW OF THE RECTANGLE: the cursor row's own Visual run.
            std::int64_t first = -1;
            std::int64_t last = -1;
            for (std::int64_t c = 0; c < cols; ++c) {
                if ((grid.groups_at(cursor.row, c) & ui_group::kVisual) != 0) {
                    if (first < 0) {
                        first = c;
                    }
                    last = c;
                }
            }
            if (first < 0) {
                first = last = cursor.col;
            }
            br = er = cursor.row;
            bc = first;
            ec = last + 1;
        }
        if (visual_mode && cursor_on_grid) {
            if (detail::before(cursor.row, cursor.col, br, bc)) {
                br = cursor.row;
                bc = cursor.col;
            }
            if (!detail::before(cursor.row, cursor.col, er, ec)) {
                er = cursor.row;
                ec = cursor.col + 1;
            }
        }
        out.sel_begin_row = br;
        out.sel_begin_col = bc;
        out.sel_end_row = er;
        out.sel_end_col = ec;
        out.source = SelectionSource::Visual;
        return out;
    }
    if (pop_first_r >= 0) {
        out.sel_begin_row = pop_first_r;
        out.sel_begin_col = pop_first_c;
        out.sel_end_row = pop_last_r;
        out.sel_end_col = pop_last_c + 1;
        out.source = SelectionSource::Popup;
        return out;
    }
    if (cursor_on_grid && block) {
        out.sel_begin_row = cursor.row;
        out.sel_begin_col = cursor.col;
        out.sel_end_row = cursor.row;
        out.sel_end_col = cursor.col + 1;
        out.source = SelectionSource::Block;
    }
    return out;
}

} // namespace zengine::neovim

#endif // ZENGINE_NEOVIM_PROJECTION_HPP
