// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_VIEW_HPP
#define ZENGINE_WORKSHOP_PANE_VIEW_HPP

// THE DESK, READ BY MESSAGE: the desk by Workshop's own numbers and words, each presented pane's
// words and named parts where the weaver sees them, the stamp a reading stands on, the point a
// press on one character reaches, and the notice that the desk moved. Visible presentation, not a
// semantic command or an authority to interact: reading grants nothing and moves nothing.
// Workshop law: agents/workshop/desk-read.md

#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zengine::workshop {

// ---- THE DESK AND THE WORDS ON IT ---------------------------------------------------------------
// Every place is in the desk's pixels: the pixels of the window that shows it, where a window
// that sets text a character a cell has `graphics::kCellPx` of them to a cell. A point to press
// (`x`, `y`) is in the input space the medium reads (`space`): pixels in a window.

/// A rectangle in the desk's pixels; empty (`w` or `h` zero) where there is nothing to place.
struct DeskRect {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    ZEN_SHAPE(DeskRect, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h));
};

/// One run of words where the weaver sees it: a run of a pane's text on one line, a menu's line,
/// a word of the band. `word` numbers it in its reading, which a point request names; `place`
/// covers its characters; `x`, `y` is the centre of its middle character, where a press names it
/// -- none, as `PanePart` says one, where no press reaches it.
struct PaneWord {
    std::int64_t word = 0;
    std::string text;
    DeskRect place;
    std::int64_t x = 0, y = 0, space = 0;
    ZEN_SHAPE(PaneWord, 1, ZEN_FIELD(word), ZEN_FIELD(text), ZEN_FIELD(place), ZEN_FIELD(x),
              ZEN_FIELD(y), ZEN_FIELD(space));
};

/// One named part where the weaver sees it: its name, the words drawn inside it, `place` the part
/// as it shows, `x`, `y` the point a press reaches it at, and whether its operation applies now,
/// as its pane said it. A part no press reaches has no point: `x` and `y` are 0, and `space` is
/// `input::space::kUnknown`, a space no consumer reads.
struct PanePart {
    std::string name, text;
    DeskRect place;
    std::int64_t x = 0, y = 0, space = 0;
    bool available = true;
    ZEN_SHAPE(PanePart, 2, ZEN_FIELD(name), ZEN_FIELD(text), ZEN_FIELD(place), ZEN_FIELD(x),
              ZEN_FIELD(y), ZEN_FIELD(space), ZEN_FIELD(available));
};

/// One pane on the desk: its state word (`open`, `covered`, `off-room`, `unresolved`), its rank
/// from the front (0 in front; -1 when it is not presented), the place its authored intent
/// resolves to and the part of that the desk has, and whether it is selected or the keyboard
/// points at it (a menu or arranging, while open, takes the keys before it).
struct DeskPane {
    std::string provider, pane, name, state;
    std::int64_t front = -1;
    DeskRect resolved, visible;
    bool selected = false, keys = false;
    ZEN_SHAPE(DeskPane, 1, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(name), ZEN_FIELD(state),
              ZEN_FIELD(front), ZEN_FIELD(resolved), ZEN_FIELD(visible), ZEN_FIELD(selected),
              ZEN_FIELD(keys));
};

/// The menu on the screen, if one is: Workshop's own (`office` is Workshop's) or a pane's, shown by
/// its presenter, the lines it shows, and the lines it names -- a pane's by its row's id, as its
/// presenter said it, Workshop's own by the action or group each shows. `picture` is the
/// presenter's number for those lines.
struct DeskMenu {
    bool open = false;
    std::string office, pane;
    std::int64_t picture = 0;
    DeskRect place;
    std::vector<PaneWord> lines;
    std::vector<PanePart> parts;
    ZEN_SHAPE(DeskMenu, 3, ZEN_FIELD(open), ZEN_FIELD(office), ZEN_FIELD(pane), ZEN_FIELD(picture),
              ZEN_FIELD(place), ZEN_FIELD(lines), ZEN_FIELD(parts));
};

/// WHAT COVERS PART OF A PANE, and how much: `by` names each thing over it -- a pane in front by
/// its name, `menu`, `arranging`, `refused mark`, `band` -- `words` counts the words and parts not
/// said for it, and `rect` bounds the covered part of the pane. Empty for a pane nothing covers.
struct PaneCover {
    std::vector<std::string> by;
    std::int64_t words = 0;
    DeskRect rect;
    ZEN_SHAPE(PaneCover, 1, ZEN_FIELD(by), ZEN_FIELD(words), ZEN_FIELD(rect));
};

