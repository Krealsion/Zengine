// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_GRAPHICS_WINDOW_WINDOW_TERMINAL_HPP
#define ZENGINE_GRAPHICS_WINDOW_WINDOW_TERMINAL_HPP

// The terminal window: the window (graphics/window/window.hpp) over a medium of characters and an
// injected Terminal. It keeps each node painted as cells, composes the areas, writes its terminal
// only the cells whose picture changed, keeps what it showed, and reads the terminal's keys and
// presses with Zengine's parser. Reference: graphics/docs/zengine.window.terminal.md, its manual,
// compiled in and answered.

#include "graphics/light_paint.hpp"
#include "graphics/window/compose.hpp"
#include "graphics/window/terminal_io.hpp"
#include "graphics/window/window.hpp"

#include "input/translate.hpp"
#include "manuals/zengine.window.terminal.hpp" // this office's manual, compiled in from its page

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::graphics::window {

/// The size a terminal window shows when its terminal cannot be measured: a pipe, a suite.
inline constexpr std::int64_t kUnmeasuredCols = 80;
inline constexpr std::int64_t kUnmeasuredRows = 24;

/// The bytes that show cells on a terminal: for each run, a cursor move (left out where the cursor
/// already stands), the role's ink where it changes, and the glyphs.
inline std::string ansi(const std::vector<term::Cell>& cells) {
    std::string out;
    std::int64_t at_col = -1, at_row = -1, ink = -2;
    for (const term::Cell& c : cells) {
        if (c.col != at_col || c.row != at_row) {
            out += "\x1b[" + std::to_string(c.row + 1) + ";" + std::to_string(c.col + 1) + "H";
        }
        if (c.role != ink) {
            ink = c.role;
            out += sgr_for_role(static_cast<int>(c.role));
        }
        out.push_back(c.glyph);
        at_col = c.col + 1;
        at_row = c.row;
    }
    return out;
}

/// A medium of characters over a Terminal: `claim()`, `size()` in cells, `write(bytes)` and
/// `poll()` for what it typed.
template <class Terminal>
class TerminalMedium {
public:
    static constexpr const char* kWindow = kTerminalWindow;
    static constexpr const zengine::manual::Manual& kManual =
        zengine::manual::pages::zengine_window_terminal::kManual;

    TerminalMedium() = default;
    explicit TerminalMedium(Terminal terminal) : terminal_(std::move(terminal)) {}

    /// The terminal taken, and measured.
    void claim() {
        terminal_.claim();
        const TerminalSize size = measure();
        cols_ = size.cols;
        rows_ = size.rows;
    }

    /// The terminal at another size in cells: every cell of the window at the new size marked to
    /// show. True when the size changed.
    bool remeasure() {
        const TerminalSize size = measure();
        if (size.cols == cols_ && size.rows == rows_) {
            return false;
        }
        cols_ = size.cols;
        rows_ = size.rows;
        for (std::int64_t r = 0; r < rows_; ++r) {
            for (std::int64_t c = 0; c < cols_; ++c) {
                dirty_.insert({c, r});
            }
        }
        return true;
    }

    CellExtent extent() const { return CellExtent{cols_, rows_}; }
    /// The size offered to the window's followers: its terminal's, or none where it has none to
    /// measure, so an arranger keeps the room it lays out in.
    CellExtent offered() const { return terminal_.size().measured() ? extent() : CellExtent{}; }

    void clear() { cache_.clear(); }
    void cover(const WinArea& a, const std::string& path) {
        cache_.cover(a.area, path, place_of(a), dirty_);
    }
    void cover_area(const WinArea& a) {
        const Place pl = place_of(a);
        for (const KeptNode& k : a.nodes) {
            cache_.cover(a.area, k.node.path, pl, dirty_);
        }
    }
    void drop(const WinArea& a, const std::string& path) { cache_.drop(a.area, path); }
    void drop_area(const std::string& area) { cache_.drop_area(area); }
    void forget(const std::string&) {}

    /// The nodes at `paths` painted with the area's skin into cells, cut to the area, kept, and
    /// their cells marked to show. An area with no skin's library paints nothing; a skin that
    /// paints light, which a terminal does not show, is refused in words.
    void paint(Painters& painters, const WinArea& a, const PaintTask& task,
               const std::set<std::string>& paths, std::string& why) {
        if (a.library.empty()) {
            return;
        }
        const ZenginePainterV1* skin = painters.get(a.library, why);
        if (skin == nullptr) {
            return;
        }
        auto result = painters.paint(skin, task, why);
        if (!result) {
            return;
        }
        for (const NodePaint& np : result->nodes) {
            if (light::is_light(np.cells)) {
                why = "the skin in '" + a.library +
                      "' paints light, which a terminal does not show";
                return;
            }
        }
        std::map<std::string, const PaintNode*> asked;
        for (const PaintNode& n : task.nodes) {
            asked[n.path] = &n;
        }
        const Place pl = place_of(a);
        for (const std::string& p : paths) {
            cache_.drop(a.area, p);
        }
        for (const NodePaint& np : result->nodes) {
            const auto n = asked.find(np.path);
            if (paths.count(np.path) == 0 || n == asked.end()) {
                continue;
            }
            PaintedNode painted;
            painted.key = n->second->key;
            for (const term::Cell& c : term::unpack(np.cells)) {
                if (c.col >= 0 && c.row >= 0 && c.col < pl.cols && c.row < pl.rows) {
                    painted.cells.push_back(c);
                }
            }
            cache_.set(a.area, np.path, std::move(painted));
            cache_.cover(a.area, np.path, pl, dirty_);
        }
    }

    /// The cells marked since the last showing, composed and written as runs: one showing, its
    /// glyphs kept where they now stand.
    void show(const std::vector<WinArea>& areas) {
        std::set<CellAt> in_window;
        for (const CellAt& c : dirty_) {
            if (c.first >= 0 && c.second >= 0 && c.first < cols_ && c.second < rows_) {
                in_window.insert(c);
            }
        }
        dirty_.clear();
        if (in_window.empty()) {
            return;
        }
        const std::vector<term::Cell> cells = compose(areas, cache_, in_window);
        const std::string bytes = ansi(cells);
        terminal_.write(bytes);
        keep_shown(cells);
        ++showings_;
        device_bytes_ += static_cast<std::int64_t>(bytes.size());
    }

    /// The window's rows as text, trailing spaces left off.
    std::vector<std::string> rows_shown(const std::vector<WinArea>& areas) const {
        std::set<CellAt> all;
        for (std::int64_t r = 0; r < rows_; ++r) {
            for (std::int64_t c = 0; c < cols_; ++c) {
                all.insert({c, r});
            }
        }
        std::vector<std::string> rows(static_cast<std::size_t>(rows_),
                                      std::string(static_cast<std::size_t>(cols_), ' '));
        for (const term::Cell& c : compose(areas, cache_, all)) {
            rows[static_cast<std::size_t>(c.row)][static_cast<std::size_t>(c.col)] = c.glyph;
        }
        for (std::string& r : rows) {
            r.erase(r.find_last_not_of(' ') + 1);
        }
        return rows;
    }

    /// A point of the terminal, in cells (a point in pixels in the cell that holds it): the top
    /// node painted there.
    Landed land(const std::vector<WinArea>& areas, std::int64_t x, std::int64_t y,
                std::int64_t space) const {
        if (space == zengine::input::space::kPixels) {
            return window::land(areas, cache_, term::floor_cell(x), term::floor_cell(y));
        }
        return window::land(areas, cache_, x, y);
    }

    std::vector<zengine::input::InputEvent> poll() { return terminal_.poll(); }

    /// A terminal has no close box: nothing asks it to close.
    bool close_requested() const noexcept { return false; }
    bool take_close() noexcept { return false; }

    /// A copy offered to the terminal's clipboard by its copy sequence; no showing.
    void clipboard_copy(std::string_view text) {
        terminal_.write(terminal_clipboard_sequence(text));
    }
    /// No terminal route reads a system clipboard, so a paste here has no read: none, not a guess.
    std::optional<std::string> clipboard_text() const { return std::nullopt; }
    /// The terminal's window is its emulator's, which tells its programs nothing and takes no
    /// instructions: no placement to say, and none to take.
    std::optional<WindowPlacementValue> placement() const { return std::nullopt; }
    void place(const WindowPlacementValue&) {}

    /// The cells the terminal last showed, less their ink: a row of glyphs a line, each ending in a
    /// newline, at the size they were shown at; none before the first showing.
    std::optional<CapturedWindowPicture> capture() const {
        if (showings_ == 0 || shown_rows_.empty()) {
            return std::nullopt;
        }
        CapturedWindowPicture p;
        p.width = static_cast<std::int64_t>(shown_rows_.front().size());
        p.height = static_cast<std::int64_t>(shown_rows_.size());
        p.format = "text/cells";
        p.bytes.reserve(shown_rows_.size() * (shown_rows_.front().size() + 1));
        for (const std::string& row : shown_rows_) {
            p.bytes += row;
            p.bytes += '\n';
        }
        return p;
    }

    std::int64_t showings() const { return showings_; }
    std::int64_t device_bytes() const { return device_bytes_; }

private:
    /// A showing's cells laid over what the terminal showed before, at the window's size now: a
    /// window measured at another size marks every cell, so its next showing covers the grid whole.
    void keep_shown(const std::vector<term::Cell>& cells) {
        shown_rows_.resize(static_cast<std::size_t>(rows_));
        for (std::string& row : shown_rows_) {
            row.resize(static_cast<std::size_t>(cols_), ' ');
        }
        for (const term::Cell& c : cells) {
            shown_rows_[static_cast<std::size_t>(c.row)][static_cast<std::size_t>(c.col)] = c.glyph;
        }
    }

    /// The window's size in cells: its terminal's, or the unmeasured size where it has none.
    TerminalSize measure() const {
        const TerminalSize size = terminal_.size();
        return size.measured() ? size : TerminalSize{kUnmeasuredCols, kUnmeasuredRows};
    }

    Terminal terminal_;
    Cache cache_;
    std::set<CellAt> dirty_;
    std::vector<std::string> shown_rows_; ///< the glyphs on the terminal, as its showings left them
    std::int64_t cols_ = kUnmeasuredCols;
    std::int64_t rows_ = kUnmeasuredRows;
    std::int64_t showings_ = 0;
    std::int64_t device_bytes_ = 0;
};

/// The terminal window over `Terminal`.
template <class Terminal>
using WindowT = Window<TerminalMedium<Terminal>>;

} // namespace zengine::graphics::window

#endif // ZENGINE_GRAPHICS_WINDOW_WINDOW_TERMINAL_HPP
