// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_NEOVIM_GRID_HPP
#define ZENGINE_NEOVIM_GRID_HPP

// The screen Neovim describes, kept as Neovim describes it: with `ext_linegrid` and
// `ext_hlstate`, `redraw` batches say which cells hold which text under which highlight, where
// the cursor is, the mode, and when a picture is complete (`flush`). This is that picture only:
// grid 1 (no `ext_multigrid`), its cells, the highlight table's semantic half, the cursor, the
// mode and its cursor shape, and the current window's viewport.
// Workshop law: agents/workshop/neovim.md

// What a highlight means is read from `ui_name`, never a colour: a colour scheme may paint Visual
// anything (names measured on 0.11.6 and 0.12.5). The cursor cell is not marked Visual; the
// projection extends a range through it. One input can produce several flushes (0.12.5 splits
// more), so the owner presents at most once per beat, from the last complete flush. A grid over
// `kMaxGridCells` is a protocol violation, refused rather than allocated.

#include "neovim/msgpack.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::neovim {

/// The largest grid this model holds. A pane's room is far smaller; Neovim sizes its grid to
/// what the embedder attached with and resized to, so a larger one names a broken peer.
inline constexpr std::int64_t kMaxGridRows = 1024;
inline constexpr std::int64_t kMaxGridColumns = 1024;
inline constexpr std::int64_t kMaxGridCells = 1u << 20;

/// The built-in highlight groups whose MEANING the projection reads. A bit each, so a cell's
/// semantics are one integer test.
namespace ui_group {
inline constexpr std::uint32_t kVisual = 1u << 0;
inline constexpr std::uint32_t kPmenuSel = 1u << 1;
inline constexpr std::uint32_t kPmenu = 1u << 2;
inline constexpr std::uint32_t kStatusLine = 1u << 3;   ///< StatusLine, StatusLineNC, and the terminal kinds
inline constexpr std::uint32_t kTabLine = 1u << 4;      ///< TabLine, TabLineSel, TabLineFill, WinBar
inline constexpr std::uint32_t kErrorMsg = 1u << 5;
inline constexpr std::uint32_t kWarningMsg = 1u << 6;
inline constexpr std::uint32_t kEndOfBuffer = 1u << 7;  ///< the `~` rows past the end of a buffer
inline constexpr std::uint32_t kMessage = 1u << 8;      ///< MsgArea, ModeMsg, MoreMsg, Question
inline constexpr std::uint32_t kSeparator = 1u << 9;    ///< WinSeparator, VertSplit

/// The bits one `ui_name` contributes; 0 for a name the projection does not read.
inline std::uint32_t of(std::string_view name) noexcept {
    if (name == "Visual" || name == "VisualNOS") {
        return kVisual;
    }
    if (name == "PmenuSel" || name == "PmenuKindSel" || name == "PmenuExtraSel" ||
        name == "PmenuMatchSel" || name == "WildMenu") {
        return kPmenuSel;
    }
    if (name == "Pmenu" || name == "PmenuKind" || name == "PmenuExtra" || name == "PmenuMatch" ||
        name == "PmenuSbar" || name == "PmenuThumb") {
        return kPmenu;
    }
    if (name == "StatusLine" || name == "StatusLineNC" || name == "StatusLineTerm" ||
        name == "StatusLineTermNC") {
        return kStatusLine;
    }
    if (name == "TabLine" || name == "TabLineSel" || name == "TabLineFill" || name == "WinBar" ||
        name == "WinBarNC") {
        return kTabLine;
    }
    if (name == "ErrorMsg") {
        return kErrorMsg;
    }
    if (name == "WarningMsg") {
        return kWarningMsg;
    }
    if (name == "EndOfBuffer") {
        return kEndOfBuffer;
    }
    if (name == "MsgArea" || name == "ModeMsg" || name == "MoreMsg" || name == "Question") {
        return kMessage;
    }
    if (name == "WinSeparator" || name == "VertSplit") {
        return kSeparator;
    }
    return 0;
}
} // namespace ui_group

/// How the cursor is drawn in a mode, as Neovim's `mode_info_set` says it.
enum class CursorShape : std::uint8_t { Block, Vertical, Horizontal };

/// One cell: the text Neovim put there (one grapheme; EMPTY for the right half of a double-width
/// character) and the highlight id it was drawn with.
struct Cell {
    std::string text = " ";
    std::int64_t hl = 0;
};

/// What applying one `redraw` notification did to the model.
struct Applied {
    bool flushed = false;      ///< at least one `flush` arrived: a coherent picture exists
    bool mode_changed = false;
    bool resized = false;
    std::string refusal;       ///< non-empty: the batch broke a rule and the model stopped reading it
};