/// THE DESK NOW: a number that moves when anything it says moves (`desk`), the desk's extent and
/// the medium's unit (`cell_px` device pixels to a cell; 0 where the device unit is the cell), the
/// room panes stand in, every pane on the desk in the desk's order, whether arranging is open, the
/// menu, and the band's notice and legend as words with places and no point -- the band owns no
/// pointer space.
struct DeskView {
    std::int64_t desk = 0;
    std::int64_t width = 0, height = 0, cell_px = 0, space = 0;
    DeskRect room;
    std::vector<DeskPane> panes;
    bool arranging = false;
    DeskMenu menu;
    std::vector<PaneWord> words;
    ZEN_SHAPE(DeskView, 4, ZEN_FIELD(desk), ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(cell_px),
              ZEN_FIELD(space), ZEN_FIELD(room), ZEN_FIELD(panes), ZEN_FIELD(arranging),
              ZEN_FIELD(menu), ZEN_FIELD(words));
};

// ---- A PANE'S READING, AND WHAT IT STANDS ON ------------------------------------------------------

/// WHAT ONE PANE'S READING STANDS ON: its office's holder (a `WeaveId`) and that holder's
/// incarnation, the incarnation of the window that shows it (`epoch`), and the fingerprint of what
/// it shows there -- the window's reading of the pane's area, or Workshop's of what it draws for a
/// pane that draws in no window. A reading is stale when any of them moved.
struct PaneStamp {
    std::string provider, pane;
    std::int64_t holder = 0, incarnation = 0, epoch = 0, fingerprint = 0;
    ZEN_SHAPE(PaneStamp, 3, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(holder),
              ZEN_FIELD(incarnation), ZEN_FIELD(epoch), ZEN_FIELD(fingerprint));
};

/// A PAGE OF ONE PANE'S READING: its words and then its parts, from item `from`. `stamp` names the
/// reading a page continues: a page asked under a stamp that is no longer the pane's is refused as
/// stale, and the reader asks again from the start. A stamp naming no holder reads the pane as it
/// stands.
struct PaneViewRequested {
    std::string provider, pane;
    std::int64_t from = 0;
    PaneStamp stamp;
    ZEN_SHAPE(PaneViewRequested, 7, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(from),
              ZEN_FIELD(stamp));
};

/// One pane's reading, or a page of it: the stamp it stands on; the place it shows in and the text
/// measure it is set by (`advance`, `line`), so a reader can name any character of it; its words and
/// named parts but the covered ones, what covers it, whether its newest picture is still in flight
/// (then no word is said), and which items this page holds: `from` its first, of `total` words and
/// parts.
struct PaneView {
    PaneStamp stamp;
    DeskRect area;
    std::int64_t advance = 0, line = 0;
    std::vector<PaneWord> words;
    std::vector<PanePart> parts;
    PaneCover covered;
    bool in_flight = false;
    std::int64_t from = 0, total = 0;
    ZEN_SHAPE(PaneView, 7, ZEN_FIELD(stamp), ZEN_FIELD(area), ZEN_FIELD(advance), ZEN_FIELD(line),
              ZEN_FIELD(words), ZEN_FIELD(parts), ZEN_FIELD(covered), ZEN_FIELD(in_flight),
              ZEN_FIELD(from), ZEN_FIELD(total));
};

/// Where one character of one word of a pane's reading is now, for a caller that read the pane at
/// `fingerprint`: a reading that no longer shows that is refused, and read again. A point is not a
/// gesture: pressing it is ordinary input.
struct PanePointRequested {
    std::string provider, pane;
    std::int64_t fingerprint = 0, word = 0, column = 0;
    ZEN_SHAPE(PanePointRequested, 4, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(fingerprint),
              ZEN_FIELD(word), ZEN_FIELD(column));
};
struct PanePoint {
    std::string provider, pane;
    std::int64_t fingerprint = 0, word = 0, column = 0, x = 0, y = 0, space = 0;
    ZEN_SHAPE(PanePoint, 3, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(fingerprint),
              ZEN_FIELD(word), ZEN_FIELD(column), ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(space));
};

// ---- THE DESK, SAID WHOLE: one turn, paged by pane, and a pane paged by index ----------------------

/// THE DESK IN ONE TURN: the desk, every presented pane's stamp front to back, and as many of
/// those panes' readings, in that order, as one decoded value and one reply's bytes hold. A stamp
/// past the last reading names a pane to ask by `PaneViewRequested`.
struct DeskReadRequested {
    ZEN_SHAPE(DeskReadRequested, 3);
};
struct DeskRead {
    DeskView desk;
    std::vector<PaneStamp> stamps;
    std::vector<PaneView> panes;
    ZEN_SHAPE(DeskRead, 3, ZEN_FIELD(desk), ZEN_FIELD(stamps), ZEN_FIELD(panes));
};

/// THE DESK MOVED: its number and every presented pane's stamp front to back, as a desk read would
/// say them now. Workshop publishes it when either moved, so the newest stands for every one before
/// it.
struct DeskStamps {
    std::int64_t desk = 0;
    std::vector<PaneStamp> panes;
    ZEN_SHAPE(DeskStamps, 3, ZEN_FIELD(desk), ZEN_FIELD(panes));
};

} // namespace zengine::workshop
#endif
