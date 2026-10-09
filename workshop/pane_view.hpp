// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_WORKSHOP_PANE_VIEW_HPP
#define ZENGINE_WORKSHOP_PANE_VIEW_HPP
#include "surface/vocabulary.hpp"
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
/// covers its glyphs; `x`, `y` is the centre of its middle character, where a press names it --
/// none, as `PanePart` says one, while its canvas picture takes no press.
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
/// the body shows it, and `x`, `y` the point a press reaches it at, a place of its own that no part
/// listed after it holds -- a text part's middle character of its own, else its middle blank cell
/// of its own; a canvas part's centre, else the middle of its widest stretch of its own on the row
/// nearest its centre. A part with no place of its own has no point: `x` and `y` are 0, and
/// `space` is `input::space::kUnknown`, a space no consumer reads. Nor has any part or word of a
/// canvas picture that takes no press: the one a managed opening shows, until its pane draws its
/// own, or one its office's holder no longer holds.
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

/// Where one cell of a pane's text lattice is now, for a caller that read the pane at `picture`,
/// answered as `PanePoint`: a text pane's painted cell, as the first version's, or a canvas pane's
/// cell of the lattice its room's text stands on (`canvas_rows`), a blank one and the one after a
/// row's last character too.
struct PanePointRequested {
    std::string provider, pane;
    std::int64_t picture = 0, row = 0, column = 0;
    ZEN_SHAPE(PanePointRequested, 3, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(picture),
              ZEN_FIELD(row), ZEN_FIELD(column));
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

// ---- THE DESK, SAID WHOLE: one turn, paged by pane, and a pane paged by index ------------------
// A reading names what it stands on. A pane's picture numbers restart when its office is offered
// again and at each new canvas room, so a picture alone names nothing: a stamp names the pane's
// holder, that holder's incarnation, the room Workshop granted it, and the picture.

/// WHAT ONE PANE'S READING STANDS ON: its office's holder (a `WeaveId`), that holder's
/// incarnation, the canvas room grant its picture was drawn for (0 for a text pane, whose room
/// grants no number), and its picture. A reading is stale when any of the four moved.
struct PaneStamp {
    std::string provider, pane;
    std::int64_t holder = 0, incarnation = 0, grant = 0, picture = 0;
    ZEN_SHAPE(PaneStamp, 1, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(holder),
              ZEN_FIELD(incarnation), ZEN_FIELD(grant), ZEN_FIELD(picture));
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

namespace v4 {

/// A PAGE OF ONE PANE'S READING: its words and then its parts, from item `from`. `stamp` names
/// the reading a page continues: a page asked under a stamp that is no longer the pane's is
/// refused as stale, and the reader asks again from the start. A stamp naming no holder reads the
/// pane as it stands.
struct PaneViewRequested {
    std::string provider, pane;
    std::int64_t from = 0;
    PaneStamp stamp;
    ZEN_SHAPE(PaneViewRequested, 4, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(from),
              ZEN_FIELD(stamp));
};
/// One pane's reading, or a page of it: the third version's words and parts, the stamp it stands
/// on (`picture` with `holder`, `incarnation`, `grant`), what covers it -- its covered words are
/// not said -- whether its newest picture is still in flight (then no word is said), and which
/// items this page holds: `from` its first, of `total` words and parts.
struct PaneView {
    std::string provider, pane;
    std::int64_t picture = 0;
    bool canvas = false;
    std::vector<PaneWord> words;
    std::vector<PanePart> parts;
    std::int64_t holder = 0, incarnation = 0, grant = 0;
    PaneCover covered;
    bool in_flight = false;
    std::int64_t from = 0, total = 0;
    ZEN_SHAPE(PaneView, 4, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(picture),
              ZEN_FIELD(canvas), ZEN_FIELD(words), ZEN_FIELD(parts), ZEN_FIELD(holder),
              ZEN_FIELD(incarnation), ZEN_FIELD(grant), ZEN_FIELD(covered), ZEN_FIELD(in_flight),
              ZEN_FIELD(from), ZEN_FIELD(total));
};

} // namespace v4

namespace v3 {

/// THE DESK WITH WORKSHOP'S OWN WORDS: the second version, a number that moves when any of its
/// fields moves (`desk`), the band's notice and legend as words with places and no parts (the
/// band owns no pointer space), and the status slot, a word with no place, as the medium is handed
/// it (`slots`).
struct DeskView {
    std::int64_t desk = 0;
    std::int64_t width = 0, height = 0, cell_px = 0, space = 0;
    DeskRect room;
    std::vector<DeskPane> panes;
    bool arranging = false;
    v2::DeskMenu menu;
    std::vector<PaneWord> words;
    std::vector<surface::SurfaceText> slots;
    ZEN_SHAPE(DeskView, 3, ZEN_FIELD(desk), ZEN_FIELD(width), ZEN_FIELD(height), ZEN_FIELD(cell_px),
              ZEN_FIELD(space), ZEN_FIELD(room), ZEN_FIELD(panes), ZEN_FIELD(arranging),
              ZEN_FIELD(menu), ZEN_FIELD(words), ZEN_FIELD(slots));
};

} // namespace v3

/// THE DESK IN ONE TURN: the desk, every presented pane's stamp front to back, and as many of
/// those panes' readings, in that order, as one decoded value and one reply's bytes hold. A stamp
/// past the last reading names a pane to ask by `v4::PaneViewRequested`.
struct DeskReadRequested {
    ZEN_SHAPE(DeskReadRequested, 1);
};
struct DeskRead {
    v3::DeskView desk;
    std::vector<PaneStamp> stamps;
    std::vector<v4::PaneView> panes;
    ZEN_SHAPE(DeskRead, 1, ZEN_FIELD(desk), ZEN_FIELD(stamps), ZEN_FIELD(panes));
};

// ---- THE DESK BY WHAT IT SHOWS: a stamp names a picture by its fingerprint ------------------
// A pane's own picture numbers stay where a press needs them; a stamp names what the picture shows,
// so an unchanged picture keeps its stamp and every change moves it, a pane numbering none included.

namespace v2 {

/// WHAT ONE PANE'S READING STANDS ON, ITS PICTURE BY WHAT IT SHOWS: the first version's holder,
/// incarnation and room grant, and the fingerprint Workshop took of what the pane sent for the
/// picture aimed at (`picture_fingerprint`). A reading is stale when any of the four moved.
struct PaneStamp {
    std::string provider, pane;
    std::int64_t holder = 0, incarnation = 0, grant = 0, fingerprint = 0;
    ZEN_SHAPE(PaneStamp, 2, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(holder),
              ZEN_FIELD(incarnation), ZEN_FIELD(grant), ZEN_FIELD(fingerprint));
};

} // namespace v2

namespace v5 {

/// A PAGE OF ONE PANE'S READING under a stamp of the second version; a stamp naming no holder
/// reads the pane as it stands.
struct PaneViewRequested {
    std::string provider, pane;
    std::int64_t from = 0;
    v2::PaneStamp stamp;
    ZEN_SHAPE(PaneViewRequested, 5, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(from),
              ZEN_FIELD(stamp));
};
/// The fourth version's reading, its stamp naming the picture by `fingerprint`; `picture` stays
/// the pane's own number for the picture aimed at, which a point is asked under.
struct PaneView {
    std::string provider, pane;
    std::int64_t picture = 0;
    bool canvas = false;
    std::vector<PaneWord> words;
    std::vector<PanePart> parts;
    std::int64_t holder = 0, incarnation = 0, grant = 0, fingerprint = 0;
    PaneCover covered;
    bool in_flight = false;
    std::int64_t from = 0, total = 0;
    ZEN_SHAPE(PaneView, 5, ZEN_FIELD(provider), ZEN_FIELD(pane), ZEN_FIELD(picture),
              ZEN_FIELD(canvas), ZEN_FIELD(words), ZEN_FIELD(parts), ZEN_FIELD(holder),
              ZEN_FIELD(incarnation), ZEN_FIELD(grant), ZEN_FIELD(fingerprint),
              ZEN_FIELD(covered), ZEN_FIELD(in_flight), ZEN_FIELD(from), ZEN_FIELD(total));
};

} // namespace v5

namespace v2 {

/// THE DESK IN ONE TURN, ITS STAMPS BY FINGERPRINT: the first version's desk, stamps and readings,
/// each a second-version stamp and a fifth-version reading.
struct DeskReadRequested {
    ZEN_SHAPE(DeskReadRequested, 2);
};
struct DeskRead {
    v3::DeskView desk;
    std::vector<PaneStamp> stamps;
    std::vector<v5::PaneView> panes;
    ZEN_SHAPE(DeskRead, 2, ZEN_FIELD(desk), ZEN_FIELD(stamps), ZEN_FIELD(panes));
};

} // namespace v2

/// THE DESK MOVED: its number and every presented pane's stamp front to back, as a desk read would
/// say them now. Workshop publishes it at the end of a delivery that moved either, so the newest
/// stands for every one before it.
struct DeskStamps {
    std::int64_t desk = 0;
    std::vector<v2::PaneStamp> panes;
    ZEN_SHAPE(DeskStamps, 1, ZEN_FIELD(desk), ZEN_FIELD(panes));
};

} // namespace zengine::workshop
#endif