class Grid {
public:
    /// APPLY ONE `redraw` NOTIFICATION's params: an array of batches, each `[name, args...]`.
    /// Unknown events are ignored (Neovim adds events; an embedder that did not ask for their
    /// extension never depends on them). A malformed or oversized event is refused whole and
    /// named; the model keeps what it had before that event.
    Applied apply(const msgpack::Value& params) {
        Applied out;
        if (!params.is_array()) {
            out.refusal = "a redraw notification whose params are not a list";
            return out;
        }
        for (const msgpack::Value& batch : params.as_array()) {
            if (!batch.is_array() || batch.size() == 0 || !batch.at(0).is_str()) {
                out.refusal = "a redraw batch that is not [name, args...]";
                return out;
            }
            const std::string& name = batch.at(0).as_str();
            for (std::size_t i = 1; i < batch.size(); ++i) {
                const msgpack::Value& args = batch.at(i);
                if (!args.is_array()) {
                    out.refusal = "redraw event `" + name + "` with arguments that are not a list";
                    return out;
                }
                if (!one(name, args, out)) {
                    if (out.refusal.empty()) {
                        out.refusal = "redraw event `" + name + "` with arguments this model cannot read";
                    }
                    return out;
                }
            }
        }
        return out;
    }

    std::int64_t rows() const noexcept { return rows_; }
    std::int64_t columns() const noexcept { return columns_; }

    const Cell& at(std::int64_t row, std::int64_t column) const noexcept {
        static const Cell kBlank;
        if (row < 0 || column < 0 || row >= rows_ || column >= columns_) {
            return kBlank;
        }
        return cells_[index(row, column)];
    }

    /// The semantic groups highlight `hl` was made of (`ui_group` bits); 0 for an unknown id.
    std::uint32_t groups(std::int64_t hl) const noexcept {
        if (hl < 0 || static_cast<std::size_t>(hl) >= groups_.size()) {
            return 0;
        }
        return groups_[static_cast<std::size_t>(hl)];
    }

    std::uint32_t groups_at(std::int64_t row, std::int64_t column) const noexcept {
        return groups(at(row, column).hl);
    }

    std::int64_t cursor_row() const noexcept { return cursor_row_; }
    std::int64_t cursor_column() const noexcept { return cursor_col_; }

    /// The mode NAME `mode_change` last said (`normal`, `insert`, `visual`, `cmdline_normal`,
    /// ...); empty before the first.
    const std::string& mode() const noexcept { return mode_; }

    /// The cursor shape for the current mode, from `mode_info_set` when Neovim reports cursor
    /// styles; otherwise a block, except in the insert and replace families and the command
    /// line's insert, where it is a bar -- Neovim's own default `guicursor`.
    CursorShape cursor_shape() const noexcept {
        if (style_enabled_ && mode_index_ >= 0 &&
            static_cast<std::size_t>(mode_index_) < shapes_.size()) {
            return shapes_[static_cast<std::size_t>(mode_index_)];
        }
        if (mode_ == "insert" || mode_ == "cmdline_insert") {
            return CursorShape::Vertical;
        }
        if (mode_ == "replace" || mode_ == "cmdline_replace" || mode_ == "operator") {
            return CursorShape::Horizontal;
        }
        return CursorShape::Block;
    }

    bool busy() const noexcept { return busy_; }

    /// How many `flush` events have arrived, ever.
    std::uint64_t flushes() const noexcept { return flushes_; }

    /// THE CURRENT WINDOW'S VIEWPORT as the last `win_viewport` for grid 1 said it: the first
    /// buffer line shown (0-based), the line after the last, the cursor line and column, and
    /// the buffer's line count. `known` is false until one arrived.
    struct Viewport {
        bool known = false;
        std::int64_t topline = 0;
        std::int64_t botline = 0;
        std::int64_t curline = 0;
        std::int64_t curcol = 0;
        std::int64_t line_count = 0;
    };
    const Viewport& viewport() const noexcept { return viewport_; }

private:
    std::size_t index(std::int64_t row, std::int64_t column) const noexcept {
        return static_cast<std::size_t>(row) * static_cast<std::size_t>(columns_) +
               static_cast<std::size_t>(column);
    }

    static bool ints(const msgpack::Value& args, std::size_t count) {
        if (args.size() < count) {
            return false;
        }
        for (std::size_t i = 0; i < count; ++i) {
            if (!args.at(i).is_int()) {
                return false;
            }
        }
        return true;
    }

