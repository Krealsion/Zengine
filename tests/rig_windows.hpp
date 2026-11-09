// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_RIG_WINDOWS_HPP
#define ZENGINE_TESTS_RIG_WINDOWS_HPP

// The windows a Workshop rig shows its desk in, compiled once (`rig_windows.cpp`): the TUI
// window over the suites' terminal (`window_backends.hpp`) for a case in cells, the GUI window over
// their pixel backend for a case on a face, and the directory beside them. Mounted and activated
// as Loom's control door activates, a window offers itself and Workshop starts on that offer; it
// answers the platform as in a run. This header names no window, so a suite that includes the rig
// compiles none.

#include "graphics/color.hpp"
#include "input/translate.hpp"

#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace zengine::testing::rig_windows {

/// What a window shows: a row of text a line of its face, each row's trailing blanks cut, and that
/// face in canvas pixels -- 12 by 12 on the TUI window, where a character is a cell.
struct Shown {
    std::string window;
    std::vector<std::string> rows;
    std::int64_t advance = 12;
    std::int64_t line = 12;

    /// The row at a canvas pixel's height, or an empty one past the last.
    const std::string& row_at_px(std::int64_t y) const;
    /// What a row shows from the column a canvas pixel stands in, at most `columns` of it (all of
    /// it by default), its trailing blanks cut.
    std::string text_at_px(std::int64_t x, std::int64_t y,
                           std::int64_t columns = -1) const;
    /// Every row, one a line.
    std::string text() const;
};

/// ONE QUAD THE GUI WINDOW HANDED ITS DEVICE in the last picture it presented: its area, its place
/// in the window's pixels, and the pool colour it is in the night or the day palette (none where
/// no one pool colour is).
struct Quad {
    std::string area;
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    zengine::graphics::Color ink;
};

/// ONE CELL THE TUI WINDOW SHOWS: its glyph, and the pool colour whose ink in the block
/// palette it wrote it in (none where no pool colour's is, or the terminal's default).
struct Cell {
    std::int64_t col = 0, row = 0;
    std::string glyph = " "; ///< its character; "" for the second cell of a wide one
    zengine::graphics::Color ink;
};

/// Where the GUI window sits on its desktop, as its backend reads it or was asked to put it.
struct Placement {
    std::int64_t x = 0;
    std::int64_t y = 0;
    bool maximized = false;
    friend bool operator==(const Placement&, const Placement&) = default;
};

class Windows {
public:
    /// On `bus`, the directory's library at `directory` loaded through `manager` once the first
    /// window comes up. The suite names the path (its `GRAPHICS_SO`): this library holds none.
    Windows(loom::Switchboard& bus, loom::WeaveId manager, std::string directory);
    ~Windows();
    Windows(const Windows&) = delete;
    Windows& operator=(const Windows&) = delete;

    /// THE ROOM A CASE ASKS FOR, in canvas pixels, and the face text is set in (0 by 0: a cell
    /// medium). Cells go to the TUI window, sized in whole cells; a face to the GUI window,
    /// measured with it. The window is brought up the first time -- the directory loaded, the
    /// window mounted and activated, so it offers itself -- and sized again after, as a weaver
    /// resizes it: on its next beat it offers its new room.
    void room(std::int64_t width_px, std::int64_t height_px, std::int64_t advance = 0,
              std::int64_t line = 0);

    /// Whether a window is up, and which: `terminal` or `sdl`, empty when none.
    bool up() const;
    std::string window() const;
    /// The role of the window up, `zengine.window.<window>`.
    std::string role() const;
    loom::WeaveId id() const;

    /// WHAT THE WINDOW UP SHOWS NOW, read off its device as a weaver reads a screen: the cells the
    /// TUI window wrote its terminal, or the text of the last picture the GUI window presented.
    /// Reading it delivers nothing, so a case may read between turns it pumps itself. While the
    /// bus is idle, every 32nd reading is checked against what the window answers
    /// `WindowShowAsked`. None up: an empty answer.
    Shown shown();
    /// The window's own beat, asked for now (`PumpSurface`).
    void pump();
    /// THE WEAVER'S OWN HAND ON THE WINDOW UP: `moments` its platform gives it, read on the beat
    /// this asks for, as the weaver's and no participant's -- a terminal's in cells, the GUI
    /// window's in pixels or cells. None up: nothing.
    void hand(const std::vector<zengine::input::InputEvent>& moments);
    /// ...queued and not read: the window's next beat (`pump`) reads every moment queued since,
    /// in order, together.
    void queue(const std::vector<zengine::input::InputEvent>& moments);
    /// THE CLOCK BOTH WINDOWS' DEVICES KEEP, which each counts the weaver's clicks by: the case's
    /// reading. None stands still at 0, so two presses of one button are a double click.
    void clock(std::function<std::int64_t()> read);
    /// THE WINDOW UP OFFERS ANOTHER ROOM, `width_px` by `height_px`, queued and not delivered, as
    /// its beat publishes an offer: its last offer at that room, its platform holding it from now.
    void queue_offer(std::int64_t width_px, std::int64_t height_px);

    /// THE PICTURE AS THE WINDOW'S DEVICE IS HANDED IT, for a case about what is drawn rather than
    /// what is said: every cell the TUI window shows with the ink it wrote it in, and the
    /// quads of the last picture the GUI window presented.
    std::vector<Quad> quads();
    std::vector<Cell> cells();

    /// EVERY SHOWING FROM HERE ON, kept: the TUI window's rows as each showing left them. The
    /// GUI window's showings are not kept (none is watched there).
    void watch();
    std::vector<Shown> showings();

    /// THE PLATFORM, as the GUI window's backend holds it: the clipboard's text (none: no read
    /// exists), which a copy replaces as a platform's does, each copy offered it, and how often it
    /// was read; the place its desktop reports, and each place it was asked to take. The TUI
    /// window reads no clipboard and has no place.
    void set_clipboard(std::optional<std::string> text);
    std::optional<std::string> clipboard() const;
    const std::vector<std::string>& copies() const;
    int clipboard_reads() const;
    void set_placement(std::optional<Placement> at);
    std::vector<Placement> placed() const;
    /// ...and each room it was shaped to, as width and height, and what was asked of its window in
    /// order: `shape` and `place`.
    std::vector<std::pair<std::int64_t, std::int64_t>> shaped() const;
    const std::vector<std::string>& window_acts() const;

    /// THE WINDOW UP STARTED AGAIN IN PLACE, as a reload in place starts it: activated anew, so it
    /// offers itself again.
    void restart();

    /// The directory's role holder, once loaded.
    loom::WeaveId directory() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace zengine::testing::rig_windows

#endif // ZENGINE_TESTS_RIG_WINDOWS_HPP
