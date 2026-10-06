// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_VIEW_HPP
#define ZENGINE_WORKSHOP_PANE_VIEW_HPP
#include <zen/weave/shape.hpp>
#include <cstdint>
#include <string>
#include <vector>
namespace zengine::workshop {
// Visible text presentation, not a semantic command or an authority to interact.
struct PaneViewRequested {
    std::string provider, pane;
    ZEN_SHAPE(PaneViewRequested, 1, ZEN_FIELD(provider), ZEN_FIELD(pane));
};
struct PaneViewRow {
    std::int64_t row = 0, x = 0, y = 0, space = 0;
    std::string text;
    ZEN_SHAPE(PaneViewRow, 1, ZEN_FIELD(row), ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(space), ZEN_FIELD(text));
};
struct PaneView {
    std::string provider, pane;
    std::int64_t picture = 0;
    std::vector<PaneViewRow> rows;
    ZEN_SHAPE(PaneView, 1, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(picture), ZEN_FIELD(rows));
};
// Where one prose cell of a pane is on screen now, for a caller that read its rows at `picture`.
// The same measurer a press is resolved by answers; a moved picture, a place outside the body or
// a covered pane is refused. A point is not a gesture: pressing it is ordinary input.
struct PanePointRequested {
    std::string provider, pane;
    std::int64_t picture = 0, row = 0, column = 0;
    ZEN_SHAPE(PanePointRequested, 1, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(picture),
              ZEN_FIELD(row), ZEN_FIELD(column));
};
struct PanePoint {
    std::string provider, pane;
    std::int64_t picture = 0, row = 0, column = 0, x = 0, y = 0, space = 0;
    ZEN_SHAPE(PanePoint, 1, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(picture), ZEN_FIELD(row),
              ZEN_FIELD(column), ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(space));
};

// ---- THE DESK AND THE WORDS ON IT, as Workshop holds them --------------------------------------
// Every place is in canvas pixels: a window pixel is one, and a terminal cell is
// `surface::kCanvasCellPx` of them, its console row `surface::kTuiCanvasTopRow` below the canvas
// row. A point to press (`x`, `y`) is in the input space the medium reads (`space`), as
// `PanePoint`'s is. Reading grants nothing and moves nothing.

/// A rectangle in canvas pixels; empty (`w` or `h` zero) where there is nothing to place.
struct DeskRect {
    std::int64_t x = 0, y = 0, w = 0, h = 0;
    ZEN_SHAPE(DeskRect, 1, ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(w), ZEN_FIELD(h));
};

/// One run of words where the medium draws it: a text pane's row, a canvas pane's label or text
/// run, a menu's line. `word` numbers it in its answer, which a point request names; `place`
/// covers its glyphs; `x`, `y` is the centre of its middle character, where a press names it.
struct PaneWord {
    std::int64_t word = 0;
    std::string text;
    DeskRect place;
    std::int64_t x = 0, y = 0, space = 0;
    ZEN_SHAPE(PaneWord, 1, ZEN_FIELD(word), ZEN_FIELD(text), ZEN_FIELD(place), ZEN_FIELD(x),
              ZEN_FIELD(y), ZEN_FIELD(space));
};

struct DeskViewRequested {
    ZEN_SHAPE(DeskViewRequested, 1);
};

/// One pane on the desk: its state word (`open`, `covered`, `off-room`, `unresolved`), its rank
/// from the front (0 in front; -1 when it is not presented), the place its authored intent
/// resolves to and the part of that the canvas has, and whether it is selected or the keyboard
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
/// its presenter, and the lines it shows. `picture` is the presenter's number for those lines.
struct DeskMenu {
    bool open = false;
    std::string office, pane;
    std::int64_t picture = 0;
    DeskRect place;
    std::vector<PaneWord> lines;
    ZEN_SHAPE(DeskMenu, 1, ZEN_FIELD(open), ZEN_FIELD(office), ZEN_FIELD(pane), ZEN_FIELD(picture),
              ZEN_FIELD(place), ZEN_FIELD(lines));
};

/// The desk now: the canvas's extent and the medium's unit (`cell_px` device pixels to a canvas
/// cell; 0 where the device unit is the cell), the room panes stand in, every pane on the desk in
/// the desk's order, whether arranging is open, and the menu.
struct DeskView {
    std::int64_t width = 0, height = 0, cell_px = 0, space = 0;
    DeskRect room;
    std::vector<DeskPane> panes;
    bool arranging = false;
    DeskMenu menu;
    ZEN_SHAPE(DeskView, 1, ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(cell_px), ZEN_FIELD(space),
              ZEN_FIELD(room), ZEN_FIELD(panes), ZEN_FIELD(arranging), ZEN_FIELD(menu));
};

namespace v2 {

/// A pane's words, text or canvas alike: a text pane's rows, a canvas pane's labels and text runs
/// as it drew them last, each with its place. `canvas` says which the pane draws.
struct PaneViewRequested {
    std::string provider, pane;
    ZEN_SHAPE(PaneViewRequested, 2, ZEN_FIELD(provider), ZEN_FIELD(pane));
};
struct PaneView {
    std::string provider, pane;
    std::int64_t picture = 0;
    bool canvas = false;
    std::vector<PaneWord> words;
    ZEN_SHAPE(PaneView, 2, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(picture),
              ZEN_FIELD(canvas), ZEN_FIELD(words));
};
/// Where one character of one word is now, for a caller that read the words at `picture`.
struct PanePointRequested {
    std::string provider, pane;
    std::int64_t picture = 0, word = 0, column = 0;
    ZEN_SHAPE(PanePointRequested, 2, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(picture),
              ZEN_FIELD(word), ZEN_FIELD(column));
};
struct PanePoint {
    std::string provider, pane;
    std::int64_t picture = 0, word = 0, column = 0, x = 0, y = 0, space = 0;
    ZEN_SHAPE(PanePoint, 2, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(picture), ZEN_FIELD(word),
              ZEN_FIELD(column), ZEN_FIELD(x), ZEN_FIELD(y), ZEN_FIELD(space));
};

} // namespace v2

// ---- THE PARTS A PANE NAMES, where they are drawn ----------------------------------------------
// A pane names the parts a weaver acts on (`PaneRowPart`, `PaneCanvasPart`); Workshop carries each
// name as the pane said it, beside the words drawn inside the part and its place, and names
// nothing itself. A pane that names nothing still reads as words.

/// One named part where the medium draws it: its name, the words drawn inside it (a text part's
/// characters; a canvas part's words that lie inside it, joined by a space), `place` the part as
/// the body shows it, and `x`, `y` the point a press names it by -- its middle character's centre,
/// its first cell where it shows none, a canvas part's centre.
struct PanePart {
    std::string name, text;
    DeskRect place;
    std::int64_t x = 0, y = 0, space = 0;
    ZEN_SHAPE(PanePart, 1, ZEN_FIELD(name), ZEN_FIELD(text), ZEN_FIELD(place), ZEN_FIELD(x),
              ZEN_FIELD(y), ZEN_FIELD(space));
};

namespace v3 {

/// A pane's words and its named parts, from one reading of its picture.
struct PaneViewRequested {
    std::string provider, pane;
    ZEN_SHAPE(PaneViewRequested, 3, ZEN_FIELD(provider), ZEN_FIELD(pane));
};
struct PaneView {
    std::string provider, pane;
    std::int64_t picture = 0;
    bool canvas = false;
    std::vector<PaneWord> words;
    std::vector<PanePart> parts;
    ZEN_SHAPE(PaneView, 3, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(picture),
              ZEN_FIELD(canvas), ZEN_FIELD(words), ZEN_FIELD(parts));
};

} // namespace v3

namespace v2 {

/// The desk with the menu's named lines beside its lines: a pane's menu names a line by its row's
/// id, as its presenter said it; Workshop's own names each line by the action or group it shows.
struct DeskViewRequested {
    ZEN_SHAPE(DeskViewRequested, 2);
};
struct DeskMenu {
    bool open = false;
    std::string office, pane;
    std::int64_t picture = 0;
    DeskRect place;
    std::vector<PaneWord> lines;
    std::vector<PanePart> parts;
    ZEN_SHAPE(DeskMenu, 2, ZEN_FIELD(open), ZEN_FIELD(office), ZEN_FIELD(pane), ZEN_FIELD(picture),
              ZEN_FIELD(place), ZEN_FIELD(lines), ZEN_FIELD(parts));
};
struct DeskView {
    std::int64_t width = 0, height = 0, cell_px = 0, space = 0;
    DeskRect room;
    std::vector<DeskPane> panes;
    bool arranging = false;
    DeskMenu menu;
    ZEN_SHAPE(DeskView, 2, ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(cell_px), ZEN_FIELD(space),
              ZEN_FIELD(room), ZEN_FIELD(panes), ZEN_FIELD(arranging), ZEN_FIELD(menu));
};

} // namespace v2

} // namespace zengine::workshop
#endif