    bool one(const std::string& name, const msgpack::Value& args, Applied& out) {
        if (name == "grid_line") {
            return grid_line(args, out);
        }
        if (name == "flush") {
            ++flushes_;
            out.flushed = true;
            return true;
        }
        if (name == "grid_cursor_goto") {
            if (!ints(args, 3)) {
                return false;
            }
            if (args.at(0).as_int() == 1) {
                cursor_row_ = args.at(1).as_int();
                cursor_col_ = args.at(2).as_int();
            }
            return true;
        }
        if (name == "grid_resize") {
            if (!ints(args, 3)) {
                return false;
            }
            if (args.at(0).as_int() != 1) {
                return true;
            }
            const std::int64_t width = args.at(1).as_int();
            const std::int64_t height = args.at(2).as_int();
            if (width < 0 || height < 0 || width > kMaxGridColumns || height > kMaxGridRows ||
                width * height > kMaxGridCells) {
                out.refusal = "Neovim resized its grid to " + std::to_string(width) + "x" +
                              std::to_string(height) + ", beyond what this model holds";
                return false;
            }
            resize(height, width);
            out.resized = true;
            return true;
        }
        if (name == "grid_clear") {
            if (!ints(args, 1)) {
                return false;
            }
            if (args.at(0).as_int() == 1) {
                for (Cell& c : cells_) {
                    c = Cell{};
                }
            }
            return true;
        }
        if (name == "grid_scroll") {
            return grid_scroll(args);
        }
        if (name == "hl_attr_define") {
            return hl_attr_define(args);
        }
        if (name == "mode_info_set") {
            return mode_info_set(args);
        }
        if (name == "mode_change") {
            if (args.size() < 2 || !args.at(0).is_str() || !args.at(1).is_int()) {
                return false;
            }
            out.mode_changed = out.mode_changed || mode_ != args.at(0).as_str() ||
                               mode_index_ != args.at(1).as_int();
            mode_ = args.at(0).as_str();
            mode_index_ = args.at(1).as_int();
            return true;
        }
        if (name == "win_viewport") {
            // [grid, win, topline, botline, curline, curcol, line_count, scroll_delta]
            if (args.size() < 7 || !args.at(0).is_int() || args.at(0).as_int() != 1) {
                return args.size() >= 1;
            }
            for (std::size_t i = 2; i < 7; ++i) {
                if (!args.at(i).is_int()) {
                    return false;
                }
            }
            viewport_.known = true;
            viewport_.topline = args.at(2).as_int();
            viewport_.botline = args.at(3).as_int();
            viewport_.curline = args.at(4).as_int();
            viewport_.curcol = args.at(5).as_int();
            viewport_.line_count = args.at(6).as_int();
            return true;
        }
        if (name == "busy_start") {
            busy_ = true;
            return true;
        }
        if (name == "busy_stop") {
            busy_ = false;
            return true;
        }
        return true; // an event this model does not read
    }

    void resize(std::int64_t rows, std::int64_t columns) {
        std::vector<Cell> next(static_cast<std::size_t>(rows * columns));
        for (std::int64_t r = 0; r < rows && r < rows_; ++r) {
            for (std::int64_t c = 0; c < columns && c < columns_; ++c) {
                next[static_cast<std::size_t>(r * columns + c)] = std::move(cells_[index(r, c)]);
            }
        }
        cells_ = std::move(next);
        rows_ = rows;
        columns_ = columns;
    }

    /// `[grid, row, col_start, cells, wrap?]` with cells `[text, hl_id?, repeat?]`: an omitted
    /// id repeats the previous cell's, an omitted repeat is one.
    bool grid_line(const msgpack::Value& args, Applied& out) {
        if (args.size() < 4 || !args.at(0).is_int() || !args.at(1).is_int() || !args.at(2).is_int() ||
            !args.at(3).is_array()) {
            return false;
        }
        if (args.at(0).as_int() != 1) {
            return true;
        }
        const std::int64_t row = args.at(1).as_int();
        std::int64_t col = args.at(2).as_int();
        if (row < 0 || row >= rows_ || col < 0) {
            out.refusal = "Neovim drew row " + std::to_string(row) + " of a grid with " +
                          std::to_string(rows_) + " rows";
            return false;
        }
        std::int64_t hl = 0;
        for (const msgpack::Value& cell : args.at(3).as_array()) {
            if (!cell.is_array() || cell.size() == 0 || !cell.at(0).is_str()) {
                return false;
            }
            if (cell.size() > 1) {
                if (!cell.at(1).is_int()) {
                    return false;
                }
                hl = cell.at(1).as_int();
            }
            std::int64_t repeat = 1;
            if (cell.size() > 2) {
                if (!cell.at(2).is_int()) {
                    return false;
                }
                repeat = cell.at(2).as_int();
            }
            if (repeat < 0 || repeat > columns_ - col) {
                out.refusal = "Neovim drew past the end of row " + std::to_string(row);
                return false;
            }
            const std::string& text = cell.at(0).as_str();
            for (std::int64_t k = 0; k < repeat; ++k) {
                Cell& target = cells_[index(row, col)];
                target.text = text;
                target.hl = hl;
                ++col;
            }
        }
        return true;
    }

