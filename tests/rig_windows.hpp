// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_TESTS_RIG_WINDOWS_HPP
#define ZENGINE_TESTS_RIG_WINDOWS_HPP

// The windows a Workshop rig shows its desk in, compiled once (`rig_windows.cpp`): the terminal
// window over the suites' terminal (`window_backends.hpp`) for a case in cells, the OS window over
// their pixel backend for a case on a face, and the directory beside them. Mounted and activated
// as Loom's control door activates, a window offers itself and Workshop starts on that offer; it
// answers the platform as in a run. This header names no window, so a suite that includes the rig
// compiles none.

#include "input/translate.hpp"

#include <zen/switchboard.hpp>
#include <zen/weave.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace zengine::testing::rig_windows {

/// What a window shows: a row of text a line of its face, each row's trailing blanks cut, and that
/// face in canvas pixels -- 12 by 12 on the terminal window, where a character is a cell.
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

/// ONE QUAD THE OS WINDOW HANDED ITS DEVICE in the last picture it presented: its area, its place
/// in the window's pixels, and the role whose ink it is in the look the window draws in (-1: none).
struct Quad {
    std::string area;
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    std::int64_t role = -1;
};

/// ONE CELL THE TERMINAL WINDOW SHOWS: its glyph, and the role whose ink it wrote it in (-1: none).
struct Cell {
    std::int64_t col = 0, row = 0;
    char glyph = ' ';
    std::int64_t role = -1;
};

/// Where the OS window sits on its desktop, as its backend reads it or was asked to put it.
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
    /// medium). Cells go to the terminal window, sized in whole cells; a face to the OS window,
    /// measured with it. The window is brought up the first time -- the directory loaded, the
    /// window mounted and activated, so it offers itself -- and sized again after, as a weaver
    /// resizes it: on its next beat it offers its new room.
    void room(std::int64_t width_px, std::int64_t height_px, std::int64_t advance = 0,
              std::int64_t line = 0);

    /// Whether a window is up, and which: `terminal` or `sdl`, empty when none.
    bool up() const;
    std::string window() const;
    /// The office of the window up, `zengine.window.<window>`.
    std::string office() const;
    loom::WeaveId id() const;

    /// WHAT THE WINDOW UP SHOWS NOW, read off its device as a weaver reads a screen: the cells the
    /// terminal window wrote its terminal, or the text of the last picture the OS window presented.
    /// Reading it delivers nothing, so a case may read between turns it pumps itself. While the
    /// bus is idle, every 32nd reading is checked against what the window answers
    /// `WindowShowAsked`. None up: an empty answer.
    Shown shown();
    /// The window's own beat, asked for now (`PumpSurface`).
    void pump();
    /// THE WEAVER'S OWN HAND ON THE WINDOW UP: `moments` its platform gives it, read on the beat
    /// this asks for, as the weaver's and no participant's -- a terminal's in cells, the OS
    /// window's in pixels or cells. None up: nothing.
    void hand(const std::vector<zengine::input::InputEvent>& moments);
    /// ...queued and not read: the window's next beat (`pump`) reads every moment queued since,
    /// in order, together.
    void queue(const std::vector<zengine::input::InputEvent>& moments);

    /// THE PICTURE AS THE WINDOW'S DEVICE IS HANDED IT, for a case about what is drawn rather than
    /// what is said: every cell the terminal window shows with the ink it wrote it in, and the
    /// quads of the last picture the OS window presented.
    std::vector<Quad> quads();
    std::vector<Cell> cells();

    /// EVERY SHOWING FROM HERE ON, kept: the terminal window's rows as each showing left them. The
    /// OS window's showings are not kept (none is watched there).
    void watch();
    std::vector<Shown> showings();

    /// THE PLATFORM, as the OS window's backend holds it: the clipboard's text (none: no read
    /// exists), which a copy replaces as a platform's does, each copy offered it, and how often it
    /// was read; the place its desktop reports, and each place it was asked to take. The terminal
    /// window reads no clipboard and has no place.
    void set_clipboard(std::optional<std::string> text);
    std::optional<std::string> clipboard() const;
    const std::vector<std::string>& copies() const;
    int clipboard_reads() const;
    void set_placement(std::optional<Placement> at);
    std::vector<Placement> placed() const;

    /// The directory's office holder, once loaded.
    loom::WeaveId directory() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace zengine::testing::rig_windows

#endif // ZENGINE_TESTS_RIG_WINDOWS_HPP
