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

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::graphics::window {

/// The size a terminal window shows when its terminal cannot be measured -- a pipe, a suite --
/// while its areas reach no further.
inline constexpr std::int64_t kUnmeasuredCols = 80;
inline constexpr std::int64_t kUnmeasuredRows = 24;
/// The furthest its areas make such a window reach, in cells each way.
inline constexpr std::int64_t kUnmeasuredReach = 1000;

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

    TerminalMedium() { dirty_.size(cols_, rows_); }
    explicit TerminalMedium(Terminal terminal) : terminal_(std::move(terminal)) {
        dirty_.size(cols_, rows_);
    }

    /// The terminal taken, and measured.
    void claim() {
        terminal_.claim();
        const TerminalSize size = measure();
        cols_ = size.cols;
        rows_ = size.rows;
        dirty_.size(cols_, rows_);
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
        dirty_.size(cols_, rows_);
        dirty_.mark_all();
        return true;
    }

    CellExtent extent() const { return CellExtent{cols_, rows_}; }
    /// The room offered to the window's followers: its terminal's cells, `kCellPx` pixels each, or
    /// none where it has none to measure, so an arranger keeps the room it lays out in.
    PixelRoom offered() const {
        if (!terminal_.size().measured()) {
            return {};
        }
        return PixelRoom{cols_ * kCellPx, rows_ * kCellPx};
    }
    /// Its device unit is the cell, which a window offers as no pixels in one.
    static constexpr std::int64_t kCellDevicePx = 0;
    /// It sets text one character a cell.
    static constexpr TextMeasure text() { return TextMeasure{kCellPx, kCellPx}; }

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
    /// glyphs kept where they now stand. With no terminal it first reaches as far as its areas.
    void show(const std::vector<WinArea>& areas) {
        reach(areas);
        if (dirty_.empty()) {
            return;
        }
        const std::vector<term::Cell> cells = compose(areas, cache_, dirty_);
        dirty_.clear();
        const std::string bytes = ansi(cells);
        terminal_.write(bytes);
        keep_shown(cells);
        ++showings_;
        device_bytes_ += static_cast<std::int64_t>(bytes.size());
    }

    /// The window's rows as text, trailing spaces left off.
    std::vector<std::string> rows_shown(const std::vector<WinArea>& areas) const {
        DirtyCells all;
        all.size(cols_, rows_);
        all.mark_all();
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

    /// What one area shows of the terminal, in the area's pixels: its words and named parts
    /// (`window::read_area`), areas `reader` owns hiding nothing. "" when said, else why not.
    std::string read_area(const std::vector<WinArea>& areas, const WinArea& a,
                          const std::string& reader, std::vector<AreaWord>& words,
                          std::vector<AreaPart>& parts) const {
        return window::read_area(areas, cache_, a, cols_, rows_, reader, words, parts);
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
    /// Nor a title or a size: the terminal's are its own.
    void shape(const std::string&, const PixelSize&, const std::vector<WinArea>&) {}

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
    /// With no terminal to measure, the window as large as its areas reach and no smaller than
    /// the unmeasured size, to `kUnmeasuredReach` cells each way: every cell of a new size marked.
    void reach(const std::vector<WinArea>& areas) {
        if (terminal_.size().measured()) {
            return;
        }
        std::int64_t cols = kUnmeasuredCols, rows = kUnmeasuredRows;
        for (const WinArea& a : areas) {
            const Place pl = place_of(a);
            cols = (std::max)(cols, pl.col + pl.cols);
            rows = (std::max)(rows, pl.row + pl.rows);
        }
        reach_cols_ = (std::min)(cols, kUnmeasuredReach);
        reach_rows_ = (std::min)(rows, kUnmeasuredReach);
        (void)remeasure();
    }

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

    /// The window's size in cells: its terminal's, or as far as its areas reach where it has none.
    TerminalSize measure() const {
        const TerminalSize size = terminal_.size();
        return size.measured() ? size : TerminalSize{reach_cols_, reach_rows_};
    }

    Terminal terminal_;
    Cache cache_;
    DirtyCells dirty_; ///< the cells to show again, over the terminal's size
    std::vector<std::string> shown_rows_; ///< the glyphs on the terminal, as its showings left them
    std::int64_t cols_ = kUnmeasuredCols;
    std::int64_t rows_ = kUnmeasuredRows;
    std::int64_t reach_cols_ = kUnmeasuredCols; ///< how far its areas reach, with no terminal
    std::int64_t reach_rows_ = kUnmeasuredRows;
    std::int64_t showings_ = 0;
    std::int64_t device_bytes_ = 0;
};

/// The terminal window over `Terminal`.
template <class Terminal>
using WindowT = Window<TerminalMedium<Terminal>>;

} // namespace zengine::graphics::window

#endif // ZENGINE_GRAPHICS_WINDOW_WINDOW_TERMINAL_HPP