    /// `[grid, top, bot, left, right, rows, cols]`: copy the cells of `[top, bot) x [left, right)`
    /// up by `rows` (down when negative). Cells no longer covered keep what they had; the
    /// `grid_line` events that follow fill them.
    bool grid_scroll(const msgpack::Value& args) {
        if (!ints(args, 7)) {
            return false;
        }
        if (args.at(0).as_int() != 1) {
            return true;
        }
        const std::int64_t top = args.at(1).as_int();
        const std::int64_t bot = args.at(2).as_int();
        const std::int64_t left = args.at(3).as_int();
        const std::int64_t right = args.at(4).as_int();
        const std::int64_t by = args.at(5).as_int();
        if (top < 0 || bot > rows_ || top > bot || left < 0 || right > columns_ || left > right) {
            return false;
        }
        if (by > 0) {
            for (std::int64_t r = top; r < bot - by; ++r) {
                for (std::int64_t c = left; c < right; ++c) {
                    cells_[index(r, c)] = cells_[index(r + by, c)];
                }
            }
        } else if (by < 0) {
            for (std::int64_t r = bot - 1; r >= top - by; --r) {
                for (std::int64_t c = left; c < right; ++c) {
                    cells_[index(r, c)] = cells_[index(r + by, c)];
                }
            }
        }
        return true;
    }

    /// `[id, rgb_attr, cterm_attr, info]`: the `info` list says which groups made the highlight.
    bool hl_attr_define(const msgpack::Value& args) {
        if (args.size() < 4 || !args.at(0).is_int() || !args.at(3).is_array()) {
            return false;
        }
        const std::int64_t id = args.at(0).as_int();
        if (id < 0 || id > 1'000'000) {
            return false;
        }
        std::uint32_t bits = 0;
        for (const msgpack::Value& entry : args.at(3).as_array()) {
            if (!entry.is_map()) {
                continue;
            }
            const msgpack::Value* kind = entry.get("kind");
            const msgpack::Value* ui_name = entry.get("ui_name");
            if (kind != nullptr && kind->is_str() && kind->as_str() == "ui" && ui_name != nullptr &&
                ui_name->is_str()) {
                bits |= ui_group::of(ui_name->as_str());
            }
        }
        const std::size_t at = static_cast<std::size_t>(id);
        if (at >= groups_.size()) {
            groups_.resize(at + 1, 0);
        }
        groups_[at] = bits;
        return true;
    }

    /// `[cursor_style_enabled, [mode_info...]]`, each mode_info a map with `cursor_shape`.
    bool mode_info_set(const msgpack::Value& args) {
        if (args.size() < 2 || !args.at(1).is_array()) {
            return false;
        }
        style_enabled_ = args.at(0).as_bool(false);
        shapes_.clear();
        for (const msgpack::Value& info : args.at(1).as_array()) {
            CursorShape shape = CursorShape::Block;
            if (info.is_map()) {
                if (const msgpack::Value* s = info.get("cursor_shape"); s != nullptr && s->is_str()) {
                    if (s->as_str() == "vertical") {
                        shape = CursorShape::Vertical;
                    } else if (s->as_str() == "horizontal") {
                        shape = CursorShape::Horizontal;
                    }
                }
            }
            shapes_.push_back(shape);
            if (shapes_.size() >= 64) {
                break;
            }
        }
        return true;
    }

    std::int64_t rows_ = 0;
    std::int64_t columns_ = 0;
    std::vector<Cell> cells_;
    std::vector<std::uint32_t> groups_;
    std::int64_t cursor_row_ = 0;
    std::int64_t cursor_col_ = 0;
    std::string mode_;
    std::int64_t mode_index_ = -1;
    bool style_enabled_ = false;
    std::vector<CursorShape> shapes_;
    bool busy_ = false;
    std::uint64_t flushes_ = 0;
    Viewport viewport_;
};

} // namespace zengine::neovim

#endif // ZENGINE_NEOVIM_GRID_HPP
